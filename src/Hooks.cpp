#include "Hooks.h"

#include "Attacks.h"
#include "Camera.h"
#include "Dev.h"
#include "Shift.h"

namespace protean::Hooks
{
	namespace
	{
		struct PlayerUpdate
		{
			static void thunk(RE::PlayerCharacter* a_this, float a_delta)
			{
				func(a_this, a_delta);
				Camera::OnFrame(a_this);
				Attacks::OnFrame();
				Shift::OnFrame();
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
