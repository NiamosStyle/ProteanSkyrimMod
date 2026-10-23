#include "Hooks.h"

#include "Evolve.h"
#include "Face.h"

#include "Attacks.h"
#include "Camera.h"
#include "Claws.h"
#include "Dev.h"
#include "Fly.h"
#include "Hud.h"
#include "Keen.h"
#include "Move.h"
#include "Flight.h"
#include "Rush.h"
#include "Shift.h"
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
				Flight::OnFrame(a_this, a_delta);
				Camera::OnFrame(a_this);
				Attacks::OnFrame();
				Shift::OnFrame();
				Evolve::OnFrame(a_delta);
				Claws::OnFrame(a_delta);
				Face::OnFrame();
				Hud::OnFrame(a_this);
				Dev::Poll();
			}
			static inline REL::Relocation<decltype(thunk)> func;
			static constexpr std::size_t                   idx = 0xAD;  // Actor::Update, SE/AE
		};
	}

	void Install()
	{
		REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_PlayerCharacter[0] };
		PlayerUpdate::func = vtbl.write_vfunc(PlayerUpdate::idx, PlayerUpdate::thunk);
		logger::info("hooked PlayerCharacter::Update");
	}
}
