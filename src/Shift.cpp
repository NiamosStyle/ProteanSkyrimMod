#include "Shift.h"

#include "Attacks.h"
#include "Camera.h"
#include "Face.h"
#include "State.h"

#undef GetObject  // windows.h renames it to GetObjectA, which hides BGSDefaultObjectManager::GetObject

namespace protean::Shift
{
	namespace
	{
		using Flag = RE::UserEvents::USER_EVENT_FLAG;

		// Controls locked while transformed (like the werewolf): no camera switching, no inventory/magic menus, and
		// (user decision) no activating: no doors, containers, dialogue or picking things up.
		constexpr auto kLocked = static_cast<Flag>(std::to_underlying(Flag::kPOVSwitch) | std::to_underlying(Flag::kMenu) |
		                                           std::to_underlying(Flag::kActivate));

		constexpr float kWitnessRange = 4000.0f;  // game units (~57 m)

		RE::PlayerCharacter* Player() { return RE::PlayerCharacter::GetSingleton(); }

		const Creature* Active()
		{
			const auto& s = State::Get();
			return s.active.empty() ? nullptr : Registry::Get().ById(s.active);
		}

		void SetControlsLocked(bool a_locked)
		{
			if (auto* cm = RE::ControlMap::GetSingleton()) {
				cm->ToggleControls(kLocked, !a_locked, false);
			}
		}

		// ---- gear ------------------------------------------------------------------------------

		void SaveAndStripGear(RE::PlayerCharacter* a_p)
		{
			auto& s = State::Get();
			s.worn.clear();

			struct Pending
			{
				RE::TESBoundObject* obj;
				RE::ExtraDataList*  list;
			};
			std::vector<Pending> toStrip;

			auto inv = a_p->GetInventory([](RE::TESBoundObject& o) { return o.IsArmor() || o.IsWeapon() || o.IsAmmo(); });
			for (auto& [obj, data] : inv) {
				auto& entry = data.second;
				if (!entry || !entry->extraLists) {
					continue;
				}
				for (auto* xList : *entry->extraLists) {
					if (!xList) {
						continue;
					}
					const bool worn = xList->HasType(RE::ExtraDataType::kWorn);
					const bool wornLeft = xList->HasType(RE::ExtraDataType::kWornLeft);
					if (!worn && !wornLeft) {
						continue;
					}
					WornItem w;
					w.base = obj->GetFormID();
					w.left = wornLeft;
					if (auto* u = xList->GetByType<RE::ExtraUniqueID>()) {
						w.uniqueId = u->uniqueID;
					}
					s.worn.push_back(w);
					toStrip.push_back({ obj, xList });
				}
			}

			auto* mgr = RE::ActorEquipManager::GetSingleton();
			for (const auto& p : toStrip) {
				mgr->UnequipObject(a_p, p.obj, p.list, 1, nullptr, false, true, false, true);
			}

			// hands: spells
			s.leftHand = s.rightHand = 0;
			if (auto* f = a_p->GetEquippedObject(true); f && f->Is(RE::FormType::Spell)) {
				s.leftHand = f->GetFormID();
				a_p->DeselectSpell(f->As<RE::SpellItem>());
			}
			if (auto* f = a_p->GetEquippedObject(false); f && f->Is(RE::FormType::Spell)) {
				s.rightHand = f->GetFormID();
				a_p->DeselectSpell(f->As<RE::SpellItem>());
			}
			// voice: shout or power
			auto& rt = a_p->GetActorRuntimeData();
			s.power = rt.selectedPower ? rt.selectedPower->GetFormID() : 0;

			logger::info("gear saved: {} worn piece(s), left hand {:08X}, right hand {:08X}, voice {:08X}", s.worn.size(),
				s.leftHand, s.rightHand, s.power);
		}

		void RestoreGear(RE::PlayerCharacter* a_p)
		{
			auto& s = State::Get();
			auto* mgr = RE::ActorEquipManager::GetSingleton();
			auto* dom = RE::BGSDefaultObjectManager::GetSingleton();
			auto* leftSlot = dom ? dom->GetObject<RE::BGSEquipSlot>(RE::DEFAULT_OBJECTS::kLeftHandEquip) : nullptr;

			auto inv = a_p->GetInventory();
			int restored = 0;
			for (const auto& w : s.worn) {
				auto* form = RE::TESForm::LookupByID<RE::TESBoundObject>(w.base);
				if (!form) {
					continue;
				}
				auto it = inv.find(form);
				if (it == inv.end()) {
					logger::warn("restore: {:08X} is no longer in the inventory", w.base);
					continue;
				}
				RE::ExtraDataList* chosen = nullptr;
				auto&              entry = it->second.second;
				if (entry && entry->extraLists) {
					for (auto* xList : *entry->extraLists) {
						if (!xList) {
							continue;
						}
						auto* u = xList->GetByType<RE::ExtraUniqueID>();
						if (w.uniqueId ? (u && u->uniqueID == w.uniqueId) : !u) {
							chosen = xList;
							break;
						}
					}
				}
				mgr->EquipObject(a_p, form, chosen, 1, (w.left && leftSlot) ? leftSlot : nullptr, false, true, false, true);
				++restored;
			}
			s.worn.clear();

			auto equipSpell = [&](RE::FormID id, const RE::BGSEquipSlot* slot) {
				if (auto* sp = id ? RE::TESForm::LookupByID<RE::SpellItem>(id) : nullptr) {
					mgr->EquipSpell(a_p, sp, slot);
				}
			};
			auto* rightSlot = dom ? dom->GetObject<RE::BGSEquipSlot>(RE::DEFAULT_OBJECTS::kRightHandEquip) : nullptr;
			equipSpell(s.leftHand, leftSlot);
			equipSpell(s.rightHand, rightSlot);
			if (s.power) {
				if (auto* form = RE::TESForm::LookupByID(s.power)) {
					if (auto* shout = form->As<RE::TESShout>()) {
						mgr->EquipShout(a_p, shout);
					} else if (auto* sp = form->As<RE::SpellItem>()) {
						mgr->EquipSpell(a_p, sp);
					}
				}
			}
			s.leftHand = s.rightHand = s.power = 0;
			logger::info("gear restored: {} piece(s)", restored);
		}

		// ---- who saw it ------------------------------------------------------------------------

		// Anyone who could see the player change, and was hostile at that moment, keeps being the
		// player's enemy: they go into Protean_WitnessFaction, which is an enemy of Protean_BeastFaction
		// (the faction the player is in while transformed). Everyone else just meets the animal.
		void CollectWitnesses(RE::PlayerCharacter* a_p)
		{
			auto& s = State::Get();
			auto& reg = Registry::Get();
			s.witnesses.clear();
			s.hostileWitnesses.clear();
			auto* pl = RE::ProcessLists::GetSingleton();
			if (!pl) {
				return;
			}
			const auto here = a_p->GetPosition();
			for (auto& handle : pl->highActorHandles) {
				auto actor = handle.get();
				if (!actor || actor.get() == a_p || actor->IsDead()) {
					continue;
				}
				const float dist = here.GetDistance(actor->GetPosition());
				if (dist > kWitnessRange) {
					continue;
				}
				const auto level = actor->RequestDetectionLevel(a_p);
				const bool hostile = actor->IsHostileToActor(a_p);
				logger::info("  nearby {:08X} '{}' dist {:.0f} detection {} hostile {}", actor->GetFormID(), actor->GetName(), dist, level, hostile);
				if (level > 0) {
					s.witnesses.push_back(actor->GetFormID());
					if (hostile && reg.WitnessFaction()) {
						actor->AddToFaction(reg.WitnessFaction(), 0);
						s.hostileWitnesses.push_back(actor->GetFormID());
					}
				}
			}
			logger::info("witnesses: {} ({} hostile)", s.witnesses.size(), s.hostileWitnesses.size());
		}

		void ReleaseWitnesses()
		{
			auto& s = State::Get();
			auto& reg = Registry::Get();
			for (auto id : s.hostileWitnesses) {
				if (auto* a = RE::TESForm::LookupByID<RE::Actor>(id); a && reg.WitnessFaction()) {
					a->RemoveFromFaction(reg.WitnessFaction());
				}
			}
			s.hostileWitnesses.clear();
		}

		void JoinFactions(RE::PlayerCharacter* a_p, const Creature& a_c)
		{
			auto& s = State::Get();
			s.addedFactions.clear();
			auto join = [&](RE::TESFaction* f) {
				if (f && !a_p->IsInFaction(f)) {
					a_p->AddToFaction(f, 0);
					s.addedFactions.push_back(f->GetFormID());
				}
			};
			for (auto* f : a_c.factions) {
				join(f);
			}
			join(Registry::Get().BeastFaction());
		}

		void LeaveFactions(RE::PlayerCharacter* a_p)
		{
			auto& s = State::Get();
			for (auto id : s.addedFactions) {
				if (auto* f = RE::TESForm::LookupByID<RE::TESFaction>(id)) {
					a_p->RemoveFromFaction(f);
				}
			}
			s.addedFactions.clear();
			ReleaseWitnesses();
		}

		// ---- where the animal fits ---------------------------------------------------------------

		// small / medium: anywhere. outdoor_only (Mammoth, Giant): open air only. large_cave_ok (Troll,
		// Horker): open air, or an interior whose location carries a cave-like keyword (dungeon, mine,
		// crypt, den, lair, ruin...), checked up the chain of parent locations. An interior with no
		// location at all counts as a building.
		bool PlaceAllows(const Creature& a_c, std::string* a_why = nullptr)
		{
			if (a_c.size != "outdoor_only" && a_c.size != "large_cave_ok") {
				return true;
			}
			auto* p = Player();
			auto* cell = p ? p->GetParentCell() : nullptr;
			if (!cell || !cell->IsInteriorCell()) {
				return true;
			}
			if (a_c.size == "large_cave_ok") {
				const auto& kws = Registry::Get().OpenInteriorKeywords();
				for (auto* loc = cell->GetLocation(); loc; loc = loc->parentLoc) {
					for (auto* kw : kws) {
						if (loc->HasKeyword(kw)) {
							return true;
						}
					}
				}
			}
			if (a_why) {
				*a_why = std::format("The {} is too big to be indoors here.", a_c.name);
			}
			return false;
		}

		void LogVitals(const char* a_tag, RE::PlayerCharacter* a_p)
		{
			auto* av = a_p->AsActorValueOwner();
			logger::info("[{}] race '{}' health now {:.1f} base {:.1f} permanent {:.1f}", a_tag, a_p->GetRace() ? a_p->GetRace()->GetName() : "?",
				av->GetActorValue(RE::ActorValue::kHealth), av->GetBaseActorValue(RE::ActorValue::kHealth),
				av->GetPermanentActorValue(RE::ActorValue::kHealth));
		}
	}

	const Creature* ActiveCreature()
	{
		return Active();
	}

	void Notify(const std::string& a_text)
	{
		RE::SendHUDMessage::ShowHUDMessage(a_text.c_str());
	}

	void Learn(const Creature& a_c)
	{
		auto* p = Player();
		auto& s = State::Get();
		if (s.known.contains(a_c.id)) {
			return;  // eating a known heart does nothing for now (bonus feature planned for later)
		}
		s.known.insert(a_c.id);
		p->AddSpell(a_c.power);
		Notify(std::format("You have learned the form of the {}.", a_c.name));
		logger::info("learned '{}'", a_c.id);
	}

	void OnCorpseActivated(RE::Actor* a_corpse)
	{
		if (!a_corpse || !a_corpse->IsDead()) {
			return;
		}
		if (Face::OnCorpseActivated(a_corpse)) {
			return;  // the Loot / Copy appearance choice took over
		}
		auto* p = Player();
		auto& reg = Registry::Get();
		if (!p->HasSpell(reg.Starter())) {
			return;
		}
		const auto* c = reg.ByActor(a_corpse);
		if (!c) {
			return;
		}
		auto& s = State::Get();
		if (s.known.contains(c->id) || s.looted.contains(a_corpse->GetFormID())) {
			return;
		}
		s.looted.insert(a_corpse->GetFormID());
		a_corpse->AddObjectToContainer(c->heart, nullptr, 1, nullptr);
		logger::info("added '{}' to corpse {:08X}", c->heart->GetName(), a_corpse->GetFormID());
	}

	void Transform(const Creature& a_c)
	{
		auto* p = Player();
		auto& s = State::Get();
		if (!s.active.empty()) {
			Notify("You must return to your own form first.");
			return;
		}
		if (std::string why; !PlaceAllows(a_c, &why)) {
			Notify(why);
			logger::info("transform into '{}' refused: {}", a_c.id, why);
			return;
		}
		Face::TakeOff("transforming");  // a stolen face does not carry over into an animal; the banked face is kept
		logger::info("=== transform into '{}' ===", a_c.id);
		LogVitals("before", p);

		const float bonus = std::floor(a_c.HealthAtLevel(p->GetLevel()) * 0.5f);
		s.realRace = p->GetRace() ? p->GetRace()->GetFormID() : 0;
		s.healthBonus = bonus;
		s.active = a_c.id;

		CollectWitnesses(p);
		SaveAndStripGear(p);
		p->SwitchRace(a_c.race, true);
		JoinFactions(p, a_c);
		p->AsActorValueOwner()->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth, bonus);

		// the way back: the revert power in the voice slot
		auto& reg = Registry::Get();
		if (!p->HasSpell(reg.Revert())) {
			p->AddSpell(reg.Revert());
		}
		RE::ActorEquipManager::GetSingleton()->EquipSpell(p, reg.Revert());

		SetControlsLocked(true);
		Attacks::Cancel();
		Camera::Begin(a_c.cameraHeight);
		if (auto* cam = RE::PlayerCamera::GetSingleton()) {
			cam->ForceThirdPerson();
		}

		for (auto id : s.witnesses) {
			if (auto* w = RE::TESForm::LookupByID<RE::Actor>(id)) {
				logger::info("  witness {:08X} '{}' hostile now: {}", id, w->GetName(), w->IsHostileToActor(p));
			}
		}
		LogVitals("after", p);
		logger::info("health bonus {:.0f} (creature {:.0f} at level {})", bonus, a_c.HealthAtLevel(p->GetLevel()), p->GetLevel());
	}

	void Revert(std::string_view a_why)
	{
		auto* p = Player();
		auto& s = State::Get();
		if (s.active.empty()) {
			return;
		}
		logger::info("=== revert ({}) ===", a_why);
		LogVitals("before", p);

		SetControlsLocked(false);
		Camera::End();
		LeaveFactions(p);

		if (auto* race = RE::TESForm::LookupByID<RE::TESRace>(s.realRace)) {
			p->SwitchRace(race, true);
		} else {
			logger::error("real race {:08X} not found; staying in the animal's race", s.realRace);
		}

		auto& reg = Registry::Get();
		p->DeselectSpell(reg.Revert());
		p->RemoveSpell(reg.Revert());

		// Removing the bonus does not kill by itself: the engine leaves health at or below zero without
		// noticing. Check it ourselves, the way damage taken beyond the real maximum must be fatal.
		p->AsActorValueOwner()->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth, -s.healthBonus);
		const float hp = p->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth);

		RestoreGear(p);

		s.active.clear();
		s.healthBonus = 0.0f;
		s.witnesses.clear();
		LogVitals("after", p);

		if (hp <= 0.0f) {
			logger::info("health {:.1f} after reverting: the damage taken exceeds the real maximum, killing the player", hp);
			Notify("The wounds of the beast follow you home.");
			p->KillImpl(nullptr, 0.0f, true, false);  // the normal death (sound, animation, death camera); KillImmediate is silent
		}
	}

	void OnPowerCast(RE::SpellItem* a_spell)
	{
		auto& reg = Registry::Get();
		auto& s = State::Get();
		if (a_spell == reg.Revert()) {
			Revert("revert power");
			return;
		}
		if (a_spell == reg.FacePower()) {
			Face::OnPower();
			return;
		}
		if (a_spell == reg.Starter()) {
			if (s.known.empty()) {
				Notify("You know no forms yet. Eat the heart of an animal.");
			} else {
				std::string names;
				for (const auto& id : s.known) {
					if (const auto* c = reg.ById(id)) {
						names += (names.empty() ? "" : ", ") + c->name;
					}
				}
				Notify("Forms you know: " + names);
			}
			return;
		}
		if (const auto* c = reg.ByPower(a_spell->GetFormID())) {
			if (s.active == c->id) {
				Revert("same power again");
			} else {
				Transform(*c);
			}
		}
	}

	void OnGameLoaded()
	{
		auto& s = State::Get();
		auto* p = Player();
		if (s.active.empty() || !p) {
			return;
		}
		// The save was made while transformed. Race, health and factions are already in the save;
		// the control lock and the camera are not.
		logger::info("loaded a save made as '{}': restoring the control lock", s.active);
		SetControlsLocked(true);
		if (const auto* c = Active()) {
			Camera::Begin(c->cameraHeight);
		}
		if (auto* cam = RE::PlayerCamera::GetSingleton()) {
			cam->ForceThirdPerson();
		}
	}

	void OnFrame()
	{
		static int frames = 0;
		if (++frames % 30 != 0) {
			return;
		}
		const auto* c = Active();
		if (!c) {
			return;
		}
		if (std::string why; !PlaceAllows(*c, &why)) {
			Notify(why + " You return to your own form.");
			SKSE::GetTaskInterface()->AddTask([]() { Revert("too big for this place"); });
		}
	}
}
