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
	// The last announcement sent, title and text (read by the test mission).
	static string s_LastSent;
	// The last ten announcements, and how many players got the last one (-1 = everyone).
	static ref array<string> s_Sent = new array<string>;
	static int s_LastRecipients;
	protected static ref map<string, string> s_PlaceCache = new map<string, string>;

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

	// The whole map's level changed. Wording from config.json (NotifyTitleMap, NotifyMapTougher,
	// NotifyMapEasier).
	protected static void AnnounceMap(AFHiveMind hm, float was, float now)
	{
		AFHM_Config cfg = hm.m_Config;
		int levelWas = AFHM_Announcer.Level(was);
		int levelNow = AFHM_Announcer.Level(now);
		bool tougher = levelNow > levelWas;
		string text = cfg.NotifyMapEasier;
		if (tougher)
			text = cfg.NotifyMapTougher;
		if (text == "")
			return;
		map<string, string> values = new map<string, string>;
		values.Set("level", levelNow.ToString());
		values.Set("was", levelWas.ToString());
		values.Set("map", levelNow.ToString());
		values.Set("changes", AFHM_Announcer.Scales(hm, now));
		AFHM_Announcer.Send(hm, AFHM_Announcer.Fill(cfg.NotifyTitleMap, values), AFHM_Announcer.Fill(text, values), tougher);
	}

	// An area's own adjustment changed. Wording from config.json (NotifyTitleArea,
	// NotifyAreaTougher, NotifyAreaEasier, NotifyAreaBack, NotifyPatrols).
	protected static void AnnounceArea(AFHiveMind hm, string key, int gap, int lastGap)
	{
		AFHM_Config cfg = hm.m_Config;
		string text = cfg.NotifyAreaBack;
		if (gap > 0)
			text = cfg.NotifyAreaTougher;
		else if (gap < 0)
			text = cfg.NotifyAreaEasier;
		if (text == "")
			return;

		float mapD = hm.m_Brain.Difficulty;
		int levelMap = AFHM_Announcer.Level(mapD);
		int levelArea = AFHM_Announcer.Level(Math.Clamp(mapD + AFHM_Announcer.AreaOffset(hm, key), 0, 1));
		vector centre = hm.ZoneCentre(key);
		string place = AFHM_Announcer.PlaceName(centre);
		string upperPlace = place;
		upperPlace.ToUpper();

		string patrols = "";
		string bigger = "1";
		float strength = hm.HotZoneStrength(centre);
		if (gap > 0 && cfg.HotZoneExtraBots && strength > 0 && cfg.HotZoneExtraBotsMax > 0)
		{
			float biggerBy = 1 + Math.Round(strength * cfg.HotZoneExtraShareMax * 10) / 10;
			bigger = AFHM_Util.F(biggerBy, 1);
			if (biggerBy > 1.05)
				patrols = cfg.NotifyPatrols;
		}

		map<string, string> values = new map<string, string>;
		values.Set("level", levelArea.ToString());
		values.Set("was", levelMap.ToString());
		values.Set("map", levelMap.ToString());
		values.Set("place", place);
		values.Set("PLACE", upperPlace);
		values.Set("bigger", bigger);
		values.Set("extra", cfg.HotZoneExtraBotsMax.ToString());
		values.Set("changes", AFHM_Announcer.Scales(hm, Math.Clamp(mapD + AFHM_Announcer.AreaOffset(hm, key), 0, 1)));
		values.Set("patrols", AFHM_Announcer.Fill(patrols, values));
		// Red when this area got tougher than players were last told, green when it eased off.
		AFHM_Announcer.Send(hm, AFHM_Announcer.Fill(cfg.NotifyTitleArea, values), AFHM_Announcer.Fill(text, values), gap > lastGap, true, centre);
	}

	// Replaces each {name} in the template with its value, and tidies the spaces an empty
	// value leaves behind.
	static string Fill(string template, map<string, string> values)
	{
		string text = template;
		foreach (string name, string value: values)
			text.Replace("{" + name + "}", value);
		while (text.Contains("  "))
			text.Replace("  ", " ");
		text = text.Trim();
		return text;
	}

	// "Accuracy x1.16, damage x1.09, hearing x1.09." against how the admins configured the bots;
	// only what differs from x1 is listed. The words come from config.json.
	protected static string Scales(AFHiveMind hm, float d)
	{
		AFHM_Config cfg = hm.m_Config;
		array<string> parts = new array<string>;
		AFHM_Announcer.AddScale(parts, cfg.NotifyWordAccuracy, d, cfg.AccuracyScaleMin, cfg.AccuracyScaleMax);
		AFHM_Announcer.AddScale(parts, cfg.NotifyWordDamage, d, cfg.DamageScaleMin, cfg.DamageScaleMax);
		AFHM_Announcer.AddScale(parts, cfg.NotifyWordDamageTaken, d, cfg.DamageReceivedScaleEasy, cfg.DamageReceivedScaleHard);
		AFHM_Announcer.AddScale(parts, cfg.NotifyWordSight, d, cfg.ThreatScaleEasy, cfg.ThreatScaleHard);
		AFHM_Announcer.AddScale(parts, cfg.NotifyWordHearing, d, cfg.NoiseScaleEasy, cfg.NoiseScaleHard);
		if (parts.Count() == 0)
			return cfg.NotifyAsConfigured;
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
		if (label == "" || Math.AbsFloat(scale - 1) < 0.005)
			return;
		parts.Insert(label + " x" + AFHM_Util.F(scale));
	}

	// "220 60 60" (or "220,60,60") to a colour; anything unreadable gives the fallback.
	static int Colour(string rgb, int fallback)
	{
		string clean = rgb;
		clean.Replace(",", " ");
		array<string> bits = new array<string>;
		clean.Split(" ", bits);
		array<int> nums = new array<int>;
		foreach (string bit: bits)
		{
			string trimmed = bit.Trim();
			if (trimmed != "")
				nums.Insert(Math.Clamp(trimmed.ToInt(), 0, 255));
		}
		if (nums.Count() < 3)
			return fallback;
		return ARGB(255, nums[0], nums[1], nums[2]);
	}

	// The nearest town or village on this map, from the map's own list of places.
	static string PlaceName(vector pos)
	{
		// Asked for every panel line: remember the answer per spot (to the metre).
		int keyX = Math.Round(pos[0]);
		int keyZ = Math.Round(pos[2]);
		string cacheKey = keyX.ToString() + "_" + keyZ.ToString();
		string cached;
		if (s_PlaceCache.Find(cacheKey, cached))
			return cached;
		if (s_PlaceCache.Count() > 2000)
			s_PlaceCache.Clear();
		string found = AFHM_Announcer.FindPlace(pos);
		s_PlaceCache.Set(cacheKey, found);
		return found;
	}

	protected static string FindPlace(vector pos)
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

	// area true: an area announcement centred on pos, for players near it (NotifyAreaRadius).
	protected static void Send(AFHiveMind hm, string title, string text, bool tougher, bool area = false, vector pos = "0 0 0")
	{
		// DayZ drops RPCs with long strings, and long notifications cover the screen.
		if (text.Length() > 250)
			text = text.Substring(0, 247) + "...";
		if (title.Length() > 80)
			title = title.Substring(0, 77) + "...";

		AFHM_Config cfg = hm.m_Config;
		string icon = cfg.NotifyIconEasier;
		int color = AFHM_Announcer.Colour(cfg.NotifyColorEasier, ARGB(255, 80, 200, 120));
		if (tougher)
		{
			icon = cfg.NotifyIconTougher;
			color = AFHM_Announcer.Colour(cfg.NotifyColorTougher, ARGB(255, 220, 60, 60));
		}

		float radius = cfg.NotifyAreaRadius;
		bool everyone = cfg.NotifyPlayers && (!area || radius <= 0);
		s_LastRecipients = -1;
		if (everyone)
		{
			ExpansionNotification(title, text, icon, color, cfg.NotifySeconds).Create();
		}
		else
		{
			// Admins always; other players only when announcements are for players and they are
			// near the area.
			s_LastRecipients = 0;
			float radiusSq = radius * radius;
			array<Man> players = new array<Man>;
			GetGame().GetPlayers(players);
			bool online = GetGame().IsMultiplayer();
			foreach (Man man: players)
			{
				// Bots can be in the player list; offline, the one player has no identity.
				PlayerBase pb = PlayerBase.Cast(man);
				if (!pb || pb.IsAI())
					continue;
				PlayerIdentity identity = man.GetIdentity();
				if (!identity && online)
					continue;
				bool send = false;
				if (identity)
					send = hm.IsAdmin(identity);
				if (!send && cfg.NotifyPlayers && area)
				{
					vector at = man.GetPosition();
					float dx = at[0] - pos[0];
					float dz = at[2] - pos[2];
					send = dx * dx + dz * dz <= radiusSq;
				}
				if (!send)
					continue;
				if (identity)
					ExpansionNotification(title, text, icon, color, cfg.NotifySeconds).Create(identity);
				else
					ExpansionNotification(title, text, icon, color, cfg.NotifySeconds).Create();
				s_LastRecipients++;
			}
		}
		s_LastSent = title + " | " + text;
		s_Sent.Insert(s_LastSent);
		if (s_Sent.Count() > 10)
			s_Sent.Remove(0);
		AFHM_Util.Log("announced: " + s_LastSent);
		AFHM_Discord.Announcement(hm, title, text, area);
	}
};
