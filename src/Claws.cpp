// Protean - a shapeshifter mod for Skyrim SE
// Copyright (C) 2026 NiamosStyle
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. It is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file or <https://www.gnu.org/licenses/>.

#include "Claws.h"

#include "Blade.h"
#include "Evolve.h"
#include "Registry.h"

namespace protean::Claws
{
	namespace
	{
		constexpr auto kPerk = "rending_claws";

		float MaxStamina(RE::PlayerCharacter* a_p)
		{
			return a_p->AsActorValueOwner()->GetPermanentActorValue(RE::ActorValue::kStamina) +
			       a_p->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kStamina);
		}

		// the claws hit hardest on a full stamina bar and about a third as hard on an empty one; the top rises with level
		// faster and faster, the way a specialist's weapon does (skill, perks and smithing multiply)
		void SetDamage(RE::PlayerCharacter* a_p, RE::TESObjectWEAP* a_weapon)
		{
			const auto& reg = Registry::Get();
			const float max = MaxStamina(a_p);
			const float share = max > 0.0f ? std::clamp(a_p->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina) / max, 0.0f, 1.0f) : 0.0f;
			const float level = static_cast<float>(a_p->GetLevel());
			const float high = reg.Dial("Protean_ClawDamageMax", 10.0f) + reg.Dial("Protean_ClawDamagePerLevel", 0.7f) * level +
			                   reg.Dial("Protean_ClawDamagePerLevelSq", 0.02f) * level * level;
			const float minimum = reg.Dial("Protean_ClawDamageMin", 6.0f);
			const float low = std::max(minimum, high * reg.Dial("Protean_ClawEmptyShare", 0.3f));
			a_weapon->attackDamage = static_cast<std::uint16_t>(std::clamp(std::lround(low + (high - low) * share), 0L, 65535L));
		}

		// every swing of a clawed hand costs stamina
		class AnimSink : public RE::BSTEventSink<RE::BSAnimationGraphEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override
			{
				auto* p = RE::PlayerCharacter::GetSingleton();
				auto* weapon = Registry::Get().ClawWeapon();
				if (!a_event || !p || !weapon || a_event->holder != p) {
					return RE::BSEventNotifyControl::kContinue;
				}
				const std::string_view tag(a_event->tag.c_str());
				const bool right = tag == "weaponSwing" && p->GetEquippedObject(false) == weapon;
				const bool left = tag == "weaponLeftSwing" && p->GetEquippedObject(true) == weapon;
				if (right || left) {
					const float cost = Registry::Get().Dial("Protean_ClawSwingCost", 5.0f);
					if (cost > 0.0f) {
						p->AsActorValueOwner()->DamageActorValue(RE::ActorValue::kStamina, cost);
					}
					logger::info("claws: {} swing, stamina -{:.0f}, damage {}", right ? "right" : "left", cost, weapon->attackDamage);
				} else if (tag.find("wing") != std::string_view::npos && (p->GetEquippedObject(false) == weapon || p->GetEquippedObject(true) == weapon)) {
					logger::info("claws: other swing event '{}'", tag);  // to learn what unarmed swings send
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
		AnimSink g_anim;

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
				Blade::SleeveSound(a_p, true, "claws");  // the sleeves' grow sound (user, 2026-11-14: the claws had none)
			} else if (!a_on) {
				if (worn) {
					mgr->UnequipObject(a_p, a_piece, nullptr, 1, nullptr, false, true, false, true);
					logger::info("claws: {} hand off", a_hand);
					Blade::SleeveSound(a_p, false, "claws");
				}
				if (const auto n = Count(a_p, a_piece); n > 0) {
					a_p->RemoveItem(a_piece, n, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
				}
			}
		}

		constexpr auto kFolder = "Protean\\Claws\\"sv;

		std::string g_folder{ kFolder };  // the 3rd-person models in use (dev 'clawstyle' points it at a style's own folder)
		float       g_rewear{ 0.0f };     // seconds until the claw pieces go back on after a style change

		struct Side
		{
			bool                                    left;
			RE::BGSBipedObjectForm::BipedObjectSlot slot;
			const char*                             firstName;
			const char*                             firstModel;
			const char*                             hand;
			const char*                             tag;
		};
		constexpr Side kSides[] = {
			{ false, RE::BGSBipedObjectForm::BipedObjectSlot::kModArmRight, "ProteanClaws1st_R", "Protean\\Claws\\Claws1st_R_{}.nif", "right", "claws right" },  // 59
			{ true, RE::BGSBipedObjectForm::BipedObjectSlot::kModArmLeft, "ProteanClaws1st_L", "Protean\\Claws\\Claws1st_L_{}.nif", "left", "claws left" },     // 58
		};

		// The sleeve and claws (2026-11-13: the sinew arm and long claws of tools/build_claw_arms.py) are on a hand only
		// while it holds the claws fully drawn, as the Blade, Shield and Hammerfist are; put away, the arm is the
		// player's own. In 1st person the same skinned model (cut at the elbow) hangs in the 1st-person body, bound to
		// its bones (Blade::HangFirstPerson): the game builds no slot 58/59 there.
		void Show(RE::PlayerCharacter* a_p, RE::TESObjectWEAP* a_weapon, bool a_allOff = false)
		{
			using WS = RE::WEAPON_STATE;
			const auto* st = a_p->AsActorState();
			const bool  drawn = st && st->GetWeaponState() == WS::kDrawn;
			const auto& reg = Registry::Get();
			for (const auto& side : kSides) {
				const bool want = !a_allOff && drawn && a_p->GetEquippedObject(side.left) == a_weapon;
				Wear(a_p, reg.ClawPiece(side.left), side.slot, want, side.hand);
				Blade::HangFirstPerson(a_p, side.firstName, side.firstModel, want, side.tag);
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
		Show(p, weapon);
		// the swing listener: the player's animation graphs are rebuilt on a race change, so add it again
		// (claws go back on with an equip, which brings us here)
		p->RemoveAnimationGraphEventSink(&g_anim);
		p->AddAnimationGraphEventSink(&g_anim);
	}

	void SetStyle(const std::string& a_style)
	{
		g_folder = std::string(kFolder) + (a_style.empty() || a_style == "default" ? "" : a_style + "\\");
		const auto& reg = Registry::Get();
		for (const bool left : { false, true }) {
			auto* piece = reg.ClawPiece(left);
			if (!piece) {
				continue;
			}
			for (auto* addon : piece->armorAddons) {
				for (const auto sex : { RE::SEXES::kMale, RE::SEXES::kFemale }) {
					const auto model = fmt::format("{}Claws_{}_{}.nif", g_folder, left ? 'L' : 'R', sex == RE::SEXES::kMale ? 'M' : 'F');
					addon->bipedModels[sex].SetModel(model.c_str());
					addon->bipedModel1stPersons[sex].SetModel(model.c_str());
				}
			}
		}
		// take everything off, then put it back on with the new models
		auto* p = RE::PlayerCharacter::GetSingleton();
		if (!p) {
			return;
		}
		if (auto* weapon = reg.ClawWeapon()) {
			Show(p, weapon, true);
		}
		// back on a moment later: put straight back on, the 3rd-person body keeps the old model (seen in game)
		g_rewear = 0.5f;
		logger::info("claws: models now from {}", g_folder);
	}

	void OnFrame(float a_delta)
	{
		if (g_rewear > 0.0f && (g_rewear -= a_delta) <= 0.0f) {
			Sync();
		}
		auto* p = RE::PlayerCharacter::GetSingleton();
		auto* weapon = Registry::Get().ClawWeapon();
		if (p && weapon && g_rewear <= 0.0f) {
			Show(p, weapon);  // every frame: they grow the moment the draw is done and go back as the sheathe starts
		}
		static float since = 0.0f;
		if ((since += a_delta) < 0.25f) {
			return;
		}
		since = 0.0f;
		if (p && weapon) {
			SetDamage(p, weapon);
			// the perk keeps one claw for each hand in the inventory (checked now and then: it counts the inventory)
			static float sinceGive = 0.0f;
			if ((sinceGive += 0.25f) >= 2.0f) {
				sinceGive = 0.0f;
				if (Evolve::Has(kPerk)) {
					Give();
				}
			}
		}
	}

	void Give()
	{
		auto* p = RE::PlayerCharacter::GetSingleton();
		if (auto* weapon = Registry::Get().ClawWeapon(); p && weapon && Count(p, weapon) < 2) {
			p->AddObjectToContainer(weapon, nullptr, 2 - Count(p, weapon), nullptr);  // one for each hand
		}
	}
}
