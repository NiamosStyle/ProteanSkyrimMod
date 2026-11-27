// Protean - a shapeshifter mod for Skyrim SE
// Copyright (C) 2026 NiamosStyle
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. It is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file or <https://www.gnu.org/licenses/>.

#include "Provoke.h"

#include "Call.h"
#include "Shift.h"
#include "State.h"

namespace protean::Provoke
{
	namespace
	{
		constexpr float kFrenzied = 3.0f;  // Aggression: attacks anyone, friends too
		constexpr float kGrace = 3.0f;     // seconds a provoked animal has to get into its fight before "out of combat" counts

		std::map<RE::FormID, float> g_age;  // seconds since provoked (not saved: after a load every entry counts as old)

		bool SharesFaction(RE::Actor* a_target, RE::PlayerCharacter* a_p)
		{
			bool shared = false;
			a_target->VisitFactions([&](RE::TESFaction* a_f, std::int8_t a_rank) {
				if (a_f && a_rank >= 0 && a_p->IsInFaction(a_f)) {
					shared = true;
					return true;
				}
				return false;
			});
			return shared;
		}

		void Provoke(RE::Actor* a_target)
		{
			auto* p = RE::PlayerCharacter::GetSingleton();
			auto& s = State::Get();
			if (!p || !Shift::ActiveCreature() || !a_target || a_target->IsDead() || a_target->IsPlayerTeammate() ||
				a_target->IsHostileToActor(p) || !SharesFaction(a_target, p)) {
				return;
			}
			const auto id = a_target->GetFormID();
			if (std::ranges::any_of(s.provoked, [&](const auto& e) { return e.first == id; })) {
				return;
			}
			auto*       av = a_target->AsActorValueOwner();
			const float was = av->GetBaseActorValue(RE::ActorValue::kAggression);
			s.provoked.emplace_back(id, was);
			g_age[id] = 0.0f;
			av->SetBaseActorValue(RE::ActorValue::kAggression, kFrenzied);
			a_target->StartCombat(p);
			logger::info("provoke: '{}' {:08X} bitten by a friend: frenzied (aggression was {:.0f}) and set on the player", a_target->GetName(), id, was);
		}

		void Calm(RE::FormID a_id, float a_was, std::string_view a_why)
		{
			if (auto* a = RE::TESForm::LookupByID<RE::Actor>(a_id)) {
				a->AsActorValueOwner()->SetBaseActorValue(RE::ActorValue::kAggression, a_was);
				logger::info("provoke: '{}' {:08X} calm again ({})", a->GetName(), a_id, a_why);
			}
			g_age.erase(a_id);
		}

		class Sink : public RE::BSTEventSink<RE::TESHitEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event, RE::BSTEventSource<RE::TESHitEvent>*) override
			{
				if (a_event && a_event->cause && a_event->cause->IsPlayerRef() && a_event->target && !a_event->target->IsPlayerRef()) {
					const auto target = a_event->target->GetHandle();
					SKSE::GetTaskInterface()->AddTask([target] {
						if (auto t = target.get()) {
							if (auto* a = t->As<RE::Actor>(); a && !Call::OnPlayerHit(a)) {
								Provoke(a);
							}
						}
					});
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Install()
	{
		static Sink sink;
		RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESHitEvent>(&sink);
	}

	void OnFrame(float a_delta)
	{
		auto& s = State::Get();
		if (s.provoked.empty()) {
			return;
		}
		for (auto& [id, age] : g_age) {
			age += a_delta;
		}
		static float tick = 0.0f;
		if ((tick -= a_delta) > 0.0f) {
			return;
		}
		tick = 1.0f;
		const bool animal = Shift::ActiveCreature() != nullptr;
		std::erase_if(s.provoked, [&](const auto& e) {
			const auto [id, was] = e;
			auto*      a = RE::TESForm::LookupByID<RE::Actor>(id);
			const auto it = g_age.find(id);
			const bool young = it != g_age.end() && it->second < kGrace;
			const char* why = !a              ? "gone" :
			                  !animal         ? "the player changed back" :
			                  a->IsDead()     ? "dead" :
			                  !young && !a->IsInCombat() ? "out of combat" :
			                                    nullptr;
			if (!why) {
				return false;
			}
			Calm(id, was, why);
			return true;
		});
	}
}
