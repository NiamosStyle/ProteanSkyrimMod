#include "Dev.h"

#include "Attacks.h"
#include "Claws.h"
#include "Evolve.h"
#include "Face.h"
#include "Fly.h"
#include "Move.h"
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
			} else if (a_cmd == "graph") {
				// graph [filter]: the event and variable names the player's behaviour graph knows (case-insensitive filter)
				RE::BSTSmartPointer<RE::BSAnimationGraphManager> mgr;
				if (!p->GetAnimationGraphManager(mgr) || !mgr || mgr->graphs.empty() || !mgr->graphs[0]) {
					logger::info("dev: graph: no animation graph");
					return;
				}
				auto* bg = mgr->graphs[0]->behaviorGraph;
				auto* sd = bg && bg->data ? bg->data->stringData.get() : nullptr;
				if (!sd) {
					logger::info("dev: graph: no string data");
					return;
				}
				auto lower = [](std::string s) {
					std::ranges::transform(s, s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
					return s;
				};
				const auto want = lower(a_arg);
				std::string events, vars;
				for (auto& e : sd->eventNames) {
					if (const char* n = e.data(); n && lower(n).find(want) != std::string::npos) {
						events += std::string(events.empty() ? "" : ", ") + n;
					}
				}
				for (auto& v : sd->variableNames) {
					const char* n = v.data();
					if (!n || lower(n).find(want) == std::string::npos) {
						continue;
					}
					bool  b = false;
					int   i = 0;
					float f = 0.0f;
					p->GetGraphVariableBool(n, b);
					p->GetGraphVariableInt(n, i);
					p->GetGraphVariableFloat(n, f);
					vars += std::format("{}{}=b{} i{} f{:.2f}", vars.empty() ? "" : ", ", n, b, i, f);
				}
				logger::info("dev: graph '{}' events: {}", a_arg, events);
				logger::info("dev: graph '{}' variables: {}", a_arg, vars);
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
			} else if (a_cmd == "movemode" && !a_arg.empty()) {
				Move::SetMode(std::atoi(a_arg.c_str()));
			} else if (a_cmd == "turnscale" && !a_arg.empty()) {
				Move::SetTurnScale(static_cast<float>(std::atof(a_arg.c_str())));
			} else if (a_cmd == "movelog") {
				Move::SetLog(a_arg != "0");
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
			} else if (a_cmd == "fly") {
				// fly [mode] [speed]: tune, then take off (mode 1 = in-air state, 2 = flying state); "fly 0" lands
				std::istringstream in(a_arg);
				int                mode = -1;
				float              speed = 0.0f;
				in >> mode >> speed;
				if (mode == 0) {
					Fly::Stop("dev command");
				} else {
					Fly::SetTuning(mode, speed);
					Fly::TakeOff();
				}
			} else if (a_cmd == "dial" && !a_arg.empty()) {
				// dial <global name> [value]: read or set one of Protean's dials (docs/INTEGRATION.md)
				std::istringstream in(a_arg);
				std::string        name;
				float              value = 0.0f;
				in >> name;
				if (in >> value) {
					reg.SetGlobal(name, value);
				}
				logger::info("dev: dial {} = {}", name, reg.Dial(name, -1.0f));
			} else if (a_cmd == "sound" && !a_arg.empty()) {
				// sound <SNDR editor ID>: play it on the player (to audition sounds)
				RE::BSSoundHandle h;
				if (auto* audio = RE::BSAudioManager::GetSingleton()) {
					audio->GetSoundHandleByName(h, a_arg.c_str(), 0x1A);
				}
				const bool ok = h.IsValid();
				if (ok) {
					h.SetPosition(p->GetPosition());
					h.Play();
				}
				logger::info("dev: sound '{}' playing: {}", a_arg, ok);
			} else if (a_cmd == "leap") {
				// leap [distance x] [height x]: tune (multipliers of the table's values), then leap
				std::istringstream in(a_arg);
				float              dist = 0.0f, height = 0.0f;
				in >> dist >> height;
				Fly::SetLeapTuning(dist, height);
			} else if (a_cmd == "cc") {
				// the player's collision capsule against the model's size (the fox clipped through a wall)
				auto* cc = p->GetCharController();
				auto* root = p->Get3D(false);
				if (cc) {
					logger::info("dev: cc radius {:.3f} height {:.3f} scale {:.3f} actorHeight {:.3f} (havok units); model radius {:.1f} "
					             "refr scale {:.2f} npc height {:.2f}",
						cc->radius, cc->height, cc->scale, cc->actorHeight, root ? root->worldBound.radius : 0.0f, p->GetScale(),
						p->GetActorBase() ? p->GetActorBase()->GetHeight() : 0.0f);
				}
			} else if (a_cmd == "find3d" && !a_arg.empty()) {
				// find3d <text>: every node in the 3rd- and 1st-person models whose name contains the text
				for (const bool first : { false, true }) {
					auto* root = p->Get3D(first);
					logger::info("dev: find3d '{}' in {} person model ({})", a_arg, first ? "1st" : "3rd", root ? "loaded" : "none");
					if (root) {
						RE::BSVisit::TraverseScenegraphObjects(root, [&](RE::NiAVObject* a_obj) {
							if (a_obj->name.contains(a_arg)) {
								const auto& t = a_obj->world.translate;
								logger::info("dev:   '{}' culled {} at {:.1f} {:.1f} {:.1f} parent '{}'", a_obj->name.c_str(),
									a_obj->GetAppCulled(), t.x, t.y, t.z, a_obj->parent ? a_obj->parent->name.c_str() : "");
							}
							return RE::BSVisit::BSVisitControl::kContinue;
						});
						RE::BSVisit::TraverseScenegraphGeometries(root, [&](RE::BSGeometry* a_geo) {
							if (a_geo->name.contains(a_arg)) {
								logger::info("dev:   shape '{}' culled {} parent '{}'", a_geo->name.c_str(), a_geo->GetAppCulled(),
									a_geo->parent ? a_geo->parent->name.c_str() : "");
							}
							return RE::BSVisit::BSVisitControl::kContinue;
						});
					}
					// what each body slot holds, and whether its model was made
					if (const auto& biped = p->GetBiped(first)) {
						for (std::uint32_t i = 0; i < RE::BIPED_OBJECTS::kTotal; ++i) {
							const auto& o = biped->objects[i];
							if (o.item) {
								logger::info("dev:   slot {} '{}' addon {:X} model {}", i, o.item->GetName(),
									o.addon ? o.addon->GetFormID() : 0, o.partClone ? o.partClone->name.c_str() : "(none)");
							}
						}
					}
				}
			} else if (a_cmd == "draw") {
				p->DrawWeaponMagicHands(true);
				logger::info("dev: draw requested");
			} else if (a_cmd == "evo") {
				// evo [n]: n feedings; no number just prints the state
				const int n = a_arg.empty() ? 0 : std::atoi(a_arg.c_str());
				if (n > 0) {
					Evolve::Add(static_cast<std::uint32_t>(n), "dev command");
				}
				Evolve::Dump();
			} else if (a_cmd == "faceperk") {
				Evolve::AddFacePoint("dev command");
			} else if (a_cmd == "buyperk" && !a_arg.empty()) {
				Evolve::Buy(a_arg);
				Evolve::Dump();
			} else if (a_cmd == "feed" && c) {
				Evolve::Feed(*c);  // as if one of its hearts was eaten (the form is not learned)
			} else if (a_cmd == "killboost") {
				Evolve::OnKill();  // as if the player had killed something
				Evolve::Dump();
			} else if (a_cmd == "claws") {
				Claws::Give();  // the Rending Claws weapon, one per hand
				logger::info("dev: claw weapons given");
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
				if (a_event->target->IsPlayerRef() && Shift::ActiveCreature()) {
					// the player in animal form being hit: how much each blow takes
					auto* source = RE::TESForm::LookupByID(a_event->source);
					logger::info("player hit by '{}' with '{}'{}{}, health now {:.1f}", a_event->cause->GetName(),
						source ? source->GetName() : "?", a_event->flags.any(RE::TESHitEvent::Flag::kPowerAttack) ? " (power attack)" : "",
						a_event->flags.any(RE::TESHitEvent::Flag::kSneakAttack) ? " (sneak attack)" : "",
						RE::PlayerCharacter::GetSingleton()->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth));
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
			// line by line: the file starts with a comment, which stopped a plain `f >> seq` loop at once and
			// replayed every old command on the next load (seen 2026-10-20)
			std::ifstream f(kCmd);
			for (std::string l; std::getline(f, l);) {
				std::istringstream ls(l);
				if (long seq = 0; ls >> seq) {
					g_lastSeq = std::max(g_lastSeq, seq);
				}
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
