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
			float      followIn{ -1.0f };  // seconds until the follow order is given again; negative = never
			float      ox{ 0.0f }, oy{ 0.0f };  // where it keeps station beside the player
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

		// Following, the way vanilla's Howl of the Pack does it: its "Pack Member" wolf (HowlSummonWolf, 0CF79E) has one AI
		// package, HowlSummonedWolfFollowPlayer (10ACD1: follow the player, 300-500 units). The placed animals are given
		// that same package. (Papyrus KeepOffsetFromActor and SetPlayerTeammate, sent through the script machine to an
		// animal with no script of its own, did nothing: 2026-11-14.)
		RE::TESPackage* FollowPackage()
		{
			static RE::TESPackage* pkg = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESPackage>(0x10ACD1, "Skyrim.esm");
			return pkg;
		}

		RE::TESPackage* CurrentPackage(RE::Actor* a_actor)
		{
			auto* proc = a_actor->GetActorRuntimeData().currentProcess;
			return proc ? proc->currentPackage.package : nullptr;
		}

		void Follow(RE::Actor* a_actor, RE::Actor*, float, float)
		{
			auto* pkg = FollowPackage();
			if (!pkg || a_actor->IsInCombat() || CurrentPackage(a_actor) == pkg) {
				return;  // in a fight the combat AI leads; afterwards the package is given again
			}
			a_actor->PutCreatedPackage(pkg, true, true, false);
		}

		void StopFollowing(RE::Actor*) {}  // they are disabled a moment later, which ends the package

		// Taking a helper away is done in two steps, never a delete: the first call-time (2026-11-14) deleted the
		// animals mid-fight and the game crashed (an enemy's combat code still held them). Step 1: stop being a
		// helper and have everyone drop them as a target. Step 2, a moment later: disable (they fade out).
		std::vector<Member> g_leaving;

		void Dismiss(RE::FormID a_id)
		{
			auto* a = RE::TESForm::LookupByID<RE::Actor>(a_id);
			if (!a) {
				return;
			}
			SetTeammate(a, false);
			StopFollowing(a);
			if (auto* f = Registry::Get().BeastFaction()) {
				a->RemoveFromFaction(f);
			}
			if (a->IsDead()) {
				return;  // a fallen helper stays as the body it is
			}
			a->StopCombat();
			if (auto* pl = RE::ProcessLists::GetSingleton()) {
				pl->StopCombatAndAlarmOnActor(a, false);
			}
			g_leaving.push_back({ a_id, 1.0f });
		}

		void FinishLeaving(float a_delta)
		{
			for (auto it = g_leaving.begin(); it != g_leaving.end();) {
				it->left -= a_delta;
				if (it->left > 0.0f) {
					++it;
					continue;
				}
				if (auto* a = RE::TESForm::LookupByID<RE::Actor>(it->id); a && !a->IsDead()) {
					a->StopCombat();
					a->Disable();
					logger::info("call: {:08X} '{}' goes", it->id, a->GetName());
				}
				it = g_leaving.erase(it);
			}
		}

		// ---- where the pack may stand ----------------------------------------------------------
		// A spot is good when: the player has a clear line to it (no wall between, so it is the same room or cave, not
		// the next tunnel over or rock), there is floor under it within a step or two of the player's level (no pit, no
		// ledge far below), there is headroom, and it is not on top of another pack member. Collision comes from the
		// game's own line-of-sight rays.

		// fraction of the way from a_from to a_to a ray travels before it hits the world (1 = nothing in the way)
		float Ray(RE::bhkWorld* a_world, const RE::NiPoint3& a_from, const RE::NiPoint3& a_to)
		{
			auto* filter = RE::bhkCollisionFilter::GetSingleton();
			if (!a_world || !filter) {
				return 1.0f;
			}
			const float     scale = RE::bhkWorld::GetWorldScale();
			RE::bhkPickData pick;
			pick.rayInput.from = RE::hkVector4(a_from.x * scale, a_from.y * scale, a_from.z * scale, 0.0f);
			pick.rayInput.to = RE::hkVector4(a_to.x * scale, a_to.y * scale, a_to.z * scale, 0.0f);
			pick.rayInput.filterInfo.filter = (filter->GetNewSystemGroup() << 16) | static_cast<std::uint32_t>(RE::COL_LAYER::kLOS);
			a_world->PickObject(pick);
			return pick.rayOutput.HasHit() ? pick.rayOutput.hitFraction : 1.0f;
		}

		bool GoodSpot(RE::bhkWorld* a_world, const RE::NiPoint3& a_player, float a_x, float a_y, const std::vector<RE::NiPoint3>& a_taken,
			RE::NiPoint3& a_out)
		{
			// a clear line at chest height from the player
			if (Ray(a_world, { a_player.x, a_player.y, a_player.z + 70.0f }, { a_x, a_y, a_player.z + 70.0f }) < 1.0f) {
				return false;
			}
			// floor below: found within a step up or two down
			const float top = a_player.z + 120.0f, reach = 320.0f;
			const float f = Ray(a_world, { a_x, a_y, top }, { a_x, a_y, top - reach });
			if (f >= 1.0f) {
				return false;  // nothing underneath: a drop
			}
			const float floorZ = top - reach * f;
			if (floorZ > a_player.z + 90.0f || floorZ < a_player.z - 140.0f) {
				return false;  // on a ledge above, or a pit below
			}
			// headroom for an animal (about a man's height)
			if (Ray(a_world, { a_x, a_y, floorZ + 8.0f }, { a_x, a_y, floorZ + 170.0f }) < 1.0f) {
				return false;
			}
			for (const auto& t : a_taken) {
				if ((t.x - a_x) * (t.x - a_x) + (t.y - a_y) * (t.y - a_y) < 90.0f * 90.0f) {
					return false;
				}
			}
			a_out = { a_x, a_y, floorZ + 6.0f };
			return true;
		}

		void Place(RE::PlayerCharacter* a_p, const Creature& a_c, int a_count, float a_seconds)
		{
			static std::mt19937 rng{ std::random_device{}() };
			// out of the player's view: the camera's heading (in animal form the camera and the body can point apart),
			// else the body's. Preferred: the arc 125-235 degrees from it. If nothing there is open (a tunnel, a
			// cave wall), the arc widens to 95-265, then to everything but the 60 degrees straight ahead, and last to
			// anywhere close by; the distance shrinks within each arc.
			float heading = a_p->GetAngleZ();
			if (auto* cam = RE::PlayerCamera::GetSingleton(); cam && cam->cameraRoot) {
				const auto fwd = cam->cameraRoot->world.rotate * RE::NiPoint3(0.0f, 1.0f, 0.0f);
				if (std::abs(fwd.x) + std::abs(fwd.y) > 0.01f) {
					heading = std::atan2(fwd.x, fwd.y);
				}
			}
			constexpr float kRad = 3.14159265f / 180.0f;
			struct Arc
			{
				float from, to;
				const char* name;
			};
			const Arc arcs[] = { { 125.0f, 235.0f, "behind" }, { 95.0f, 265.0f, "behind and sides" }, { 30.0f, 330.0f, "all but straight ahead" },
				{ 0.0f, 360.0f, "anywhere" } };
			const float ranges[] = { 400.0f, 320.0f, 250.0f, 190.0f, 140.0f, 100.0f };

			auto*       cell = a_p->GetParentCell();
			auto*       world = cell ? cell->GetbhkWorld() : nullptr;
			const auto  here = a_p->GetPosition();
			std::vector<RE::NiPoint3> taken;
			int         fallbacks = 0;

			for (int i = 0; i < a_count; ++i) {
				RE::NiPoint3 spot = here;
				const char*  tier = nullptr;
				for (const auto& arc : arcs) {
					for (float range : ranges) {
						// the angles of the arc, tried in a random order
						std::vector<float> angles;
						for (float deg = arc.from; deg <= arc.to; deg += 15.0f) {
							angles.push_back(deg);
						}
						std::ranges::shuffle(angles, rng);
						for (float deg : angles) {
							const float h = heading + deg * kRad;
							if (GoodSpot(world, here, here.x + std::sin(h) * range, here.y + std::cos(h) * range, taken, spot)) {
								tier = arc.name;
								break;
							}
						}
						if (tier) {
							break;
						}
					}
					if (tier) {
						break;
					}
				}
				if (!tier) {
					// nowhere open at all (a closet): stand on the player's own spot; the game pushes them clear
					++fallbacks;
					spot = here;
				}
				auto  ref = a_p->PlaceObjectAtMe(a_c.npc, false);
				auto* actor = ref ? ref->As<RE::Actor>() : nullptr;
				if (!actor) {
					logger::warn("call: could not place a '{}'", a_c.id);
					continue;
				}
				actor->SetPosition(spot, true);
				taken.push_back(spot);
				if (auto* f = Registry::Get().BeastFaction()) {
					actor->AddToFaction(f, 0);
				}
				SetTeammate(actor, true);
				// the follow order is given a moment later: a placed animal has no AI running yet and drops it
				g_pack.push_back({ actor->GetFormID(), a_seconds, 1.5f, (static_cast<float>(i) - (static_cast<float>(a_count) - 1.0f) / 2.0f) * 130.0f, -170.0f });
				logger::info("call: placed {:08X} '{}' {} ({:.0f} away)", actor->GetFormID(), actor->GetName(), tier ? tier : "ON THE PLAYER (no open spot)",
					std::sqrt((spot.x - here.x) * (spot.x - here.x) + (spot.y - here.y) * (spot.y - here.y)));
			}
			if (fallbacks > 0) {
				logger::warn("call: {} of {} had no open spot nearby", fallbacks, a_count);
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

	void Reset() { g_cooldown = 0.0f; }  // a load: the cooldown is not saved, so a save that never used Call must not inherit one

	void OnFrame(float a_delta)
	{
		if (g_cooldown > 0.0f) {
			g_cooldown = std::max(0.0f, g_cooldown - a_delta);
		}
		FinishLeaving(a_delta);
		for (auto it = g_pack.begin(); it != g_pack.end();) {
			it->left -= a_delta;
			if (it->followIn >= 0.0f && (it->followIn -= a_delta) < 0.0f) {
				it->followIn = 3.0f;  // and checked every 3 s: after a fight, or if the game dropped it
				auto* me = RE::TESForm::LookupByID<RE::Actor>(it->id);
				auto* pl = RE::PlayerCharacter::GetSingleton();
				if (me && pl && !me->IsDead()) {
					Follow(me, pl, it->ox, it->oy);
					auto* cur = CurrentPackage(me);
					logger::info("call: {:08X} {:.0f} away, combat {}, package {:08X}{}", it->id, pl->GetPosition().GetDistance(me->GetPosition()),
						me->IsInCombat(), cur ? cur->GetFormID() : 0u, cur && cur == FollowPackage() ? " (following)" : "");
				}
			}
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
						g_pack.push_back({ a->GetFormID(), 10.0f, -1.0f });
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
