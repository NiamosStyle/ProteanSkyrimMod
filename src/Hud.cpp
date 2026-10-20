#include "Hud.h"

#include "Shift.h"

namespace protean::Hud
{
	namespace
	{
		using Clock = std::chrono::steady_clock;

		constexpr auto        kEvery = std::chrono::milliseconds(100);
		constexpr const char* kMeter = "_root.HUDMovieBaseInstance.Health.HealthMeter_mc";
		constexpr std::int32_t kTickDepth = 16000;  // above the bar's own clips
		constexpr std::uint32_t kTickColour = 0xFFFFFF;

		Clock::time_point g_next{};
		bool              g_shown{ false };  // ticks or tint are on the bar now

		struct Bar
		{
			const void* movie{ nullptr };  // the HUD movie the geometry was measured in
			double      centre{ 0.0 };
			double      half{ 0.0 };        // half the full bar's width
			double      top{ 0.0 };
			double      bottom{ 0.0 };
		};
		Bar g_bar;

		double Num(const RE::GFxValue& a_obj, const char* a_name)
		{
			RE::GFxValue v;
			return a_obj.GetMember(a_name, &v) && v.IsNumber() ? v.GetNumber() : 0.0;
		}

		// The meter is a timeline animation (frame "Full" .. frame "Empty"), shrinking from both ends to the middle.
		// Measure the full bar once per HUD movie: jump to "Full", read its bounds, jump back.
		bool Measure(RE::GFxMovieView* a_movie, RE::GFxValue& a_meter)
		{
			if (g_bar.movie == a_movie) {
				return true;
			}
			RE::GFxValue fill;
			if (!a_meter.GetMember("HealthLeft", &fill) || !fill.IsDisplayObject()) {
				return false;
			}
			const double frame = Num(fill, "_currentframe");
			fill.Invoke("gotoAndStop", std::array<RE::GFxValue, 1>{ RE::GFxValue("Full") });
			RE::GFxValue bounds;
			fill.Invoke("getBounds", &bounds, std::array<RE::GFxValue, 1>{ a_meter });
			fill.Invoke("gotoAndStop", std::array<RE::GFxValue, 1>{ RE::GFxValue(frame) });
			if (!bounds.IsObject()) {
				return false;
			}
			const double x0 = Num(bounds, "xMin"), x1 = Num(bounds, "xMax");
			g_bar = { a_movie, (x0 + x1) * 0.5, (x1 - x0) * 0.5, Num(bounds, "yMin"), Num(bounds, "yMax") };
			logger::info("hud: health bar x {:.1f}..{:.1f} y {:.1f}..{:.1f}", x0, x1, g_bar.top, g_bar.bottom);
			return g_bar.half > 0.0;
		}

		RE::GFxValue Tick(RE::GFxValue& a_holder, const char* a_name)
		{
			RE::GFxValue t;
			if (a_holder.GetMember(a_name, &t) && t.IsDisplayObject()) {
				return t;
			}
			a_holder.CreateEmptyMovieClip(&t, a_name);
			const double pad = 2.0;
			t.Invoke("lineStyle", std::array<RE::GFxValue, 3>{ RE::GFxValue(2.0), RE::GFxValue(static_cast<double>(kTickColour)), RE::GFxValue(100.0) });
			t.Invoke("moveTo", std::array<RE::GFxValue, 2>{ RE::GFxValue(0.0), RE::GFxValue(g_bar.top - pad) });
			t.Invoke("lineTo", std::array<RE::GFxValue, 2>{ RE::GFxValue(0.0), RE::GFxValue(g_bar.bottom + pad) });
			return t;
		}

		// a_line = the danger line as a share of the full bar (0 = hide everything); a_danger = health is below it;
		// a_hurt = health is below its maximum
		void Draw(double a_line, bool a_danger, bool a_hurt)
		{
			auto* ui = RE::UI::GetSingleton();
			auto  hud = ui ? ui->GetMenu<RE::HUDMenu>() : nullptr;
			auto* movie = hud ? hud->uiMovie.get() : nullptr;
			RE::GFxValue meter;
			if (!movie || !movie->GetVariable(&meter, kMeter) || !meter.IsDisplayObject() || !Measure(movie, meter)) {
				return;
			}

			// The game fades the bar out once health reaches the maximum without the animal's bonus (measured: the
			// bar vanished part-filled), hiding how far there is to heal. Keep fading it in, as the game does on a
			// health change (HUDMenu.RunMeterAnim).
			RE::GFxValue anim;
			if (a_hurt && movie->GetVariable(&anim, "_root.HUDMovieBaseInstance.Health") && anim.IsDisplayObject()) {
				anim.Invoke("PlayForward", std::array<RE::GFxValue, 1>{ RE::GFxValue(Num(anim, "_currentframe")) });
			}

			RE::GFxValue fill;
			if (meter.GetMember("HealthLeft", &fill) && fill.IsDisplayObject()) {
				RE::GRenderer::Cxform cx;  // identity
				if (a_danger) {
					// the red bar to orange: keep red, lift green (the add terms are 0..255)
					cx.matrix[RE::GRenderer::Cxform::kG][RE::GRenderer::Cxform::kAdd] = 120.0f;
					cx.matrix[RE::GRenderer::Cxform::kB][RE::GRenderer::Cxform::kMult] = 0.3f;
				}
				fill.SetCxform(cx);
			}

			RE::GFxValue holder;
			if (!meter.GetMember("proteanTicks", &holder) || !holder.IsDisplayObject()) {
				meter.CreateEmptyMovieClip(&holder, "proteanTicks", kTickDepth);
			}
			if (!holder.IsDisplayObject()) {
				return;
			}
			holder.SetMember("_visible", RE::GFxValue(a_line > 0.0));
			if (a_line > 0.0) {
				auto l = Tick(holder, "left");
				auto r = Tick(holder, "right");
				l.SetMember("_x", RE::GFxValue(g_bar.centre - g_bar.half * a_line));
				r.SetMember("_x", RE::GFxValue(g_bar.centre + g_bar.half * a_line));
			}
		}
	}

	void OnFrame(RE::PlayerCharacter* a_player)
	{
		const auto now = Clock::now();
		if (now < g_next || !a_player) {
			return;
		}
		g_next = now + kEvery;

		double line = 0.0;
		bool   danger = false;
		bool   hurt = false;
		if (Shift::ActiveCreature()) {
			auto*       av = a_player->AsActorValueOwner();
			const float max = av->GetPermanentActorValue(RE::ActorValue::kHealth) +
			                  a_player->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth);
			const float deadly = Shift::RevertKillsBelow();
			if (max > 0.0f && deadly > 0.0f) {
				line = std::clamp(static_cast<double>(deadly / max), 0.001, 1.0);
				const float hp = av->GetActorValue(RE::ActorValue::kHealth);
				danger = hp <= deadly;
				hurt = hp < max - 0.5f;
			}
		}
		if (line == 0.0 && !g_shown) {
			return;
		}
		g_shown = line > 0.0;
		SKSE::GetTaskInterface()->AddUITask([line, danger, hurt] { Draw(line, danger, hurt); });
	}
}
