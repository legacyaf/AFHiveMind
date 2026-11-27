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

// The F9 panel: the report as see-through bubbles on the right of the screen. It never takes the
// mouse or the keyboard.
class AFHM_Panel
{
	protected static const float WIDTH = 540;
	protected Widget m_Root;
	protected ref AFHM_ReportView m_View;

	void ~AFHM_Panel()
	{
		m_View = null;
		if (m_Root)
			m_Root.Unlink();
	}

	void SetRows(array<string> items)
	{
		m_View = null;
		if (m_Root)
			m_Root.Unlink();
		int screenW;
		int screenH;
		GetScreenSize(screenW, screenH);
		// Stops above the game's status icons at the bottom of the screen.
		float maxH = Math.Max(screenH - 180, 300);

		// On the right: Expansion shows its notifications in the top-left corner.
		m_Root = AFHM_Ui.Frame(null, Math.Max(screenW - WIDTH - 20, 0), 80, WIDTH, maxH);
		if (!m_Root)
			return;
		m_View = new AFHM_ReportView();
		float h = m_View.Layout(m_Root, items, WIDTH, maxH, false);
		// The window first, so the report is drawn on top of it.
		new AFHM_Bubble(m_Root, 0, 0, WIDTH, h, 22, AFHM_Ui.Rgba(22, 22, 24, 0.62));
		m_View.Layout(m_Root, items, WIDTH, maxH, true);
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

// The admin controls (F10): the report on the left, the buttons on the right, in one rounded
// see-through window in the middle of the screen. Esc, or Close, shuts it.
class AFHM_ControlsMenu extends UIScriptedMenu
{
	protected static const float REPORT_W = 540;
	protected static const float SIDE_W = 250;
	protected static const float MARGIN = 16;
	protected static const string PREFIX = "AFHM_cmd_";
	// Its own menu id, so the UI manager does not mistake it for another menu.
	static const int MENU_ID = 874410010;

	protected ref AFHM_ControlsHandler m_Handler;
	protected ref map<string, ref AFHM_Button> m_Buttons = new map<string, ref AFHM_Button>;
	protected ref AFHM_ReportView m_View;
	protected ref AFHM_Bubble m_Window;
	protected Widget m_ReportHolder;
	protected TextWidget m_Note;
	protected float m_NoteAge = 99;
	protected float m_Height;
	protected float m_ResetArmed;
	protected float m_Refresh;
	protected float m_Age;
	protected bool m_EscWasDown;
	protected int m_Stamp = -1;

	override int GetID()
	{
		return MENU_ID;
	}

	override Widget Init()
	{
		int screenW;
		int screenH;
		GetScreenSize(screenW, screenH);
		float width = MARGIN + REPORT_W + MARGIN + SIDE_W + MARGIN;
		m_Height = Math.Min(screenH - 60, 860);

		layoutRoot = AFHM_Ui.Frame(null, Math.Max((screenW - width) / 2, 0), Math.Max((screenH - m_Height) / 2, 0), width, m_Height);
		if (!layoutRoot)
		{
			Print("[AFHiveMind] admin controls: could not create the window");
			return null;
		}
		m_Handler = new AFHM_ControlsHandler(this);

		// Back to front: the window, the report's place, the side panel, the buttons.
		m_Window = new AFHM_Bubble(layoutRoot, 0, 0, width, m_Height, 26, AFHM_Ui.Rgba(22, 22, 24, 0.72));
		m_ReportHolder = AFHM_Ui.Frame(layoutRoot, MARGIN, 0, REPORT_W, m_Height);
		float sx = MARGIN + REPORT_W + MARGIN;
		new AFHM_Bubble(layoutRoot, sx, MARGIN, SIDE_W, m_Height - 2 * MARGIN, 20, AFHM_Ui.Rgba(44, 44, 46, 0.62));

		int label = ARGB(255, 142, 142, 147);
		float bx = sx + 14;
		float bw = SIDE_W - 28;
		float y = MARGIN + 14;
		AFHM_Ui.Text(layoutRoot, "Controls", bx, y, bw, 28, 22, ARGB(255, 255, 255, 255));
		y += 40;

		AFHM_Ui.Text(layoutRoot, "LEARNING", bx + 6, y, bw, 18, 12, label);
		y += 22;
		AddButton("freeze", "Freeze learning", bx, y, bw, "grey");
		y += 42;
		AddButton("resume", "Resume learning", bx, y, bw, "grey");
		y += 42;
		AddButton("evolve", "Run a learning step now", bx, y, bw, "blue");
		y += 54;

		AFHM_Ui.Text(layoutRoot, "DIFFICULTY", bx + 6, y, bw, 18, 12, label);
		y += 22;
		AddButton("pin", "Pin the current level", bx, y, bw, "blue");
		y += 42;
		float half = (bw - 8) / 2;
		AddButton("pin_down", "Easier  -0.05", bx, y, half, "grey");
		AddButton("pin_up", "Harder  +0.05", bx + half + 8, y, half, "grey");
		y += 42;
		AddButton("unpin", "Unpin (learn again)", bx, y, bw, "grey");
		y += 54;

		AFHM_Ui.Text(layoutRoot, "DANGER ZONE", bx + 6, y, bw, 18, 12, label);
		y += 22;
		AddButton("reset", "Reset brain", bx, y, bw, "red");
		y += 50;

		m_Note = AFHM_Ui.Text(layoutRoot, "", bx, y, bw, 40, 13, ARGB(255, 174, 174, 178), 1);

		AddButton("close", "Close   (Esc)", bx, m_Height - MARGIN - 14 - 34, bw, "grey");

		// Show the last report at once, if there is one, while a fresh one is asked for.
		m_View = new AFHM_ReportView();
		if (AFHM_ClientState.s_Report.Count() > 0)
		{
			ShowReport(AFHM_ClientState.s_Report);
		}
		else
		{
			array<string> waiting = new array<string>;
			waiting.Insert("H|AF HiveMind");
			waiting.Insert("I|Asking the server...");
			ShowReport(waiting);
		}
		m_Stamp = AFHM_ClientState.s_Stamp;
		AFHM_Client.RequestReport();
		m_Refresh = 3;

		// Fade in.
		layoutRoot.SetAlpha(0);
		return layoutRoot;
	}

	protected void AddButton(string cmd, string label, float x, float y, float w, string style)
	{
		int r = 72;
		int g = 72;
		int b = 74;
		float a = 0.9;
		if (style == "blue")
		{
			r = 10;
			g = 132;
			b = 255;
		}
		else if (style == "red")
		{
			r = 255;
			g = 69;
			b = 58;
		}
		m_Buttons.Insert(cmd, new AFHM_Button(layoutRoot, PREFIX + cmd, label, x, y, w, 34, r, g, b, a, m_Handler));
	}

	protected void ShowReport(array<string> items)
	{
		if (m_View && m_ReportHolder)
			m_View.Layout(m_ReportHolder, items, REPORT_W, m_Height, true);
	}

	protected void Note(string text)
	{
		if (!m_Note)
			return;
		m_Note.SetText(text);
		m_NoteAge = 0;
		m_Note.SetAlpha(1);
	}

	protected void Disarm()
	{
		m_ResetArmed = 0;
		AFHM_Button reset = m_Buttons.Get("reset");
		if (reset)
			reset.SetLabel("Reset brain");
	}

	override void OnShow()
	{
		super.OnShow();
		Print("[AFHiveMind] admin controls shown");
		GetGame().GetInput().ChangeGameFocus(1);
		GetGame().GetUIManager().ShowUICursor(true);
		GetGame().GetMission().PlayerControlDisable(INPUT_EXCLUDE_ALL);
	}

	override void OnHide()
	{
		super.OnHide();
		Print("[AFHiveMind] admin controls hidden");
		GetGame().GetInput().ChangeGameFocus(-1);
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
		AFHM_Button button = m_Buttons.Get(cmd);
		if (button)
			button.Flash();

		if (cmd == "close")
		{
			Close();
			return true;
		}
		// Reset needs a second click within five seconds.
		if (cmd == "reset" && m_ResetArmed <= 0)
		{
			m_ResetArmed = 5;
			if (button)
				button.SetLabel("Click again to reset");
			Note("Reset wipes what it learned. The old brain is kept as a backup.");
			return true;
		}
		Disarm();
		AFHM_Client.SendCommand(cmd);
		Note(Describe(cmd));
		return true;
	}

	protected static string Describe(string cmd)
	{
		if (cmd == "freeze")
			return "Learning frozen.";
		if (cmd == "resume")
			return "Learning resumed.";
		if (cmd == "evolve")
			return "Learning step done.";
		if (cmd == "pin")
			return "Difficulty pinned.";
		if (cmd == "pin_down")
			return "Pinned 0.05 easier.";
		if (cmd == "pin_up")
			return "Pinned 0.05 harder.";
		if (cmd == "unpin")
			return "Unpinned: it learns again.";
		if (cmd == "reset")
			return "Brain reset.";
		return "Sent.";
	}

	bool AFHM_Highlight(Widget w, bool on)
	{
		if (!w || w.GetName().IndexOf(PREFIX) != 0)
			return false;
		string name = w.GetName();
		AFHM_Button button = m_Buttons.Get(name.Substring(PREFIX.Length(), name.Length() - PREFIX.Length()));
		if (!button)
			return false;
		button.SetHover(on);
		return true;
	}

	override void Update(float timeslice)
	{
		super.Update(timeslice);
		m_Age += timeslice;

		if (layoutRoot && m_Age < 0.5)
			layoutRoot.SetAlpha(Math.Clamp(m_Age / 0.18, 0, 1));

		// Esc closes it. Read the key itself: the game's own "back" input is blocked while the
		// menu holds the controls. The first moments are skipped, so the key that opened the
		// menu cannot close it, and the pause menu is kept from opening on the same press.
		bool escDown = KeyState(KeyCode.KC_ESCAPE) != 0;
		bool escPressed = escDown && !m_EscWasDown;
		m_EscWasDown = escDown;
		if (m_Age > 0.3 && (escPressed || GetUApi().GetInputByID(UAUIBack).LocalPress()))
		{
			ClearKey(KeyCode.KC_ESCAPE);
			GetUApi().GetInputByID(UAUIMenu).Supress();
			GetUApi().GetInputByID(UAUIBack).Supress();
			if (GetGame().GetUIManager().GetMenu() == this)
				Close();
			return;
		}

		foreach (string cmd, AFHM_Button button: m_Buttons)
			button.Update(timeslice);

		if (m_ResetArmed > 0)
		{
			m_ResetArmed -= timeslice;
			if (m_ResetArmed <= 0)
				Disarm();
		}

		if (m_Note && m_NoteAge < 99)
		{
			m_NoteAge += timeslice;
			if (m_NoteAge > 3)
				m_Note.SetAlpha(Math.Clamp(1 - (m_NoteAge - 3) / 0.6, 0, 1));
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
			ShowReport(AFHM_ClientState.s_Report);
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

	// Opens the admin controls unless another menu is open. Returns whether it opened.
	bool AFHM_OpenControls()
	{
		UIScriptedMenu open = GetGame().GetUIManager().GetMenu();
		if (open)
		{
			Print("[AFHiveMind] F10: another menu is open (" + open.ClassName() + "), not opening the admin controls");
			return false;
		}
		AFHM_ShowPanel(false);
		m_AFHM_Controls = new AFHM_ControlsMenu();
		GetGame().GetUIManager().ShowScriptedMenu(m_AFHM_Controls, null);
		Print("[AFHiveMind] F10: admin controls opened");
		return true;
	}

	bool AFHM_ControlsOpen()
	{
		return m_AFHM_Controls && GetGame().GetUIManager().GetMenu() == m_AFHM_Controls;
	}

	void AFHM_ShowPanel(bool show)
	{
		if (show && !m_AFHM_Panel)
		{
			m_AFHM_Panel = new AFHM_Panel();
			array<string> waiting = new array<string>;
			waiting.Insert("H|AF HiveMind");
			waiting.Insert("I|Asking the server...");
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
			Print("[AFHiveMind] F10 pressed");
			AFHM_OpenControls();
		}

		// The F9 panel stays closed while the admin controls are open.
		if (m_AFHM_Panel && m_AFHM_Controls && GetGame().GetUIManager().GetMenu() == m_AFHM_Controls)
			AFHM_ShowPanel(false);

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
		// Close the controls first, so the cursor and the character's controls are restored.
		if (m_AFHM_Controls && GetGame().GetUIManager().GetMenu() == m_AFHM_Controls)
			m_AFHM_Controls.Close();
		m_AFHM_Controls = null;
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
