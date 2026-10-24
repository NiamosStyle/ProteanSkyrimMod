#include "Feed.h"

#include "Evolve.h"
#include "Registry.h"
#include "Shift.h"
#include "State.h"

namespace protean::Feed
{
	namespace
	{
		constexpr std::string_view kPerk = "survivor_2";
		constexpr float            kFront = 0.5f;   // cos of the widest angle off the facing a meal is looked for (60 deg)
		constexpr float            kStray = 40.0f;  // game units the player may slide while eating

		struct Meal
		{
			bool               on{ false };
			bool               corpse{ false };
			RE::ObjectRefHandle target;
			float              t{ 0.0f };
			RE::NiPoint3       at;
		};
		Meal g_meal;

		float Dial(std::string_view a_name, float a_fallback) { return Registry::Get().Dial(a_name, a_fallback); }

		bool IsPerson(RE::Actor* a_body)
		{
			auto* race = a_body->GetRace();
			return race && race->data.flags.all(RE::RACE_DATA::Flag::kFaceGenHead) && !Registry::Get().ByRace(race);
		}

		bool Edible(RE::Actor* a_body)
		{
			auto* race = a_body->GetRace();
			if (!a_body->IsDead() || a_body->IsPlayerRef() || !race || race->IsChildRace() ||
				State::Get().eaten.contains(a_body->GetFormID())) {
				return false;
			}
			return !a_body->HasKeywordString("ActorTypeUndead") && !a_body->HasKeywordString("ActorTypeDwarven") &&
			       !a_body->HasKeywordString("ActorTypeGhost");
		}

		RE::TESProduceForm* Produce(RE::TESObjectREFR* a_ref)
		{
			auto* base = a_ref->GetBaseObject();
			if (!base) {
				return nullptr;
			}
			if (auto* flora = base->As<RE::TESFlora>()) {
				return flora;
			}
			if (auto* tree = base->As<RE::TESObjectTREE>()) {
				return tree;
			}
			return nullptr;
		}

		bool Plant(RE::TESObjectREFR* a_ref)
		{
			auto* pr = Produce(a_ref);
			return pr && pr->produceItem && !(a_ref->formFlags & RE::TESObjectREFR::RecordFlags::kHarvested) && !a_ref->IsDisabled();
		}

		// the nearest body or plant in reach and in front of the animal
		RE::TESObjectREFR* Find(RE::PlayerCharacter* a_p, const Creature& a_c, bool& a_corpse)
		{
			const bool meat = a_c.diet == "meat" || a_c.diet == "both";
			const bool plants = a_c.diet == "plants" || a_c.diet == "both";
			const auto here = a_p->GetPosition();
			const float yaw = a_p->GetAngleZ();
			const RE::NiPoint3 facing{ std::sin(yaw), std::cos(yaw), 0.0f };
			RE::TESObjectREFR* best = nullptr;
			float              bestDist = Dial("Protean_FeedReach", 250.0f) + a_c.reach * 0.5f;
			RE::TES::GetSingleton()->ForEachReferenceInRange(a_p, bestDist, [&](RE::TESObjectREFR* a_ref) {
				if (!a_ref || a_ref == a_p) {
					return RE::BSContainer::ForEachResult::kContinue;
				}
				auto*      actor = a_ref->As<RE::Actor>();
				const bool body = actor && meat && Edible(actor);
				if (!body && !(!actor && plants && Plant(a_ref))) {
					return RE::BSContainer::ForEachResult::kContinue;
				}
				auto to = a_ref->GetPosition() - here;
				to.z = 0.0f;
				const float dist = to.Length();
				if (dist < bestDist && (dist < 60.0f || (to.x * facing.x + to.y * facing.y) / dist >= kFront)) {
					best = a_ref;
					bestDist = dist;
					a_corpse = body;
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});
			return best;
		}

		void Stop(RE::PlayerCharacter* a_p, std::string_view a_why)
		{
			if (!g_meal.on) {
				return;
			}
			g_meal = {};
			if (a_p) {
				a_p->NotifyAnimationGraph("IdleForceDefaultState");
			}
			logger::info("feed: stopped ({})", a_why);
		}

		// harvested as the game does it (its look changes and it grows back on the game's timer); the ingredient is
		// taken back, the animal ate it
		void Harvest(RE::PlayerCharacter* a_p, RE::TESObjectREFR* a_plant)
		{
			auto*       item = Produce(a_plant)->produceItem;
			const auto  count = [&] {
				const auto inv = a_p->GetInventoryCounts([item](RE::TESBoundObject& a_o) { return &a_o == item; });
				return inv.empty() ? 0 : inv.begin()->second;
			};
			const auto before = count();
			a_plant->ActivateRef(a_p, 0, nullptr, 1, false);
			if (const auto got = count() - before; got > 0) {
				a_p->RemoveItem(item, got, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
			}
		}

		void Finish(RE::PlayerCharacter* a_p)
		{
			auto  ref = g_meal.target.get();
			const bool corpse = g_meal.corpse;
			g_meal = {};
			a_p->NotifyAnimationGraph("IdleForceDefaultState");
			if (!ref) {
				return;
			}
			auto*       av = a_p->AsActorValueOwner();
			const float max = av->GetPermanentActorValue(RE::ActorValue::kHealth) +
			                  a_p->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth);
			const float heal = std::round(max * (corpse ? Dial("Protean_FeedCorpseHeal", 0.25f) : Dial("Protean_FeedPlantHeal", 0.2f)));
			av->RestoreActorValue(RE::ActorValue::kHealth, heal);
			if (corpse) {
				State::Get().eaten.insert(ref->GetFormID());
			} else {
				Harvest(a_p, ref.get());
			}
			logger::info("feed: ate '{}', healed {:.0f} of {:.0f}", ref->GetName(), heal, max);
		}
	}

	bool OnActivate()
	{
		auto*       p = RE::PlayerCharacter::GetSingleton();
		const auto* c = Shift::ActiveCreature();
		if (!p || !c || c->diet.empty() || !Evolve::Has(kPerk)) {
			logger::info("feed: Activate, no meal: form '{}' diet '{}', perk {}", c ? c->id : "", c ? c->diet : "", Evolve::Has(kPerk));
			return false;
		}
		if (g_meal.on) {
			return true;  // already eating
		}
		if (p->IsInMidair()) {
			return false;
		}
		bool corpse = false;
		auto* target = Find(p, *c, corpse);
		if (!target) {
			logger::info("feed: Activate, nothing to eat in reach");
			return false;
		}
		// face the meal
		const auto to = target->GetPosition() - p->GetPosition();
		p->SetAngle(RE::NiPoint3{ p->GetAngleX(), p->GetAngleY(), std::atan2(to.x, to.y) });
		const bool played = !c->feedEvent.empty() && p->NotifyAnimationGraph(c->feedEvent);
		g_meal = { true, corpse, target->GetHandle(), 0.0f, p->GetPosition() };
		logger::info("feed: '{}' starts on '{}' ({}), graph took '{}': {}", c->id, target->GetName(), corpse ? "body" : "plant",
			c->feedEvent, played);
		if (corpse) {
			if (auto* body = target->As<RE::Actor>(); body && IsPerson(body)) {
				Shift::SeenFeeding(body);
			}
		}
		return true;
	}

	void OnFrame(RE::PlayerCharacter* a_p, float a_delta)
	{
		if (!g_meal.on) {
			return;
		}
		auto* pc = RE::PlayerControls::GetSingleton();
		if (!Shift::ActiveCreature() || a_p->IsDead()) {
			Stop(a_p, "no longer an animal");
		} else if ((pc && pc->data.moveInputVec.Length() > 0.1f) || a_p->GetPosition().GetDistance(g_meal.at) > kStray ||
				   a_p->IsInMidair()) {
			Stop(a_p, "moved");
		} else if (!g_meal.target.get()) {
			Stop(a_p, "the meal is gone");
		} else if ((g_meal.t += a_delta) >= Dial("Protean_FeedSeconds", 3.0f)) {
			Finish(a_p);
		}
	}

	void Cancel() { Stop(RE::PlayerCharacter::GetSingleton(), "cancelled"); }
}
