// Where the HiveMind plugs into the game and into DayZ Expansion AI. Kept thin on purpose:
// an Expansion update can only break these few overrides.

modded class PlayerBase
{
	protected string m_AFHM_LastHitZone;
	protected string m_AFHM_Profile;

	string AFHM_GetLastHitZone()
	{
		return m_AFHM_LastHitZone;
	}

	// What the HiveMind set this bot to, and why (empty for players and untuned bots).
	string AFHM_GetProfile()
	{
		return m_AFHM_Profile;
	}

	void AFHM_SetProfile(string profile)
	{
		m_AFHM_Profile = profile;
	}

	// Quest givers and traders are Expansion AI too, but they are not enemies: they are never
	// tuned and fights with them teach nothing. Checked by config class, so this works whether
	// or not the Quests and Market mods are loaded.
	bool AFHM_IsServiceNPC()
	{
		return IsKindOf("ExpansionQuestNPCAIBase") || IsKindOf("ExpansionTraderAIBase");
	}

	// The player or bot holding (or being) the thing that did the damage; null for anything else.
	protected PlayerBase AFHM_Opponent(Object source)
	{
		EntityAI entity = EntityAI.Cast(source);
		if (!entity)
			return null;
		// Run over by a vehicle: the driver did it.
		Transport vehicle = Transport.Cast(entity);
		if (vehicle)
		{
			PlayerBase driver = PlayerBase.Cast(vehicle.CrewMember(DayZPlayerConstants.VEHICLESEAT_DRIVER));
			if (driver == this)
				return null;
			return driver;
		}
		PlayerBase other = PlayerBase.Cast(entity.GetHierarchyRootPlayer());
		if (other == this)
			return null;
		return other;
	}

	override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
	{
		super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);

		if (!GetGame().IsServer())
			return;
		m_AFHM_LastHitZone = dmgZone;

		PlayerBase other = AFHM_Opponent(source);
		// Only player-versus-bot hits teach it anything.
		if (!other || other.IsAI() == IsAI() || AFHM_IsServiceNPC() || other.AFHM_IsServiceNPC())
			return;
		AFHiveMind hm = AFHiveMind.Get();
		if (hm)
			hm.RecordHit(this, other, dmgZone);
	}

	override void EEKilled(Object killer)
	{
		if (GetGame().IsServer())
		{
			PlayerBase other = AFHM_Opponent(killer);
			if (other && other.IsAI() != IsAI() && !AFHM_IsServiceNPC() && !other.AFHM_IsServiceNPC())
			{
				AFHiveMind hm = AFHiveMind.Get();
				if (hm)
				{
					if (IsAI())
						hm.RecordBotKilled(this, other, EntityAI.Cast(killer));
					else
						hm.RecordPlayerKilled(this, other, EntityAI.Cast(killer));
				}
			}
		}

		super.EEKilled(killer);
	}

	protected void AFHM_SendReport(PlayerIdentity to, array<string> lines)
	{
		ScriptRPC rpc = new ScriptRPC();
		rpc.Write(lines);
		rpc.Send(this, AFHM_Const.RPC_REPORT, true, to);
	}

	override void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
	{
		super.OnRPC(sender, rpc_type, ctx);

		if (rpc_type == AFHM_Const.RPC_REQUEST)
		{
			if (!GetGame().IsServer())
				return;
			AFHiveMind hm = AFHiveMind.Get();
			if (!hm)
				return;
			// Sent line by line: one long string does not survive the trip.
			array<string> lines = new array<string>;
			if (hm.IsAdmin(sender))
				lines = hm.PanelLines();
			else
				lines.Insert("AF HiveMind: you are not listed under Admins in config.json.");
			AFHM_SendReport(sender, lines);
		}
		else if (rpc_type == AFHM_Const.RPC_COMMAND)
		{
			// A button on the admin controls panel. Answered with a fresh report.
			if (!GetGame().IsServer())
				return;
			AFHiveMind hmCmd = AFHiveMind.Get();
			string cmd;
			if (!hmCmd || !ctx.Read(cmd))
				return;
			array<string> cmdLines = new array<string>;
			if (hmCmd.IsAdmin(sender))
			{
				hmCmd.AdminCommand(sender, cmd);
				cmdLines = hmCmd.PanelLines();
			}
			else
			{
				cmdLines.Insert("AF HiveMind: you are not listed under Admins in config.json.");
				AFHM_Util.Log("refused admin command '" + cmd + "' from a player not listed under Admins");
			}
			AFHM_SendReport(sender, cmdLines);
		}
		else if (rpc_type == AFHM_Const.RPC_REPORT)
		{
			if (GetGame().IsDedicatedServer())
				return;
			array<string> report = new array<string>;
			if (ctx.Read(report))
			{
				AFHM_ClientState.s_Report = report;
				AFHM_ClientState.s_Stamp++;
			}
		}
	}
};

modded class eAIDynamicPatrol
{
	override void SetupAI(eAIBase ai)
	{
		super.SetupAI(ai);

		AFHiveMind hm = AFHiveMind.Get();
		if (!hm)
			return;
		string name;
		if (m_Config)
			name = m_Config.Name;
		// A patrol Expansion saves across restarts is rebuilt from its save each time, and runs
		// through here again: changing its gear then would take away what the bots carried.
		bool fresh = !m_Config || !m_Config.Persist;
		hm.TuneBot(ai, name, m_Position, fresh);
	}
};

// Bots that no patrol tunes: other mods (Moving AI Convoy, BS Patrol Helicopter and Tank, ...)
// create Expansion bots directly. Each bot gets a few seconds for its own mod to set it up;
// if no patrol has tuned it by then, it is tuned from whatever values it ended up with.
modded class eAIBase
{
	protected bool m_AFHM_Tuned;
	protected string m_AFHM_SwapTo;
	protected float m_AFHM_Difficulty;
	protected EntityAI m_AFHM_NightGear;
	protected bool m_AFHM_NightGearRemoveOnDeath;
	protected int m_AFHM_NightGearTries;

	void AFHM_SetDifficulty(float d)
	{
		m_AFHM_Difficulty = d;
	}

	// The difficulty HiveMind tuned this bot at; 0 for a bot it never tuned.
	float AFHM_GetDifficulty()
	{
		return m_AFHM_Difficulty;
	}

	// Night-vision goggles on a headstrap in the eyewear slot, whatever the bot wears on its
	// head. Glasses already in that slot make way. Expansion switches the goggles on by itself
	// at night once it is told to look again.
	void AFHM_GiveNightVision(bool removeOnDeath)
	{
		m_AFHM_NightGearRemoveOnDeath = removeOnDeath;
		m_AFHM_NightGearTries = 0;
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Call(AFHM_FitNightVision);
	}

	protected void AFHM_FitNightVision()
	{
		if (!IsAlive() || ++m_AFHM_NightGearTries > 3)
			return;

		EntityAI strap = GetInventory().FindAttachment(InventorySlots.EYEWEAR);
		if (strap && !strap.IsInherited(NVGHeadstrap))
		{
			GetGame().ObjectDelete(strap);
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Call(AFHM_FitNightVision);
			return;
		}
		if (!strap)
			strap = GetInventory().CreateAttachment("NVGHeadstrap");
		if (!strap)
			return;

		EntityAI goggles = strap.GetInventory().CreateAttachment("NVGoggles");
		if (goggles)
			goggles.GetInventory().CreateAttachment("Battery9V");
		m_AFHM_NightGear = strap;
		m_Expansion_TriedTurningOnVisibilityEnhancers = false;
	}

	bool AFHM_HasNightVision()
	{
		EntityAI strap = GetInventory().FindAttachment(InventorySlots.EYEWEAR);
		return strap && strap.IsInherited(NVGHeadstrap) && strap.GetInventory().AttachmentCount() > 0;
	}

	override void EEKilled(Object killer)
	{
		super.EEKilled(killer);
		if (GetGame().IsServer() && m_AFHM_NightGearRemoveOnDeath && m_AFHM_NightGear)
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Call(AFHM_RemoveNightGear);
	}

	protected void AFHM_RemoveNightGear()
	{
		if (m_AFHM_NightGear && m_AFHM_NightGear.GetHierarchyRoot() == this)
			GetGame().ObjectDelete(m_AFHM_NightGear);
		m_AFHM_NightGear = null;
	}
	void AFHM_MarkTuned()
	{
		m_AFHM_Tuned = true;
	}

	bool AFHM_IsTuned()
	{
		return m_AFHM_Tuned;
	}

	override void Init()
	{
		super.Init();

		if (!GetGame().IsServer())
			return;
		AFHiveMind hm = AFHiveMind.Get();
		if (!hm || !hm.m_Config.Enabled || !hm.m_Config.TuneOtherBots)
			return;
		int delay = Math.Max(hm.m_Config.OtherBotsDelaySeconds, 0.5) * 1000;
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(AFHM_TuneLate, delay, false);
	}

	void AFHM_TuneLate()
	{
		if (m_AFHM_Tuned || !IsAlive())
			return;
		if (AFHM_IsServiceNPC())
		{
			m_AFHM_Tuned = true;
			return;
		}

		// A bot following a player (Expansion's recruit feature) is the player's, not an enemy.
		string label = "bot from another mod (" + GetType() + ")";
		eAIGroup group = GetGroup();
		if (group)
		{
			DayZPlayerImplement leader = group.GetLeader();
			if (leader && !leader.IsAI())
			{
				m_AFHM_Tuned = true;
				return;
			}
			if (group.GetName())
				label = group.GetName();
		}

		AFHiveMind hm = AFHiveMind.Get();
		if (hm)
			hm.TuneBot(this, label, GetPosition());
	}

	// Gives the bot a weapon from another loadout, keeping its clothes and gear. Its long guns
	// are removed first, because AI weapons are created in the hands and that fails while the
	// hands are full. The new loadout goes on a frame later, once the removal has settled.
	void AFHM_SwapWeapons(string loadout)
	{
		m_AFHM_SwapTo = loadout;
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Call(AFHM_DropLongGuns);
	}

	protected void AFHM_DropLongGuns()
	{
		if (!IsAlive())
			return;
		array<EntityAI> guns = new array<EntityAI>;
		EntityAI inHands = GetHumanInventory().GetEntityInHands();
		if (inHands && inHands.IsWeapon())
			guns.Insert(inHands);
		array<int> slots = new array<int>;
		slots.Insert(InventorySlots.SHOULDER);
		slots.Insert(InventorySlots.MELEE);
		foreach (int slot: slots)
		{
			EntityAI carried = GetInventory().FindAttachment(slot);
			if (carried && carried.IsWeapon() && guns.Find(carried) == -1)
				guns.Insert(carried);
		}
		foreach (EntityAI gun: guns)
			GetGame().ObjectDelete(gun);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Call(AFHM_ApplySwap);
	}

	protected void AFHM_ApplySwap()
	{
		if (!IsAlive() || !m_AFHM_SwapTo)
			return;
		if (!ExpansionHumanLoadout.Apply(this, m_AFHM_SwapTo, false))
			AFHM_Util.Log("could not apply loadout " + m_AFHM_SwapTo + " (missing from ExpansionMod\\Loadouts?)");
		m_AFHM_SwapTo = "";
	}

	override void EEDelete(EntityAI parent)
	{
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(AFHM_TuneLate);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(AFHM_DropLongGuns);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(AFHM_ApplySwap);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(AFHM_FitNightVision);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(AFHM_RemoveNightGear);
		super.EEDelete(parent);
	}
};