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

		float Dial(std::string_view a_name, float a_fallback) { return Registry::Get().Dial(a_name, a_fallback); }

		// light body armor of these materials leaves the carapace on (the user's list, 2026-10-25: fur, hide, leather,
		// chitin, dragonscale): their keywords, Skyrim.esm and Dragonborn.esm
		constexpr std::string_view kNatural[] = { "ArmorMaterialHide", "ArmorMaterialLeather", "ArmorMaterialDragonscale",
			"DLC2ArmorMaterialChitinLight" };
		// the game's Fur Armor has no material keyword: its four cuirasses, Skyrim.esm
		constexpr RE::FormID kFur[] = { 0x06F393, 0x10594B, 0x10594D, 0x10594F };

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

		float Wanted(RE::PlayerCharacter* a_p)
		{
			const int rank = Evolve::Has("subtle_carapace_2") ? 2 : Evolve::Has("subtle_carapace") ? 1 : 0;
			if (rank == 0 || a_p->IsDead() || (!Shift::ActiveCreature() && WearsBodyArmor(a_p))) {
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
		if (want != g_logged) {
			g_logged = want;
			logger::info("subtle carapace: armor +{:.0f}{}", want, want == 0.0f && Evolve::Has("subtle_carapace") ? " (body armor worn)" : "");
		}
	}
}
