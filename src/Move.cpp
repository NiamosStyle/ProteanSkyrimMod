#include "Move.h"

#include "Shift.h"

namespace protean::Move
{
	namespace
	{
		using Clock = std::chrono::steady_clock;

		constexpr auto  kHeldFor = std::chrono::milliseconds(150);  // a held key repeats its event every frame
		constexpr float kTurnRate = 2.0f * RE::NI_PI * 1.5f;        // radians per second (540 degrees)

		enum Dir { kForward, kBack, kLeft, kRight };
		std::array<Clock::time_point, 4> g_keys{};
		RE::NiPoint2                     g_stick{};
		bool                             g_overriding{ false };
		int                              g_mode{ 1 };            // 1 = directional, 2 = tank (A/D turn)
		float                            g_wantHeading{ 0.0f };  // mode 1: the heading put back after the update
		bool                             g_steering{ false };
		constexpr float                  kTankRate = RE::NI_PI;  // mode 2: 180 degrees per second

		float Wrap(float a_angle)
		{
			while (a_angle > RE::NI_PI) {
				a_angle -= 2.0f * RE::NI_PI;
			}
			while (a_angle < -RE::NI_PI) {
				a_angle += 2.0f * RE::NI_PI;
			}
			return a_angle;
		}

		RE::ThirdPersonState* Tps()
		{
			auto* cam = RE::PlayerCamera::GetSingleton();
			if (!cam || !cam->IsInThirdPerson()) {
				return nullptr;
			}
			return static_cast<RE::ThirdPersonState*>(cam->GetRuntimeData().cameraStates[RE::CameraStates::kThirdPerson].get());
		}

		class Sink : public RE::BSTEventSink<RE::InputEvent*>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) override
			{
				if (!a_event) {
					return RE::BSEventNotifyControl::kContinue;
				}
				const auto* ue = RE::UserEvents::GetSingleton();
				for (auto* e = *a_event; e; e = e->next) {
					if (auto* b = e->AsButtonEvent()) {
						const auto& ev = b->userEvent;
						const int   dir = ev == ue->forward ? kForward : ev == ue->back ? kBack :
						                  ev == ue->strafeLeft ? kLeft : ev == ue->strafeRight ? kRight : -1;
						if (dir >= 0) {
							g_keys[dir] = b->IsPressed() ? Clock::now() : Clock::time_point{};
						}
					} else if (e->GetEventType() == RE::INPUT_EVENT_TYPE::kThumbstick) {
						auto* t = static_cast<RE::ThumbstickEvent*>(e);
						if (t->IsLeft()) {
							g_stick = { t->xValue, t->yValue };
						}
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		RE::NiPoint2 Input()
		{
			if (std::abs(g_stick.x) > 0.1f || std::abs(g_stick.y) > 0.1f) {
				return g_stick;
			}
			const auto now = Clock::now();
			auto       held = [&](Dir d) { return now - g_keys[d] < kHeldFor ? 1.0f : 0.0f; };
			return { held(kRight) - held(kLeft), held(kForward) - held(kBack) };
		}

		// hand movement back to the game with the keys as they are now: zeroing it while W is still held would stop
		// the animal until the next key event
		void StopOverriding(RE::NiPoint2 a_keys = {})
		{
			if (g_overriding) {
				RE::PlayerControls::GetSingleton()->data.moveInputVec = a_keys;
				g_overriding = false;
			}
		}

		// diagnostics while steering: what the game did to the heading and the camera offset during its update
		struct Probe
		{
			float heading{ 0.0f };
			float free{ 0.0f };
			float lookX{ 0.0f };
		};
		Probe g_before;
		int   g_probeFrames{ 0 };

		// While steering, the game drops the mouse (measured: the look input arrives, the heading ignores it), so the
		// camera is turned here. How far one unit of look input turns is measured from plain Skyrim movement (W
		// alone, the game turning the player with the mouse, as a person or an animal: one mouse setting for every body),
	// so it follows the player's own sensitivity setting.
		float g_turnPerLook{ 0.01f };  // radians per unit of look input; starting guess until measured
		int   g_samples{ 0 };
		struct Calibrate
		{
			bool  valid{ false };
			float heading{ 0.0f };
			float lookX{ 0.0f };
		};
		Calibrate g_cal;
	}

	void Install()
	{
		static Sink sink;
		if (auto* idm = RE::BSInputDeviceManager::GetSingleton()) {
			idm->AddEventSink<RE::InputEvent*>(&sink);
		}
	}

	void BeforeUpdate(RE::PlayerCharacter* a_player, float a_delta)
	{
		auto* tps = Tps();
		auto* ui = RE::UI::GetSingleton();
		g_steering = false;
		g_cal.valid = false;
		const auto  in = Input();
		const float lookX = RE::PlayerControls::GetSingleton()->data.lookInputVec.x;
		if (tps && in.y > 0.1f && std::abs(in.x) < 0.1f && !tps->freeRotationEnabled && std::abs(lookX) > 0.05f) {
			// running straight ahead, in any body: the game turns the player with the mouse. AfterUpdate measures how far.
			g_cal = { true, a_player->GetAngleZ(), lookX };
		}
		if (!Shift::ActiveCreature() || !tps || (ui && ui->GameIsPaused())) {
			StopOverriding();
			return;
		}
		if (g_mode == 2) {
			// tank: A/D turn the animal (the camera follows it), W/S as the game does
			if (std::abs(in.x) > 0.1f) {
				a_player->SetHeading(a_player->GetAngleZ() + in.x * kTankRate * a_delta);
				RE::PlayerControls::GetSingleton()->data.moveInputVec = { 0.0f, in.y };
				g_overriding = true;
			} else {
				StopOverriding();
			}
			return;
		}
		const float len = std::sqrt(in.x * in.x + in.y * in.y);
		if (std::abs(in.x) < 0.1f && in.y > -0.1f) {
			// standing, or straight ahead: plain Skyrim, the mouse turns the animal as it turns a person
			StopOverriding(in);
			return;
		}
		tps->freeRotationEnabled = true;  // the mouse turns the camera; the animal turns to where it is steered
		const float heading = a_player->GetAngleZ();
		const float camYaw = heading + tps->freeRotation.x + lookX * g_turnPerLook;
		const float want = camYaw + std::atan2(in.x, in.y);
		const float step = std::clamp(Wrap(want - heading), -kTurnRate * a_delta, kTurnRate * a_delta);
		const float now = heading + step;
		a_player->SetHeading(now);
		tps->freeRotation.x = Wrap(camYaw - now);  // the camera stays where it was looking
		RE::PlayerControls::GetSingleton()->data.moveInputVec = { 0.0f, std::min(len, 1.0f) };
		g_overriding = true;
		g_steering = true;
		g_wantHeading = now;
		g_before = { now, tps->freeRotation.x, RE::PlayerControls::GetSingleton()->data.lookInputVec.x };
	}

	void AfterUpdate(RE::PlayerCharacter* a_player)
	{
		// mode 1: the game turns a moving player back toward the camera during its update; put our turn back
		auto* tps = Tps();
		if (g_cal.valid && tps && !tps->freeRotationEnabled) {
			const float sample = Wrap(a_player->GetAngleZ() - g_cal.heading) / g_cal.lookX;
			if (std::abs(sample) > 0.0005f && std::abs(sample) < 0.2f) {  // either sign: whichever way the game turns
				g_turnPerLook = g_samples == 0 ? sample : 0.9f * g_turnPerLook + 0.1f * sample;
				if (g_samples++ % 50 == 0) {
					logger::info("move: mouse turn measured {:.5f} rad per unit (sample {:.5f}, {} samples)", g_turnPerLook, sample, g_samples);
				}
			}
		}
		if (!g_steering || !tps) {
			return;
		}
		if (g_probeFrames < 600 && g_probeFrames++ % 10 == 0) {
			logger::info("move: heading {:.3f} -> {:.3f}, camera offset {:.3f} -> {:.3f} (enabled {}), mouse x {:.3f}",
				g_before.heading, a_player->GetAngleZ(), g_before.free, tps->freeRotation.x, tps->freeRotationEnabled, g_before.lookX);
		}
		// where the camera points now, including any mouse turn the game applied to the player this frame
		const float camNow = a_player->GetAngleZ() + tps->freeRotation.x;
		a_player->SetHeading(g_wantHeading);
		tps->freeRotationEnabled = true;
		tps->freeRotation.x = Wrap(camNow - g_wantHeading);
	}

	void SetMode(int a_mode)
	{
		Reset();
		g_mode = a_mode;
		logger::info("move: steering mode {}", g_mode);
	}

	void FaceCamera(RE::PlayerCharacter* a_player)
	{
		auto* tps = Tps();
		if (!tps || !tps->freeRotationEnabled) {
			return;
		}
		a_player->SetHeading(a_player->GetAngleZ() + tps->freeRotation.x);
		tps->freeRotation.x = 0.0f;
	}

	void Reset()
	{
		StopOverriding();
		g_keys = {};
		g_stick = {};
	}
}
