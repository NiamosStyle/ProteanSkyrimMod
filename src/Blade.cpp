#include "Blade.h"

#include "Registry.h"

#undef GetObject  // windows.h renames it to GetObjectA, which hides BGSDefaultObjectManager::GetObject

namespace protean::Blade
{
	namespace
	{
		bool  g_had{ false };   // the Blade was in the right hand at the last look
		float g_wait{ 0.0f };   // after a change, give the game time to carry it out before looking again

		// the game's Actor.UnequipSpell (source 0: the left hand); CommonLib has no direct call for it
		void UnequipLeftSpell(RE::PlayerCharacter* a_p, RE::SpellItem* a_spell)
		{
			auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			if (!vm) {
				return;
			}
			const auto handle = vm->GetObjectHandlePolicy()->GetHandleForObject(a_p->GetFormType(), a_p);
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> cb;
			std::int32_t source = 0;
			vm->DispatchMethodCall2(handle, "Actor", "UnequipSpell", RE::MakeFunctionArguments(std::move(a_spell), std::move(source)), cb);
		}

		void ClearLeft(RE::PlayerCharacter* a_p, RE::TESForm* a_left, RE::TESObjectARMO* a_shield)
		{
			auto* mgr = RE::ActorEquipManager::GetSingleton();
			if (a_shield) {
				mgr->UnequipObject(a_p, a_shield);
			}
			if (a_left) {
				if (auto* spell = a_left->As<RE::SpellItem>()) {
					UnequipLeftSpell(a_p, spell);
				} else if (auto* obj = a_left->As<RE::TESBoundObject>()) {
					auto* dom = RE::BGSDefaultObjectManager::GetSingleton();
					auto* slot = dom ? dom->GetObject<RE::BGSEquipSlot>(RE::DEFAULT_OBJECTS::kLeftHandEquip) : nullptr;
					mgr->UnequipObject(a_p, obj, nullptr, 1, slot);
				}
			}
		}
	}

	void OnFrame(float a_delta)
	{
		static float since = 0.0f;
		if ((since += a_delta) < 0.1f) {
			return;
		}
		const float step = since;
		since = 0.0f;
		if (g_wait > 0.0f) {
			g_wait -= step;
			return;
		}
		auto* p = RE::PlayerCharacter::GetSingleton();
		auto* blade = Registry::Get().BladeWeapon();
		if (!p || !blade) {
			return;
		}
		const bool has = p->GetEquippedObject(false) == blade;
		auto*      left = p->GetEquippedObject(true);
		auto*      shield = p->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kShield);
		if (has && (left || shield)) {
			const char* what = shield ? shield->GetName() : left->GetName();
			if (!g_had) {
				// the Blade was just taken up: it needs both hands
				logger::info("blade: taken up, the left hand lets go of '{}'", what);
				ClearLeft(p, left, shield);
			} else {
				// something went into the left hand: the Blade goes away, as a greatsword would
				logger::info("blade: '{}' into the left hand, the Blade goes away", what);
				RE::ActorEquipManager::GetSingleton()->UnequipObject(p, blade);
				g_had = false;
				g_wait = 0.5f;
				return;
			}
			g_wait = 0.5f;
		}
		g_had = has;
	}
}
