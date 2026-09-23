[ComponentEditorProps(category: "ArgA/Component/Gameplay", description: "Inicializa la faccion percibida por ropa en game modes que reemplazan el spawn vanilla")]
class ARGA_IncognitoComponentClass : ScriptComponentClass
{
}

//! Initializes the vanilla perceived-faction (disguise) system under game modes that bypass the vanilla
//! spawn pipeline, where its own spawn hook never runs. Server only.
//!
//! Disguise break (global, faction-agnostic): talking near an "observer" (an enemy-of-his-real-faction AI
//! that is not enemy-of-his-outfit), or shooting or killing someone of the outfit's own side in an
//! observer's sight, clears the player's AI override at once. Attacking an enemy of the outfit faction
//! gives nothing away. Aiming at the outfit's side, stray shots, sprinting or standing too close only
//! raise a server-side suspicion level, and the disguise breaks when it reaches MAX_SUSPICION. Suspicion decays while he does nothing
//! suspicious; once broken, it decays only while no "hunter" (enemy-of-his-real-faction AI) sees him, and
//! the disguise is restored at m_fRestoreThreshold. Voice is the only rule that does not require line of
//! sight, and its radius comes from the VON component the player transmits through.
class ARGA_IncognitoState
{
	int m_iPlayerId;
	SCR_PlayerController m_Controller;
	IEntity m_Entity;
	bool m_bBroken;
	float m_fLastSeenTime;
	float m_fRestoredTime;
	float m_fVoiceTime;
	float m_fVoiceRadius;
	string m_sLastPerception;
	bool m_bWasSprinting;
	vector m_vLastPos;
	float m_fLastPosTime;
	int m_iLastSpeedBucket;
	float m_fSuspicion;
	int m_iLastSuspicionBucket;
	float m_fCombatUntil;
}

class ARGA_IncognitoReinforcement
{
	SCR_AIGroup m_Group;
	AIWaypoint m_Waypoint;
	float m_fSpawnedAt;
	float m_fDespawnAt;
}

class ARGA_IncognitoComponent : ScriptComponent
{
	[Attribute("75", UIWidgets.Slider, "Radio de disparo (m).", params: "0 500 1", category: "Disguise Break")]
	protected float m_fShotRadius;

	[Attribute("50", UIWidgets.Slider, "Radio de apuntado (m).", params: "0 200 1", category: "Disguise Break")]
	protected float m_fAimRadius;

	[Attribute("300", UIWidgets.Slider, "Alcance para identificar el blanco (m).", params: "10 1000 1", category: "Disguise Break")]
	protected float m_fAimTargetRange;

	[Attribute("100", UIWidgets.Slider, "Radio de sprint (m).", params: "0 500 1", category: "Disguise Break")]
	protected float m_fSprintRadius;

	[Attribute("10", UIWidgets.Slider, "Radio de cercania (m).", params: "0 100 1", category: "Disguise Break")]
	protected float m_fProximityRadius;

	[Attribute("200", UIWidgets.Slider, "Radio de testigos (m).", params: "0 500 1", category: "Disguise Break")]
	protected float m_fWitnessRadius;

	[Attribute("1", UIWidgets.CheckBox, "Romper por voz.", category: "Disguise Break")]
	protected bool m_bVoiceBreak;

	[Attribute("40", UIWidgets.Slider, "Sospecha por segundo al esprintar.", params: "0 200 1", category: "Suspicion")]
	protected float m_fSprintSuspicionRate;

	[Attribute("50", UIWidgets.Slider, "Sospecha por segundo al apuntar.", params: "0 200 1", category: "Suspicion")]
	protected float m_fAimSuspicionRate;

	[Attribute("25", UIWidgets.Slider, "Sospecha por segundo por cercania.", params: "0 200 1", category: "Suspicion")]
	protected float m_fProximitySuspicionRate;

	[Attribute("30", UIWidgets.Slider, "Sospecha por disparo sin blanco.", params: "0 100 1", category: "Suspicion")]
	protected float m_fStrayShotSuspicion;

	[Attribute("30", UIWidgets.Slider, "Duracion del combate propio (s).", params: "0 300 1", category: "Suspicion")]
	protected float m_fOwnCombatSeconds;

	[Attribute("2.5", UIWidgets.Slider, "Descenso de sospecha por segundo.", params: "0 50 0.1", category: "Suspicion")]
	protected float m_fSuspicionDecayRate;

	[Attribute("25", UIWidgets.Slider, "Umbral de recuperacion.", params: "0 99 1", category: "Suspicion")]
	protected float m_fRestoreThreshold;

	[Attribute("0", UIWidgets.CheckBox, "Activar refuerzos.", category: "Reinforcements")]
	protected bool m_bReinforcementsEnabled;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Prefab del grupo de refuerzo.", "et", category: "Reinforcements")]
	protected ResourceName m_sReinforcementGroup;

	[Attribute("200", UIWidgets.Slider, "Distancia de aparicion (m).", params: "20 2000 1", category: "Reinforcements")]
	protected float m_fReinforcementDistance;

	[Attribute("0", UIWidgets.Slider, "Rumbo de aparicion (grados, 0 = norte).", params: "0 359 1", category: "Reinforcements")]
	protected float m_fReinforcementBearing;

	[Attribute("{B3E7B8DC2BAB8ACC}Prefabs/AI/Waypoints/AIWaypoint_SearchAndDestroy.et", UIWidgets.ResourcePickerThumbnail, "Waypoint del grupo.", "et", category: "Reinforcements")]
	protected ResourceName m_sReinforcementWaypoint;

	[Attribute("30", UIWidgets.Slider, "Radio del waypoint (m).", params: "5 200 1", category: "Reinforcements")]
	protected float m_fReinforcementWaypointRadius;

	[Attribute("120", UIWidgets.Slider, "Tiempo de retiro (s).", params: "0 1800 1", category: "Reinforcements")]
	protected float m_fReinforcementDespawnSeconds;

	[Attribute("300", UIWidgets.Slider, "Radio de vision para el retiro (m).", params: "0 2000 1", category: "Reinforcements")]
	protected float m_fReinforcementDespawnSightRadius;

	[Attribute("300", UIWidgets.Slider, "Radio de reutilizacion (m).", params: "0 2000 1", category: "Reinforcements")]
	protected float m_fReinforcementReuseRadius;

	[Attribute("1", UIWidgets.Slider, "Maximo de grupos.", params: "1 10 1", category: "Reinforcements")]
	protected int m_iMaxReinforcementGroups;

	[Attribute("0", UIWidgets.CheckBox, "Log de diagnostico.", category: "Disguise Break")]
	protected bool m_bDebugLog;

	//! Server tick period, also the voice window: voice arrives every frame while the key is held.
	protected const int TICK_MS = 500;

	//! cos(70 deg): half of the 140 deg peripheral FOV vanilla gives the EyesSensor in Character_Base.et.
	protected const float MIN_FOV_DOT = 0.342;

	protected const float EYE_HEIGHT = 1.6;

	//! GetMovementSpeed() slides continuously up to 2 as the player wheels from walk to run, and jumps to
	//! a fixed 3 on sprint. Measured on a dedicated server: run peaked at 1.98, sprint held exactly 3.
	protected const float SPRINT_MOVEMENT_SPEED = 2.5;

	protected const float MAX_SUSPICION = 100;

	//! cos(5 deg): how close to the weapon's aim line a character must be to count as the one aimed at.
	protected const float AIM_TARGET_MIN_DOT = 0.996;

	//! Delay before judging a shot's victim, so the bullet has landed and a hit that took him out is known.
	protected const int VICTIM_CHECK_MS = 500;

	protected const int TARGET_NONE = 0;
	protected const int TARGET_DISGUISE_SIDE = 1;
	protected const int TARGET_OUTFIT_ENEMY = 2;

	//! A freshly spawned group may still be creating its members; it is not pruned as empty before this.
	protected const float REINFORCEMENT_SPAWN_GRACE_MS = 10000;

	protected ref array<ref ARGA_IncognitoReinforcement> m_aReinforcements = {};

	//! Reused instead of allocated per trace, as vanilla does in its own AI trace nodes.
	protected ref TraceParam m_TraceParam = new TraceParam();
	protected ref array<IEntity> m_aTraceExclude = {null, null};

	protected int m_iTraceCount;
	protected int m_iFovRejects;

	//! Diagnostics: why the last Sees() said no, cone or geometry.
	protected string m_sLastSeeFail;

	protected ref map<int, ref ARGA_IncognitoState> m_mStates = new map<int, ref ARGA_IncognitoState>();

	//! Speaking range per VON component class. Resolved once: OnVoNUsed fires every frame.
	protected ref map<string, float> m_mVoiceRadii = new map<string, float>();

	protected static ARGA_IncognitoComponent s_Instance;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);

		if (!GetGame().InPlayMode())
			return;

		if (!Replication.IsServer())
		{
			Print("[ARGA_Incognito] EOnInit skipped, not the server.", LogLevel.NORMAL);
			return;
		}

		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (!gameMode)
		{
			Print("[ARGA_Incognito] No SCR_BaseGameMode found. Perceived faction will never initialize.", LogLevel.ERROR);
			return;
		}

		if (!SCR_PerceivedFactionManagerComponent.GetInstance())
			Print("[ARGA_Incognito] No SCR_PerceivedFactionManagerComponent in the world. Check the game mode.", LogLevel.WARNING);

		gameMode.GetOnPlayerRegistered().Insert(OnPlayerRegistered);
		gameMode.GetOnPlayerDisconnected().Insert(OnPlayerDisconnected);
		gameMode.GetOnControllableDestroyed().Insert(OnControllableDestroyed);

		s_Instance = this;

		Print("[ARGA_Incognito] Armed on server. Waiting for players.", LogLevel.NORMAL);

		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		super.OnDelete(owner);

		if (s_Instance == this)
			s_Instance = null;

		GetGame().GetCallqueue().Remove(Tick);
		GetGame().GetCallqueue().Remove(CheckVictimWitness);

		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (gameMode)
		{
			gameMode.GetOnPlayerRegistered().Remove(OnPlayerRegistered);
			gameMode.GetOnPlayerDisconnected().Remove(OnPlayerDisconnected);
			gameMode.GetOnControllableDestroyed().Remove(OnControllableDestroyed);
		}

		foreach (int playerId, ARGA_IncognitoState state : m_mStates)
		{
			if (state.m_Controller)
				state.m_Controller.m_OnControlledEntityChanged.Remove(OnControlledEntityChanged);

			UnregisterShotHook(state.m_Entity);
		}

		m_mStates.Clear();

		foreach (ARGA_IncognitoReinforcement reinforcement : m_aReinforcements)
			DeleteReinforcement(reinforcement);

		m_aReinforcements.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Entry point for the modded SCR_VoNComponent. vonComponent is the speaker's active one.
	static void NotifyVoiceUsed(int playerId, SCR_VoNComponent vonComponent)
	{
		if (s_Instance)
			s_Instance.OnVoiceUsed(playerId, vonComponent);
	}

	//------------------------------------------------------------------------------------------------
	//! Fires every frame while transmitting, so it only stamps state; the observer search stays in Tick.
	protected void OnVoiceUsed(int playerId, SCR_VoNComponent vonComponent)
	{
		if (!m_bVoiceBreak || !vonComponent)
			return;

		ARGA_IncognitoState state = m_mStates.Get(playerId);
		if (!state || state.m_bBroken || !state.m_Entity)
			return;

		float radius = ResolveVoiceRadius(vonComponent, state.m_Entity);
		if (radius <= 0)
			return;

		state.m_fVoiceTime = GetGame().GetWorld().GetWorldTime();
		state.m_fVoiceRadius = radius;
	}

	//------------------------------------------------------------------------------------------------
	//! Speaking range read from the mod's own .acp, so it is never configured in two places.
	//! Keyed by class, which stays available when the lookup fails, so failures also cache once.
	protected float ResolveVoiceRadius(SCR_VoNComponent vonComponent, IEntity entity)
	{
		string key = vonComponent.ClassName();

		float cached;
		if (m_mVoiceRadii.Find(key, cached))
			return cached;

		ResourceName acp;
		float radius = 0;
		string detail;

		BaseContainer source = vonComponent.GetComponentSource(entity);
		if (!source)
			detail = "sin component source";
		else if (!source.Get("Filename", acp))
			detail = "el componente no tiene Filename";
		else
		{
			radius = ReadOuterRange(acp);
			detail = acp;
		}

		m_mVoiceRadii.Set(key, radius);

		if (m_bDebugLog)
			Print(string.Format("[ARGA_Incognito][Debug] Voice range %1 = %2m (%3)", key, radius, detail), LogLevel.NORMAL);

		return radius;
	}

	//------------------------------------------------------------------------------------------------
	protected bool SpokeSinceLastTick(ARGA_IncognitoState state)
	{
		if (state.m_fVoiceRadius <= 0 || state.m_fVoiceTime <= 0)
			return false;

		return (GetGame().GetWorld().GetWorldTime() - state.m_fVoiceTime) <= TICK_MS;
	}

	//------------------------------------------------------------------------------------------------
	//! Returns the .acp's speaking range in metres, or 0 if it cannot be read.
	protected float ReadOuterRange(ResourceName acp)
	{
		Resource holder = BaseContainerTools.LoadContainer(acp);
		if (!holder)
			return 0;

		BaseContainer root = holder.GetResource().ToBaseContainer();
		if (!root)
			return 0;

		BaseContainerList amplitudes = root.GetObjectArray("amplitudes");
		if (!amplitudes || amplitudes.Count() == 0)
			return 0;

		float outerRange;
		if (!amplitudes.Get(0).Get("outerRange", outerRange))
			return 0;

		return outerRange;
	}

	//------------------------------------------------------------------------------------------------
	//! A player exists now, but under PS they have no character yet: the lobby hands one over later.
	protected void OnPlayerRegistered(int playerId)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (!controller)
		{
			Print(string.Format("[ARGA_Incognito] No PlayerController for playerId=%1.", playerId), LogLevel.WARNING);
			return;
		}

		if (m_mStates.Contains(playerId))
			return;

		ARGA_IncognitoState state = new ARGA_IncognitoState();
		state.m_iPlayerId = playerId;
		state.m_Controller = controller;
		m_mStates.Insert(playerId, state);
		controller.m_OnControlledEntityChanged.Insert(OnControlledEntityChanged);

		Print(string.Format("[ARGA_Incognito] Watching playerId=%1.", playerId), LogLevel.NORMAL);

		// The character may already be assigned when we get here, in which case no event is coming.
		SyncWatchedEntity(state);
	}

	//------------------------------------------------------------------------------------------------
	//! Fast path only: not proven to fire server-side for remote players, so Tick() resyncs anyway.
	protected void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		// The invoker does not say which player fired it, so resync every watched state.
		foreach (int playerId, ARGA_IncognitoState state : m_mStates)
			SyncWatchedEntity(state);
	}

	//------------------------------------------------------------------------------------------------
	//! On a real entity change: re-inits the perceived faction, re-arms the shot hook and clears the break
	//! state. Cheap no-op otherwise, so Tick() can call it every poll.
	protected void SyncWatchedEntity(ARGA_IncognitoState state)
	{
		if (!state.m_Controller)
			return;

		IEntity current = state.m_Controller.GetControlledEntity();
		if (current == state.m_Entity)
			return;

		UnregisterShotHook(state.m_Entity);
		state.m_Entity = current;
		state.m_bBroken = false;
		state.m_fLastSeenTime = 0;
		state.m_fRestoredTime = 0;
		state.m_fSuspicion = 0;
		state.m_iLastSuspicionBucket = 0;
		state.m_fCombatUntil = 0;

		if (!current)
			return;

		InitPerceivedFaction(current);
		RegisterShotHook(current);
	}

	//------------------------------------------------------------------------------------------------
	protected void RegisterShotHook(IEntity entity)
	{
		EventHandlerManagerComponent eventHandlerManager = EventHandlerManagerComponent.Cast(entity.FindComponent(EventHandlerManagerComponent));
		if (!eventHandlerManager)
		{
			Print(string.Format("[ARGA_Incognito] %1 has no EventHandlerManagerComponent, shot trigger will never fire.", entity), LogLevel.WARNING);
			return;
		}

		eventHandlerManager.RegisterScriptHandler("OnProjectileShot", this, OnWeaponFired);
	}

	//------------------------------------------------------------------------------------------------
	protected void UnregisterShotHook(IEntity entity)
	{
		if (!entity)
			return;

		EventHandlerManagerComponent eventHandlerManager = EventHandlerManagerComponent.Cast(entity.FindComponent(EventHandlerManagerComponent));
		if (!eventHandlerManager)
			return;

		eventHandlerManager.RemoveScriptHandler("OnProjectileShot", this, OnWeaponFired);
	}

	//------------------------------------------------------------------------------------------------
	//! Runs the init vanilla would have run on spawn finalize. Idempotent.
	protected void InitPerceivedFaction(IEntity entity)
	{
		if (!entity)
			return;

		SCR_CharacterFactionAffiliationComponent affiliation = SCR_CharacterFactionAffiliationComponent.Cast(entity.FindComponent(SCR_CharacterFactionAffiliationComponent));
		if (!affiliation)
		{
			Print(string.Format("[ARGA_Incognito] %1 has no SCR_CharacterFactionAffiliationComponent, skipped.", entity), LogLevel.WARNING);
			return;
		}

		if (affiliation.HasPerceivedFaction())
			return;

		affiliation.InitPlayerOutfitFaction_S();

		Faction perceived = affiliation.GetPerceivedFaction();
		string perceivedKey = "UNKNOWN";
		if (perceived)
			perceivedKey = perceived.GetFactionKey();

		Print(string.Format("[ARGA_Incognito] Initialized %1: perceived=%2 disguise=%3", entity, perceivedKey, typename.EnumToString(SCR_ECharacterDisguiseType, affiliation.GetCharacterDisguiseType())), LogLevel.NORMAL);

		ApplyPerceivedFactionForAI(entity, perceived);
	}

	//------------------------------------------------------------------------------------------------
	//! Works around a vanilla ordering bug that leaves the AI override unapplied. Mirrors
	//! SCR_CharacterFactionAffiliationComponent.SetPerceivedFactionForAI().
	protected void ApplyPerceivedFactionForAI(IEntity entity, Faction perceived)
	{
		SCR_PerceivedFactionManagerComponent manager = SCR_PerceivedFactionManagerComponent.GetInstance();
		if (!manager || !manager.DoesPerceivedFactionChangesAffectsAI())
			return;

		PerceivableComponent perceivable = PerceivableComponent.Cast(entity.FindComponent(PerceivableComponent));
		if (!perceivable)
		{
			Print(string.Format("[ARGA_Incognito] %1 has no PerceivableComponent, AI will see the real faction.", entity), LogLevel.WARNING);
			return;
		}

		Faction aiFaction = perceived;
		if (!aiFaction)
			aiFaction = manager.GetFallbackFaction();

		perceivable.SetPerceivedFactionOverride(aiFaction);

		string aiKey = "NONE";
		if (perceivable.GetPerceivedFaction())
			aiKey = perceivable.GetPerceivedFaction().GetFactionKey();

		Print(string.Format("[ARGA_Incognito] AI override applied: AI now perceives %1", aiKey), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPlayerDisconnected(int playerId, KickCauseCode cause = KickCauseCode.NONE, int timeout = -1)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (controller)
			controller.m_OnControlledEntityChanged.Remove(OnControlledEntityChanged);

		ARGA_IncognitoState state = m_mStates.Get(playerId);
		if (state)
			UnregisterShotHook(state.m_Entity);

		m_mStates.Remove(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! True when the player is currently wearing a faction's outfit different from his real one.
	protected bool IsDisguised(SCR_CharacterFactionAffiliationComponent affiliation)
	{
		Faction outfit = affiliation.GetPerceivedFaction();
		if (!outfit)
			return false;

		return outfit != affiliation.GetAffiliatedFaction();
	}

	//------------------------------------------------------------------------------------------------
	//! Controlled entity of every active AI agent. Cheaper than QueryEntitiesBySphere over the world.
	protected void CollectAIEntities(out array<IEntity> aiEntities)
	{
		array<AIAgent> agents = {};
		GetGame().GetAIWorld().GetAIAgents(agents);

		foreach (AIAgent agent : agents)
		{
			IEntity entity = agent.GetControlledEntity();
			if (entity)
				aiEntities.Insert(entity);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! An "observer" is an alive AI whose own faction is enemy to the player's real faction but not
	//! enemy to his outfit faction, i.e. an AI the disguise is currently fooling.
	protected bool IsObserver(IEntity aiEntity, IEntity player, Faction realFaction, Faction outfitFaction)
	{
		if (aiEntity == player)
			return false;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(aiEntity);
		if (!character)
			return false;

		CharacterControllerComponent controller = character.GetCharacterController();
		if (!controller || controller.IsDead())
			return false;

		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(aiEntity.FindComponent(FactionAffiliationComponent));
		if (!affiliation)
			return false;

		Faction aiFaction = affiliation.GetAffiliatedFaction();
		if (!aiFaction)
			return false;

		if (!aiFaction.IsFactionEnemy(realFaction))
			return false;

		return !aiFaction.IsFactionEnemy(outfitFaction);
	}

	//------------------------------------------------------------------------------------------------
	//! A "hunter" is an alive AI whose own faction is enemy to the player's real faction, regardless of
	//! his current outfit. Used while broken to decide when the disguise can be restored.
	protected bool IsHunter(IEntity aiEntity, Faction realFaction)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(aiEntity);
		if (!character)
			return false;

		CharacterControllerComponent controller = character.GetCharacterController();
		if (!controller || controller.IsDead())
			return false;

		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(aiEntity.FindComponent(FactionAffiliationComponent));
		if (!affiliation)
			return false;

		Faction aiFaction = affiliation.GetAffiliatedFaction();
		if (!aiFaction)
			return false;

		return aiFaction.IsFactionEnemy(realFaction);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether aiEntity currently has direct line of sight on player, per its own perception target.
	//! Real eye position when the entity is a character, origin plus a fixed height otherwise.
	protected vector EyeOf(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (character)
			return character.EyePosition();

		vector pos = entity.GetOrigin();
		pos[1] = pos[1] + EYE_HEIGHT;
		return pos;
	}

	//------------------------------------------------------------------------------------------------
	//! Where the character is actually looking. The body transform is only a fallback: an idle AI keeps
	//! its body still and turns its head, so using the body alone rejects sightings it really has.
	protected vector LookDirOf(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (character)
		{
			AimingComponent head = character.GetHeadAimingComponent();
			if (head)
				return head.GetAimingDirectionWorld();
		}

		vector transform[4];
		entity.GetTransform(transform);
		return transform[2];
	}

	//------------------------------------------------------------------------------------------------
	//! Deliberately ignores BaseTarget: while the disguise works the player is FRIENDLY to that AI, and
	//! the engine does not trace occlusion for friendlies, so its sight data reads fully visible always.
	//! Cheap cone check first, ray only for whatever survives it.
	protected bool Sees(IEntity aiEntity, IEntity player)
	{
		vector eye = EyeOf(aiEntity);
		vector torso = EyeOf(player);

		float dot = vector.DotXZ(LookDirOf(aiEntity), vector.Direction(eye, torso).Normalized());
		if (dot < MIN_FOV_DOT)
		{
			m_iFovRejects++;
			m_sLastSeeFail = string.Format("fov dot=%1", dot);
			return false;
		}

		float fraction = TraceFraction(eye, torso, aiEntity, player);
		if (fraction < 1)
		{
			m_sLastSeeFail = string.Format("blocked frac=%1", fraction);
			return false;
		}

		m_sLastSeeFail = "";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! 1 when nothing but a and b lies between from and to.
	protected float TraceFraction(vector from, vector to, IEntity a, IEntity b)
	{
		m_iTraceCount++;

		m_aTraceExclude[0] = a;
		m_aTraceExclude[1] = b;

		m_TraceParam.Start = from;
		m_TraceParam.End = to;
		m_TraceParam.Flags = TraceFlags.ENTS | TraceFlags.OCEAN | TraceFlags.WORLD | TraceFlags.ANY_CONTACT;
		m_TraceParam.Exclude = null;
		m_TraceParam.ExcludeArray = m_aTraceExclude;

		return GetGame().GetWorld().TraceMove(m_TraceParam, null);
	}

	//------------------------------------------------------------------------------------------------
	//! The alive AI character the player's weapon points at: closest to the aim line within a narrow
	//! cone and in range. A cone instead of a hit test, so a miss aimed at someone still counts as aimed
	//! at him. Hips, chest and head are each checked; the character is visible when any of the ones
	//! inside the cone has a clear line of fire, so a head peeking over cover counts. Returns the best
	//! visible one; hidden gets the best one behind cover. Both null when nobody is aimed at.
	protected IEntity FindAimedCharacter(IEntity player, array<IEntity> aiEntities, out IEntity hidden)
	{
		hidden = null;

		ChimeraCharacter character = ChimeraCharacter.Cast(player);
		if (!character)
			return null;

		AimingComponent aiming = character.GetWeaponAimingComponent();
		if (!aiming)
			return null;

		vector aimDir = aiming.GetAimingDirectionWorld();
		vector eye = EyeOf(player);
		vector playerPos = player.GetOrigin();

		IEntity best;
		float bestDot = AIM_TARGET_MIN_DOT;
		float hiddenDot = AIM_TARGET_MIN_DOT;

		foreach (IEntity aiEntity : aiEntities)
		{
			if (aiEntity == player)
				continue;

			if (vector.Distance(playerPos, aiEntity.GetOrigin()) > m_fAimTargetRange)
				continue;

			vector head = EyeOf(aiEntity);
			vector feet = aiEntity.GetOrigin();
			float dot = -1;

			for (int i = 0; i < 3; i++)
			{
				dot = Math.Max(dot, vector.Dot(aimDir, vector.Direction(eye, BodyPoint(feet, head, i)).Normalized()));
			}

			if (dot <= AIM_TARGET_MIN_DOT || (dot <= bestDot && dot <= hiddenDot))
				continue;

			if (!IsAliveCharacter(aiEntity))
				continue;

			// Rays only for the body points inside the cone, cheapest first.
			bool visible = false;
			for (int j = 0; j < 3 && !visible; j++)
			{
				vector point = BodyPoint(feet, head, j);
				if (vector.Dot(aimDir, vector.Direction(eye, point).Normalized()) <= AIM_TARGET_MIN_DOT)
					continue;

				visible = TraceFraction(eye, point, aiEntity, player) >= 1;
			}

			if (visible && dot > bestDot)
			{
				best = aiEntity;
				bestDot = dot;
			}
			else if (!visible && dot > hiddenDot)
			{
				hidden = aiEntity;
				hiddenDot = dot;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Hips, chest and head for index 0, 1 and 2.
	protected vector BodyPoint(vector feet, vector head, int index)
	{
		return feet + (head - feet) * (0.4 + 0.3 * index);
	}

	//------------------------------------------------------------------------------------------------
	//! What the player's weapon points at. A visible character decides. Failing that, an enemy of the
	//! outfit behind cover still counts, so suppressive fire at him is not a stray shot; the outfit's own
	//! side only counts when visible.
	protected int ClassifyAim(IEntity player, array<IEntity> aiEntities, Faction outfitFaction, out IEntity aimed)
	{
		IEntity hidden;
		aimed = FindAimedCharacter(player, aiEntities, hidden);
		if (aimed)
			return ClassifyTarget(aimed, outfitFaction);

		if (ClassifyTarget(hidden, outfitFaction) != TARGET_OUTFIT_ENEMY)
			return TARGET_NONE;

		aimed = hidden;
		return TARGET_OUTFIT_ENEMY;
	}

	//------------------------------------------------------------------------------------------------
	//! True from VIGILANT up, i.e. once the AI has heard gunfire or is fighting: with shots around,
	//! running and shooting at nothing in particular is what everyone does.
	protected bool IsInCombat(IEntity aiEntity)
	{
		return ThreatStateOf(aiEntity) >= EAIThreatState.VIGILANT;
	}

	//------------------------------------------------------------------------------------------------
	//! Opens, or extends, the window in which the player counts as fighting a common enemy.
	protected void EnterOwnCombat(ARGA_IncognitoState state, string cause)
	{
		float now = GetGame().GetWorld().GetWorldTime();

		if (m_bDebugLog && now >= state.m_fCombatUntil)
			Print(string.Format("[ARGA_Incognito][Debug] Own combat playerId=%1 cause=%2", state.m_iPlayerId, cause), LogLevel.NORMAL);

		state.m_fCombatUntil = now + m_fOwnCombatSeconds * 1000;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsInOwnCombat(ARGA_IncognitoState state)
	{
		return GetGame().GetWorld().GetWorldTime() < state.m_fCombatUntil;
	}

	//------------------------------------------------------------------------------------------------
	//! True when an alive enemy of the outfit within the witness radius holds the player as an ENEMY
	//! target in its own perception, i.e. is fighting him. Distance first, perception last.
	protected bool IsEngagedByOutfitEnemy(IEntity player, Faction outfitFaction, array<IEntity> aiEntities)
	{
		vector playerPos = player.GetOrigin();

		foreach (IEntity aiEntity : aiEntities)
		{
			if (aiEntity == player)
				continue;

			if (vector.Distance(playerPos, aiEntity.GetOrigin()) > m_fWitnessRadius)
				continue;

			if (ClassifyTarget(aiEntity, outfitFaction) != TARGET_OUTFIT_ENEMY)
				continue;

			if (!IsAliveCharacter(aiEntity))
				continue;

			PerceptionComponent perception = PerceptionComponent.Cast(aiEntity.FindComponent(PerceptionComponent));
			if (!perception)
				continue;

			BaseTarget target = perception.FindTargetPerceptionObject(player);
			if (target && target.GetTargetCategory() == ETargetCategory.ENEMY)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The AI's own EAIThreatState, or -1 when it has no threat system.
	protected int ThreatStateOf(IEntity aiEntity)
	{
		AIControlComponent control = AIControlComponent.Cast(aiEntity.FindComponent(AIControlComponent));
		if (!control)
			return -1;

		AIAgent agent = control.GetControlAIAgent();
		if (!agent)
			return -1;

		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
		if (!utility || !utility.m_ThreatSystem)
			return -1;

		return utility.m_ThreatSystem.GetState();
	}

	//------------------------------------------------------------------------------------------------
	//! Diagnostics only: the AI character closest to the weapon's aim line, whatever the cone, with the
	//! cosine of its angle, its distance and whether the chest is in clear line of fire.
	protected string DescribeAim(IEntity player, array<IEntity> aiEntities)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(player);
		if (!character || !character.GetWeaponAimingComponent())
			return "no weapon aiming component";

		vector aimDir = character.GetWeaponAimingComponent().GetAimingDirectionWorld();
		vector eye = EyeOf(player);

		IEntity closest;
		float closestDot = -1;

		foreach (IEntity aiEntity : aiEntities)
		{
			if (aiEntity == player)
				continue;

			float dot = vector.Dot(aimDir, vector.Direction(eye, BodyPoint(aiEntity.GetOrigin(), EyeOf(aiEntity), 1)).Normalized());
			if (dot <= closestDot)
				continue;

			closest = aiEntity;
			closestDot = dot;
		}

		if (!closest)
			return string.Format("aimDir=%1 no AI", aimDir);

		vector chest = BodyPoint(closest.GetOrigin(), EyeOf(closest), 1);
		return string.Format("aimDir=%1 closest=%2 dot=%3 dist=%4 chestClear=%5", aimDir, closest, closestDot, vector.Distance(eye, chest), TraceFraction(eye, chest, closest, player) >= 1);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsAliveCharacter(IEntity entity)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		if (!character)
			return false;

		CharacterControllerComponent controller = character.GetCharacterController();
		return controller && !controller.IsDead();
	}

	//------------------------------------------------------------------------------------------------
	//! TARGET_DISGUISE_SIDE: the outfit faction or anyone it is not hostile to; attacking it gives the
	//! player away. TARGET_OUTFIT_ENEMY: someone the outfit faction is hostile to; attacking it is what
	//! that faction would do anyway. TARGET_NONE: nobody, or no faction.
	protected int ClassifyTarget(IEntity target, Faction outfitFaction)
	{
		if (!target || !outfitFaction)
			return TARGET_NONE;

		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(target.FindComponent(FactionAffiliationComponent));
		if (!affiliation)
			return TARGET_NONE;

		Faction targetFaction = affiliation.GetAffiliatedFaction();
		if (!targetFaction)
			return TARGET_NONE;

		if (outfitFaction.IsFactionEnemy(targetFaction))
			return TARGET_OUTFIT_ENEMY;

		return TARGET_DISGUISE_SIDE;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds a one-off amount and breaks at MAX_SUSPICION.
	protected void RaiseSuspicion(ARGA_IncognitoState state, float amount, string rule, IEntity witness)
	{
		SetSuspicion(state, state.m_fSuspicion + amount);

		if (state.m_fSuspicion >= MAX_SUSPICION)
			Break(state, "suspicion:" + rule, witness);
	}

	//------------------------------------------------------------------------------------------------
	//! First qualifying observer within radius, or null. exclude skips one entity (a kill's own victim).
	protected IEntity FindObserverInRange(array<IEntity> aiEntities, IEntity player, vector playerPos, float radius, Faction realFaction, Faction outfitFaction, bool requireSight, IEntity exclude = null, bool requireCalm = false)
	{
		if (radius <= 0)
			return null;

		foreach (IEntity aiEntity : aiEntities)
		{
			if (aiEntity == exclude)
				continue;

			// Distance first: it is a vector subtraction, while IsObserver() does component lookups.
			if (vector.Distance(playerPos, aiEntity.GetOrigin()) > radius)
				continue;

			if (!IsObserver(aiEntity, player, realFaction, outfitFaction))
				continue;

			if (requireCalm && IsInCombat(aiEntity))
				continue;

			if (requireSight && !Sees(aiEntity, player))
				continue;

			return aiEntity;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Diagnostics only: the closest alive hunter within radius, whether it sees the player or not.
	protected IEntity FindClosestHunter(array<IEntity> aiEntities, IEntity player, float radius, Faction realFaction)
	{
		vector playerPos = player.GetOrigin();
		IEntity closest;
		float closestDistance = radius;

		foreach (IEntity aiEntity : aiEntities)
		{
			float distance = vector.Distance(playerPos, aiEntity.GetOrigin());
			if (distance > closestDistance)
				continue;

			if (!IsHunter(aiEntity, realFaction))
				continue;

			closest = aiEntity;
			closestDistance = distance;
		}

		return closest;
	}

	//------------------------------------------------------------------------------------------------
	//! Diagnostics only: what aiEntity's perception currently holds about player.
	protected string DescribePerception(IEntity aiEntity, IEntity player)
	{
		float distance = vector.Distance(player.GetOrigin(), aiEntity.GetOrigin());

		PerceptionComponent perception = PerceptionComponent.Cast(aiEntity.FindComponent(PerceptionComponent));
		if (!perception)
			return string.Format("ai=%1 dist=%2 perception=NONE", aiEntity, distance);

		BaseTarget target = perception.FindTargetPerceptionObject(player);
		if (!target)
			return string.Format("ai=%1 dist=%2 target=NONE", aiEntity, distance);

		// interval is how stale trace/sinceSeen can be: perception refreshes slower at higher LOD.
		return string.Format("ai=%1 dist=%2 category=%3 sinceSeen=%4 sinceDetected=%5 trace=%6 exposure=%7 interval=%8",
			aiEntity,
			distance,
			typename.EnumToString(ETargetCategory, target.GetTargetCategory()),
			target.GetTimeSinceSeen(),
			target.GetTimeSinceDetected(),
			target.GetTraceFraction(),
			target.GetExposure(),
			perception.GetUpdateInterval());
	}

	//------------------------------------------------------------------------------------------------
	//! Scans aiEntities for one hunter within radius that currently sees player. Always requires sight.
	protected bool HasHunterInRange(array<IEntity> aiEntities, IEntity player, float radius, Faction realFaction)
	{
		if (radius <= 0)
			return false;

		vector playerPos = player.GetOrigin();

		foreach (IEntity aiEntity : aiEntities)
		{
			if (vector.Distance(playerPos, aiEntity.GetOrigin()) > radius)
				continue;

			if (!IsHunter(aiEntity, realFaction))
				continue;

			if (!Sees(aiEntity, player))
				continue;

			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Clears the AI override so every AI resolves the player's real faction. No-op if already broken.
	//! Deliberately does NOT call SCR_FactionManager.RequestUpdateAllTargetsFactions(): it froze AI targets.
	protected void Break(ARGA_IncognitoState state, string reason, IEntity witness)
	{
		if (state.m_bBroken)
			return;

		IEntity entity = state.m_Entity;
		if (!entity)
			return;

		// Logged before touching the override, so it shows what the AI held while still fooled.
		if (m_bDebugLog && witness)
		{
			float sinceRestore = -1;
			if (state.m_fRestoredTime > 0)
				sinceRestore = (GetGame().GetWorld().GetWorldTime() - state.m_fRestoredTime) * 0.001;

			Print(string.Format("[ARGA_Incognito][Debug] Break witness reason=%1 sinceRestore=%2s threat=%3 %4", reason, sinceRestore, ThreatStateOf(witness), DescribePerception(witness, entity)), LogLevel.NORMAL);
		}

		PerceivableComponent perceivable = PerceivableComponent.Cast(entity.FindComponent(PerceivableComponent));
		if (!perceivable)
			return;

		perceivable.SetPerceivedFactionOverride(null);

		state.m_bBroken = true;
		state.m_fLastSeenTime = GetGame().GetWorld().GetWorldTime();
		SetSuspicion(state, MAX_SUSPICION);

		Print(string.Format("[ARGA_Incognito] Broken playerId=%1 reason=%2", state.m_iPlayerId, reason), LogLevel.NORMAL);

		if (m_bReinforcementsEnabled)
			SendReinforcements(entity.GetOrigin());
	}

	//------------------------------------------------------------------------------------------------
	//! Re-tasks a live group within the reuse radius, or the closest one once the cap is reached;
	//! otherwise spawns a new group.
	protected void SendReinforcements(vector breakPos)
	{
		PruneReinforcements();

		ARGA_IncognitoReinforcement closest;
		float closestDistance = float.MAX;

		foreach (ARGA_IncognitoReinforcement reinforcement : m_aReinforcements)
		{
			float distance = vector.Distance(breakPos, GroupPosition(reinforcement.m_Group));
			if (distance >= closestDistance)
				continue;

			closest = reinforcement;
			closestDistance = distance;
		}

		bool atCap = m_aReinforcements.Count() >= m_iMaxReinforcementGroups;
		if (!closest || (closestDistance > m_fReinforcementReuseRadius && !atCap))
		{
			SpawnReinforcements(breakPos);
			return;
		}

		RetaskReinforcement(closest, breakPos);
		Print(string.Format("[ARGA_Incognito] Reinforcements re-tasked to %1, group was %2m away, atCap=%3", breakPos, closestDistance, atCap), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void RetaskReinforcement(ARGA_IncognitoReinforcement reinforcement, vector breakPos)
	{
		if (reinforcement.m_Waypoint)
		{
			reinforcement.m_Group.RemoveWaypoint(reinforcement.m_Waypoint);
			SCR_EntityHelper.DeleteEntityAndChildren(reinforcement.m_Waypoint);
		}

		reinforcement.m_Waypoint = SpawnReinforcementWaypoint(breakPos);
		if (reinforcement.m_Waypoint)
			reinforcement.m_Group.AddWaypoint(reinforcement.m_Waypoint);

		reinforcement.m_fDespawnAt = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Position of the first member still standing; the group entity itself does not follow its members.
	protected vector GroupPosition(SCR_AIGroup group)
	{
		array<AIAgent> agents = {};
		group.GetAgents(agents);

		foreach (AIAgent agent : agents)
		{
			IEntity member = agent.GetControlledEntity();
			if (member)
				return member.GetOrigin();
		}

		return group.GetOrigin();
	}

	//------------------------------------------------------------------------------------------------
	//! Drops groups that were deleted or wiped out, past the spawn grace period.
	protected void PruneReinforcements()
	{
		float now = GetGame().GetWorld().GetWorldTime();

		for (int i = m_aReinforcements.Count() - 1; i >= 0; i--)
		{
			ARGA_IncognitoReinforcement reinforcement = m_aReinforcements[i];
			if (reinforcement.m_Group && (reinforcement.m_Group.GetAgentsCount() > 0 || now - reinforcement.m_fSpawnedAt < REINFORCEMENT_SPAWN_GRACE_MS))
				continue;

			DeleteReinforcement(reinforcement);
			m_aReinforcements.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Same order vanilla uses to despawn a group: members first, then the group. Skips anything already
	//! being deleted, which is everything when the world itself is shutting down.
	protected void DeleteReinforcement(ARGA_IncognitoReinforcement reinforcement)
	{
		if (reinforcement.m_Waypoint && !reinforcement.m_Waypoint.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(reinforcement.m_Waypoint);

		if (!reinforcement.m_Group || reinforcement.m_Group.IsDeleted())
			return;

		array<AIAgent> agents = {};
		reinforcement.m_Group.GetAgents(agents);

		foreach (AIAgent agent : agents)
		{
			IEntity member = agent.GetControlledEntity();
			if (member && !member.IsDeleted())
				RplComponent.DeleteRplEntity(member, false);
		}

		RplComponent.DeleteRplEntity(reinforcement.m_Group, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Any broken disguise holds every group. Once none is broken, each group waits its despawn time and
	//! then leaves as soon as no player within the sight radius sees any of its members.
	protected void TickReinforcements(bool anyBroken)
	{
		if (m_aReinforcements.IsEmpty())
			return;

		PruneReinforcements();

		float now = GetGame().GetWorld().GetWorldTime();

		for (int i = m_aReinforcements.Count() - 1; i >= 0; i--)
		{
			ARGA_IncognitoReinforcement reinforcement = m_aReinforcements[i];

			if (anyBroken)
			{
				reinforcement.m_fDespawnAt = 0;
				continue;
			}

			if (reinforcement.m_fDespawnAt <= 0)
			{
				reinforcement.m_fDespawnAt = now + m_fReinforcementDespawnSeconds * 1000;
				continue;
			}

			if (now < reinforcement.m_fDespawnAt)
				continue;

			if (IsSeenByAnyPlayer(reinforcement.m_Group))
				continue;

			DeleteReinforcement(reinforcement);
			m_aReinforcements.Remove(i);

			Print("[ARGA_Incognito] Reinforcements despawned out of sight.", LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsSeenByAnyPlayer(SCR_AIGroup group)
	{
		array<AIAgent> agents = {};
		group.GetAgents(agents);

		foreach (int playerId, ARGA_IncognitoState state : m_mStates)
		{
			IEntity player = state.m_Entity;
			if (!player)
				continue;

			foreach (AIAgent agent : agents)
			{
				IEntity member = agent.GetControlledEntity();
				if (!member)
					continue;

				if (vector.Distance(player.GetOrigin(), member.GetOrigin()) > m_fReinforcementDespawnSightRadius)
					continue;

				if (Sees(player, member))
					return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the reinforcement group at the editor's distance and bearing from breakPos, and sends it
	//! to breakPos: where the alarm was raised, not wherever the player goes next.
	protected void SpawnReinforcements(vector breakPos)
	{
		Resource groupResource = Resource.Load(m_sReinforcementGroup);
		if (!groupResource || !groupResource.IsValid())
		{
			Print(string.Format("[ARGA_Incognito] Reinforcement group '%1' is not a valid prefab, nothing spawned.", m_sReinforcementGroup), LogLevel.ERROR);
			return;
		}

		BaseWorld world = GetGame().GetWorld();
		float bearing = m_fReinforcementBearing * Math.DEG2RAD;

		vector spawnPos = breakPos;
		spawnPos[0] = spawnPos[0] + Math.Sin(bearing) * m_fReinforcementDistance;
		spawnPos[2] = spawnPos[2] + Math.Cos(bearing) * m_fReinforcementDistance;
		spawnPos[1] = world.GetSurfaceY(spawnPos[0], spawnPos[2]);

		SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().SpawnEntityPrefab(groupResource, world, SpawnParamsAt(spawnPos)));
		if (!group)
		{
			Print(string.Format("[ARGA_Incognito] Could not spawn reinforcement group '%1'.", m_sReinforcementGroup), LogLevel.ERROR);
			return;
		}

		AIWaypoint waypoint = SpawnReinforcementWaypoint(breakPos);
		if (waypoint)
			group.AddWaypoint(waypoint);

		ARGA_IncognitoReinforcement reinforcement = new ARGA_IncognitoReinforcement();
		reinforcement.m_Group = group;
		reinforcement.m_Waypoint = waypoint;
		reinforcement.m_fSpawnedAt = GetGame().GetWorld().GetWorldTime();
		m_aReinforcements.Insert(reinforcement);

		Print(string.Format("[ARGA_Incognito] Reinforcements spawned at %1, heading to %2", spawnPos, breakPos), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected AIWaypoint SpawnReinforcementWaypoint(vector pos)
	{
		Resource waypointResource = Resource.Load(m_sReinforcementWaypoint);
		if (!waypointResource || !waypointResource.IsValid())
		{
			Print(string.Format("[ARGA_Incognito] Reinforcement waypoint '%1' is not a valid prefab, the group gets no orders.", m_sReinforcementWaypoint), LogLevel.ERROR);
			return null;
		}

		AIWaypoint waypoint = AIWaypoint.Cast(GetGame().SpawnEntityPrefab(waypointResource, GetGame().GetWorld(), SpawnParamsAt(pos)));
		if (!waypoint)
		{
			Print(string.Format("[ARGA_Incognito] Could not spawn reinforcement waypoint '%1', the group gets no orders.", m_sReinforcementWaypoint), LogLevel.ERROR);
			return null;
		}

		waypoint.SetCompletionRadius(m_fReinforcementWaypointRadius);
		return waypoint;
	}

	//------------------------------------------------------------------------------------------------
	protected EntitySpawnParams SpawnParamsAt(vector pos)
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = pos;
		return params;
	}

	//------------------------------------------------------------------------------------------------
	//! Re-applies the outfit's AI override once recovery time has passed unseen.
	protected void Restore(ARGA_IncognitoState state)
	{
		IEntity entity = state.m_Entity;
		if (!entity)
			return;

		SCR_CharacterFactionAffiliationComponent affiliation = SCR_CharacterFactionAffiliationComponent.Cast(entity.FindComponent(SCR_CharacterFactionAffiliationComponent));
		if (!affiliation)
			return;

		ApplyPerceivedFactionForAI(entity, affiliation.GetPerceivedFaction());

		state.m_bBroken = false;
		state.m_fRestoredTime = GetGame().GetWorld().GetWorldTime();

		Print(string.Format("[ARGA_Incognito] Restored playerId=%1", state.m_iPlayerId), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Shooting trigger. OnProjectileShot fires on the weapon's owner entity; playerID identifies which
	//! watched state (if any) fired.
	protected void OnWeaponFired(int playerID, BaseWeaponComponent weapon, IEntity entity)
	{
		ARGA_IncognitoState state = m_mStates.Get(playerID);
		if (!state || state.m_bBroken || !state.m_Entity)
			return;

		SCR_CharacterFactionAffiliationComponent affiliation = SCR_CharacterFactionAffiliationComponent.Cast(state.m_Entity.FindComponent(SCR_CharacterFactionAffiliationComponent));
		if (!affiliation || !IsDisguised(affiliation))
			return;

		Faction realFaction = affiliation.GetAffiliatedFaction();
		Faction outfitFaction = affiliation.GetPerceivedFaction();

		array<IEntity> aiEntities = {};
		CollectAIEntities(aiEntities);

		IEntity aimed;
		int target = ClassifyAim(state.m_Entity, aiEntities, outfitFaction, aimed);

		if (m_bDebugLog)
		{
			Print(string.Format("[ARGA_Incognito][Debug] Shot playerId=%1 target=%2 aimed=%3", state.m_iPlayerId, target, aimed), LogLevel.NORMAL);
			if (target == TARGET_NONE)
				Print(string.Format("[ARGA_Incognito][Debug] Aim %1", DescribeAim(state.m_Entity, aiEntities)), LogLevel.NORMAL);
		}

		// Shooting at an enemy of the outfit is what that faction would do itself.
		if (target == TARGET_OUTFIT_ENEMY)
		{
			EnterOwnCombat(state, "shot at outfit enemy");
			return;
		}

		vector playerPos = state.m_Entity.GetOrigin();

		// The victim is judged only after the shot lands, even when he is looking at the player.
		IEntity witness = FindObserverInRange(aiEntities, state.m_Entity, playerPos, m_fShotRadius, realFaction, outfitFaction, requireSight: true, exclude: aimed);

		if (target == TARGET_DISGUISE_SIDE)
		{
			if (witness)
			{
				Break(state, "shot", witness);
				return;
			}

			// The victim knows where the shot came from even without looking, unless it took him out.
			if (vector.Distance(playerPos, aimed.GetOrigin()) <= m_fShotRadius)
				GetGame().GetCallqueue().CallLater(CheckVictimWitness, VICTIM_CHECK_MS, false, state.m_iPlayerId, aimed);

			return;
		}

		if (IsInOwnCombat(state))
			return;

		// Only an observer who is not fighting himself finds a stray shot odd.
		IEntity calmWitness = FindObserverInRange(aiEntities, state.m_Entity, playerPos, m_fShotRadius, realFaction, outfitFaction, requireSight: true, requireCalm: true);
		if (calmWitness)
			RaiseSuspicion(state, m_fStrayShotSuspicion, "stray shot", calmWitness);
	}

	//------------------------------------------------------------------------------------------------
	//! Runs once the shot has landed: a victim still conscious gives the shooter away.
	protected void CheckVictimWitness(int playerId, IEntity victim)
	{
		ARGA_IncognitoState state = m_mStates.Get(playerId);
		if (!state || state.m_bBroken || !state.m_Entity || !victim)
			return;

		// Like any other witness, the victim must be hostile to the player's real faction.
		SCR_CharacterFactionAffiliationComponent affiliation = SCR_CharacterFactionAffiliationComponent.Cast(state.m_Entity.FindComponent(SCR_CharacterFactionAffiliationComponent));
		if (!affiliation || !IsHunter(victim, affiliation.GetAffiliatedFaction()))
			return;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(victim);
		if (!character)
			return;

		CharacterControllerComponent controller = character.GetCharacterController();
		if (!controller || controller.GetLifeState() != ECharacterLifeState.ALIVE)
			return;

		Break(state, "shot:victim", victim);
	}

	//------------------------------------------------------------------------------------------------
	//! Kill trigger. Fires for every controllable death; only the killer's watched state (if any) matters.
	protected void OnControllableDestroyed(notnull SCR_InstigatorContextData instigatorContextData)
	{
		int killerId = instigatorContextData.GetKillerPlayerID();
		if (killerId <= 0)
			return;

		ARGA_IncognitoState state = m_mStates.Get(killerId);
		if (!state || state.m_bBroken || !state.m_Entity)
			return;

		SCR_CharacterFactionAffiliationComponent affiliation = SCR_CharacterFactionAffiliationComponent.Cast(state.m_Entity.FindComponent(SCR_CharacterFactionAffiliationComponent));
		if (!affiliation || !IsDisguised(affiliation))
			return;

		Faction realFaction = affiliation.GetAffiliatedFaction();
		Faction outfitFaction = affiliation.GetPerceivedFaction();

		// Killing an enemy of the outfit is what that faction would do itself.
		if (ClassifyTarget(instigatorContextData.GetVictimEntity(), outfitFaction) == TARGET_OUTFIT_ENEMY)
			return;

		array<IEntity> aiEntities = {};
		CollectAIEntities(aiEntities);

		vector playerPos = state.m_Entity.GetOrigin();

		IEntity witness = FindObserverInRange(aiEntities, state.m_Entity, playerPos, m_fWitnessRadius, realFaction, outfitFaction, requireSight: true, exclude: instigatorContextData.GetVictimEntity());
		if (witness)
			Break(state, "kill", witness);
	}

	//------------------------------------------------------------------------------------------------
	//! Server poll: evaluates the aiming, sprinting, voice and proximity triggers, and drives recovery.
	//! Collects the AI list once and shares it across every watched player.
	//! Diagnostics only: tracks what the nearest hunter perceives, tick by tick, so trace and exposure can
	//! be watched against real cover instead of only at the instant of a break. Prints on change.
	protected void LogClosestHunter(ARGA_IncognitoState state, IEntity entity, Faction realFaction, array<IEntity> aiEntities)
	{
		IEntity hunter = FindClosestHunter(aiEntities, entity, m_fWitnessRadius, realFaction);
		if (!hunter)
			return;

		PerceptionComponent perception = PerceptionComponent.Cast(hunter.FindComponent(PerceptionComponent));
		if (!perception)
			return;

		BaseTarget target = perception.FindTargetPerceptionObject(entity);
		if (!target)
			return;

		bool sees = Sees(hunter, entity);

		// Deduped on visibility plus a 5 m distance bucket, so the log follows the player's approach
		// without printing every tick.
		string key = string.Format("%1 %2 %3 %4 %5",
			typename.EnumToString(ETargetCategory, target.GetTargetCategory()),
			target.GetTraceFraction(),
			target.GetExposure(),
			sees,
			Math.Round(vector.Distance(entity.GetOrigin(), hunter.GetOrigin()) / 5));

		if (key == state.m_sLastPerception)
			return;

		state.m_sLastPerception = key;
		Print(string.Format("[ARGA_Incognito][Debug] Watch sees=%1 why=%2 playerPos=%3 %4", sees, m_sLastSeeFail, entity.GetOrigin(), DescribePerception(hunter, entity)), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		m_iTraceCount = 0;
		m_iFovRejects = 0;

		array<IEntity> aiEntities = {};
		CollectAIEntities(aiEntities);

		bool anyBroken;

		foreach (int playerId, ARGA_IncognitoState state : m_mStates)
		{
			SyncWatchedEntity(state);

			if (state.m_bBroken)
				anyBroken = true;

			IEntity entity = state.m_Entity;
			if (!entity)
				continue;

			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
			if (!character)
				continue;

			CharacterControllerComponent controller = character.GetCharacterController();
			if (!controller || controller.IsDead())
				continue;

			SCR_CharacterFactionAffiliationComponent affiliation = SCR_CharacterFactionAffiliationComponent.Cast(entity.FindComponent(SCR_CharacterFactionAffiliationComponent));
			if (!affiliation)
				continue;

			Faction realFaction = affiliation.GetAffiliatedFaction();

			if (state.m_bBroken)
			{
				TickRecovery(state, entity, realFaction, aiEntities);
				continue;
			}

			if (!IsDisguised(affiliation))
				continue;

			if (m_bDebugLog)
				LogClosestHunter(state, entity, realFaction, aiEntities);

			if (!EvaluateBreakRules(state, entity, controller, affiliation, aiEntities))
				DecaySuspicion(state, entity, realFaction, aiEntities);
		}

		TickReinforcements(anyBroken);

		if (m_bDebugLog && (m_iTraceCount > 0 || m_iFovRejects > 0))
			Print(string.Format("[ARGA_Incognito][Debug] Tick traces=%1 fovRejects=%2 ai=%3", m_iTraceCount, m_iFovRejects, aiEntities.Count()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! CharacterControllerComponent.IsSprinting() is not replicated to a dedicated server: it reads false
	//! there for every remote player, however fast they run. GetMovementSpeed() is.
	protected bool IsSprinting(CharacterControllerComponent controller)
	{
		return controller.IsSprinting() || controller.GetMovementSpeed() >= SPRINT_MOVEMENT_SPEED;
	}

	//------------------------------------------------------------------------------------------------
	//! Diagnostics only: compares the three ways of telling that a player is running, to find which one
	//! survives replication on a dedicated server. Speed is measured from replicated positions, which is
	//! the only one that cannot be local-side state. Prints when the measured speed changes by 1 m/s.
	protected void LogSpeed(ARGA_IncognitoState state, IEntity entity, CharacterControllerComponent controller, bool sprinting)
	{
		vector pos = entity.GetOrigin();
		float now = GetGame().GetWorld().GetWorldTime();
		float elapsed = (now - state.m_fLastPosTime) * 0.001;

		float measured;
		if (state.m_fLastPosTime > 0 && elapsed > 0)
			measured = vector.Distance(pos, state.m_vLastPos) / elapsed;

		state.m_vLastPos = pos;
		state.m_fLastPosTime = now;

		int bucket = Math.Round(measured);
		if (bucket == state.m_iLastSpeedBucket)
			return;

		state.m_iLastSpeedBucket = bucket;
		Print(string.Format("[ARGA_Incognito][Debug] Speed playerId=%1 measured=%2 movementSpeed=%3 sprinting=%4 stance=%5",
			state.m_iPlayerId,
			measured,
			controller.GetMovementSpeed(),
			sprinting,
			controller.GetStance()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! One pass over the AI list per player, instead of one per rule. Distance and IsObserver are paid
	//! once per AI, and so is the sight ray. Voice breaks at once and ends the pass; the sight rules keep
	//! the strongest observer per rule and add up. Returns true when suspicion was raised this tick.
	protected bool EvaluateBreakRules(ARGA_IncognitoState state, IEntity entity, CharacterControllerComponent controller, SCR_CharacterFactionAffiliationComponent affiliation, array<IEntity> aiEntities)
	{
		if (IsEngagedByOutfitEnemy(entity, affiliation.GetPerceivedFaction(), aiEntities))
			EnterOwnCombat(state, "engaged by outfit enemy");

		// A player trading fire with a common enemy runs, sweeps his muzzle and stands close like
		// everyone else. Only talking, or shooting the outfit's own side, still gives him away.
		bool ownCombat = IsInOwnCombat(state);

		// Only aiming at the outfit's own side is suspicious; the cone search runs only with the weapon up.
		float aimRadius;
		IEntity aimTarget;
		if (!ownCombat && (controller.IsWeaponRaised() || controller.IsWeaponADS()) && ClassifyAim(entity, aiEntities, affiliation.GetPerceivedFaction(), aimTarget) == TARGET_DISGUISE_SIDE)
			aimRadius = m_fAimRadius;

		bool sprinting = IsSprinting(controller);

		if (m_bDebugLog)
			LogSpeed(state, entity, controller, sprinting);

		float sprintRadius;
		if (sprinting && !ownCombat)
			sprintRadius = m_fSprintRadius;

		float voiceRadius;
		if (SpokeSinceLastTick(state))
			voiceRadius = state.m_fVoiceRadius;

		float maxRadius = Math.Max(Math.Max(aimRadius, sprintRadius), Math.Max(voiceRadius, m_fProximityRadius));
		if (maxRadius <= 0)
			return false;

		Faction realFaction = affiliation.GetAffiliatedFaction();
		Faction outfitFaction = affiliation.GetPerceivedFaction();
		vector playerPos = entity.GetOrigin();

		float aimGain;
		float sprintGain;
		float proximityGain;
		float strongestGain;
		string strongestRule;
		IEntity strongestWitness;
		float gain;

		foreach (IEntity aiEntity : aiEntities)
		{
			float distance = vector.Distance(playerPos, aiEntity.GetOrigin());
			if (distance > maxRadius)
				continue;

			if (!IsObserver(aiEntity, entity, realFaction, outfitFaction))
				continue;

			// Voice carries through walls, so it is resolved before spending the ray.
			if (voiceRadius > 0 && distance <= voiceRadius)
			{
				Break(state, "voice", aiEntity);
				return true;
			}

			if (!Sees(aiEntity, entity))
				continue;

			// A muzzle swept across an ally in a firefight is not a threat to him.
			if (aimRadius > 0 && distance <= aimRadius && !IsInCombat(aiEntity))
			{
				gain = ScaledGain(m_fAimSuspicionRate, distance, aimRadius);
				aimGain = Math.Max(aimGain, gain);
				if (gain > strongestGain)
				{
					strongestGain = gain;
					strongestRule = "aiming";
					strongestWitness = aiEntity;
				}
			}

			// Everyone runs in a firefight; only a calm observer finds it odd.
			if (sprintRadius > 0 && distance <= sprintRadius && !IsInCombat(aiEntity))
			{
				gain = ScaledGain(m_fSprintSuspicionRate, distance, sprintRadius);
				sprintGain = Math.Max(sprintGain, gain);
				if (gain > strongestGain)
				{
					strongestGain = gain;
					strongestRule = "sprinting";
					strongestWitness = aiEntity;
				}
			}

			// Allies stand close in a firefight; only a calm observer finds it odd.
			if (m_fProximityRadius > 0 && distance <= m_fProximityRadius && !ownCombat && !IsInCombat(aiEntity))
			{
				gain = ScaledGain(m_fProximitySuspicionRate, distance, m_fProximityRadius);
				proximityGain = Math.Max(proximityGain, gain);
				if (gain > strongestGain)
				{
					strongestGain = gain;
					strongestRule = "proximity";
					strongestWitness = aiEntity;
				}
			}
		}

		float perSecond = aimGain + sprintGain + proximityGain;
		if (perSecond <= 0)
			return false;

		RaiseSuspicion(state, perSecond * TICK_MS * 0.001, strongestRule, strongestWitness);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Full rate at point blank, half at the edge of the rule's radius.
	protected float ScaledGain(float rate, float distance, float radius)
	{
		return rate * (1 - 0.5 * distance / radius);
	}

	//------------------------------------------------------------------------------------------------
	//! Clamped setter. Diagnostics print every 10 points crossed, so the log shows the climb and the fall.
	protected void SetSuspicion(ARGA_IncognitoState state, float value)
	{
		state.m_fSuspicion = Math.Clamp(value, 0, MAX_SUSPICION);

		if (!m_bDebugLog)
			return;

		int bucket = Math.Floor(state.m_fSuspicion / 10);
		if (bucket == state.m_iLastSuspicionBucket)
			return;

		state.m_iLastSuspicionBucket = bucket;
		Print(string.Format("[ARGA_Incognito][Debug] Suspicion playerId=%1 value=%2 broken=%3", state.m_iPlayerId, state.m_fSuspicion, state.m_bBroken), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Doubles when no hostile AI is within the witness radius at all. Distance and faction only, no ray.
	protected void DecaySuspicion(ARGA_IncognitoState state, IEntity entity, Faction realFaction, array<IEntity> aiEntities)
	{
		if (state.m_fSuspicion <= 0)
			return;

		float rate = m_fSuspicionDecayRate;
		if (!FindClosestHunter(aiEntities, entity, m_fWitnessRadius, realFaction))
			rate = rate * 2;

		SetSuspicion(state, state.m_fSuspicion - rate * TICK_MS * 0.001);
	}

	//------------------------------------------------------------------------------------------------
	//! While broken: keeps the override null, holds suspicion while a hunter sees the player, lets it decay
	//! otherwise, and restores at m_fRestoreThreshold.
	protected void TickRecovery(ARGA_IncognitoState state, IEntity entity, Faction realFaction, array<IEntity> aiEntities)
	{
		PerceivableComponent perceivable = PerceivableComponent.Cast(entity.FindComponent(PerceivableComponent));
		if (!perceivable)
			return;

		if (perceivable.GetPerceivedFactionOverride())
			perceivable.SetPerceivedFactionOverride(null);

		if (HasHunterInRange(aiEntities, entity, m_fWitnessRadius, realFaction))
		{
			state.m_fLastSeenTime = GetGame().GetWorld().GetWorldTime();
			return;
		}

		DecaySuspicion(state, entity, realFaction, aiEntities);
		if (state.m_fSuspicion > m_fRestoreThreshold)
			return;

		if (m_bDebugLog)
		{
			IEntity closest = FindClosestHunter(aiEntities, entity, m_fWitnessRadius, realFaction);
			if (closest)
				Print(string.Format("[ARGA_Incognito][Debug] Restore closest hunter %1", DescribePerception(closest, entity)), LogLevel.NORMAL);
			else
				Print("[ARGA_Incognito][Debug] Restore: no hunter within witness radius", LogLevel.NORMAL);
		}

		Restore(state);
	}
}
