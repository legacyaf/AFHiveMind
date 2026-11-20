// Wakes the HiveMind with the mission, saves it when the mission ends, and draws the
// admin debug panel (F9 by default, rebindable under Controls > AF HiveMind).

modded class MissionServer
{
	override void OnInit()
	{
		super.OnInit();
		// Wake it now so it is ready before the first patrol spawns.
		AFHiveMind.Get();
	}

	override void OnMissionFinish()
	{
		AFHiveMind hm = AFHiveMind.Get();
		if (hm)
			hm.Save();
		super.OnMissionFinish();
	}
};

class AFHM_Panel
{
	protected static const int LINES = 34;
	protected static const int LINE_HEIGHT = 18;

	protected Widget m_Root;
	protected ref array<TextWidget> m_Lines = new array<TextWidget>;

	void AFHM_Panel()
	{
		m_Root = GetGame().GetWorkspace().CreateWidgets("AFHiveMind/GUI/layouts/afhm_panel.layout");
		if (!m_Root)
			return;
		for (int i = 0; i < LINES; i++)
		{
			TextWidget line = TextWidget.Cast(GetGame().GetWorkspace().CreateWidgets("AFHiveMind/GUI/layouts/afhm_line.layout", m_Root));
			if (!line)
				continue;
			line.SetPos(12, 10 + i * LINE_HEIGHT);
			m_Lines.Insert(line);
		}
		m_Root.SetSize(660, 20 + LINES * LINE_HEIGHT);
	}

	void ~AFHM_Panel()
	{
		if (m_Root)
			m_Root.Unlink();
	}

	void SetRows(array<string> rows)
	{
		for (int i = 0; i < m_Lines.Count(); i++)
		{
			string row = "";
			if (rows && i < rows.Count())
				row = rows[i];
			m_Lines[i].SetText(row);
		}
	}
};

modded class MissionGameplay
{
	protected ref AFHM_Panel m_AFHM_Panel;
	protected float m_AFHM_Refresh;
	protected int m_AFHM_Stamp = -1;

	void AFHM_ShowPanel(bool show)
	{
		if (show && !m_AFHM_Panel)
		{
			m_AFHM_Panel = new AFHM_Panel();
			array<string> waiting = new array<string>;
			waiting.Insert("AF HiveMind: asking the server...");
			m_AFHM_Panel.SetRows(waiting);
			m_AFHM_Refresh = 0;
			m_AFHM_Stamp = AFHM_ClientState.s_Stamp;
		}
		else if (!show && m_AFHM_Panel)
		{
			m_AFHM_Panel = null;
		}
	}

	override void OnUpdate(float timeslice)
	{
		super.OnUpdate(timeslice);

		UAInput toggle = GetUApi().GetInputByName("UAAFHiveMindDebug");
		if (toggle && toggle.LocalPress() && !GetGame().GetUIManager().GetMenu())
			AFHM_ShowPanel(!m_AFHM_Panel);

		if (!m_AFHM_Panel)
			return;

		// While the panel is open, ask the server for a fresh report every three seconds.
		m_AFHM_Refresh -= timeslice;
		if (m_AFHM_Refresh <= 0)
		{
			m_AFHM_Refresh = 3;
			Man player = GetGame().GetPlayer();
			if (player)
				GetGame().RPCSingleParam(player, AFHM_Const.RPC_REQUEST, new Param1<bool>(true), true);
		}

		if (m_AFHM_Stamp != AFHM_ClientState.s_Stamp)
		{
			m_AFHM_Stamp = AFHM_ClientState.s_Stamp;
			m_AFHM_Panel.SetRows(AFHM_ClientState.s_Report);
		}
	}

	override void OnMissionFinish()
	{
		m_AFHM_Panel = null;
		// Offline single player has no MissionServer: save here.
		if (GetGame().IsServer())
		{
			AFHiveMind hm = AFHiveMind.Get();
			if (hm)
				hm.Save();
		}
		super.OnMissionFinish();
	}
};
