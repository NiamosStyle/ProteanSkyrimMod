# Flight research (for the Chaurus Hunter and Fledgling) - 2026-10-19, offline

Decision already made by the user: **build real flight**. This note is what the research says, so the Phase 4
plan can be honest about cost.

## What the existing mods do

| Mod | Method | Notes for us |
|---|---|---|
| Vampire Lords Can Fly (46023, 2021) | "Falling with style": Better Jumping SE lets you jump very high and jump again in the air; fall damage off; the falling animation is swapped for the hovering one. No scripts. | Simple and robust, but needs Better Jumping SE (not installed; 1.7.104 support unverified). |
| True Dragon Aspect (142635, v1.0.8) | Same Better Jumping method, wrapped in a dragon transformation. Jump = fly, Sneak = descend, Toggle-Run = hover/cruise. Takes over Better Jumping. | The user may also use this mod, so we must not fight it for Better Jumping. Its control scheme is a good template. |
| WingFlight (194587, v0.1, uploaded the day of this research) | A real SKSE flight system: wingbeats, auto-hover, high-speed flight. Needs Behavior Data Injector, Open Animation Replacer, Flying Animations - Resource. Supports 1.7.104. Incompatible with other flight mods. | Built around humanoid wing skeletons (vampire lord, wings mods). Brand new; not a dependency I would rely on. |
| Flying Mod Beta (13905) | Old Papyrus + FNIS. | Not suitable. |

Permissions were not retrievable from the API for these (the page text carries no explicit permission
block); before reusing any code or asset from them we must read their Nexus permission tab by hand. My plan
copies no code, only the control ideas.

## The Chaurus Hunter specifically

I read its animation graph (`DLC01/ChaurusFlyer/behaviors/chaurusflyerbehavior.hkx`). It has idle, wing idle,
combat idle, `Attack_GroundDive`, `Attack_Spit`, power attack, pod-ambush falling/landing, and sleeping
animations. **It has no flight or hover locomotion at all**; in vanilla the Hunter walks and dives. The race
record has no "Flies" flag either. So real flight needs two separate things:

1. **Physics (we can build this):** an SKSE plugin that, while transformed into a flying animal, controls the
   player's character controller: takeoff on Jump, hold to climb, Sneak to descend, forward along the camera
   when moving, no fall damage, landing on touch-down. The Portal Spheres project already measured how the
   character controller behaves (see `KNOWLEDGEBASE.local.md`: false "on ground" after a move, in-air steering
   eats horizontal velocity, set velocities need re-applying each frame). Same tools, new use.
2. **Animation (the hard part):** there is no flying animation to play. Options:
   - **A. Reuse what exists** (recommended first try): while airborne, replace the falling/jump loop with the
     Hunter's `IdleWings` loop, and use `Attack_GroundDive` for a dive. Done with Open Animation Replacer, which
     is already installed. It will look like "hovering with fluttering wings", not a true flight cycle.
   - **B. Borrow the Flying Animations - Resource pack** (installed): made for humanoid wing rigs, so it will
     not fit a Chaurus skeleton without retargeting. Not recommended.
   - **C. Author a new animation** in a Havok pipeline: not available in this toolkit; out of scope.

## Recommendation
Build flight as our own C++ code (no extra dependencies, no conflict with True Dragon Aspect), driven by the
same per-frame hook the camera uses, with animation option A. Cost: medium for the physics, uncertain for the
look. Do it last (Phase 4), after the shared systems are proven.

## Open questions for the user (for Phase 4)
- Is "hovering with fluttering wings, diving with the ground-dive animation" an acceptable first version?
- Should flight work indoors? (Vampire Lords Can Fly allows it.) The size rule currently keeps big animals out
  of buildings; the Hunter is medium, so it would be allowed.
