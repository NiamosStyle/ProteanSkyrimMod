#include "Tendril.h"

#include "Evolve.h"
#include "Registry.h"
#include "Shift.h"

namespace protean::Tendril
{
	namespace
	{
		using Clock = std::chrono::steady_clock;
		constexpr auto kPerk = "tendril";

		constexpr float kKnock = 15.0f;          // the yank: Pull Mastery's force (a negative knock pulls)
		constexpr float kHaulSpeed = 2200.0f;    // the player hauled to a large target, game units / s
		constexpr float kHaulGravity = 1000.0f;  // the haul's own gravity (as a leap's): a short, low arc
		constexpr float kHaulGap = 60.0f;        // stop this far short of the target's edge

		float g_cooldown{ 0.0f };     // seconds until the next throw
		Clock::time_point g_armed{};  // a paid throw is in flight until then: its hit may pull
		Clock::time_point g_toldAt{};
		Clock::time_point g_thrownAt{};  // the last paid throw: the gate leaves its own cast alone a moment

		// the player hauled toward a large target (set every frame after the player's update, as a leap is)
		struct Haul
		{
			bool                on{ false };
			Clock::time_point   since{};
			RE::ObjectRefHandle target{};
			RE::NiPoint3        dir{};       // horizontal, unit
			float               speed{ 0.0f };
			float               vz0{ 0.0f };
			float               time{ 0.0f };  // seconds the arc takes
			float               stop{ 0.0f };  // horizontal distance to stop at
			float               gravity{ 1.0f };
			RE::NiPoint3        lastPos{};
		};
		Haul g_haul;

		float Dial(std::string_view a_name, float a_fallback) { return Registry::Get().Dial(a_name, a_fallback); }

		bool Ready(RE::PlayerCharacter* a_p)
		{
			return g_cooldown <= 0.0f && a_p->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina) >= Dial("Protean_TendrilCost", 25.0f);
		}

		// damage: a share of what Rending Claws deal on a full bar; drain: a base plus a little per level
		void SetMagnitudes(RE::PlayerCharacter* a_p, RE::SpellItem* a_spell)
		{
			const float level = static_cast<float>(a_p->GetLevel());
			const float claws = Dial("Protean_ClawDamageMax", 10.0f) + Dial("Protean_ClawDamagePerLevel", 0.7f) * level +
			                    Dial("Protean_ClawDamagePerLevelSq", 0.02f) * level * level;
			a_spell->effects[0]->effectItem.magnitude = std::round(claws * Dial("Protean_TendrilDamageShare", 0.5f));
			a_spell->effects[1]->effectItem.magnitude = std::round(Dial("Protean_TendrilDrain", 20.0f) + Dial("Protean_TendrilDrainPerLevel", 0.5f) * level);
		}

		// a throw begun while it isn't ready is broken off before it leaves the hand
		void Gate(RE::PlayerCharacter* a_p, RE::SpellItem* a_spell)
		{
			if (Ready(a_p) || Clock::now() - g_thrownAt < std::chrono::milliseconds(500)) {
				return;
			}
			using State = RE::MagicCaster::State;
			for (const auto source : { RE::MagicSystem::CastingSource::kLeftHand, RE::MagicSystem::CastingSource::kRightHand }) {
				auto* caster = a_p->GetMagicCaster(source);
				if (!caster || caster->currentSpell != a_spell) {
					continue;
				}
				const auto state = caster->state.get();
				if (state == State::kNone || state == State::kUnk07 || state == State::kUnk08 || state == State::kUnk09) {
					continue;
				}
				caster->InterruptCast(false);
				if (Clock::now() - g_toldAt > std::chrono::seconds(1)) {
					g_toldAt = Clock::now();
					Shift::Notify(g_cooldown > 0.0f ? "Your tendril is not ready yet." : "You are too tired to lash out.");
				}
				logger::info("tendril: throw broken off ({}, cooldown {:.1f} s)", g_cooldown > 0.0f ? "cooling down" : "stamina", g_cooldown);
			}
		}

		void EndHaul(RE::PlayerCharacter* a_p, const char* a_why)
		{
			if (auto* cc = a_p->GetCharController()) {
				cc->gravity = g_haul.gravity;
				cc->fallStartHeight = a_p->GetPositionZ();  // a drop after the haul hurts from here, not from the arc
				const float s = RE::bhkWorld::GetWorldScale();
				cc->SetLinearVelocityImpl(RE::hkVector4(0.0f, 0.0f, std::min(0.0f, g_haul.vz0 - kHaulGravity * g_haul.time) * s, 0.0f));
			}
			logger::info("tendril: haul {} after {:.2f} s", a_why, std::chrono::duration<float>(Clock::now() - g_haul.since).count());
			g_haul = {};
		}

		void UpdateHaul(RE::PlayerCharacter* a_p, float a_delta)
		{
			if (!g_haul.on) {
				return;
			}
			auto* cc = a_p->GetCharController();
			auto  target = g_haul.target.get();
			auto* actor = target ? target->As<RE::Actor>() : nullptr;
			if (!cc || a_p->IsDead()) {
				g_haul = {};
				return;
			}
			if (!actor || actor->IsDisabled() || !actor->Is3DLoaded()) {
				EndHaul(a_p, "lost its target");
				return;
			}
			const float t = std::chrono::duration<float>(Clock::now() - g_haul.since).count();
			const auto  pos = a_p->GetPosition();
			const auto  to = actor->GetPosition() - pos;
			if (std::sqrt(to.x * to.x + to.y * to.y) <= g_haul.stop) {
				EndHaul(a_p, "arrived");
				return;
			}
			if (t > g_haul.time + 0.3f) {
				EndHaul(a_p, "ran out of time");
				return;
			}
			// a wall or a slope in the way: stop where we are
			if (t > 0.15f && a_delta > 0.0001f) {
				const auto  step = (pos - g_haul.lastPos) / a_delta;
				const float ahead = step.x * g_haul.dir.x + step.y * g_haul.dir.y;
				if (ahead < 0.3f * g_haul.speed) {
					EndHaul(a_p, "blocked");
					return;
				}
			}
			g_haul.lastPos = pos;
			cc->gravity = 0.0f;
			cc->context.currentState = RE::hkpCharacterStateType::kInAir;
			cc->fallStartHeight = pos.z;  // no fall damage from the arc itself
			const float vz = g_haul.vz0 - kHaulGravity * t;
			const float s = RE::bhkWorld::GetWorldScale();
			cc->SetLinearVelocityImpl(RE::hkVector4(g_haul.dir.x * g_haul.speed * s, g_haul.dir.y * g_haul.speed * s, vz * s, 0.0f));
		}

		// a large target: the player is hauled to it along a low arc that ends at its edge
		void StartHaul(RE::PlayerCharacter* a_p, RE::Actor* a_target)
		{
			auto* cc = a_p->GetCharController();
			if (!cc) {
				return;
			}
			const auto  from = a_p->GetPosition();
			const auto  to = a_target->GetPosition();
			const auto  ext = (a_target->GetBoundMax() - a_target->GetBoundMin()) * a_target->GetScale();
			const float edge = 0.5f * std::max(ext.x, ext.y);
			auto        flat = RE::NiPoint3(to.x - from.x, to.y - from.y, 0.0f);
			const float dist = flat.Length();
			const float run = dist - edge - kHaulGap;
			if (dist < 1.0f || run < 50.0f) {
				logger::info("tendril: '{}' is already within reach", a_target->GetName());
				return;
			}
			flat /= dist;
			const float speed = kHaulSpeed * Dial("Protean_TendrilPullMult", 1.0f);
			const float time = run / speed;
			const float dz = std::min(to.z - from.z, 300.0f);  // up a ledge, not onto a dragon's back
			g_haul = { true, Clock::now(), a_target->CreateRefHandle(), flat, speed, dz / time + 0.5f * kHaulGravity * time, time,
				edge + kHaulGap, cc->gravity, from };
			logger::info("tendril: hauling the player {:.0f} units to '{}' ({:.2f} s, {:.0f} up)", run, a_target->GetName(), time, dz);
		}

		// a light target: knocked off its feet toward the player
		void Yank(RE::PlayerCharacter* a_p, RE::Actor* a_target)
		{
			auto* proc = a_target->GetActorRuntimeData().currentProcess;
			if (!proc) {
				return;
			}
			const float force = kKnock * Dial("Protean_TendrilPullMult", 1.0f);
			proc->KnockExplosion(a_target, a_p->GetPosition(), -force);
			logger::info("tendril: yanked '{}' (force {:.0f}) from {:.0f} units", a_target->GetName(), force, a_p->GetPosition().GetDistance(a_target->GetPosition()));
		}
	}

	void OnFrame(RE::PlayerCharacter* a_p, float a_delta)
	{
		auto* spell = Registry::Get().Tendril();
		if (!spell || !a_p) {
			return;
		}
		if (g_cooldown > 0.0f) {
			g_cooldown -= a_delta;
		}
		UpdateHaul(a_p, a_delta);
		Gate(a_p, spell);
		static float since = 0.0f;
		if ((since += a_delta) < 0.25f) {
			return;
		}
		since = 0.0f;
		SetMagnitudes(a_p, spell);
		static float sinceGive = 0.0f;
		if ((sinceGive += 0.25f) >= 2.0f) {
			sinceGive = 0.0f;
			if (Evolve::Has(kPerk)) {
				Give();
			}
		}
	}

	void OnCast(RE::SpellItem* a_spell)
	{
		auto* p = RE::PlayerCharacter::GetSingleton();
		if (!p || !a_spell || a_spell != Registry::Get().Tendril()) {
			return;
		}
		if (!Ready(p)) {
			logger::info("tendril: a throw slipped past the gate (cooldown {:.1f} s): no pull", g_cooldown);
			return;
		}
		const float cost = Dial("Protean_TendrilCost", 25.0f);
		p->AsActorValueOwner()->DamageActorValue(RE::ActorValue::kStamina, cost);
		g_cooldown = Dial("Protean_TendrilCooldown", 3.0f);
		g_thrownAt = Clock::now();
		g_armed = g_thrownAt + std::chrono::milliseconds(1500);
		logger::info("tendril: thrown, stamina -{:.0f}, damage {:.0f}, drain {:.0f}", cost, a_spell->effects[0]->effectItem.magnitude,
			a_spell->effects[1]->effectItem.magnitude);
	}

	void OnHit(RE::Actor* a_target)
	{
		auto* p = RE::PlayerCharacter::GetSingleton();
		if (!p || !a_target || a_target == p || Clock::now() > g_armed) {
			return;
		}
		g_armed = {};  // one pull per throw
		const char* skip = a_target->IsDead()             ? "dead" :
		                   a_target->IsPlayerTeammate()   ? "a follower" :
		                   a_target->IsEssential()        ? "essential" :
		                   a_target->AsActorState()->IsFlying() ? "flying" :
		                   a_target->AsActorState()->IsSwimming() || p->AsActorState()->IsSwimming() ? "in the water" :
		                   g_haul.on ? "already hauling" : nullptr;
		if (skip) {
			logger::info("tendril: hit '{}', no pull ({})", a_target->GetName(), skip);
			return;
		}
		const auto* race = a_target->GetRace();
		const bool  large = race && race->data.raceSize.get() >= RE::RACE_SIZE::kLarge;
		if (large) {
			StartHaul(p, a_target);
		} else {
			Yank(p, a_target);
		}
	}

	void Give()
	{
		auto* p = RE::PlayerCharacter::GetSingleton();
		if (auto* spell = Registry::Get().Tendril(); p && spell && !p->HasSpell(spell)) {
			p->AddSpell(spell);
			logger::info("tendril: spell taught");
		}
	}
}
