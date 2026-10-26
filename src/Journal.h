#pragma once

namespace protean::Journal
{
	// A "Protean" page in SKSE Menu Framework (optional: without the mod there is no page): the evolution count, the
	// perk points, and the hearts eaten of each kind with the points they gave and the next one that pays (the user
	// asked for a way to track hearts, 2026-10-26).

	void Register();                 // kPostLoad: every plugin is loaded, SKSEMenuFramework.dll among them
	void OnFrame(float a_delta);     // main thread: the page's copy of the numbers (it draws on the render thread)
}
