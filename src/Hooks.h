#pragma once

namespace protean::Hooks
{
	// vtable hook on PlayerCharacter::Update (Actor::Update, SE/AE index 0xAD), same pattern as Portal
	// Spheres: one cheap per-frame callback on the main thread for everything that needs a tick.
	void Install();
}
