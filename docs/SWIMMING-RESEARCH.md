# Slaughterfish / swimming research - 2026-10-19, offline

User decision: **real swimming** for the Slaughterfish, including diving and surfacing; it **must not leave the
water onto land**; its power can only be used in water deep enough to swim; the Horker stays amphibious.

## What the game data gives us
- Race `SlaughterfishRace` has only the **Swims** flag (no Walks), health base 35, stamina 25. Vanilla fish
  never leave water because their AI keeps them there, not because the engine forbids it.
- The animation graph is complete for swimming: forward, fast forward, backward, left/right 45/90/180, looping
  turns, idle (combat and non-combat), stagger, and four attacks (`AttackStart_LCross`, `_LSlash`, `_RCorss`
  [sic, the typo is in the game data], `_RUppercut`). **No ground locomotion exists.**
- The graph has `Direction` and `Speed` variables and directional blending; no pitch variable. Vertical
  movement underwater therefore comes from the engine's own swim movement (as for the player swimming), not
  from the animation.

## What we do not know (needs a live probe, first thing in Phase 4)
1. What the engine does with a player whose race has Swims-only on dry land. Likely candidates: stuck, slides
   without animation, or falls through the missing walk state. A one-minute spike with the dev channel
   (`transform slaughterfish` standing on a shore) answers it.
2. Whether player diving (look down + move forward) works in a fish race the same way as in a normal race.

## Plan (to be approved in Phase 4)
- **Entering the form:** refuse unless the player is swimming (`Actor::IsSwimming`) with water deeper than a
  threshold; message "You need deep water." (Same pattern as the indoor size rule, already built.)
- **Staying in water:** every frame, if the player is no longer swimming and is near the shore, cancel the
  horizontal velocity and nudge back toward the water ("invisible wall at the beach"); if they somehow end up on
  land anyway, auto-revert after a short grace time (same auto-revert path as the size rule). Existing swimming
  mods were not needed for this; the character controller knowledge from Portal Spheres applies.
- **Attacks:** the four fish attack events go through the same attack-button translation as every other animal.
- **Camera:** the generic node-on-the-model camera works underwater too; underwater tint/fog is the engine's.

## Risk
Medium. The animal data is complete, so the unknown is only the engine's land behavior, which the spike settles
before any feature work. Fallback if land is unworkable: keep the player in the water with the auto-revert only.
