#include "Moves.h"

#include "Shift.h"
#include "State.h"

#include <SKSEMenuFramework.h>

#include <mutex>

namespace protean::Moves
{
	namespace
	{
		namespace ui = ImGuiMCP;
		namespace dl = ImGuiMCP::ImDrawListManager;

		constexpr auto kIni = "Data/SKSE/Plugins/Protean.ini";
		constexpr auto kShadow = IM_COL32(0, 0, 0, 200);
		constexpr auto kText = IM_COL32(235, 235, 220, 235);
		constexpr auto kDim = IM_COL32(170, 170, 160, 170);

		// DirectInput scan codes
		constexpr std::uint32_t kKey1 = 2;       // keys 1..8 are scan codes 2..9
		constexpr std::uint32_t kLShift = 0x2A;
		constexpr std::uint32_t kRShift = 0x36;

		std::uint32_t g_menuKey{ 0x30 };  // B
		bool          g_shift{ false };

		SKSEMenuFramework::Model::WindowInterface* g_window{ nullptr };
		std::mutex                                 g_lock;  // the draw functions run on the render thread

		// ---- helpers -----------------------------------------------------------------------------

		std::string Pretty(std::string a_event)
		{
			for (std::string_view prefix : { "attackPowerStart_"sv, "attackStart_"sv, "AttackStart_"sv, "attackPowerStart"sv, "attackStart"sv, "bashStart"sv }) {
				if (a_event.size() > prefix.size() && a_event.compare(0, prefix.size(), prefix) == 0) {
					a_event.erase(0, prefix.size());
					break;
				}
			}
			std::string out;
			for (std::size_t i = 0; i < a_event.size(); ++i) {
				const char ch = a_event[i];
				if (ch == '_') {
					out += ' ';
					continue;
				}
				if (i > 0 && std::isupper(static_cast<unsigned char>(ch)) && std::islower(static_cast<unsigned char>(a_event[i - 1]))) {
					out += ' ';
				}
				out += ch;
			}
			if (!out.empty()) {
				out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
			}
			return out.empty() ? "Attack" : out;
		}

		const std::string& SlotName(Slot a_slot)
		{
			static const std::string lmb = "Left mouse button", rmb = "Right mouse button";
			return a_slot == Slot::kLmb ? lmb : rmb;
		}

		std::string& Entry(State& a_s, const std::string& a_id, Slot a_slot)
		{
			auto& pair = a_s.moves[a_id];
			return a_slot == Slot::kLmb ? pair.first : pair.second;
		}

		void Assign(const std::string& a_id, Slot a_slot, const std::string& a_event)
		{
			{
				std::scoped_lock lk(g_lock);
				Entry(State::Get(), a_id, a_slot) = a_event;
			}
			const auto* c = Registry::Get().ById(a_id);
			if (c) {
				for (const auto* m : List(*c)) {
					if (m->event == a_event) {
						Shift::Notify(std::format("{}: {}", SlotName(a_slot), a_event.empty() ? std::string("normal attack") : Label(*m)));
					}
				}
			}
		}

		// ---- window and HUD strip (render thread) -----------------------------------------------------

		void __stdcall RenderWindow()
		{
			const auto* c = Shift::ActiveCreature();
			if (!c) {
				ui::Text("%s", "You are not in an animal form.");
				return;
			}
			ui::Text("%s moves", c->name.c_str());
			ui::TextDisabled("Choose which move each mouse button performs. Tapping uses it; holding is always a power attack.");
			ui::Separator();
			const auto moves = List(*c);
			int        id = 0;
			for (const auto* m : moves) {
				ui::PushID(id++);
				ui::Text("%s", Label(*m).c_str());
				ui::SameLine(260.0f);
				if (ui::SmallButton("LMB")) {
					const auto ev = m->event, cid = c->id;
					SKSE::GetTaskInterface()->AddTask([cid, ev]() { Assign(cid, Slot::kLmb, ev); });
				}
				ui::SameLine();
				if (ui::SmallButton("RMB")) {
					const auto ev = m->event, cid = c->id;
					SKSE::GetTaskInterface()->AddTask([cid, ev]() { Assign(cid, Slot::kRmb, ev); });
				}
				ui::PopID();
			}
			ui::Separator();
			if (ui::Button("Reset both buttons")) {
				const auto cid = c->id;
				SKSE::GetTaskInterface()->AddTask([cid]() {
					Assign(cid, Slot::kLmb, "");
					Assign(cid, Slot::kRmb, "");
				});
			}
			ui::SameLine();
			if (ui::Button("Close") && g_window) {
				g_window->IsOpen = false;
			}
		}

		void ShadowText(ui::ImDrawList* a_dl, float a_x, float a_y, ui::ImU32 a_col, const char* a_text)
		{
			dl::AddText(a_dl, { a_x + 1.0f, a_y + 1.0f }, kShadow, a_text);
			dl::AddText(a_dl, { a_x, a_y }, a_col, a_text);
		}

		void __stdcall DrawStrip()
		{
			const auto* c = Shift::ActiveCreature();
			if (!c) {
				return;
			}
			auto* game = RE::UI::GetSingleton();
			if (!game || game->GameIsPaused() || !game->IsShowingMenus()) {
				return;
			}
			auto*      drawList = ui::GetForegroundDrawList();
			const auto screen = ui::GetIO()->DisplaySize;
			if (!drawList || screen.x <= 0.0f) {
				return;
			}
			auto name = [&](Slot s) {
				const auto* m = Assigned(*c, s);
				return m ? Label(*m) : std::string("normal");
			};
			const std::string text = std::format("LMB: {}    RMB: {}", name(Slot::kLmb), name(Slot::kRmb));
			const auto        size = ui::CalcTextSize(text.c_str());
			ShadowText(drawList, (screen.x - size.x) * 0.5f, screen.y * 0.93f, kText, text.c_str());
			ShadowText(drawList, (screen.x - ui::CalcTextSize("Moves menu: B   |   keys 1-8 set LMB   |   Shift+1-8 set RMB").x) * 0.5f,
				screen.y * 0.93f + size.y + 2.0f, kDim, "Moves menu: B   |   keys 1-8 set LMB   |   Shift+1-8 set RMB");
		}

		// ---- keys --------------------------------------------------------------------------------------

		class Sink : public RE::BSTEventSink<RE::InputEvent*>
		{
		public:
			static Sink* Get()
			{
				static Sink s;
				return &s;
			}

			RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) override
			{
				if (!a_event) {
					return RE::BSEventNotifyControl::kContinue;
				}
				for (auto* e = *a_event; e; e = e->next) {
					auto* b = e->AsButtonEvent();
					if (!b || b->GetDevice() != RE::INPUT_DEVICE::kKeyboard) {
						continue;
					}
					const auto code = b->GetIDCode();
					if (code == kLShift || code == kRShift) {
						g_shift = b->IsPressed();
						continue;
					}
					const auto* c = Shift::ActiveCreature();
					if (!c || !b->IsDown()) {
						continue;
					}
					if (code == g_menuKey && g_window) {
						g_window->IsOpen = !g_window->IsOpen;
					} else if (code >= kKey1 && code < kKey1 + 8) {
						const auto moves = List(*c);
						const std::size_t idx = code - kKey1;
						if (idx < moves.size()) {
							const auto cid = c->id, ev = moves[idx]->event;
							const Slot slot = g_shift ? Slot::kRmb : Slot::kLmb;
							SKSE::GetTaskInterface()->AddTask([cid, ev, slot]() { Assign(cid, slot, ev); });
						}
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		void LoadIni()
		{
			std::ifstream in(kIni);
			std::string   line;
			bool          inSection = false;
			while (std::getline(in, line)) {
				if (!line.empty() && line[0] == '[') {
					inSection = line.starts_with("[Moves]");
				} else if (inSection && line.starts_with("MenuKey=")) {
					try {
						g_menuKey = static_cast<std::uint32_t>(std::stoul(line.substr(8)));
					} catch (...) {
					}
				}
			}
		}
	}

	bool WindowOpen()
	{
		return g_window && g_window->IsOpen;
	}

	std::string Label(const Attack& a_attack)
	{
		std::string s = Pretty(a_attack.event);
		if (a_attack.hasSpell) {
			s += " (spell)";
		} else if (a_attack.bash) {
			s += " (bash)";
		} else if (a_attack.power) {
			s += " (power)";
		}
		return s;
	}

	std::vector<const Attack*> List(const Creature& a_c)
	{
		std::vector<const Attack*> out;
		std::set<std::string>      seen;
		for (const auto& a : a_c.attacks) {
			if (!a.event.empty() && seen.insert(a.event).second) {
				out.push_back(&a);
			}
		}
		return out;
	}

	const Attack* Assigned(const Creature& a_c, Slot a_slot)
	{
		std::string ev;
		{
			std::scoped_lock lk(g_lock);
			const auto&      moves = State::Get().moves;
			const auto       it = moves.find(a_c.id);
			if (it == moves.end()) {
				return nullptr;
			}
			ev = a_slot == Slot::kLmb ? it->second.first : it->second.second;
		}
		if (ev.empty()) {
			return nullptr;
		}
		for (const auto& a : a_c.attacks) {
			if (a.event == ev) {
				return &a;
			}
		}
		return nullptr;
	}

	void Install()
	{
		LoadIni();
		if (auto* idm = RE::BSInputDeviceManager::GetSingleton()) {
			idm->AddEventSink<RE::InputEvent*>(Sink::Get());
		}
		logger::info("moves menu key {} (scan code); keys 1-8 assign moves", g_menuKey);
	}

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
			logger::warn("SKSE Menu Framework not installed: no Beast Moves window or strip (number keys still work)");
			return;
		}
		g_window = SKSEMenuFramework::AddWindow(RenderWindow, false);
		SKSEMenuFramework::AddHudElement(DrawStrip);
		logger::info("Beast Moves window and HUD strip registered");
	}
}
