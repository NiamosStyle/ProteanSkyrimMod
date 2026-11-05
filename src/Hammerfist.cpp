#include "Hammerfist.h"

#include "Blade.h"
#include "Evolve.h"
#include "Registry.h"

namespace protean::Hammerfist
{
	namespace
	{
		constexpr auto kPerk = "hammerfist";

		struct Side
		{
			bool                                      left;
			RE::BGSBipedObjectForm::BipedObjectSlot   slot;
			const char*                               firstName;
			const char*                               firstModel;
		};
		constexpr Side kSides[] = {
			{ false, RE::BGSBipedObjectForm::BipedObjectSlot::kModChestSecondary, "ProteanHammerfist1st_R", "Protean\\Hammerfist\\Arm1st_R_{}.nif" },  // 56
			{ true, RE::BGSBipedObjectForm::BipedObjectSlot::kModMisc2, "ProteanHammerfist1st_L", "Protean\\Hammerfist\\Arm1st_L_{}.nif" },             // 60
		};

		float Dial(std::string_view a_name, float a_fallback) { return Registry::Get().Dial(a_name, a_fallback); }

		std::int32_t Count(RE::PlayerCharacter* a_p, RE::TESBoundObject* a_item)
		{
			const auto counts = a_p->GetInventoryCounts([a_item](RE::TESBoundObject& a_obj) { return &a_obj == a_item; });
			const auto it = counts.find(a_item);
			return it != counts.end() ? it->second : 0;
		}

		// the arm and fist on a hand only while that hand holds a Hammerfist fully drawn; never kept in the inventory
		void Arm(RE::PlayerCharacter* a_p, RE::TESObjectWEAP* a_weapon, RE::TESObjectARMO* a_arm, const Side& a_side)
		{
			using WS = RE::WEAPON_STATE;
			const auto* st = a_p->AsActorState();
			const auto  ws = st ? st->GetWeaponState() : WS::kSheathed;
			const bool  want = a_p->GetEquippedObject(a_side.left) == a_weapon && ws == WS::kDrawn;
			Blade::HangFirstPerson(a_p, a_side.firstName, a_side.firstModel, want, a_side.left ? "hammerfist left" : "hammerfist right");
			const bool worn = a_p->GetWornArmor(a_side.slot) == a_arm;
			auto*      mgr = RE::ActorEquipManager::GetSingleton();
			if (want && !worn) {
				if (Count(a_p, a_arm) == 0) {
					a_p->AddObjectToContainer(a_arm, nullptr, 1, nullptr);
				}
				mgr->EquipObject(a_p, a_arm, nullptr, 1, nullptr, false, false, false, true);
				logger::info("hammerfist: {} arm grows", a_side.left ? "left" : "right");
			} else if (!want) {
				if (worn) {
					mgr->UnequipObject(a_p, a_arm, nullptr, 1, nullptr, false, true, false, true);
					logger::info("hammerfist: {} arm goes back", a_side.left ? "left" : "right");
				}
				if (const auto n = Count(a_p, a_arm); n > 0) {
					a_p->RemoveItem(a_arm, n, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
				}
			}
		}

		// the claws' damage (full on a full Stamina bar, about a third on an empty one; rising with level) times a mace's
		// lead over a sword
		void SetDamage(RE::PlayerCharacter* a_p, RE::TESObjectWEAP* a_weapon)
		{
			auto*       av = a_p->AsActorValueOwner();
			const float max = av->GetPermanentActorValue(RE::ActorValue::kStamina) +
			                  a_p->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kStamina);
			const float share = max > 0.0f ? std::clamp(av->GetActorValue(RE::ActorValue::kStamina) / max, 0.0f, 1.0f) : 0.0f;
			const float level = static_cast<float>(a_p->GetLevel());
			const float high = Dial("Protean_ClawDamageMax", 10.0f) + Dial("Protean_ClawDamagePerLevel", 0.7f) * level +
			                   Dial("Protean_ClawDamagePerLevelSq", 0.02f) * level * level;
			const float low = std::max(Dial("Protean_ClawDamageMin", 6.0f), high * Dial("Protean_ClawEmptyShare", 0.3f));
			const float dmg = (low + (high - low) * share) * Dial("Protean_HammerfistDamageMult", 1.25f);
			a_weapon->attackDamage = static_cast<std::uint16_t>(std::clamp(std::lround(dmg), 0L, 65535L));
		}

		// every swing of a Hammerfist costs Stamina
		class AnimSink : public RE::BSTEventSink<RE::BSAnimationGraphEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override
			{
				auto* p = RE::PlayerCharacter::GetSingleton();
				auto* weapon = Registry::Get().HammerfistWeapon();
				if (!a_event || !p || !weapon || a_event->holder != p) {
					return RE::BSEventNotifyControl::kContinue;
				}
				const std::string_view tag(a_event->tag.c_str());
				const bool right = tag == "weaponSwing" && p->GetEquippedObject(false) == weapon;
				const bool left = tag == "weaponLeftSwing" && p->GetEquippedObject(true) == weapon;
				if (right || left) {
					const float cost = Dial("Protean_HammerfistSwingCost", 7.0f);
					if (cost > 0.0f) {
						p->AsActorValueOwner()->DamageActorValue(RE::ActorValue::kStamina, cost);
					}
					logger::info("hammerfist: {} swing, stamina -{:.0f}, damage {}", right ? "right" : "left", cost, weapon->attackDamage);
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
		AnimSink g_anim;
	}

	void Install() {}

	void OnFrame(float a_delta)
	{
		auto*       p = RE::PlayerCharacter::GetSingleton();
		const auto& reg = Registry::Get();
		auto*       weapon = reg.HammerfistWeapon();
		if (!p || !weapon) {
			return;
		}
		// the player's animation graphs are rebuilt on a race change or a load: listen again whenever one is taken up
		static bool held = false;
		const bool  has = p->GetEquippedObject(false) == weapon || p->GetEquippedObject(true) == weapon;
		if (has && !held) {
			p->RemoveAnimationGraphEventSink(&g_anim);
			p->AddAnimationGraphEventSink(&g_anim);
		}
		held = has;
		for (const auto& side : kSides) {
			if (auto* arm = reg.HammerfistArm(side.left)) {
				Arm(p, weapon, arm, side);
			}
		}
		static float since = 0.0f;
		if ((since += a_delta) < 0.25f) {
			return;
		}
		since = 0.0f;
		SetDamage(p, weapon);
		// the perk keeps one for each hand in the inventory
		static float sinceGive = 0.0f;
		if ((sinceGive += 0.25f) >= 2.0f) {
			sinceGive = 0.0f;
			if (Evolve::Has(kPerk) && Count(p, weapon) < 2) {
				p->AddObjectToContainer(weapon, nullptr, 2 - Count(p, weapon), nullptr);
				logger::info("hammerfist: the perk gives the Hammerfists");
			}
		}
	}
}
