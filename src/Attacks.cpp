#include "Attacks.h"

#include "Moves.h"
#include "Registry.h"
#include "Shift.h"

namespace protean::Attacks
{
	namespace
	{
		constexpr float kHoldSeconds = 0.4f;
		constexpr auto  kIni = "Data/SKSE/Plugins/Protean.ini";

		bool g_enabled{ true };

		struct Press
		{
			bool                                  down{ false };
			bool                                  fired{ false };
			bool                                  left{ false };
			std::chrono::steady_clock::time_point since{};
		};
		Press g_press;
		int   g_rotor{ 0 };  // round-robin through the animal's attacks so they vary

		void LoadIni()
		{
			std::ifstream in(kIni);
			std::string   line;
			bool          inSection = false;
			while (std::getline(in, line)) {
				if (!line.empty() && line[0] == '[') {
					inSection = line.starts_with("[Attacks]");
				} else if (inSection && line.starts_with("Translate=")) {
					g_enabled = line.size() > 10 && line[10] == '1';
				}
			}
		}

		bool Contains(const std::string& a_hay, std::string_view a_needle)
		{
			return a_hay.find(a_needle) != std::string::npos;
		}

		// Choose an event for this press from the animal's own attack list.
		const Attack* Pick(const Creature& a_c, bool a_power, bool a_left)
		{
			std::vector<const Attack*> all;
			std::vector<const Attack*> sided;
			for (const auto& a : a_c.attacks) {
				if (a.bash || a.event.empty()) {
					continue;
				}
				if (a.power != a_power) {
					continue;
				}
				all.push_back(&a);
				if (Contains(a.event, a_left ? "Left" : "Right")) {
					sided.push_back(&a);
				}
			}
			const auto& pool = sided.empty() ? all : sided;
			if (pool.empty()) {
				return nullptr;
			}
			return pool[static_cast<std::size_t>(g_rotor++) % pool.size()];
		}

		void Fire(bool a_power)
		{
			const auto* c = Shift::ActiveCreature();
			auto*       p = RE::PlayerCharacter::GetSingleton();
			if (!c || !p) {
				return;
			}
			// a tap uses the move assigned to that mouse button (Beast Moves menu); a hold is always a power attack
			const Attack* a = a_power ? nullptr : Moves::Assigned(*c, g_press.left ? Moves::Slot::kRmb : Moves::Slot::kLmb);
			if (!a) {
				a = Pick(*c, a_power, g_press.left);
			}
			if (!a && a_power) {
				a = Pick(*c, false, g_press.left);  // no power attack in the list: fall back to a normal one
			}
			if (!a) {
				logger::info("attack: '{}' has no usable {} attack", c->id, a_power ? "power" : "normal");
				return;
			}
			if (auto* st = p->AsActorState(); st && !st->IsWeaponDrawn()) {
				p->DrawWeaponMagicHands(true);
			}
			const bool ok = p->NotifyAnimationGraph(a->event);
			logger::info("attack: '{}' {} {} -> graph accepted: {}", c->id, a_power ? "power" : "normal", a->event, ok);
		}

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
				if (!g_enabled || !a_event || !Shift::ActiveCreature() || Moves::WindowOpen()) {
					return RE::BSEventNotifyControl::kContinue;
				}
				const auto* ue = RE::UserEvents::GetSingleton();
				for (auto* e = *a_event; e; e = e->next) {
					auto* b = e->AsButtonEvent();
					if (!b) {
						continue;
					}
					if (b->userEvent == ue->activate && b->IsDown()) {
						// doors, containers, talking, picking things up: blocked in animal form (user decision)
						static auto last = std::chrono::steady_clock::time_point{};
						const auto  now = std::chrono::steady_clock::now();
						if (now - last > std::chrono::seconds(3)) {
							last = now;
							Shift::Notify("You cannot do that in animal form. Change back first.");
						}
						continue;
					}
					const bool isLeft = b->userEvent == ue->leftAttack;
					const bool isRight = b->userEvent == ue->rightAttack;
					if (!isLeft && !isRight) {
						continue;
					}
					if (b->IsDown()) {
						g_press = Press{ true, false, isLeft, std::chrono::steady_clock::now() };
					} else if (b->IsUp() && g_press.down) {
						if (!g_press.fired) {
							Fire(false);  // released before the hold time: a normal attack
						}
						g_press = Press{};
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Install()
	{
		LoadIni();
		if (auto* idm = RE::BSInputDeviceManager::GetSingleton()) {
			idm->AddEventSink<RE::InputEvent*>(Sink::Get());
		}
		logger::info("attack translation {}", g_enabled ? "on" : "off");
	}

	void OnFrame()
	{
		if (!g_press.down || g_press.fired) {
			return;
		}
		const float held = std::chrono::duration<float>(std::chrono::steady_clock::now() - g_press.since).count();
		if (held >= kHoldSeconds) {
			g_press.fired = true;
			Fire(true);  // still held after the hold time: a power attack
		}
	}

	void Cancel()
	{
		g_press = Press{};
	}
}
