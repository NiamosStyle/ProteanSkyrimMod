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
		constexpr float            kPowerHold = 0.3f;            // seconds an attack key is held to count as a power attack

		std::atomic<bool> g_jumpHeld{ false };
		std::atomic<bool> g_jumpPressed{ false };
		std::atomic<bool> g_powerPressed{ false };

		float g_air = 0.0f;         // seconds in the air
		bool  g_doubleUsed = false;
		float g_push = -1.0f;       // >= 0: the double jump's take-off, seconds in
		float g_pushVz = 0.0f;
		bool  g_gliding = false;
		float g_glideStart = 0.0f;  // the fall speed when the glide began, game units / s
		float g_glideStartAcross = 0.0f;  // the speed across the ground when it began
		float g_glideEase = 0.0f;
		bool  g_diving = false;

		std::string g_sound = "NPCDragonLandCrashShortSD";
		float       g_shake = 1.0f;

		float Dial(std::string_view a_name, float a_fallback) { return Registry::Get().Dial(a_name, a_fallback); }

		// as high as the player's own jump (Predator's Rush's when owned), unless the dial says otherwise
		float JumpHeight()
		{
			if (const float dial = Dial("Protean_FlightJumpHeight", 0.0f); dial > 0.0f) {
				return dial;
			}
			auto* gs = RE::GameSettingCollection::GetSingleton();
			auto* jump = gs ? gs->GetSetting("fJumpHeightMin") : nullptr;
			return jump && jump->GetFloat() > 0.0f ? jump->GetFloat() : 76.0f;
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
			a_cc->context.currentState = RE::hkpCharacterStateType::kInAir;
			a_cc->SetLinearVelocityImpl(RE::hkVector4(a_vx, a_vy, a_vz * kUnit, 0.0f));
		}

		void Reset()
		{
			g_air = 0.0f;
			g_doubleUsed = false;
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

	void OnFrame(RE::PlayerCharacter* a_p, float a_delta)
	{
		auto* cc = a_p->GetCharController();
		const bool jumpPressed = g_jumpPressed.exchange(false);
		const bool powerPressed = g_powerPressed.exchange(false);
		if (!cc || a_p->IsDead() || !Evolve::Has(kPerk)) {
			Reset();
			return;
		}

		// no fall damage: the drop is always measured from here
		RE::hkVector4 pos;
		cc->GetPositionImpl(pos, false);
		cc->fallStartHeight = pos.quad.m128_f32[2] / kUnit;
		cc->fallTime = 0.0f;

		const auto state = cc->context.currentState;
		const bool air = state == RE::hkpCharacterStateType::kInAir || state == RE::hkpCharacterStateType::kJumping;
		if (Shift::ActiveCreature() || !air || a_p->AsActorState()->IsSwimming()) {
			if (g_diving && state == RE::hkpCharacterStateType::kOnGround) {
				Slam(a_p);
			}
			Reset();
			return;
		}
		g_air += a_delta;
		const auto v = Velocity(cc);
		const float vz = v.quad.m128_f32[2] / kUnit;

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

		if (jumpPressed && !g_doubleUsed && g_push < 0.0f) {
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
				g_gliding = true;
				g_glideStart = vz;
				g_glideStartAcross = std::sqrt(vx * vx + vy * vy);
				g_glideEase = 0.0f;
			}
			Spend(a_p, cost);
			g_glideEase = std::min(g_glideEase + a_delta * kEase, 1.0f);
			const float down = -std::abs(Dial("Protean_GlideFallSpeed", 300.0f));
			// forward where the player faces, at least the glide's speed (a faster run going in keeps its speed)
			const float across = std::max(g_glideStartAcross, Dial("Protean_GlideSpeed", 900.0f));
			const float speed = g_glideStartAcross + (across - g_glideStartAcross) * g_glideEase;
			const float heading = a_p->GetAngleZ();
			SetVz(cc, std::sin(heading) * speed * kUnit, std::cos(heading) * speed * kUnit,
				g_glideStart + (down - g_glideStart) * g_glideEase);
		} else {
			g_gliding = false;
		}
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
		if (a_what == "sound") {
			g_sound = a_value;
		} else if (a_what == "shake") {
			g_shake = std::strtof(std::string(a_value).c_str(), nullptr);
		}
		logger::info("dev: slam sound '{}', shake {:.2f}", g_sound, g_shake);
	}
}
