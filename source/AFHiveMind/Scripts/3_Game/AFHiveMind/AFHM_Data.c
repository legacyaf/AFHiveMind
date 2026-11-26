// Data the HiveMind keeps on disk, in the server profile folder (profile\AFHiveMind):
//   config.json  - limits and switches, written with defaults on first start, re-read at every evolve step
//   brain.json   - everything it has learned; safe to read, edit or delete (it then starts over)
//   report.txt   - the same knowledge as plain text, rewritten at every save
//   hivemind.log - journal of every change of mind and, with Debug 2, every bot it tuned

class AFHM_Const
{
	// Keep equal to version in config.cpp (tools\release.ps1 checks). Shown at the top of the report.
	static const string VERSION = "0.4.0";
	static const string DIR = "$profile:AFHiveMind";
	static const string CONFIG_PATH = "$profile:AFHiveMind/config.json";
	static const string BRAIN_PATH = "$profile:AFHiveMind/brain.json";
	static const string REPORT_PATH = "$profile:AFHiveMind/report.txt";
	static const string BACKUP_DIR = "$profile:AFHiveMind/backups";
	static const int BACKUPS_KEPT = 7;
	static const string LOG_PATH = "$profile:AFHiveMind/hivemind.log";

	static const int BRAIN_VERSION = 1;

	static const int RPC_REQUEST = 874410001;
	static const int RPC_REPORT = 874410002;
	static const int RPC_COMMAND = 874410003;
};

class AFHM_Config
{
	// Which version of the settings this file was last brought up to. Older files are updated
	// on load (see AFHiveMind.LoadConfig); do not edit.
	int ConfigVersion = 0;
	bool Enabled = true;
	// 0 = journal only, 1 = also log every recorded fight, 2 = also log every tuned bot
	int DebugLevel = 1;
	// Steam64 ids allowed to open the in-game debug panel. Offline play always may.
	ref array<string> Admins = new array<string>;

	// Bots that do not come from an Expansion patrol (convoys, helicopter and tank patrols and
	// other mods that create Expansion bots themselves) are tuned this many seconds after they
	// appear, from whatever their own mod set by then. Bots led by a player are never tuned.
	bool TuneOtherBots = true;
	float OtherBotsDelaySeconds = 3;
	int SaveIntervalSeconds = 120;
	int EvolveIntervalSeconds = 600;
	// Old habits fade: after this many hours of server uptime a recorded fight counts half.
	float MemoryHalfLifeHours = 72;

	// Share of lethal player-versus-bot fights the players should win.
	float TargetPlayerWinRate = 0.8;
	// How far difficulty (0..1) may move per evolve step when there is plenty of new evidence.
	float LearnRate = 0.15;
	// Fights needed before it trusts a win rate per zone, per player and for tactics.
	float MinSamples = 3;
	// The global difficulty follows the whole community, not its best player:
	// - no player counts for more than PlayerFightsCap remembered fights (one with more still
	//   counts with their own win rate, only scaled down to that many fights; 0 = no cap),
	// - difficulty only moves once DifficultyMinFights fights count, all players together,
	// - no player adds more than PlayerFightsPerStepMax fights to one evolve step.
	// With the defaults it takes at least three players who fight bots before it moves.
	float PlayerFightsCap = 8;
	float DifficultyMinFights = 20;
	float PlayerFightsPerStepMax = 3;

	// Admin controls, also on the in-game controls panel (F10 by default).
	// Frozen: nothing is learned and nothing fades; bots are still tuned from what it knows.
	bool Frozen = false;
	// From 0 to 1: the global difficulty is held there. Below 0 (the default): it learns.
	float PinnedDifficulty = -1;

	// Difficulty 0 gives the Min scale, 0.5 gives 1 (the patrol's own setting), 1 gives the Max scale.
	float AccuracyScaleMin = 0.6;
	float AccuracyScaleMax = 1.5;
	float DamageScaleMin = 0.75;
	float DamageScaleMax = 1.3;
	// Damage the bots receive: above 1 = softer bots at low difficulty, below 1 = tougher at high.
	// Off (1 and 1) by default: try for example 1.15 and 0.9.
	float DamageReceivedScaleEasy = 1.0;
	float DamageReceivedScaleHard = 1.0;

	// Bots in a zone the players farm, or near a player who rarely loses, get this much extra at most.
	// PlayerOffsetMax is 0 (off) by default since 0.4.0: difficulty follows the community, not
	// one player. Raise it (0.2 at most is sensible) to make bots harder near strong players.
	float ZoneOffsetMax = 0.15;
	float PlayerOffsetMax = 0;
	float PlayerInfluenceRadius = 1200;
	float ZoneSize = 500;

	// How far bots notice threats, and how far they go to check out a gunshot, scaled by
	// difficulty like accuracy: Easy at 0, as configured at 0.5, Hard at 1. Expansion caps
	// what a bot can see at about 1,100 m, so threat range is never set above that.
	float ThreatScaleEasy = 0.75;
	float ThreatScaleHard = 1.1;
	float NoiseScaleEasy = 0.7;
	float NoiseScaleHard = 1.3;

	// Hot zones: patrols spawning in a zone where players win far more often than the target get
	// extra bots: up to HotZoneExtraShareMax more (0.5 = half as many again), and never more than
	// HotZoneExtraBotsMax extra per patrol. Saved (Persist) patrols are never made bigger.
	bool HotZoneExtraBots = true;
	float HotZoneExtraShareMax = 0.5;
	int HotZoneExtraBotsMax = 3;

	// Range counter: bots adapt to the range players fight at.
	// A kill beyond LongRangeDistance counts as sniping, one within CloseRangeDistance as rushing.
	bool AdaptTactics = true;
	float LongRangeDistance = 150;
	float CloseRangeDistance = 40;
	// When the community clearly snipes (or rushes), up to this share of new patrol bots swap
	// their long gun for one from MarksmanLoadout (or AssaultLoadout). 0 switches weapons off.
	float RoleShareMax = 0.4;
	string MarksmanLoadout = "AFHM_MarksmanWeapons";
	string AssaultLoadout = "AFHM_AssaultWeapons";
	// How far bots swing out to flank: up to this much wider against snipers,
	// down to this much narrower (pushing straight in) against rushers.
	float FlankWiderMax = 1.5;
	float FlankNarrowerMin = 0.6;
	// Range counter, hearing: against snipers bots go further to find a shooter they heard,
	// against rushers they stay closer to their position.
	float HearingWiderMax = 1.5;
	float HearingNarrowerMin = 0.8;

	// Night counter: when a large share of fights happens at night, some new patrol bots get
	// night-vision goggles on a headstrap. NightShareStart is the night share where it starts,
	// NightShareFull where NightGearShareMax of bots get them. With NightGearLootable false
	// the goggles are removed when the bot dies, so they never become loot.
	bool NightCounter = true;
	float NightShareStart = 0.3;
	float NightShareFull = 0.7;
	float NightGearShareMax = 0.5;
	bool NightGearLootable = false;

	// Bounty loot: a bot tuned above BountyFromDifficulty may drop one extra item where it
	// dies, but only when a player killed it. The chance rises to BountyChanceMax at
	// difficulty 1, and so does the share of drops taken from the rare list.
	bool BountyLoot = true;
	float BountyFromDifficulty = 0.6;
	float BountyChanceMax = 0.75;
	float BountyRareShareMax = 0.5;
	ref array<string> BountyCommon = {"BandageDressing", "Morphine", "Epinephrine", "AmmoBox_556x45_20Rnd", "AmmoBox_762x39_20Rnd", "AmmoBox_762x54_20Rnd", "AmmoBox_9x19_25rnd", "Mag_STANAG_30Rnd", "Mag_AKM_30Rnd"};
	ref array<string> BountyRare = {"NVGoggles", "Rangefinder", "ACOGOptic", "PSO1Optic", "PersonalRadio", "SalineBagIV", "Mag_STANAG_60Rnd", "PlateCarrierVest"};
};

class AFHM_ZoneStats
{
	float BotsKilled;
	float PlayersKilled;
	float Presence;
};

class AFHM_PlayerStats
{
	string Name;
	float BotsKilled;
	float DeathsToBots;
	float KillDistanceAvg;
	float HeadshotKills;
};

class AFHM_Brain
{
	int Version = AFHM_Const.BRAIN_VERSION;
	int Generation;
	// Real-world time (UTC, seconds) memory was last faded to, so downtime counts too.
	int LastFadeTimestamp;
	string LastUpdated;

	// What it currently believes. 0 = easiest bots, 0.5 = as configured, 1 = hardest.
	float Difficulty = 0.5;

	// Fading counts (see MemoryHalfLifeHours).
	float BotsKilledByPlayers;
	float PlayersKilledByBots;
	float HitsOnBots;
	float HeadHitsOnBots;
	float HitsOnPlayers;
	float LongKills;
	float CloseKills;
	// Lethal player-versus-bot fights that happened at night.
	float NightFights;
	int TotalBountyDrops;
	// Running averages, in metres.
	float KillDistanceAvg;
	float KillDistanceFar;
	float BotHitDistanceAvg;

	// Lifetime totals, never fade.
	int TotalBotsKilled;
	int TotalPlayersKilled;
	int TotalBotsTuned;

	ref array<float> HourActivity = new array<float>;
	ref map<string, float> Weapons = new map<string, float>;
	ref map<string, ref AFHM_ZoneStats> Zones = new map<string, ref AFHM_ZoneStats>;
	ref map<string, ref AFHM_PlayerStats> Players = new map<string, ref AFHM_PlayerStats>;

	// What it concluded at the last evolve step, in plain words.
	ref array<string> Insights = new array<string>;

	// A hand-edited or older file may miss parts.
	void Validate()
	{
		if (!HourActivity)
			HourActivity = new array<float>;
		while (HourActivity.Count() < 24)
			HourActivity.Insert(0);
		if (!Weapons)
			Weapons = new map<string, float>;
		if (!Zones)
			Zones = new map<string, ref AFHM_ZoneStats>;
		if (!Players)
			Players = new map<string, ref AFHM_PlayerStats>;
		if (!Insights)
			Insights = new array<string>;
		Difficulty = Math.Clamp(Difficulty, 0, 1);
		Version = AFHM_Const.BRAIN_VERSION;
	}
};

// Client side of the debug panel: the last report the server sent.
class AFHM_ClientState
{
	static ref array<string> s_Report = new array<string>;
	static int s_Stamp;
};

class AFHM_Util
{
	static string F(float v, int decimals = 2)
	{
		float m = Math.Pow(10, decimals);
		float r = Math.Round(v * m) / m;
		return r.ToString();
	}

	static string Pct(float v)
	{
		int p = Math.Round(v * 100);
		return p.ToString() + "%";
	}

	static string Now()
	{
		int y, mo, d, h, mi, s;
		GetYearMonthDay(y, mo, d);
		GetHourMinuteSecond(h, mi, s);
		return y.ToString() + "-" + mo.ToStringLen(2) + "-" + d.ToStringLen(2) + " " + h.ToStringLen(2) + ":" + mi.ToStringLen(2) + ":" + s.ToStringLen(2);
	}

	static void Log(string msg)
	{
		Print("[AFHiveMind] " + msg);
		FileHandle f = OpenFile(AFHM_Const.LOG_PATH, FileMode.APPEND);
		if (f != 0)
		{
			FPrintln(f, Now() + " " + msg);
			CloseFile(f);
		}
	}
};
