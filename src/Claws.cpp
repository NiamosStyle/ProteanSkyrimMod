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

		constexpr auto kPrefix = "ProteanClaw|"sv;
		constexpr auto kFolder = "Protean\\Claws\\"sv;

		std::string g_folder{ kFolder };  // the models in use (dev 'clawstyle' points it at a style's own folder)

		// the blades now hanging in a 1st-person body
		std::vector<RE::NiAVObject*> Hung(RE::NiAVObject* a_root, char a_side)
		{
			std::vector<RE::NiAVObject*> out;
			const auto                   mine = std::string(kPrefix) + "NPC " + a_side + " ";
			RE::BSVisit::TraverseScenegraphGeometries(a_root, [&](RE::BSGeometry* a_geo) {
				if (std::string_view(a_geo->name.c_str()).starts_with(mine)) {
					out.push_back(a_geo);
				}
				return RE::BSVisit::BSVisitControl::kContinue;
			});
			return out;
		}

		void Hang(RE::PlayerCharacter* a_p, bool a_left, bool a_on)
		{
			auto* root = a_p->Get3D(true);
			if (!root) {
				return;
			}
			const char side = a_left ? 'L' : 'R';
			const auto hung = Hung(root, side);
			if (!a_on) {
				for (auto* blade : hung) {
					if (blade->parent) {
						blade->parent->DetachChild2(blade);
					}
				}
				return;
			}
			if (!hung.empty()) {
				return;
			}
			const auto* base = a_p->GetActorBase();
			const char  sex = base && base->GetSex() == RE::SEX::kFemale ? 'F' : 'M';
			const auto  path = fmt::format("{}Claws1st_{}_{}.nif", g_folder, side, sex);
			RE::NiPointer<RE::NiNode> model;
			if ((RE::BSModelDB::Demand(path.c_str(), model, {}) != RE::BSResource::ErrorCode::kNone || !model) &&
				(RE::BSModelDB::Demand(("Meshes\\" + path).c_str(), model, {}) != RE::BSResource::ErrorCode::kNone || !model)) {
				logger::warn("claws: could not load {}", path);
				return;
			}
			// the loaded model is shared: hang a copy
			RE::NiPointer<RE::NiObject> clone(model->Clone());
			auto*                       copy = clone ? clone->AsNode() : nullptr;
			if (!copy) {
				return;
			}
			std::vector<RE::NiPointer<RE::NiAVObject>> blades;
			for (auto& child : copy->GetChildren()) {
				if (child) {
					blades.emplace_back(child);
				}
			}
			int n = 0;
			for (auto& blade : blades) {
				const std::string_view name(blade->name.c_str());
				if (!name.starts_with(kPrefix)) {
					continue;
				}
				auto* bone = root->GetObjectByName(RE::BSFixedString(std::string(name.substr(kPrefix.size()))));
				if (auto* node = bone ? bone->AsNode() : nullptr) {
					copy->DetachChild2(blade.get());
					blade->local = RE::NiTransform();
					node->AttachChild(blade.get(), true);
					RE::NiUpdateData update;
					blade->Update(update);
					++n;
				}
			}
			logger::info("claws: {} hand, {} blade(s) hung in 1st person", a_left ? "left" : "right", n);
		}

		void HangBoth(RE::PlayerCharacter* a_p, RE::TESObjectWEAP* a_weapon)
		{
			Hang(a_p, false, a_p->GetEquippedObject(false) == a_weapon);
			Hang(a_p, true, a_p->GetEquippedObject(true) == a_weapon);
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
		HangBoth(p, weapon);
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
		using Slot = RE::BGSBipedObjectForm::BipedObjectSlot;
		Wear(p, reg.ClawPiece(false), Slot::kModArmRight, false, "right");
		Wear(p, reg.ClawPiece(true), Slot::kModArmLeft, false, "left");
		Hang(p, false, false);
		Hang(p, true, false);
		SKSE::GetTaskInterface()->AddTask([]() { Sync(); });
		logger::info("claws: models now from {}", g_folder);
	}

	void OnFrame(float a_delta)
	{
		static float since = 0.0f;
		if ((since += a_delta) < 0.25f) {
			return;
		}
		since = 0.0f;
		auto* p = RE::PlayerCharacter::GetSingleton();
		if (auto* weapon = Registry::Get().ClawWeapon(); p && weapon) {
			HangBoth(p, weapon);
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
