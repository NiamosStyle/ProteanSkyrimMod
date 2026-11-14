#include "Keen.h"

#include "Evolve.h"
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
				// the menu shows only what the player knows: the Keen powers are known while the perk is owned
				g_list->forms.clear();
				if (auto* revert = Registry::Get().Revert()) {
					g_list->forms.push_back(revert);
				}
				for (auto* power : Registry::Get().KeenPowers()) {
					if (power) {
						g_list->forms.push_back(power);
					}
				}
				if (auto* call = Registry::Get().CallPower(); call && p->HasSpell(call)) {
					g_list->forms.push_back(call);  // Apex's Call
				}
				if (auto* stalk = Registry::Get().StalkerPower()) {
					g_list->forms.push_back(stalk);  // Stalker's toggle; known only while the animal has that perk
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

	namespace
	{
		constexpr std::string_view kPerk = "keen";
		constexpr const char*      kNames[] = { "Night vision", "Detect Life", "Detect Dead" };
		constexpr float            kPulse = 1.0f;  // seconds between detect pulses (each lasts 2 s)
		float                      g_tick = 0.0f;
		float                      g_pulse = 0.0f;
		bool                       g_wasAnimal = false;
		// Detect Life and Detect Dead put their glow on the creatures around, not on the player: Protean keeps them
		// on itself and repeats a short pulse. Night vision is an effect on the player, read from there.
		std::array<bool, 3>        g_on{};

		RE::PlayerCharacter* Player() { return RE::PlayerCharacter::GetSingleton(); }

		template <class F>
		void ForEachOwnEffect(RE::PlayerCharacter* a_p, RE::SpellItem* a_sense, F&& a_fn)
		{
			auto* list = a_p->AsMagicTarget()->GetActiveEffectList();
			if (!list || !a_sense) {
				return;
			}
			for (auto* ae : *list) {
				if (ae && ae->spell == a_sense && !ae->flags.all(RE::ActiveEffect::Flag::kDispelled)) {
					a_fn(ae);
				}
			}
		}

		bool IsOn(RE::PlayerCharacter* a_p, std::size_t a_i)
		{
			if (a_i > 0) {
				return g_on[a_i];
			}
			bool on = false;
			ForEachOwnEffect(a_p, Registry::Get().KeenSenses()[0], [&](RE::ActiveEffect*) { on = true; });
			return on;
		}

		void SwitchOff(RE::PlayerCharacter* a_p, RE::SpellItem* a_sense)
		{
			ForEachOwnEffect(a_p, a_sense, [](RE::ActiveEffect* a_ae) { a_ae->Dispel(true); });
		}

		void SwitchOn(RE::PlayerCharacter* a_p, RE::SpellItem* a_sense)
		{
			if (auto* caster = a_p->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)) {
				caster->CastSpellImmediate(a_sense, false, a_p, 1.0f, false, 0.0f, a_p);
			}
		}

		void Set(RE::PlayerCharacter* a_p, std::size_t a_i, bool a_on)
		{
			auto* sense = Registry::Get().KeenSenses()[a_i];
			if (a_i > 0) {
				g_on[a_i] = a_on;
				if (a_on) {
					SwitchOn(a_p, sense);  // the first pulse now; OnFrame repeats it
				}
			} else if (a_on) {
				SwitchOn(a_p, sense);
			} else {
				SwitchOff(a_p, sense);
			}
			logger::info("Keen: {} {}", kNames[a_i], a_on ? "on" : "off");
		}

		void AllOff(RE::PlayerCharacter* a_p)
		{
			for (std::size_t i = 0; i < 3; ++i) {
				if (IsOn(a_p, i)) {
					Set(a_p, i, false);
				}
			}
		}
	}

	bool OnPowerCast(RE::SpellItem* a_spell)
	{
		const auto& reg = Registry::Get();
		const auto& powers = reg.KeenPowers();
		const auto  it = std::ranges::find(powers, a_spell);
		auto*       p = Player();
		if (!a_spell || it == powers.end() || !p) {
			return false;
		}
		const auto i = static_cast<std::size_t>(it - powers.begin());
		if (!Shift::ActiveCreature()) {
			Shift::Notify("Keen works only in animal form.");
		} else if (IsOn(p, i)) {
			Set(p, i, false);
		} else if (i > 0 && p->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina) <= 1.0f) {
			Shift::Notify("You are too tired to keep your senses sharp.");
		} else {
			Set(p, i, true);
		}
		return true;
	}

	void OnFrame(RE::PlayerCharacter* a_p, float a_delta)
	{
		const auto& reg = Registry::Get();
		const bool  animal = Shift::ActiveCreature() != nullptr;

		// Detect Life and Detect Dead cost Stamina while on; an empty bar puts them out
		if (animal && (g_on[1] || g_on[2])) {
			auto* av = a_p->AsActorValueOwner();
			av->DamageActorValue(RE::ActorValue::kStamina,
				reg.Dial("Protean_KeenDrain", 2.0f) * ((g_on[1] ? 1 : 0) + (g_on[2] ? 1 : 0)) * a_delta);
			if (av->GetActorValue(RE::ActorValue::kStamina) <= 0.0f) {
				Set(a_p, 1, false);
				Set(a_p, 2, false);
				Shift::Notify("Your senses dull: you are too tired.");
			} else if ((g_pulse -= a_delta) <= 0.0f) {
				g_pulse = kPulse;
				for (std::size_t i = 1; i < 3; ++i) {
					if (g_on[i]) {
						SwitchOn(a_p, reg.KeenSenses()[i]);
					}
				}
			}
		}

		// the powers are known only by an animal with the perk; every sense goes out with the animal form
		if ((g_tick -= a_delta) > 0.0f && animal == g_wasAnimal) {
			return;
		}
		g_tick = 0.5f;
		g_wasAnimal = animal;
		const bool keen = animal && Evolve::Has(kPerk);
		for (auto* power : reg.KeenPowers()) {
			if (!power) {
				continue;
			}
			if (keen && !a_p->HasSpell(power)) {
				a_p->AddSpell(power);
			} else if (!keen && a_p->HasSpell(power)) {
				a_p->RemoveSpell(power);
			}
		}
		if (!animal) {
			AllOff(a_p);
		}
	}

	void Reset() { g_on = {}; }

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
