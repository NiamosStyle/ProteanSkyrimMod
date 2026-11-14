#include "Devastator.h"

#include "Evolve.h"
#include "Registry.h"
#include "Shift.h"
#include "State.h"

namespace protean::Devastator
{
	namespace
	{
		using Clock = std::chrono::steady_clock;

		// the ritual pose (picked by the user from four, 2026-10-27; the change into a beast stretched the arms and neck)
		constexpr std::string_view kRise = "IdleRitualStart";
		// sounds picked by ear in game (2026-10-27): the change into a beast as the pose builds, then Miraak's tentacle
		// stab with Unrelenting Force's thunder as the tendrils burst out
		constexpr const char* kWindupSound = "NPCWerewolfTransformationSD";
		// the burst with more weight (picked in game, 2026-10-27): the Storm Atronach's thunder blast, the Serpent Stone's
		// explosion and Alduin's death explosion over the stab and Unrelenting Force
		constexpr const char* kBurstSounds[] = { "QSTMiraakDeathTentacleStab", "VOCShoutFXUnrelentingForceThunderA",
			"NPCAtronachStormAttackPowerForwardBombThunder2D", "MAGPowerSerpentStoneExplosion", "QSTAlduinDeathExplosionASD",
			"AMBWeatherThunderExtra" };  // and storm thunder rolling on after it
		// the look, picked in game the same day: red tentacles writhe over the body from the start of the pose (Protean's
		// red copy of Dragonborn's tentacle shader); at the burst the red lurker splash and the Vampire Lord's blood burst
		// (the one a change of form shows)
		constexpr RE::FormID kBloodBurst = 0x015373;  // DLC1VampChangeExplosion (Dawnguard.esm)
		// the moment after the burst is held (the user, 2026-10-27: the pose kept, the tendrils out, the camera circling in
		// slow motion): Protean_DevastatorHold real seconds, slow for Protean_DevastatorSlowSeconds of them, the tendrils
		// thrown again every Protean_DevastatorRethrow so they stay out
		float g_rethrow{ 0.0f };
		constexpr float            kSheatheMax = 2.0f;     // seconds the pose is asked for before going on without it
		// the pose was taken by the graph and stopped the same instant (IdleStop, 2026-11-05, hands already empty, so asked
		// for at once); guessed cause: the player still moving. So it waits for the player to stand (at most kStillMax s),
		// and a pose stopped in its first kStopWatch s is asked for again, up to kRetries times
		constexpr float            kStillMax = 0.6f;
		constexpr float            kStillSpeed = 0.3f;     // horizontal speed (Havok units a second) that counts as standing
		constexpr float            kStopWatch = 0.6f;
		constexpr int              kRetries = 3;
		std::atomic<bool>          g_stopped{ false };    // set by the animation events (another thread)
		int                        g_retries{ 0 };

		enum class Phase
		{
			kNone,
			kSheathe,  // asking for the pose each frame until it plays: it is refused with weapons out, and still while
			           // they are being put away (their state reads put away at once, 2026-10-27)
			kWindup,
			kAfter
		};

		Phase             g_phase{ Phase::kNone };
		std::map<RE::FormID, Clock::time_point> g_struck;  // who each burst struck, and when: their deaths feed no boost
		Clock::time_point g_start{};
		Clock::time_point g_last{};
		bool              g_slow{ false };

		using Flag = RE::ControlMap::UEFlag;
		constexpr auto kLocked = static_cast<Flag>(std::to_underlying(Flag::kMovement) | std::to_underlying(Flag::kLooking) |
		                                           std::to_underlying(Flag::kJumping) | std::to_underlying(Flag::kFighting) |
		                                           std::to_underlying(Flag::kActivate) | std::to_underlying(Flag::kSneaking));

		float Dial(std::string_view a_name, float a_fallback) { return Registry::Get().Dial(a_name, a_fallback); }

		// the player's animation events while the pose is asked for and building, in the log (2026-11-04: once the pose
		// was taken by the graph yet the player only stood still, after the claws were put away)
		class AnimLog : public RE::BSTEventSink<RE::BSAnimationGraphEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override
			{
				if (a_event && (g_phase == Phase::kSheathe || g_phase == Phase::kWindup)) {
					const float in = std::chrono::duration<float>(Clock::now() - g_start).count();
					logger::info("devastator: anim event '{}' {:.2f} s in", a_event->tag.c_str(), in);
					if (g_phase == Phase::kWindup && in < kStopWatch && a_event->tag == "IdleStop") {
						g_stopped = true;
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
		AnimLog g_animLog;

		float Seconds(Clock::time_point a_from) { return std::chrono::duration<float>(Clock::now() - a_from).count(); }

		void Play(RE::PlayerCharacter* a_p, const char* a_name)
		{
			RE::BSSoundHandle h;
			if (auto* audio = RE::BSAudioManager::GetSingleton()) {
				audio->GetSoundHandleByName(h, a_name, 0x1A);
			}
			if (h.IsValid()) {
				h.SetPosition(a_p->GetPosition());
				h.Play();
			} else {
				logger::warn("devastator: no sound '{}'", a_name);
			}
		}

		void Lock(bool a_on)
		{
			if (auto* cm = RE::ControlMap::GetSingleton()) {
				cm->ToggleControls(kLocked, !a_on, false);
			}
		}

		void Slow(bool a_on)
		{
			if (auto* t = RE::BSTimer::GetSingleton(); t && g_slow != a_on) {
				t->SetGlobalTimeMultiplier(a_on ? Dial("Protean_DevastatorSlowMo", 0.15f) : 1.0f, true);
				g_slow = a_on;
			}
		}

		// The circling: the game's own idle camera, which slowly orbits a player left alone (the free rotation of the
		// third-person camera did nothing under the crouch and rise's own camera, 2026-10-27)
		RE::TESCameraState* State(RE::CameraStates::CameraState a_id)
		{
			auto* cam = RE::PlayerCamera::GetSingleton();
			return cam ? cam->GetRuntimeData().cameraStates[a_id].get() : nullptr;
		}

		int CameraNow()
		{
			auto* cam = RE::PlayerCamera::GetSingleton();
			auto* st = cam ? cam->currentState.get() : nullptr;
			return st ? static_cast<int>(st->id) : -1;
		}

		void CameraStart()
		{
			auto* cam = RE::PlayerCamera::GetSingleton();
			if (auto* vanity = State(RE::CameraStates::kAutoVanity); cam && vanity) {
				cam->SetState(vanity);
			}
			logger::info("devastator: camera state {} (auto vanity is {})", CameraNow(), static_cast<int>(RE::CameraStates::kAutoVanity));
		}

		void CameraKeep()
		{
			auto* cam = RE::PlayerCamera::GetSingleton();
			if (auto* vanity = State(RE::CameraStates::kAutoVanity); cam && vanity && cam->currentState.get() != vanity) {
				static int logged = 0;
				if (logged++ < 3) {
					logger::info("devastator: camera went to state {}; back to the orbit", CameraNow());
				}
				cam->SetState(vanity);
			}
		}

		// From the burst on the circling turns faster and stands farther out (the user, 2026-10-27): the game's own settings
		// for the idle camera, raised for the moment and put back after
		struct Boost
		{
			const char* name;
			float       was{ 0.0f };
			bool        on{ false };
		};
		Boost g_boost[] = { { "fAutoVanityIncrement:Camera" }, { "fVanityModeMaxDist:Camera" }, { "fDefaultAutoVanityZoom:Camera" } };

		void CameraBoost(bool a_on)
		{
			auto* ini = RE::INISettingCollection::GetSingleton();
			if (!ini) {
				return;
			}
			const float mult[] = { Dial("Protean_DevastatorOrbitMult", 3.0f), Dial("Protean_DevastatorZoomMult", 2.0f),
				Dial("Protean_DevastatorZoomMult", 2.0f) };
			for (std::size_t i = 0; i < std::size(g_boost); ++i) {
				auto& b = g_boost[i];
				auto* s = ini->GetSetting(b.name);
				if (!s || s->GetType() != RE::Setting::Type::kFloat || b.on == a_on) {
					continue;
				}
				if (a_on) {
					b.was = s->GetFloat();
					s->data.f = b.was * mult[i];
					logger::info("devastator: camera {} {:.2f} -> {:.2f}", b.name, b.was, s->data.f);
				} else {
					s->data.f = b.was;
				}
				b.on = a_on;
			}
		}

		void CameraEnd()
		{
			CameraBoost(false);
			if (auto* cam = RE::PlayerCamera::GetSingleton()) {
				cam->ForceThirdPerson();
			}
		}

		// The tendrils thrown for the look: cast the ordinary way (Protean's own launch showed nothing, 2026-10-27), each
		// at an unseen marker placed on a ring around the player, slightly up and down in turn. Each from its own unseen
		// caster at the chest, as fire traps cast: the tendril is a stream, and one caster keeps one stream (cast 8 times by
		// the player, only the last showed)
		std::vector<RE::ObjectRefHandle> g_marks;
		std::vector<RE::ObjectRefHandle> g_sources;

		void PlaceMarks(RE::PlayerCharacter* a_p)
		{
			auto* base = RE::TESForm::LookupByID<RE::TESBoundObject>(0x3B);  // XMarker, Skyrim.esm
			const int n = std::max(static_cast<int>(Dial("Protean_DevastatorTendrils", 8.0f)), 1);
			const auto  at = a_p->GetPosition();
			const float out = Dial("Protean_DevastatorTendrilOut", 0.0f);
			for (int i = 0; base && i < n; ++i) {
				const float a = a_p->GetAngleZ() + 6.2831853f * i / n;
				auto m = a_p->PlaceObjectAtMe(base, false);
				auto src = a_p->PlaceObjectAtMe(base, false);
				if (m && src) {
					m->SetPosition(RE::NiPoint3(at.x + 700.0f * std::sin(a), at.y + 700.0f * std::cos(a), at.z + (i % 2 ? 40.0f : 220.0f)));
					src->SetPosition(RE::NiPoint3(at.x + out * std::sin(a), at.y + out * std::cos(a), at.z + Dial("Protean_DevastatorTendrilHeight", 60.0f)));
					g_marks.push_back(m->GetHandle());
					g_sources.push_back(src->GetHandle());
				}
			}
		}

		void ClearMarks()
		{
			for (auto* list : { &g_marks, &g_sources }) {
				for (auto& h : *list) {
					if (auto m = h.get()) {
						m->Disable();
						m->SetDelete(true);
					}
				}
				list->clear();
			}
		}

		void Push(RE::PlayerCharacter* a_p, RE::Actor* a_target, float a_force)
		{
			auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			if (!vm) {
				return;
			}
			const auto handle = vm->GetObjectHandlePolicy()->GetHandleForObject(a_p->GetFormType(), a_p);
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> cb;
			vm->DispatchMethodCall2(handle, "ObjectReference", "PushActorAway", RE::MakeFunctionArguments(std::move(a_target), std::move(a_force)), cb);
		}

		int Tendrils(RE::PlayerCharacter* a_p)
		{
			auto* spell = Registry::Get().DevastatorTendril();
			if (!spell) {
				return 0;
			}
			int cast = 0;
			for (std::size_t i = 0; i < g_marks.size() && i < g_sources.size(); ++i) {
				auto m = g_marks[i].get();
				auto src = g_sources[i].get();
				auto* caster = src ? src->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant) : nullptr;
				if (m && caster) {
					caster->CastSpellImmediate(spell, false, m.get(), 1.0f, false, 0.0f, a_p);
					++cast;
				}
			}
			return cast;
		}

		void Burst(RE::PlayerCharacter* a_p)
		{
			const float radius = Dial("Protean_DevastatorRadius", 2100.0f);
			const float damage = Dial("Protean_DevastatorDamage", 100.0f) + Dial("Protean_DevastatorDamagePerLevel", 8.0f) * a_p->GetLevel();
			const float force = Dial("Protean_DevastatorForce", 10.0f);
			logger::info("devastator: {} tendril(s) cast", Tendrils(a_p));
			if (auto* splash = Registry::Get().DevastatorSplash()) {
				a_p->PlaceObjectAtMe(splash, false);
			}
			if (auto* dh = RE::TESDataHandler::GetSingleton()) {
				if (auto* blood = dh->LookupForm<RE::BGSExplosion>(kBloodBurst, "Dawnguard.esm")) {
					a_p->PlaceObjectAtMe(blood, false);
				}
			}
			for (const auto* sound : kBurstSounds) {
				Play(a_p, sound);
			}
			int  hit = 0;
			auto* pl = RE::ProcessLists::GetSingleton();
			if (pl) {
				const auto here = a_p->GetPosition();
				for (auto& h : pl->highActorHandles) {
					auto a = h.get();
					if (!a || a.get() == a_p || a->IsDead()) {
						continue;
					}
					// the ones passed over near enough to matter are logged with the reason (a bandit on a ledge was
					// missed, 2026-10-27, and the log could not say why)
					const float dist = here.GetDistance(a->GetPosition());
					const char* skip = a->IsPlayerTeammate()      ? "a follower" :
					                   !a->IsHostileToActor(a_p) ? "not hostile" :
					                   dist > radius             ? "too far" :
					                                               nullptr;
					if (skip) {
						if (dist <= radius * 2.0f && (a->IsInCombat() || a->IsHostileToActor(a_p))) {
							logger::info("devastator: '{}' {:08X} at {:.0f} passed over ({})", a->GetName(), a->GetFormID(), dist, skip);
						}
						continue;
					}
					Push(a_p, a.get(), force);
					g_struck[a->GetFormID()] = Clock::now();
					a->AsActorValueOwner()->DamageActorValue(RE::ActorValue::kHealth, damage);
					logger::info("devastator: '{}' {:08X} at {:.0f}: knocked down, {:.0f} damage, health now {:.0f}", a->GetName(), a->GetFormID(),
						here.GetDistance(a->GetPosition()), damage, a->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth));
					++hit;
				}
			}
			logger::info("devastator: burst, {} enemy(ies) within {:.0f}", hit, radius);
		}

		// The tentacles end with the rest: their time runs in game time, which the slow motion stretches (they stayed 3-4 s
		// after everything else, 2026-10-27), so they are stopped here and only their own fade is left. Papyrus's
		// EffectShader.Stop did not stop them (still 4 s, the same day): the running effect is ended directly
		void StopShader(RE::PlayerCharacter* a_p)
		{
			auto* fx = Registry::Get().DevastatorShader();
			auto* lists = RE::ProcessLists::GetSingleton();
			if (!fx || !lists) {
				return;
			}
			int stopped = 0;
			lists->ForEachShaderEffect([&](RE::ShaderReferenceEffect* a_e) {
				if (a_e && a_e->effectData == fx && a_e->target.get().get() == a_p) {
					a_e->lifetime = a_e->age;
					a_e->finished = true;
					++stopped;
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});
			logger::info("devastator: {} tentacle effect(s) stopped", stopped);
		}

		float Speed(RE::PlayerCharacter* a_p)
		{
			auto* cc = a_p->GetCharController();
			if (!cc) {
				return 0.0f;
			}
			RE::hkVector4 v;
			cc->GetLinearVelocityImpl(v);
			return std::hypot(v.quad.m128_f32[0], v.quad.m128_f32[1]);
		}

		void Finish(RE::PlayerCharacter* a_p)
		{
			StopShader(a_p);
			Slow(false);
			CameraEnd();
			ClearMarks();
			Lock(false);
			a_p->NotifyAnimationGraph("IdleForceDefaultState");
			g_phase = Phase::kNone;
			logger::info("devastator: over");
		}

		// the pose, its sound, the red tentacles, the circling camera and the tendrils' markers
		void Begin(RE::PlayerCharacter* a_p, bool a_took)
		{
			const float waited = Seconds(g_start);
			Play(a_p, kWindupSound);
			if (auto* fx = Registry::Get().DevastatorShader()) {
				a_p->ApplyEffectShader(fx, Dial("Protean_DevastatorWindup", 2.0f) + Dial("Protean_DevastatorHold", 4.5f));
			}
			CameraStart();
			PlaceMarks(a_p);
			g_phase = Phase::kWindup;
			g_start = g_last = Clock::now();
			g_stopped = false;
			g_retries = 0;
			logger::info("devastator: wind-up (the rise {} after {:.2f} s, weapon state {})", a_took ? "playing" : "refused by the graph",
				waited, std::to_underlying(a_p->AsActorState()->GetWeaponState()));
		}
	}

	void OnCast()
	{
		auto* p = RE::PlayerCharacter::GetSingleton();
		if (!p || g_phase != Phase::kNone || !Evolve::Has("devastator")) {
			return;
		}
		if (Shift::ActiveCreature()) {
			Shift::Notify("Devastator needs human form.");
			return;
		}
		const auto need = static_cast<std::uint32_t>(Dial("Protean_DevastatorStacks", 2.0f));
		if (Evolve::BoostStacks() < need) {
			Shift::Notify(std::format("Devastator needs {} biomass boosts. Kill enemies to gain them.", need));
			return;
		}
		auto*       av = p->AsActorValueOwner();
		const float stamina = av->GetActorValue(RE::ActorValue::kStamina);
		if (stamina < av->GetPermanentActorValue(RE::ActorValue::kStamina) - 1.0f) {
			Shift::Notify("Devastator needs all your Stamina.");
			return;
		}
		av->DamageActorValue(RE::ActorValue::kStamina, stamina);
		p->RemoveAnimationGraphEventSink(&g_animLog);  // the graphs are rebuilt on loads: listen afresh each time
		p->AddAnimationGraphEventSink(&g_animLog);
		Lock(true);
		g_start = g_last = Clock::now();
		if (p->AsActorState()->GetWeaponState() != RE::WEAPON_STATE::kSheathed) {
			p->DrawWeaponMagicHands(false);
			logger::info("devastator: the weapons put away first");
		}
		g_phase = Phase::kSheathe;
	}

	void OnFrame(RE::PlayerCharacter* a_p)
	{
		if (g_phase == Phase::kNone) {
			return;
		}
		g_last = Clock::now();
		const float t = Seconds(g_start);
		if (g_phase == Phase::kSheathe) {
			if (a_p->IsDead()) {
				Lock(false);
				g_phase = Phase::kNone;
			} else if (const float speed = Speed(a_p); speed > kStillSpeed && t < kStillMax) {
				// still moving: wait
			} else if (a_p->NotifyAnimationGraph(kRise)) {
				logger::info("devastator: the rise asked for at speed {:.2f}", Speed(a_p));
				Begin(a_p, true);
			} else if (t >= kSheatheMax) {
				Begin(a_p, false);
			}
			return;
		}
		CameraKeep();
		if (g_phase == Phase::kWindup && g_stopped.exchange(false) && g_retries < kRetries) {
			++g_retries;
			const bool took = a_p->NotifyAnimationGraph(kRise);
			logger::info("devastator: the rise was stopped; asked again ({}), {}, speed {:.2f}", g_retries, took ? "taken" : "refused",
				Speed(a_p));
		}
		if (a_p->IsDead()) {
			Finish(a_p);
			return;
		}
		if (g_phase == Phase::kWindup && t >= Dial("Protean_DevastatorWindup", 2.0f)) {
			Slow(true);
			CameraBoost(true);
			Burst(a_p);
			g_phase = Phase::kAfter;
			g_start = Clock::now();
			g_rethrow = 0.0f;
			return;
		}
		if (g_phase == Phase::kAfter) {
			if (t >= Dial("Protean_DevastatorSlowSeconds", 3.5f)) {
				Slow(false);
			}
			if (t >= Dial("Protean_DevastatorHold", 4.5f)) {
				Finish(a_p);
				return;
			}
			if (t >= g_rethrow + Dial("Protean_DevastatorRethrow", 0.25f)) {
				g_rethrow = t;
				Tendrils(a_p);
			}
		}
	}

	bool Struck(RE::FormID a_actor)
	{
		const auto it = g_struck.find(a_actor);
		return it != g_struck.end() && Seconds(it->second) < 15.0f;  // the knock-down and its fall take a few seconds
	}

	void Reset()
	{
		g_struck.clear();
		if (g_phase != Phase::kNone) {
			Slow(false);
			CameraBoost(false);
			Lock(false);
			g_marks.clear();
			g_sources.clear();
			g_phase = Phase::kNone;
		}
	}
}

namespace protean::Regeneration
{
	void OnCast()
	{
		auto& s = State::Get();
		if (!Evolve::Has("regeneration")) {
			return;
		}
		s.regenOn = !s.regenOn;
		Shift::Notify(s.regenOn ? "Regeneration on: Stamina turns into Health." : "Regeneration off.");
		logger::info("regeneration: {}", s.regenOn ? "on" : "off");
	}

	void OnFrame(RE::PlayerCharacter* a_p, float a_delta)
	{
		auto& s = State::Get();
		if (auto* power = Registry::Get().RegenPower(); power && Evolve::Has("regeneration") && !a_p->HasSpell(power)) {
			a_p->AddSpell(power);
			logger::info("regeneration: the power given");
		}
		// Devastator's power comes with the perk, so it can sit in Favorites before the moment comes (it was missing, 2026-10-27)
		if (auto* power = Registry::Get().DevastatorPower(); power && Evolve::Has("devastator") && !a_p->HasSpell(power)) {
			a_p->AddSpell(power);
			logger::info("devastator: the power given");
		}
		if (!s.regenOn || a_p->IsDead() || a_delta <= 0.0f) {
			return;
		}
		if (!Evolve::Has("regeneration")) {
			s.regenOn = false;
			return;
		}
		auto*       av = a_p->AsActorValueOwner();
		const float missing = av->GetPermanentActorValue(RE::ActorValue::kHealth) +
		                      a_p->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth) -
		                      av->GetActorValue(RE::ActorValue::kHealth);
		const float give = std::min({ Registry::Get().Dial("Protean_RegenPerSecond", 8.0f) * a_delta, missing,
			av->GetActorValue(RE::ActorValue::kStamina) });
		if (give <= 0.0f) {
			return;
		}
		av->DamageActorValue(RE::ActorValue::kStamina, give);
		av->RestoreActorValue(RE::ActorValue::kHealth, give);
	}
}
