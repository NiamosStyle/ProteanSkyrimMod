// Protean - a shapeshifter mod for Skyrim SE
// Copyright (C) 2026 NiamosStyle
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. It is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file or <https://www.gnu.org/licenses/>.

#include "Shield.h"

#include "Blade.h"
#include "Evolve.h"
#include "Registry.h"

namespace protean::Shield
{
	namespace
	{
		constexpr auto kPerk = "shield";
		constexpr auto kFirstName = "ProteanShield1st";
		constexpr auto kFirstModel = "Protean\\Shield\\Sleeve1st_{}.nif";  // M or F
		constexpr auto kSleeveSlot = RE::BGSBipedObjectForm::BipedObjectSlot::kModLegLeft;  // body slot 54

		float Dial(std::string_view a_name, float a_fallback) { return Registry::Get().Dial(a_name, a_fallback); }

		bool Holds(RE::PlayerCharacter* a_p)
		{
			auto* shield = Registry::Get().ShieldArmor();
			return shield && a_p->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kShield) == shield;
		}

		// ---- the block (the user's rule, 2026-11-05): a blocked hit or arrow from the front does nothing and costs a
		// flat Protean_ShieldBlockCost Stamina; with less Stamina than that it lands in full and staggers, a guard break.
		// The game's own "percent blocked" stops at most 70% (fBlockMax) and the Health give-back was not liked
		// (user, 2026-11-05): the hit itself is changed as the game works it out (option 1). Option 3, if this fails: a
		// perk that cancels all incoming damage while blocking (hits from behind too).
		// every hostile effect landing on the player while blocking with the Shield (what still hurt, 2026-11-05)
		class EffectSink : public RE::BSTEventSink<RE::TESMagicEffectApplyEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESMagicEffectApplyEvent* a_event, RE::BSTEventSource<RE::TESMagicEffectApplyEvent>*) override
			{
				auto* p = RE::PlayerCharacter::GetSingleton();
				if (!a_event || !p || a_event->target.get() != p || !p->IsBlocking() || !Holds(p)) {
					return RE::BSEventNotifyControl::kContinue;
				}
				auto* mgef = RE::TESForm::LookupByID<RE::EffectSetting>(a_event->magicEffect);
				if (mgef && mgef->IsHostile()) {
					logger::info("shield: effect '{}' {:08X} landed while blocking, from '{}'", mgef->GetName(), a_event->magicEffect,
						a_event->caster ? a_event->caster->GetName() : "?");
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
		EffectSink g_effects;

		float Stamina(RE::PlayerCharacter* a_p) { return a_p->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina); }

		// degrees between where the player faces and an actor (0 = straight ahead)
		float OffFront(RE::PlayerCharacter* a_p, const RE::Actor* a_other)
		{
			const auto  d = a_other->GetPosition() - a_p->GetPosition();
			const float h = a_p->GetAngleZ();
			const float len = std::hypot(d.x, d.y);
			const float cosA = len > 1.0f ? (d.x * std::sin(h) + d.y * std::cos(h)) / len : 1.0f;
			return std::acos(std::clamp(cosA, -1.0f, 1.0f)) * 57.29578f;
		}

		// One arrow reaches the apply-hit function twice, a few ms apart, on different threads (measured 2026-11-19):
		// the Stamina is paid once. Same aggressor within 0.1 s = the same arrow.
		std::mutex                            g_arrowLock;
		RE::FormID                            g_lastArrowFrom{ 0 };
		std::chrono::steady_clock::time_point g_lastArrowAt{};
		std::atomic<bool>                     g_lastArrowPaid{ false };  // the last arrow was blocked (its second pass is too)

		bool NewArrow(RE::FormID a_from)
		{
			std::lock_guard lock(g_arrowLock);
			const auto      now = std::chrono::steady_clock::now();
			const bool      repeat = a_from == g_lastArrowFrom && now - g_lastArrowAt < std::chrono::milliseconds(100);
			g_lastArrowFrom = a_from;
			g_lastArrowAt = now;
			return !repeat;
		}

		struct ProcessHit
		{
			// the game's call that applies a worked-out hit to its target (as established combat mods hook it)
			static void thunk(RE::Actor* a_target, RE::HitData& a_hit)
			{
				auto* p = RE::PlayerCharacter::GetSingleton();
				if (p && a_target == p) {   // a plain address check first: nothing of the hit is read for anyone else
					Block(p, a_hit, reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
				}
				func(a_target, a_hit);
			}
			static inline REL::Relocation<decltype(thunk)> func;

			// An arrow or bolt from the front while blocking with the Shield: the game never counts it blocked (vanilla
			// blocks arrows by the shield model's collision, and the Shield's model has none), so Protean does, by the
			// spell ward's angle rule (user, 2026-11-19: arrows went through).
			static bool RangedFromFront(RE::PlayerCharacter* a_p, const RE::HitData& a_hit)
			{
				if (!a_hit.weapon || !(a_hit.weapon->IsBow() || a_hit.weapon->IsCrossbow())) {
					return false;
				}
				auto who = a_hit.aggressor.get();
				return who && OffFront(a_p, who.get()) <= Dial("Protean_ShieldWardAngle", 60.0f);
			}

			static void Block(RE::PlayerCharacter* a_p, RE::HitData& a_hit, std::uintptr_t a_from)
			{
				static int seen = 0;
				if (seen < 3) {   // the first few hits on the player, to show the patch reads real hits
					++seen;
					auto who = a_hit.aggressor.get();
					logger::info("shield: hit seen: from '{}', damage {:.1f}, blocked {:.2f}, flags {:X}", who ? who->GetName() : "?",
						a_hit.totalDamage, a_hit.percentBlocked, a_hit.flags.underlying());
				}
				const bool blocking = Holds(a_p) && a_p->IsBlocking();
				if (blocking) {  // every hit while blocking with the Shield: what it was, what the game made of it, and from where
					auto who = a_hit.aggressor.get();
					logger::info("shield: hit while blocking: from '{}' ({:.0f} deg off), weapon '{}', damage {:.1f}, blocked {:.2f}, flags {:X}{}, call +{:X}",
						who ? who->GetName() : "?", who ? OffFront(a_p, who.get()) : -1.0f, a_hit.weapon ? a_hit.weapon->GetName() : "-",
						a_hit.totalDamage, a_hit.percentBlocked, a_hit.flags.underlying(),
						a_hit.flags.any(RE::HitData::Flag::kBlocked) ? " (blocked)" : " (NOT blocked)", a_from - REL::Module::get().base() - 5);
				}
				// a hit Protean already blocked (fully blocked, nothing left: the game's own block stops at fBlockMax, 70%):
				// an arrow's hit comes through here twice, the second time already blocked, and was paid twice (2026-11-19)
				if (a_hit.flags.any(RE::HitData::Flag::kBlocked) && a_hit.percentBlocked >= 1.0f && a_hit.totalDamage <= 0.0f) {
					return;
				}
				const bool ranged = blocking && !a_hit.flags.any(RE::HitData::Flag::kBlocked) && RangedFromFront(a_p, a_hit);
				if (!ranged && (!a_hit.flags.any(RE::HitData::Flag::kBlocked) || !Holds(a_p))) {
					return;
				}
				auto*       av = a_p->AsActorValueOwner();
				const float cost = Dial("Protean_ShieldBlockCost", 5.0f);
				auto        who = a_hit.aggressor.get();
				const std::string name = who ? who->GetName() : "?";
				if (ranged && !NewArrow(who ? who->GetFormID() : 0)) {
					// the same arrow's second pass: blocked again, paid once (a guard break already let the first through)
					if (g_lastArrowPaid) {
						a_hit.totalDamage = 0.0f;
						a_hit.physicalDamage = 0.0f;
						a_hit.percentBlocked = 1.0f;
					}
					return;
				}
				if (ranged) {
					a_hit.flags.set(RE::HitData::Flag::kBlocked);
				}
				g_lastArrowPaid = false;
				if (av->GetActorValue(RE::ActorValue::kStamina) >= cost) {
					g_lastArrowPaid = ranged;
					const float was = a_hit.totalDamage;
					a_hit.totalDamage = 0.0f;       // nothing of it lands
					a_hit.physicalDamage = 0.0f;
					a_hit.percentBlocked = 1.0f;
					av->DamageActorValue(RE::ActorValue::kStamina, cost);
					logger::info("shield: blocked {:.1f} damage from '{}', stamina -{:.0f}", was, name, cost);
				} else {
					a_hit.percentBlocked = 0.0f;    // a guard break: all of it lands, and it staggers
					a_hit.flags.reset(RE::HitData::Flag::kBlocked);
					SKSE::GetTaskInterface()->AddTask([]() {
						if (auto* p = RE::PlayerCharacter::GetSingleton()) {
							p->SetGraphVariableFloat("staggerMagnitude", 0.5f);
							p->NotifyAnimationGraph("staggerStart");
						}
					});
					logger::info("shield: guard broken by '{}' (stamina under {:.0f}): {:.1f} damage lands", name, cost, a_hit.totalDamage);
				}
			}
		};

		// ---- the ward: a hostile spell from the front, while blocking, does nothing (MagicTarget::AddTarget refused)
		std::mutex                                                        g_wardLock;
		std::map<std::pair<RE::FormID, RE::FormID>, std::chrono::steady_clock::time_point> g_warded;  // caster, spell -> when paid

		bool Ward(RE::MagicTarget* a_this, const RE::MagicTarget::AddTargetData& a_data)
		{
			auto* p = RE::PlayerCharacter::GetSingleton();
			if (!p || a_this != p->AsMagicTarget() || !a_data.caster || a_data.caster == p || !a_data.effect ||
				!a_data.effect->baseEffect || !a_data.effect->baseEffect->IsHostile() || !p->IsBlocking() || !Holds(p)) {
				return false;
			}
			// in front: the caster within Protean_ShieldWardAngle of where the player faces
			const auto  d = a_data.caster->GetPosition() - p->GetPosition();
			const float h = p->GetAngleZ();
			const float len = std::hypot(d.x, d.y);
			const float cosA = len > 1.0f ? (d.x * std::sin(h) + d.y * std::cos(h)) / len : 1.0f;
			const float deg = std::acos(std::clamp(cosA, -1.0f, 1.0f)) * 57.29578f;
			if (deg > Dial("Protean_ShieldWardAngle", 60.0f)) {
				return false;
			}
			const float cost = Dial("Protean_ShieldBlockCost", 5.0f);
			const auto  key = std::make_pair(a_data.caster->GetFormID(), a_data.magicItem ? a_data.magicItem->GetFormID() : 0u);
			const auto  now = std::chrono::steady_clock::now();
			std::lock_guard lock(g_wardLock);
			// one spell lands as several effects at once (and a stream every moment): it is paid once each half second
			if (auto it = g_warded.find(key); it != g_warded.end() && now - it->second < std::chrono::milliseconds(500)) {
				return true;
			}
			auto* av = p->AsActorValueOwner();
			if (av->GetActorValue(RE::ActorValue::kStamina) < cost) {
				return false;  // no Stamina: the spell goes through
			}
			av->DamageActorValue(RE::ActorValue::kStamina, cost);
			g_warded[key] = now;
			logger::info("shield: warded '{}' from '{}' ({:.0f} degrees off), stamina -{:.0f}",
				a_data.magicItem ? a_data.magicItem->GetName() : "?", a_data.caster->GetName(), deg, cost);
			return true;
		}

		struct AddTarget
		{
			static bool thunk(RE::MagicTarget* a_this, RE::MagicTarget::AddTargetData& a_data)
			{
				if (Ward(a_this, a_data)) {
					return false;
				}
				return func(a_this, a_data);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		std::int32_t Count(RE::PlayerCharacter* a_p, RE::TESBoundObject* a_item)
		{
			const auto counts = a_p->GetInventoryCounts([a_item](RE::TESBoundObject& a_obj) { return &a_obj == a_item; });
			const auto it = counts.find(a_item);
			return it != counts.end() ? it->second : 0;
		}

		// the flesh and the shield are on the arm only while the shield is worn and fully drawn (as the Blade); never
		// kept in the inventory
		void Sleeve(RE::PlayerCharacter* a_p, RE::TESObjectARMO* a_shield, RE::TESObjectARMO* a_sleeve)
		{
			using WS = RE::WEAPON_STATE;
			const auto* st = a_p->AsActorState();
			const auto  ws = st ? st->GetWeaponState() : WS::kSheathed;
			const bool  held = a_p->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kShield) == a_shield;
			const bool  want = held && ws == WS::kDrawn;
			Blade::HangFirstPerson(a_p, kFirstName, kFirstModel, want, "shield");
			const bool worn = a_p->GetWornArmor(kSleeveSlot) == a_sleeve;
			auto*      mgr = RE::ActorEquipManager::GetSingleton();
			if (want && !worn) {
				if (Count(a_p, a_sleeve) == 0) {
					a_p->AddObjectToContainer(a_sleeve, nullptr, 1, nullptr);
				}
				mgr->EquipObject(a_p, a_sleeve, nullptr, 1, nullptr, false, false, false, true);
				logger::info("shield: grows (weapon state {})", static_cast<int>(ws));
				Blade::SleeveSound(a_p, true, "shield");
			} else if (!want) {
				if (worn) {
					mgr->UnequipObject(a_p, a_sleeve, nullptr, 1, nullptr, false, true, false, true);
					logger::info("shield: goes back (weapon state {})", static_cast<int>(ws));
					Blade::SleeveSound(a_p, false, "shield");
				}
				if (const auto n = Count(a_p, a_sleeve); n > 0) {
					a_p->RemoveItem(a_sleeve, n, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
				}
			}
		}
	}

	namespace
	{
		// the ward: the player's own MagicTarget table, found by matching the player's table pointer (which of the
		// class's tables is MagicTarget's is not assumed). Tried once at Install and once more on the first frame.
		void HookWard(RE::PlayerCharacter* a_p)
		{
			static int tries = 0;
			if (!a_p || tries++ >= 2) {
				return;
			}
			const auto base = REL::Module::get().base();
			const auto mine = *reinterpret_cast<std::uintptr_t*>(a_p->AsMagicTarget());
			std::string seen;
			for (std::size_t i = 0; i < RE::VTABLE_PlayerCharacter.size(); ++i) {
				REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_PlayerCharacter[i] };
				if (vtbl.address() == mine) {
					AddTarget::func = vtbl.write_vfunc(0x1, AddTarget::thunk);
					logger::info("shield: ward hooked (PlayerCharacter table {}: MagicTarget::AddTarget)", i);
					return;
				}
				seen += fmt::format(" {}:{:X}", i, vtbl.address() - base);
			}
			logger::warn("shield: the player's MagicTarget table ({:X}, at +{:X} in the player) not among{}; the ward is not installed",
				mine - base, reinterpret_cast<std::uintptr_t>(a_p->AsMagicTarget()) - reinterpret_cast<std::uintptr_t>(a_p), seen);
		}
	}

	void Install()
	{
		// the hit patch, only where the game's code holds a call (E8) as expected; otherwise nothing is written
		{
			REL::Relocation<std::uintptr_t> fn{ RELOCATION_ID(37673, 38627) };
			const auto site = fn.address() + REL::Relocate(0x3C0, 0x4A8);
			if (*reinterpret_cast<const std::uint8_t*>(site) == 0xE8) {
				// the function that call reaches (it applies a worked-out hit). The melee swing calls it from the site
				// above; arrows and other projectiles call it from elsewhere and went through unblocked (user,
				// 2026-11-19). Every call to it in the game's code gets the same patch.
				const auto base = REL::Module::get().base();
				const auto target = site + 5 + *reinterpret_cast<const std::int32_t*>(site + 1);
				const auto text = REL::Module::get().segment(REL::Segment::textx);
				const auto* b = reinterpret_cast<const std::uint8_t*>(text.address());
				std::vector<std::uintptr_t> sites;
				for (std::size_t i = 0; i + 5 <= text.size(); ++i) {
					if (b[i] == 0xE8) {
						const auto at = text.address() + i;
						if (at + 5 + *reinterpret_cast<const std::int32_t*>(at + 1) == target) {
							sites.push_back(at);
						}
					}
				}
				if (std::ranges::find(sites, site) == sites.end()) {
					sites.push_back(site);
				}
				SKSE::AllocTrampoline(14 * sites.size());
				std::string list;
				for (auto at : sites) {
					const auto was = SKSE::GetTrampoline().write_call<5>(at, ProcessHit::thunk);
					if (!ProcessHit::func.address()) {
						ProcessHit::func = was;
					}
					list += std::format(" +{:X}{}", at - base, at == site ? " (melee)" : "");
				}
				logger::info("shield: hit patch on {} call(s) of +{:X}:{}", sites.size(), target - base, list);
			} else {
				logger::warn("shield: no call at +{:X} (byte {:02X}); the hit patch is not written",
					site - REL::Module::get().base(), *reinterpret_cast<const std::uint8_t*>(site));
			}
		}
		RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESMagicEffectApplyEvent>(&g_effects);
		HookWard(RE::PlayerCharacter::GetSingleton());
	}

	void OnFrame(float a_delta)
	{
		auto*       p = RE::PlayerCharacter::GetSingleton();
		const auto& reg = Registry::Get();
		auto*       shield = reg.ShieldArmor();
		auto*       sleeve = reg.ShieldSleeve();
		if (!p || !shield || !sleeve) {
			return;
		}
		Sleeve(p, shield, sleeve);
		if (!AddTarget::func.address()) {
			HookWard(p);  // the player may not have been made yet when Install ran
		}
		// the perk keeps the Shield in the inventory
		static float since = 0.0f;
		if ((since += a_delta) >= 2.0f) {
			since = 0.0f;
			if (Evolve::Has(kPerk) && Count(p, shield) == 0) {
				p->AddObjectToContainer(shield, nullptr, 1, nullptr);
				logger::info("shield: the perk gives the Shield");
			}
		}
	}
}
