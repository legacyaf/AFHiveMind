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
	// New fights since the last evolve step, per player id.
	protected ref map<string, float> m_NewByPlayer = new map<string, float>;
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
		if (!cfg.BountyCommon)
			cfg.BountyCommon = new array<string>;
		if (!cfg.BountyRare)
			cfg.BountyRare = new array<string>;
		if (cfg.ZoneSize < 50)
			cfg.ZoneSize = 50;
		if (cfg.MemoryHalfLifeHours <= 0)
			cfg.MemoryHalfLifeHours = 72;
		if (cfg.PinnedDifficulty > 1)
			cfg.PinnedDifficulty = 1;
		if (cfg.HotZoneExtraBotsMax < 0)
			cfg.HotZoneExtraBotsMax = 0;
		// Bring an older config.json up to date. 0.4.0: the per-player boost is off by default;
		// a file still holding the old default (0.2) gets the new one, a value the admin chose
		// (anything else) is kept.
		if (cfg.ConfigVersion < 2)
		{
			if (Math.AbsFloat(cfg.PlayerOffsetMax - 0.2) < 0.001)
			{
				cfg.PlayerOffsetMax = 0;
				AFHM_Util.Log("config.json: PlayerOffsetMax set from 0.2 to the new default 0 (difficulty follows the community, not one player)");
			}
			cfg.ConfigVersion = 2;
		}
		m_Config = cfg;
		// Write it back: a config.json from an older version gains the new settings, with
		// their defaults, while every value the admin set is kept.
		if (FileExist(AFHM_Const.CONFIG_PATH) && !err)
			JsonFileLoader<AFHM_Config>.SaveFile(AFHM_Const.CONFIG_PATH, cfg, err);
	}

	void SaveConfig()
	{
		string err;
		if (!JsonFileLoader<AFHM_Config>.SaveFile(AFHM_Const.CONFIG_PATH, m_Config, err))
			AFHM_Util.Log("config.json could not be written: " + err);
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
		else
			Backup();

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

	// One copy of brain.json per day in AFHiveMind\backups, the newest BACKUPS_KEPT kept.
	// To roll back: stop the server and copy a backup over brain.json.
	protected void Backup()
	{
		if (!FileExist(AFHM_Const.BACKUP_DIR))
			MakeDirectory(AFHM_Const.BACKUP_DIR);
		string today = AFHM_Const.BACKUP_DIR + "/brain_" + AFHM_Util.Now().Substring(0, 10) + ".json";
		if (FileExist(today))
			return;
		CopyFile(AFHM_Const.BRAIN_PATH, today);

		array<string> names = new array<string>;
		string name;
		FileAttr attr;
		FindFileHandle find = FindFile(AFHM_Const.BACKUP_DIR + "/brain_*.json", name, attr, FindFileFlags.ALL);
		if (find)
		{
			names.Insert(name);
			while (FindNextFile(find, name, attr))
				names.Insert(name);
			CloseFindFile(find);
		}
		names.Sort();
		for (int i = 0; i < names.Count() - AFHM_Const.BACKUPS_KEPT; i++)
			DeleteFile(AFHM_Const.BACKUP_DIR + "/" + names[i]);
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
		if (m_Config.Frozen)
			return;
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
		m_LastEvolve = GetGame().GetTickTime();

		LoadConfig();
		if (!m_Config.Enabled)
			return;

		// Fade by real time since the last fade, so days the server was down count as well.
		int stamp = ExpansionStatic.GetTimestamp(true);
		if (m_Config.Frozen)
		{
			// Frozen: time stands still for its memory, so nothing fades while it is frozen.
			m_Brain.LastFadeTimestamp = stamp;
			m_NewEvidence = 0;
			m_NewByPlayer.Clear();
			RefreshInsights();
			Save();
			return;
		}
		if (m_Brain.LastFadeTimestamp <= 0 || stamp < m_Brain.LastFadeTimestamp)
			m_Brain.LastFadeTimestamp = stamp;
		float hours = (stamp - m_Brain.LastFadeTimestamp) / 3600.0;
		m_Brain.LastFadeTimestamp = stamp;
		Decay(Math.Pow(0.5, hours / m_Config.MemoryHalfLifeHours));

		float wins;
		float losses;
		int players;
		float counted = CountedFights(wins, losses, players);
		float winRate = WinRate(wins, losses);
		float before = m_Brain.Difficulty;

		if (m_Config.PinnedDifficulty >= 0)
		{
			m_Brain.Difficulty = Math.Clamp(m_Config.PinnedDifficulty, 0, 1);
		}
		else if (counted >= m_Config.DifficultyMinFights && m_NewEvidence > 0)
		{
			// Little new evidence moves it a little; ten or more fights move it the full step.
			float weight = Math.Clamp(CountedNewFights() / 10, 0, 1);
			// A win rate 20 points or more off the target counts as a full miss.
			float miss = Math.Clamp((winRate - m_Config.TargetPlayerWinRate) / 0.2, -1, 1);
			m_Brain.Difficulty = Math.Clamp(before + m_Config.LearnRate * weight * miss, 0, 1);
		}
		m_NewEvidence = 0;
		m_NewByPlayer.Clear();
		m_Brain.Generation++;

		BuildInsights(winRate, counted, players);

		if (Math.AbsFloat(m_Brain.Difficulty - before) > 0.004)
			AFHM_Util.Log("generation " + m_Brain.Generation.ToString() + ": players win " + AFHM_Util.Pct(winRate) + " of " + AFHM_Util.F(counted, 1) + " counted fights from " + players.ToString() + " players (target " + AFHM_Util.Pct(m_Config.TargetPlayerWinRate) + "), difficulty " + AFHM_Util.F(before) + " -> " + AFHM_Util.F(m_Brain.Difficulty));

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
		m_Brain.NightFights *= keep;

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

	// The fights that set the global difficulty: every player's remembered fights, but no player
	// counting for more than PlayerFightsCap of them, so one strong player cannot set it alone.
	// A capped player keeps their own win rate, only with fewer fights behind it.
	float CountedFights(out float wins, out float losses, out int players)
	{
		wins = 0;
		losses = 0;
		players = 0;
		foreach (string id, AFHM_PlayerStats ps: m_Brain.Players)
		{
			float n = ps.BotsKilled + ps.DeathsToBots;
			if (n <= 0)
				continue;
			float f = 1;
			if (m_Config.PlayerFightsCap > 0 && n > m_Config.PlayerFightsCap)
				f = m_Config.PlayerFightsCap / n;
			wins += ps.BotsKilled * f;
			losses += ps.DeathsToBots * f;
			if (n >= 0.5)
				players++;
		}
		return wins + losses;
	}

	// New fights since the last evolve step, no player adding more than PlayerFightsPerStepMax.
	protected float CountedNewFights()
	{
		float total;
		foreach (string id, float n: m_NewByPlayer)
		{
			if (m_Config.PlayerFightsPerStepMax > 0)
				total += Math.Min(n, m_Config.PlayerFightsPerStepMax);
			else
				total += n;
		}
		return total;
	}

	static string PlayerId(PlayerBase player)
	{
		if (player && player.GetIdentity())
			return player.GetIdentity().GetPlainId();
		return "offline";
	}

	protected void NoteNewFight(PlayerBase player)
	{
		m_NewEvidence += 1;
		string id = PlayerId(player);
		float n;
		m_NewByPlayer.Find(id, n);
		m_NewByPlayer.Set(id, n + 1);
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

	float HearingScale()
	{
		float s = StyleStrength();
		if (Style() > 0)
			return Math.Lerp(1, m_Config.HearingWiderMax, s);
		return Math.Lerp(1, m_Config.HearingNarrowerMin, s);
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

	// The middle of a zone, from its key.
	vector ZoneCentre(string key)
	{
		array<string> parts = new array<string>;
		key.Split("_", parts);
		if (parts.Count() != 2)
			return vector.Zero;
		return Vector((parts[0].ToInt() + 0.5) * m_Config.ZoneSize, 0, (parts[1].ToInt() + 0.5) * m_Config.ZoneSize);
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
		string id = PlayerId(player);
		string name = "Offline player";
		PlayerIdentity identity = player.GetIdentity();
		if (identity)
			name = identity.GetName();
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
		if (m_Config.Frozen)
		{
			// Nothing is learned, but a hard bot still pays out its bounty.
			DropBounty(bot);
			return;
		}

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
		NoteNight(bot);
		NoteHour();
		DropBounty(bot);
		NoteNewFight(killer);
		m_Dirty = true;

		if (m_Config.DebugLevel >= 1)
			AFHM_Util.Log("seen: " + ps.Name + " killed a bot at " + AFHM_Util.F(dist, 0) + " m with " + weapon + " in zone " + ZoneLabel(ZoneKey(bot.GetPosition())));
	}

	void RecordPlayerKilled(PlayerBase victim, PlayerBase bot, EntityAI source)
	{
		if (!m_Config.Enabled || m_Config.Frozen)
			return;

		float dist = vector.Distance(bot.GetPosition(), victim.GetPosition());

		m_Brain.PlayersKilledByBots += 1;
		m_Brain.TotalPlayersKilled++;
		GetZone(victim.GetPosition()).PlayersKilled += 1;

		AFHM_PlayerStats ps = GetPlayerStats(victim);
		ps.DeathsToBots += 1;

		NoteNight(bot);
		NoteHour();
		NoteNewFight(victim);
		m_Dirty = true;

		if (m_Config.DebugLevel >= 1)
			AFHM_Util.Log("seen: a bot killed " + ps.Name + " at " + AFHM_Util.F(dist, 0) + " m in zone " + ZoneLabel(ZoneKey(victim.GetPosition())));
	}

	// Night by the bot's own reckoning: Expansion marks each bot day or night by how much
	// daylight it sees, the same figure it uses to decide when to switch on lights and NVG.
	protected void NoteNight(PlayerBase someBot)
	{
		eAIBase ai = eAIBase.Cast(someBot);
		if (ai && ai.m_eAI_IsDay == 0)
			m_Brain.NightFights += 1;
	}

	float NightShare()
	{
		return m_Brain.NightFights / (m_Brain.BotsKilledByPlayers + m_Brain.PlayersKilledByBots + 2);
	}

	float NightGearShare()
	{
		if (!m_Config.NightCounter || m_Brain.BotsKilledByPlayers + m_Brain.PlayersKilledByBots < m_Config.MinSamples)
			return 0;
		float span = Math.Max(m_Config.NightShareFull - m_Config.NightShareStart, 0.01);
		return Math.Clamp((NightShare() - m_Config.NightShareStart) / span, 0, 1) * m_Config.NightGearShareMax;
	}

	// One extra item where a hard bot died, from the common or the rare list.
	protected void DropBounty(PlayerBase someBot)
	{
		eAIBase ai = eAIBase.Cast(someBot);
		if (!ai || !m_Config.BountyLoot)
			return;
		float from = m_Config.BountyFromDifficulty;
		float strength = Math.Clamp((ai.AFHM_GetDifficulty() - from) / Math.Max(1 - from, 0.01), 0, 1);
		if (strength <= 0 || Math.RandomFloat01() >= strength * m_Config.BountyChanceMax)
			return;

		array<string> list = m_Config.BountyCommon;
		if (m_Config.BountyRare.Count() > 0 && Math.RandomFloat01() < strength * m_Config.BountyRareShareMax)
			list = m_Config.BountyRare;
		if (list.Count() == 0)
			return;
		string item = list.GetRandomElement();
		if (!GetGame().ConfigIsExisting("CfgVehicles " + item) && !GetGame().ConfigIsExisting("CfgMagazines " + item) && !GetGame().ConfigIsExisting("CfgWeapons " + item))
		{
			AFHM_Util.Log("bounty: no item called '" + item + "' (check BountyCommon and BountyRare in config.json)");
			return;
		}
		vector pos = ai.GetPosition() + Vector(Math.RandomFloat(-0.6, 0.6), 0, Math.RandomFloat(-0.6, 0.6));
		pos[1] = GetGame().SurfaceY(pos[0], pos[2]);
		if (!GetGame().CreateObjectEx(item, pos, ECE_PLACE_ON_SURFACE))
			return;
		m_Brain.TotalBountyDrops++;
		if (m_Config.DebugLevel >= 1)
			AFHM_Util.Log("bounty: " + item + " dropped by a bot at difficulty " + AFHM_Util.F(ai.AFHM_GetDifficulty()));
	}

	void RecordHit(PlayerBase victim, PlayerBase attacker, string dmgZone)
	{
		if (!m_Config.Enabled || m_Config.Frozen)
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

	// Hot zones: how strongly players farm bots in a zone, from 0 (not at all, or too few fights
	// to tell) to 1 (players win 15 points or more above the target there).
	float HotZoneStrength(vector pos)
	{
		AFHM_ZoneStats zone;
		if (!m_Brain.Zones.Find(ZoneKey(pos), zone) || zone.BotsKilled + zone.PlayersKilled < m_Config.MinSamples)
			return 0;
		float over = WinRate(zone.BotsKilled, zone.PlayersKilled) - m_Config.TargetPlayerWinRate;
		return Math.Clamp(over / 0.15, 0, 1);
	}

	// Extra bots for a patrol of baseCount spawning at pos.
	int ExtraBotsFor(vector pos, int baseCount)
	{
		if (!m_Config.Enabled || !m_Config.HotZoneExtraBots || baseCount <= 0)
			return 0;
		float strength = HotZoneStrength(pos);
		if (strength <= 0)
			return 0;
		int extra = Math.Round(baseCount * m_Config.HotZoneExtraShareMax * strength);
		if (extra > m_Config.HotZoneExtraBotsMax)
			extra = m_Config.HotZoneExtraBotsMax;
		if (extra < 0)
			extra = 0;
		return extra;
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

		// How far a bot notices threats, and how far it goes to check out a gunshot.
		float threat = ai.m_eAI_ThreatDistanceLimit;
		float threatScale = ScaleFor(d, m_Config.ThreatScaleEasy, m_Config.ThreatScaleHard);
		if (Math.AbsFloat(threatScale - 1) >= 0.02)
		{
			threat = Math.Clamp(threat * threatScale, 100, 1100);
			ai.eAI_SetThreatDistanceLimit(threat);
		}
		float hearing = ai.m_eAI_NoiseInvestigationDistanceLimit;
		float hearingScale = ScaleFor(d, m_Config.NoiseScaleEasy, m_Config.NoiseScaleHard);
		if (m_Config.AdaptTactics)
			hearingScale *= HearingScale();
		if (Math.AbsFloat(hearingScale - 1) >= 0.02 && hearing > 0)
		{
			hearing = Math.Max(hearing * hearingScale, 50);
			ai.eAI_SetNoiseInvestigationDistanceLimit(hearing);
		}

		ai.AFHM_SetDifficulty(d);
		m_Brain.TotalBotsTuned++;

		string profile = "difficulty " + AFHM_Util.F(d) + " (" + why + "): accuracy " + AFHM_Util.F(accMin) + "-" + AFHM_Util.F(accMax) + ", damage x" + AFHM_Util.F(dmg) + ", takes x" + AFHM_Util.F(recv) + ", sees " + AFHM_Util.F(threat, 0) + " m, hears " + AFHM_Util.F(hearing, 0) + " m";

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

		// Night counter: night-vision goggles for some patrol bots when players fight at night.
		if (allowRoles && Math.RandomFloat01() < NightGearShare())
		{
			ai.AFHM_GiveNightVision(!m_Config.NightGearLootable);
			profile += ", night vision";
		}
		ai.AFHM_SetProfile(profile);

		if (!patrolName)
			patrolName = "unnamed patrol";
		string line = patrolName + " @ " + ZoneLabel(ZoneKey(pos)) + ": " + patrolProfile;
		// One line per patrol in the panel, not one per bot, even when two patrols spawn their
		// bots at the same time and the lines arrive interleaved.
		if (m_Recent.Find(line) == -1)
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

	void RefreshInsights()
	{
		float wins;
		float losses;
		int players;
		float counted = CountedFights(wins, losses, players);
		BuildInsights(WinRate(wins, losses), counted, players);
	}

	protected void BuildInsights(float winRate, float counted, int players)
	{
		array<string> ins = m_Brain.Insights;
		ins.Clear();
		float fights = m_Brain.BotsKilledByPlayers + m_Brain.PlayersKilledByBots;

		if (m_Config.Frozen)
			ins.Insert("Learning is FROZEN by an admin: no fights are recorded and nothing fades. Bots are still tuned from what it knows.");
		if (m_Config.PinnedDifficulty >= 0)
			ins.Insert("Difficulty is PINNED at " + AFHM_Util.F(m_Config.PinnedDifficulty) + " by an admin: fights are still learned from, but do not move it.");

		string spread = AFHM_Util.F(counted, 1) + " fights counted from " + players.ToString() + " players";
		if (m_Config.PlayerFightsCap > 0)
			spread += ", at most " + AFHM_Util.F(m_Config.PlayerFightsCap, 0) + " each";
		if (counted < m_Config.DifficultyMinFights)
		{
			ins.Insert("Not enough fights across all players yet (" + spread + "; " + AFHM_Util.F(m_Config.DifficultyMinFights, 0) + " needed): difficulty does not move.");
		}
		else
		{
			string verdict = "about right";
			if (winRate > m_Config.TargetPlayerWinRate + 0.03)
				verdict = "bots are too easy";
			else if (winRate < m_Config.TargetPlayerWinRate - 0.03)
				verdict = "bots are too hard";
			ins.Insert("Players win " + AFHM_Util.Pct(winRate) + " of lethal fights (" + spread + "; target " + AFHM_Util.Pct(m_Config.TargetPlayerWinRate) + "): " + verdict + ".");
		}

		float d = m_Brain.Difficulty;
		ins.Insert("Difficulty " + AFHM_Util.F(d) + ": accuracy x" + AFHM_Util.F(ScaleFor(d, m_Config.AccuracyScaleMin, m_Config.AccuracyScaleMax)) + ", damage x" + AFHM_Util.F(ScaleFor(d, m_Config.DamageScaleMin, m_Config.DamageScaleMax)) + ", damage taken x" + AFHM_Util.F(ScaleFor(d, m_Config.DamageReceivedScaleEasy, m_Config.DamageReceivedScaleHard)) + ".");

		if (m_Brain.KillDistanceAvg > 0)
		{
			ins.Insert("Players kill from " + AFHM_Util.F(m_Brain.KillDistanceAvg, 0) + " m on average, long shots around " + AFHM_Util.F(m_Brain.KillDistanceFar, 0) + " m. Bots see x" + AFHM_Util.F(ScaleFor(d, m_Config.ThreatScaleEasy, m_Config.ThreatScaleHard)) + " as far and hear x" + AFHM_Util.F(ScaleFor(d, m_Config.NoiseScaleEasy, m_Config.NoiseScaleHard) * HearingScale()) + " as far as configured.");
			if (m_Config.NightCounter && fights >= m_Config.MinSamples)
			{
				string night = AFHM_Util.Pct(NightShare()) + " of fights happen at night: ";
				if (NightGearShare() > 0)
					night += "up to " + AFHM_Util.Pct(NightGearShare()) + " of new patrol bots get night vision.";
				else
					night += "below " + AFHM_Util.Pct(m_Config.NightShareStart) + ", so no night vision is handed out.";
				ins.Insert(night);
			}
			if (m_Config.BountyLoot && m_Brain.TotalBountyDrops > 0)
				ins.Insert("Bounty loot dropped by hard bots so far: " + m_Brain.TotalBountyDrops.ToString() + " items.");
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
			string hotLine = "Bots die most around " + ZoneLabel(hotKey) + " (" + AFHM_Util.F(hot.BotsKilled, 1) + " lost, " + AFHM_Util.F(hot.PlayersKilled, 1) + " players killed): local difficulty " + Signed(hotOffset);
			float hotStrength = HotZoneStrength(ZoneCentre(hotKey));
			if (m_Config.HotZoneExtraBots && hotStrength > 0)
				hotLine += ", patrols there up to " + AFHM_Util.Pct(m_Config.HotZoneExtraShareMax * hotStrength) + " bigger (at most +" + m_Config.HotZoneExtraBotsMax.ToString() + " bots)";
			ins.Insert(hotLine + ".");
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
		r.Insert("AF HIVEMIND " + AFHM_Const.VERSION + " - generation " + m_Brain.Generation.ToString() + " - " + AFHM_Util.Now());
		string state = "ON";
		if (!m_Config.Enabled)
			state = "OFF (Enabled is false in config.json)";
		if (m_Config.Frozen)
			state += " | learning FROZEN";
		string pinned = "";
		if (m_Config.PinnedDifficulty >= 0)
			pinned = " PINNED";
		r.Insert("Tuning " + state + " | difficulty " + AFHM_Util.F(m_Brain.Difficulty) + pinned + " (0 easiest, 0.5 as configured, 1 hardest)");
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

	// ------------------------------------------------------------------ admin controls

	// A button on the in-game controls panel. The caller has checked the sender is an admin.
	void AdminCommand(PlayerIdentity who, string cmd)
	{
		string name = "offline admin";
		if (who)
			name = who.GetName() + " [" + who.GetPlainId() + "]";
		float current = m_Brain.Difficulty;
		if (m_Config.PinnedDifficulty >= 0)
			current = m_Config.PinnedDifficulty;

		string done;
		if (cmd == "freeze")
		{
			m_Config.Frozen = true;
			done = "froze learning";
		}
		else if (cmd == "resume")
		{
			if (m_Config.Frozen)
				m_Brain.LastFadeTimestamp = ExpansionStatic.GetTimestamp(true);
			m_Config.Frozen = false;
			done = "resumed learning";
		}
		else if (cmd == "pin")
		{
			done = "pinned difficulty at " + AFHM_Util.F(PinAt(current));
		}
		else if (cmd == "pin_down")
		{
			done = "pinned difficulty at " + AFHM_Util.F(PinAt(current - 0.05));
		}
		else if (cmd == "pin_up")
		{
			done = "pinned difficulty at " + AFHM_Util.F(PinAt(current + 0.05));
		}
		else if (cmd == "unpin")
		{
			m_Config.PinnedDifficulty = -1;
			done = "unpinned difficulty: it learns again from " + AFHM_Util.F(m_Brain.Difficulty);
		}
		else if (cmd == "evolve")
		{
			Evolve();
			done = "ran an evolve step: difficulty " + AFHM_Util.F(m_Brain.Difficulty);
		}
		else if (cmd == "reset")
		{
			ResetBrain();
			done = "reset the brain (the old one is in AFHiveMind\\backups)";
		}
		else
		{
			return;
		}

		SaveConfig();
		RefreshInsights();
		Save();
		AFHM_Util.Log("admin " + name + ": " + done);
	}

	protected float PinAt(float value)
	{
		float pin = Math.Clamp(Math.Round(value * 100) / 100, 0, 1);
		m_Config.PinnedDifficulty = pin;
		m_Brain.Difficulty = pin;
		return pin;
	}

	// Starts over with an empty brain. The current one is saved first and kept in the backups
	// folder as reset_brain_<date>_<time>.json, outside the daily backups that get cleaned up.
	protected void ResetBrain()
	{
		Save();
		if (!FileExist(AFHM_Const.BACKUP_DIR))
			MakeDirectory(AFHM_Const.BACKUP_DIR);
		string stamp = AFHM_Util.Now();
		stamp.Replace(":", "-");
		stamp.Replace(" ", "_");
		if (FileExist(AFHM_Const.BRAIN_PATH))
			CopyFile(AFHM_Const.BRAIN_PATH, AFHM_Const.BACKUP_DIR + "/reset_brain_" + stamp + ".json");

		AFHM_Brain fresh = new AFHM_Brain();
		fresh.Validate();
		fresh.LastFadeTimestamp = ExpansionStatic.GetTimestamp(true);
		if (m_Config.PinnedDifficulty >= 0)
			fresh.Difficulty = Math.Clamp(m_Config.PinnedDifficulty, 0, 1);
		m_Brain = fresh;
		m_NewEvidence = 0;
		m_NewByPlayer.Clear();
	}

	// The text sent to an admin's debug panel.
	array<string> PanelLines()
	{
		// The panel is 640 px wide: longer lines continue, indented, on the next row.
		array<string> source = BuildReport(false);
		source.Insert(" ");
		source.Insert("Admin controls (freeze, pin, reset): F10, rebindable under Controls > AF HiveMind.");
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
