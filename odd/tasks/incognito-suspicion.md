# Incognito with suspicion and reinforcements

## Objective
Standalone, reusable incognito system for Arma Reforger missions: the disguise rules proven in
A La Deriva, plus an invisible suspicion meter and optional reinforcements, packaged in the
`arga_incognito` addon so any mission can depend on it.

## Problem
The disguise system (`ARGA_DisguiseComponent`) lives inside the A La Deriva mission and breaks
the disguise instantly on any rule. It cannot be reused, and it has no tolerance for minor
mistakes and no reaction beyond the faction switch.

## Why
Ideas taken from the Incognito-Continuatio mod (INCON), whose own code runs client-side only and
is never wired to any entity, rebuilt on top of our server-authoritative, dedicated-verified base.

## Constraints
- Do NOT modify A La Deriva in any way. Code is copied, not moved.
- Do not modify code outside the `arga_incognito` addon.
- Server-authoritative; must work on a dedicated server with remote players.
- Suspicion is fully invisible to the player: no UI, no hint, no AI behaviour change before a break.
- Avoid clashing with A La Deriva if both addons are ever loaded together: distinct class names and
  distinct script file paths (Enfusion overrides files that share a path across addons).

## Design (agreed 2026-09-23)
- Instant-break rules: shoot, kill, voice.
- Suspicion-accumulating rules (only when a hostile AI sees the player, more when closer): sprint,
  aim / weapon raised, proximity.
- Suspicion 0-100 per player, server-only. At 100 the disguise breaks.
- Recovery: suspicion decays while unseen (faster when far from witnesses); the disguise returns only
  below a threshold (hysteresis), replacing the fixed recovery time.
- Discarded: corpse discovery (bodies cannot be moved), weapon-in-hand rule, INCON gear inspection,
  INCON weapon-fired danger reaction, any player-facing suspicion feedback.
- Reinforcements (own attribute category, enabled by the mission editor): group prefab spawned at an
  editor-set distance and bearing from the break point, moves to the break point. Despawns a set time
  after the disguise is restored (never in sight of a player). A new break within a reuse radius
  re-tasks the existing group instead of spawning another. Cap on simultaneous groups.

## Tasks
- [x] T0 - Copy `ARGA_DisguiseComponent` and its VON bridge into this addon as
      `ARGA_IncognitoComponent`, with no behaviour change. Rename classes and log tag only.
- [x] T0b - Prefab / world entity that hosts the component, and a dedicated-server re-run of the
      12/09 checks (sprint, aim, proximity, voice).
- [ ] T1 - Suspicion meter: instant-break vs accumulating rules, decay, recovery threshold.
- [ ] T2 - Reinforcements: spawn at distance and bearing, move to break point.
- [ ] T3 - Reinforcements: timed despawn out of sight, reuse radius, group cap.

## TDD
Strict TDD is enabled globally, but this addon has no Enforce Script test runner. Checks are manual:
the user runs Workbench / play / dedicated-server tests and the agent reads the logs.

## Delivery
Strategy: ask-on-risk. Branch `feat/incognito-component`.

## Progress
- T0: route direct inline (2 files, mechanical copy + rename). Commit `6b4294b`. Word diff against
  the A La Deriva source shows only the renames; A La Deriva files untouched (mtime 2026-09-12).
  `mod_validate` passed (structure, gproj, scripts, references). NOT compiled yet: Workbench NET API
  did not respond. Review assess: medium, review due; native review blocked at the untracked-files
  selection step (`addon.gproj` is untracked), so the commit is unreviewed.

- T0b (in progress): test world `Worlds/TestIncognito.ent` (Arland). Run 11:09 log: `Armed on server`,
  `Watching playerId=1`, `Initialized ... perceived=ARGA disguise=DEFAULT_FACTION` (no disguise yet).
  Found: GameMode_ARGA leaves `m_bPerceivedFactionChangesAffectsAI` at vanilla default 0
  (`SCR_PerceivedFactionManagerComponent.c:20`), so AI would ignore disguises; overridden in the
  test layer to 1 + `HIGHEST_VALUE`, as A La Deriva does. Test player: prefab
  `Prefabs/Characters/Incognito/ARGA_Incognito_Character_USSR_Rifleman.et` (vanilla USSR rifleman,
  faction ARGA) as the only loadout; enemy: `Group_USSR_LightFireTeam` in `C_EnemyFaction.layer`.
  Missions using this addon must enable that perceived-faction flag themselves.

- T0b closed (Workbench play, log `logs_2026-09-23_10-58-20`, run from 11:28): `perceived=USSR
  disguise=HOSTILE_FACTION`, `AI override applied: AI now perceives USSR`; breaks observed for
  sprinting (22 m), proximity (6.3 m and 9.6 m), aiming (4.5 m); `Restored` after 30 s unseen. Shot,
  kill, voice and the behind-a-wall case were not re-run: the code is a rename-only copy of the
  version verified in A La Deriva, so the user accepted partial coverage. No script errors from the
  addon. Side effect seen: after a break the AI moves to the spawn, and a respawned player breaks by
  proximity about 10 s later (spawn camping; a test-world artefact).

## Next step
T1: suspicion meter.
