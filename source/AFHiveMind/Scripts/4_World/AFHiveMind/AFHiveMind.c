// The HiveMind: watches how players and Expansion AI bots fight, remembers it in brain.json,
// and tunes every newly spawned patrol bot from what it has learned. Server side only.

class AFHiveMind
{
	protected static ref AFHiveMind s_Instance;
	protected static const int PANEL_COLUMNS = 74;

	ref AFHM_Config m_Config;
	ref AFHM_Brain m_Brain;

	protected float m_LastEvolve;
	protected float m_LastSave;
	protected float m_LastPresence;
	protected float m_NewEvidence;
	protected bool m_Dirty;
	protected ref array<string> m_Recent = new array<string>;

	static AFHiveMind Get()
	{
		if (!s_Instance && GetGame() && GetGame().IsServer())
		{
			s_Instance = new AFHiveMind();
			s_Instance.Start();
		}
		return s_Instance;
	}

	protected void Start()
	{
		if (!FileExist(AFHM_Const.DIR))
			MakeDirectory(AFHM_Const.DIR);

		LoadConfig();
		LoadBrain();
		AFHM_Loadouts.WriteDefaults();

		float now = GetGame().GetTickTime();
		m_LastEvolve = now;
		m_LastSave = now;
		m_LastPresence = now;

		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Tick, 15000, true);
		AFHM_Util.Log("awake: generation " + m_Brain.Generation.ToString() + ", difficulty " + AFHM_Util.F(m_Brain.Difficulty) + ", lifetime " + m_Brain.TotalBotsKilled.ToString() + " bots lost / " + m_Brain.TotalPlayersKilled.ToString() + " players killed");
	}

	// ------------------------------------------------------------------ files

	void LoadConfig()
	{
		AFHM_Config cfg = new AFHM_Config();
		string err;
		if (FileExist(AFHM_Const.CONFIG_PATH))
		{
			if (!JsonFileLoader<AFHM_Config>.LoadFile(AFHM_Const.CONFIG_PATH, cfg, err))
			{
				AFHM_Util.Log("config.json could not be read (" + err + "); using the defaults");
				cfg = new AFHM_Config();
			}
		}
		else
		{
			JsonFileLoader<AFHM_Config>.SaveFile(AFHM_Const.CONFIG_PATH, cfg, err);
		}
		if (!cfg.Admins)
			cfg.Admins = new array<string>;
		if (cfg.ZoneSize < 50)
			cfg.ZoneSize = 50;
		if (cfg.MemoryHalfLifeHours <= 0)
			cfg.MemoryHalfLifeHours = 72;
		m_Config = cfg;
	}

	protected void LoadBrain()
	{
		AFHM_Brain brain = new AFHM_Brain();
		string err;
		if (FileExist(AFHM_Const.BRAIN_PATH))
		{
			if (!JsonFileLoader<AFHM_Brain>.LoadFile(AFHM_Const.BRAIN_PATH, brain, err))
			{
				// Keep the unreadable file for inspection rather than overwrite it.
				CopyFile(AFHM_Const.BRAIN_PATH, AFHM_Const.BRAIN_PATH + ".broken");
				AFHM_Util.Log("brain.json could not be read (" + err + "); kept as brain.json.broken, starting with an empty brain");
				brain = new AFHM_Brain();
			}
		}
		if (!brain)
			brain = new AFHM_Brain();
		brain.Validate();
		m_Brain = brain;
	}

	void Save()
	{
		m_Brain.LastUpdated = AFHM_Util.Now();
		string err;
		if (!JsonFileLoader<AFHM_Brain>.SaveFile(AFHM_Const.BRAIN_PATH, m_Brain, err))
			AFHM_Util.Log("brain.json could not be written: " + err);

		FileHandle f = OpenFile(AFHM_Const.REPORT_PATH, FileMode.WRITE);
		if (f != 0)
		{
			array<string> lines = BuildReport(true);
			foreach (string line: lines)
				FPrintln(f, line);
			CloseFile(f);
		}
		m_Dirty = false;
		m_LastSave = GetGame().GetTickTime();
	}

	// ------------------------------------------------------------------ clock

	void Tick()
	{
		float now = GetGame().GetTickTime();

		if (now - m_LastPresence >= 60)
		{
			m_LastPresence = now;
			SamplePresence();
		}

		if (now - m_LastEvolve >= m_Config.EvolveIntervalSeconds)
			Evolve();
		else if (m_Dirty && now - m_LastSave >= m_Config.SaveIntervalSeconds)
			Save();
	}

	protected void SamplePresence()
	{
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		foreach (Man man: players)
		{
			PlayerBase pb = PlayerBase.Cast(man);
			if (!pb || pb.IsAI() || !pb.IsAlive())
				continue;
			GetZone(pb.GetPosition()).Presence += 1;
			m_Dirty = true;
		}
	}

	// ------------------------------------------------------------------ learning

	// One step of learning: forget a little, compare the win rate with the target, shift difficulty.
	void Evolve()
	{
		float now = GetGame().GetTickTime();
		float hours = (now - m_LastEvolve) / 3600;
		m_LastEvolve = now;

		LoadConfig();
		if (!m_Config.Enabled)
			return;

		Decay(Math.Pow(0.5, hours / m_Config.MemoryHalfLifeHours));

		float fights = m_Brain.BotsKilledByPlayers + m_Brain.PlayersKilledByBots;
		float winRate = WinRate(m_Brain.BotsKilledByPlayers, m_Brain.PlayersKilledByBots);
		float before = m_Brain.Difficulty;

		if (fights >= m_Config.MinSamples && m_NewEvidence > 0)
		{
			// Little new evidence moves it a little; ten or more fights move it the full step.
			float weight = Math.Clamp(m_NewEvidence / 10, 0, 1);
			// A win rate 20 points or more off the target counts as a full miss.
			float miss = Math.Clamp((winRate - m_Config.TargetPlayerWinRate) / 0.2, -1, 1);
			m_Brain.Difficulty = Math.Clamp(before + m_Config.LearnRate * weight * miss, 0, 1);
		}
		m_NewEvidence = 0;
		m_Brain.Generation++;

		BuildInsights(winRate, fights);

		if (Math.AbsFloat(m_Brain.Difficulty - before) > 0.004)
			AFHM_Util.Log("generation " + m_Brain.Generation.ToString() + ": players win " + AFHM_Util.Pct(winRate) + " (target " + AFHM_Util.Pct(m_Config.TargetPlayerWinRate) + "), difficulty " + AFHM_Util.F(before) + " -> " + AFHM_Util.F(m_Brain.Difficulty));

		Save();
	}

	protected void Decay(float keep)
	{
		if (keep >= 1)
			return;

		m_Brain.BotsKilledByPlayers *= keep;
		m_Brain.PlayersKilledByBots *= keep;
		m_Brain.HitsOnBots *= keep;
		m_Brain.HeadHitsOnBots *= keep;
		m_Brain.HitsOnPlayers *= keep;
		m_Brain.LongKills *= keep;
		m_Brain.CloseKills *= keep;

		for (int h = 0; h < m_Brain.HourActivity.Count(); h++)
			m_Brain.HourActivity[h] = m_Brain.HourActivity[h] * keep;

		// Entries that have faded to nothing are dropped so the file does not grow for ever.
		array<string> gone = new array<string>;
		foreach (string wName, float wCount: m_Brain.Weapons)
		{
			if (wCount * keep < 0.05)
				gone.Insert(wName);
		}
		foreach (string wGone: gone)
			m_Brain.Weapons.Remove(wGone);
		array<string> wKeys = m_Brain.Weapons.GetKeyArray();
		foreach (string wKey: wKeys)
			m_Brain.Weapons.Set(wKey, m_Brain.Weapons.Get(wKey) * keep);

		gone.Clear();
		foreach (string zName, AFHM_ZoneStats zone: m_Brain.Zones)
		{
			zone.BotsKilled *= keep;
			zone.PlayersKilled *= keep;
			zone.Presence *= keep;
			if (zone.BotsKilled + zone.PlayersKilled + zone.Presence < 0.05)
				gone.Insert(zName);
		}
		foreach (string zGone: gone)
			m_Brain.Zones.Remove(zGone);

		gone.Clear();
		foreach (string pId, AFHM_PlayerStats ps: m_Brain.Players)
		{
			ps.BotsKilled *= keep;
			ps.DeathsToBots *= keep;
			ps.HeadshotKills *= keep;
			if (ps.BotsKilled + ps.DeathsToBots < 0.05)
				gone.Insert(pId);
		}
		foreach (string pGone: gone)
			m_Brain.Players.Remove(pGone);
	}

	// Win rate pulled towards the target until there is real evidence.
	float WinRate(float botsKilled, float playersKilled)
	{
		float prior = 4;
		return (botsKilled + prior * m_Config.TargetPlayerWinRate) / (botsKilled + playersKilled + prior);
	}

	// How players fight, from -1 (all close-range kills) to +1 (all long-range kills).
	// Mid-range kills pull it towards 0, and two imagined mid-range kills keep a handful of
	// fights from deciding it.
	float Style()
	{
		return (m_Brain.LongKills - m_Brain.CloseKills) / (m_Brain.BotsKilledByPlayers + 2);
	}

	// 0 while the style is within +-0.25 of neutral, rising to 1 at a clear +-1.
	float StyleStrength()
	{
		if (m_Brain.BotsKilledByPlayers < m_Config.MinSamples)
			return 0;
		return Math.Clamp((Math.AbsFloat(Style()) - 0.25) / 0.75, 0, 1);
	}

	float RoleShare()
	{
		return Math.Clamp(StyleStrength() * m_Config.RoleShareMax, 0, 1);
	}

	float FlankScale()
	{
		float s = StyleStrength();
		if (Style() > 0)
			return Math.Lerp(1, m_Config.FlankWiderMax, s);
		return Math.Lerp(1, m_Config.FlankNarrowerMin, s);
	}

	// Difficulty 0..1 to a multiplier: lo at 0, 1 at 0.5, hi at 1.
	static float ScaleFor(float difficulty, float lo, float hi)
	{
		if (difficulty < 0.5)
			return Math.Lerp(lo, 1, difficulty * 2);
		return Math.Lerp(1, hi, (difficulty - 0.5) * 2);
	}

	string ZoneKey(vector pos)
	{
		int gx = Math.Floor(pos[0] / m_Config.ZoneSize);
		int gz = Math.Floor(pos[2] / m_Config.ZoneSize);
		return gx.ToString() + "_" + gz.ToString();
	}

	// "9_20" with 500 m zones -> "4750 / 10250" (the zone's middle on the map).
	string ZoneLabel(string key)
	{
		array<string> parts = new array<string>;
		key.Split("_", parts);
		if (parts.Count() != 2)
			return key;
		int cx = (parts[0].ToInt() + 0.5) * m_Config.ZoneSize;
		int cz = (parts[1].ToInt() + 0.5) * m_Config.ZoneSize;
		return cx.ToString() + " / " + cz.ToString();
	}

	AFHM_ZoneStats GetZone(vector pos)
	{
		string key = ZoneKey(pos);
		AFHM_ZoneStats zone;
		if (!m_Brain.Zones.Find(key, zone))
		{
			zone = new AFHM_ZoneStats();
			m_Brain.Zones.Insert(key, zone);
		}
		return zone;
	}

	AFHM_PlayerStats GetPlayerStats(PlayerBase player)
	{
		string id = "offline";
		string name = "Offline player";
		PlayerIdentity identity = player.GetIdentity();
		if (identity)
		{
			id = identity.GetPlainId();
			name = identity.GetName();
		}
		AFHM_PlayerStats ps;
		if (!m_Brain.Players.Find(id, ps))
		{
			ps = new AFHM_PlayerStats();
			m_Brain.Players.Insert(id, ps);
		}
		ps.Name = name;
		return ps;
	}

	protected void NoteHour()
	{
		int y, mo, d, h, mi;
		GetGame().GetWorld().GetDate(y, mo, d, h, mi);
		if (h >= 0 && h < 24)
			m_Brain.HourActivity[h] = m_Brain.HourActivity[h] + 1;
	}

	// ------------------------------------------------------------------ what it observes

	void RecordBotKilled(PlayerBase bot, PlayerBase killer, EntityAI source)
	{
		if (!m_Config.Enabled)
			return;

		float dist = vector.Distance(bot.GetPosition(), killer.GetPosition());
		string weapon = "Melee or other";
		if (source && source != killer)
			weapon = source.GetType();

		m_Brain.BotsKilledByPlayers += 1;
		m_Brain.TotalBotsKilled++;

		if (m_Brain.KillDistanceAvg <= 0)
			m_Brain.KillDistanceAvg = dist;
		else
			m_Brain.KillDistanceAvg = Math.Lerp(m_Brain.KillDistanceAvg, dist, 0.1);
		// The "far" figure jumps up on a long shot and sinks back slowly.
		if (dist > m_Brain.KillDistanceFar)
			m_Brain.KillDistanceFar = Math.Lerp(m_Brain.KillDistanceFar, dist, 0.5);
		else
			m_Brain.KillDistanceFar = Math.Lerp(m_Brain.KillDistanceFar, dist, 0.02);

		float tally;
		m_Brain.Weapons.Find(weapon, tally);
		m_Brain.Weapons.Set(weapon, tally + 1);

		GetZone(bot.GetPosition()).BotsKilled += 1;

		AFHM_PlayerStats ps = GetPlayerStats(killer);
		ps.BotsKilled += 1;
		if (ps.KillDistanceAvg <= 0)
			ps.KillDistanceAvg = dist;
		else
			ps.KillDistanceAvg = Math.Lerp(ps.KillDistanceAvg, dist, 0.15);
		if (bot.AFHM_GetLastHitZone() == "Head")
			ps.HeadshotKills += 1;
		if (dist >= m_Config.LongRangeDistance)
			m_Brain.LongKills += 1;
		else if (dist <= m_Config.CloseRangeDistance)
			m_Brain.CloseKills += 1;
		NoteHour();
		m_NewEvidence += 1;
		m_Dirty = true;

		if (m_Config.DebugLevel >= 1)
			AFHM_Util.Log("seen: " + ps.Name + " killed a bot at " + AFHM_Util.F(dist, 0) + " m with " + weapon + " in zone " + ZoneLabel(ZoneKey(bot.GetPosition())));
	}

	void RecordPlayerKilled(PlayerBase victim, PlayerBase bot, EntityAI source)
	{
		if (!m_Config.Enabled)
			return;

		float dist = vector.Distance(bot.GetPosition(), victim.GetPosition());

		m_Brain.PlayersKilledByBots += 1;
		m_Brain.TotalPlayersKilled++;
		GetZone(victim.GetPosition()).PlayersKilled += 1;

		AFHM_PlayerStats ps = GetPlayerStats(victim);
		ps.DeathsToBots += 1;

		NoteHour();
		m_NewEvidence += 1;
		m_Dirty = true;

		if (m_Config.DebugLevel >= 1)
			AFHM_Util.Log("seen: a bot killed " + ps.Name + " at " + AFHM_Util.F(dist, 0) + " m in zone " + ZoneLabel(ZoneKey(victim.GetPosition())));
	}

	void RecordHit(PlayerBase victim, PlayerBase attacker, string dmgZone)
	{
		if (!m_Config.Enabled)
			return;

		if (victim.IsAI())
		{
			m_Brain.HitsOnBots += 1;
			if (dmgZone == "Head")
				m_Brain.HeadHitsOnBots += 1;
		}
		else
		{
			m_Brain.HitsOnPlayers += 1;
			float dist = vector.Distance(victim.GetPosition(), attacker.GetPosition());
			if (m_Brain.BotHitDistanceAvg <= 0)
				m_Brain.BotHitDistanceAvg = dist;
			else
				m_Brain.BotHitDistanceAvg = Math.Lerp(m_Brain.BotHitDistanceAvg, dist, 0.1);
		}
		m_Dirty = true;
	}

	// ------------------------------------------------------------------ what it changes

	// Difficulty for a bot spawning at pos: the global figure, shifted for the zone and the nearest player.
	float DifficultyAt(vector pos, out string why)
	{
		float d = m_Brain.Difficulty;
		why = "global " + AFHM_Util.F(d);

		AFHM_ZoneStats zone;
		if (m_Brain.Zones.Find(ZoneKey(pos), zone) && zone.BotsKilled + zone.PlayersKilled >= m_Config.MinSamples)
		{
			float zoneOffset = Math.Clamp(WinRate(zone.BotsKilled, zone.PlayersKilled) - m_Config.TargetPlayerWinRate, -m_Config.ZoneOffsetMax, m_Config.ZoneOffsetMax);
			if (Math.AbsFloat(zoneOffset) >= 0.01)
			{
				d += zoneOffset;
				why += ", zone " + Signed(zoneOffset);
			}
		}

		PlayerBase nearest = NearestPlayer(pos, m_Config.PlayerInfluenceRadius);
		if (nearest)
		{
			string id = "offline";
			if (nearest.GetIdentity())
				id = nearest.GetIdentity().GetPlainId();
			AFHM_PlayerStats ps;
			if (m_Brain.Players.Find(id, ps) && ps.BotsKilled + ps.DeathsToBots >= m_Config.MinSamples)
			{
				float playerOffset = Math.Clamp(WinRate(ps.BotsKilled, ps.DeathsToBots) - m_Config.TargetPlayerWinRate, -m_Config.PlayerOffsetMax, m_Config.PlayerOffsetMax);
				if (Math.AbsFloat(playerOffset) >= 0.01)
				{
					d += playerOffset;
					why += ", " + ps.Name + " " + Signed(playerOffset);
				}
			}
		}

		return Math.Clamp(d, 0, 1);
	}

	static string Signed(float v)
	{
		if (v >= 0)
			return "+" + AFHM_Util.F(v);
		return AFHM_Util.F(v);
	}

	PlayerBase NearestPlayer(vector pos, float radius)
	{
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		PlayerBase best;
		float bestDistSq = radius * radius;
		foreach (Man man: players)
		{
			PlayerBase pb = PlayerBase.Cast(man);
			if (!pb || pb.IsAI() || !pb.IsAlive())
				continue;
			float distSq = vector.DistanceSq(pos, pb.GetPosition());
			if (distSq < bestDistSq)
			{
				bestDistSq = distSq;
				best = pb;
			}
		}
		return best;
	}

	// Called for every bot a patrol spawns, after Expansion applied the patrol's own settings,
	// and (see eAIBase.AFHM_TuneLate) for bots other mods create without a patrol.
	void TuneBot(eAIBase ai, string patrolName, vector pos, bool allowRoles = false)
	{
		if (!m_Config.Enabled || !ai)
			return;
		ai.AFHM_MarkTuned();

		string why;
		float d = DifficultyAt(pos, why);

		float accScale = ScaleFor(d, m_Config.AccuracyScaleMin, m_Config.AccuracyScaleMax);
		float dmgScale = ScaleFor(d, m_Config.DamageScaleMin, m_Config.DamageScaleMax);
		float recvScale = ScaleFor(d, m_Config.DamageReceivedScaleEasy, m_Config.DamageReceivedScaleHard);

		// A change of under 2% is not worth making: Expansion handles every hit on or by a bot
		// whose damage multiplier is not exactly 1 in a slower, separate way.
		float accMin = ai.m_eAI_AccuracyMin;
		float accMax = ai.m_eAI_AccuracyMax;
		float dmg = ai.m_eAI_DamageMultiplier;
		float recv = ai.m_eAI_DamageReceivedMultiplier;

		if (Math.AbsFloat(accScale - 1) >= 0.02)
		{
			accMin = Math.Clamp(accMin * accScale, 0.05, 1);
			accMax = Math.Clamp(accMax * accScale, accMin, 1);
			ai.eAI_SetAccuracy(accMin, accMax);
		}
		if (Math.AbsFloat(dmgScale - 1) >= 0.02)
		{
			dmg = Math.Max(dmg * dmgScale, 0.05);
			ai.eAI_SetDamageMultiplier(dmg);
		}
		if (Math.AbsFloat(recvScale - 1) >= 0.02)
		{
			recv = Math.Max(recv * recvScale, 0.05);
			ai.eAI_SetDamageReceivedMultiplier(recv);
		}

		// Players who kill from far away are noticed from further away.
		float threat = ai.m_eAI_ThreatDistanceLimit;
		float wanted = Math.Min(m_Brain.KillDistanceFar * m_Config.ThreatDistanceMargin, m_Config.ThreatDistanceMax);
		if (wanted > threat)
		{
			threat = wanted;
			ai.eAI_SetThreatDistanceLimit(threat);
		}

		m_Brain.TotalBotsTuned++;

		string profile = "difficulty " + AFHM_Util.F(d) + " (" + why + "): accuracy " + AFHM_Util.F(accMin) + "-" + AFHM_Util.F(accMax) + ", damage x" + AFHM_Util.F(dmg) + ", takes x" + AFHM_Util.F(recv) + ", threat range " + AFHM_Util.F(threat, 0) + " m";

		// Range counter: flank wider against snipers, push straight in against rushers, and
		// hand some patrol bots a weapon that answers the way players fight.
		string role;
		if (m_Config.AdaptTactics && StyleStrength() > 0)
		{
			float flankScale = FlankScale();
			if (Math.AbsFloat(flankScale - 1) >= 0.02 && ai.m_eAI_MaxFlankingDistance > 0)
			{
				float flank = ai.m_eAI_MaxFlankingDistance * flankScale;
				ai.eAI_SetMaxFlankingDistance(flank);
				profile += ", flanking " + AFHM_Util.F(flank, 0) + " m";
			}

			string loadout = m_Config.AssaultLoadout;
			role = "assault";
			if (Style() > 0)
			{
				loadout = m_Config.MarksmanLoadout;
				role = "marksman";
			}
			if (allowRoles && loadout && Math.RandomFloat01() < RoleShare())
				ai.AFHM_SwapWeapons(loadout);
			else
				role = "";
		}
		string patrolProfile = profile;
		if (role)
			profile += ", role " + role;
		ai.AFHM_SetProfile(profile);

		if (!patrolName)
			patrolName = "unnamed patrol";
		string line = patrolName + " @ " + ZoneLabel(ZoneKey(pos)) + ": " + patrolProfile;
		// One line per patrol in the panel, not one per bot.
		if (m_Recent.Count() == 0 || m_Recent[m_Recent.Count() - 1] != line)
		{
			m_Recent.Insert(line);
			if (m_Recent.Count() > 6)
				m_Recent.RemoveOrdered(0);
		}

		if (m_Config.DebugLevel >= 2)
			AFHM_Util.Log("tuned: " + patrolName + " bot: " + profile);
	}

	// ------------------------------------------------------------------ what it reports

	protected string TopWeapon(out float share)
	{
		string best;
		float bestCount;
		float total;
		foreach (string name, float count: m_Brain.Weapons)
		{
			total += count;
			if (count > bestCount)
			{
				bestCount = count;
				best = name;
			}
		}
		share = 0;
		if (total > 0)
			share = bestCount / total;
		return best;
	}

	protected void BuildInsights(float winRate, float fights)
	{
		array<string> ins = m_Brain.Insights;
		ins.Clear();

		if (fights < m_Config.MinSamples)
		{
			ins.Insert("Not enough fights seen yet (" + AFHM_Util.F(fights, 1) + " of " + AFHM_Util.F(m_Config.MinSamples, 0) + "): bots stay as configured.");
		}
		else
		{
			string verdict = "about right";
			if (winRate > m_Config.TargetPlayerWinRate + 0.03)
				verdict = "bots are too easy";
			else if (winRate < m_Config.TargetPlayerWinRate - 0.03)
				verdict = "bots are too hard";
			ins.Insert("Players win " + AFHM_Util.Pct(winRate) + " of lethal fights (target " + AFHM_Util.Pct(m_Config.TargetPlayerWinRate) + "): " + verdict + ".");
		}

		float d = m_Brain.Difficulty;
		ins.Insert("Difficulty " + AFHM_Util.F(d) + ": accuracy x" + AFHM_Util.F(ScaleFor(d, m_Config.AccuracyScaleMin, m_Config.AccuracyScaleMax)) + ", damage x" + AFHM_Util.F(ScaleFor(d, m_Config.DamageScaleMin, m_Config.DamageScaleMax)) + ", damage taken x" + AFHM_Util.F(ScaleFor(d, m_Config.DamageReceivedScaleEasy, m_Config.DamageReceivedScaleHard)) + ".");

		if (m_Brain.KillDistanceAvg > 0)
		{
			float wanted = Math.Min(m_Brain.KillDistanceFar * m_Config.ThreatDistanceMargin, m_Config.ThreatDistanceMax);
			ins.Insert("Players kill from " + AFHM_Util.F(m_Brain.KillDistanceAvg, 0) + " m on average, long shots around " + AFHM_Util.F(m_Brain.KillDistanceFar, 0) + " m: bots watch out to at least " + AFHM_Util.F(wanted, 0) + " m.");
		}

		if (m_Brain.BotsKilledByPlayers >= m_Config.MinSamples)
		{
			float kills = m_Brain.BotsKilledByPlayers;
			string range = "Players kill " + AFHM_Util.Pct(m_Brain.LongKills / kills) + " of bots beyond " + AFHM_Util.F(m_Config.LongRangeDistance, 0) + " m and " + AFHM_Util.Pct(m_Brain.CloseKills / kills) + " within " + AFHM_Util.F(m_Config.CloseRangeDistance, 0) + " m: ";
			if (!m_Config.AdaptTactics || StyleStrength() <= 0)
				range += "no clear style, tactics unchanged.";
			else if (Style() > 0)
				range += "snipers. Bots flank x" + AFHM_Util.F(FlankScale()) + " as wide and up to " + AFHM_Util.Pct(RoleShare()) + " carry marksman rifles.";
			else
				range += "rushers. Bots flank x" + AFHM_Util.F(FlankScale()) + " as wide and up to " + AFHM_Util.Pct(RoleShare()) + " carry automatics.";
			ins.Insert(range);
		}

		if (m_Brain.HitsOnBots >= 5)
			ins.Insert("Headshots are " + AFHM_Util.Pct(m_Brain.HeadHitsOnBots / m_Brain.HitsOnBots) + " of the hits players land on bots.");

		float share;
		string weapon = TopWeapon(share);
		if (weapon)
			ins.Insert("Favourite weapon against bots: " + weapon + " (" + AFHM_Util.Pct(share) + " of kills).");

		string hotKey;
		float hotCount;
		foreach (string zKey, AFHM_ZoneStats zone: m_Brain.Zones)
		{
			if (zone.BotsKilled > hotCount)
			{
				hotCount = zone.BotsKilled;
				hotKey = zKey;
			}
		}
		if (hotKey && hotCount >= m_Config.MinSamples)
		{
			AFHM_ZoneStats hot = m_Brain.Zones.Get(hotKey);
			float hotOffset = Math.Clamp(WinRate(hot.BotsKilled, hot.PlayersKilled) - m_Config.TargetPlayerWinRate, -m_Config.ZoneOffsetMax, m_Config.ZoneOffsetMax);
			ins.Insert("Bots die most around " + ZoneLabel(hotKey) + " (" + AFHM_Util.F(hot.BotsKilled, 1) + " lost, " + AFHM_Util.F(hot.PlayersKilled, 1) + " players killed): local difficulty " + Signed(hotOffset) + ".");
		}

		int peakHour = -1;
		float peak;
		for (int h = 0; h < m_Brain.HourActivity.Count(); h++)
		{
			if (m_Brain.HourActivity[h] > peak)
			{
				peak = m_Brain.HourActivity[h];
				peakHour = h;
			}
		}
		if (peakHour >= 0)
			ins.Insert("Most fighting happens around " + peakHour.ToStringLen(2) + ":00 in-game time.");
	}

	// The debug text: for report.txt (withPlayers) and for the in-game panel.
	array<string> BuildReport(bool withPlayers)
	{
		array<string> r = new array<string>;
		r.Insert("AF HIVEMIND - generation " + m_Brain.Generation.ToString() + " - " + AFHM_Util.Now());
		string state = "ON";
		if (!m_Config.Enabled)
			state = "OFF (Enabled is false in config.json)";
		r.Insert("Tuning " + state + " | difficulty " + AFHM_Util.F(m_Brain.Difficulty) + " (0 easiest, 0.5 as configured, 1 hardest)");
		r.Insert("Remembered: " + AFHM_Util.F(m_Brain.BotsKilledByPlayers, 1) + " bots lost, " + AFHM_Util.F(m_Brain.PlayersKilledByBots, 1) + " players killed | lifetime " + m_Brain.TotalBotsKilled.ToString() + " / " + m_Brain.TotalPlayersKilled.ToString() + " | bots tuned " + m_Brain.TotalBotsTuned.ToString());
		r.Insert("New fights since the last evolve step: " + AFHM_Util.F(m_NewEvidence, 0) + " | next step in " + AFHM_Util.F(Math.Max(m_Config.EvolveIntervalSeconds - (GetGame().GetTickTime() - m_LastEvolve), 0), 0) + " s");
		r.Insert(" ");
		r.Insert("WHAT IT LEARNED");
		if (m_Brain.Insights.Count() == 0)
			r.Insert("  nothing yet: the first evolve step has not run");
		foreach (string insight: m_Brain.Insights)
			r.Insert("  " + insight);
		r.Insert(" ");
		r.Insert("LATEST PATROLS IT TUNED");
		if (m_Recent.Count() == 0)
			r.Insert("  none since the server started");
		foreach (string recent: m_Recent)
			r.Insert("  " + recent);

		if (withPlayers)
		{
			r.Insert(" ");
			r.Insert("PLAYERS (bots killed / deaths to bots / average kill distance)");
			if (m_Brain.Players.Count() == 0)
				r.Insert("  none yet");
			foreach (string id, AFHM_PlayerStats ps: m_Brain.Players)
				r.Insert("  " + ps.Name + " [" + id + "]: " + AFHM_Util.F(ps.BotsKilled, 1) + " / " + AFHM_Util.F(ps.DeathsToBots, 1) + " / " + AFHM_Util.F(ps.KillDistanceAvg, 0) + " m");
		}
		return r;
	}

	bool IsAdmin(PlayerIdentity identity)
	{
		if (!GetGame().IsMultiplayer())
			return true;
		if (!identity)
			return false;
		return m_Config.Admins.Find(identity.GetPlainId()) > -1;
	}

	// The text sent to an admin's debug panel.
	array<string> PanelLines()
	{
		// The panel is 640 px wide: longer lines continue, indented, on the next row.
		array<string> source = BuildReport(false);
		array<string> lines = new array<string>;
		foreach (string full: source)
		{
			string rest = full;
			while (rest.Length() > PANEL_COLUMNS)
			{
				int cut = PANEL_COLUMNS;
				while (cut > 20 && rest.Get(cut) != " ")
					cut--;
				lines.Insert(rest.Substring(0, cut));
				rest = "      " + rest.Substring(cut + 1, rest.Length() - cut - 1);
			}
			lines.Insert(rest);
		}
		return lines;
	}
};
