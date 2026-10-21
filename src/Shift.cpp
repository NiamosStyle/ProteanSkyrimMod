#include "Shift.h"

#include "Attacks.h"
#include "Camera.h"
#include "Face.h"
#include "Move.h"
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

		// The Esc key ("Pause") belongs to the same control group as the inventory and magic menus. Move it into a
		// group we never lock, so the game menu (save, load, quit) opens and closes while transformed.
		void KeepPauseMenu(bool a_locked)
		{
			auto* cm = RE::ControlMap::GetSingleton();
			if (!cm) {
				return;
			}
			using Ctx = RE::UserEvents::INPUT_CONTEXT_ID;
			for (auto id : { Ctx::kGameplay, Ctx::kMenuMode }) {
				auto* ctx = cm->controlMap[id];
				if (!ctx) {
					continue;
				}
				for (auto& list : ctx->deviceMappings) {
					for (auto& m : list) {
						if (m.eventID == "Pause" || m.eventID == "Cancel") {
							if (a_locked && m.userEventGroupFlag.all(Flag::kMenu)) {
								m.userEventGroupFlag = Flag::kVATS;
							} else if (!a_locked && m.userEventGroupFlag.all(Flag::kVATS)) {
								m.userEventGroupFlag = Flag::kMenu;
							}
						}
					}
				}
			}
		}

		// The fighting controls are never taken: taking them also stopped the giant walking and the power key (Z)
		// working (measured). The game's own attack is kept off the giant in Attacks instead. Unlocking still turns
		// them on, for a save made while an older build had them off.
		void SetControlsLocked(bool a_locked)
		{
			if (auto* cm = RE::ControlMap::GetSingleton()) {
				KeepPauseMenu(a_locked);
				cm->ToggleControls(kLocked, !a_locked, false);
				if (!a_locked) {
					cm->ToggleControls(Flag::kFighting, true, false);
				}
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

		// ---- crime committed as the animal --------------------------------------------------------------

		void SnapshotCrime()
		{
			auto& s = State::Get();
			s.crime.clear();
			for (auto* f : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESFaction>()) {
				if (f && f->TracksCrimes()) {
					const auto v = f->GetCrimeGoldViolent(), nv = f->GetCrimeGoldNonViolent();
					if (v || nv) {
						s.crime.push_back({ f->GetFormID(), v, nv });
					}
				}
			}
		}

		// Nobody saw the person change into the animal, so what the animal did is not held against the person:
		// bounties gained since the change are removed and anyone fighting the player stands down.
		void ForgiveAnimalCrimes(RE::PlayerCharacter* a_p, bool a_forgive)
		{
			auto& s = State::Get();
			int   forgiven = 0;
			for (auto* f : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESFaction>()) {
				if (!f || !f->TracksCrimes()) {
					continue;
				}
				std::int32_t sv = 0, snv = 0;
				for (const auto& c : s.crime) {
					if (c.faction == f->GetFormID()) {
						sv = c.violent;
						snv = c.nonViolent;
					}
				}
				const auto v = f->GetCrimeGoldViolent(), nv = f->GetCrimeGoldNonViolent();
				if (v > sv) {
					if (a_forgive) {
						f->ModCrimeGold(-(v - sv), true);
					}
					forgiven += v - sv;
				}
				if (nv > snv) {
					if (a_forgive) {
						f->ModCrimeGold(-(nv - snv), false);
					}
					forgiven += nv - snv;
				}
			}
			int calmed = 0;
			if (!a_forgive) {
				// seen changing back: whoever saw it holds the animal's crimes against the player
				if (forgiven > 0) {
					if (auto* pl = RE::ProcessLists::GetSingleton()) {
						for (auto& handle : pl->highActorHandles) {
							auto actor = handle.get();
							if (actor && actor.get() != a_p && !actor->IsDead() && !actor->IsPlayerTeammate() &&
								a_p->GetPosition().GetDistance(actor->GetPosition()) <= kWitnessRange && actor->RequestDetectionLevel(a_p) > 0) {
								actor->StartCombat(a_p);
							}
						}
					}
					logger::info("seen changing back: {} gold of bounty stays, witnesses turn on the player", forgiven);
				}
				return;
			}
			if (auto* pl = RE::ProcessLists::GetSingleton()) {
				for (auto& handle : pl->highActorHandles) {
					auto actor = handle.get();
					if (!actor || actor.get() == a_p || actor->IsDead()) {
						continue;
					}
					auto target = actor->GetActorRuntimeData().currentCombatTarget.get();
					if (target && target.get() == a_p) {
						actor->StopCombat();
						actor->StopAlarmOnActor();
						++calmed;
					}
				}
			}
			s.crime.clear();
			logger::info("nobody saw the change: forgave {} gold of bounty, calmed {} actor(s)", forgiven, calmed);
			if (forgiven || calmed) {
				Notify("Nobody connects you with what the beast did.");
			}
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

		// Variants that share a race (Ice Wolf, Pit Wolf) differ only by the skin and size on their own NPC record.
		// The player's record gets the animal's while transformed (memory only); the player's own values are kept
		// in the co-save, because a save made while transformed may store the animal's.
		void WearBody(RE::PlayerCharacter* a_p, const Creature& a_c)
		{
			auto* base = a_p->GetActorBase();
			auto& s = State::Get();
			if (!base) {
				return;
			}
			if (s.bodyHeight <= 0.0f) {
				s.bodySkin = base->skin ? base->skin->GetFormID() : 0;
				s.bodyHeight = base->height;
			}
			base->skin = a_c.skin;
			base->height = a_c.height;
			logger::info("body: skin {:08X} height {:.2f} (own skin {:08X} height {:.2f})", a_c.skin ? a_c.skin->GetFormID() : 0,
				a_c.height, s.bodySkin, s.bodyHeight);
		}

		void TakeOffBody(RE::PlayerCharacter* a_p)
		{
			auto* base = a_p->GetActorBase();
			auto& s = State::Get();
			if (!base || s.bodyHeight <= 0.0f) {
				return;
			}
			base->skin = s.bodySkin ? RE::TESForm::LookupByID<RE::TESObjectARMO>(s.bodySkin) : nullptr;
			base->height = s.bodyHeight;
			s.bodySkin = 0;
			s.bodyHeight = 0.0f;
		}

		// A race switch rebuilds the body standing up, but the game's sneak flag survives it: the player then walks at
		// sneak speed upright and counts as sneaking (seen after changing back from the dog: the face offer took the
		// loot window). Drop the flag so it matches the new body.
		void StandUp(RE::PlayerCharacter* a_p)
		{
			if (auto* st = a_p->AsActorState(); st && st->IsSneaking()) {
				st->actorState1.sneaking = 0;
				a_p->NotifyAnimationGraph("SneakStop");
				logger::info("sneak flag cleared after the race switch");
			}
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
		// a water animal starts in water; once changed it may flop onto land, where the game suffocates it
		// (measured; kept by user decision)
		if (auto* st = p->AsActorState(); a_c.swims && !(st && st->IsSwimming())) {
			Notify(std::format("You must be swimming to become the {}.", a_c.name));
			logger::info("transform into '{}' refused: not swimming", a_c.id);
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
		SnapshotCrime();
		SaveAndStripGear(p);
		WearBody(p, a_c);  // before the race switch, which rebuilds the model
		p->SwitchRace(a_c.race, true);
		StandUp(p);
		JoinFactions(p, a_c);
		p->AsActorValueOwner()->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth, bonus);
		if (a_c.swims) {
			// the fish drowned underwater without it (measured); the same value an Argonian or a potion raises
			p->AsActorValueOwner()->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kWaterBreathing, 1.0f);
		}

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
		Move::Reset();
		Camera::End();
		LeaveFactions(p);
		{
			// who can see the change back? (the earlier witnesses of the change into the animal also count)
			// a person who saw the change into the animal knows; animals that saw it do not count
			bool seen = std::ranges::any_of(s.witnesses, [](RE::FormID id) {
				auto* a = RE::TESForm::LookupByID<RE::Actor>(id);
				auto* r = a ? a->GetRace() : nullptr;
				return r && r->data.flags.all(RE::RACE_DATA::Flag::kFaceGenHead);
			});
			if (auto* pl = RE::ProcessLists::GetSingleton(); pl && !seen) {
				for (auto& handle : pl->highActorHandles) {
					auto actor = handle.get();
					auto* race = actor ? actor->GetRace() : nullptr;  // only people count: a horse or dog watching is not a witness
					if (actor && actor.get() != p && !actor->IsDead() && !actor->IsPlayerTeammate() && race &&
						race->data.flags.all(RE::RACE_DATA::Flag::kFaceGenHead) &&
						p->GetPosition().GetDistance(actor->GetPosition()) <= kWitnessRange && actor->RequestDetectionLevel(p) > 0) {
						logger::info("  {:08X} '{}' sees the change back", actor->GetFormID(), actor->GetName());
						seen = true;
						break;
					}
				}
			}
			ForgiveAnimalCrimes(p, !seen);
		}

		TakeOffBody(p);
		if (auto* race = RE::TESForm::LookupByID<RE::TESRace>(s.realRace)) {
			p->SwitchRace(race, true);
			StandUp(p);
		} else {
			logger::error("real race {:08X} not found; staying in the animal's race", s.realRace);
		}

		auto& reg = Registry::Get();
		p->DeselectSpell(reg.Revert());
		p->RemoveSpell(reg.Revert());

		// Removing the bonus does not kill by itself: the engine leaves health at or below zero without
		// noticing. Check it ourselves, the way damage taken beyond the real maximum must be fatal.
		p->AsActorValueOwner()->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth, -s.healthBonus);
		if (const auto* c = Active(); c && c->swims) {
			p->AsActorValueOwner()->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kWaterBreathing, -1.0f);
		}
		const float hp = p->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth);

		RestoreGear(p);

		s.active.clear();
		s.healthBonus = 0.0f;
		s.witnesses.clear();
		s.crime.clear();
		LogVitals("after", p);

		if (hp <= 0.0f) {
			logger::info("health {:.1f} after reverting: the damage taken exceeds the real maximum, killing the player", hp);
			Notify("The wounds of the beast follow you home.");
			p->KillImpl(nullptr, 0.0f, true, false);  // the normal death (sound, animation, death camera); KillImmediate is silent
		}
	}

	namespace
	{
		// The player's own wish to change back is refused once when it would kill them; casting again within
		// kConfirmFor goes ahead. (Forced reverts - death, loading, dev - never ask.)
		constexpr auto kConfirmFor = std::chrono::seconds(10);
		std::chrono::steady_clock::time_point g_warnedAt{};

		bool ConfirmRevert()
		{
			const float hp = Player()->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth) - RevertKillsBelow();
			const auto  now = std::chrono::steady_clock::now();
			if (hp > 0.0f || now - g_warnedAt < kConfirmFor) {
				g_warnedAt = {};
				return true;
			}
			g_warnedAt = now;
			logger::info("revert refused once: health would be {:.1f}", hp);
			Notify("Your wounds are too deep to survive changing back. Cast again to change back anyway.");
			return false;
		}
	}

	namespace
	{
		// An animal cannot drink potions or cast heals, so out of combat its health tops up at kRegenShare of the
		// maximum per second (the game's own regeneration counts towards it) until changing back is safe again.
		// Healing directly, rather than raising HealRate, leaves nothing behind in a save.
		constexpr float kRegenShare = 0.02f;  // a person: 0.7%; magicka 3%, stamina 5%

		void Regenerate()
		{
			using Clock = std::chrono::steady_clock;
			static Clock::time_point last{};
			const auto now = Clock::now();
			const float dt = std::min(std::chrono::duration<float>(now - last).count(), 1.0f);  // a pause is not healing time
			last = now;

			auto* p = Player();
			if (!p || p->IsInCombat() || p->IsDead()) {
				return;
			}
			auto*       av = p->AsActorValueOwner();
			const float hp = av->GetActorValue(RE::ActorValue::kHealth);
			const float line = RevertKillsBelow();
			if (hp > line) {
				return;
			}
			const float max = av->GetPermanentActorValue(RE::ActorValue::kHealth) +
			                  p->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth);
			const float own = std::max(av->GetActorValue(RE::ActorValue::kHealRate), 0.0f) / 100.0f;  // percent per second
			const float extra = std::max(kRegenShare - own, 0.0f) * max * dt;
			if (extra > 0.0f) {
				av->RestoreActorValue(RE::ActorValue::kHealth,std::min(extra, line + 1.0f - hp));
			}
		}
	}

	float RevertKillsBelow()
	{
		auto*       p = Player();
		const auto& s = State::Get();
		if (s.active.empty() || !p) {
			return 0.0f;
		}
		// changing back takes away the bonus and whatever the animal's race adds to the maximum (the werewolf's
		// +50); any other permanent bonus is counted as lost too, which can only warn early
		auto*       av = p->AsActorValueOwner();
		const float raceExtra = av->GetPermanentActorValue(RE::ActorValue::kHealth) - av->GetBaseActorValue(RE::ActorValue::kHealth);
		return s.healthBonus + std::max(raceExtra, 0.0f);
	}

	void OnPowerCast(RE::SpellItem* a_spell)
	{
		auto& reg = Registry::Get();
		auto& s = State::Get();
		if (a_spell == reg.Revert()) {
			if (ConfirmRevert()) {
				Revert("revert power");
			}
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
				if (ConfirmRevert()) {
					Revert("same power again");
				}
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
		logger::info("loaded a save made as '{}': restoring the control lock and the body", s.active);
		SetControlsLocked(true);
		if (auto* cm = RE::ControlMap::GetSingleton()) {
			cm->ToggleControls(Flag::kFighting, true, false);  // a save from a build that took them
		}
		if (const auto* c = Active()) {
			WearBody(p, *c);
			SKSE::GetTaskInterface()->AddTask([]() {
				if (auto* pl = RE::PlayerCharacter::GetSingleton(); pl && pl->Is3DLoaded()) {
					pl->DoReset3D(true);
				}
			});
			Camera::Begin(c->cameraHeight);
		}
		if (auto* cam = RE::PlayerCamera::GetSingleton()) {
			cam->ForceThirdPerson();
		}
	}

	void BeforeLoad()
	{
		// hand the player's own skin and height back, so a save loaded next does not inherit the animal's
		if (auto* p = Player()) {
			TakeOffBody(p);
		}
	}

	void OnFrame()
	{
		static int frames = 0;
		if (++frames % 30 != 0) {
			return;
		}
		const auto* c = Active();
		static bool        deathLogged = false;
		static const void* stanceFor = nullptr;  // the form whose fighting stance has been entered
		static int         settle = 0;
		if (!c) {
			deathLogged = false;
			stanceFor = nullptr;
			settle = 0;
			return;
		}
		// The giant walks only in its fighting stance with its (empty) hands readied, as the AI keeps wild ones.
		// Measured: the stance alone, sent as the body loads, did not stick; readying, resetting the graph to its
		// default state and then the stance did. Done once the body has settled (every 30 frames here).
		if (!c->stance.empty() && stanceFor != c) {
			auto* p = Player();
			auto* st = p->AsActorState();
			if (++settle < 3) {
				// let the new body load
			} else if (st && !st->IsWeaponDrawn()) {
				p->DrawWeaponMagicHands(true);
			} else if (p->NotifyAnimationGraph("IdleForceDefaultState") && p->NotifyAnimationGraph(c->stance)) {
				stanceFor = c;
				settle = 0;
				logger::info("'{}' in its fighting stance ({})", c->id, c->stance);
			}
		}
		if (auto* p = Player(); p && p->IsDead()) {
			if (!deathLogged) {
				deathLogged = true;
				logger::info("player died as '{}' at z {:.1f}, knock state {}, in midair {}", c->id, p->GetPositionZ(),
					std::to_underlying(p->AsActorState()->GetKnockState()), p->IsInMidair());
			}
			return;
		}
		if (std::string why; !PlaceAllows(*c, &why)) {
			Notify(why + " You return to your own form.");
			SKSE::GetTaskInterface()->AddTask([]() { Revert("too big for this place"); });
		}
		Regenerate();
	}
}
