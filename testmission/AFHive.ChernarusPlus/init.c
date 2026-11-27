// Offline test for AF HiveMind. Runs only in DayZDiag_x64.exe on this PC; it never connects
// to a server. Start it with testmission\run_test.ps1.
//
// 1. A patrol spawns while the brain is empty: its bots must come out as configured.
// 2. The player's rifle kills that whole patrol (damage dealt by script, with the rifle as
//    its source, so the game reports it exactly like shots fired by the player).
// 3. The HiveMind is told to evolve now instead of after ten minutes.
// 4. A second patrol spawns: its bots must come out harder, and say why.
// 5. The player stops being invulnerable and the second patrol is left to kill them, which
//    the HiveMind must see as a player lost to bots.

PlayerBase g_HivePlayer;
EntityAI g_HiveGun;
vector g_HivePos;

void main()
{
	GetGame().GetWorld().SetDate(2026, 6, 15, 12, 0);
	Weather weather = GetGame().GetWeather();
	weather.GetOvercast().Set(0, 0, 0);
	weather.GetRain().Set(0, 0, 0);
	weather.GetFog().Set(0, 0, 0);

	g_HivePos = "4660 0 10330";
	g_HivePos[1] = GetGame().SurfaceY(g_HivePos[0], g_HivePos[2]);
	g_HivePlayer = PlayerBase.Cast(GetGame().CreatePlayer(NULL, "SurvivorM_Mirek", g_HivePos, 0, "NONE"));
	GetGame().SelectPlayer(NULL, g_HivePlayer);
	g_HivePlayer.GetInventory().CreateInInventory("TShirt_Green");
	g_HivePlayer.GetInventory().CreateInInventory("CargoPants_Green");
	g_HivePlayer.GetInventory().CreateInInventory("AthleticShoes_Green");
	g_HiveGun = g_HivePlayer.GetHumanInventory().CreateInHands("M4A1");
	g_HivePlayer.SetAllowDamage(false);
}

class CustomMission: MissionGameplay
{
	protected float m_HiveTime;
	protected int m_HiveStep = -1;
	protected float m_HiveStepTime;
	protected int m_HiveVolleys;
	protected float m_AnnouncedBefore;
	protected int m_LevelBefore;
	protected float m_DragX;
	protected string m_ZoneReset;
	protected float m_DragY;
	protected eAIDynamicPatrol m_Alpha;
	protected eAIDynamicPatrol m_Bravo;

	eAIDynamicPatrol SpawnPatrol(string name, int count, vector offset)
	{
		vector pos = g_HivePos + offset;
		pos[1] = GetGame().SurfaceY(pos[0], pos[2]);
		TVectorArray way = new TVectorArray;
		way.Insert(pos);
		ExpansionAIPatrol cfg = new ExpansionAIPatrol(count, "WALK", "SPRINT", "HALT", "Raiders", "", true, 0, 1.0, 0, 800, -1, 0, way);
		cfg.Name = name;
		eAIDynamicPatrol patrol = eAIDynamicPatrol.CreateEx(cfg, pos, false);
		if (!patrol)
		{
			Print("[AFHive] FAILED to create patrol " + name);
			return null;
		}
		patrol.Spawn();
		Print("[AFHive] patrol " + name + " asked to spawn " + count.ToString() + " bots at " + pos.ToString());
		return patrol;
	}

	void ReportPatrol(string name, eAIDynamicPatrol patrol)
	{
		if (!patrol || !patrol.m_Group)
		{
			Print("[AFHive] patrol " + name + " has no group");
			return;
		}
		Print("[AFHive] patrol " + name + " has " + patrol.m_Group.Count().ToString() + " bots");
		for (int i = 0; i < patrol.m_Group.Count(); i++)
		{
			eAIBase ai = eAIBase.Cast(patrol.m_Group.GetMember(i));
			if (ai)
				Print("[AFHive] " + name + " bot " + i.ToString() + ": accuracy " + ai.m_eAI_AccuracyMin.ToString() + "-" + ai.m_eAI_AccuracyMax.ToString() + ", damage x" + ai.m_eAI_DamageMultiplier.ToString() + ", takes x" + ai.m_eAI_DamageReceivedMultiplier.ToString() + ", threat " + ai.m_eAI_ThreatDistanceLimit.ToString() + " | hivemind: " + ai.AFHM_GetProfile());
		}
	}

	// One rifle hit to the head of every living bot of the patrol; returns how many still live.
	// One hit per bot per call: the game takes only one scripted hit per character per frame,
	// and Expansion re-applies a hit a frame later when a bot's damage multiplier is not 1.
	int ShootPatrol(eAIDynamicPatrol patrol)
	{
		if (!patrol || !patrol.m_Group)
			return 0;
		int alive;
		for (int i = 0; i < patrol.m_Group.Count(); i++)
		{
			eAIBase ai = eAIBase.Cast(patrol.m_Group.GetMember(i));
			if (!ai || !ai.IsAlive())
				continue;
			alive++;
			ai.ProcessDirectDamage(DT_FIRE_ARM, g_HiveGun, "Head", "Bullet_556x45", "0 0 0", 20);
		}
		return alive;
	}

	void BotKillsPlayer(eAIDynamicPatrol patrol)
	{
		if (!patrol || !patrol.m_Group || patrol.m_Group.Count() == 0)
		{
			Print("[AFHive] FAILED: no Bravo bot left to attribute the kill to");
			return;
		}
		eAIBase shooter = eAIBase.Cast(patrol.m_Group.GetMember(0));
		// Melee damage with the bot as its source. Bullet damage cannot be faked for a bot:
		// Expansion checks every bullet hit against a shot the bot really fired.
		Print("[AFHive] damage source: " + shooter.GetType());
		for (int hit = 0; hit < 5 && g_HivePlayer.IsAlive(); hit++)
			g_HivePlayer.ProcessDirectDamage(DT_CLOSE_COMBAT, shooter, "Head", "MeleeFist_Heavy", "0 0 0", 500);
		Print("[AFHive] player alive after that: " + g_HivePlayer.IsAlive().ToString());
	}

	protected eAIBase m_Lone;
	protected eAIDynamicPatrol m_Charlie;
	protected eAIDynamicPatrol m_Delta;
	protected ref array<eAIBase> m_DeltaBots = new array<eAIBase>;
	protected int m_BountyBefore;

	void ReportDelta()
	{
		if (!m_Delta || !m_Delta.m_Group)
		{
			Print("[AFHive] Delta: no group");
			return;
		}
		for (int i = 0; i < m_Delta.m_Group.Count(); i++)
		{
			eAIBase ai = eAIBase.Cast(m_Delta.m_Group.GetMember(i));
			if (!ai)
				continue;
			m_DeltaBots.Insert(ai);
			Print("[AFHive] Delta bot " + i.ToString() + ": night vision " + ai.AFHM_HasNightVision().ToString() + ", sees " + ai.m_eAI_ThreatDistanceLimit.ToString() + " m, hears " + ai.m_eAI_NoiseInvestigationDistanceLimit.ToString() + " m, difficulty " + ai.AFHM_GetDifficulty().ToString() + " | " + ai.AFHM_GetProfile());
		}
	}

	void ReportDeltaDead()
	{
		int dead;
		int withGoggles;
		foreach (eAIBase bot: m_DeltaBots)
		{
			if (!bot)
				continue;
			if (!bot.IsAlive())
				dead++;
			if (bot.AFHM_HasNightVision())
				withGoggles++;
		}
		int drops = AFHiveMind.Get().m_Brain.TotalBountyDrops - m_BountyBefore;
		Print("[AFHive] Delta after the shooting: " + dead.ToString() + " of " + m_DeltaBots.Count().ToString() + " dead, bodies still wearing goggles " + withGoggles.ToString() + ", bounty items dropped " + drops.ToString());
	}

	void ReportWeapons(string name, eAIDynamicPatrol patrol)
	{
		if (!patrol || !patrol.m_Group)
		{
			Print("[AFHive] " + name + ": no group");
			return;
		}
		for (int i = 0; i < patrol.m_Group.Count(); i++)
		{
			eAIBase ai = eAIBase.Cast(patrol.m_Group.GetMember(i));
			if (!ai)
				continue;
			string hands = "nothing";
			EntityAI held = ai.GetHumanInventory().GetEntityInHands();
			if (held)
				hands = held.GetType();
			string shoulder = "nothing";
			EntityAI slung = ai.GetInventory().FindAttachment(InventorySlots.SHOULDER);
			if (slung)
				shoulder = slung.GetType();
			Print("[AFHive] " + name + " bot " + i.ToString() + " weapons: hands " + hands + ", shoulder " + shoulder + ", flanking " + ai.m_eAI_MaxFlankingDistance.ToString() + " | " + ai.AFHM_GetProfile());
		}
	}
	protected eAIBase m_Follower;

	// Bots made the way convoy and helicopter mods make them: created directly, no patrol.
	// The lone one must be tuned after a few seconds; the one following the player must not.
	void SpawnLoneBots()
	{
		vector pos = g_HivePos + "-20 0 30";
		pos[1] = GetGame().SurfaceY(pos[0], pos[2]);
		m_Lone = eAIBase.Cast(GetGame().CreateObject("eAI_SurvivorM_Boris", pos));
		if (m_Lone)
		{
			eAIGroup convoy = eAIGroup.CreateGroup(eAIFaction.Create("Raiders"));
			convoy.SetName("Test convoy");
			m_Lone.SetGroup(convoy);
		}

		pos = g_HivePos + "3 0 -3";
		pos[1] = GetGame().SurfaceY(pos[0], pos[2]);
		m_Follower = eAIBase.Cast(GetGame().CreateObject("eAI_SurvivorM_Denis", pos));
		if (m_Follower)
			m_Follower.SetGroup(eAIGroup.GetGroupByLeader(g_HivePlayer));

		Print("[AFHive] lone bots created: convoy " + m_Lone.ToString() + ", follower " + m_Follower.ToString() + "; tuned right away: " + m_Lone.AFHM_IsTuned().ToString() + " / " + m_Follower.AFHM_IsTuned().ToString());
	}

	void ReportLoneBots()
	{
		if (m_Lone)
			Print("[AFHive] convoy bot after the wait: tuned " + m_Lone.AFHM_IsTuned().ToString() + ", accuracy " + m_Lone.m_eAI_AccuracyMin.ToString() + "-" + m_Lone.m_eAI_AccuracyMax.ToString() + " | hivemind: " + m_Lone.AFHM_GetProfile());
		if (m_Follower)
			Print("[AFHive] follower bot after the wait: leader is the player " + (m_Follower.GetGroup().GetLeader() == g_HivePlayer).ToString() + ", accuracy " + m_Follower.m_eAI_AccuracyMin.ToString() + "-" + m_Follower.m_eAI_AccuracyMax.ToString() + ", profile '" + m_Follower.AFHM_GetProfile() + "'");
	}

	void PrintReport(string label)
	{
		AFHiveMind hm = AFHiveMind.Get();
		if (!hm)
		{
			Print("[AFHive] FAILED: no HiveMind instance");
			return;
		}
		Print("[AFHive] ---- report " + label + " ----");
		array<string> lines = hm.BuildReport(true);
		foreach (string line: lines)
			Print("[AFHive] | " + line);
	}

	void Next()
	{
		m_HiveStep++;
		m_HiveStepTime = 0;
	}

	override void OnUpdate(float timeslice)
	{
		super.OnUpdate(timeslice);
		if (!g_HivePlayer)
			return;
		m_HiveTime += timeslice;
		m_HiveStepTime += timeslice;

		switch (m_HiveStep)
		{
			case -1:
				if (m_HiveTime >= 12)
				{
					PrintReport("at start");
					// Announcements: the first look only records the current level (5/10).
					AFHM_Announcer.Check(AFHiveMind.Get());
					m_AnnouncedBefore = AFHiveMind.Get().m_Brain.AnnouncedDifficulty;
					Print("[AFHive] announcer baseline: level " + AFHM_Announcer.Level(m_AnnouncedBefore).ToString() + ", nearest town to the test site: " + AFHM_Announcer.PlaceName(g_HivePos));
					m_Alpha = SpawnPatrol("Alpha", 6, "0 0 40");
					Next();
				}
				break;
			case 0:
				if (m_HiveStepTime >= 6)
				{
					ReportPatrol("Alpha", m_Alpha);
					m_HiveVolleys = 0;
					m_HiveStep = 100;
					m_HiveStepTime = 0;
				}
				break;
			case 100:
				// A volley every half second until no Alpha bot is left standing.
				if (m_HiveStepTime >= 0.5)
				{
					m_HiveStepTime = 0;
					int standing = ShootPatrol(m_Alpha);
					if (standing == 0)
					{
						Print("[AFHive] all Alpha bots are dead after " + m_HiveVolleys.ToString() + " volleys of rifle damage");
						m_HiveStep = 1;
					}
					else if (m_HiveVolleys >= 10)
					{
						Print("[AFHive] FAILED: " + standing.ToString() + " Alpha bots still alive after 10 volleys");
						m_HiveStep = 1;
					}
					else
					{
						m_HiveVolleys++;
					}
				}
				break;
			case 1:
				if (m_HiveStepTime >= 3)
				{
					// The test has one player, so the community rules (at most 8 fights per
					// player, 20 in total) would hold difficulty still. Saved to the test
					// profile's config.json, because every evolve step re-reads it.
					AFHiveMind hmCap = AFHiveMind.Get();
					hmCap.m_Config.PlayerFightsCap = 0;
					hmCap.m_Config.DifficultyMinFights = 3;
					hmCap.m_Config.PlayerFightsPerStepMax = 0;
					hmCap.m_Config.Frozen = false;
					hmCap.m_Config.PinnedDifficulty = -1;
					hmCap.SaveConfig();
					AFHiveMind.Get().Evolve();
					PrintReport("after the player wiped out Alpha");
					// 0.50 -> 0.55 is level 5 -> 6: the whole map must have been announced, and the
					// test site's zone (players won every fight there) as an area above the map.
					AFHM_Brain told = AFHiveMind.Get().m_Brain;
					int mapNow = AFHM_Announcer.Level(told.Difficulty);
					if (told.AnnouncedDifficulty < 0 || AFHM_Announcer.Level(told.AnnouncedDifficulty) != mapNow || mapNow == AFHM_Announcer.Level(m_AnnouncedBefore))
						Print("[AFHive] FAILED: no map announcement (baseline " + m_AnnouncedBefore.ToString() + ", announced " + told.AnnouncedDifficulty.ToString() + ", difficulty " + told.Difficulty.ToString() + ")");
					else
						Print("[AFHive] map announcement made: level " + AFHM_Announcer.Level(m_AnnouncedBefore).ToString() + " to " + mapNow.ToString());
					Print("[AFHive] areas announced above or below the map: " + told.AnnouncedAreas.Count().ToString());
					if (told.AnnouncedAreas.Count() == 0)
						Print("[AFHive] FAILED: no area announcement for the test site");
					Print("[AFHive] SHOT notifications");
					// Alpha died at close range, so the bots should lean to automatics. The test
					// raises the share so that every Bravo bot swaps and the result is certain.
					Print("[AFHive] style after Alpha: " + AFHiveMind.Get().Style().ToString() + ", role share " + AFHiveMind.Get().RoleShare().ToString() + ", flank x" + AFHiveMind.Get().FlankScale().ToString());
					AFHiveMind.Get().m_Config.RoleShareMax = 5;
					m_Bravo = SpawnPatrol("Bravo", 4, "30 0 50");
					Next();
				}
				break;
			case 2:
				if (m_HiveStepTime >= 6)
				{
					ReportPatrol("Bravo", m_Bravo);
					// Bravo spawned where Alpha was wiped out: the hot zone should make it bigger.
					if (m_Bravo && m_Bravo.m_Group)
					{
						int bravoExtra = AFHiveMind.Get().ExtraBotsFor(m_Bravo.m_Position, 4);
						int bravoCount = m_Bravo.m_Group.Count();
						int bravoWanted = 4 + bravoExtra;
						if (bravoExtra <= 0)
							Print("[AFHive] FAILED: the zone where Alpha died does not count as a hot zone");
						else if (bravoCount != bravoWanted)
							Print("[AFHive] FAILED: Bravo has " + bravoCount.ToString() + " bots, the hot zone should have made it " + bravoWanted.ToString());
						else
							Print("[AFHive] hot zone: Bravo asked for 4 bots and got " + bravoCount.ToString());
					}
					else
					{
						Print("[AFHive] FAILED: Bravo has no group, so the hot zone could not be checked");
					}
					SpawnLoneBots();
					// Now pretend the community snipes: Charlie should get marksman rifles.
					AFHiveMind hmc = AFHiveMind.Get();
					hmc.m_Brain.LongKills = 20;
					hmc.m_Brain.CloseKills = 0;
					hmc.m_Brain.BotsKilledByPlayers = 26;
					Print("[AFHive] style forced to snipers: " + hmc.Style().ToString() + ", role share " + hmc.RoleShare().ToString() + ", flank x" + hmc.FlankScale().ToString());
					m_Charlie = SpawnPatrol("Charlie", 3, "-40 0 60");
					// And pretend most fights happen at night: Delta should get night vision.
					hmc.m_Brain.NightFights = 30;
					hmc.m_Config.NightGearShareMax = 5;
					Print("[AFHive] night share forced: " + hmc.NightShare().ToString() + ", night gear share " + hmc.NightGearShare().ToString());
					m_Delta = SpawnPatrol("Delta", 3, "20 0 70");
					AFHM_ShowPanel(true);
					Next();
				}
				break;
			case 3:
				if (m_HiveStepTime >= 5)
				{
					ReportLoneBots();
					ReportWeapons("Bravo", m_Bravo);
					ReportWeapons("Charlie", m_Charlie);
					ReportDelta();
					// Shoot Delta dead with the bounty certain: each should drop an item, and
					// its goggles should vanish from the body.
					AFHiveMind.Get().m_Config.BountyChanceMax = 5;
					m_BountyBefore = AFHiveMind.Get().m_Brain.TotalBountyDrops;
					ShootPatrol(m_Delta);
					Print("[AFHive] SHOT panel_after_learning");
					m_HiveStep = 200;
					m_HiveStepTime = 0;
				}
				break;
			case 200:
				// The admin controls (F10): open, picture, then the runner presses Esc.
				if (m_HiveStepTime >= 2)
				{
					AFHM_ShowPanel(false);
					Print("[AFHive] admin controls opened by the test: " + AFHM_OpenControls().ToString());
					m_HiveStep = 201;
					m_HiveStepTime = 0;
				}
				break;
			case 201:
				if (m_HiveStepTime >= 4)
				{
					Print("[AFHive] SHOT admin_controls");
					// Ask the runner to drag the window by its top strip, 200 px left and 120 down.
					if (AFHM_ControlsPos(m_DragX, m_DragY))
						Print("[AFHive] PRESS DRAG " + Math.Round(m_DragX + 300).ToString() + " " + Math.Round(m_DragY + 20).ToString() + " -200 120");
					m_HiveStep = 205;
					m_HiveStepTime = 0;
				}
				break;
			case 205:
				if (m_HiveStepTime >= 5)
				{
					float dragX;
					float dragY;
					AFHM_ControlsPos(dragX, dragY);
					Print("[AFHive] window moved from " + m_DragX.ToString() + "," + m_DragY.ToString() + " to " + dragX.ToString() + "," + dragY.ToString());
					if (Math.AbsFloat(dragX - m_DragX) > 50 && Math.AbsFloat(dragY - m_DragY) > 30)
						Print("[AFHive] drag moved the admin controls window");
					else
						Print("[AFHive] drag NOT confirmed (the window did not move; check the picture)");
					Print("[AFHive] SHOT admin_dragged");
					m_HiveStep = 202;
					m_HiveStepTime = 0;
				}
				break;
			case 202:
				if (m_HiveStepTime >= 2)
				{
					Print("[AFHive] PRESS ESC");
					m_HiveStep = 203;
					m_HiveStepTime = 0;
				}
				break;
			case 203:
				if (!AFHM_ControlsOpen() || m_HiveStepTime >= 8)
				{
					UIScriptedMenu after = GetGame().GetUIManager().GetMenu();
					if (AFHM_ControlsOpen())
						Print("[AFHive] FAILED: Esc did not close the admin controls");
					else if (after)
						Print("[AFHive] FAILED: Esc closed the admin controls but opened " + after.ClassName());
					else
						Print("[AFHive] Esc closed the admin controls, and no other menu opened");
					if (after)
						after.Close();
					// Easier/Harder move the threat level by exactly one per click, and clicks in a
					// row are announced once, for where they ended.
					AFHiveMind hmPin = AFHiveMind.Get();
					// Reload config.json: a value changed only in memory goes back to the file's.
					hmPin.m_Config.NotifySeconds = 99;
					hmPin.AdminCommand(null, "reload");
					Print("[AFHive] reload config: NotifySeconds " + hmPin.m_Config.NotifySeconds.ToString());
					if (hmPin.m_Config.NotifySeconds == 99)
						Print("[AFHive] FAILED: Reload config.json did not re-read the file");
					// The wording comes from config.json: a test text must be the one sent.
					hmPin.m_Config.NotifyMapEasier = "TEST easier: {was} to {level}. {changes}";
					// Discord: the runner listens on this local address and reports what arrives.
					hmPin.m_Config.DiscordWebhookUrl = "http://127.0.0.1:8765/webhook";
					// run_test.ps1 -Webhook: post to a real Discord channel instead.
					string realHook = AFHM_Const.DIR + "/test_webhook.txt";
					if (FileExist(realHook))
					{
						FileHandle hookFh = OpenFile(realHook, FileMode.READ);
						string hookUrl;
						if (hookFh)
						{
							FGets(hookFh, hookUrl);
							CloseFile(hookFh);
						}
						hookUrl = hookUrl.Trim();
						if (hookUrl != "")
						{
							hmPin.m_Config.DiscordWebhookUrl = hookUrl;
							Print("[AFHive] discord: posting to the real webhook from the runner");
						}
					}
					AFHM_Discord.s_Posted = 0;
					AFHM_Discord.s_Delivered = 0;
					// Reset one area from the hot-zone list: it must be forgotten.
					m_ZoneReset = "";
					array<string> zoneKeys = hmPin.m_Brain.Zones.GetKeyArray();
					foreach (string zoneKey: zoneKeys)
					{
						if (m_ZoneReset == "" && AFHM_Announcer.AreaGap(hmPin, zoneKey) != 0)
							m_ZoneReset = zoneKey;
					}
					if (m_ZoneReset == "")
					{
						Print("[AFHive] no area above or below the map to reset (run with -Fresh)");
					}
					else
					{
						hmPin.AdminCommand(null, "zreset:" + m_ZoneReset);
						bool stillThere = hmPin.m_Brain.Zones.Contains(m_ZoneReset);
						Print("[AFHive] area reset: zone " + m_ZoneReset + " still remembered: " + stillThere.ToString());
						if (stillThere)
							Print("[AFHive] FAILED: the area reset did not forget the zone");
					}
					m_LevelBefore = AFHM_Announcer.Level(hmPin.m_Brain.Difficulty);
					int toldBefore = AFHM_Announcer.Level(hmPin.m_Brain.AnnouncedDifficulty);
					hmPin.AdminCommand(null, "pin_up");
					int afterUp = AFHM_Announcer.Level(hmPin.m_Brain.Difficulty);
					hmPin.AdminCommand(null, "pin_down");
					hmPin.AdminCommand(null, "pin_down");
					int afterDowns = AFHM_Announcer.Level(hmPin.m_Brain.Difficulty);
					Print("[AFHive] admin clicks: level " + m_LevelBefore.ToString() + ", Harder -> " + afterUp.ToString() + ", Easier twice -> " + afterDowns.ToString() + " (difficulty " + hmPin.m_Brain.Difficulty.ToString() + ")");
					if (afterUp != Math.Min(m_LevelBefore + 1, 10) || afterDowns != Math.Max(m_LevelBefore - 1, 1))
						Print("[AFHive] FAILED: Easier/Harder did not move the level by exactly one");
					if (AFHM_Announcer.Level(hmPin.m_Brain.AnnouncedDifficulty) != toldBefore)
						Print("[AFHive] FAILED: admin clicks were announced at once instead of after the wait");
					m_HiveStep = 204;
					m_HiveStepTime = 0;
				}
				break;
			case 204:
				if (m_HiveStepTime >= 23)
				{
					AFHiveMind hmTold = AFHiveMind.Get();
					int toldNow = AFHM_Announcer.Level(hmTold.m_Brain.AnnouncedDifficulty);
					Print("[AFHive] after the wait, players were told level " + toldNow.ToString() + ": " + AFHM_Announcer.s_LastSent);
					string sentAll = "";
					foreach (string sentOne: AFHM_Announcer.s_Sent)
						sentAll += sentOne + " || ";
					if (sentAll.IndexOf("TEST easier: ") < 0)
						Print("[AFHive] FAILED: the announcement did not use the text from config.json");
					if (m_ZoneReset != "")
					{
						Print("[AFHive] area announcement after the reset went to " + AFHM_Announcer.s_LastRecipients.ToString() + " player(s) near it: " + AFHM_Announcer.s_LastSent);
						if (sentAll.IndexOf("back to the map's level") < 0)
							Print("[AFHive] FAILED: the reset area was not announced as back to the map's level");
						if (AFHM_Announcer.s_LastRecipients != 1)
							Print("[AFHive] FAILED: the area announcement did not reach the one player near it");
					}
					Print("[AFHive] discord: posted " + AFHM_Discord.s_Posted.ToString() + ", delivered " + AFHM_Discord.s_Delivered.ToString());
					if (AFHM_Discord.s_Posted < 3 || AFHM_Discord.s_Delivered < 1)
						Print("[AFHive] FAILED: the Discord webhook posts did not go out");
					AFHM_Config plain = new AFHM_Config();
					hmTold.m_Config.NotifyMapEasier = plain.NotifyMapEasier;
					hmTold.m_Config.DiscordWebhookUrl = "";
					if (toldNow != AFHM_Announcer.Level(hmTold.m_Brain.Difficulty))
						Print("[AFHive] FAILED: the admin clicks were not announced after the wait");
					Print("[AFHive] SHOT admin_notification");
					hmTold.AdminCommand(null, "unpin");
					m_HiveStep = 4;
					m_HiveStepTime = 0;
				}
				break;
			case 4:
				if (m_HiveStepTime >= 3)
				{
					ReportDeltaDead();
					g_HivePlayer.SetAllowDamage(true);
					Print("[AFHive] player can be hurt now; waiting for Bravo to attack");
					Next();
				}
				break;
			case 5:
				if (!g_HivePlayer.IsAlive())
				{
					Print("[AFHive] Bravo killed the player on its own after " + m_HiveStepTime.ToString() + " s");
					Next();
				}
				else if (m_HiveStepTime >= 45)
				{
					Print("[AFHive] Bravo did NOT kill the player on its own in 45 s; killing them with melee damage from a Bravo bot instead");
					BotKillsPlayer(m_Bravo);
					Next();
				}
				break;
			case 6:
				if (m_HiveStepTime >= 3)
				{
					AFHiveMind.Get().Evolve();
					PrintReport("at the end");
					AFHiveMind.Get().Save();
					string backup = AFHM_Const.BACKUP_DIR + "/brain_" + AFHM_Util.Now().Substring(0, 10) + ".json";
					Print("[AFHive] memory last faded at " + AFHiveMind.Get().m_Brain.LastFadeTimestamp.ToString() + " (now " + ExpansionStatic.GetTimestamp(true).ToString() + "), today's backup exists: " + FileExist(backup).ToString());
					Next();
				}
				break;
			case 7:
				if (m_HiveStepTime >= 5)
				{
					Print("[AFHive] SHOT panel_final");
					Next();
				}
				break;
			case 8:
				if (m_HiveStepTime >= 3)
				{
					Print("[AFHive] TOUR DONE");
					Next();
				}
				break;
		}
	}
};

Mission CreateCustomMission(string path)
{
	return new CustomMission();
}
