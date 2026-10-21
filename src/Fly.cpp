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

		// The wild hunter's wing buzz plays only from its idle animation, so a flying player was nearly silent:
		// loop it while flying.
		constexpr auto    kWings = "NPCChaurusInsectIdleWingsLP";  // Dawnguard's looping wing sound (editor ID)
		RE::BSSoundHandle g_wings;
		bool              g_wingsStarted{ false };  // once per flight: a just-started sound is not "playing" yet,
		                                            // and restarting it every frame stacked dozens of copies (heard)

		void Wings(RE::PlayerCharacter* a_player, bool a_on)
		{
			if (!a_on) {
				if (g_wings.IsValid()) {
					g_wings.FadeOutAndRelease(300);
				}
				g_wings = {};
				g_wingsStarted = false;
				return;
			}
			if (g_wingsStarted) {
				return;
			}
			g_wingsStarted = true;
			auto* audio = RE::BSAudioManager::GetSingleton();
			if (!audio) {
				return;
			}
			audio->GetSoundHandleByName(g_wings, kWings, 0x1A);
			if (!g_wings.IsValid()) {
				static bool warned = false;
				if (!warned) {
					warned = true;
					logger::warn("fly: no sound '{}'", kWings);
				}
				return;
			}
			g_wings.SetObjectToFollow(a_player->Get3D());
			g_wings.Play();
		}

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

		// ---- leaping (the ash hopper: user decision) ----------------------------------------------
		// Its own leaping lunges are refused for the player (probed: standing, running, bSkeeverLunge on), so
		// Jump throws it along an arc: the velocity is set every frame from our own gravity, as in flight,
		// until it lands. The engine's in-air steering would otherwise damp the run (Portal Spheres).
		constexpr float kLeapGravity = 1000.0f;  // game units / s^2: snappier than falling, a hopper's spring

		struct Leap
		{
			bool                  on{ false };
			Clock::time_point     since{};
			RE::NiPoint3          dir{};      // horizontal, unit
			float                 speed{ 0.0f };
			float                 vz0{ 0.0f };
			float                 apex{ 0.0f };  // highest z: a fall past the landing hurts from there
			float                 gravity{ 1.0f };
			RE::NiPoint3          lastPos{};
			float                 airtime{ 0.0f };  // seconds to land back at the starting height
		};
		Leap  g_leap;
		float g_leapScale{ 1.0f };   // dev: multiplies distance
		float g_leapHScale{ 1.0f };  // dev: multiplies height

		void StartLeap()
		{
			auto*       p = RE::PlayerCharacter::GetSingleton();
			const auto* c = Shift::ActiveCreature();
			auto*       cc = p ? p->GetCharController() : nullptr;
			if (g_leap.on || !c || c->leap <= 0.0f || !cc || p->IsDead() || !cc->flags.all(RE::CHARACTER_FLAGS::kSupport)) {
				return;
			}
			Move::FaceCamera(p);
			const float h = std::max(c->leapHeight * g_leapHScale, 10.0f);
			const float vz0 = std::sqrt(2.0f * kLeapGravity * h);
			const float airtime = 2.0f * vz0 / kLeapGravity;
			const float yaw = p->GetAngleZ();
			g_leap = { true, Clock::now(), { std::sin(yaw), std::cos(yaw), 0.0f }, c->leap * g_leapScale / airtime, vz0, p->GetPositionZ(),
				cc->gravity, p->GetPosition(), airtime };
			logger::info("leap: {:.0f} units, {:.0f} high: {:.0f} u/s ahead, {:.0f} u/s up, {:.2f} s", c->leap * g_leapScale, h, g_leap.speed, vz0, airtime);
		}

		void EndLeap(RE::PlayerCharacter* a_p, const char* a_why)
		{
			if (auto* cc = a_p->GetCharController()) {
				cc->gravity = g_leap.gravity;
				cc->fallStartHeight = g_leap.apex;
			}
			logger::info("leap: {} after {:.2f} s", a_why, std::chrono::duration<float>(Clock::now() - g_leap.since).count());
			g_leap = {};
		}

		void UpdateLeap(RE::PlayerCharacter* a_p, float a_delta)
		{
			if (!g_leap.on) {
				return;
			}
			auto* cc = a_p->GetCharController();
			if (!cc || a_p->IsDead() || !Shift::ActiveCreature()) {
				g_leap = {};
				return;
			}
			const float t = std::chrono::duration<float>(Clock::now() - g_leap.since).count();
			const auto  pos = a_p->GetPosition();
			g_leap.apex = std::max(g_leap.apex, pos.z);
			if (t > 0.2f && cc->flags.all(RE::CHARACTER_FLAGS::kSupport)) {
				EndLeap(a_p, "landed");
				return;
			}
			if (t > 6.0f) {
				EndLeap(a_p, "still falling: left to the game");
				return;
			}
			// a wall: the run stops, the fall goes on
			if (t > 0.15f && a_delta > 0.0001f && g_leap.speed > 0.0f) {
				const auto  step = (pos - g_leap.lastPos) / a_delta;
				const float ahead = step.x * g_leap.dir.x + step.y * g_leap.dir.y;
				if (ahead < 0.3f * g_leap.speed) {
					logger::info("leap: blocked ({:.0f} of {:.0f} u/s)", ahead, g_leap.speed);
					g_leap.speed = 0.0f;
				}
			}
			g_leap.lastPos = pos;
			cc->gravity = 0.0f;
			cc->context.currentState = RE::hkpCharacterStateType::kInAir;
			const float vz = g_leap.vz0 - kLeapGravity * t;
			const float s = RE::bhkWorld::GetWorldScale();
			cc->SetLinearVelocityImpl(RE::hkVector4(g_leap.dir.x * g_leap.speed * s, g_leap.dir.y * g_leap.speed * s, vz * s, 0.0f));
		}

		class Sink : public RE::BSTEventSink<RE::InputEvent*>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) override
			{
				const auto* c = a_event ? Shift::ActiveCreature() : nullptr;
				if (!c || g_flying || (!c->flies && c->leap <= 0.0f)) {
					return RE::BSEventNotifyControl::kContinue;
				}
				const auto* ue = RE::UserEvents::GetSingleton();
				for (auto* e = *a_event; e; e = e->next) {
					if (auto* b = e->AsButtonEvent(); b && b->userEvent == ue->jump && b->IsDown()) {
						if (c->flies) {
							SKSE::GetTaskInterface()->AddTask([] { TakeOff(); });
						} else {
							SKSE::GetTaskInterface()->AddTask([] { StartLeap(); });
						}
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

	// The legs hold still only until just before touching down: stopping them until landing made the animal
	// stand still a moment before it could run on (user).
	bool Leaping()
	{
		constexpr float kLegsBefore = 0.3f;  // seconds before the arc's landing that the run picks up again
		return g_leap.on && std::chrono::duration<float>(Clock::now() - g_leap.since).count() < g_leap.airtime - kLegsBefore;
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
		Wings(p, false);
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
		UpdateLeap(a_player, a_delta);
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

		Wings(a_player, true);
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

	void SetLeapTuning(float a_distance, float a_height)
	{
		if (a_distance > 0.0f) {
			g_leapScale = a_distance;
		}
		if (a_height > 0.0f) {
			g_leapHScale = a_height;
		}
		logger::info("leap: distance x{:.2f} height x{:.2f}", g_leapScale, g_leapHScale);
		StartLeap();
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
