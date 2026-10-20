#include "Dev.h"

#include "Attacks.h"
#include "Face.h"
#include "Registry.h"
#include "Shift.h"
#include "State.h"

namespace protean::Dev
{
	namespace
	{
		constexpr auto kIni = "Data/SKSE/Plugins/Protean.ini";
		constexpr auto kCmd = "Data/SKSE/Plugins/Protean.cmd";

		bool g_enabled{ false };
		long g_lastSeq{ -1 };
		int  g_frames{ 0 };

		void Run(const std::string& a_cmd, const std::string& a_arg)
		{
			auto* p = RE::PlayerCharacter::GetSingleton();
			auto& reg = Registry::Get();
			auto& s = State::Get();
			const auto* c = a_arg.empty() ? nullptr : reg.ById(a_arg);

			if (a_cmd == "starter") {
				p->AddSpell(reg.Starter());
				logger::info("dev: starter power given");
			} else if (a_cmd == "hearts") {
				for (const auto& cr : reg.All()) {
					p->AddObjectToContainer(cr.heart, nullptr, 1, nullptr);
				}
				logger::info("dev: one heart of each creature given");
			} else if (a_cmd == "learn" && c) {
				Shift::Learn(*c);
				logger::info("dev: learn '{}' ok", c->id);
			} else if (a_cmd == "unlearn" && c) {
				s.known.erase(c->id);
				p->RemoveSpell(c->power);
				logger::info("dev: unlearned '{}'", c->id);
			} else if (a_cmd == "transform" && c) {
				Shift::Transform(*c);
				logger::info("dev: transform '{}' requested, active now '{}'", c->id, s.active);
			} else if (a_cmd == "revert") {
				Shift::Revert("dev command");
				logger::info("dev: revert done, active now '{}'", s.active);
			} else if (a_cmd == "dump") {
				auto* av = p->AsActorValueOwner();
				logger::info("dev: dump race '{}' level {} health {:.1f} (perm {:.1f}) active '{}' known {} bonus {:.0f} "
				             "witnesses {} hostile {} factions-added {} looted {}",
					p->GetRace() ? p->GetRace()->GetName() : "?", p->GetLevel(), av->GetActorValue(RE::ActorValue::kHealth),
					av->GetPermanentActorValue(RE::ActorValue::kHealth), s.active, s.known.size(), s.healthBonus, s.witnesses.size(),
					s.hostileWitnesses.size(), s.addedFactions.size(), s.looted.size());
			} else if (a_cmd == "anim" && !a_arg.empty()) {
				// send an animation-graph event to the player; the result says whether the graph accepted it
				const bool ok = p->NotifyAnimationGraph(a_arg);
				logger::info("dev: anim '{}' accepted by the graph: {}", a_arg, ok);
			} else if (a_cmd == "scan") {
				// how does everyone nearby feel about the player right now?
				auto* pl = RE::ProcessLists::GetSingleton();
				int   n = 0;
				if (pl) {
					for (auto& h : pl->highActorHandles) {
						auto a = h.get();
						if (!a || a.get() == p) {
							continue;
						}
						const float dist = p->GetPosition().GetDistance(a->GetPosition());
						if (dist > 5000.0f) {
							continue;
						}
						auto target = a->GetActorRuntimeData().currentCombatTarget.get();
						logger::info("dev: scan {:08X} '{}' dist {:.0f} dead {} hostile {} detection {} targetsPlayer {} inCombat {}", a->GetFormID(),
							a->GetName(), dist, a->IsDead(), a->IsHostileToActor(p), a->RequestDetectionLevel(p),
							target && target.get() == p, a->IsInCombat());
						++n;
					}
				}
				logger::info("dev: scan done, {} actor(s)", n);
			} else if (a_cmd == "spawn" && !a_arg.empty()) {
				// spawn <hex base FormID> about 8 m in front of the player
				RE::FormID id = 0;
				try {
					id = static_cast<RE::FormID>(std::stoul(a_arg, nullptr, 16));
				} catch (...) {
				}
				auto* base = id ? RE::TESForm::LookupByID<RE::TESBoundObject>(id) : nullptr;
				if (!base) {
					logger::warn("dev: spawn: no base object {}", a_arg);
				} else {
					auto ref = p->PlaceObjectAtMe(base, false);
					if (ref) {
						const float yaw = p->GetAngleZ();
						ref->SetPosition(p->GetPositionX() + std::sin(yaw) * 560.0f, p->GetPositionY() + std::cos(yaw) * 560.0f, p->GetPositionZ());
						logger::info("dev: spawned {:08X} '{}' (ref {:08X})", id, base->GetName(), ref->GetFormID());
					}
				}
			} else if ((a_cmd == "steal" || a_cmd == "stealq" || a_cmd == "stealdata") && !a_arg.empty()) {
				// steal <hex base NPC FormID>: copy that NPC's face (stealq = nobody sees it)
				RE::FormID id = 0;
				try {
					id = static_cast<RE::FormID>(std::stoul(a_arg, nullptr, 16));
				} catch (...) {
				}
				auto* npc = id ? RE::TESForm::LookupByID<RE::TESNPC>(id) : nullptr;
				logger::info("dev: steal {} -> {}", a_arg, npc ? npc->GetName() : "no such NPC");
				if (npc) {
					Face::SetIgnoreFaceFiles(a_cmd == "stealdata");  // stealdata: build the face from the data, ignoring face files
					Face::Steal(npc, nullptr, a_cmd == "steal");
					Face::SetIgnoreFaceFiles(false);
				}
			} else if (a_cmd == "attack") {
				// attack [lmb|rmb|power|rpower]: what that button would do
				Attacks::TestFire(a_arg.find("power") != std::string::npos, a_arg.starts_with("r"));
			} else if (a_cmd == "place" && !a_arg.empty()) {
				// place <ref hex> <distance>: put that actor in front of the player, facing the player, held in place
				std::istringstream in(a_arg);
				std::string         hex;
				float               dist = 0.0f;
				in >> hex >> dist;
				auto* a = RE::TESForm::LookupByID<RE::Actor>(static_cast<RE::FormID>(std::stoul(hex, nullptr, 16)));
				if (a) {
					const float yaw = p->GetAngleZ();
					auto pos = p->GetPosition();
					pos.x += dist * std::sin(yaw);
					pos.y += dist * std::cos(yaw);
					a->SetPosition(pos, true);
					a->SetAngle({ 0.0f, 0.0f, yaw + RE::NI_PI });
					logger::info("dev: placed {:08X} '{}' {:.0f} in front, measured distance {:.0f}", a->GetFormID(), a->GetName(), dist,
						p->GetPosition().GetDistance(a->GetPosition()));
				}
			} else if (a_cmd == "reach" && !a_arg.empty()) {
				// reach <unarmed reach> [strike angle]: change the player's current race in memory (affects every actor
				// of that race until the game restarts) to try out how far and how wide attacks land
				float reach = 0.0f, angle = 0.0f;
				std::istringstream in(a_arg);
				in >> reach >> angle;
				if (auto* race = p->GetRace(); race && reach > 0.0f) {
					logger::info("dev: reach: race '{}' unarmed reach {:.0f} -> {:.0f}", race->GetName(), race->data.unarmedReach, reach);
					race->data.unarmedReach = reach;
					if (angle > 0.0f && race->attackDataMap) {
						for (auto& [name, data] : race->attackDataMap->attackDataMap) {
							if (data) {
								logger::info("  {} strike angle {:.0f} -> {:.0f}", name.c_str(), data->data.strikeAngle, angle);
								data->data.strikeAngle = angle;
							}
						}
					}
				}
			} else if (a_cmd == "facepower") {
				Face::OnPower();
			} else if (a_cmd == "takeoff") {
				Face::TakeOff("dev command");
			} else if (a_cmd == "facedump") {
				Face::Dump();
			} else if (a_cmd == "draw") {
				p->DrawWeaponMagicHands(true);
				logger::info("dev: draw requested");
			} else if (a_cmd == "kill") {
				p->KillImpl(nullptr, 0.0f, true, false);
				logger::info("dev: player killed");
			} else {
				logger::warn("dev: unknown or incomplete command '{} {}'", a_cmd, a_arg);
			}
		}
	}

	namespace
	{
		// test-only: one log line per hit the player lands, so attacks can be matched to hits
		class HitSink : public RE::BSTEventSink<RE::TESHitEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event, RE::BSTEventSource<RE::TESHitEvent>*) override
			{
				if (!a_event || !a_event->cause || !a_event->target) {
					return RE::BSEventNotifyControl::kContinue;
				}
				if (!a_event->cause->IsPlayerRef()) {
					// a wild animal Protean knows: its bites are the yardstick for the player's damage
					auto* animal = a_event->cause->As<RE::Actor>();
					auto* target = a_event->target->As<RE::Actor>();
					if (animal && target && Registry::Get().ByRace(animal->GetRace())) {
						logger::info("wild hit: '{}' bit '{}'{}, health now {:.1f}", animal->GetName(), target->GetName(),
							a_event->flags.any(RE::TESHitEvent::Flag::kPowerAttack) ? " (power attack)" : "",
							target->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth));
					}
					return RE::BSEventNotifyControl::kContinue;
				}
				auto*       target = a_event->target->As<RE::Actor>();
				const auto  f = a_event->flags;
				const char* kind = f.any(RE::TESHitEvent::Flag::kPowerAttack) ? "power attack" :
				                   f.any(RE::TESHitEvent::Flag::kBashAttack)  ? "bash" :
				                                                                "normal attack";
				// the attack data the engine has on the player right now: decides damage multiplier and power flag
				auto*       p = RE::PlayerCharacter::GetSingleton();
				const char* data = "none";
				float       cone = 0.0f;
				if (auto* proc = p->GetActorRuntimeData().currentProcess; proc && proc->high && proc->high->attackData) {
					data = proc->high->attackData->event.c_str();
					cone = proc->high->attackData->data.strikeAngle;
				}
				logger::info("hit: '{}' by {}{}, health now {:.1f}, attack data '{}' cone {:.0f}, race reach {:.0f}, distance {:.0f}",
					a_event->target->GetName(), kind, f.any(RE::TESHitEvent::Flag::kHitBlocked) ? " (blocked)" : "",
					target ? target->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth) : 0.0f, data, cone,
					p->GetRace() ? p->GetRace()->data.unarmedReach : 0.0f, p->GetPosition().GetDistance(a_event->target->GetPosition()));
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Init()
	{
		std::ifstream in(kIni);
		std::string   line;
		bool          inDev = false;
		while (std::getline(in, line)) {
			if (!line.empty() && line[0] == '[') {
				inDev = line.starts_with("[Dev]");
			} else if (inDev && line.starts_with("Commands=")) {
				g_enabled = line.size() > 9 && line[9] == '1';
			}
		}
		if (g_enabled) {
			logger::warn("dev command channel is ENABLED (Protean.ini [Dev] Commands=1); watching {}", kCmd);
			static HitSink hits;
			RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESHitEvent>(&hits);
			// ignore whatever is already in the file: only lines written after this start count
			std::ifstream f(kCmd);
			long          seq = 0;
			while (f >> seq) {
				g_lastSeq = std::max(g_lastSeq, seq);
				std::string rest;
				std::getline(f, rest);
			}
		}
	}

	void Poll()
	{
		if (!g_enabled || ++g_frames % 20 != 0) {
			return;
		}
		std::ifstream f(kCmd);
		if (!f) {
			return;
		}
		std::vector<std::tuple<long, std::string, std::string>> todo;
		std::string                                              line;
		while (std::getline(f, line)) {
			std::istringstream ls(line);
			long               seq = -1;
			std::string        cmd, arg;
			if (!(ls >> seq >> cmd) || seq <= g_lastSeq) {
				continue;
			}
			std::getline(ls >> std::ws, arg);  // the rest of the line: some commands take more than one word
			todo.emplace_back(seq, cmd, arg);
		}
		for (auto& [seq, cmd, arg] : todo) {
			g_lastSeq = std::max(g_lastSeq, seq);
			// run outside the player's Update call, on SKSE's task queue
			SKSE::GetTaskInterface()->AddTask([cmd, arg]() { Run(cmd, arg); });
		}
	}
}
