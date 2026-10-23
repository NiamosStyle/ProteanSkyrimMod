#include "Keen.h"

#include "Registry.h"
#include "Shift.h"

#undef GetObject  // windows.h renames it to GetObjectA

namespace protean::Keen
{
	namespace
	{
		using DOID = RE::DefaultObjectID;

		RE::BGSListForm* g_list = nullptr;  // made at runtime: what an animal's Favorites menu offers

		RE::TESForm** Slot(DOID a_id)
		{
			auto* dom = RE::BGSDefaultObjectManager::GetSingleton();
			return dom ? dom->GetObject(a_id) : nullptr;
		}

		// Points the two vampire default objects at the animal while it lives; puts them back after.
		class Borrow
		{
		public:
			Borrow()
			{
				auto* p = RE::PlayerCharacter::GetSingleton();
				if (!p || !g_list || !Shift::ActiveCreature()) {
					return;
				}
				_race = Slot(DOID::kVampireRace);
				_spells = Slot(DOID::kVampireSpells);
				if (!_race || !_spells) {
					_race = _spells = nullptr;
					return;
				}
				g_list->forms.clear();
				if (auto* revert = Registry::Get().Revert()) {
					g_list->forms.push_back(revert);
				}
				_oldRace = *_race;
				_oldSpells = *_spells;
				*_race = p->GetRace();
				*_spells = g_list;
			}
			~Borrow()
			{
				if (_race) {
					*_race = _oldRace;
					*_spells = _oldSpells;
				}
			}
			Borrow(const Borrow&) = delete;
			Borrow& operator=(const Borrow&) = delete;

		private:
			RE::TESForm** _race = nullptr;
			RE::TESForm** _spells = nullptr;
			RE::TESForm*  _oldRace = nullptr;
			RE::TESForm*  _oldSpells = nullptr;
		};

		bool IsVampire(RE::FavoritesMenu* a_menu) { return a_menu && a_menu->GetRuntimeData().isVampire; }

		RE::UI::Create_t* g_create = nullptr;

		RE::IMenu* Create()
		{
			Borrow b;
			auto* menu = g_create();
			logger::info("Keen: Favorites created, animal {}, isVampire {}", Shift::ActiveCreature() != nullptr,
				IsVampire(static_cast<RE::FavoritesMenu*>(menu)));
			return menu;
		}

		struct ProcessMessage
		{
			static RE::UI_MESSAGE_RESULTS thunk(RE::FavoritesMenu* a_this, RE::UIMessage& a_message)
			{
				Borrow b;
				const auto result = func(a_this, a_message);
				// An animal has the menu controls locked (Shift), and they also work the menu itself: give them back
				// while it is open.
				const bool animal = Shift::ActiveCreature() != nullptr;
				auto*      cm = RE::ControlMap::GetSingleton();
				if (a_message.type == RE::UI_MESSAGE_TYPE::kShow) {
					logger::info("Keen: Favorites shown, isVampire {}, {} entries", IsVampire(a_this),
						a_this->GetRuntimeData().favorites.size());
					if (animal && cm) {
						cm->ToggleControls(RE::UserEvents::USER_EVENT_FLAG::kMenu, true, false);
					}
				} else if (a_message.type == RE::UI_MESSAGE_TYPE::kHide && animal && cm) {
					cm->ToggleControls(RE::UserEvents::USER_EVENT_FLAG::kMenu, false, false);
				}
				return result;
			}
			static inline REL::Relocation<decltype(thunk)> func;
			static constexpr std::size_t                   idx = 0x04;  // IMenu::ProcessMessage
		};
	}

	void Install()
	{
		if (auto* factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSListForm>()) {
			g_list = factory->Create();
		}
		if (auto* ui = RE::UI::GetSingleton()) {
			if (auto it = ui->menuMap.find(RE::FavoritesMenu::MENU_NAME); it != ui->menuMap.end() && it->second.create) {
				g_create = it->second.create;
				it->second.create = Create;
			}
		}
		REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_FavoritesMenu[0] };
		ProcessMessage::func = vtbl.write_vfunc(ProcessMessage::idx, ProcessMessage::thunk);
		logger::info("Keen: list {}, creator wrapped {}", g_list != nullptr, g_create != nullptr);
	}
}
