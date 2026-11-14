#include "Predator.h"

#include "Evolve.h"
#include "Registry.h"
#include "Shift.h"

namespace protean::Predator
{
	namespace
	{
		float Dial(std::string_view a_name, float a_fallback) { return Registry::Get().Dial(a_name, a_fallback); }

		void Poison(RE::ObjectRefHandle a_target)
		{
			auto* p = RE::PlayerCharacter::GetSingleton();
			auto  ref = a_target.get();
			auto* target = ref ? ref->As<RE::Actor>() : nullptr;
			auto* spell = Registry::Get().PredatorPoison();
			if (!p || !target || !spell || spell->effects.empty() || target->IsDead()) {
				return;
			}
			if (target->HasKeywordString("ActorTypeUndead") || target->HasKeywordString("ActorTypeDwarven")) {
				logger::info("predator: '{}' is immune to poison", target->GetName());
				return;
			}
			auto& e = spell->effects[0]->effectItem;
			e.magnitude = std::round(Dial("Protean_PredatorPoison", 5.0f) + Dial("Protean_PredatorPoisonPerLevel", 0.3f) * p->GetLevel());
			e.duration = static_cast<std::uint32_t>(std::max(Dial("Protean_PredatorPoisonSeconds", 5.0f), 1.0f));
			if (auto* caster = p->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)) {
				caster->CastSpellImmediate(spell, false, target, 1.0f, false, 0.0f, p);
				logger::info("predator: poisoned '{}', {:.0f} a second for {} s", target->GetName(), e.magnitude, e.duration);
			}
		}

		class Sink : public RE::BSTEventSink<RE::TESHitEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event, RE::BSTEventSource<RE::TESHitEvent>*) override
			{
				if (a_event && a_event->cause && a_event->cause->IsPlayerRef() && a_event->target &&
					a_event->flags.any(RE::TESHitEvent::Flag::kPowerAttack) && !a_event->flags.any(RE::TESHitEvent::Flag::kHitBlocked)) {
					const auto target = a_event->target->GetHandle();
					SKSE::GetTaskInterface()->AddTask([target] {
						if (Shift::ActiveCreature() && Evolve::Has("predator_2")) {
							Poison(target);
						}
					});
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	float DamageShare(RE::PlayerCharacter* a_p, const RE::TESRace* a_race)
	{
		if (!Evolve::Has("predator")) {
			return 0.0f;
		}
		float share = std::min(Dial("Protean_PredatorDamagePerLevel", 0.02f) * a_p->GetLevel(), Dial("Protean_PredatorDamageMax", 1.0f));
		if (a_race && a_race->data.unarmedDamage > 0.0f && Evolve::Has("apex")) {
			const float floor = Dial("Protean_ApexDamageBase", 17.0f) + Dial("Protean_ApexDamagePerLevel", 2.6f) * a_p->GetLevel();
			const float needed = floor / a_race->data.unarmedDamage - 1.0f;
			if (needed > share) {
				logger::info("apex: base hit {:.1f} lifted to {:.1f} (x{:.2f})", a_race->data.unarmedDamage, floor, needed + 1.0f);
				share = needed;
			}
		}
		return share;
	}

	void Install()
	{
		static Sink sink;
		RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESHitEvent>(&sink);
	}
}
