#include "Fly.h"

#include "Move.h"
#include "Shift.h"

namespace protean::Fly
{
	namespace
	{
		using Clock = std::chrono::steady_clock;

		constexpr float kTakeOffSpeed = 350.0f;  // game units per second, straight up
		constexpr float kTakeOffSeconds = 0.5f;
		constexpr float kLandAfter = 0.8f;       // seconds in the air before touching the ground lands

		bool              g_flying{ false };
		Clock::time_point g_since{};
		float             g_gravity{ 1.0f };     // the controller's own, put back on landing
		int               g_mode{ 1 };           // 1 = keep the controller in its in-air state, 2 = its flying state
		float             g_speed{ 600.0f };     // game units per second
		float             g_logIn{ 0.0f };
		RE::NiPoint3      g_lastPos{};

		bool Flies()
		{
			const auto* c = Shift::ActiveCreature();
			return c && c->flies;
		}

		float Seconds()
		{
			return std::chrono::duration<float>(Clock::now() - g_since).count();
		}

		// where the camera looks, including up and down
		RE::NiPoint3 CameraForward()
		{
			auto* cam = RE::PlayerCamera::GetSingleton();
			if (!cam || !cam->cameraRoot) {
				return { 0.0f, 1.0f, 0.0f };
			}
			const auto& r = cam->cameraRoot->world.rotate;
			return { r.entry[0][1], r.entry[1][1], r.entry[2][1] };
		}

		class Sink : public RE::BSTEventSink<RE::InputEvent*>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) override
			{
				if (!a_event || g_flying || !Flies()) {
					return RE::BSEventNotifyControl::kContinue;
				}
				const auto* ue = RE::UserEvents::GetSingleton();
				for (auto* e = *a_event; e; e = e->next) {
					if (auto* b = e->AsButtonEvent(); b && b->userEvent == ue->jump && b->IsDown()) {
						SKSE::GetTaskInterface()->AddTask([] { TakeOff(); });
						break;
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Install()
	{
		static Sink sink;
		if (auto* idm = RE::BSInputDeviceManager::GetSingleton()) {
			idm->AddEventSink<RE::InputEvent*>(&sink);
		}
	}

	bool Flying()
	{
		return g_flying;
	}

	void TakeOff()
	{
		auto* p = RE::PlayerCharacter::GetSingleton();
		auto* cc = p ? p->GetCharController() : nullptr;
		if (g_flying || !cc || !Flies() || p->IsDead()) {
			return;
		}
		g_flying = true;
		g_since = Clock::now();
		g_gravity = cc->gravity;
		g_lastPos = p->GetPosition();
		g_logIn = 0.0f;
		logger::info("fly: take off (mode {}, speed {:.0f}, controller gravity {:.2f})", g_mode, g_speed, g_gravity);
	}

	void Stop(const char* a_why)
	{
		if (!g_flying) {
			return;
		}
		g_flying = false;
		auto* p = RE::PlayerCharacter::GetSingleton();
		if (auto* cc = p ? p->GetCharController() : nullptr) {
			cc->gravity = g_gravity;
			cc->fallStartHeight = p->GetPositionZ();  // fall damage counts from here, not from the highest point
			cc->fallTime = 0.0f;
			cc->context.currentState = RE::hkpCharacterStateType::kInAir;  // the engine finds the ground itself
		}
		logger::info("fly: {} after {:.1f} s", a_why, Seconds());
	}

	void AfterUpdate(RE::PlayerCharacter* a_player, float a_delta)
	{
		if (!g_flying) {
			return;
		}
		if (!Flies() || a_player->IsDead()) {
			Stop(a_player->IsDead() ? "died" : "no longer a flying animal");
			return;
		}
		auto* cc = a_player->GetCharController();
		if (!cc) {
			return;
		}
		const float t = Seconds();
		if (t > kLandAfter && cc->flags.all(RE::CHARACTER_FLAGS::kSupport)) {
			Stop("landed");
			return;
		}

		cc->gravity = 0.0f;
		cc->context.currentState = g_mode == 2 ? RE::hkpCharacterStateType::kFlying : RE::hkpCharacterStateType::kInAir;
		cc->fallStartHeight = a_player->GetPositionZ();  // no fall damage while flying
		cc->fallTime = 0.0f;

		// swim-style: towards where the camera looks (W), its sides (A/D), back (S); hover with no key
		const auto   keys = Move::Keys();
		RE::NiPoint3 fwd = CameraForward();
		RE::NiPoint3 right{ fwd.y, -fwd.x, 0.0f };  // horizontal, to the camera's right
		if (const float l = right.Length(); l > 0.001f) {
			right /= l;
		}
		RE::NiPoint3 v = fwd * keys.y + right * keys.x;
		if (const float l = v.Length(); l > 1.0f) {
			v /= l;
		}
		v *= g_speed;
		if (t < kTakeOffSeconds) {
			v.z = std::max(v.z, kTakeOffSpeed);
		}

		const float s = RE::bhkWorld::GetWorldScale();
		cc->SetLinearVelocityImpl(RE::hkVector4(v.x * s, v.y * s, v.z * s, 0.0f));

		// measure what the engine made of it (see Portal Spheres: in-air steering damped a set speed)
		const auto pos = a_player->GetPosition();
		g_logIn -= a_delta;
		if (g_logIn <= 0.0f && a_delta > 0.0001f) {
			g_logIn = 0.25f;
			const auto moved = (pos - g_lastPos) / a_delta;
			logger::info("fly: t {:.1f} keys ({:.0f},{:.0f}) cam ({:.2f},{:.2f},{:.2f}) want ({:.0f},{:.0f},{:.0f}) got ({:.0f},{:.0f},{:.0f}) "
			             "state {} support {} z {:.0f}",
				t, keys.x, keys.y, fwd.x, fwd.y, fwd.z, v.x, v.y, v.z, moved.x, moved.y, moved.z, static_cast<int>(cc->context.currentState),
				cc->flags.all(RE::CHARACTER_FLAGS::kSupport), pos.z);
		}
		g_lastPos = pos;
	}

	void SetTuning(int a_mode, float a_speed)
	{
		if (a_mode > 0) {
			g_mode = a_mode;
		}
		if (a_speed > 0.0f) {
			g_speed = a_speed;
		}
		logger::info("fly: mode {} speed {:.0f}", g_mode, g_speed);
	}
}
