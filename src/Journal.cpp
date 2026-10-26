#include "Journal.h"

#include "Evolve.h"
#include "Registry.h"
#include "State.h"

#include <SKSEMenuFramework.h>

namespace protean::Journal
{
	namespace
	{
		namespace ui = ImGuiMCP;

		struct Row
		{
			std::string   name;
			std::uint32_t hearts{ 0 };
			std::uint32_t points{ 0 };
			std::uint32_t next{ 0 };  // the heart that pays next; 0 = none left
		};

		// the numbers as the main thread last saw them: the page draws on the render thread
		std::mutex       g_lock;
		std::vector<Row> g_rows;
		std::string      g_summary;
		std::string      g_next;

		void __stdcall Render()
		{
			std::vector<Row> rows;
			std::string      summary, next;
			{
				std::scoped_lock l(g_lock);
				rows = g_rows;
				summary = g_summary;
				next = g_next;
			}
			ui::TextUnformatted(summary.c_str());
			ui::TextUnformatted(next.c_str());
			ui::SeparatorText("Hearts eaten");
			ui::TextWrapped("%s", "Each kind gives an Evolution Point for its 1st, 3rd, 5th, 10th, 15th and 20th heart.");
			if (ui::BeginTable("hearts", 4, ui::ImGuiTableFlags_RowBg | ui::ImGuiTableFlags_Borders | ui::ImGuiTableFlags_SizingStretchProp)) {
				ui::TableSetupColumn("Creature", ui::ImGuiTableColumnFlags_WidthStretch, 2.0f);
				ui::TableSetupColumn("Hearts", ui::ImGuiTableColumnFlags_WidthStretch, 1.0f);
				ui::TableSetupColumn("Points", ui::ImGuiTableColumnFlags_WidthStretch, 1.0f);
				ui::TableSetupColumn("Next point", ui::ImGuiTableColumnFlags_WidthStretch, 1.5f);
				ui::TableHeadersRow();
				for (const auto& r : rows) {
					ui::TableNextRow();
					ui::TableNextColumn();
					ui::TextUnformatted(r.name.c_str());
					ui::TableNextColumn();
					ui::TextUnformatted(std::to_string(r.hearts).c_str());
					ui::TableNextColumn();
					ui::TextUnformatted(std::format("{} / {}", r.points, Evolve::HeartPointsMax()).c_str());
					ui::TableNextColumn();
					ui::TextUnformatted(r.next ? std::format("heart {}", r.next).c_str() : "all earned");
				}
				ui::EndTable();
			}
		}

		void Refresh()
		{
			const auto&      s = State::Get();
			std::vector<Row> rows;
			for (const auto& c : Registry::Get().All()) {
				if (!c.heart) {
					continue;
				}
				const auto    it = s.feeds.find(c.id);
				const auto    n = it == s.feeds.end() ? 0u : it->second;
				rows.push_back({ c.name, n, Evolve::HeartPoints(n), Evolve::NextHeart(n) });
			}
			// kinds already tasted first, the most eaten on top; the rest by name
			std::ranges::sort(rows, [](const Row& a, const Row& b) {
				return a.hearts != b.hearts ? a.hearts > b.hearts : a.name < b.name;
			});
			const auto at = Evolve::NextPointAt(s.evoTotal);
			std::scoped_lock l(g_lock);
			g_rows = std::move(rows);
			g_summary = std::format("Evolution Points: {}    Perk points available: {}", s.evoTotal, Evolve::PointsAvailable());
			g_next = std::format("Next perk point at {} Evolution Points ({} to go)", at, at - s.evoTotal);
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
		static float tick = 0.0f;
		if ((tick -= a_delta) <= 0.0f) {
			tick = 0.5f;
			Refresh();
		}
	}
}
