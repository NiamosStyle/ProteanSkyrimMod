#include "Flight.h"

#include "Evolve.h"
#include "Registry.h"
#include "Shift.h"

namespace protean::Flight
{
	namespace
	{
		constexpr std::string_view kPerk = "preys_flight";
		constexpr float            kUnit = 0.0142875f;           // havok units per game unit
		constexpr float            kGravity = 9.80665f / kUnit;  // the engine's fall, game units / s^2
		constexpr float            kPush = 0.12f;                // seconds a jump's take-off speed is held
		constexpr float            kEase = 1.5f;                 // the glide reaches its fall speed in 1 / kEase seconds
		constexpr float            kDiveSpeed = 2500.0f;         // game units / s straight down
		constexpr float            kDoubleAfter = 0.15f;         // seconds off the ground before the jump in mid-air
		constexpr float            kPowerHold = 0.3f;            // seconds an attack key is held to count as a power attack

		std::atomic<bool> g_jumpHeld{ false };
		std::atomic<bool> g_jumpPressed{ false };
		std::atomic<bool> g_powerPressed{ false };

		float g_air = 0.0f;         // seconds in the air
		bool  g_doubleUsed = false;
		bool  g_doubleQueued = false;
		float g_push = -1.0f;       // >= 0: the double jump's take-off, seconds in
		float g_pushVz = 0.0f;
		bool  g_gliding = false;
		float g_glideStart = 0.0f;  // the fall speed when the glide began, game units / s
		float g_glideStartAcross = 0.0f;  // the speed across the ground when it began
		float g_glideEase = 0.0f;
		int   g_fallSent = 0;
		float g_outOfFall = 0.0f;               // seconds the graph has been out of its fall while gliding
		constexpr float kOutOfFall = 0.1f;  // log lines so far for the glide's fall pose
		bool  g_diving = false;
		std::atomic<bool> g_landedInGlide{ false };  // the graph landed while still gliding (set from its event)
		float g_stuck = 0.0f;  // seconds on the ground with the game's jumping flag still up

		std::string g_sound = "NPCDragonLandCrashShortSD";

		// the glide's looks while being chosen live: art objects / effect shaders on the player (dev 'glidefx <hex>...')
		// and a wind loop (dev 'glidesound <SNDR editor ID>')
		std::vector<RE::FormID> g_fx{ 0x000ABF02, 0x02002947 };  // AbsorbRedHandFX01, DLC1DrainVitalCasterFXS
		std::string             g_wind = "AMBDungeonIceWindRadius2DLPSD";  // the user's pick of the loops; 2D: the outdoor ones stayed faint
		float                   g_windVolume = 1.6f;  // a little louder than the loop's own (the user)
		int                     g_windLogged = 0;
		std::array<RE::BSSoundHandle, 3> g_windHandles;
		int                     g_windLayers = 1;  // copies played at once, for more volume than one allows
		bool                    g_looksOn = false;

		void Looks(RE::PlayerCharacter* a_p, bool a_on)
		{
			if (a_on == g_looksOn) {
				return;
			}
			g_looksOn = a_on;
			if (a_on) {
				for (const auto id : g_fx) {
					auto* form = RE::TESForm::LookupByID(id);
					if (auto* art = form ? form->As<RE::BGSArtObject>() : nullptr) {
						// a hand effect on each hand (left to itself it sat at the feet)
						auto* root = a_p->Get3D(false);
						for (const char* node : { "NPC R MagicNode [RMag]", "NPC L MagicNode [LMag]" }) {
							a_p->ApplyArtObject(art, -1.0f, nullptr, false, false, root ? root->GetObjectByName(node) : nullptr);
						}
					} else if (auto* shader = form ? form->As<RE::TESEffectShader>() : nullptr) {
						a_p->ApplyEffectShader(shader, -1.0f);
					}
				}
				if (auto* audio = RE::BSAudioManager::GetSingleton(); audio && !g_wind.empty()) {
					for (int i = 0; i < g_windLayers; ++i) {
						auto& h = g_windHandles[i];
						audio->GetSoundHandleByName(h, g_wind.c_str(), 0x1A);
						if (h.IsValid()) {
							h.SetObjectToFollow(a_p->Get3D());
							h.SetVolume(g_windVolume);
							h.Play();
						}
					}
					if (g_windLogged++ < 20) {
						logger::info("flight: glide wind '{}' x{} {}", g_wind, g_windLayers, g_windHandles[0].IsValid() ? "playing" : "NOT FOUND");
					}
				}
				return;
			}
			// off: the effects on the player that are ours end
			if (auto* pl = RE::ProcessLists::GetSingleton()) {
				const auto mine = [&](RE::ReferenceEffect* a_e, RE::TESForm* a_form) {
					auto ref = a_e->target.get();
					if (ref && ref->IsPlayerRef() && a_form && std::ranges::find(g_fx, a_form->GetFormID()) != g_fx.end()) {
						a_e->finished = true;
					}
				};
				pl->ForEachModelEffect([&](RE::ModelReferenceEffect* a_e) {
					mine(a_e, a_e->artObject);
					return RE::BSContainer::ForEachResult::kContinue;
				});
				pl->ForEachShaderEffect([&](RE::ShaderReferenceEffect* a_e) {
					mine(a_e, a_e->effectData);
					return RE::BSContainer::ForEachResult::kContinue;
				});
			}
			for (auto& h : g_windHandles) {
				if (h.IsValid()) {
					h.FadeOutAndRelease(400);
				}
			}
		}
		int         g_pose = 1;               // which glide marker (1-3; 0 = none): the pose being chosen
		RE::SpellItem* g_marker = nullptr;    // the one on the player now

		// the arms turned a little further on top of the glide pose (degrees about each bone's own x, y, z), being
		// chosen live: dev 'glidearm r|l x y z'
		bool         g_steer = true;  // test switch: the glide's forward push (dev 'glidesteer 0|1')
		int          g_diag = 0;      // frames left to log in full (dev 'glidediag <frames>')
		RE::NiPoint3 g_armR{};  // none: Protean's glide animation has the arms out (25 deg about y was the pick on a borrowed pose)
		RE::NiPoint3 g_armL{};

		void TurnArms(RE::PlayerCharacter* a_p)
		{
			auto* root = a_p->Get3D(false);
			if (!root) {
				return;
			}
			constexpr float kDeg = 3.14159265f / 180.0f;
			for (const auto& [name, turn] : { std::pair{ "NPC R UpperArm [RUar]", g_armR }, std::pair{ "NPC L UpperArm [LUar]", g_armL } }) {
				auto* bone = root->GetObjectByName(name);
				if (!bone || (turn.x == 0.0f && turn.y == 0.0f && turn.z == 0.0f)) {
					continue;
				}
				RE::NiMatrix3 m;
				m.SetEulerAnglesXYZ(turn.x * kDeg, turn.y * kDeg, turn.z * kDeg);
				bone->local.rotate = bone->local.rotate * m;
				RE::NiUpdateData update;
				bone->Update(update);
			}
		}
		float       g_shake = 1.0f;

		float Dial(std::string_view a_name, float a_fallback) { return Registry::Get().Dial(a_name, a_fallback); }

		// a share of the player's own jump (Predator's Rush's when owned)
		float JumpHeight()
		{
			auto* gs = RE::GameSettingCollection::GetSingleton();
			auto* jump = gs ? gs->GetSetting("fJumpHeightMin") : nullptr;
			return (jump && jump->GetFloat() > 0.0f ? jump->GetFloat() : 76.0f) * Dial("Protean_FlightJumpHeight", 1.25f);
		}

		float Stamina(RE::PlayerCharacter* a_p) { return a_p->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina); }
		void  Spend(RE::PlayerCharacter* a_p, float a_cost) { a_p->AsActorValueOwner()->DamageActorValue(RE::ActorValue::kStamina, a_cost); }

		RE::hkVector4 Velocity(RE::bhkCharacterController* a_cc)
		{
			RE::hkVector4 v;
			a_cc->GetLinearVelocityImpl(v);
			return v;
		}

		void SetVz(RE::bhkCharacterController* a_cc, float a_vx, float a_vy, float a_vz)
		{
			// the state only from the ground: overriding the game's own jump state mid-jump is the likely way a jump
			// was left unfinished (every later Jump only hopped)
			if (a_cc->context.currentState == RE::hkpCharacterStateType::kOnGround) {
				a_cc->context.currentState = RE::hkpCharacterStateType::kInAir;
			}
			a_cc->SetLinearVelocityImpl(RE::hkVector4(a_vx, a_vy, a_vz * kUnit, 0.0f));
		}

		// the hidden marker Open Animation Replacer looks for to play the glide pose in place of the fall
		void Mark(RE::PlayerCharacter* a_p, bool a_on)
		{
			const auto& markers = Registry::Get().GlideMarkers();
			RE::SpellItem* want = a_on && g_pose >= 1 && g_pose <= static_cast<int>(markers.size()) ? markers[g_pose - 1] : nullptr;
			if (want == g_marker) {
				return;
			}
			if (g_marker) {
				a_p->RemoveSpell(g_marker);
			}
			if (want) {
				a_p->AddSpell(want);
			}
			g_marker = want;
		}

		// test aid: the animation events while gliding, to find what takes the graph out of its fall every few seconds
		class GraphSink : public RE::BSTEventSink<RE::BSAnimationGraphEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override
			{
				if (a_event && g_gliding && a_event->holder && a_event->holder->IsPlayerRef()) {
					// the game's fall fail-safe lands the player when the fall clip ends: back into the glide at once
					if (std::string_view(a_event->tag.c_str()).starts_with("JumpLand")) {
						g_landedInGlide = true;
					}
					if (_logged < 60) {
						++_logged;
						logger::info("flight: graph event '{}' ({}) while gliding", a_event->tag.c_str(), a_event->payload.c_str());
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}

		private:
			int _logged{ 0 };
		};
		GraphSink g_graphSink;

		void Reset()
		{
			g_air = 0.0f;
			g_doubleUsed = false;
			g_doubleQueued = false;
			g_push = -1.0f;
			g_gliding = false;
			g_diving = false;
		}

		// the dive's landing: everyone near, followers too, staggers as from a dragon landing
		void Slam(RE::PlayerCharacter* a_p)
		{
			const auto  here = a_p->GetPosition();
			const float radius = Dial("Protean_SlamRadius", 550.0f);
			int         hit = 0;
			if (auto* pl = RE::ProcessLists::GetSingleton()) {
				for (auto& h : pl->highActorHandles) {
					auto a = h.get();
					if (!a || a.get() == a_p || a->IsDead() || a->GetPosition().GetDistance(here) > radius) {
						continue;
					}
					const float angle = a->GetHeadingAngle(here, false);
					a->SetGraphVariableFloat("staggerDirection", (angle >= 0.0f ? angle : 360.0f + angle) / 360.0f);
					a->SetGraphVariableFloat("staggerMagnitude", 1.0f);
					a->NotifyAnimationGraph("staggerStart");
					logger::info("flight: slam staggers '{}' {:08X} at {:.0f}", a->GetName(), a->GetFormID(), a->GetPosition().GetDistance(here));
					++hit;
				}
			}
			RE::BSSoundHandle sound;
			if (auto* audio = RE::BSAudioManager::GetSingleton()) {
				audio->GetSoundHandleByName(sound, g_sound.c_str(), 0x1A);
			}
			if (sound.IsValid()) {
				sound.SetPosition(here);
				sound.Play();
			}
			if (g_shake > 0.0f) {
				RE::ShakeCamera(g_shake, here, 0.6f);
			}
			logger::info("flight: slam staggered {} within {:.0f}", hit, radius);
		}
	}

	namespace
	{
		void Update(RE::PlayerCharacter* a_p, float a_delta)
		{
			auto* cc = a_p->GetCharController();
			const bool jumpPressed = g_jumpPressed.exchange(false);
			const bool powerPressed = g_powerPressed.exchange(false);
			if (!cc || a_p->IsDead() || !Evolve::Has(kPerk)) {
				Reset();
				return;
			}

			const auto state = cc->context.currentState;
			// off the ground by the support flag: the state can still read "on ground" for a moment after a take-off,
			// which once let the jump in mid-air be had again and again
			const bool air = !cc->flags.all(RE::CHARACTER_FLAGS::kSupport) && state != RE::hkpCharacterStateType::kSwimming;
			const auto v = Velocity(cc);
			const float vz = v.quad.m128_f32[2] / kUnit;

			// no fall damage: while coming down the drop is measured from here. Only the height: the fall's time is how
			// the game knows to play the landing, and with it held at 0 a landing could leave the jump unfinished
			if (air && vz < 0.0f) {
				RE::hkVector4 pos;
				cc->GetPositionImpl(pos, false);
				cc->fallStartHeight = pos.quad.m128_f32[2] / kUnit;
			}
			if (jumpPressed && !air) {
				bool inJump = false;
				a_p->GetGraphVariableBool("bInJumpState", inJump);
				logger::info("flight: Jump on the ground: state {} wantState {} graph bInJumpState {} support {} flags {:08X}",
					static_cast<int>(state), static_cast<int>(cc->wantState), inJump, cc->flags.all(RE::CHARACTER_FLAGS::kSupport),
					static_cast<std::uint32_t>(cc->flags.underlying()));
			}
			// a jump the game never finished: standing still on the ground with its jumping flag up. Seen as Jump only
			// hopping; put right after a moment so a real take-off is never touched
			if (!air && state == RE::hkpCharacterStateType::kOnGround && cc->flags.all(RE::CHARACTER_FLAGS::kJumping)) {
				g_stuck += a_delta;
				if (g_stuck > 0.5f) {
					cc->flags.reset(RE::CHARACTER_FLAGS::kJumping);
					cc->flags.set(RE::CHARACTER_FLAGS::kCanJump);
					logger::info("flight: a jump left unfinished, its flag cleared");
					g_stuck = 0.0f;
				}
			} else {
				g_stuck = 0.0f;
			}
			if (Shift::ActiveCreature() || !air || a_p->AsActorState()->IsSwimming()) {
				if (g_diving && cc->flags.all(RE::CHARACTER_FLAGS::kSupport)) {  // landed, not in water
					Slam(a_p);
				}
				Reset();
				return;
			}
			g_air += a_delta;

			if (g_diving) {
				SetVz(cc, v.quad.m128_f32[0] * 0.3f, v.quad.m128_f32[1] * 0.3f, -kDiveSpeed);
				return;
			}
			if (powerPressed && g_air >= Dial("Protean_SlamMinAir", 1.0f)) {
				const float cost = Dial("Protean_SlamCost", 30.0f);
				if (Stamina(a_p) >= cost) {
					Spend(a_p, cost);
					g_diving = true;
					g_gliding = false;
					g_push = -1.0f;
					logger::info("flight: dive after {:.1f} s in the air", g_air);
					return;
				}
			}

			// pressed while the game's own jump is still lifting: kept until it is done, since the game holds its own
			// speed till then and the second jump came out low
			if (jumpPressed && !g_doubleUsed && g_push < 0.0f && g_air >= kDoubleAfter) {
				g_doubleQueued = true;
			}
			if (g_doubleQueued && state != RE::hkpCharacterStateType::kJumping) {
				g_doubleQueued = false;
				const float cost = Dial("Protean_FlightJumpCost", 15.0f);
				if (Stamina(a_p) >= cost) {
					Spend(a_p, cost);
					g_doubleUsed = true;
					g_gliding = false;
					g_push = 0.0f;
					g_pushVz = std::sqrt(2.0f * kGravity * JumpHeight());
					logger::info("flight: double jump after {:.1f} s in the air", g_air);
				}
			}
			if (g_push >= 0.0f) {
				g_push += a_delta;
				if (g_push <= kPush) {
					SetVz(cc, v.quad.m128_f32[0], v.quad.m128_f32[1], g_pushVz - kGravity * g_push);
					return;
				}
				g_push = -1.0f;
			}

			// the glide: Jump held while falling
			const float cost = Dial("Protean_GlideCost", 5.0f) * a_delta;
			if (g_jumpHeld && vz < 0.0f && Stamina(a_p) > cost) {
				const float vx = v.quad.m128_f32[0] / kUnit;
				const float vy = v.quad.m128_f32[1] / kUnit;
				if (!g_gliding) {
					g_landedInGlide = false;  // nothing left from before this glide
					g_gliding = true;
					g_glideStart = vz;
					g_glideStartAcross = std::sqrt(vx * vx + vy * vy);
					g_glideEase = 0.0f;
				}
				Spend(a_p, cost);
				// the falling pose throughout: slowed, the fall sometimes read to the game as a run
				if (bool inJump = true; a_p->GetGraphVariableBool("bInJumpState", inJump) && inJump) {
					g_outOfFall = 0.0f;
				}
				if (g_landedInGlide.exchange(false)) {
					a_p->NotifyAnimationGraph("JumpFall");
					logger::info("flight: the graph landed mid-glide, back into the fall");
				} else if (bool inJump = true; a_p->GetGraphVariableBool("bInJumpState", inJump) && !inJump &&
												(g_outOfFall += a_delta) > kOutOfFall) {
					// only when the graph stays out: sent on a one-frame blip, the resend itself showed a run
					const bool took = a_p->NotifyAnimationGraph("JumpFall");
					g_outOfFall = 0.0f;
					if (g_fallSent++ < 3) {
						logger::info("flight: glide out of the falling pose, JumpFall sent (taken {})", took);
					}
				}
				g_glideEase = std::min(g_glideEase + a_delta * kEase, 1.0f);
				const float down = -std::abs(Dial("Protean_GlideFallSpeed", 250.0f));
				// forward where the player faces, at least the glide's speed (a faster run going in keeps its speed)
				const float across = std::max(g_glideStartAcross, Dial("Protean_GlideSpeed", 900.0f));
				const float speed = g_glideStartAcross + (across - g_glideStartAcross) * g_glideEase;
				const float heading = a_p->GetAngleZ();
				const float vzGlide = g_glideStart + (down - g_glideStart) * g_glideEase;
				if (g_steer) {
					SetVz(cc, std::sin(heading) * speed * kUnit, std::cos(heading) * speed * kUnit, vzGlide);
				} else {
					SetVz(cc, v.quad.m128_f32[0], v.quad.m128_f32[1], vzGlide);  // test: the fall slowed, no push
				}
			} else {
				g_gliding = false;
			}
		}

	}

	namespace
	{
		// test aid, the pose probe: how far the upper body turned against the character's root since the last frame
		// (the skeleton as the frame's animation left it), with the frame's time and the glide's physics. A pose that
		// shakes shows as big turns frame after frame; a hitch as a long frame; a jerk of the body as a jump in speed.
		float          g_frameDelta = 0.0f;
		RE::NiMatrix3  g_lastSpine{};
		bool           g_haveSpine = false;
		RE::NiPoint3   g_lastVel{};

		float Angle(const RE::NiMatrix3& a, const RE::NiMatrix3& b)
		{
			// the angle of a^T b
			float tr = 0.0f;
			for (int i = 0; i < 3; ++i) {
				for (int j = 0; j < 3; ++j) {
					tr += a.entry[j][i] * b.entry[j][i];
				}
			}
			return std::acos(std::clamp((tr - 1.0f) * 0.5f, -1.0f, 1.0f)) * 57.2958f;
		}

		void Probe(RE::PlayerCharacter* a_p)
		{
			auto* root = a_p->Get3D(false);
			auto* spine = root ? root->GetObjectByName("NPC Spine2 [Spn2]") : nullptr;
			auto* cc = a_p->GetCharController();
			if (!spine || !cc || g_air <= 0.0f) {
				g_haveSpine = false;
				return;
			}
			// the spine against the model's root: the pose alone, not the character turning
			const RE::NiMatrix3 rel = root->world.rotate.Transpose() * spine->world.rotate;
			RE::hkVector4 v;
			cc->GetLinearVelocityImpl(v);
			const RE::NiPoint3 vel{ v.quad.m128_f32[0] / kUnit, v.quad.m128_f32[1] / kUnit, v.quad.m128_f32[2] / kUnit };
			const float turned = g_haveSpine ? Angle(g_lastSpine, rel) : 0.0f;
			const float jolt = g_haveSpine ? (vel - g_lastVel).Length() : 0.0f;
			const bool  odd = turned > 4.0f || g_frameDelta > 0.05f || jolt > 300.0f;
			if (g_diag > 0 || odd) {
				bool inJump = false;
				a_p->GetGraphVariableBool("bInJumpState", inJump);
				logger::info("probe: t {:.2f} dt {:.3f} spine turned {:.1f} deg, speed ({:.0f}, {:.0f}, {:.0f}) jolt {:.0f}, gliding {} jump held {} "
				             "graph in jump {} state {} support {}{}",
					g_air, g_frameDelta, turned, vel.x, vel.y, vel.z, jolt, g_gliding, g_jumpHeld.load(), inJump,
					static_cast<int>(cc->context.currentState), cc->flags.all(RE::CHARACTER_FLAGS::kSupport), odd ? "  <--" : "");
				if (g_diag > 0) {
					--g_diag;
				}
			}
			g_lastSpine = rel;
			g_lastVel = vel;
			g_haveSpine = true;
		}
	}

	void OnFrame(RE::PlayerCharacter* a_p, float a_delta)
	{
		g_frameDelta = a_delta;
		// the graph listener, once per animation graph (a load or a change of form builds new ones). Removed and
		// added again at each glide's start, the graph left its fall for a moment and a run showed (testing 2026-10-24)
		static RE::BSAnimationGraphManager* listened = nullptr;
		if (RE::BSTSmartPointer<RE::BSAnimationGraphManager> mgr; a_p->GetAnimationGraphManager(mgr) && mgr.get() != listened) {
			listened = mgr.get();
			a_p->AddAnimationGraphEventSink(&g_graphSink);
		}
		Update(a_p, a_delta);
		Mark(a_p, g_gliding);  // every way a glide ends takes the marker off
		Looks(a_p, g_gliding);

		// test aid: each change while airborne, to find what briefly swaps the glide pose for a run
		static bool  wasGliding = false;
		static bool  wasInJump = false;
		static float airTime = 0.0f;
		bool         inJump = false;
		a_p->GetGraphVariableBool("bInJumpState", inJump);
		airTime = g_air > 0.0f ? airTime + a_delta : 0.0f;
		if (g_air > 0.0f && (g_gliding != wasGliding || inJump != wasInJump)) {
			auto* cc = a_p->GetCharController();
			logger::info("flight: at {:.2f} s in the air gliding {} graph in jump {} (state {})", airTime, g_gliding, inJump,
				cc ? static_cast<int>(cc->context.currentState) : -1);
		}
		wasGliding = g_gliding;
		wasInJump = inJump;

		// test aid: what the graph's direction blend sees while gliding and turning (the pose shook like a kite)
		static float sinceSample = 0.0f;
		if (g_gliding && (sinceSample += a_delta) >= 0.1f) {
			sinceSample = 0.0f;
			float dir = 0.0f, turn = 0.0f, speed = 0.0f;
			a_p->GetGraphVariableFloat("Direction", dir);
			a_p->GetGraphVariableFloat("TurnDelta", turn);
			a_p->GetGraphVariableFloat("Speed", speed);
			logger::info("flight: glide sample heading {:.0f} direction {:.2f} turnDelta {:.2f} speed {:.0f}",
				a_p->GetAngleZ() * 57.2958f, dir, turn, speed);
		}
	}

	void KeepFall(RE::PlayerCharacter* a_p, float a_delta)
	{
		// A jump far higher than the game's (Predator's Rush, the jump in mid-air) outlasts the jump's animation: the
		// graph gave up on it while still in the air and showed a run for a moment before the fall (seen in testing
		// 2026-10-24). At the top of the arc the fall is started here, and a graph out of its jump while airborne is
		// put back after a moment.
		static float lastVz = 0.0f;
		static float out = 0.0f;
		static float stuck = 0.0f;
		auto*        cc = a_p->GetCharController();
		if (!cc || Shift::ActiveCreature() || (!Evolve::Has(kPerk) && !Evolve::Has("predators_rush")) ||
			cc->flags.all(RE::CHARACTER_FLAGS::kSupport) || a_p->AsActorState()->IsSwimming() || g_diving) {
			lastVz = 0.0f;
			out = 0.0f;
			stuck = 0.0f;
			return;
		}
		// The engine's own trap: at the top of a jump straight up it can switch to its on-ground state with nothing
		// underneath, where gravity is off and the player sinks ~11 units a second (seen in testing 2026-10-24; other
		// jump mods show it too). A moment of that and the player is put back in the air.
		if (cc->context.currentState == RE::hkpCharacterStateType::kOnGround) {
			if ((stuck += a_delta) > 0.05f) {
				cc->context.currentState = RE::hkpCharacterStateType::kInAir;
				stuck = 0.0f;
			}
		} else {
			stuck = 0.0f;
		}
		RE::hkVector4 v;
		cc->GetLinearVelocityImpl(v);
		const float vz = v.quad.m128_f32[2] / kUnit;
		bool        inJump = true;
		a_p->GetGraphVariableBool("bInJumpState", inJump);
		out = inJump ? 0.0f : out + a_delta;
		if ((lastVz > 0.0f && vz <= 0.0f) || out > kOutOfFall) {
			a_p->NotifyAnimationGraph("JumpFall");
			out = 0.0f;
		}
		lastVz = vz;
	}

	void AfterAnimation(RE::PlayerCharacter* a_p)
	{
		if (g_gliding) {
			TurnArms(a_p);
		}
		Probe(a_p);
	}

	namespace
	{
		class Sink : public RE::BSTEventSink<RE::InputEvent*>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) override
			{
				const auto* ue = RE::UserEvents::GetSingleton();
				for (auto* e = a_event ? *a_event : nullptr; e; e = e->next) {
					auto* b = e->AsButtonEvent();
					if (!b) {
						continue;
					}
					if (b->userEvent == ue->jump) {
						if (b->IsDown()) {
							g_jumpPressed = true;
						}
						g_jumpHeld = b->IsPressed();
					} else if (b->userEvent == ue->rightAttack || b->userEvent == ue->leftAttack) {
						// once per press, when the hold becomes a power attack
						if (b->IsDown()) {
							_counted = false;
						} else if (b->IsHeld() && !_counted && b->HeldDuration() >= kPowerHold) {
							_counted = true;
							g_powerPressed = true;
						}
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}

		private:
			bool _counted{ false };
		};
	}

	void Install()
	{
		static Sink sink;
		if (auto* idm = RE::BSInputDeviceManager::GetSingleton()) {
			idm->AddEventSink<RE::InputEvent*>(&sink);
		}
	}

	void SetLooks(std::string_view a_what, std::string_view a_value)
	{
		if (a_what == "armr" || a_what == "arml") {
			RE::NiPoint3 v{};
			std::istringstream in{ std::string(a_value) };
			in >> v.x >> v.y >> v.z;
			(a_what == "armr" ? g_armR : g_armL) = v;
			logger::info("dev: glide arms right ({}, {}, {}) left ({}, {}, {})", g_armR.x, g_armR.y, g_armR.z, g_armL.x, g_armL.y, g_armL.z);
			return;
		}
		if (a_what == "fx") {
			// off first, so a glide under way shows the new set from its next start
			if (auto* p = RE::PlayerCharacter::GetSingleton(); p && g_looksOn) {
				Looks(p, false);
			}
			g_fx.clear();
			std::istringstream in{ std::string(a_value) };
			for (std::string w; in >> w;) {
				if (w != "none") {
					g_fx.push_back(static_cast<RE::FormID>(std::strtoul(w.c_str(), nullptr, 16)));
				}
			}
			std::string list;
			for (const auto id : g_fx) {
				auto* f = RE::TESForm::LookupByID(id);
				list += std::format(" {:08X}({})", id, f ? (f->Is(RE::FormType::ArtObject) ? "art" : f->Is(RE::FormType::EffectShader) ? "shader" : "?") : "missing");
			}
			logger::info("dev: glide fx{}", list);
			return;
		}
		if (a_what == "wind") {
			if (auto* p = RE::PlayerCharacter::GetSingleton(); p && g_looksOn) {
				Looks(p, false);
			}
			std::istringstream in{ std::string(a_value) };
			std::string        name;
			in >> name;
			if (float vol = 0.0f; in >> vol) {
				g_windVolume = vol;
			}
			if (int layers = 0; in >> layers) {
				g_windLayers = std::clamp(layers, 1, static_cast<int>(g_windHandles.size()));
			}
			g_wind = name == "none" ? "" : name;
			logger::info("dev: glide wind '{}' volume {:.2f} layers {}", g_wind, g_windVolume, g_windLayers);
			return;
		}
		if (a_what == "steer") {
			g_steer = a_value != "0";
			logger::info("dev: glide steering {}", g_steer);
			return;
		}
		if (a_what == "diag") {
			g_diag = std::atoi(std::string(a_value).c_str());
			logger::info("dev: glide probe logs the next {} frames in the air", g_diag);
			return;
		}
		if (a_what == "pose") {
			g_pose = std::atoi(std::string(a_value).c_str());
			logger::info("dev: glide pose {}", g_pose);
			return;
		}
		if (a_what == "sound") {
			g_sound = a_value;
		} else if (a_what == "shake") {
			g_shake = std::strtof(std::string(a_value).c_str(), nullptr);
		}
		logger::info("dev: slam sound '{}', shake {:.2f}", g_sound, g_shake);
	}
}
