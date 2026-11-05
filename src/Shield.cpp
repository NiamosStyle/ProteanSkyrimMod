#include "Shield.h"

#include "Blade.h"
#include "Evolve.h"
#include "Registry.h"

namespace protean::Shield
{
	namespace
	{
		constexpr auto kPerk = "shield";
		constexpr auto kFirstName = "ProteanShield1st";
		constexpr auto kFirstModel = "Protean\\Shield\\Sleeve1st_{}.nif";  // M or F
		constexpr auto kSleeveSlot = RE::BGSBipedObjectForm::BipedObjectSlot::kModLegLeft;  // body slot 54

		std::int32_t Count(RE::PlayerCharacter* a_p, RE::TESBoundObject* a_item)
		{
			const auto counts = a_p->GetInventoryCounts([a_item](RE::TESBoundObject& a_obj) { return &a_obj == a_item; });
			const auto it = counts.find(a_item);
			return it != counts.end() ? it->second : 0;
		}

		// the flesh and the shield are on the arm only while the shield is worn and fully drawn (as the Blade); never
		// kept in the inventory
		void Sleeve(RE::PlayerCharacter* a_p, RE::TESObjectARMO* a_shield, RE::TESObjectARMO* a_sleeve)
		{
			using WS = RE::WEAPON_STATE;
			const auto* st = a_p->AsActorState();
			const auto  ws = st ? st->GetWeaponState() : WS::kSheathed;
			const bool  held = a_p->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kShield) == a_shield;
			const bool  want = held && ws == WS::kDrawn;
			Blade::HangFirstPerson(a_p, kFirstName, kFirstModel, want, "shield");
			const bool worn = a_p->GetWornArmor(kSleeveSlot) == a_sleeve;
			auto*      mgr = RE::ActorEquipManager::GetSingleton();
			if (want && !worn) {
				if (Count(a_p, a_sleeve) == 0) {
					a_p->AddObjectToContainer(a_sleeve, nullptr, 1, nullptr);
				}
				mgr->EquipObject(a_p, a_sleeve, nullptr, 1, nullptr, false, false, false, true);
				logger::info("shield: grows (weapon state {})", static_cast<int>(ws));
			} else if (!want) {
				if (worn) {
					mgr->UnequipObject(a_p, a_sleeve, nullptr, 1, nullptr, false, true, false, true);
					logger::info("shield: goes back (weapon state {})", static_cast<int>(ws));
				}
				if (const auto n = Count(a_p, a_sleeve); n > 0) {
					a_p->RemoveItem(a_sleeve, n, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
				}
			}
		}
	}

	void OnFrame(float a_delta)
	{
		auto*       p = RE::PlayerCharacter::GetSingleton();
		const auto& reg = Registry::Get();
		auto*       shield = reg.ShieldArmor();
		auto*       sleeve = reg.ShieldSleeve();
		if (!p || !shield || !sleeve) {
			return;
		}
		Sleeve(p, shield, sleeve);
		// the perk keeps the Shield in the inventory
		static float since = 0.0f;
		if ((since += a_delta) >= 2.0f) {
			since = 0.0f;
			if (Evolve::Has(kPerk) && Count(p, shield) == 0) {
				p->AddObjectToContainer(shield, nullptr, 1, nullptr);
				logger::info("shield: the perk gives the Shield");
			}
		}
	}
}
