#include "Skills.h"

#include "Registry.h"
#include "Shift.h"
#include "Stalker.h"
#include "State.h"

namespace protean::Skills
{
	namespace
	{
		// the game's own perks, Skyrim.esm (ranks lowest first)
		constexpr std::pair<RE::FormID, float> kArmsman[] = { { 0x0BABE4, 1.2f }, { 0x079343, 1.4f }, { 0x079342, 1.6f },
			{ 0x079344, 1.8f }, { 0x079345, 2.0f } };
		constexpr std::pair<RE::FormID, float> kAgile[] = { { 0x0BE123, 1.2f }, { 0x079376, 1.4f }, { 0x079389, 1.6f },
			{ 0x079391, 1.8f }, { 0x079392, 2.0f } };
		constexpr std::pair<RE::FormID, float> kStealth[] = { { 0x0BE126, 0.20f }, { 0x0C07C6, 0.25f }, { 0x0C07C7, 0.30f },
			{ 0x0C07C8, 0.35f }, { 0x0C07C9, 0.40f } };
		constexpr RE::FormID kFightingStance = 0x052D50;  // power attacks cost 25% less stamina
		constexpr RE::FormID kSavageStrike = 0x03AF81;    // standing power attacks +25%
		constexpr RE::FormID kDeftMovement = 0x107831;    // 10% chance to take no damage from a melee hit
		constexpr RE::FormID kSilence = 0x105F24;         // walking and running do not affect detection
		constexpr float      kDeftChance = 0.10f;
		constexpr float      kSneakRange = 2000.0f;  // game units: someone this near and unaware counts, as in vanilla

		float Dial(std::string_view a_name, float a_fallback) { return Registry::Get().Dial(a_name, a_fallback); }

		bool Owns(RE::PlayerCharacter* a_p, RE::FormID a_id)
		{
			auto* perk = RE::TESForm::LookupByID<RE::BGSPerk>(a_id);
			return perk && a_p->HasPerk(perk);
		}

		template <std::size_t N>
		float Best(RE::PlayerCharacter* a_p, const std::pair<RE::FormID, float> (&a_ranks)[N], float a_none)
		{
			float v = a_none;
			for (const auto& [id, value] : a_ranks) {
				if (Owns(a_p, id)) {
					v = value;
				}
			}
			return v;
		}

		// a hidden perk's entry value (its one OneValue datum), live data
		void SetEntry(RE::BGSPerk* a_perk, std::size_t a_index, float a_value)
		{
			if (!a_perk || a_index >= a_perk->perkEntries.size()) {
				return;
			}
			if (auto* entry = static_cast<RE::BGSEntryPointPerkEntry*>(a_perk->perkEntries[static_cast<std::uint32_t>(a_index)]); entry && entry->functionData) {
				if (auto* one = skyrim_cast<RE::BGSEntryPointFunctionDataOneValue*>(entry->functionData)) {
					one->data = a_value;
				}
			}
		}

		void Hold(RE::PlayerCharacter* a_p, RE::BGSPerk* a_perk, bool a_on)
		{
			if (!a_perk || a_p->HasPerk(a_perk) == a_on) {
				return;
			}
			if (a_on) {
				a_p->AddPerk(a_perk);
			} else {
				a_p->RemovePerk(a_perk);
			}
		}

		bool CarapaceWorks() { return State::Get().carapaceApplied > 0.0f; }

		bool WearsLightBody(RE::PlayerCharacter* a_p)
		{
			auto* body = a_p->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kBody);
			return body && body->IsLightArmor();
		}

		// a hit counts for Light Armor in animal form, or with the carapace on and no light body armor (that gives its own)
		bool LightArmorHit(RE::PlayerCharacter* a_p)
		{
			return Shift::ActiveCreature() || (CarapaceWorks() && !WearsLightBody(a_p));
		}

		void Deft()
		{
			// the chance is drawn ahead of each hit: the perk's multiplier for the next weapon hit is 0 (no damage) one time
			// in ten (vanilla's perk asks GetRandomPercent at the hit; a hidden perk with no conditions cannot)
			static std::mt19937 rng{ std::random_device{}() };
			SetEntry(Registry::Get().DeftPerk(), 0, std::uniform_real_distribution<float>(0.0f, 1.0f)(rng) < kDeftChance ? 0.0f : 1.0f);
		}

		float g_armsMult = 1.0f;  // Armsman's multiplier, the base of each animal attack's

		void AddXp(RE::ActorValue a_skill, float a_uses, std::string_view a_why)
		{
			auto* p = RE::PlayerCharacter::GetSingleton();
			if (p && a_uses > 0.0f) {
				p->AddSkillExperience(a_skill, a_uses);
				logger::info("skills: {} +{:.2f} ({})", a_skill == RE::ActorValue::kOneHanded ? "One-Handed" :
				                                        a_skill == RE::ActorValue::kLightArmor ? "Light Armor" :
				                                                                                 "Sneak",
					a_uses, a_why);
			}
		}

		float g_lastHealth = 0.0f;  // the player's health last frame: the hit event comes after the damage

		// Vanilla's rates, measured on the player 2026-10-25: a weapon hit pays its listed damage in skill uses (an
		// Imperial Sword, 8: 60.5 One-Handed XP, whatever it dealt); a hit on light armor that hurts pays about 0.38 uses
		// whatever the damage, one that hurts nothing pays nothing; sneaking unseen pays about 0.62 uses a second, only
		// while moving.
		class Sink : public RE::BSTEventSink<RE::TESHitEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event, RE::BSTEventSource<RE::TESHitEvent>*) override
			{
				if (!a_event || !a_event->cause || !a_event->target) {
					return RE::BSEventNotifyControl::kContinue;
				}
				auto* target = a_event->target->As<RE::Actor>();
				if (!target) {
					return RE::BSEventNotifyControl::kContinue;
				}
				if (a_event->cause->IsPlayerRef() && !target->IsPlayerRef()) {
					auto*      claws = Registry::Get().ClawWeapon();
					const bool clawHit = claws && a_event->source == claws->GetFormID();
					if (!clawHit && !Shift::ActiveCreature()) {
						return RE::BSEventNotifyControl::kContinue;
					}
					// the listed damage: the claws' (it grows with the level), or the animal's own bite before Protean's boosts
					auto*       p = RE::PlayerCharacter::GetSingleton();
					const float listed = clawHit ? static_cast<float>(claws->GetAttackDamage()) :
					                               (p && p->GetRace() ? p->GetRace()->data.unarmedDamage : 0.0f);
					SKSE::GetTaskInterface()->AddTask([listed, clawHit] {
						AddXp(RE::ActorValue::kOneHanded, listed * Dial("Protean_XpOneHanded", 1.0f), clawHit ? "claws" : "bite");
					});
				} else if (target->IsPlayerRef() && !a_event->cause->IsPlayerRef()) {
					const float before = g_lastHealth;
					SKSE::GetTaskInterface()->AddTask([before] {
						auto* p = RE::PlayerCharacter::GetSingleton();
						if (!p) {
							return;
						}
						if (LightArmorHit(p) && p->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth) < before - 0.01f) {
							AddXp(RE::ActorValue::kLightArmor, Dial("Protean_XpLightArmor", 0.38f),
								Shift::ActiveCreature() ? "hit as an animal" : "hit with the carapace");
						}
						Deft();  // the next hit's chance
					});
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		void SyncPerks(RE::PlayerCharacter* a_p)
		{
			auto&      reg = Registry::Get();
			const bool animal = Shift::ActiveCreature() != nullptr;
			g_armsMult = Best(a_p, kArmsman, 1.0f);
			if (auto* arms = reg.AnimalArmsPerk()) {
				SetEntry(arms, 1, Owns(a_p, kFightingStance) ? 0.75f : 1.0f);
				Hold(a_p, arms, animal);
			}
			Hold(a_p, reg.DeftPerk(), (animal || CarapaceWorks()) && Owns(a_p, kDeftMovement));
		}
	}

	void OnFrame(RE::PlayerCharacter* a_p, float a_delta)
	{
		g_lastHealth = a_p->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth);
		static float tick = 0.0f;
		if ((tick -= a_delta) <= 0.0f) {
			tick = 0.25f;
			SyncPerks(a_p);
		}
		// Sneak: moving while faded, near someone who has not noticed the player
		auto* pc = RE::PlayerControls::GetSingleton();
		if (!Stalker::Hidden() || !pc || pc->data.moveInputVec.Length() <= 0.1f) {
			return;
		}
		static float sneakTick = 0.0f;
		if ((sneakTick += a_delta) < 1.0f) {
			return;
		}
		const float seconds = sneakTick;
		sneakTick = 0.0f;
		auto* pl = RE::ProcessLists::GetSingleton();
		if (!pl) {
			return;
		}
		for (auto& h : pl->highActorHandles) {
			auto a = h.get();
			if (a && a.get() != a_p && !a->IsDead() && !a->IsPlayerTeammate() &&
				a_p->GetPosition().GetDistance(a->GetPosition()) <= kSneakRange && a->RequestDetectionLevel(a_p) <= 0) {
				AddXp(RE::ActorValue::kSneak, Dial("Protean_XpSneak", 0.62f) * seconds, std::format("unseen by '{}'", a->GetName()));
				return;
			}
		}
	}

	void OnAnimalAttack(bool a_power)
	{
		auto* p = RE::PlayerCharacter::GetSingleton();
		auto* pc = RE::PlayerControls::GetSingleton();
		if (!p) {
			return;
		}
		const bool standing = !pc || pc->data.moveInputVec.Length() <= 0.1f;
		const bool savage = a_power && standing && Owns(p, kSavageStrike);
		SetEntry(Registry::Get().AnimalArmsPerk(), 0, g_armsMult * (savage ? 1.25f : 1.0f));
	}

	float AgileDefender(RE::PlayerCharacter* a_p) { return Best(a_p, kAgile, 1.0f); }

	float StealthShare(RE::PlayerCharacter* a_p) { return Owns(a_p, kSilence) ? 1.0f : Best(a_p, kStealth, 0.0f); }

	void Install()
	{
		static Sink sink;
		RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESHitEvent>(&sink);
	}

	void OnGameLoaded()
	{
		if (auto* p = RE::PlayerCharacter::GetSingleton()) {
			SyncPerks(p);
			Deft();
		}
	}
}
