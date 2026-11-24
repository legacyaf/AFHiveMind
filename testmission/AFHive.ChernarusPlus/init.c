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
	}	protected eAIBase m_Follower;

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
					AFHiveMind.Get().Evolve();
					PrintReport("after the player wiped out Alpha");
					// Alpha died at close range, so the bots should lean to automatics. The test
					// raises the share so that every Bravo bot swaps and the result is certain.
					Print("[AFHive] style after Alpha: " + AFHiveMind.Get().Style().ToString() + ", role share " + AFHiveMind.Get().RoleShare().ToString() + ", flank x" + AFHiveMind.Get().FlankScale().ToString());
					AFHiveMind.Get().m_Config.RoleShareMax = 5;					m_Bravo = SpawnPatrol("Bravo", 4, "30 0 50");
					Next();
				}
				break;
			case 2:
				if (m_HiveStepTime >= 6)
				{
					ReportPatrol("Bravo", m_Bravo);
					SpawnLoneBots();
					// Now pretend the community snipes: Charlie should get marksman rifles.
					AFHiveMind hmc = AFHiveMind.Get();
					hmc.m_Brain.LongKills = 20;
					hmc.m_Brain.CloseKills = 0;
					hmc.m_Brain.BotsKilledByPlayers = 26;
					Print("[AFHive] style forced to snipers: " + hmc.Style().ToString() + ", role share " + hmc.RoleShare().ToString() + ", flank x" + hmc.FlankScale().ToString());
					m_Charlie = SpawnPatrol("Charlie", 3, "-40 0 60");					AFHM_ShowPanel(true);
					Next();
				}
				break;
			case 3:
				if (m_HiveStepTime >= 5)
				{
					ReportLoneBots();
					ReportWeapons("Bravo", m_Bravo);
					ReportWeapons("Charlie", m_Charlie);
					Print("[AFHive] SHOT panel_after_learning");
					Next();
				}
				break;
			case 4:
				if (m_HiveStepTime >= 3)
				{
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
