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

	// The player or bot holding (or being) the thing that did the damage; null for anything else.
	protected PlayerBase AFHM_Opponent(Object source)
	{
		EntityAI entity = EntityAI.Cast(source);
		if (!entity)
			return null;
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
		if (!other || other.IsAI() == IsAI())
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
			if (other && other.IsAI() != IsAI())
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
			ScriptRPC rpc = new ScriptRPC();
			rpc.Write(lines);
			rpc.Send(this, AFHM_Const.RPC_REPORT, true, sender);
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
		hm.TuneBot(ai, name, m_Position);
	}
};

// Bots that no patrol tunes: other mods (Moving AI Convoy, BS Patrol Helicopter and Tank, ...)
// create Expansion bots directly. Each bot gets a few seconds for its own mod to set it up;
// if no patrol has tuned it by then, it is tuned from whatever values it ended up with.
modded class eAIBase
{
	protected bool m_AFHM_Tuned;

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

	override void EEDelete(EntityAI parent)
	{
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(AFHM_TuneLate);
		super.EEDelete(parent);
	}
};