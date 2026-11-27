// Tells players (or only admins) through Expansion's notifications when the AI threat level
// changes. The level runs 1-10 and follows the difficulty (0-1): 5 is the bots as configured.
//
// Two kinds of announcement:
//   the whole map - the community-wide level went up or down;
//   an area       - an area players farm (one HiveMind zone, named after the nearest town) moved
//                   above or below the map's level, or came back to it.
// Only changes of a whole level are announced, so small adjustments stay quiet. The first look
// after an install or update only records the current state, so it never opens with a burst.

class AFHM_Announcer
{
	protected static ref array<ref ExpansionLocation> s_Towns;

	static int Level(float difficulty)
	{
		int level = Math.Floor(difficulty * 10 + 0.5);
		return Math.Clamp(level, 1, 10);
	}

	// Called after every learning step and every admin action. Cheap when nothing changed.
	static void Check(AFHiveMind hm)
	{
		if (!hm || !hm.m_Config || !hm.m_Brain)
			return;
		AFHM_Config cfg = hm.m_Config;
		AFHM_Brain brain = hm.m_Brain;
		float now = brain.Difficulty;

		// First look: record what players would see now, announce nothing.
		if (brain.AnnouncedDifficulty < 0)
		{
			brain.AnnouncedDifficulty = now;
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

		// Areas: an area is announced when its gap to the map's level changes, so a change of the
		// map's own level does not set off every area again.
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
			float stored;
			int lastGap = 0;
			if (brain.AnnouncedAreas.Find(key, stored))
				lastGap = Math.Round(stored);
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
			AFHM_Announcer.AnnounceArea(hm, pick, pickGap);
			if (pickGap == 0)
				brain.AnnouncedAreas.Remove(pick);
			else
				brain.AnnouncedAreas.Set(pick, pickGap);
			changed.Remove(best);
			sizes.Remove(best);
		}
	}

	// How many levels an area's bots sit above (+) or below (-) the map's level.
	static int AreaGap(AFHiveMind hm, string key)
	{
		float mapD = hm.m_Brain.Difficulty;
		return AFHM_Announcer.Level(mapD + AFHM_Announcer.AreaOffset(hm, key)) - AFHM_Announcer.Level(mapD);
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

	protected static void AnnounceMap(AFHiveMind hm, float was, float now)
	{
		int levelWas = AFHM_Announcer.Level(was);
		int levelNow = AFHM_Announcer.Level(now);
		string title = "AI THREAT LEVEL " + levelNow.ToString() + "/10";
		string text;
		if (levelNow > levelWas)
			text = "Bots across the whole map got tougher (level " + levelWas.ToString() + " to " + levelNow.ToString() + "). ";
		else
			text = "Bots across the whole map eased off (level " + levelWas.ToString() + " to " + levelNow.ToString() + "). ";
		text += AFHM_Announcer.Changes(hm, was, now, "was");
		AFHM_Announcer.Send(hm, title, text, levelNow > levelWas);
	}

	protected static void AnnounceArea(AFHiveMind hm, string key, int gap)
	{
		float mapD = hm.m_Brain.Difficulty;
		float area = Math.Clamp(mapD + AFHM_Announcer.AreaOffset(hm, key), 0, 1);
		int levelMap = AFHM_Announcer.Level(mapD);
		int levelArea = AFHM_Announcer.Level(area);
		string place = AFHM_Announcer.PlaceName(hm.ZoneCentre(key));
		string upperPlace = place;
		upperPlace.ToUpper();
		string title = "AI THREAT LEVEL " + levelArea.ToString() + "/10: " + upperPlace;
		string text;
		bool tougher = gap > 0;
		if (gap == 0)
		{
			text = "Bots around " + place + " are back to the map's level (" + levelMap.ToString() + "/10).";
			tougher = false;
		}
		else if (gap > 0)
		{
			text = "Players keep winning around " + place + ", so bots there are tougher than the rest of the map (level " + levelArea.ToString() + ", map " + levelMap.ToString() + "). ";
			text += AFHM_Announcer.Changes(hm, mapD, area, "map");
			float strength = hm.HotZoneStrength(hm.ZoneCentre(key));
			if (hm.m_Config.HotZoneExtraBots && strength > 0)
			{
				float bigger = 1 + Math.Round(strength * hm.m_Config.HotZoneExtraShareMax * 10) / 10;
				if (bigger > 1.05 && hm.m_Config.HotZoneExtraBotsMax > 0)
					text += " Patrols there come out up to x" + AFHM_Util.F(bigger, 1) + " as big (at most " + hm.m_Config.HotZoneExtraBotsMax.ToString() + " extra bots).";
			}
		}
		else
		{
			text = "Bots keep winning around " + place + ", so bots there ease off compared to the rest of the map (level " + levelArea.ToString() + ", map " + levelMap.ToString() + "). ";
			text += AFHM_Announcer.Changes(hm, mapD, area, "map");
		}
		AFHM_Announcer.Send(hm, title, text, tougher);
	}

	// "accuracy 110% (was 105%), damage 106% (was 103%), ..." for what differs between two
	// difficulties, in percent of how the admins configured the bots.
	protected static string Changes(AFHiveMind hm, float from, float to, string word)
	{
		AFHM_Config cfg = hm.m_Config;
		array<string> parts = new array<string>;
		AFHM_Announcer.AddChange(parts, "accuracy", from, to, cfg.AccuracyScaleMin, cfg.AccuracyScaleMax, word);
		AFHM_Announcer.AddChange(parts, "damage", from, to, cfg.DamageScaleMin, cfg.DamageScaleMax, word);
		AFHM_Announcer.AddChange(parts, "damage taken", from, to, cfg.DamageReceivedScaleEasy, cfg.DamageReceivedScaleHard, word);
		AFHM_Announcer.AddChange(parts, "sight", from, to, cfg.ThreatScaleEasy, cfg.ThreatScaleHard, word);
		AFHM_Announcer.AddChange(parts, "hearing", from, to, cfg.NoiseScaleEasy, cfg.NoiseScaleHard, word);
		if (parts.Count() == 0)
			return "";
		string text = "Now: ";
		for (int i = 0; i < parts.Count(); i++)
		{
			if (i > 0)
				text += ", ";
			text += parts[i];
		}
		return text + ".";
	}

	protected static void AddChange(array<string> parts, string label, float from, float to, float lo, float hi, string word)
	{
		// Multipliers, not percent: Expansion's notifications strip "%" from the text.
		float before = Math.Round(AFHiveMind.ScaleFor(from, lo, hi) * 100) / 100;
		float after = Math.Round(AFHiveMind.ScaleFor(to, lo, hi) * 100) / 100;
		if (Math.AbsFloat(before - after) < 0.005)
			return;
		parts.Insert(label + " x" + AFHM_Util.F(after) + " (" + word + " x" + AFHM_Util.F(before) + ")");
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
		// DayZ drops RPCs with long strings; keep each notification short.
		if (text.Length() > 380)
			text = text.Substring(0, 377) + "...";

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
