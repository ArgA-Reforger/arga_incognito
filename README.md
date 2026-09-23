# Arga Incognito

Server-side incognito system for Arma Reforger missions. A player wearing the uniform of another
faction is taken for one of them by the AI, until his behaviour gives him away. Suspicious behaviour
builds up an invisible suspicion level; a hostile act breaks the disguise at once. Optionally, a
reinforcement group is sent to where the player was exposed.

[Español más abajo](#español)

---

## English

### Quick setup

1. Add **Arga Incognito** as a dependency of your mission addon.
2. In the game mode, enable perceived factions for AI on `SCR_PerceivedFactionManagerComponent`:
   - `m_bPerceivedFactionChangesAffectsAI` = `1`
   - `m_eCharacterPerceivedFactionOutfitType` = `HIGHEST_VALUE`

   Without this the AI ignores uniforms and the disguise never works. `GameMode_ARGA` leaves it off.
3. Place a `GenericEntity` in the world and add `ARGA_IncognitoComponent` to it. One per world.
4. Give the players a loadout that wears the other faction's uniform while keeping their own faction.
5. Optional: enable **Reinforcements** and pick a group prefab.

Expected result: in the server log, `[ARGA_Incognito] Armed on server`, then for each disguised player
`Initialized ... disguise=HOSTILE_FACTION` and `AI override applied: AI now perceives <faction>`.

### Who is who

| Term | Meaning |
|------|---------|
| **Disguise faction** | The faction whose uniform the player wears (e.g. USSR). |
| **Observer** | An AI hostile to the player's real faction but not to the disguise faction: the AI being fooled. |
| **Disguise side** | The disguise faction and anyone it is not hostile to. |
| **Common enemy** | Anyone the disguise faction is hostile to (e.g. FIA). |

Every rule needs an observer who **sees** the player (vision cone plus a line-of-sight ray), except
voice, which carries through walls.

### Rules

| Action | Out of combat | In combat |
|--------|---------------|-----------|
| Talking on direct voice within its range of an observer | Breaks at once | Breaks at once |
| Shooting at someone of the disguise side, seen by an observer | Breaks at once | Breaks at once |
| Shooting someone of the disguise side who stays conscious (he knows who shot) | Breaks at once | Breaks at once |
| Killing someone of the disguise side, seen by an observer | Breaks at once | Breaks at once |
| Aiming at someone of the disguise side | Adds suspicion | Nothing |
| Sprinting | Adds suspicion | Nothing |
| Standing very close to an observer | Adds suspicion | Nothing |
| Shooting at nothing (air, walls) | Adds suspicion per shot | Nothing |
| Aiming, shooting or killing a common enemy | Nothing | Nothing |

**In combat** means either:

- **Own combat**: the player shot at a common enemy, or a common enemy is engaging him, in the last
  `m_fOwnCombatSeconds` seconds.
- **Observer in combat**: the observer's own AI threat state is VIGILANT or higher (it heard gunfire
  or is fighting).

The target of a shot or aim is the AI closest to the weapon's aim line within a 5° cone. A shot aimed
at someone counts as aimed at him even if it misses. A common enemy behind cover still counts, so
suppressive fire is not a stray shot. Only AI characters are recognised as targets.

Accidental friendly fire counts too: if a disguise-side soldier steps into the line of fire, the
disguise breaks. When the disguise faction and a common enemy start fighting, the intended play is to
break off and slip away.

### Suspicion and recovery

- Each player has a suspicion level from 0 to 100, kept on the server only. **The player never sees
  it**, and the AI does not act differently before the disguise breaks.
- Suspicious actions add suspicion per second, twice as fast at point blank as at the edge of the rule's
  radius. At 100 the disguise breaks.
- While nothing suspicious happens, suspicion drops by `m_fSuspicionDecayRate` per second, twice as
  fast with no hostile AI within the witness radius.
- Once broken, suspicion stays at 100 while any hostile AI sees the player and drops otherwise. The
  disguise is restored at `m_fRestoreThreshold`.

### Reinforcements

When a disguise breaks and reinforcements are enabled:

1. The group prefab spawns at `m_fReinforcementDistance` metres and `m_fReinforcementBearing` degrees
   (0 = north, 90 = east) from where the player was exposed.
2. It gets a waypoint (Search and Destroy by default) at that spot, not on the player.
3. A new break near a live group (`m_fReinforcementReuseRadius`) re-tasks that group instead of
   spawning another. At `m_iMaxReinforcementGroups`, the closest group is re-tasked.
4. When no player has a broken disguise, each group leaves after `m_fReinforcementDespawnSeconds`,
   but never while a player within `m_fReinforcementDespawnSightRadius` sees one of its members.

### Attributes

**Disguise Break**

| Attribute | Default | Meaning |
|-----------|---------|---------|
| `m_fShotRadius` | 75 | Shot radius (m) |
| `m_fAimRadius` | 50 | Aim radius (m) |
| `m_fAimTargetRange` | 300 | Range to identify the target (m) |
| `m_fSprintRadius` | 100 | Sprint radius (m) |
| `m_fProximityRadius` | 10 | Proximity radius (m) |
| `m_fWitnessRadius` | 200 | Witness radius (m) |
| `m_bVoiceBreak` | on | Break on voice |
| `m_bDebugLog` | off | Diagnostic log |

A radius of 0 turns its rule off.

**Suspicion**

| Attribute | Default | Meaning |
|-----------|---------|---------|
| `m_fSprintSuspicionRate` | 40 | Suspicion per second while sprinting |
| `m_fAimSuspicionRate` | 50 | Suspicion per second while aiming |
| `m_fProximitySuspicionRate` | 25 | Suspicion per second for proximity |
| `m_fStrayShotSuspicion` | 30 | Suspicion per shot with no target |
| `m_fOwnCombatSeconds` | 30 | Own combat duration (s) |
| `m_fSuspicionDecayRate` | 2.5 | Suspicion drop per second |
| `m_fRestoreThreshold` | 25 | Restore threshold |

**Reinforcements**

| Attribute | Default | Meaning |
|-----------|---------|---------|
| `m_bReinforcementsEnabled` | off | Enable reinforcements |
| `m_sReinforcementGroup` | — | Group prefab |
| `m_fReinforcementDistance` | 200 | Spawn distance (m) |
| `m_fReinforcementBearing` | 0 | Spawn bearing (degrees, 0 = north) |
| `m_sReinforcementWaypoint` | `AIWaypoint_SearchAndDestroy` | Group waypoint |
| `m_fReinforcementWaypointRadius` | 30 | Waypoint radius (m) |
| `m_fReinforcementDespawnSeconds` | 120 | Despawn time (s) |
| `m_fReinforcementDespawnSightRadius` | 300 | Sight radius for despawn (m) |
| `m_fReinforcementReuseRadius` | 300 | Reuse radius (m) |
| `m_iMaxReinforcementGroups` | 1 | Maximum groups |

### Notes for mission makers

- **Voice range** is not configured here. It is read from the speaker's active VON component, so a
  voice mod's ranges apply automatically.
- **Disguise prefabs that inherit a vanilla character** also inherit its random variants, and the
  loadout system may spawn the vanilla variant instead. Point the inherited variant at your own prefab
  (see `Prefabs/Characters/Incognito/ARGA_Incognito_Character_USSR_Rifleman.et`).
- **Faction relations** come from the mission's faction configs; the rules above follow them.
- **Diagnostics**: with `m_bDebugLog` on, the log shows suspicion changes, what each shot was aimed at,
  own-combat windows and the witness of every break, including its AI threat state.
- **Test world**: `Worlds/TestIncognito.ent` has a disguised ARGA player, a USSR fireteam, an FIA
  fireteam as a common enemy and naval infantry reinforcements.

### Verification status

Verified in Workbench and on a dedicated server: every rule above, own combat, reinforcements,
re-tasking and despawn. Not reproduced in play (AI reactions are hard to stage), only checked by code
reading: taking out a victim who faces the player with one shot, and reaching the group cap.

---

## Español

Sistema de incógnito para misiones de Arma Reforger, del lado del servidor. Un jugador con el uniforme
de otra facción es tomado por uno de ellos por la IA, hasta que su conducta lo delata. Las conductas
sospechosas acumulan un nivel de sospecha invisible; un acto hostil rompe el disfraz al instante.
Opcionalmente, se envía un grupo de refuerzo al lugar donde el jugador fue descubierto.

### Configuración rápida

1. Agregar **Arga Incognito** como dependencia del addon de la misión.
2. En el game mode, activar las facciones percibidas para la IA en `SCR_PerceivedFactionManagerComponent`:
   - `m_bPerceivedFactionChangesAffectsAI` = `1`
   - `m_eCharacterPerceivedFactionOutfitType` = `HIGHEST_VALUE`

   Sin esto la IA ignora los uniformes y el disfraz nunca funciona. `GameMode_ARGA` lo deja apagado.
3. Colocar un `GenericEntity` en el mundo y agregarle `ARGA_IncognitoComponent`. Uno por mundo.
4. Dar a los jugadores un loadout con el uniforme de la otra facción, conservando su propia facción.
5. Opcional: activar **Reinforcements** y elegir el prefab de grupo.

Resultado esperado: en el log del servidor, `[ARGA_Incognito] Armed on server` y, por cada jugador
disfrazado, `Initialized ... disguise=HOSTILE_FACTION` y `AI override applied: AI now perceives <facción>`.

### Quién es quién

| Término | Significado |
|---------|-------------|
| **Facción del disfraz** | La facción cuyo uniforme lleva el jugador (por ejemplo, USSR). |
| **Observador** | Una IA hostil a la facción real del jugador pero no a la del disfraz: la IA engañada. |
| **Bando del disfraz** | La facción del disfraz y cualquiera que no le sea hostil. |
| **Enemigo común** | Cualquiera al que la facción del disfraz le es hostil (por ejemplo, FIA). |

Toda regla necesita un observador que **vea** al jugador (cono de visión más un rayo de línea de
vista), salvo la voz, que atraviesa paredes.

### Reglas

| Acción | Fuera de combate | En combate |
|--------|------------------|------------|
| Hablar por voz directa dentro de su alcance respecto de un observador | Rompe al instante | Rompe al instante |
| Disparar a alguien del bando del disfraz, visto por un observador | Rompe al instante | Rompe al instante |
| Disparar a alguien del bando del disfraz que queda consciente (sabe quién disparó) | Rompe al instante | Rompe al instante |
| Matar a alguien del bando del disfraz, visto por un observador | Rompe al instante | Rompe al instante |
| Apuntar a alguien del bando del disfraz | Suma sospecha | Nada |
| Esprintar | Suma sospecha | Nada |
| Estar muy cerca de un observador | Suma sospecha | Nada |
| Disparar a la nada (aire, paredes) | Suma sospecha por disparo | Nada |
| Apuntar, disparar o matar a un enemigo común | Nada | Nada |

**En combate** significa alguna de estas dos cosas:

- **Combate propio**: el jugador disparó a un enemigo común, o un enemigo común lo está combatiendo, en
  los últimos `m_fOwnCombatSeconds` segundos.
- **Observador en combate**: el estado de amenaza de la IA del observador es VIGILANT o superior (oyó
  disparos o está peleando).

El blanco de un disparo o de un apuntado es la IA más cercana a la línea de mira del arma, dentro de un
cono de 5°. Un disparo dirigido a alguien cuenta como dirigido a él aunque falle. Un enemigo común
detrás de cobertura también cuenta, así que el fuego de supresión no es un disparo a la nada. Solo se
reconocen como blanco personajes de IA.

El fuego amigo accidental también cuenta: si un soldado del bando del disfraz se cruza en la línea de
tiro, el disfraz se rompe. Cuando la facción del disfraz y un enemigo común empiezan a pelear, lo
esperado es alejarse del combate y escapar.

### Sospecha y recuperación

- Cada jugador tiene un nivel de sospecha de 0 a 100 que existe solo en el servidor. **El jugador nunca
  lo ve**, y la IA no se comporta distinto antes de que el disfraz se rompa.
- Las acciones sospechosas suman sospecha por segundo, el doble a quemarropa que en el borde del radio
  de la regla. Al llegar a 100 el disfraz se rompe.
- Mientras no pasa nada sospechoso, la sospecha baja `m_fSuspicionDecayRate` por segundo, el doble si
  no hay IA hostil dentro del radio de testigos.
- Con el disfraz roto, la sospecha se mantiene en 100 mientras alguna IA hostil vea al jugador y baja
  en caso contrario. El disfraz se recupera al llegar a `m_fRestoreThreshold`.

### Refuerzos

Cuando un disfraz se rompe y los refuerzos están activados:

1. El prefab de grupo aparece a `m_fReinforcementDistance` metros y `m_fReinforcementBearing` grados
   (0 = norte, 90 = este) del lugar donde el jugador fue descubierto.
2. Recibe un waypoint (Search and Destroy por defecto) en ese lugar, no sobre el jugador.
3. Una nueva ruptura cerca de un grupo vivo (`m_fReinforcementReuseRadius`) reasigna ese grupo en lugar
   de crear otro. Al llegar a `m_iMaxReinforcementGroups`, se reasigna el grupo más cercano.
4. Cuando ningún jugador tiene el disfraz roto, cada grupo se retira pasados
   `m_fReinforcementDespawnSeconds`, pero nunca mientras un jugador a menos de
   `m_fReinforcementDespawnSightRadius` vea a alguno de sus miembros.

### Atributos

**Disguise Break**

| Atributo | Por defecto | Significado |
|----------|-------------|-------------|
| `m_fShotRadius` | 75 | Radio de disparo (m) |
| `m_fAimRadius` | 50 | Radio de apuntado (m) |
| `m_fAimTargetRange` | 300 | Alcance para identificar el blanco (m) |
| `m_fSprintRadius` | 100 | Radio de sprint (m) |
| `m_fProximityRadius` | 10 | Radio de cercanía (m) |
| `m_fWitnessRadius` | 200 | Radio de testigos (m) |
| `m_bVoiceBreak` | activado | Romper por voz |
| `m_bDebugLog` | desactivado | Log de diagnóstico |

Un radio en 0 desactiva su regla.

**Suspicion**

| Atributo | Por defecto | Significado |
|----------|-------------|-------------|
| `m_fSprintSuspicionRate` | 40 | Sospecha por segundo al esprintar |
| `m_fAimSuspicionRate` | 50 | Sospecha por segundo al apuntar |
| `m_fProximitySuspicionRate` | 25 | Sospecha por segundo por cercanía |
| `m_fStrayShotSuspicion` | 30 | Sospecha por disparo sin blanco |
| `m_fOwnCombatSeconds` | 30 | Duración del combate propio (s) |
| `m_fSuspicionDecayRate` | 2.5 | Descenso de sospecha por segundo |
| `m_fRestoreThreshold` | 25 | Umbral de recuperación |

**Reinforcements**

| Atributo | Por defecto | Significado |
|----------|-------------|-------------|
| `m_bReinforcementsEnabled` | desactivado | Activar refuerzos |
| `m_sReinforcementGroup` | — | Prefab del grupo |
| `m_fReinforcementDistance` | 200 | Distancia de aparición (m) |
| `m_fReinforcementBearing` | 0 | Rumbo de aparición (grados, 0 = norte) |
| `m_sReinforcementWaypoint` | `AIWaypoint_SearchAndDestroy` | Waypoint del grupo |
| `m_fReinforcementWaypointRadius` | 30 | Radio del waypoint (m) |
| `m_fReinforcementDespawnSeconds` | 120 | Tiempo de retiro (s) |
| `m_fReinforcementDespawnSightRadius` | 300 | Radio de visión para el retiro (m) |
| `m_fReinforcementReuseRadius` | 300 | Radio de reutilización (m) |
| `m_iMaxReinforcementGroups` | 1 | Máximo de grupos |

### Notas para quien arma la misión

- **El alcance de la voz** no se configura acá. Se lee del componente de voz activo del jugador que
  habla, así que los alcances de un mod de voz se aplican solos.
- **Los prefabs de disfraz que heredan de un personaje vanilla** heredan también sus variantes
  aleatorias, y el sistema de loadouts puede hacer aparecer la variante vanilla. Hay que apuntar la
  variante heredada al propio prefab (ver
  `Prefabs/Characters/Incognito/ARGA_Incognito_Character_USSR_Rifleman.et`).
- **Las relaciones entre facciones** salen de las configuraciones de facciones de la misión; las reglas
  de arriba las respetan.
- **Diagnóstico**: con `m_bDebugLog` activado, el log muestra los cambios de sospecha, a quién apuntaba
  cada disparo, las ventanas de combate propio y el testigo de cada ruptura, con su estado de amenaza.
- **Mundo de prueba**: `Worlds/TestIncognito.ent` tiene un jugador ARGA disfrazado, un equipo USSR, un
  equipo FIA como enemigo común y refuerzos de infantería naval.

### Estado de verificación

Verificado en Workbench y en servidor dedicado: todas las reglas de arriba, el combate propio, los
refuerzos, la reasignación y el retiro. No reproducido en juego (las reacciones de la IA son difíciles
de forzar), solo revisado en el código: voltear de un tiro a una víctima que mira al jugador, y llegar
al máximo de grupos.
