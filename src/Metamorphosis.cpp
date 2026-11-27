// Protean - a shapeshifter mod for Skyrim SE
// Copyright (C) 2026 NiamosStyle
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. It is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file or <https://www.gnu.org/licenses/>.

#include "Metamorphosis.h"
#include "Papyrus.h"

#include "Evolve.h"
#include "Face.h"
#include "Registry.h"
#include "Shift.h"

namespace protean::Metamorphosis
{
	namespace
	{
		using Skills = RE::PlayerCharacter::PlayerSkills::Data;
		constexpr auto kSkills = static_cast<std::size_t>(Skills::Skill::kTotal);
		constexpr float kRestoreAfter = 0.5f;  // seconds after the menu closes: the game finishes the new race first

		// the skill actor values, in PlayerSkills order (OneHanded .. Enchanting are 6 .. 23)
		RE::ActorValue SkillValue(std::size_t a_i) { return static_cast<RE::ActorValue>(static_cast<std::uint32_t>(RE::ActorValue::kOneHanded) + a_i); }

		constexpr RE::ActorValue kAttributes[] = { RE::ActorValue::kHealth, RE::ActorValue::kMagicka, RE::ActorValue::kStamina };

		struct Snapshot
		{
			bool                         taken{ false };
			std::array<float, kSkills>   base{};       // the skill levels as the actor values have them
			Skills                       skills{};     // levels, progress, legendary counts, the level's progress
			std::array<float, 3>         attributes{};
			std::vector<RE::FormID>      perks;
			std::uint8_t                 perkPoints{ 0 };
			std::uint16_t                level{ 0 };
			RE::FormID                   race{ 0 };
		};
		Snapshot g_snap;
		bool     g_menuOpened{ false };
		float    g_restoreIn{ -1.0f };  // > 0: seconds until the put-back

		Skills* SkillData(RE::PlayerCharacter* a_p)
		{
			auto* s = a_p->GetInfoRuntimeData().skills;
			return s ? s->data : nullptr;
		}

		void Take(RE::PlayerCharacter* a_p)
		{
			auto& snap = g_snap;
			snap = {};
			auto* av = a_p->AsActorValueOwner();
			for (std::size_t i = 0; i < kSkills; ++i) {
				snap.base[i] = av->GetBaseActorValue(SkillValue(i));
			}
			if (auto* d = SkillData(a_p)) {
				snap.skills = *d;
			}
			for (std::size_t i = 0; i < std::size(kAttributes); ++i) {
				snap.attributes[i] = av->GetBaseActorValue(kAttributes[i]);
			}
			for (auto* perk : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::BGSPerk>()) {
				if (perk && a_p->HasPerk(perk)) {
					snap.perks.push_back(perk->GetFormID());
				}
			}
			snap.perkPoints = a_p->GetGameStatsData().perkCount;
			snap.level = a_p->GetLevel();
			snap.race = a_p->GetRace() ? a_p->GetRace()->GetFormID() : 0;
			snap.taken = true;
			logger::info("metamorphosis: noted {} skills, {} perks, {} perk point(s), level {}, health {:.0f} magicka {:.0f} stamina {:.0f}",
				kSkills, snap.perks.size(), snap.perkPoints, snap.level, snap.attributes[0], snap.attributes[1], snap.attributes[2]);
		}

		void PutBack(RE::PlayerCharacter* a_p)
		{
			auto& snap = g_snap;
			if (!snap.taken) {
				return;
			}
			auto*       av = a_p->AsActorValueOwner();
			auto*       race = a_p->GetRace();
			int         skillsFixed = 0, perksBack = 0;
			std::string changed;
			for (std::size_t i = 0; i < kSkills; ++i) {
				const float now = av->GetBaseActorValue(SkillValue(i));
				if (now != snap.base[i]) {
					changed += std::format(" {}:{:.0f}->{:.0f}", i, snap.base[i], now);
					av->SetBaseActorValue(SkillValue(i), snap.base[i]);
					++skillsFixed;
				}
			}
			if (auto* d = SkillData(a_p)) {
				*d = snap.skills;
			}
			for (std::size_t i = 0; i < std::size(kAttributes); ++i) {
				if (const float now = av->GetBaseActorValue(kAttributes[i]); now != snap.attributes[i]) {
					changed += std::format(" attribute {}:{:.0f}->{:.0f}", i, snap.attributes[i], now);
					av->SetBaseActorValue(kAttributes[i], snap.attributes[i]);
				}
			}
			for (auto id : snap.perks) {
				if (auto* perk = RE::TESForm::LookupByID<RE::BGSPerk>(id); perk && !a_p->HasPerk(perk)) {
					a_p->AddPerk(perk);
					++perksBack;
				}
			}
			if (a_p->GetGameStatsData().perkCount != snap.perkPoints) {
				changed += std::format(" perk points {}->{}", snap.perkPoints, a_p->GetGameStatsData().perkCount);
				a_p->GetGameStatsData().perkCount = snap.perkPoints;
			}
			if (a_p->GetLevel() != snap.level) {
				changed += std::format(" level {}->{} (left as it is)", snap.level, a_p->GetLevel());
			}
			logger::info("metamorphosis: put back {} skill(s), {} perk(s); race {:08X} -> {:08X} '{}'; the menu had changed:{}", skillsFixed,
				perksBack, snap.race, race ? race->GetFormID() : 0, race ? race->GetName() : "?", changed.empty() ? " nothing" : changed.c_str());
			snap = {};
			Shift::Notify("You are someone new. Nobody knows what you did.");
		}

		class MenuSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (a_event && a_event->menuName == RE::RaceSexMenu::MENU_NAME && g_snap.taken) {
					const bool opening = a_event->opening;
					SKSE::GetTaskInterface()->AddTask([opening] {
						if (opening) {
							g_menuOpened = true;
							logger::info("metamorphosis: the character menu is open");
						} else if (g_menuOpened) {
							g_menuOpened = false;
							g_restoreIn = kRestoreAfter;
							logger::info("metamorphosis: the character menu closed");
						}
					});
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Install()
	{
		static MenuSink sink;
		if (auto* ui = RE::UI::GetSingleton()) {
			ui->AddEventSink<RE::MenuOpenCloseEvent>(&sink);
		}
	}

	void OnCast()
	{
		auto* p = RE::PlayerCharacter::GetSingleton();
		if (!p || !Evolve::Has("metamorphosis") || g_snap.taken) {
			return;
		}
		if (Shift::ActiveCreature()) {
			Shift::Notify("You must return to your own form first.");
			return;
		}
		if (Face::Disguised()) {
			Shift::Notify("Take off the stolen face first.");
			return;
		}
		if (p->IsInCombat()) {
			Shift::Notify("You cannot reshape yourself in a fight.");
			return;
		}
		auto*       av = p->AsActorValueOwner();
		const float stamina = av->GetActorValue(RE::ActorValue::kStamina);
		const float most = av->GetPermanentActorValue(RE::ActorValue::kStamina);
		if (stamina < most - 1.0f) {
			Shift::Notify("Metamorphosis needs all your Stamina.");
			return;
		}
		av->DamageActorValue(RE::ActorValue::kStamina, stamina);
		Shift::ForgiveAll("Every bounty on you is gone.");
		Take(p);
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!vm || !Papyrus::Has("Game", "ShowRaceMenu", true)) {
			g_snap = {};
			return;
		}
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> cb;
		vm->DispatchStaticCall("Game", "ShowRaceMenu", RE::MakeFunctionArguments(), cb);
		logger::info("metamorphosis: the character menu asked for");
	}

	void OnFrame(float a_delta)
	{
		if (g_restoreIn < 0.0f) {
			return;
		}
		if ((g_restoreIn -= a_delta) <= 0.0f) {
			g_restoreIn = -1.0f;
			if (auto* p = RE::PlayerCharacter::GetSingleton()) {
				PutBack(p);
			}
		}
	}
}
