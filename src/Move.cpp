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

		void StopOverriding()
		{
			if (g_overriding) {
				RE::PlayerControls::GetSingleton()->data.moveInputVec = { 0.0f, 0.0f };
				g_overriding = false;
			}
		}
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
		if (!Shift::ActiveCreature() || !tps || (ui && ui->GameIsPaused())) {
			StopOverriding();
			return;
		}
		tps->freeRotationEnabled = true;  // the mouse turns the camera; the animal turns to where it is steered
		const auto in = Input();
		const float len = std::sqrt(in.x * in.x + in.y * in.y);
		if (len < 0.1f) {
			StopOverriding();
			return;
		}
		const float heading = a_player->GetAngleZ();
		const float camYaw = heading + tps->freeRotation.x;
		const float want = camYaw + std::atan2(in.x, in.y);
		const float step = std::clamp(Wrap(want - heading), -kTurnRate * a_delta, kTurnRate * a_delta);
		const float now = heading + step;
		a_player->SetHeading(now);
		tps->freeRotation.x = Wrap(camYaw - now);  // the camera stays where it was looking
		RE::PlayerControls::GetSingleton()->data.moveInputVec = { 0.0f, std::min(len, 1.0f) };
		g_overriding = true;
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
