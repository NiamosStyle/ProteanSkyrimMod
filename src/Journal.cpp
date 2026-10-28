#include "Journal.h"

#include "Evolve.h"
#include "Registry.h"
#include "State.h"

#include <SKSEMenuFramework.h>

#include <atomic>

namespace protean::Journal
{
	namespace
	{
		namespace ui = ImGuiMCP;

		struct Row
		{
			std::string   name;
			std::uint32_t bodies{ 0 };
		};

		// the numbers as the main thread last saw them: the page draws on the render thread
		std::mutex       g_lock;
		std::vector<Row> g_rows;
		std::string      g_summary;
		std::string      g_next;
		std::string      g_total;
		// the loot switch: the page shows g_loot and leaves a click in g_lootSet (-1 = none) for the main thread
		std::atomic<bool> g_loot{ false };
		std::atomic<int>  g_lootSet{ -1 };
		std::atomic<bool> g_ask{ true };  // the same for "Ask first"
		std::atomic<int>  g_askSet{ -1 };
		std::atomic<bool> g_stealthLoot{ true };  // the same for "Stealth Consume takes the loot"
		std::atomic<int>  g_stealthLootSet{ -1 };

		void __stdcall Render()
		{
			std::vector<Row> rows;
			std::string      summary, next, total;
			{
				std::scoped_lock l(g_lock);
				rows = g_rows;
				summary = g_summary;
				next = g_next;
				total = g_total;
			}
			ui::TextUnformatted(summary.c_str());
			ui::TextUnformatted(next.c_str());
			ui::SeparatorText("Consuming");
			const int pending = g_lootSet.load();
			bool      loot = pending >= 0 ? pending == 1 : g_loot.load();
			if (ui::Checkbox("Take the loot when consuming", &loot)) {
				g_lootSet = loot ? 1 : 0;
			}
			ui::TextWrapped("%s", "Off, a consumed body's belongings are destroyed with it (quest items are always kept).");
			const int askPending = g_askSet.load();
			bool      ask = askPending >= 0 ? askPending == 1 : g_ask.load();
			if (ui::Checkbox("Ask first: Loot / Consume / Cancel", &ask)) {
				g_askSet = ask ? 1 : 0;
			}
			ui::TextWrapped("%s", "In your own form, sneaking and activating a body asks what to do. Off, it is consumed at once. "
			                      "(In animal form a body is always consumed at once.)");
			const int stealthPending = g_stealthLootSet.load();
			bool      stealthLoot = stealthPending >= 0 ? stealthPending == 1 : g_stealthLoot.load();
			if (ui::Checkbox("Stealth Consume takes the loot", &stealthLoot)) {
				g_stealthLootSet = stealthLoot ? 1 : 0;
			}
			ui::TextWrapped("%s", "Assassin's power: on, everything a sneak kill carries goes into your inventory. Off, the "
			                      "switch above decides, as for any body.");
			ui::SeparatorText("Bodies consumed");
			ui::TextWrapped("%s", "Every body gives Evolution Points by its size: small creatures 1, people and middling animals 2, "
			                      "large creatures 3, giants and mammoths 5. Each perk point needs one more than the last. "
			                      "Consuming one of the renowned (Ulfric, Tullius, Mercer Frey, the Ebony Warrior...) "
			                      "gives a perk point at once.");
			ui::TextUnformatted(total.c_str());
			if (!rows.empty() && ui::BeginTable("bodies", 2, ui::ImGuiTableFlags_RowBg | ui::ImGuiTableFlags_Borders | ui::ImGuiTableFlags_SizingStretchProp)) {
				ui::TableSetupColumn("Kind", ui::ImGuiTableColumnFlags_WidthStretch, 2.0f);
				ui::TableSetupColumn("Bodies", ui::ImGuiTableColumnFlags_WidthStretch, 1.0f);
				ui::TableHeadersRow();
				for (const auto& r : rows) {
					ui::TableNextRow();
					ui::TableNextColumn();
					ui::TextUnformatted(r.name.c_str());
					ui::TableNextColumn();
					ui::TextUnformatted(std::to_string(r.bodies).c_str());
				}
				ui::EndTable();
			}
		}

		void Refresh()
		{
			auto&            s = State::Get();
			std::vector<Row> rows;
			std::uint32_t    bodies = 0;
			for (const auto& [kind, n] : s.consumed) {
				const auto* c = Registry::Get().ById(kind);
				rows.push_back({ kind == "people" ? "People" : c ? c->name : "Other creatures", n });
				bodies += n;
			}
			// the most consumed on top
			std::ranges::sort(rows, [](const Row& a, const Row& b) { return a.bodies != b.bodies ? a.bodies > b.bodies : a.name < b.name; });
			const auto at = Evolve::NextPointAt(s.evoTotal);
			std::scoped_lock l(g_lock);
			g_rows = std::move(rows);
			g_summary = std::format("Evolution Points: {}    Perk points available: {}", s.evoTotal, Evolve::PointsAvailable());
			g_next = std::format("Next perk point at {} Evolution Points ({} to go)", at, at - s.evoTotal);
			g_total = std::format("{} bodies consumed", bodies);
		}
	}

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
			logger::info("SKSE Menu Framework not installed: no Protean page");
			return;
		}
		SKSEMenuFramework::SetSection("Protean");
		SKSEMenuFramework::AddSectionItem("Evolution", Render);
		logger::info("menu: Protean page registered");
	}

	void OnFrame(float a_delta)
	{
		auto& s = State::Get();
		if (const int set = g_lootSet.exchange(-1); set >= 0) {
			s.consumeLoot = set == 1;
			logger::info("menu: take the loot when consuming {}", s.consumeLoot ? "on" : "off");
		}
		g_loot = s.consumeLoot;
		if (const int set = g_askSet.exchange(-1); set >= 0) {
			s.consumeAsk = set == 1;
			logger::info("menu: ask before consuming {}", s.consumeAsk ? "on" : "off");
		}
		g_ask = s.consumeAsk;
		if (const int set = g_stealthLootSet.exchange(-1); set >= 0) {
			s.stealthLoot = set == 1;
			logger::info("menu: Stealth Consume takes the loot {}", s.stealthLoot ? "on" : "off");
		}
		g_stealthLoot = s.stealthLoot;
		static float tick = 0.0f;
		if ((tick -= a_delta) <= 0.0f) {
			tick = 0.5f;
			Refresh();
		}
	}
}
