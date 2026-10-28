#pragma once

namespace protean::Journal
{
	// A "Protean" page in SKSE Menu Framework (optional: without the mod there is no page): the evolution count, the
	// perk points, the switch "Take the loot when consuming" (the user, 2026-10-28) and the bodies consumed of each
	// kind (hearts until then; the user asked for a way to track them, 2026-10-26).

	void Register();                 // kPostLoad: every plugin is loaded, SKSEMenuFramework.dll among them
	void OnFrame(float a_delta);     // main thread: the page's copy of the numbers (it draws on the render thread)
}
