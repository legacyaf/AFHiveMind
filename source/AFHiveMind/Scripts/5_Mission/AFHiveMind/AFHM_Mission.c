// Wakes the HiveMind with the mission, saves it when the mission ends, and draws the
// admin debug panel (F9) and the admin controls (F10), both rebindable under
// Controls > AF HiveMind.

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

class AFHM_Client
{
	// Asks the server for a fresh report; it arrives in AFHM_ClientState.
	static void RequestReport()
	{
		Man player = GetGame().GetPlayer();
		if (player)
			GetGame().RPCSingleParam(player, AFHM_Const.RPC_REQUEST, new Param1<bool>(true), true);
	}

	// A button on the controls panel. The server checks the sender is an admin.
	static void SendCommand(string cmd)
	{
		Man player = GetGame().GetPlayer();
		if (!player)
			return;
		ScriptRPC rpc = new ScriptRPC();
		rpc.Write(cmd);
		rpc.Send(player, AFHM_Const.RPC_COMMAND, true, null);
	}
};

// Passes clicks and mouse-overs on the controls' buttons to the menu. A menu is not a widget
// event handler itself, so the buttons need this go-between.
class AFHM_ControlsHandler extends ScriptedWidgetEventHandler
{
	protected AFHM_ControlsMenu m_Menu;

	void AFHM_ControlsHandler(AFHM_ControlsMenu menu)
	{
		m_Menu = menu;
	}

	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (!m_Menu)
			return false;
		return m_Menu.AFHM_Clicked(w);
	}

	override bool OnMouseEnter(Widget w, int x, int y)
	{
		if (!m_Menu)
			return false;
		return m_Menu.AFHM_Highlight(w, true);
	}

	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		if (!m_Menu)
			return false;
		return m_Menu.AFHM_Highlight(w, false);
	}
};

// The admin controls: the same report as the debug panel, with buttons under it. Opens with
// the mouse cursor and the character's controls locked; Esc or Close shuts it.
class AFHM_ControlsMenu extends UIScriptedMenu
{
	protected static const int LINES = 30;
	protected static const int LINE_HEIGHT = 18;
	protected static const int BUTTON_WIDTH = 120;
	protected static const int BUTTON_HEIGHT = 28;
	protected static const int GAP = 8;
	protected static const string PREFIX = "AFHM_cmd_";

	protected ref array<TextWidget> m_Lines = new array<TextWidget>;
	protected ref AFHM_ControlsHandler m_Handler;
	protected TextWidget m_ResetText;
	protected float m_ResetArmed;
	protected float m_Refresh;
	protected float m_Age;
	protected int m_Stamp = -1;

	override Widget Init()
	{
		layoutRoot = GetGame().GetWorkspace().CreateWidgets("AFHiveMind/GUI/layouts/afhm_controls.layout");
		if (!layoutRoot)
		{
			Print("[AFHiveMind] admin controls: could not load afhm_controls.layout");
			return null;
		}
		m_Handler = new AFHM_ControlsHandler(this);
		for (int i = 0; i < LINES; i++)
		{
			TextWidget line = TextWidget.Cast(GetGame().GetWorkspace().CreateWidgets("AFHiveMind/GUI/layouts/afhm_line.layout", layoutRoot));
			if (!line)
				continue;
			line.SetPos(12, 10 + i * LINE_HEIGHT);
			m_Lines.Insert(line);
		}

		int row1 = 20 + LINES * LINE_HEIGHT;
		int row2 = row1 + BUTTON_HEIGHT + GAP;
		AddButton("Freeze learning", "freeze", 0, row1);
		AddButton("Resume learning", "resume", 1, row1);
		AddButton("Evolve now", "evolve", 2, row1);
		m_ResetText = AddButton("Reset brain", "reset", 3, row1);
		AddButton("Close", "close", 4, row1);
		AddButton("Pin difficulty", "pin", 0, row2);
		AddButton("Pin -0.05", "pin_down", 1, row2);
		AddButton("Pin +0.05", "pin_up", 2, row2);
		AddButton("Unpin", "unpin", 3, row2);
		layoutRoot.SetSize(660, row2 + BUTTON_HEIGHT + 12);

		array<string> waiting = new array<string>;
		waiting.Insert("AF HiveMind: asking the server...");
		SetRows(waiting);
		return layoutRoot;
	}

	// Returns the button's label so it can be changed later.
	protected TextWidget AddButton(string label, string cmd, int column, int top)
	{
		Widget button = GetGame().GetWorkspace().CreateWidgets("AFHiveMind/GUI/layouts/afhm_button.layout", layoutRoot);
		if (!button)
			return null;
		button.SetName(PREFIX + cmd);
		button.SetPos(12 + column * (BUTTON_WIDTH + GAP), top);
		button.SetHandler(m_Handler);
		TextWidget text = TextWidget.Cast(button.FindAnyWidget("AFHM_ButtonText"));
		if (text)
			text.SetText(label);
		return text;
	}

	protected void SetRows(array<string> rows)
	{
		for (int i = 0; i < m_Lines.Count(); i++)
		{
			string row = "";
			if (rows && i < rows.Count())
				row = rows[i];
			m_Lines[i].SetText(row);
		}
	}

	protected void Disarm()
	{
		m_ResetArmed = 0;
		if (m_ResetText)
			m_ResetText.SetText("Reset brain");
	}

	override void OnShow()
	{
		super.OnShow();
		GetGame().GetInput().ChangeGameFocus(1);
		GetGame().GetUIManager().ShowUICursor(true);
		GetGame().GetMission().PlayerControlDisable(INPUT_EXCLUDE_ALL);
	}

	override void OnHide()
	{
		super.OnHide();
		GetGame().GetInput().ResetGameFocus();
		GetGame().GetUIManager().ShowUICursor(false);
		GetGame().GetMission().PlayerControlEnable(false);
	}

	bool AFHM_Clicked(Widget w)
	{
		if (!w)
			return false;
		string name = w.GetName();
		if (name.IndexOf(PREFIX) != 0)
			return false;
		string cmd = name.Substring(PREFIX.Length(), name.Length() - PREFIX.Length());

		if (cmd == "close")
		{
			Close();
			return true;
		}
		// Reset needs a second click within five seconds.
		if (cmd == "reset" && m_ResetArmed <= 0)
		{
			m_ResetArmed = 5;
			if (m_ResetText)
				m_ResetText.SetText("Click to confirm");
			return true;
		}
		Disarm();
		AFHM_Client.SendCommand(cmd);
		return true;
	}

	bool AFHM_Highlight(Widget w, bool on)
	{
		if (!w || w.GetName().IndexOf(PREFIX) != 0)
			return false;
		Widget bg = w.FindAnyWidget("AFHM_ButtonBg");
		if (!bg)
			return false;
		if (on)
			bg.SetColor(ARGB(255, 90, 90, 90));
		else
			bg.SetColor(ARGB(255, 51, 51, 51));
		return true;
	}

	override void Update(float timeslice)
	{
		super.Update(timeslice);
		m_Age += timeslice;
		// Skip the first moments, so the key press that opened the menu cannot close it.
		if (m_Age > 0.3 && GetUApi().GetInputByID(UAUIBack).LocalPress())
		{
			Close();
			return;
		}

		if (m_ResetArmed > 0)
		{
			m_ResetArmed -= timeslice;
			if (m_ResetArmed <= 0)
				Disarm();
		}

		m_Refresh -= timeslice;
		if (m_Refresh <= 0)
		{
			m_Refresh = 3;
			AFHM_Client.RequestReport();
		}
		if (m_Stamp != AFHM_ClientState.s_Stamp)
		{
			m_Stamp = AFHM_ClientState.s_Stamp;
			SetRows(AFHM_ClientState.s_Report);
		}
	}
};

modded class MissionGameplay
{
	protected ref AFHM_Panel m_AFHM_Panel;
	// Held here: a menu made with new and handed to the UI manager is otherwise deleted at once.
	protected ref AFHM_ControlsMenu m_AFHM_Controls;
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

		UAInput controls = GetUApi().GetInputByName("UAAFHiveMindControls");
		if (controls && controls.LocalPress())
		{
			UIScriptedMenu open = GetGame().GetUIManager().GetMenu();
			if (open)
			{
				Print("[AFHiveMind] F10: another menu is open (" + open.ClassName() + "), not opening the admin controls");
			}
			else
			{
				AFHM_ShowPanel(false);
				m_AFHM_Controls = new AFHM_ControlsMenu();
				GetGame().GetUIManager().ShowScriptedMenu(m_AFHM_Controls, null);
				Print("[AFHiveMind] F10: admin controls opened");
			}
		}

		if (!m_AFHM_Panel)
			return;

		// While the panel is open, ask the server for a fresh report every three seconds.
		m_AFHM_Refresh -= timeslice;
		if (m_AFHM_Refresh <= 0)
		{
			m_AFHM_Refresh = 3;
			AFHM_Client.RequestReport();
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
