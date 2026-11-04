#include "Blade.h"

#include "Evolve.h"
#include "Registry.h"

#undef GetObject  // windows.h renames it to GetObjectA, which hides BGSDefaultObjectManager::GetObject

namespace protean::Blade
{
	namespace
	{
		constexpr auto kPerk = "blade";

		bool  g_had{ false };   // the Blade was in the right hand at the last look
		float g_wait{ 0.0f };   // after a change, give the game time to carry it out before looking again

		std::int32_t Count(RE::PlayerCharacter* a_p, RE::TESBoundObject* a_item)
		{
			const auto counts = a_p->GetInventoryCounts([a_item](RE::TESBoundObject& a_obj) { return &a_obj == a_item; });
			const auto it = counts.find(a_item);
			return it != counts.end() ? it->second : 0;
		}

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

		// like a greatsword, the Blade keeps the left hand empty (checked a few times a second)
		void KeepLeftEmpty(RE::PlayerCharacter* a_p, RE::TESObjectWEAP* a_blade, float a_step)
		{
			if (g_wait > 0.0f) {
				g_wait -= a_step;
				return;
			}
			const bool has = a_p->GetEquippedObject(false) == a_blade;
			auto*      left = a_p->GetEquippedObject(true);
			auto*      shield = a_p->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kShield);
			if (has && (left || shield)) {
				const char* what = shield ? shield->GetName() : left->GetName();
				if (!g_had) {
					// the Blade was just taken up: it needs both hands
					logger::info("blade: taken up, the left hand lets go of '{}'", what);
					ClearLeft(a_p, left, shield);
				} else {
					// something went into the left hand: the Blade goes away, as a greatsword would
					logger::info("blade: '{}' into the left hand, the Blade goes away", what);
					RE::ActorEquipManager::GetSingleton()->UnequipObject(a_p, a_blade);
					g_had = false;
					g_wait = 0.5f;
					return;
				}
				g_wait = 0.5f;
			}
			g_had = has;
		}

		constexpr auto kFirstName = "ProteanBlade1st";
		constexpr auto kFirstModel = "Protean\\Blade\\Sleeve1st_M.nif";

		// The game draws no body slot 57 in 1st person (seen 2026-11-04). There the plugin hangs Sleeve1st_M.nif in the
		// 1st-person body itself and binds its skinned flesh and blade to the 1st-person skeleton's bones of the same
		// names, as the game does for worn armor, so the flesh bends with the wrist.
		void FirstPerson(RE::PlayerCharacter* a_p, bool a_on)
		{
			auto* root3d = a_p->Get3D(true);
			auto* root = root3d ? root3d->AsNode() : nullptr;
			if (!root) {
				return;
			}
			auto* have = root->GetObjectByName(kFirstName);
			if (!a_on) {
				if (have && have->parent) {
					have->parent->DetachChild2(have);
					logger::info("blade: 1st person goes back");
				}
				return;
			}
			if (have) {
				return;
			}
			RE::NiPointer<RE::NiNode> model;
			if ((RE::BSModelDB::Demand(kFirstModel, model, {}) != RE::BSResource::ErrorCode::kNone || !model) &&
				(RE::BSModelDB::Demand((std::string("Meshes\\") + kFirstModel).c_str(), model, {}) != RE::BSResource::ErrorCode::kNone || !model)) {
				static bool warned = false;
				if (!warned) {
					warned = true;
					logger::warn("blade: could not load {}", kFirstModel);
				}
				return;
			}
			// the loaded model is shared: hang a copy
			RE::NiPointer<RE::NiObject> clone(model->Clone());
			auto*                       copy = clone ? clone->AsNode() : nullptr;
			if (!copy) {
				return;
			}
			copy->name = kFirstName;
			int bound = 0, missing = 0;
			RE::BSVisit::TraverseScenegraphGeometries(copy, [&](RE::BSGeometry* a_geo) {
				auto* skin = a_geo->GetGeometryRuntimeData().skinInstance.get();
				const auto n = skin && skin->skinData ? skin->skinData->bones : 0u;
				for (std::uint32_t i = 0; i < n; ++i) {
					auto* own = skin->bones[i];
					auto* real = own ? root->GetObjectByName(own->name) : nullptr;
					if (!real) {
						logger::info("blade: 1st person has no bone '{}': it follows the body instead", own ? own->name.c_str() : "?");
						real = root;
						++missing;
					}
					skin->bones[i] = real;
					skin->boneWorldTransforms[i] = &real->world;
					++bound;
				}
				if (skin) {
					skin->rootParent = root;
				}
				// its bounds were measured where the 3rd-person arm is: never cull it for being "off screen"
				a_geo->GetFlags().set(RE::NiAVObject::Flag::kAlwaysDraw);
				return RE::BSVisit::BSVisitControl::kContinue;
			});
			root->AttachChild(copy, true);
			RE::NiUpdateData update;
			copy->Update(update);
			logger::info("blade: 1st person grows ({} bones bound, {} missing)", bound, missing);
		}

		// the flesh and the bone blade are on the arm only while the Blade is drawn; never kept in the inventory
		void Sleeve(RE::PlayerCharacter* a_p, RE::TESObjectWEAP* a_blade, RE::TESObjectARMO* a_sleeve)
		{
			using WS = RE::WEAPON_STATE;
			const auto* st = a_p->AsActorState();
			const auto  ws = st ? st->GetWeaponState() : WS::kSheathed;
			// only while fully drawn: both the draw and the sheathe reach across the body to the hip as if to a
			// scabbard, which looked odd with the blade on the arm (user, 2026-11-04, 1st person); it grows once the
			// draw is done and goes back the moment the sheathe starts
			const bool want = a_p->GetEquippedObject(false) == a_blade && ws == WS::kDrawn;
			FirstPerson(a_p, want);
			const bool worn = a_p->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kModShoulder) == a_sleeve;
			auto*      mgr = RE::ActorEquipManager::GetSingleton();
			if (want && !worn) {
				if (Count(a_p, a_sleeve) == 0) {
					a_p->AddObjectToContainer(a_sleeve, nullptr, 1, nullptr);
				}
				mgr->EquipObject(a_p, a_sleeve, nullptr, 1, nullptr, false, false, false, true);
				logger::info("blade: grows (weapon state {})", static_cast<int>(ws));
			} else if (!want) {
				if (worn) {
					mgr->UnequipObject(a_p, a_sleeve, nullptr, 1, nullptr, false, true, false, true);
					logger::info("blade: goes back (weapon state {})", static_cast<int>(ws));
				}
				if (const auto n = Count(a_p, a_sleeve); n > 0) {
					a_p->RemoveItem(a_sleeve, n, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
				}
			}
		}

		// every swing costs Stamina
		class AnimSink : public RE::BSTEventSink<RE::BSAnimationGraphEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override
			{
				auto* p = RE::PlayerCharacter::GetSingleton();
				auto* blade = Registry::Get().BladeWeapon();
				if (!a_event || !p || !blade || a_event->holder != p || p->GetEquippedObject(false) != blade) {
					return RE::BSEventNotifyControl::kContinue;
				}
				const std::string_view tag(a_event->tag.c_str());
				if (tag == "weaponSwing") {
					const float cost = Registry::Get().Dial("Protean_BladeSwingCost", 5.0f);
					if (cost > 0.0f) {
						p->AsActorValueOwner()->DamageActorValue(RE::ActorValue::kStamina, cost);
					}
					logger::info("blade: swing, stamina -{:.0f}", cost);
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
		AnimSink g_anim;
	}

	void OnFrame(float a_delta)
	{
		auto*       p = RE::PlayerCharacter::GetSingleton();
		const auto& reg = Registry::Get();
		auto*       blade = reg.BladeWeapon();
		auto*       sleeve = reg.BladeSleeve();
		if (!p || !blade || !sleeve) {
			return;
		}
		// the player's animation graphs are rebuilt on a race change or a load: listen again whenever the Blade is
		// taken up (adding twice is harmless: it is taken off first)
		static bool held = false;
		const bool  has = p->GetEquippedObject(false) == blade;
		if (has && !held) {
			p->RemoveAnimationGraphEventSink(&g_anim);
			p->AddAnimationGraphEventSink(&g_anim);
		}
		held = has;
		Sleeve(p, blade, sleeve);

		static float since = 0.0f;
		if ((since += a_delta) < 0.1f) {
			return;
		}
		const float step = since;
		since = 0.0f;
		KeepLeftEmpty(p, blade, step);
		// the perk keeps the Blade in the inventory
		static float sinceGive = 0.0f;
		if ((sinceGive += step) >= 2.0f) {
			sinceGive = 0.0f;
			if (Evolve::Has(kPerk) && Count(p, blade) == 0) {
				p->AddObjectToContainer(blade, nullptr, 1, nullptr);
				logger::info("blade: the perk gives the Blade");
			}
		}
	}
}
