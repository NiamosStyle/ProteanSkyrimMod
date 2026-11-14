#include "Call.h"

#include "Apex.h"
#include "Registry.h"
#include "Shift.h"

#include <random>

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

		// The pack: animals placed beside the player, made followers, taken away when their time is up. (A Summon
		// Creature effect built at runtime ran its countdown but spawned nothing, 2026-11-14: the game sets the
		// effect's behaviour up only when it loads an effect from a plugin.)
		struct Member
		{
			RE::FormID id;
			float      left;
		};
		std::vector<Member> g_pack;
		float               g_scan{ 0.0f };

		void SetTeammate(RE::Actor* a_actor, bool a_on)
		{
			auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			if (!vm) {
				return;
			}
			const auto handle = vm->GetObjectHandlePolicy()->GetHandleForObject(a_actor->GetFormType(), a_actor);
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> cb;
			vm->DispatchMethodCall2(handle, "Actor", "SetPlayerTeammate", RE::MakeFunctionArguments(std::move(a_on), std::move(a_on)), cb);
		}

		void Dismiss(RE::FormID a_id)
		{
			if (auto* a = RE::TESForm::LookupByID<RE::Actor>(a_id)) {
				SetTeammate(a, false);
				if (auto* f = Registry::Get().BeastFaction()) {
					a->RemoveFromFaction(f);
				}
				if (a->IsDead()) {
					return;  // a fallen helper stays as the body it is
				}
				a->StopCombat();
				a->Disable();
				a->SetDelete(true);
				logger::info("call: {:08X} '{}' goes", a_id, a->GetName());
			}
		}

		void Place(RE::PlayerCharacter* a_p, const Creature& a_c, int a_count, float a_seconds)
		{
			static std::mt19937 rng{ std::random_device{}() };
			std::uniform_real_distribution<float> jitter(-0.12f, 0.12f), dist(260.0f, 400.0f);
			// out of the player's view: the camera's heading (in animal form the camera and the body can point apart),
			// else the body's; the animals stand in the arc 125-235 degrees from it, spread across it
			float heading = a_p->GetAngleZ();
			if (auto* cam = RE::PlayerCamera::GetSingleton(); cam && cam->cameraRoot) {
				const auto fwd = cam->cameraRoot->world.rotate * RE::NiPoint3(0.0f, 1.0f, 0.0f);
				if (std::abs(fwd.x) + std::abs(fwd.y) > 0.01f) {
					heading = std::atan2(fwd.x, fwd.y);
				}
			}
			constexpr float kFrom = 125.0f * 3.14159265f / 180.0f, kSpan = 110.0f * 3.14159265f / 180.0f;
			for (int i = 0; i < a_count; ++i) {
				auto ref = a_p->PlaceObjectAtMe(a_c.npc, false);
				auto* actor = ref ? ref->As<RE::Actor>() : nullptr;
				if (!actor) {
					logger::warn("call: could not place a '{}'", a_c.id);
					continue;
				}
				// heading is clockwise from north (+Y): a direction at angle h has x = sin h, y = cos h
				const float h = heading + kFrom + kSpan * (static_cast<float>(i) + 0.5f) / static_cast<float>(a_count) + jitter(rng);
				const float d = dist(rng);
				auto        pos = a_p->GetPosition();
				actor->SetPosition(RE::NiPoint3{ pos.x + std::sin(h) * d, pos.y + std::cos(h) * d, pos.z }, true);
				if (auto* f = Registry::Get().BeastFaction()) {
					actor->AddToFaction(f, 0);
				}
				SetTeammate(actor, true);
				g_pack.push_back({ actor->GetFormID(), a_seconds });
				logger::info("call: placed {:08X} '{}'", actor->GetFormID(), actor->GetName());
			}
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
		for (auto it = g_pack.begin(); it != g_pack.end();) {
			it->left -= a_delta;
			if (it->left <= 0.0f) {
				Dismiss(it->id);
				it = g_pack.erase(it);
			} else {
				++it;
			}
		}
		// helpers left from before a save and load (the list is not saved): anyone of the pack's faction that is a
		// teammate and not in the list gets ten more seconds
		if ((g_scan -= a_delta) <= 0.0f) {
			g_scan = 5.0f;
			auto* pl = RE::ProcessLists::GetSingleton();
			auto* beast = Registry::Get().BeastFaction();
			auto* p = RE::PlayerCharacter::GetSingleton();
			if (pl && beast && p) {
				for (auto& h : pl->highActorHandles) {
					auto a = h.get();
					if (a && a.get() != p && a->IsInFaction(beast) && a->IsPlayerTeammate() && !a->IsDead() &&
						std::ranges::none_of(g_pack, [&](const Member& m) { return m.id == a->GetFormID(); })) {
						g_pack.push_back({ a->GetFormID(), 10.0f });
						logger::info("call: {:08X} '{}' is left from before; goes in 10 s", a->GetFormID(), a->GetName());
					}
				}
			}
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
		if (!c->npc) {
			logger::error("call: '{}' has no NPC record to place", c->id);
			return;
		}
		av->DamageActorValue(RE::ActorValue::kStamina, cost);
		g_cooldown = reg.Dial("Protean_CallCooldown", 180.0f);
		Noise(p, *c);
		Place(p, *c, count, seconds);
		logger::info("call: '{}' called {} for {:.0f} s", c->id, count, seconds);
	}
}
