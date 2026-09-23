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
- [x] T1 - Suspicion meter: instant-break vs accumulating rules, decay, recovery threshold.
- [x] T2 - Reinforcements: spawn at distance and bearing, move to break point.
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

- T1 (implemented, pending manual test): route direct inline (1 file). Sprint / aim / proximity add
  suspicion per second while an observer sees the player (rates 40 / 50 / 25, full at point blank,
  half at the rule's radius edge; per rule the strongest observer counts, rules add up). Break at 100
  with reason `suspicion:<rule>`. Shot, kill and voice still break at once and set suspicion to 100.
  Decay 2.5/s when nothing suspicious happens this tick, doubled with no hostile AI within the witness
  radius (distance only, no ray). While broken: held while a hunter sees the player, decays otherwise,
  restored at 25 (replaces `m_fRecoverySeconds`). Debug log prints every 10 points crossed.
  Behaviour note: the sight rules no longer stop at the first observer, so every observer in range
  and cone costs a ray.

- T1 test (log `logs_2026-09-23_11-37-48`, two runs): short sprint rose to 28.5 without breaking and
  decayed at 2.5/s; continuous sprint broke in ~3.5 s (`suspicion:sprinting`, witness 36 m); aiming
  broke in ~3 s (`suspicion:aiming`, 21.6 m); proximity broke in ~6 s (`suspicion:proximity`, 3.3 m);
  while broken suspicion held when seen, then decayed 100 -> 25 in ~30 s and `Restored` fired twice.
  Voice run: ranges resolved 5 / 30 / 68 m; whisper and normal did not break (enemies ~60 m away),
  loud broke at once (`reason=voice`, 61.2 m). No script errors from the addon.
  Test-world fix: the first run spawned vanilla `Character_USSR_Rifleman_Variant_1.et`, because
  `SCR_PlayerLoadout` picks a random editable-entity variant (`SCR_PlayerLoadout.c:16`) and the
  prefab inherited the rifleman's variant list. The inherited variant now points at the prefab itself.

- T2 (implemented, pending manual test): route direct inline (1 script + test layer). New category
  "Reinforcements": enable flag, group prefab, distance (200), bearing (0 = north), waypoint prefab
  (vanilla SearchAndDestroy) and its completion radius (30). On every break the group spawns at
  breakPos + bearing * distance on the terrain surface and gets the waypoint at breakPos. No reuse,
  despawn or cap yet (T3), so every break spawns a new group. No check that the spawn point is clear
  of buildings or water. Test layer: USSR light fireteam at 150 m north.

- T2 test (log `logs_2026-09-23_11-56-31`): break `suspicion:sprinting` at 11:58:26 and
  `Reinforcements spawned` in the same tick, exactly 150 m north of the break point (2678.9, 1697.7 ->
  2678.9, 1847.7). The user saw the group arrive from the north and search the area; at `Restored`
  (11:59:21) the nearest hunter stood 5 m from the break point. No addon errors. The player also
  spawned with the pinned prefab, confirming the variant fix. The user then switched the test group
  to `Group_USSR_SentryTeam_NI` (naval infantry) to tell it apart from the local fireteam.

- T3 (implemented, pending manual test): route direct inline (1 script + test layer). Spawned groups
  are tracked. A break re-tasks the closest live group when it is within the reuse radius (300 m), or
  whatever its distance once the cap (2) is reached; otherwise it spawns a new one. Re-tasking swaps
  the waypoint and cancels the despawn timer. Any broken disguise holds all groups; once none is
  broken, each group waits the despawn time (120 s; 30 s in the test layer) and is deleted, members
  first as vanilla does (`SCR_AIGroup.c:2904-2911`), only when no player within 300 m sees any member.
  Dead or deleted groups are pruned after a 10 s spawn grace.

## Next step
T3: manual test; the agent reads the log.
