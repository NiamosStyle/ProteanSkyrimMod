// Protean - a shapeshifter mod for Skyrim SE
// Copyright (C) 2026 NiamosStyle
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. It is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file or <https://www.gnu.org/licenses/>.

#include "Carapace.h"

#include "Evolve.h"
#include "Registry.h"
#include "Shift.h"
#include "Skills.h"
#include "State.h"

namespace protean::Carapace
{
	namespace
	{
		constexpr float kCheckEvery = 0.25f;  // seconds; gear and level change rarely

		float g_wait = 0.0f;
		float g_logged = -1.0f;
		float g_loggedMagic = -1.0f;

		float Dial(std::string_view a_name, float a_fallback) { return Registry::Get().Dial(a_name, a_fallback); }

		// light body armor of these materials leaves the carapace on (the user's list, 2026-10-25: fur, hide, leather,
		// chitin, dragonscale; 2026-11-14 added the shrouded armors, Thieves Guild, Guild Master's, Linwe's, Morag Tong
		// and Forsworn, all tempered with leather alone, but not the leather-tempered sets with metal on them, such as
		// the hold guards'): their keywords, Skyrim.esm and Dragonborn.esm
		constexpr std::string_view kNatural[] = { "ArmorMaterialHide", "ArmorMaterialLeather", "ArmorMaterialDragonscale",
			"DLC2ArmorMaterialChitinLight",
			"ArmorDarkBrotherhood",              // Shrouded, Ancient Shrouded and Worn Shrouded Armor
			"ArmorMaterialThievesGuild", "ArmorMaterialThievesGuildLeader", "DLC2ArmorMaterialMoragTong",
			"ArmorMaterialForsworn", "ArmorMaterialMS02Forsworn" };   // Forsworn Armor, Armor of the Old Gods
		// cuirasses with no material keyword: the game's Fur Armor (four) and Linwe's Armor, Skyrim.esm
		constexpr RE::FormID kFur[] = { 0x06F393, 0x10594B, 0x10594D, 0x10594F, 0x108544 };

		bool WearsBodyArmor(RE::PlayerCharacter* a_p)
		{
			auto* body = a_p->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kBody);
			if (!body || !(body->IsLightArmor() || body->IsHeavyArmor())) {
				return false;
			}
			if (body->IsHeavyArmor()) {
				return true;
			}
			const auto* base = body->templateArmor ? body->templateArmor : body;
			const bool  natural = std::ranges::any_of(kNatural, [&](std::string_view k) { return body->HasKeywordString(k); }) ||
			                     std::ranges::find(kFur, base->GetFormID()) != std::end(kFur) ||
			                     std::ranges::find(kFur, body->GetFormID()) != std::end(kFur);
			return !natural;
		}

		int Rank(RE::PlayerCharacter* a_p)
		{
			const int rank = Evolve::Has("subtle_carapace_2") ? 2 : Evolve::Has("subtle_carapace") ? 1 : 0;
			return rank == 0 || a_p->IsDead() || (!Shift::ActiveCreature() && WearsBodyArmor(a_p)) ? 0 : rank;
		}

		// magic resistance too, under the same rule (the user, 2026-11-05: 10% at rank 1, 20% at rank 2)
		float WantedMagic(RE::PlayerCharacter* a_p)
		{
			switch (Rank(a_p)) {
			case 2:
				return Dial("Protean_Carapace2Magic", 20.0f);
			case 1:
				return Dial("Protean_CarapaceMagic", 10.0f);
			default:
				return 0.0f;
			}
		}

		float Wanted(RE::PlayerCharacter* a_p)
		{
			const int rank = Rank(a_p);
			if (rank == 0) {
				return 0.0f;
			}
			const float level = static_cast<float>(a_p->GetLevel());
			// Agile Defender counts the carapace as light armor (the user's design, 2026-10-25)
			return Skills::AgileDefender(a_p) *
			       (rank == 2 ? Dial("Protean_Carapace2Armor", 100.0f) + Dial("Protean_Carapace2ArmorPerLevel", 7.0f) * level :
			                    Dial("Protean_CarapaceArmor", 50.0f) + Dial("Protean_CarapaceArmorPerLevel", 4.0f) * level);
		}
	}

	void OnFrame(RE::PlayerCharacter* a_p, float a_delta)
	{
		if ((g_wait -= a_delta) > 0.0f) {
			return;
		}
		g_wait = kCheckEvery;
		auto&       s = State::Get();
		const float want = std::round(Wanted(a_p));
		if (want != s.carapaceApplied) {
			a_p->AsActorValueOwner()->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kDamageResist,
				want - s.carapaceApplied);
			s.carapaceApplied = want;
		}
		const float magic = WantedMagic(a_p);
		if (magic != s.carapaceMagicApplied) {
			a_p->AsActorValueOwner()->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kResistMagic,
				magic - s.carapaceMagicApplied);
			s.carapaceMagicApplied = magic;
		}
		if (want != g_logged || magic != g_loggedMagic) {
			g_logged = want;
			g_loggedMagic = magic;
			logger::info("subtle carapace: armor +{:.0f}, magic resistance +{:.0f}%{}", want, magic,
				want == 0.0f && Evolve::Has("subtle_carapace") ? " (body armor worn)" : "");
		}
	}
}
