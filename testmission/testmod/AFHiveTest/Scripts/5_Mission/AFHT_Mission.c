modded class MissionGameplay
{
	protected bool m_AFHT_Holding;
	protected bool m_AFHT_PanelShown;
	protected float m_AFHT_Diag;

	override void OnUpdate(float timeslice)
	{
		super.OnUpdate(timeslice);

		PlayerBase me = PlayerBase.Cast(GetGame().GetPlayer());
		m_AFHT_Diag += timeslice;
		if (m_AFHT_Diag >= 5)
		{
			m_AFHT_Diag = 0;
			string menu = "none";
			if (GetGame().GetUIManager().GetMenu())
				menu = GetGame().GetUIManager().GetMenu().ToString();
			string state = "no player";
			if (me)
			{
				Weapon_Base held = Weapon_Base.Cast(me.GetHumanInventory().GetEntityInHands());
				if (held)
					state = "chamber empty " + held.IsChamberEmpty(held.GetCurrentMuzzle()).ToString() + ", magazine " + held.GetMagazine(held.GetCurrentMuzzle()).ToString() + ", ";
				else
					state = "";
				state += "raised " + me.IsRaised().ToString() + ", in hands " + me.GetHumanInventory().GetEntityInHands().ToString() + ", unconscious " + me.IsUnconscious().ToString() + ", loaded " + me.IsPlayerLoaded().ToString();
			}
			Print("[AFHiveTest] fire " + AFHT_State.s_Fire.ToString() + ", menu " + menu + ", focus " + GetGame().GetInput().HasGameFocus().ToString() + ", player: " + state);
		}
		// Forced key presses (UAInput.ForceEnable) do not reach the character, and a trigger pulled
		// by script (WeaponManager.Fire) is ignored by the game. So the helper only raises the
		// weapon; run_server_test.ps1 clicks the real left mouse button in the game window.
		if (me && AFHT_State.s_Fire)
		{
			m_AFHT_Holding = true;
			me.GetInputController().OverrideRaise(HumanInputControllerOverrideType.ENABLED, true);
			// Keep the aim level: recoil would otherwise walk it up into the sky.
			me.GetInputController().OverrideAimChangeY(HumanInputControllerOverrideType.ENABLED, 0);
		}		else if (me && m_AFHT_Holding)
		{
			m_AFHT_Holding = false;
			me.GetInputController().OverrideRaise(HumanInputControllerOverrideType.DISABLED, false);
			me.GetInputController().OverrideAimChangeY(HumanInputControllerOverrideType.DISABLED, 0);
		}
		if (AFHT_State.s_Panel != m_AFHT_PanelShown)
		{
			m_AFHT_PanelShown = AFHT_State.s_Panel;
			AFHM_ShowPanel(m_AFHT_PanelShown);
		}
	}
};
