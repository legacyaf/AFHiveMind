// Dedicated-server test for AF HiveMind, with real bullets. Runs only through
// testmission\run_server_test.ps1 on this PC: DayZDiag as a local server plus one client.
//
// The offline test (AFHive) has to fake every hit. Here nothing is faked:
// 1. The connected player spawns with a loaded rifle. Three passive bots stand in the line of
//    fire, spawned while the HiveMind is at difficulty 0.9 with damage-received scaling
//    switched on, so their multipliers are not 1. The rifle is raised by script and
//    run_server_test.ps1 clicks the mouse. KNOWN LIMIT: the server fires the rounds but none
//    hits, so this step does not show a player's bullet killing a bot. Shoot one yourself.
// 2. The panel is opened while the player is not listed as an admin, then again once listed.
// 3. The player becomes vulnerable and a hostile patrol is left to shoot them. The HiveMind
//    must record the death.

vector g_HivePos = "4660 0 10330";

void main()
{
	// The stock economy and character database. Without them a client logs in but is left on a
	// black screen with its controls locked. run_server_test.ps1 puts Bohemia's stock mission
	// files next to this init.c, with infected and animals switched off.
	Hive hive = CreateHive();
	if (hive)
		hive.InitOffline();
	GetGame().GetWorld().SetDate(2026, 6, 15, 12, 0);
	Weather weather = GetGame().GetWeather();
	weather.GetOvercast().Set(0, 0, 0);
	weather.GetRain().Set(0, 0, 0);
	weather.GetFog().Set(0, 0, 0);
	g_HivePos[1] = GetGame().SurfaceY(g_HivePos[0], g_HivePos[2]);
}

class CustomMission: MissionServer
{
	protected PlayerBase m_Player;
	protected Weapon_Base m_Gun;
	protected int m_Step = -1;
	protected float m_StepTime;
	protected ref array<eAIDynamicPatrol> m_Targets = new array<eAIDynamicPatrol>;
	protected eAIDynamicPatrol m_Raiders;

	override PlayerBase CreateCharacter(PlayerIdentity identity, vector pos, ParamsReadContext ctx, string characterName)
	{
		return super.CreateCharacter(identity, g_HivePos, ctx, characterName);
	}

	// The loaded rifle is part of the character from the start, so the client receives it in
	// its final state. A rifle loaded by script after the client has it only dry-fires there.
	override void StartingEquipSetup(PlayerBase player, bool clothesChosen)
	{
		Weapon_Base gun = Weapon_Base.Cast(player.GetHumanInventory().CreateInHands("M4A1"));
		if (gun)
			gun.SpawnAmmo("Mag_STANAG_60Rnd", Weapon_Base.SAMF_DEFAULT);
	}

	eAIDynamicPatrol SpawnPatrol(string name, int count, string faction, vector pos)
	{
		pos[1] = GetGame().SurfaceY(pos[0], pos[2]);
		TVectorArray way = new TVectorArray;
		way.Insert(pos);
		ExpansionAIPatrol cfg = new ExpansionAIPatrol(count, "WALK", "SPRINT", "HALT", faction, "", true, 0, 1.0, 0, 800, -1, 0, way);
		cfg.Name = name;
		eAIDynamicPatrol patrol = eAIDynamicPatrol.CreateEx(cfg, pos, false);
		if (!patrol)
		{
			Print("[AFHive] FAILED to create patrol " + name);
			return null;
		}
		patrol.Spawn();
		return patrol;
	}

	int Alive(eAIDynamicPatrol patrol)
	{
		if (!patrol || !patrol.m_Group)
			return 0;
		int alive;
		for (int i = 0; i < patrol.m_Group.Count(); i++)
		{
			eAIBase ai = eAIBase.Cast(patrol.m_Group.GetMember(i));
			if (ai && ai.IsAlive())
				alive++;
		}
		return alive;
	}

	void ReportPatrol(string name, eAIDynamicPatrol patrol)
	{
		if (!patrol || !patrol.m_Group)
		{
			Print("[AFHive] patrol " + name + " has no group");
			return;
		}
		for (int i = 0; i < patrol.m_Group.Count(); i++)
		{
			eAIBase ai = eAIBase.Cast(patrol.m_Group.GetMember(i));
			if (ai)
				Print("[AFHive] " + name + " bot: accuracy " + ai.m_eAI_AccuracyMin.ToString() + "-" + ai.m_eAI_AccuracyMax.ToString() + ", damage x" + ai.m_eAI_DamageMultiplier.ToString() + ", takes x" + ai.m_eAI_DamageReceivedMultiplier.ToString() + " | hivemind: " + ai.AFHM_GetProfile());
		}
	}

	void PlayerState(string when)
	{
		Print("[AFHive] player " + when + ": alive " + m_Player.IsAlive().ToString() + ", unconscious " + m_Player.IsUnconscious().ToString() + ", health " + m_Player.GetHealth("", "Health").ToString() + ", blood " + m_Player.GetHealth("", "Blood").ToString() + ", shock " + m_Player.GetHealth("", "Shock").ToString() + ", in hands " + m_Player.GetHumanInventory().GetEntityInHands().ToString() + ", at " + m_Player.GetPosition().ToString());
	}

	void Order(int rpc, bool on)
	{
		if (m_Player && m_Player.GetIdentity())
			GetGame().RPCSingleParam(m_Player, rpc, new Param1<bool>(on), true, m_Player.GetIdentity());
	}

	void PrintReport(string label)
	{
		Print("[AFHive] ---- report " + label + " ----");
		array<string> lines = AFHiveMind.Get().BuildReport(false);
		foreach (string line: lines)
			Print("[AFHive] | " + line);
	}

	void Next()
	{
		m_Step++;
		m_StepTime = 0;
	}

	override void OnUpdate(float timeslice)
	{
		super.OnUpdate(timeslice);
		m_StepTime += timeslice;
		AFHiveMind hm = AFHiveMind.Get();

		if (m_Step == -1)
		{
			array<Man> players = new array<Man>;
			GetGame().GetPlayers(players);
			foreach (Man man: players)
			{
				PlayerBase pb = PlayerBase.Cast(man);
				if (pb && !pb.IsAI() && pb.GetIdentity() && pb.IsAlive())
					m_Player = pb;
			}
			if (m_Player)
			{
				Print("[AFHive] player connected at " + m_Player.GetPosition().ToString());
				m_Player.SetAllowDamage(false);
				Next();
			}
			return;
		}
		if (!m_Player)
			return;

		switch (m_Step)
		{
			case 0:
				if (m_StepTime >= 15)
				{
					m_Gun = Weapon_Base.Cast(m_Player.GetHumanInventory().GetEntityInHands());
					PlayerState("before raising the rifle");

					// Make every multiplier differ from 1, the case the offline test could not judge.
					hm.m_Config.DamageReceivedScaleEasy = 1.15;
					hm.m_Config.DamageReceivedScaleHard = 0.9;
					hm.m_Brain.Difficulty = 0.9;

					// Raise the rifle on both sides: the server ignores shots from a weapon it does
					// not see raised. The aim is held still so recoil cannot walk it off the targets.
					Order(874410101, true);
					m_Player.GetInputController().OverrideRaise(HumanInputControllerOverrideType.ENABLED, true);
					m_Player.GetInputController().OverrideAimChangeY(HumanInputControllerOverrideType.ENABLED, 0);
					Next();
				}
				break;
			case 1:
				if (m_StepTime >= 4 && m_Targets.Count() == 0)
				{
					// The targets stand where the raised rifle points.
					vector at = m_Player.GetPosition();
					vector aim = m_Player.Expansion_GetAimDirection();
					Print("[AFHive] rifle raised: " + m_Player.IsRaised().ToString() + ", aiming along " + aim.ToString());
					aim[1] = 0;
					aim.Normalize();
					m_Targets.Insert(SpawnPatrol("Target1", 1, "Passive", at + aim * 4));
					m_Targets.Insert(SpawnPatrol("Target2", 1, "Passive", at + aim * 6));
					m_Targets.Insert(SpawnPatrol("Target3", 1, "Passive", at + aim * 8));
				}
				if (m_StepTime >= 9)
				{
					foreach (eAIDynamicPatrol target: m_Targets)
						ReportPatrol("Target", target);
					Print("[AFHive] SHOT targets_before_firing");
					Print("[AFHive] FIRE START");
					Next();
				}
				break;
			case 2:
				int standing;
				foreach (eAIDynamicPatrol t: m_Targets)
					standing += Alive(t);
				if (standing == 0 || m_StepTime >= 45)
				{
					Print("[AFHive] FIRE STOP");
					Order(874410101, false);
					m_Player.GetInputController().OverrideRaise(HumanInputControllerOverrideType.DISABLED, false);
					m_Player.GetInputController().OverrideAimChangeY(HumanInputControllerOverrideType.DISABLED, 0);
					PlayerState("after firing");
					Print("[AFHive] rounds left in the magazine: " + m_Gun.GetMagazine(m_Gun.GetCurrentMuzzle()).GetAmmoCount().ToString());
					if (standing == 0)
						Print("[AFHive] all 3 target bots died from the player's bullets after " + m_StepTime.ToString() + " s");
					else
						Print("[AFHive] NOT SHOWN: " + standing.ToString() + " of 3 target bots still alive after 45 s of fire (known limit: the scripted player's bullets do not hit, see the README)");
					Next();
				}
				break;
			case 3:
				if (m_StepTime >= 3)
				{
					Print("[AFHive] player listed as admin: " + hm.IsAdmin(m_Player.GetIdentity()).ToString());
					Order(874410102, true);
					Next();
				}
				break;
			case 4:
				if (m_StepTime >= 6)
				{
					Print("[AFHive] SHOT panel_not_admin");
					Next();
				}
				break;
			case 5:
				if (m_StepTime >= 3)
				{
					hm.m_Config.Admins.Insert(m_Player.GetIdentity().GetPlainId());
					Print("[AFHive] player listed as admin: " + hm.IsAdmin(m_Player.GetIdentity()).ToString());
					Next();
				}
				break;
			case 6:
				if (m_StepTime >= 6)
				{
					Print("[AFHive] SHOT panel_admin");
					Next();
				}
				break;
			case 7:
				if (m_StepTime >= 3)
				{
					Order(874410102, false);
					vector from = m_Player.GetPosition();
					vector ahead = m_Player.GetDirection();
					ahead[1] = 0;
					ahead.Normalize();
					m_Raiders = SpawnPatrol("Raiders", 3, "Raiders", from + ahead * 15);
					Next();
				}
				break;
			case 8:
				if (m_StepTime >= 5)
				{
					ReportPatrol("Raiders", m_Raiders);
					m_Player.SetAllowDamage(true);
					Print("[AFHive] player can be hurt now; waiting for the raiders to shoot");
					Next();
				}
				break;
			case 9:
				if (!m_Player.IsAlive())
				{
					Print("[AFHive] the raiders killed the player on their own after " + m_StepTime.ToString() + " s");
					Next();
				}
				else if (m_StepTime >= 120)
				{
					Print("[AFHive] FAILED: the raiders did not kill the player in 120 s (player health " + m_Player.GetHealth("", "Health").ToString() + ")");
					for (int r = 0; r < m_Raiders.m_Group.Count(); r++)
					{
						eAIBase raider = eAIBase.Cast(m_Raiders.m_Group.GetMember(r));
						if (raider)
							Print("[AFHive] raider " + r.ToString() + ": alive " + raider.IsAlive().ToString() + ", " + vector.Distance(raider.GetPosition(), m_Player.GetPosition()).ToString() + " m away, holding " + raider.GetHumanInventory().GetEntityInHands().ToString() + ", threat " + raider.m_eAI_CurrentThreatToSelfActive.ToString());
					}
					Next();
				}
				break;
			case 10:
				if (m_StepTime >= 3)
				{
					PrintReport("at the end");
					hm.Save();
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
