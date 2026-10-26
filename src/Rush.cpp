#include "Rush.h"

#include "Evolve.h"
#include "Registry.h"
#include "Shift.h"
#include "State.h"

namespace protean::Rush
{
	namespace
	{
		constexpr std::string_view kPerk = "predators_rush";
		constexpr float            kVanillaJump = 76.0f;   // fJumpHeightMin, Skyrim.esm
		constexpr float            kVanillaFall = 600.0f;  // fJumpFallHeightMin, Skyrim.esm: a drop below it is harmless

		float g_sprintTime = 0.0f;  // seconds of unbroken sprinting
		float g_jumpOriginal = -1.0f;
		float g_fallOriginal = -1.0f;

		// Sprint jump. The game's animation graph has no way from a sprint into a jump, so Jump does nothing while
		// sprinting. With the perk Protean starts the jump itself: the graph's jump event, and the take-off speed set
		// for the first moments (the engine would otherwise keep the player on the ground). The sprint's build-up is
		// kept through the air, so landing into a sprint goes on at the same speed.
		constexpr float kGravity = 9.80665f / 0.0142875f;  // the engine's fall, game units / s^2
		constexpr float kPush = 0.12f;                      // seconds the take-off speed is held
		constexpr float kKeepRamp = 0.5f;                   // seconds after landing the build-up waits for a sprint
		struct SprintJump
		{
			bool  on{ false };
			float t{ 0.0f };
			float vz0{ 0.0f };
			float vx{ 0.0f };  // havok units / s, taken at the jump
			float vy{ 0.0f };
		};
		SprintJump g_jump;
		float      g_keepRamp = 0.0f;  // > 0: the build-up survives a moment without sprinting

		// Falls at speed (the user died running fast over rocks, 2026-10-25): the ground height last stood on, and
		// whether the player left it during a boosted sprint. Landing no more than Protean_RushSafeDrop below it counts as
		// no fall; a deeper drop is left to the game.
		// The game's own fall mark is not put back by a moment's footing: down a rocky slope it was left 1,100-1,200
		// units above ground the player had just stood on, and a 16-unit step killed (two deaths, 2026-10-25). Any frame
		// with footing puts it at the feet, so a fall counts from the last ground touched.
		float g_groundZ = 0.0f;
		bool  g_fastAir = false;
		bool  g_deathLogged = false;
		float g_rushSeen = 0.0f;    // the Rush speed before this frame's (a death takes it off before the log)
		bool  g_sprintSeen = false;
		float g_airTime = 0.0f;     // seconds since the last footing

		float Dial(std::string_view a_name, float a_fallback) { return Registry::Get().Dial(a_name, a_fallback); }

		// SpeedMult points added on top of the player's own; the engine takes a new speed only when something makes
		// it recompute movement, hence the carry weight nudge (as Move's sprint does)
		void Apply(RE::PlayerCharacter* a_p, float a_want)
		{
			auto& s = State::Get();
			if (std::abs(a_want - s.rushApplied) < 0.5f && !(a_want == 0.0f && s.rushApplied != 0.0f)) {
				return;
			}
			auto* av = a_p->AsActorValueOwner();
			av->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kSpeedMult, a_want - s.rushApplied);
			s.rushApplied = a_want;
			av->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kCarryWeight, 0.1f);
			av->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kCarryWeight, -0.1f);
		}

		bool Works(RE::PlayerCharacter* a_p) { return !Shift::ActiveCreature() && Evolve::Has(kPerk) && !a_p->IsDead(); }

		void StartSprintJump()
		{
			auto* p = RE::PlayerCharacter::GetSingleton();
			auto* cc = p ? p->GetCharController() : nullptr;
			if (!cc || g_jump.on || !Works(p) || !p->AsActorState()->IsSprinting() || !cc->flags.all(RE::CHARACTER_FLAGS::kSupport)) {
				return;
			}
			const bool dir = p->NotifyAnimationGraph("JumpDirectionalStart");
			const bool standing = !dir && p->NotifyAnimationGraph("JumpStandingStart");
			RE::hkVector4 v;
			cc->GetLinearVelocityImpl(v);
			const float h = Dial("Protean_RushJumpHeight", 400.0f);
			g_jump = { true, 0.0f, std::sqrt(2.0f * kGravity * h), v.quad.m128_f32[0], v.quad.m128_f32[1] };
			g_keepRamp = 1000.0f;
			// the game's fall mark is not reset when Protean launches the jump (as the hare's leap, which died of a fall
			// counted from an old height): the jump counts from where it starts
			cc->fallStartHeight = p->GetPositionZ();
			logger::info("sprint jump: graph took JumpDirectionalStart {} / JumpStandingStart {}; {:.0f} u/s up", dir, standing,
				g_jump.vz0);
		}

		void UpdateSprintJump(RE::PlayerCharacter* a_p, float a_delta)
		{
			auto* cc = a_p->GetCharController();
			if (!g_jump.on || !cc) {
				return;
			}
			g_jump.t += a_delta;
			if (g_jump.t <= kPush) {
				const float s = RE::bhkWorld::GetWorldScale();
				cc->context.currentState = RE::hkpCharacterStateType::kInAir;
				cc->SetLinearVelocityImpl(RE::hkVector4(g_jump.vx, g_jump.vy, (g_jump.vz0 - kGravity * g_jump.t) * s, 0.0f));
			} else if (cc->context.currentState == RE::hkpCharacterStateType::kOnGround) {
				g_jump.on = false;
				g_keepRamp = kKeepRamp;
			}
		}

		class Sink : public RE::BSTEventSink<RE::InputEvent*>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) override
			{
				const auto* ue = RE::UserEvents::GetSingleton();
				for (auto* e = a_event ? *a_event : nullptr; e; e = e->next) {
					if (auto* b = e->AsButtonEvent(); b && b->userEvent == ue->jump && b->IsDown()) {
						SKSE::GetTaskInterface()->AddTask([] { StartSprintJump(); });
						break;
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// the jump height, and the drop that starts fall damage raised by as much, so a big jump hurts no more than a
		// normal one would from the same spot (real falls still do; only Prey's Flight takes fall damage away)
		void SetJump(bool a_raised)
		{
			auto* gs = RE::GameSettingCollection::GetSingleton();
			auto* jump = gs ? gs->GetSetting("fJumpHeightMin") : nullptr;
			auto* fall = gs ? gs->GetSetting("fJumpFallHeightMin") : nullptr;
			if (!jump || !fall) {
				return;
			}
			if (g_jumpOriginal < 0.0f) {
				g_jumpOriginal = jump->GetFloat() > 0.0f ? jump->GetFloat() : kVanillaJump;
				g_fallOriginal = fall->GetFloat() > 0.0f ? fall->GetFloat() : kVanillaFall;
			}
			const float height = a_raised ? Dial("Protean_RushJumpHeight", 400.0f) : g_jumpOriginal;
			jump->data.f = height;
			fall->data.f = g_fallOriginal + std::max(height - g_jumpOriginal, 0.0f);
		}
	}

	namespace
	{
		void GuardFall(RE::PlayerCharacter* a_p, bool a_boosted, float a_delta)
		{
			auto* cc = a_p->GetCharController();
			if (a_p->IsDead()) {
				if (!g_deathLogged && !Shift::ActiveCreature()) {
					g_deathLogged = true;
					logger::info("player died in human form at z {:.0f} (ground {:.0f}, {:.2f} s before, fall mark {:.0f}), Rush speed +{:.0f}{}, in the air {}",
						a_p->GetPositionZ(), g_groundZ, g_airTime, cc ? cc->fallStartHeight : 0.0f, g_rushSeen,
						g_sprintSeen ? " sprinting" : "", a_p->IsInMidair());
				}
				return;
			}
			g_deathLogged = false;
			if (!cc) {
				return;
			}
			const float z = a_p->GetPositionZ();
			if (cc->flags.all(RE::CHARACTER_FLAGS::kSupport)) {
				g_groundZ = z;
				g_fastAir = false;
				g_airTime = 0.0f;
				cc->fallStartHeight = z;
				return;
			}
			g_airTime += a_delta;
			g_fastAir = g_fastAir || a_boosted;
			if (g_fastAir && g_groundZ - z <= Dial("Protean_RushSafeDrop", 300.0f)) {
				cc->fallStartHeight = z;  // landing here would be a hop, not a fall
			}
		}
	}

	void OnFrame(RE::PlayerCharacter* a_p, float a_delta)
	{
		if (!a_p->IsDead()) {
			g_rushSeen = State::Get().rushApplied;
			g_sprintSeen = a_p->AsActorState()->IsSprinting();
		}
		const bool works = Works(a_p);
		UpdateSprintJump(a_p, a_delta);
		const bool sprinting = works && a_p->AsActorState()->IsSprinting();
		if (sprinting) {
			g_sprintTime += a_delta;
			g_keepRamp = 0.0f;
		} else if (g_keepRamp > 0.0f) {
			g_keepRamp -= g_jump.on ? 0.0f : a_delta;  // in the air: the build-up and its speed are kept
		} else {
			g_sprintTime = 0.0f;
		}

		float want = 0.0f;
		if (works) {
			// as a share of the player's own speed: the walk/run bonus always, the sprint's build-up while sprinting
			float mult = 1.0f + Dial("Protean_RushSpeed", 0.1f);
			if (sprinting || g_keepRamp > 0.0f) {
				const float ramp = std::max(Dial("Protean_RushRampSeconds", 10.0f), 0.1f);
				const float t = std::min(g_sprintTime / ramp, 1.0f);
				const float start = Dial("Protean_RushSprintStart", 1.25f);
				mult = std::max(mult, start + (Dial("Protean_RushSprintTop", 3.0f) - start) * t);
			}
			want = (mult - 1.0f) * 100.0f;
		}
		Apply(a_p, want);
		SetJump(works);
		GuardFall(a_p, works && (sprinting || g_keepRamp > 0.0f), a_delta);
	}

	void Install()
	{
		static Sink sink;
		if (auto* idm = RE::BSInputDeviceManager::GetSingleton()) {
			idm->AddEventSink<RE::InputEvent*>(&sink);
		}
	}
}
