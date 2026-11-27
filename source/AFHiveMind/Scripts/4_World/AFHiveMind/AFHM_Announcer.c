// Tells players (or only admins) through Expansion's notifications when the AI threat level
// changes. The level runs 1-10 and follows the difficulty (0-1): 5 is the bots as configured.
//
// Two kinds of announcement:
//   the whole map - the community-wide level went up or down;
//   an area       - an area players farm (one HiveMind zone, named after the nearest town) got its
//                   own adjustment above or below the map, or lost it.
// Only changes of a whole level are announced, so small adjustments stay quiet. The first look
// after an install or update only records the current state, so it never opens with a burst.

class AFHM_Announcer
{
	protected static ref array<ref ExpansionLocation> s_Towns;

	// How AnnouncedAreas is measured (AFHM_Brain.AnnounceFormat). 0.5.0 stored gaps measured
	// against the map's rounded level; from 0.5.1 an area's gap comes from its own offset only.
	static const int FORMAT = 1;

	static int Level(float difficulty)
	{
		int level = Math.Floor(difficulty * 10 + 0.5);
		return Math.Clamp(level, 1, 10);
	}

	// Called after every learning step, and shortly after admin clicks. Cheap when nothing changed.
	static void Check(AFHiveMind hm)
	{
		if (!hm || !hm.m_Config || !hm.m_Brain)
			return;
		AFHM_Config cfg = hm.m_Config;
		AFHM_Brain brain = hm.m_Brain;
		float now = brain.Difficulty;

		// First look (or a brain saved by an older version): record what players would see
		// now, announce nothing.
		if (brain.AnnouncedDifficulty < 0 || brain.AnnounceFormat != AFHM_Announcer.FORMAT)
		{
			brain.AnnouncedDifficulty = now;
			brain.AnnounceFormat = AFHM_Announcer.FORMAT;
			brain.AnnouncedAreas.Clear();
			array<string> firstKeys = brain.Zones.GetKeyArray();
			foreach (string firstKey: firstKeys)
			{
				int firstGap = AFHM_Announcer.AreaGap(hm, firstKey);
				if (firstGap != 0)
					brain.AnnouncedAreas.Set(firstKey, firstGap);
			}
			return;
		}

		if (!cfg.Enabled || !cfg.Notify)
			return;

		// The whole map.
		float was = brain.AnnouncedDifficulty;
		if (AFHM_Announcer.Level(now) != AFHM_Announcer.Level(was))
		{
			AFHM_Announcer.AnnounceMap(hm, was, now);
			brain.AnnouncedDifficulty = now;
		}

		// Areas: announced when their own adjustment changes by a whole level. It does not
		// depend on the map's level, so moving the map never sets off the areas.
		if (cfg.NotifyAreasMax <= 0)
			return;
		array<string> keys = brain.Zones.GetKeyArray();
		array<string> oldKeys = brain.AnnouncedAreas.GetKeyArray();
		foreach (string oldKey: oldKeys)
		{
			if (keys.Find(oldKey) == -1)
				keys.Insert(oldKey);
		}

		array<string> changed = new array<string>;
		array<int> sizes = new array<int>;
		foreach (string key: keys)
		{
			int gap = AFHM_Announcer.AreaGap(hm, key);
			int lastGap = AFHM_Announcer.LastGap(brain, key);
			if (gap == lastGap)
				continue;
			changed.Insert(key);
			sizes.Insert(Math.AbsInt(gap - lastGap));
		}

		// The biggest changes first, at most NotifyAreasMax of them. The rest wait for a later step.
		for (int sent = 0; sent < cfg.NotifyAreasMax && changed.Count() > 0; sent++)
		{
			int best = 0;
			for (int i = 1; i < changed.Count(); i++)
			{
				if (sizes[i] > sizes[best])
					best = i;
			}
			string pick = changed[best];
			int pickGap = AFHM_Announcer.AreaGap(hm, pick);
			AFHM_Announcer.AnnounceArea(hm, pick, pickGap, AFHM_Announcer.LastGap(brain, pick));
			if (pickGap == 0)
				brain.AnnouncedAreas.Remove(pick);
			else
				brain.AnnouncedAreas.Set(pick, pickGap);
			changed.Remove(best);
			sizes.Remove(best);
		}
	}

	protected static int LastGap(AFHM_Brain brain, string key)
	{
		float stored;
		if (brain.AnnouncedAreas.Find(key, stored))
			return Math.Round(stored);
		return 0;
	}

	// How many levels an area's bots sit above (+) or below (-) the rest of the map, from the
	// area's own adjustment (0.1 difficulty = one level).
	static int AreaGap(AFHiveMind hm, string key)
	{
		return Math.Round(AFHM_Announcer.AreaOffset(hm, key) * 10);
	}

	// The same zone adjustment the bots get when they spawn there (see AFHiveMind.DifficultyAt).
	static float AreaOffset(AFHiveMind hm, string key)
	{
		AFHM_ZoneStats zone;
		if (!hm.m_Brain.Zones.Find(key, zone) || zone.BotsKilled + zone.PlayersKilled < hm.m_Config.MinSamples)
			return 0;
		float offset = Math.Clamp(hm.WinRate(zone.BotsKilled, zone.PlayersKilled) - hm.m_Config.TargetPlayerWinRate, -hm.m_Config.ZoneOffsetMax, hm.m_Config.ZoneOffsetMax);
		if (Math.AbsFloat(offset) < 0.01)
			return 0;
		return offset;
	}

	// "AI THREAT LEVEL 6/10" / "Bots map-wide got tougher (was 5/10). Accuracy x1.08, damage x1.05."
	protected static void AnnounceMap(AFHiveMind hm, float was, float now)
	{
		int levelWas = AFHM_Announcer.Level(was);
		int levelNow = AFHM_Announcer.Level(now);
		bool tougher = levelNow > levelWas;
		string title = "AI THREAT LEVEL " + levelNow.ToString() + "/10";
		string text = "Bots map-wide got ";
		if (tougher)
			text += "tougher";
		else
			text += "easier";
		text += " (was " + levelWas.ToString() + "/10). " + AFHM_Announcer.Scales(hm, now);
		AFHM_Announcer.Send(hm, title, text, tougher);
	}

	// "AI THREAT LEVEL 7/10: KAMENKA" / "Players keep winning here: bots are tougher than the
	// rest of the map (5/10). Patrols up to x1.4 bigger."
	protected static void AnnounceArea(AFHiveMind hm, string key, int gap, int lastGap)
	{
		float mapD = hm.m_Brain.Difficulty;
		int levelMap = AFHM_Announcer.Level(mapD);
		int levelArea = AFHM_Announcer.Level(Math.Clamp(mapD + AFHM_Announcer.AreaOffset(hm, key), 0, 1));
		string place = AFHM_Announcer.PlaceName(hm.ZoneCentre(key));
		string upperPlace = place;
		upperPlace.ToUpper();
		string title = "AI THREAT LEVEL " + levelArea.ToString() + "/10: " + upperPlace;
		string mapText = " (map " + levelMap.ToString() + "/10).";
		string text;
		if (gap == 0)
		{
			text = "Bots here are back to the map's level" + mapText;
		}
		else if (gap > 0)
		{
			text = "Players keep winning here: bots are tougher than the rest of the map" + mapText;
			float strength = hm.HotZoneStrength(hm.ZoneCentre(key));
			if (hm.m_Config.HotZoneExtraBots && strength > 0 && hm.m_Config.HotZoneExtraBotsMax > 0)
			{
				float bigger = 1 + Math.Round(strength * hm.m_Config.HotZoneExtraShareMax * 10) / 10;
				if (bigger > 1.05)
					text += " Patrols up to x" + AFHM_Util.F(bigger, 1) + " bigger.";
			}
		}
		else
		{
			text = "Bots keep winning here: they ease off compared to the rest of the map" + mapText;
		}
		// Red when this area got tougher than players were last told, green when it eased off.
		AFHM_Announcer.Send(hm, title, text, gap > lastGap);
	}

	// "Accuracy x1.16, damage x1.09, hearing x1.09." against how the admins configured the bots;
	// only what differs from x1 is listed.
	protected static string Scales(AFHiveMind hm, float d)
	{
		AFHM_Config cfg = hm.m_Config;
		array<string> parts = new array<string>;
		AFHM_Announcer.AddScale(parts, "accuracy", d, cfg.AccuracyScaleMin, cfg.AccuracyScaleMax);
		AFHM_Announcer.AddScale(parts, "damage", d, cfg.DamageScaleMin, cfg.DamageScaleMax);
		AFHM_Announcer.AddScale(parts, "damage taken", d, cfg.DamageReceivedScaleEasy, cfg.DamageReceivedScaleHard);
		AFHM_Announcer.AddScale(parts, "sight", d, cfg.ThreatScaleEasy, cfg.ThreatScaleHard);
		AFHM_Announcer.AddScale(parts, "hearing", d, cfg.NoiseScaleEasy, cfg.NoiseScaleHard);
		if (parts.Count() == 0)
			return "Bots are as the server set them.";
		string head = parts[0];
		string first = head.Substring(0, 1);
		first.ToUpper();
		string text = first + head.Substring(1, head.Length() - 1);
		for (int i = 1; i < parts.Count(); i++)
			text += ", " + parts[i];
		return text + ".";
	}

	protected static void AddScale(array<string> parts, string label, float d, float lo, float hi)
	{
		// Multipliers, not percent: Expansion's notifications strip "%" from the text.
		float scale = Math.Round(AFHiveMind.ScaleFor(d, lo, hi) * 100) / 100;
		if (Math.AbsFloat(scale - 1) < 0.005)
			return;
		parts.Insert(label + " x" + AFHM_Util.F(scale));
	}

	// The nearest town or village on this map, from the map's own list of places.
	static string PlaceName(vector pos)
	{
		if (!s_Towns)
		{
			s_Towns = ExpansionLocation.GetWorldLocations(ExpansionLocationType.SETTLEMENT);
			if (!s_Towns)
				s_Towns = new array<ref ExpansionLocation>;
		}
		ExpansionLocation nearest;
		float bestSq = float.MAX;
		foreach (ExpansionLocation town: s_Towns)
		{
			float dx = town.Position[0] - pos[0];
			float dz = town.Position[2] - pos[2];
			float sq = dx * dx + dz * dz;
			if (sq < bestSq)
			{
				bestSq = sq;
				nearest = town;
			}
		}
		if (!nearest)
		{
			int gridX = Math.Round(pos[0]);
			int gridZ = Math.Round(pos[2]);
			return "grid " + gridX.ToString() + " / " + gridZ.ToString();
		}
		return AFHM_Announcer.CleanName(nearest);
	}

	// Map configs give either a plain name or a string-table key; for a key, the class name
	// ("Settlement_Chernogorsk") carries the plain name.
	// Chernarus lists its towns in Russian ("Гришино"); the class name ("Settlement_Grishino") has
	// the English spelling, so a name with any non-ASCII letter falls back to the class name.
	protected static string CleanName(ExpansionLocation town)
	{
		string name = town.Name;
		if (name && name.IndexOf("$") != 0 && name.IndexOf("#") != 0 && AFHM_Announcer.IsPlain(name))
			return name;
		string cls = town.m_ClassName;
		int cut = cls.LastIndexOf("_");
		if (cut >= 0 && cut < cls.Length() - 1)
			return cls.Substring(cut + 1, cls.Length() - cut - 1);
		return cls;
	}

	protected static bool IsPlain(string text)
	{
		for (int i = 0; i < text.Length(); i++)
		{
			int code = text.Get(i).ToAscii();
			if (code < 32 || code > 126)
				return false;
		}
		return true;
	}

	protected static void Send(AFHiveMind hm, string title, string text, bool tougher)
	{
		// DayZ drops RPCs with long strings, and long notifications cover the screen.
		if (text.Length() > 250)
			text = text.Substring(0, 247) + "...";

		string icon = "Shield";
		int color = ARGB(255, 80, 200, 120);
		if (tougher)
		{
			icon = "Skull 1";
			color = ARGB(255, 220, 60, 60);
		}

		if (hm.m_Config.NotifyPlayers)
		{
			ExpansionNotification(title, text, icon, color, hm.m_Config.NotifySeconds).Create();
		}
		else
		{
			array<Man> players = new array<Man>;
			GetGame().GetPlayers(players);
			foreach (Man man: players)
			{
				PlayerIdentity identity = man.GetIdentity();
				if (identity && hm.IsAdmin(identity))
					ExpansionNotification(title, text, icon, color, hm.m_Config.NotifySeconds).Create(identity);
			}
		}
		AFHM_Util.Log("announced: " + title + " | " + text);
	}
};
