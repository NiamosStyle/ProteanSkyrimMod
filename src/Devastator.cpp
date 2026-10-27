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
		constexpr const char* kBurstSounds[] = { "QSTMiraakDeathTentacleStab", "VOCShoutFXUnrelentingForceThunderA" };
		constexpr float            kSlowSeconds = 0.5f;   // real seconds of slow motion at the burst
		constexpr float            kAfterSeconds = 2.0f;  // real seconds the camera keeps circling after the burst
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
		std::map<RE::FormID, Clock::time_point> g_struck;

		using Flag = RE::ControlMap::UEFlag;
		constexpr auto kLocked = static_cast<Flag>(std::to_underlying(Flag::kMovement) | std::to_underlying(Flag::kLooking) |
		                                           std::to_underlying(Flag::kJumping) | std::to_underlying(Flag::kFighting) |
		                                           std::to_underlying(Flag::kActivate) | std::to_underlying(Flag::kSneaking));

		float Dial(std::string_view a_name, float a_fallback) { return Registry::Get().Dial(a_name, a_fallback); }

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
				t->SetGlobalTimeMultiplier(a_on ? Dial("Protean_DevastatorSlowMo", 0.33f) : 1.0f, true);
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

		void CameraEnd()
		{
			if (auto* cam = RE::PlayerCamera::GetSingleton()) {
				cam->ForceThirdPerson();
			}
		}

		// The tendrils thrown for the look: cast the ordinary way (Protean's own launch showed nothing, 2026-10-27), each
		// at an unseen marker placed on a ring around the player, slightly up and down in turn
		std::vector<RE::ObjectRefHandle> g_marks;

		void PlaceMarks(RE::PlayerCharacter* a_p)
		{
			auto* base = RE::TESForm::LookupByID<RE::TESBoundObject>(0x3B);  // XMarker, Skyrim.esm
			const int n = std::max(static_cast<int>(Dial("Protean_DevastatorTendrils", 8.0f)), 1);
			const auto at = a_p->GetPosition();
			for (int i = 0; base && i < n; ++i) {
				const float a = a_p->GetAngleZ() + 6.2831853f * i / n;
				if (auto m = a_p->PlaceObjectAtMe(base, false)) {
					m->SetPosition(RE::NiPoint3(at.x + 700.0f * std::sin(a), at.y + 700.0f * std::cos(a), at.z + (i % 2 ? 40.0f : 220.0f)));
					g_marks.push_back(m->GetHandle());
				}
			}
		}

		void ClearMarks()
		{
			for (auto& h : g_marks) {
				if (auto m = h.get()) {
					m->Disable();
					m->SetDelete(true);
				}
			}
			g_marks.clear();
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

		void Tendrils(RE::PlayerCharacter* a_p)
		{
			auto* spell = Registry::Get().DevastatorTendril();
			auto* caster = a_p->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
			if (!spell || !caster) {
				return;
			}
			int cast = 0;
			for (auto& h : g_marks) {
				if (auto m = h.get()) {
					caster->CastSpellImmediate(spell, false, m.get(), 1.0f, false, 0.0f, a_p);
					++cast;
				}
			}
			logger::info("devastator: {} tendril(s) cast", cast);
		}

		void Burst(RE::PlayerCharacter* a_p)
		{
			const float radius = Dial("Protean_DevastatorRadius", 1050.0f);
			const float damage = Dial("Protean_DevastatorDamage", 100.0f) + Dial("Protean_DevastatorDamagePerLevel", 8.0f) * a_p->GetLevel();
			const float force = Dial("Protean_DevastatorForce", 10.0f);
			Tendrils(a_p);
			for (const auto* sound : kBurstSounds) {
				Play(a_p, sound);
			}
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
			ClearMarks();
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
		const bool took = p->NotifyAnimationGraph(kRise);
		Play(p, kWindupSound);
		CameraStart();
		PlaceMarks(p);
		g_phase = Phase::kWindup;
		g_start = g_last = Clock::now();
		logger::info("devastator: wind-up (the rise {})", took ? "playing" : "refused by the graph");
	}

	void OnFrame(RE::PlayerCharacter* a_p)
	{
		if (g_phase == Phase::kNone) {
			return;
		}
		g_last = Clock::now();
		CameraKeep();
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
			g_marks.clear();
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
