#include "Call.h"

#include "Apex.h"
#include "Registry.h"
#include "Shift.h"

namespace protean::Call
{
	namespace
	{
		// Each animal's noise: the vanilla sound descriptor played (found in the game's sound records, 2026-11-14).
		// A few animals have no call of their own in the game; they borrow the nearest kin's, to be judged by ear.
		const std::unordered_map<std::string, const char*> kNoise = {
			{ "wolf", "NPCWolfHowl" },           { "ice_wolf", "NPCWolfHowl" },        { "pit_wolf", "NPCWolfHowl" },
			{ "deer", "NPCElkAggroWarning" },    { "elk", "NPCElkAggroWarning" },      { "fox", "NPCWolfBark" },
			{ "hare", "NPCHareInjured" },        { "dog", "NPCDogBarkSD" },            { "goat", "NPCGoatBleat" },
			{ "cow", "NPCCowMoo" },              { "bear", "NPCBearAggroWarning01" },  { "cave_bear", "NPCBearAggroWarning01" },
			{ "snow_bear", "NPCBearAggroWarning02" }, { "sabre_cat", "NPCSabreCatAggroWarning01" },
			{ "snowy_sabre_cat", "NPCSabreCatAggroWarning02" }, { "vale_sabre_cat", "NPCSabreCatAggroWarning01" },
			{ "death_hound", "NPCDogDeathhoundBark" }, { "boar", "NPCRieklingBristlebackAttackStanding" },
			{ "skeever", "NPCSkeeverAttackSD" }, { "chicken", "NPCChickenCluck" },     { "ash_hopper", "NPCChaurusWarningCall" },
			{ "mudcrab", "NPCMudcrabIdle" },     { "chaurus", "NPCChaurusWarningCall" }, { "troll", "NPCTrollCombatIdleRoarSD" },
			{ "frost_troll", "NPCTrollCombatIdleRoarSD" }, { "horse", "NPCHorseRearUp" }, { "horker", "NPCHorkerAggroWarning" },
			{ "mammoth", "NPCMammothAggroWarning01" }, { "giant", "NPCGiantAttackVocal" }, { "werewolf", "NPCWerewolfShout" },
			{ "werebear", "NPCWerewolfShout" }, { "slaughterfish", "NPCSlaughterfishMainIdle" },
			{ "chaurus_hunter", "NPCChaurusWarningCall" }, { "chaurus_hunter_fledgling", "NPCChaurusWarningCall" },
			{ "frostbite_spider", "NPCSpiderFrostbiteAttackASD" },
		};

		int Count(const Creature& a_c)
		{
			int n = 3;
			if (a_c.size == "small") {
				n = 5;
			} else if (a_c.size == "large_cave_ok") {
				n = 2;
			} else if (a_c.size == "outdoor_only") {
				n = 1;
			}
			return std::max(1, static_cast<int>(std::lround(n * Registry::Get().Dial("Protean_CallCountMult", 1.0f))));
		}

		float g_cooldown{ 0.0f };
		std::unordered_map<std::string, RE::SpellItem*> g_spells;  // per kind, made when first needed

		RE::SpellItem* SpellFor(const Creature& a_c, int a_count, float a_seconds)
		{
			auto& slot = g_spells[a_c.id];
			if (slot && static_cast<int>(slot->effects.size()) == a_count) {
				for (auto* e : slot->effects) {
					e->effectItem.duration = static_cast<std::uint32_t>(a_seconds);
				}
				return slot;
			}
			auto* mgefFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::EffectSetting>();
			auto* spellFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::SpellItem>();
			if (!mgefFactory || !spellFactory || !a_c.npc) {
				return nullptr;
			}
			auto* mgef = mgefFactory->Create();
			auto* spell = spellFactory->Create();
			if (!mgef || !spell) {
				return nullptr;
			}
			mgef->magicItemDescription = "Summons a pack of the animal's kind.";
			mgef->data.archetype = RE::EffectArchetypes::ArchetypeID::kSummonCreature;
			mgef->data.associatedForm = a_c.npc;
			mgef->data.castingType = RE::MagicSystem::CastingType::kFireAndForget;
			mgef->data.delivery = RE::MagicSystem::Delivery::kSelf;
			mgef->data.baseCost = 0.0f;
			mgef->data.flags.set(RE::EffectSetting::EffectSettingData::Flag::kNoArea);

			spell->fullName = "Call";
			spell->data.spellType = RE::MagicSystem::SpellType::kSpell;
			spell->data.castingType = RE::MagicSystem::CastingType::kFireAndForget;
			spell->data.delivery = RE::MagicSystem::Delivery::kSelf;
			spell->data.costOverride = 0;
			spell->data.flags.set(RE::SpellItem::SpellFlag::kCostOverride);
			for (int i = 0; i < a_count; ++i) {
				auto* eff = new RE::Effect();
				eff->baseEffect = mgef;
				eff->effectItem.magnitude = 0.0f;
				eff->effectItem.area = 0;
				eff->effectItem.duration = static_cast<std::uint32_t>(a_seconds);
				eff->cost = 0.0f;
				spell->effects.push_back(eff);
			}
			slot = spell;
			logger::info("call: made the summon for '{}' ({} x {:08X}, {:.0f} s)", a_c.id, a_count, a_c.npc->GetFormID(), a_seconds);
			return slot;
		}

		void Noise(RE::PlayerCharacter* a_p, const Creature& a_c)
		{
			const auto it = kNoise.find(a_c.id);
			if (it == kNoise.end()) {
				return;
			}
			RE::BSSoundHandle h;
			if (auto* audio = RE::BSAudioManager::GetSingleton()) {
				audio->GetSoundHandleByName(h, it->second, 0x1A);
			}
			if (h.IsValid()) {
				h.SetPosition(a_p->GetPosition());
				h.Play();
			} else {
				logger::warn("call: no sound '{}' for '{}'", it->second, a_c.id);
			}
		}
	}

	void Reset() { g_cooldown = 0.0f; }

	void OnFrame(float a_delta)
	{
		if (g_cooldown > 0.0f) {
			g_cooldown = std::max(0.0f, g_cooldown - a_delta);
		}
	}

	void OnCast()
	{
		auto* p = RE::PlayerCharacter::GetSingleton();
		const auto* c = Shift::ActiveCreature();
		if (!p || !c || !Apex::Owned()) {
			return;
		}
		auto& reg = Registry::Get();
		if (g_cooldown > 0.0f) {
			Shift::Notify(std::format("Your call needs {:.0f} more seconds before it can be heard again.", g_cooldown));
			return;
		}
		auto*       av = p->AsActorValueOwner();
		const float cost = reg.Dial("Protean_ChangeStaminaCost", 15.0f);
		if (av->GetActorValue(RE::ActorValue::kStamina) < cost) {
			Shift::Notify("You are too winded to call.");
			return;
		}
		const int   count = Count(*c);
		const float seconds = reg.Dial("Protean_CallSeconds", 60.0f);
		auto*       spell = SpellFor(*c, count, seconds);
		if (!spell) {
			logger::error("call: no summon could be made for '{}'", c->id);
			return;
		}
		av->DamageActorValue(RE::ActorValue::kStamina, cost);
		g_cooldown = reg.Dial("Protean_CallCooldown", 180.0f);
		Noise(p, *c);
		if (auto* caster = p->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)) {
			caster->CastSpellImmediate(spell, false, p, 1.0f, false, 0.0f, p);
		}
		logger::info("call: '{}' called {} for {:.0f} s", c->id, count, seconds);
	}
}
