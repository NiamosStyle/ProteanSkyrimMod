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

		// ---- the block (the user's rule, 2026-11-05): a blocked hit, arrow or spell from the front does nothing and
		// costs a flat Protean_ShieldBlockCost Stamina; with less Stamina than that the block stops nothing and the
		// hit staggers, as a guard break

		// the hidden perk's "percent blocked" (set, not multiplied): 1 stops all of a blocked hit, 0 none of it
		void SetBlocked(RE::BGSPerk* a_perk, float a_value)
		{
			for (auto* e : a_perk->perkEntries) {
				if (e && e->GetType() == RE::PERK_ENTRY_TYPE::kEntryPoint) {
					auto* entry = static_cast<RE::BGSEntryPointPerkEntry*>(e);
					if (entry->IsEntryPoint(RE::BGSEntryPoint::ENTRY_POINT::kModPercentBlocked) && entry->functionData) {
						if (auto* one = skyrim_cast<RE::BGSEntryPointFunctionDataOneValue*>(entry->functionData)) {
							one->data = a_value;
						}
					}
				}
			}
		}

		float g_lastStamina = -1.0f;  // the player's Stamina last frame: the hit event comes after the game's own block cost
		float g_lastHealth = -1.0f;

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

		class HitSink : public RE::BSTEventSink<RE::TESHitEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event, RE::BSTEventSource<RE::TESHitEvent>*) override
			{
				auto* p = RE::PlayerCharacter::GetSingleton();
				if (!a_event || !p || a_event->target.get() != p || !a_event->flags.any(RE::TESHitEvent::Flag::kHitBlocked) || !Holds(p)) {
					return RE::BSEventNotifyControl::kContinue;
				}
				auto*       av = p->AsActorValueOwner();
				const float cost = Dial("Protean_ShieldBlockCost", 5.0f);
				const float now = Stamina(p);
				const float gameCost = g_lastStamina >= 0.0f ? std::max(0.0f, g_lastStamina - now) : 0.0f;
				const bool  paid = g_lastStamina >= cost;  // what the perk was set to when the hit landed
				if (gameCost > 0.0f) {
					av->RestoreActorValue(RE::ActorValue::kStamina, gameCost);  // the flat cost instead
				}
				auto* who = a_event->cause.get();
				// what the hit did (2026-11-05, user: still hurt while blocking): Health and Stamina against the last frame,
				// and whether the block perk was held and at what share
				{
					auto*       perk = Registry::Get().ShieldBlockPerk();
					float       share = -1.0f;
					if (perk) {
						for (auto* e : perk->perkEntries) {
							if (e && e->GetType() == RE::PERK_ENTRY_TYPE::kEntryPoint) {
								if (auto* one = skyrim_cast<RE::BGSEntryPointFunctionDataOneValue*>(static_cast<RE::BGSEntryPointPerkEntry*>(e)->functionData)) {
									share = one->data;
								}
							}
						}
					}
					const float hp = av->GetActorValue(RE::ActorValue::kHealth);
					logger::info("shield: hit check: health {:.1f} -> {:.1f}, stamina {:.1f} -> {:.1f}, perk held {}, share {:.1f}, blocking {}, source {:08X}",
						g_lastHealth, hp, g_lastStamina, now, perk && p->HasPerk(perk), share, p->IsBlocking(), a_event->source);
				}
				if (paid) {
					av->DamageActorValue(RE::ActorValue::kStamina, cost);
					logger::info("shield: blocked a hit from '{}', stamina -{:.0f} (the game's own {:.1f} given back), now {:.0f}",
						who ? who->GetName() : "?", cost, gameCost, Stamina(p));
				} else {
					p->SetGraphVariableFloat("staggerMagnitude", 0.5f);
					p->NotifyAnimationGraph("staggerStart");
					logger::info("shield: guard broken by '{}': stamina {:.0f} under {:.0f}, the hit went through", who ? who->GetName() : "?",
						g_lastStamina, cost);
				}
				g_lastStamina = Stamina(p);
				return RE::BSEventNotifyControl::kContinue;
			}
		};
		HitSink g_hits;

		// ---- the ward: a hostile spell from the front, while blocking, does nothing (MagicTarget::AddTarget refused)
		std::mutex                                                        g_wardLock;
		std::map<std::pair<RE::FormID, RE::FormID>, std::chrono::steady_clock::time_point> g_warded;  // caster, spell -> when paid

		bool Ward(RE::MagicTarget* a_this, const RE::MagicTarget::AddTargetData& a_data)
		{
			auto* p = RE::PlayerCharacter::GetSingleton();
			if (!p || a_this != static_cast<RE::MagicTarget*>(p) || !a_data.caster || a_data.caster == p || !a_data.effect ||
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
			} else if (!want) {
				if (worn) {
					mgr->UnequipObject(a_p, a_sleeve, nullptr, 1, nullptr, false, true, false, true);
					logger::info("shield: goes back (weapon state {})", static_cast<int>(ws));
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
			const auto mine = *reinterpret_cast<std::uintptr_t*>(static_cast<RE::MagicTarget*>(a_p));
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
				mine - base, reinterpret_cast<std::uintptr_t>(static_cast<RE::MagicTarget*>(a_p)) - reinterpret_cast<std::uintptr_t>(a_p), seen);
		}
	}

	void Install()
	{
		RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESHitEvent>(&g_hits);
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
		// the block perk held while the Shield is worn; it stops all of a blocked hit only while the Stamina can pay
		if (auto* perk = reg.ShieldBlockPerk()) {
			const bool on = Holds(p);
			if (on != p->HasPerk(perk)) {
				if (on) {
					p->AddPerk(perk);
				} else {
					p->RemovePerk(perk);
				}
			}
			SetBlocked(perk, Stamina(p) >= Dial("Protean_ShieldBlockCost", 5.0f) ? 1.0f : 0.0f);
		}
		g_lastStamina = Stamina(p);
		g_lastHealth = p->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth);
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
