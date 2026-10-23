#include "Claws.h"

#include "Registry.h"

namespace protean::Claws
{
	namespace
	{
		std::int32_t Count(RE::PlayerCharacter* a_p, RE::TESBoundObject* a_item)
		{
			const auto counts = a_p->GetInventoryCounts([a_item](RE::TESBoundObject& a_obj) { return &a_obj == a_item; });
			const auto it = counts.find(a_item);
			return it != counts.end() ? it->second : 0;
		}

		void Wear(RE::PlayerCharacter* a_p, RE::TESObjectARMO* a_piece, RE::BGSBipedObjectForm::BipedObjectSlot a_slot, bool a_on,
			const char* a_hand)
		{
			if (!a_piece) {
				return;
			}
			auto*      mgr = RE::ActorEquipManager::GetSingleton();
			const bool worn = a_p->GetWornArmor(a_slot) == a_piece;
			if (a_on && !worn) {
				if (Count(a_p, a_piece) == 0) {
					a_p->AddObjectToContainer(a_piece, nullptr, 1, nullptr);
				}
				// no prevent-removal lock (see Shift::RestoreGear): unequipping the weapon takes the piece off
				mgr->EquipObject(a_p, a_piece, nullptr, 1, nullptr, false, false, false, true);
				logger::info("claws: {} hand on", a_hand);
			} else if (!a_on) {
				if (worn) {
					mgr->UnequipObject(a_p, a_piece, nullptr, 1, nullptr, false, true, false, true);
					logger::info("claws: {} hand off", a_hand);
				}
				if (const auto n = Count(a_p, a_piece); n > 0) {
					a_p->RemoveItem(a_piece, n, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
				}
			}
		}
	}

	void Sync()
	{
		auto*       p = RE::PlayerCharacter::GetSingleton();
		const auto& reg = Registry::Get();
		auto*       weapon = reg.ClawWeapon();
		if (!p || !weapon) {
			return;
		}
		using Slot = RE::BGSBipedObjectForm::BipedObjectSlot;
		Wear(p, reg.ClawPiece(false), Slot::kModArmRight, p->GetEquippedObject(false) == weapon, "right");
		Wear(p, reg.ClawPiece(true), Slot::kModArmLeft, p->GetEquippedObject(true) == weapon, "left");
	}

	void Give()
	{
		auto* p = RE::PlayerCharacter::GetSingleton();
		if (auto* weapon = Registry::Get().ClawWeapon(); p && weapon && Count(p, weapon) < 2) {
			p->AddObjectToContainer(weapon, nullptr, 2 - Count(p, weapon), nullptr);  // one for each hand
		}
	}
}
