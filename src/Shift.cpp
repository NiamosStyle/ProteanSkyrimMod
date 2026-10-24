#include "Shift.h"

#include "Attacks.h"
#include "Camera.h"
#include "Face.h"
#include "Fly.h"
#include "Move.h"
#include "State.h"
#include "Evolve.h"

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
		// group we never lock, so the game menu (save, load, quit) opens and closes while transformed. The Favorites
		// key goes with it: an animal's Favorites menu offers only its own powers (see Keen).
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
						if (m.eventID == "Pause" || m.eventID == "Cancel" || m.eventID == "Favorites") {
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
				// a_forceEquip is the "prevent removal" lock (Papyrus EquipItem's abPreventRemoval): leave it off, or the
				// player cannot take the gear off again (seen 2026-10-22: footwraps stuck after a revert)
				mgr->EquipObject(a_p, form, chosen, 1, (w.left && leftSlot) ? leftSlot : nullptr, false, false, false, true);
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

	namespace
	{
		// ---- integration (docs/INTEGRATION.md) ---------------------------------------------------------
		// Another mod learns what happened from SKSE mod events (Papyrus: RegisterForModEvent), sees what the
		// player is from the form's hidden ability (keywords) and Protean_ActiveForm, and turns Protean's dials.

		void SendModEvent(const char* a_name, const std::string& a_form, float a_number)
		{
			SKSE::ModCallbackEvent ev{ a_name, RE::BSFixedString(a_form), a_number, Player() };
			if (auto* src = SKSE::GetModCallbackEventSource()) {
				src->SendEvent(&ev);
			}
		}

		void PutOnMarkers(RE::PlayerCharacter* a_p, const Creature& a_c)
		{
			if (a_c.ability && !a_p->HasSpell(a_c.ability)) {
				a_p->AddSpell(a_c.ability);
			}
			Registry::Get().SetGlobal("Protean_ActiveForm", static_cast<float>(a_c.index + 1));
		}

		// armour and damage dials, as the player changes: what was added is kept in the co-save and taken off
		// on changing back, so a dial changed meanwhile cannot leave anything behind
		void AddDialStats(RE::PlayerCharacter* a_p)
		{
			auto&       reg = Registry::Get();
			auto&       s = State::Get();
			auto*       av = a_p->AsActorValueOwner();
			s.armorBonus = reg.Dial("Protean_ArmorBonus", 0.0f);
			s.damageBonus = reg.Dial("Protean_DamageMult", 1.0f) - 1.0f;
			av->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kDamageResist, s.armorBonus);
			av->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kAttackDamageMult, s.damageBonus);
		}

		void TakeOffMarkers(RE::PlayerCharacter* a_p)
		{
			auto& s = State::Get();
			for (const auto& c : Registry::Get().All()) {
				if (c.ability && a_p->HasSpell(c.ability)) {
					a_p->RemoveSpell(c.ability);
				}
			}
			Registry::Get().SetGlobal("Protean_ActiveForm", 0.0f);
			auto* av = a_p->AsActorValueOwner();
			av->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kDamageResist, -s.armorBonus);
			av->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kAttackDamageMult, -s.damageBonus);
			s.armorBonus = 0.0f;
			s.damageBonus = 0.0f;
		}
	}

	namespace
	{
		// Health keeps its share of the maximum across a change, both ways (never below 1%, so changing back
		// cannot kill). Maximum = permanent value + temporary modifier; the damage taken sits on top of it.
		float MaxHealth(RE::PlayerCharacter* a_p)
		{
			return a_p->AsActorValueOwner()->GetPermanentActorValue(RE::ActorValue::kHealth) +
			       a_p->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth);
		}

		float HealthShare(RE::PlayerCharacter* a_p)
		{
			const float max = MaxHealth(a_p);
			return max > 0.0f ? std::clamp(a_p->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth) / max, 0.0f, 1.0f) : 1.0f;
		}

		void SetHealth(RE::PlayerCharacter* a_p, float a_value)
		{
			auto*       av = a_p->AsActorValueOwner();
			const float delta = a_value - av->GetActorValue(RE::ActorValue::kHealth);
			if (delta > 0.0f) {
				av->RestoreActorValue(RE::ActorValue::kHealth, delta);
			} else if (delta < 0.0f) {
				av->DamageActorValue(RE::ActorValue::kHealth, -delta);
			}
		}

		void SetHealthShare(RE::PlayerCharacter* a_p, float a_share)
		{
			if (a_p->IsDead() || a_share <= 0.0f) {
				return;  // a forced change at death: leave the dying alone
			}
			SetHealth(a_p, std::max(a_share, 0.01f) * MaxHealth(a_p));
		}
	}

	void Learn(const Creature& a_c)
	{
		auto* p = Player();
		auto& s = State::Get();
		if (s.known.contains(a_c.id)) {
			Evolve::Feed(a_c);  // a known form again: counts towards the milestones
			return;
		}
		s.known.insert(a_c.id);
		p->AddSpell(a_c.power);
		Notify(std::format("You have learned the form of the {}.", a_c.name));
		logger::info("learned '{}'", a_c.id);
		SendModEvent("Protean_Learned", a_c.id, static_cast<float>(a_c.index + 1));
		Evolve::Feed(a_c);
	}

	void OnCorpseActivated(RE::Actor* a_corpse)
	{
		if (!a_corpse || !a_corpse->IsDead()) {
			return;
		}
		if (Face::OnCorpseActivated(a_corpse)) {
			return;  // the Loot / Copy appearance choice took over
		}
		AddHeart(a_corpse);
	}

	// at the death, so the heart is there the first time the body is opened (added on opening, the loot window was
	// already filled and showed it only the second time); opening still adds it to a body that died out of sight
	void AddHeart(RE::Actor* a_corpse)
	{
		if (!a_corpse || !a_corpse->IsDead()) {
			return;
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
		// a form already known keeps dropping its heart once Unbound Form is bought: repeat feedings are worth points
		if ((s.known.contains(c->id) && !Evolve::Has("unbound_form")) || s.looted.contains(a_corpse->GetFormID())) {
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

		const float share = HealthShare(p);
		const float own = MaxHealth(p);
		const float animal = a_c.HealthAtLevel(p->GetLevel());
		auto&       reg0 = Registry::Get();
		const float bonus = std::floor(own * reg0.Dial("Protean_HealthBonusPct", 0.25f) +
		                               std::max(animal - own, 0.0f) * reg0.Dial("Protean_HealthGapShare", 0.25f));
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
		PutOnMarkers(p, a_c);
		AddDialStats(p);
		if (a_c.swims) {
			// the fish drowned underwater without it (measured); the same value an Argonian or a potion raises
			p->AsActorValueOwner()->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kWaterBreathing, 1.0f);
		}
		SetHealthShare(p, share);

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
		logger::info("health bonus {:.0f} (own {:.0f}, creature {:.0f} at level {}); kept {:.1f}% of maximum", bonus, own, animal, p->GetLevel(),
			share * 100.0f);
		SendModEvent("Protean_Transformed", a_c.id, static_cast<float>(a_c.index + 1));
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
		const float share = HealthShare(p);
		if (!p->IsDead() && share > 0.0f) {
			// Losing the maximum drops current health by the same amount, and below zero the engine kills (seen in a
			// fight, 2026-10-22). So set health first to what it will be after the loss: the final share plus the loss.
			auto*       av = p->AsActorValueOwner();
			const float raceExtra = std::max(av->GetPermanentActorValue(RE::ActorValue::kHealth) - av->GetBaseActorValue(RE::ActorValue::kHealth), 0.0f);
			const float loss = s.healthBonus + raceExtra;
			SetHealth(p, std::max(share, 0.01f) * (MaxHealth(p) - loss) + loss);
		}

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

		p->AsActorValueOwner()->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth, -s.healthBonus);
		if (const auto* c = Active(); c && c->swims) {
			p->AsActorValueOwner()->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kWaterBreathing, -1.0f);
		}
		SetHealthShare(p, share);

		RestoreGear(p);
		TakeOffMarkers(p);

		const std::string was = s.active;
		const float       wasNumber = Active() ? static_cast<float>(Active()->index + 1) : 0.0f;
		s.active.clear();
		s.healthBonus = 0.0f;
		s.witnesses.clear();
		s.crime.clear();
		LogVitals("after", p);
		SendModEvent("Protean_Reverted", was, wasNumber);
	}

	namespace
	{
		// The player's own wish to change back while flying is refused once, since it means a fall (user decision);
		// casting again within kConfirmFor goes ahead. (Forced reverts - death, loading, dev - never ask.)
		constexpr auto kConfirmFor = std::chrono::seconds(10);
		std::chrono::steady_clock::time_point g_warnedAt{};
		std::chrono::steady_clock::time_point g_ignoreCastsUntil{};  // see OnPowerKey

		bool ConfirmRevert()
		{
			const auto now = std::chrono::steady_clock::now();
			if (!Fly::Flying() || now - g_warnedAt < kConfirmFor) {
				g_warnedAt = {};
				return true;
			}
			g_warnedAt = now;
			logger::info("revert refused once: flying");
			Notify("You are flying: changing back now means a fall. Cast again to change back anyway.");
			return false;
		}

		void BeginRevertChange(const char* a_why);  // below, with the change effects
	}

	// The power itself is cast again only ~3 s after the warning (measured: the second Z press waited for it),
	// so a press of the key inside the warning's window confirms at once.
	void OnPowerKey()
	{
		if (State::Get().active.empty() || g_warnedAt == std::chrono::steady_clock::time_point{} ||
			std::chrono::steady_clock::now() - g_warnedAt >= kConfirmFor) {
			return;
		}
		g_warnedAt = {};
		BeginRevertChange("confirmed with the power key");
		// that press still casts, ~a second later, whatever power is in the slot by then: the animal's own,
		// put back by the revert, which changed the player straight back (seen). Ignore it.
		g_ignoreCastsUntil = std::chrono::steady_clock::now() + std::chrono::seconds(4);
	}

	namespace
	{
		// A change the player asks for (user decisions, each previewed live): the Become Ethereal shout sound with
		// a skin shimmer (WerewolfTrans02FXS, the "B" effect), the Vampire Lord's transformation sound a second
		// in, and at kChangeDelay the Vampire Lord's blood burst as the body switches; the burst hides the swap.
		// Forced changes (death, loading, too big, dev) stay instant.
		constexpr auto       kStartSound = "VOCShoutFXBecomeEtherealIn2D";        // Skyrim.esm SNDR (editor ID)
		constexpr auto       kChangeSound = "NPCVampireLordTransformation022D";  // Dawnguard SNDR (editor ID)
		constexpr RE::FormID kBurst = 0x015373;                                  // DLC1VampChangeExplosion (Dawnguard.esm)
		constexpr RE::FormID kChangeShader = 0x0EBECD;                          // WerewolfTrans02FXS (Skyrim.esm)
		constexpr float      kShaderSeconds = 4.0f;
		constexpr auto       kSoundDelay = std::chrono::milliseconds(1000);
		constexpr auto       kChangeDelay = std::chrono::milliseconds(1500);

		struct Pending
		{
			const Creature*                       into{ nullptr };  // null with revert set: changing back
			bool                                  revert{ false };
			std::string                           why;
			std::chrono::steady_clock::time_point at{};
			std::chrono::steady_clock::time_point soundAt{};
			bool                                  soundPlayed{ false };
		};
		std::optional<Pending> g_pending;

		// At the player's spot, not following the body: the switch replaces the body, which cut the sound off.
		void PlayChangeSound(RE::PlayerCharacter* a_p, const char* a_sound)
		{
			RE::BSSoundHandle h;
			if (auto* audio = RE::BSAudioManager::GetSingleton()) {
				audio->GetSoundHandleByName(h, a_sound, 0x1A);
			}
			if (h.IsValid()) {
				h.SetPosition(a_p->GetPosition());
				h.Play();
			} else {
				logger::warn("change: no sound '{}'", a_sound);
			}
		}

		void PlayBurst(RE::PlayerCharacter* a_p)
		{
			auto* dh = RE::TESDataHandler::GetSingleton();
			if (auto* burst = dh ? dh->LookupForm<RE::BGSExplosion>(kBurst, "Dawnguard.esm") : nullptr) {
				a_p->PlaceObjectAtMe(burst, false);
			}
		}

		void PlayChangeShader(RE::PlayerCharacter* a_p)
		{
			if (auto* fx = RE::TESForm::LookupByID<RE::TESEffectShader>(kChangeShader)) {
				a_p->ApplyEffectShader(fx, kShaderSeconds);
			}
		}

		void BeginChange(Pending a_change)
		{
			if (g_pending) {
				return;  // one change at a time
			}
			PlayChangeShader(Player());
			PlayChangeSound(Player(), kStartSound);
			const auto now = std::chrono::steady_clock::now();
			a_change.at = now + kChangeDelay;
			a_change.soundAt = now + kSoundDelay;
			g_pending = std::move(a_change);
		}

		void FinishChange()
		{
			if (!g_pending) {
				return;
			}
			const auto now = std::chrono::steady_clock::now();
			if (!g_pending->soundPlayed && now >= g_pending->soundAt) {
				g_pending->soundPlayed = true;
				if (auto* p = Player()) {
					PlayChangeSound(p, kChangeSound);
				}
			}
			if (now < g_pending->at) {
				return;
			}
			const auto change = std::move(*g_pending);
			g_pending.reset();
			auto* p = Player();
			if (!p || p->IsDead()) {
				return;
			}
			PlayBurst(p);
			if (change.revert) {
				Revert(change.why);
			} else if (change.into) {
				Transform(*change.into);
			}
		}

		void BeginRevertChange(const char* a_why)
		{
			BeginChange({ nullptr, true, a_why });
		}
	}

	void OnPowerCast(RE::SpellItem* a_spell)
	{
		auto& reg = Registry::Get();
		auto& s = State::Get();
		if (std::chrono::steady_clock::now() < g_ignoreCastsUntil) {
			g_ignoreCastsUntil = {};
			logger::info("power {:08X} ignored: the key press that already changed the player back", a_spell->GetFormID());
			return;
		}
		if (a_spell == reg.Revert()) {
			if (ConfirmRevert()) {
				BeginChange({ nullptr, true, "revert power" });
			}
			return;
		}
		if (a_spell == reg.FacePower()) {
			Face::OnPower();
			return;
		}
		if (a_spell == reg.Evolution()) {
			Evolve::OpenMenu();
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
					BeginChange({ nullptr, true, "same power again" });
				}
			} else if (!s.active.empty() || !PlaceAllows(*c)) {
				Transform(*c);  // refused: it says why, with no effects first
			} else {
				// changing form costs stamina (a dial); forced changes are free
				const float cost = reg.Dial("Protean_ChangeStaminaCost", 15.0f);
				auto*       av = Player()->AsActorValueOwner();
				if (cost > 0.0f && av->GetActorValue(RE::ActorValue::kStamina) < cost) {
					Notify("You are too tired to change form.");
					logger::info("change into '{}' refused: stamina {:.0f} < {:.0f}", c->id, av->GetActorValue(RE::ActorValue::kStamina), cost);
					return;
				}
				if (cost > 0.0f) {
					av->DamageActorValue(RE::ActorValue::kStamina, cost);
				}
				BeginChange({ c, false, {} });
			}
		}
	}

	void OnGameLoaded()
	{
		Evolve::OnGameLoaded();
		auto& s = State::Get();
		auto* p = Player();
		if (!p) {
			return;
		}
		Move::ClearSprint();  // a save made mid-sprint holds the sprint's speed; the co-save says how much
		// Every form learned has its power (seen 2026-10-21: a save without powers the player had been given;
		// the co-save's list of learned forms is the record that counts).
		for (const auto& id : s.known) {
			if (const auto* c = Registry::Get().ById(id); c && !p->HasSpell(c->power)) {
				p->AddSpell(c->power);
				logger::info("load: power for '{}' was missing; added back", id);
			}
		}
		if (s.active.empty()) {
			TakeOffMarkers(p);  // nothing of a form may linger in a human (a save from before the markers, a crash)
			return;
		}
		if (const auto* c = Active()) {
			PutOnMarkers(p, *c);  // a save from before the markers existed has none on
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
		g_pending.reset();  // a change still on its way belongs to the game being left
		// hand the player's own skin and height back, so a save loaded next does not inherit the animal's
		if (auto* p = Player()) {
			TakeOffBody(p);
		}
	}

	void OnFrame()
	{
		FinishChange();  // every frame: the change lands on time
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
		// The giant's graph picks its walk by iState, which the AI sets for wild ones: 0 (its default) gave a
		// broken split-legged walk or none, 1 its combat walk (measured live). Only 0 is corrected: the game sets 2,
		// its combat run, while the player runs, and forcing that back to 1 made the legs split (measured: 57 runs
		// put back in one fight).
		if (c->stanceState > 0 && stanceFor == c) {
			auto* p = Player();
			if (std::int32_t now = 0; p->GetGraphVariableInt("iState", now) && now == 0) {
				p->SetGraphVariableInt("iState", c->stanceState);
				logger::info("'{}' graph iState {} -> {}", c->id, now, c->stanceState);
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
	}
}
