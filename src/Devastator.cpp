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

		constexpr std::string_view kRise = "IdleWerewolfTransformation";  // the hunched crouch and rise of the change into a beast
		constexpr float            kSlowSeconds = 0.5f;   // real seconds of slow motion at the burst
		constexpr float            kAfterSeconds = 2.0f;  // real seconds the camera keeps circling after the burst
		constexpr float            kOrbit = 0.6f;          // radians a real second the camera circles
		constexpr float            kStruckFor = 5.0f;      // seconds a struck actor's death is not the player's kill

		enum class Phase
		{
			kNone,
			kWindup,
			kAfter
		};

		Phase             g_phase{ Phase::kNone };
		Clock::time_point g_start{};
		Clock::time_point g_last{};
		bool              g_slow{ false };
		float             g_zoom{ -1.0f };
		std::map<RE::FormID, Clock::time_point> g_struck;

		using Flag = RE::ControlMap::UEFlag;
		constexpr auto kLocked = static_cast<Flag>(std::to_underlying(Flag::kMovement) | std::to_underlying(Flag::kLooking) |
		                                           std::to_underlying(Flag::kJumping) | std::to_underlying(Flag::kFighting) |
		                                           std::to_underlying(Flag::kActivate) | std::to_underlying(Flag::kSneaking));

		float Dial(std::string_view a_name, float a_fallback) { return Registry::Get().Dial(a_name, a_fallback); }

		float Seconds(Clock::time_point a_from) { return std::chrono::duration<float>(Clock::now() - a_from).count(); }

		RE::ThirdPersonState* Tps()
		{
			auto* cam = RE::PlayerCamera::GetSingleton();
			if (!cam || !cam->IsInThirdPerson()) {
				return nullptr;
			}
			return static_cast<RE::ThirdPersonState*>(cam->GetRuntimeData().cameraStates[RE::CameraStates::kThirdPerson].get());
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
				t->SetGlobalTimeMultiplier(a_on ? Dial("Protean_DevastatorSlowMo", 0.33f) : 1.0f, true);
				g_slow = a_on;
			}
		}

		void CameraStart()
		{
			if (auto* cam = RE::PlayerCamera::GetSingleton(); cam && !cam->IsInThirdPerson()) {
				cam->ForceThirdPerson();
			}
			if (auto* tps = Tps()) {
				g_zoom = tps->targetZoomOffset;
				tps->targetZoomOffset = 1.0f;  // as far out as the game's camera goes
				tps->freeRotationEnabled = true;
			}
		}

		void CameraTurn(float a_dt)
		{
			if (auto* tps = Tps()) {
				tps->freeRotationEnabled = true;
				tps->freeRotation.x += kOrbit * a_dt;
				tps->targetZoomOffset = 1.0f;
			}
		}

		void CameraEnd()
		{
			if (auto* tps = Tps()) {
				tps->ResetFreeRotation();
				tps->freeRotationEnabled = false;
				if (g_zoom >= 0.0f) {
					tps->targetZoomOffset = g_zoom;
				}
			}
			g_zoom = -1.0f;
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

		// the look: tendrils thrown from the chest in a ring, slightly up and down in turn
		void Tendrils(RE::PlayerCharacter* a_p)
		{
			auto* spell = Registry::Get().DevastatorTendril();
			if (!spell) {
				return;
			}
			const int  n = std::max(static_cast<int>(Dial("Protean_DevastatorTendrils", 8.0f)), 1);
			auto       from = a_p->GetPosition();
			from.z += 90.0f;
			for (int i = 0; i < n; ++i) {
				RE::Projectile::ProjectileRot rot{ (i % 2 ? -0.25f : 0.15f), a_p->GetAngleZ() + 6.2831853f * i / n };
				RE::ProjectileHandle h;
				RE::Projectile::LaunchSpell(&h, a_p, spell, from, rot);
			}
		}

		void Burst(RE::PlayerCharacter* a_p)
		{
			const float radius = Dial("Protean_DevastatorRadius", 1050.0f);
			const float damage = Dial("Protean_DevastatorDamage", 100.0f) + Dial("Protean_DevastatorDamagePerLevel", 8.0f) * a_p->GetLevel();
			const float force = Dial("Protean_DevastatorForce", 10.0f);
			Tendrils(a_p);
			int  hit = 0;
			auto* pl = RE::ProcessLists::GetSingleton();
			if (pl) {
				const auto here = a_p->GetPosition();
				for (auto& h : pl->highActorHandles) {
					auto a = h.get();
					if (!a || a.get() == a_p || a->IsDead() || a->IsPlayerTeammate() || !a->IsHostileToActor(a_p) ||
						here.GetDistance(a->GetPosition()) > radius) {
						continue;
					}
					g_struck[a->GetFormID()] = Clock::now();
					Push(a_p, a.get(), force);
					a->AsActorValueOwner()->DamageActorValue(RE::ActorValue::kHealth, damage);
					logger::info("devastator: '{}' {:08X} at {:.0f}: knocked down, {:.0f} damage, health now {:.0f}", a->GetName(), a->GetFormID(),
						here.GetDistance(a->GetPosition()), damage, a->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth));
					++hit;
				}
			}
			logger::info("devastator: burst, {} enemy(ies) within {:.0f}", hit, radius);
		}

		void Finish(RE::PlayerCharacter* a_p)
		{
			Slow(false);
			CameraEnd();
			Lock(false);
			a_p->NotifyAnimationGraph("IdleForceDefaultState");
			g_phase = Phase::kNone;
			logger::info("devastator: over");
		}
	}

	void OnCast()
	{
		auto* p = RE::PlayerCharacter::GetSingleton();
		if (!p || g_phase != Phase::kNone || !Evolve::Has("devastator")) {
			return;
		}
		if (Shift::ActiveCreature()) {
			Shift::Notify("Devastator needs your own form.");
			return;
		}
		const auto need = static_cast<std::uint32_t>(Dial("Protean_DevastatorStacks", 2.0f));
		if (Evolve::BoostStacks() < need) {
			Shift::Notify(std::format("Devastator needs {} kill boosts.", need));
			return;
		}
		auto*       av = p->AsActorValueOwner();
		const float stamina = av->GetActorValue(RE::ActorValue::kStamina);
		if (stamina < av->GetPermanentActorValue(RE::ActorValue::kStamina) - 1.0f) {
			Shift::Notify("Devastator needs all your Stamina.");
			return;
		}
		av->DamageActorValue(RE::ActorValue::kStamina, stamina);
		Lock(true);
		CameraStart();
		const bool took = p->NotifyAnimationGraph(kRise);
		g_phase = Phase::kWindup;
		g_start = g_last = Clock::now();
		logger::info("devastator: wind-up (the rise {})", took ? "playing" : "refused by the graph");
	}

	void OnFrame(RE::PlayerCharacter* a_p)
	{
		if (g_phase == Phase::kNone) {
			return;
		}
		const float dt = Seconds(g_last);
		g_last = Clock::now();
		CameraTurn(dt);
		const float t = Seconds(g_start);
		if (a_p->IsDead()) {
			Finish(a_p);
			return;
		}
		if (g_phase == Phase::kWindup && t >= Dial("Protean_DevastatorWindup", 1.0f)) {
			Slow(true);
			Burst(a_p);
			g_phase = Phase::kAfter;
			g_start = Clock::now();
			return;
		}
		if (g_phase == Phase::kAfter) {
			if (t >= kSlowSeconds) {
				Slow(false);
			}
			if (t >= kAfterSeconds) {
				Finish(a_p);
			}
		}
	}

	bool Struck(RE::FormID a_id)
	{
		const auto it = g_struck.find(a_id);
		return it != g_struck.end() && Seconds(it->second) < kStruckFor;
	}

	void Reset()
	{
		if (g_phase != Phase::kNone) {
			Slow(false);
			Lock(false);
			g_zoom = -1.0f;
			g_phase = Phase::kNone;
		}
		g_struck.clear();
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
