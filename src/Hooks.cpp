#include "Hooks.h"

#include "Evolve.h"
#include "Feed.h"
#include "Face.h"

#include "Attacks.h"
#include "Camera.h"
#include "Carapace.h"
#include "Claws.h"
#include "Dev.h"
#include "Fly.h"
#include "Journal.h"
#include "Hud.h"
#include "Keen.h"
#include "Move.h"
#include "Flight.h"
#include "Rush.h"
#include "Shift.h"
#include "Skills.h"
#include "Stalker.h"
#include "Tendril.h"

namespace protean::Hooks
{
	namespace
	{
		struct PlayerUpdate
		{
			static void thunk(RE::PlayerCharacter* a_this, float a_delta)
			{
				Move::BeforeUpdate(a_this, a_delta);
				func(a_this, a_delta);
				Move::AfterUpdate(a_this);
				Fly::AfterUpdate(a_this, a_delta);
				Tendril::OnFrame(a_this, a_delta);
				Keen::OnFrame(a_this, a_delta);
				Rush::OnFrame(a_this, a_delta);
				Carapace::OnFrame(a_this, a_delta);
				Feed::OnFrame(a_this, a_delta);
				Stalker::OnFrame(a_this, a_delta);
				Skills::OnFrame(a_this, a_delta);
				Flight::OnFrame(a_this, a_delta);
				Flight::KeepFall(a_this, a_delta);
				Camera::OnFrame(a_this);
				Attacks::OnFrame();
				Shift::OnFrame();
				Evolve::OnFrame(a_delta);
				Journal::OnFrame(a_delta);
				Claws::OnFrame(a_delta);
				Face::OnFrame();
				Hud::OnFrame(a_this);
				Dev::Poll();
			}
			static inline REL::Relocation<decltype(thunk)> func;
			static constexpr std::size_t                   idx = 0xAD;  // Actor::Update, SE/AE
		};

		// a body being searched: its heart goes in first, so the loot window shows it. Added at the death instead, a
		// body still falling went stiff and could no longer be searched (testing 2026-10-24); added on the activate
		// event, the window was already filled and showed it only the second time
		struct NpcActivate
		{
			static bool thunk(RE::TESNPC* a_this, RE::TESObjectREFR* a_target, RE::TESObjectREFR* a_activator, std::uint8_t a_arg3,
				RE::TESBoundObject* a_object, std::int32_t a_count)
			{
				if (a_activator && a_activator->IsPlayerRef() && a_target) {
					if (auto* corpse = a_target->As<RE::Actor>(); corpse && corpse->IsDead()) {
						Shift::AddHeart(corpse);
					}
				}
				return func(a_this, a_target, a_activator, a_arg3, a_object, a_count);
			}
			static inline REL::Relocation<decltype(thunk)> func;
			static constexpr std::size_t                   idx = 0x37;  // TESForm::Activate
		};

		// the frame's animation applied to the skeleton: a bone turned before this is overwritten
		struct PlayerUpdateAnimation
		{
			static void thunk(RE::PlayerCharacter* a_this, float a_delta)
			{
				func(a_this, a_delta);
				Flight::AfterAnimation(a_this);
			}
			static inline REL::Relocation<decltype(thunk)> func;
			static constexpr std::size_t                   idx = 0x7D;  // TESObjectREFR::UpdateAnimation
		};
	}

	void Install()
	{
		REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_PlayerCharacter[0] };
		PlayerUpdate::func = vtbl.write_vfunc(PlayerUpdate::idx, PlayerUpdate::thunk);
		PlayerUpdateAnimation::func = vtbl.write_vfunc(PlayerUpdateAnimation::idx, PlayerUpdateAnimation::thunk);
		logger::info("hooked PlayerCharacter::Update and UpdateAnimation");
		REL::Relocation<std::uintptr_t> npc{ RE::VTABLE_TESNPC[0] };
		NpcActivate::func = npc.write_vfunc(NpcActivate::idx, NpcActivate::thunk);
		logger::info("hooked TESNPC::Activate");
	}
}
