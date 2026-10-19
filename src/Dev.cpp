#include "Dev.h"

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
			ls >> arg;
			todo.emplace_back(seq, cmd, arg);
		}
		for (auto& [seq, cmd, arg] : todo) {
			g_lastSeq = std::max(g_lastSeq, seq);
			// run outside the player's Update call, on SKSE's task queue
			SKSE::GetTaskInterface()->AddTask([cmd, arg]() { Run(cmd, arg); });
		}
	}
}
