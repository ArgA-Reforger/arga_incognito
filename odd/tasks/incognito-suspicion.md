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
- [x] T4 - Target-aware shot and kill rules (decided 2026-09-23): attacking a faction
      that is an enemy of both the player and the disguise faction neither breaks nor adds
      suspicion. Kill breaks only if the victim is not an enemy of the outfit faction. Shot: trace
      along the weapon aim when firing; aimed at a non-enemy of the outfit -> break, even on a miss;
      aimed at an enemy of the outfit -> nothing; aimed at nothing -> adds suspicion. Aiming adds
      suspicion only while aimed at the disguise side (user, 2026-09-23).
- [x] T3 - Reinforcements: timed despawn out of sight, reuse radius, group cap.

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

- T3 test (log `logs_2026-09-23_11-56-31`, run from 12:08): three despawns 30.8-30.9 s after
  `Restored` (despawn time 30 s + one tick). Re-tasking instead of spawning at 12:13:00 (group 136 m
  away) and 12:14:06 (72.7 m), both `atCap=0`. Hold-while-seen shown indirectly: after the 12:13:31
  restore the group was still alive at 12:14:06, 35 s later, when the next break re-tasked it.
  Cap scenario (two groups, then a third break) NOT tested. No addon errors.

- Native review: `addon.gproj` committed (`37c6b82`); review started with untracked `Missions/` and
  `thumbnail.png` excluded, consent granted by the user, one lens (reliability), APPROVED and
  acknowledged (lineage `review-f327a0bd98064a66`, authority burned). Four non-blocking warnings:
  1. Voice radius cached by VON component class, not by .acp; a failed lookup caches 0 for good.
  2. `OnDelete` does not unsubscribe the game-mode / controller / shot handlers and leaves tracked
     reinforcement groups orphaned if the host entity is deleted.
  3. Cap branch never exercised.
  4. Shot and kill breaks not observed since `Break` gained suspicion and reinforcements.

- Review warning 2 fixed (pending manual test): `OnDelete` now removes the game-mode and controller
  subscriptions and the shot hooks, and deletes tracked reinforcements; `DeleteReinforcement` skips
  entities already being deleted (world shutdown). Warning 1 left as is (inherited design, low risk).
  Test layer reuse radius lowered to 50 m so the cap can be reached without walking 300 m.

- T4 (implemented, pending manual test): route direct inline (1 script + test layer). The aimed
  character is the alive AI closest to the weapon aim line (`ChimeraCharacter.GetWeaponAimingComponent`
  -> `GetAimingDirectionWorld`) within 5 deg, checked at hips, chest and head, in range (300 m) and
  with a clear line of fire; a cone instead of a hit test so misses aimed at someone still count.
  Targets classify as disguise side (outfit faction or its non-enemies), outfit enemy, or none.
  Shot: outfit enemy -> nothing; disguise side + witness -> break; none + witness -> +30 suspicion.
  Kill: victim an outfit enemy -> nothing. Aim: suspicion only while aimed at the disguise side.
  Only AI characters are candidates; players are not. Unverified: whether the weapon aim direction of
  a remote player is up to date on a dedicated server. Test layer: FIA fireteam ~100 m south of the
  spawn as the third party (no friendly-faction lists found in the vanilla or ArgA faction configs,
  so every pair of factions is hostile).

- T4 run A (log `logs_2026-09-23_12-43-26`, FIA moved ~1.4 km away): stray shots 30 -> 58 -> 86 ->
  100 and `suspicion:stray shot`; aiming at USSR broke twice in ~2.5 s; re-task at 42.6 m; shots at
  USSR broke at 12:50:13 and 12:51:45 (`target=1`); every stray shot was `aimed=NULL` and every shot
  at USSR picked the right soldier; Play stopped at 12:53 with a live group and no addon errors
  (warning 2 closed). Gap found: from 12:49:56 to 12:50:13 about 20 shots at one USSR broke nothing,
  because the victim was not facing the player and no other observer saw it; the victim then stopped
  moving (likely knocked unconscious, ACE medical). Kill break and the cap were not exercised.
- Fix (user decision): a shot's victim counts as witness when, 500 ms after the shot, he is still
  `ECharacterLifeState.ALIVE` (neither unconscious nor dead) and within the shot radius. Reason
  `shot:victim`.

- Run A2 (same log, from 13:05): `shot:victim` at 13:05:46 (victim 12 m, still conscious) and at
  13:10:18 against a naval infantry reinforcement; voice break at 50 m; re-task at 43 m. Edge case:
  at 13:07:39 the victim was facing the player (4.1 m), so he counted as an ordinary observer and the
  shot broke at once, before the bullet landed; a knock-out or kill of the only observer still gave
  the player away. Fix: the aimed victim is excluded from the immediate witness search and judged only
  by the delayed victim check. Kill break and the cap still not exercised (groups despawned between
  breaks). Test layer cap set to 1 so a single group reaches it.

- Run A3 (same log, from 13:18): aiming at a USSR facing the player broke `suspicion:aiming` before
  any shot (~2 s at close range), so no shot was judged; voice break at 30 m. The user accepts the
  aiming sensitivity as is. The user also judged the remaining cases not reproducible by hand, since
  the AI reacts on its own: one-shot take-out of a victim facing the player, the kill break (a shot
  in front of a witness always breaks first) and the group cap. They stay UNVERIFIED in play; covered
  only by code reading and the native review.

- Default reinforcement cap changed from 2 to 1 (user, 2026-09-23); mission editors will test raising
  it. Test layer no longer overrides the cap. FIA moved back ~100 m south of the spawn for run B.

- Run B (same log, from 14:05; the user moved FIA to 2607, 1649): the disguise broke at 14:06:25 with
  `suspicion:stray shot`. Sprinting under fire first raised suspicion 12 -> 81 (USSR observer ~58 m),
  then two shots at FIA came out `target=0 aimed=NULL` (FIA ~65 m away, lower and behind cover) and
  added 30 each. Both are normal behaviour in a firefight against a common enemy.
- Fixes (user decision): (1) target detection accepts a character when any body point inside the cone
  has a clear line of fire, and an enemy of the outfit hidden behind cover still counts as the target
  (suppressive fire); the outfit's own side counts only when visible. (2) Combat context: an observer
  whose `SCR_AIThreatSystem.GetState()` is ALERTED or THREATENED (`SCR_AIThreatSystem.c:5-11`,
  reached through `AIControlComponent.GetControlAIAgent()` -> `SCR_AIUtilityComponent.m_ThreatSystem`)
  adds no sprint suspicion and does not count as a stray-shot witness. Aiming at or shooting the
  outfit's own side is unaffected.

- Run B repeated (same log, from 14:14): five shots 14:14:42-53 still `target=0 aimed=NULL`, but no
  suspicion followed (no calm observer saw them). Then sprinting 14:15:03-06 in view of a USSR at
  ~48 m raised suspicion 15 -> 100 and broke `suspicion:sprinting`; the USSR then attacked. Open
  questions: why the shots at FIA found no target even with the hidden fallback, and which threat
  state the sprint witness had. Diagnostics added (debug log only): `Aim` line on untargeted shots
  (aim direction, closest AI to the aim line, cosine, distance, chest clear) and `threat=` on every
  break witness.

- Diagnostic run (same log, from 14:18): 17 shots at FIA 14:19:08-21 all `target=2` with the right
  FIA soldier, no suspicion, no break (third-party shooting verified). Sprint break at 14:19:30 with
  witness `threat=1` (VIGILANT): the USSR had heard the shots but FIA never engaged them, so they
  never reached ALERTED. User decision: the combat context now starts at VIGILANT.

- Run B (same log, from 14:22): 25 shots at FIA all `target=2`, no suspicion. Sprint then broke at
  14:23:21 with a witness at `threat=0` (SAFE) 35 s after the exchange, while FIA was shooting at the
  player (user). The USSR do not react to the player's fire (he looks friendly), and whether they
  heard FIA and calmed down again is not observable.
- Own-combat window (user decision): shooting at an outfit enemy, or an alive outfit enemy within the
  witness radius holding the player as an `ETargetCategory.ENEMY` perception target, opens a window
  (`m_fOwnCombatSeconds`, 30 s) in which sprinting and stray shots add no suspicion. Aiming at or
  shooting the outfit's own side is unaffected. Being engaged is read from the enemy's perception,
  not from actual shots or damage.

- Run B with own combat (same log, from 14:34): `Own combat cause=shot at outfit enemy` at 14:34:32;
  every later shot at FIA added nothing, including untargeted ones (one FIA at 27 m sat ~8 deg off the
  aim line, outside the 5 deg cone). The break came at 14:36:44 by `suspicion:proximity` from a USSR
  at 9.8 m with `threat=3` (THREATENED) who had closed in to fight FIA. Fix: the proximity rule gets
  the same combat exemption as sprinting (own combat, or the observer at VIGILANT or above).

- Combat rule set (user decision, 2026-09-23): in combat (own combat, or the observer at VIGILANT or
  above) only talking near the outfit's side and shooting or killing someone of that side in front of
  a witness give the player away. Aiming at the outfit's side no longer adds suspicion in combat
  (muzzle sweeping); sprinting, stray shots and proximity were already exempt. Out of combat nothing
  changes.

- Run at 14:44 is void: Workbench had last reloaded scripts at 14:33, before 99752d0 and dd5111e.
- Run B final (same log, scripts reloaded 14:54:50, after every change): own combat from 14:55:29;
  60 shots (43 at FIA, 17 untargeted) up to 14:57:06 with no suspicion and no break while running,
  aiming and standing among USSR. The user then talked on purpose: `reason=voice` at 14:57:34
  (witness 42 m, VIGILANT). Restored 14:58:16, group despawned 14:58:46. At 14:59:43, 37 s after the
  window closed, standing 4.5 m from a calm USSR (`threat=0`) broke by `suspicion:proximity`, as
  designed out of combat. T4 closed.

- Second native review (lineage `review-ad4133a167ee9e9e`, 37c6b82..2415456, reliability lens):
  APPROVED and acknowledged. Five non-blocking warnings:
  1. Test layer had two properties on one line after an edit (fixed).
  2. Remote-player aim direction on a dedicated server unverified (all runs were Workbench).
  3. The delayed victim check does not require the victim to be hostile to the player's real faction.
  4. Default cap changed 2 -> 1 silently (no mission uses this addon yet, so no impact today).
  5. Several branches proven only by reading (kill of an outfit enemy, one-shot facing victim, cap,
     OnDelete beyond one Play stop).

- Warning 3 fixed (user approved): `CheckVictimWitness` now requires the victim to be hostile to the
  player's real faction (`IsHunter`), like every other witness. Not re-run in play: with the current
  configs every faction is hostile, so the outcome of past runs does not change.

- Dedicated-server run (addon v0.0.9 from the Workshop, log `logs_2026-09-23_18-13-02`, UTC): 57 shots
  at FIA classified `target=2` with the right soldier each time, so a remote player's weapon aim
  reaches the server usably (warning 2 closed). Own combat by `engaged by outfit enemy` seen for the
  first time (18:21:09, 18:24:21); first observed `kill` break (18:21:36, witness THREATENED; victim
  not logged); `shot:victim` on a USSR at 2.8 m (18:28:03); restores and despawns normal. The break at
  18:18:41 was a USSR stepping into the player's line of fire (user). User decision: keep shooting a
  USSR as an instant break in combat too; in an incognito mission the player is expected to break off
  when the disguise faction and the third party start fighting. Kill breaks now log the victim.

## Next step
Turn off the test-layer debug log; turn off the test-layer debug log;
delivery (push / PR) is the user's decision.
Delivery (push / PR) is the user's decision.
