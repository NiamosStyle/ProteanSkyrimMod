// Protean - a shapeshifter mod for Skyrim SE
// Copyright (C) 2026 NiamosStyle
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. It is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file or <https://www.gnu.org/licenses/>.

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
		// the meal's sounds, played at these seconds into it (user's pick, chosen live 2026-10-24: the animals' own
		// sounds were too quiet to hear)
		constexpr const char*      kPlantSound = "ITMFoodEat";
		constexpr float            kSoundAt[] = { 0.3f, 1.8f };
		constexpr float            kRetryAnim = 0.5f;      // seconds the eating animation is asked for again

		struct Meal
		{
			bool               on{ false };
			RE::ObjectRefHandle target;
			float              t{ 0.0f };
			RE::NiPoint3       at;
			bool               played{ false };  // the graph took the eating animation
			int                sounds{ 0 };      // sounds played so far
		};
		Meal g_meal;

		float Dial(std::string_view a_name, float a_fallback) { return Registry::Get().Dial(a_name, a_fallback); }

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

		// the nearest plant in reach and in front of the animal
		RE::TESObjectREFR* Find(RE::PlayerCharacter* a_p, const Creature& a_c)
		{
			const auto here = a_p->GetPosition();
			const float yaw = a_p->GetAngleZ();
			const RE::NiPoint3 facing{ std::sin(yaw), std::cos(yaw), 0.0f };
			RE::TESObjectREFR* best = nullptr;
			float              bestDist = Dial("Protean_FeedReach", 250.0f) + a_c.reach * 0.5f;
			RE::TES::GetSingleton()->ForEachReferenceInRange(a_p, bestDist, [&](RE::TESObjectREFR* a_ref) {
				if (!a_ref || a_ref == a_p || a_ref->As<RE::Actor>() || !Plant(a_ref)) {
					return RE::BSContainer::ForEachResult::kContinue;
				}
				auto to = a_ref->GetPosition() - here;
				to.z = 0.0f;
				const float dist = to.Length();
				if (dist < bestDist && (dist < 60.0f || (to.x * facing.x + to.y * facing.y) / dist >= kFront)) {
					best = a_ref;
					bestDist = dist;
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});
			return best;
		}

		void Sound(RE::PlayerCharacter* a_p, const char* a_name)
		{
			RE::BSSoundHandle h;
			if (auto* audio = RE::BSAudioManager::GetSingleton()) {
				audio->GetSoundHandleByName(h, a_name, 0x1A);
			}
			if (h.IsValid()) {
				h.SetPosition(a_p->GetPosition());
				h.Play();
			}
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
			auto ref = g_meal.target.get();
			g_meal = {};
			a_p->NotifyAnimationGraph("IdleForceDefaultState");
			if (!ref) {
				return;
			}
			auto*       av = a_p->AsActorValueOwner();
			const float max = av->GetPermanentActorValue(RE::ActorValue::kHealth) +
			                  a_p->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth);
			const float heal = std::round(max * Dial("Protean_FeedPlantHeal", 0.2f));
			av->RestoreActorValue(RE::ActorValue::kHealth, heal);
			Harvest(a_p, ref.get());
			logger::info("feed: ate '{}', healed {:.0f} of {:.0f}", ref->GetName(), heal, max);
		}
	}

	bool OnActivate()
	{
		auto*       p = RE::PlayerCharacter::GetSingleton();
		const auto* c = Shift::ActiveCreature();
		if (!p || !c || (c->diet != "plants" && c->diet != "both") || !Evolve::Has(kPerk)) {
			logger::info("feed: Activate, no meal: form '{}' diet '{}', perk {}", c ? c->id : "", c ? c->diet : "", Evolve::Has(kPerk));
			return false;
		}
		if (g_meal.on) {
			return true;  // already eating
		}
		if (p->IsInMidair()) {
			return false;
		}
		auto* target = Find(p, *c);
		if (!target) {
			logger::info("feed: Activate, nothing to eat in reach");
			return false;
		}
		// face the meal
		const auto to = target->GetPosition() - p->GetPosition();
		p->SetAngle(RE::NiPoint3{ p->GetAngleX(), p->GetAngleY(), std::atan2(to.x, to.y) });
		const bool played = !c->feedEvent.empty() && p->NotifyAnimationGraph(c->feedEvent);
		g_meal = { true, target->GetHandle(), 0.0f, p->GetPosition(), played, 0 };
		logger::info("feed: '{}' starts on '{}', graph took '{}': {}", c->id, target->GetName(), c->feedEvent, played);
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
		} else {
			// the graph refuses the eating animation straight after a walk (seen 2026-10-24): asked again for a moment
			if (const auto* c = Shift::ActiveCreature(); !g_meal.played && g_meal.t < kRetryAnim && c && !c->feedEvent.empty() &&
				a_p->NotifyAnimationGraph(c->feedEvent)) {
				g_meal.played = true;
				logger::info("feed: the graph took '{}' after {:.2f} s", c->feedEvent, g_meal.t);
			}
			if (g_meal.sounds < static_cast<int>(std::size(kSoundAt)) && g_meal.t >= kSoundAt[g_meal.sounds]) {
				Sound(a_p, kPlantSound);
				++g_meal.sounds;
			}
		}
	}

	void Cancel() { Stop(RE::PlayerCharacter::GetSingleton(), "cancelled"); }

	bool Eating() { return g_meal.on; }
}
