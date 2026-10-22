#include "Hud.h"

#include "Shift.h"

namespace protean::Hud
{
	namespace
	{
		using Clock = std::chrono::steady_clock;

		constexpr auto kEvery = std::chrono::milliseconds(100);

		Clock::time_point g_next{};

		double Num(const RE::GFxValue& a_obj, const char* a_name)
		{
			RE::GFxValue v;
			return a_obj.GetMember(a_name, &v) && v.IsNumber() ? v.GetNumber() : 0.0;
		}

		// Keep fading the bar in, as the game does on a health change (HUDMenu.RunMeterAnim).
		void Show()
		{
			auto* ui = RE::UI::GetSingleton();
			auto  hud = ui ? ui->GetMenu<RE::HUDMenu>() : nullptr;
			auto* movie = hud ? hud->uiMovie.get() : nullptr;
			RE::GFxValue anim;
			if (movie && movie->GetVariable(&anim, "_root.HUDMovieBaseInstance.Health") && anim.IsDisplayObject()) {
				anim.Invoke("PlayForward", std::array<RE::GFxValue, 1>{ RE::GFxValue(Num(anim, "_currentframe")) });
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

		if (!Shift::ActiveCreature()) {
			return;
		}
		auto*       av = a_player->AsActorValueOwner();
		const float max = av->GetPermanentActorValue(RE::ActorValue::kHealth) +
		                  a_player->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth);
		if (max > 0.0f && av->GetActorValue(RE::ActorValue::kHealth) < max - 0.5f) {
			SKSE::GetTaskInterface()->AddUITask([] { Show(); });
		}
	}
}
