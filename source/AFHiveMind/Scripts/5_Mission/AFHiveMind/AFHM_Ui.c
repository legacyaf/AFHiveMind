// Building blocks for the F9 panel and the F10 admin controls: rounded, see-through "bubbles"
// in the style of a chat app, smooth buttons, and a report view that lays the server's report
// out in sections (see AFHiveMind.PanelItems for the item format).

class AFHM_Ui
{
	static const string LAYOUTS = "AFHiveMind/GUI/layouts/";
	static const string TEXTURES = "AFHiveMind/GUI/textures/";

	// Colours, in the spirit of a chat app's dark mode.
	static int Rgba(int r, int g, int b, float a)
	{
		int alpha = Math.Round(Math.Clamp(a, 0, 1) * 255);
		return ARGB(alpha, r, g, b);
	}

	static Widget Frame(Widget parent, float x, float y, float w, float h)
	{
		Widget f = GetGame().GetWorkspace().CreateWidgets(AFHM_Ui.LAYOUTS + "afhm_frame.layout", parent);
		if (!f)
			return null;
		f.SetPos(x, y);
		f.SetSize(w, h);
		return f;
	}

	static Widget Rect(Widget parent, float x, float y, float w, float h, int color)
	{
		if (w <= 0 || h <= 0)
			return null;
		Widget r = GetGame().GetWorkspace().CreateWidgets(AFHM_Ui.LAYOUTS + "afhm_rect.layout", parent);
		if (!r)
			return null;
		r.SetPos(x, y);
		r.SetSize(w, h);
		r.SetColor(color);
		return r;
	}

	static Widget Corner(Widget parent, string which, float x, float y, float size, int color)
	{
		if (size <= 0)
			return null;
		ImageWidget img = ImageWidget.Cast(GetGame().GetWorkspace().CreateWidgets(AFHM_Ui.LAYOUTS + "afhm_corner.layout", parent));
		if (!img)
			return null;
		img.LoadImageFile(0, AFHM_Ui.TEXTURES + "afhm_corner_" + which + ".paa");
		img.SetImage(0);
		img.SetPos(x, y);
		img.SetSize(size, size);
		img.SetColor(color);
		return img;
	}

	// align: 0 left, 1 centre, 2 right.
	static TextWidget Text(Widget parent, string text, float x, float y, float w, float h, int size, int color, int align = 0)
	{
		string layout = AFHM_Ui.LAYOUTS + "afhm_text.layout";
		if (align == 1)
			layout = AFHM_Ui.LAYOUTS + "afhm_text_center.layout";
		else if (align == 2)
			layout = AFHM_Ui.LAYOUTS + "afhm_text_right.layout";
		TextWidget t = TextWidget.Cast(GetGame().GetWorkspace().CreateWidgets(layout, parent));
		if (!t)
			return null;
		t.SetTextExactSize(size);
		t.SetText(text);
		t.SetPos(x, y);
		t.SetSize(w, h);
		t.SetColor(color);
		return t;
	}

	// How wide a line of text is, roughly, in the game's Metron font.
	static float TextWidth(string text, int size)
	{
		return text.Length() * size * 0.53;
	}

	static int CharsFor(float width, int size)
	{
		int chars = Math.Floor(width / (size * 0.53));
		return Math.Max(chars, 8);
	}

	// Breaks text into lines of at most maxChars, at spaces where it can.
	static array<string> Wrap(string text, int maxChars)
	{
		array<string> lines = new array<string>;
		string rest = text;
		while (rest.Length() > maxChars)
		{
			int cut = maxChars;
			while (cut > maxChars / 2 && rest.Get(cut) != " ")
				cut--;
			if (rest.Get(cut) == " ")
			{
				lines.Insert(rest.Substring(0, cut));
				rest = rest.Substring(cut + 1, rest.Length() - cut - 1);
			}
			else
			{
				lines.Insert(rest.Substring(0, maxChars));
				rest = rest.Substring(maxChars, rest.Length() - maxChars);
			}
		}
		if (rest.Length() > 0 || lines.Count() == 0)
			lines.Insert(rest);
		return lines;
	}
};

// A rounded rectangle from seven pieces that never overlap, so its transparency stays even:
// a full-height middle, a strip down each side, and a quarter circle in each corner.
class AFHM_Bubble
{
	protected ref array<Widget> m_Pieces = new array<Widget>;

	void AFHM_Bubble(Widget parent, float x, float y, float w, float h, float radius, int color)
	{
		float r = Math.Floor(Math.Min(radius, Math.Min(w, h) / 2));
		m_Pieces.Insert(AFHM_Ui.Rect(parent, x + r, y, w - 2 * r, h, color));
		m_Pieces.Insert(AFHM_Ui.Rect(parent, x, y + r, r, h - 2 * r, color));
		m_Pieces.Insert(AFHM_Ui.Rect(parent, x + w - r, y + r, r, h - 2 * r, color));
		m_Pieces.Insert(AFHM_Ui.Corner(parent, "tl", x, y, r, color));
		m_Pieces.Insert(AFHM_Ui.Corner(parent, "tr", x + w - r, y, r, color));
		m_Pieces.Insert(AFHM_Ui.Corner(parent, "bl", x, y + h - r, r, color));
		m_Pieces.Insert(AFHM_Ui.Corner(parent, "br", x + w - r, y + h - r, r, color));
	}

	void SetColor(int color)
	{
		foreach (Widget piece: m_Pieces)
		{
			if (piece)
				piece.SetColor(color);
		}
	}
};

// A pill-shaped button whose colour glides towards its hover colour, and flashes on a click.
class AFHM_Button
{
	Widget m_Hit;
	protected ref AFHM_Bubble m_Bubble;
	protected TextWidget m_Text;
	protected float m_BaseR;
	protected float m_BaseG;
	protected float m_BaseB;
	protected float m_BaseA;
	protected float m_R;
	protected float m_G;
	protected float m_B;
	protected float m_A;
	protected bool m_Hover;
	protected float m_Flash;

	void AFHM_Button(Widget parent, string name, string label, float x, float y, float w, float h, int r, int g, int b, float a, ScriptedWidgetEventHandler handler)
	{
		m_BaseR = r;
		m_BaseG = g;
		m_BaseB = b;
		m_BaseA = a;
		m_R = r;
		m_G = g;
		m_B = b;
		m_A = a;
		m_Hit = GetGame().GetWorkspace().CreateWidgets(AFHM_Ui.LAYOUTS + "afhm_hit.layout", parent);
		if (!m_Hit)
			return;
		m_Hit.SetName(name);
		m_Hit.SetPos(x, y);
		m_Hit.SetSize(w, h);
		m_Hit.SetHandler(handler);
		m_Bubble = new AFHM_Bubble(m_Hit, 0, 0, w, h, h / 2, AFHM_Ui.Rgba(r, g, b, a));
		m_Text = AFHM_Ui.Text(m_Hit, label, 0, 0, w, h, 15, ARGB(255, 255, 255, 255), 1);
	}

	void SetLabel(string label)
	{
		if (m_Text)
			m_Text.SetText(label);
	}

	void SetHover(bool on)
	{
		m_Hover = on;
	}

	void Flash()
	{
		m_Flash = 1;
	}

	void Update(float dt)
	{
		float tr = m_BaseR;
		float tg = m_BaseG;
		float tb = m_BaseB;
		float ta = m_BaseA;
		if (m_Hover)
		{
			tr = Math.Min(tr + 30, 255);
			tg = Math.Min(tg + 30, 255);
			tb = Math.Min(tb + 30, 255);
			ta = Math.Min(ta + 0.12, 1);
		}
		if (m_Flash > 0)
		{
			tr = Math.Lerp(tr, 255, m_Flash * 0.6);
			tg = Math.Lerp(tg, 255, m_Flash * 0.6);
			tb = Math.Lerp(tb, 255, m_Flash * 0.6);
			m_Flash = Math.Max(m_Flash - dt * 3, 0);
		}
		float k = Math.Clamp(dt * 14, 0, 1);
		m_R = Math.Lerp(m_R, tr, k);
		m_G = Math.Lerp(m_G, tg, k);
		m_B = Math.Lerp(m_B, tb, k);
		m_A = Math.Lerp(m_A, ta, k);
		if (m_Bubble)
		{
			int ir = Math.Round(m_R);
			int ig = Math.Round(m_G);
			int ib = Math.Round(m_B);
			m_Bubble.SetColor(AFHM_Ui.Rgba(ir, ig, ib, m_A));
		}
	}
};

// The report, laid out like a chat: a header, a blue threat-level card with status chips, a
// grey card of key facts, then a section per topic with each finding in its own grey bubble,
// and the latest patrols as blue bubbles on the right.
class AFHM_ReportView
{
	protected Widget m_Frame;
	protected static const int PAD = 14;

	void ~AFHM_ReportView()
	{
		Clear();
	}

	void Clear()
	{
		if (m_Frame)
			m_Frame.Unlink();
		m_Frame = null;
	}

	// Lays the report out in width x maxHeight. With draw false it only measures. Returns the
	// height used. Whatever does not fit is left out, with a note where to read the rest.
	float Layout(Widget parent, array<string> items, float width, float maxHeight, bool draw)
	{
		if (draw)
		{
			Clear();
			m_Frame = AFHM_Ui.Frame(parent, 0, 0, width, maxHeight);
		}
		Widget f = m_Frame;
		int white = ARGB(255, 255, 255, 255);
		int dim = ARGB(255, 174, 174, 178);
		int label = ARGB(255, 142, 142, 147);
		int grey = AFHM_Ui.Rgba(58, 58, 60, 0.78);
		int blue = AFHM_Ui.Rgba(10, 132, 255, 0.88);

		// Sort the items.
		string title = "AF HiveMind";
		string version;
		int level = 0;
		array<string> chips = new array<string>;
		array<string> keys = new array<string>;
		array<string> body = new array<string>;
		foreach (string raw: items)
		{
			int bar = raw.IndexOf("|");
			string tag = "I";
			string rest = raw;
			if (bar > 0 && bar <= 2)
			{
				tag = raw.Substring(0, bar);
				rest = raw.Substring(bar + 1, raw.Length() - bar - 1);
			}
			if (tag == "H")
				title = rest;
			else if (tag == "V")
				version = rest;
			else if (tag == "L")
				level = rest.ToInt();
			else if (tag == "C")
				chips.Insert(rest);
			else if (tag == "K")
				keys.Insert(rest);
			else
				body.Insert(tag + "|" + rest);
		}

		float y = PAD;
		float limit = maxHeight - PAD;

		// Header.
		if (draw)
		{
			AFHM_Ui.Text(f, title, PAD, y, width - 2 * PAD, 30, 26, white);
			if (version)
				AFHM_Ui.Text(f, "version " + version, PAD, y + 4, width - 2 * PAD, 24, 13, label, 2);
		}
		y += 40;

		// Threat level card.
		if (level > 0)
		{
			float cardH = 112;
			if (chips.Count() == 0)
				cardH = 84;
			if (draw)
			{
				new AFHM_Bubble(f, PAD, y, width - 2 * PAD, cardH, 18, blue);
				AFHM_Ui.Text(f, "THREAT LEVEL", PAD + 16, y + 10, 200, 18, 12, AFHM_Ui.Rgba(255, 255, 255, 0.75));
				AFHM_Ui.Text(f, level.ToString() + "/10", PAD + 16, y + 28, 160, 46, 40, white);
				float segW = 18;
				float segGap = 5;
				float barX = width - PAD - 16 - (10 * segW + 9 * segGap);
				for (int s = 0; s < 10; s++)
				{
					float alpha = 0.25;
					if (s < level)
						alpha = 1;
					new AFHM_Bubble(f, barX + s * (segW + segGap), y + 36, segW, 12, 6, AFHM_Ui.Rgba(255, 255, 255, alpha));
				}
				AFHM_Ui.Text(f, "5 = bots as configured", barX, y + 54, 10 * segW + 9 * segGap, 16, 12, AFHM_Ui.Rgba(255, 255, 255, 0.75), 2);

				float chipX = PAD + 16;
				foreach (string chip: chips)
				{
					array<string> chipParts = new array<string>;
					chip.Split("|", chipParts);
					string chipText = chipParts[0];
					string tone = "info";
					if (chipParts.Count() > 1)
						tone = chipParts[1];
					int chipColor = AFHM_Ui.Rgba(255, 255, 255, 0.22);
					if (tone == "good")
						chipColor = AFHM_Ui.Rgba(48, 209, 88, 0.95);
					else if (tone == "warn")
						chipColor = AFHM_Ui.Rgba(255, 159, 10, 0.95);
					else if (tone == "bad")
						chipColor = AFHM_Ui.Rgba(255, 69, 58, 0.95);
					float chipW = AFHM_Ui.TextWidth(chipText, 13) + 22;
					new AFHM_Bubble(f, chipX, y + 80, chipW, 22, 11, chipColor);
					AFHM_Ui.Text(f, chipText, chipX, y + 80, chipW, 22, 13, white, 1);
					chipX += chipW + 6;
				}
			}
			y += cardH + 10;
		}

		// Key facts.
		if (keys.Count() > 0)
		{
			float keyW = 130;
			float valueW = width - 2 * PAD - 32 - keyW;
			int valueChars = AFHM_Ui.CharsFor(valueW, 14);
			float rowsH = 0;
			foreach (string kvMeasure: keys)
			{
				array<string> kvm = new array<string>;
				kvMeasure.Split("|", kvm);
				string vm = "";
				if (kvm.Count() > 1)
					vm = kvm[1];
				rowsH += AFHM_Ui.Wrap(vm, valueChars).Count() * 19 + 6;
			}
			float factsH = rowsH + 16;
			if (y + factsH <= limit)
			{
				if (draw)
				{
					new AFHM_Bubble(f, PAD, y, width - 2 * PAD, factsH, 16, grey);
					float ry = y + 10;
					foreach (string kv: keys)
					{
						array<string> kvp = new array<string>;
						kv.Split("|", kvp);
						string value = "";
						if (kvp.Count() > 1)
							value = kvp[1];
						array<string> valueLines = AFHM_Ui.Wrap(value, valueChars);
						AFHM_Ui.Text(f, kvp[0], PAD + 16, ry, keyW, 19, 13, label);
						for (int vl = 0; vl < valueLines.Count(); vl++)
							AFHM_Ui.Text(f, valueLines[vl], PAD + 16 + keyW, ry + vl * 19, valueW, 19, 14, white);
						ry += valueLines.Count() * 19 + 6;
					}
				}
				y += factsH + 12;
			}
		}

		// Sections: a small title, then each finding in its own bubble.
		float bubbleMax = (width - 2 * PAD) * 0.86;
		bool cut = false;
		// A section title is drawn only once the item under it fits, so a cut never leaves an
		// empty title at the bottom.
		string pendingTitle = "";
		float pendingY = 0;
		foreach (string item: body)
		{
			int sep = item.IndexOf("|");
			string kind = item.Substring(0, sep);
			string text = item.Substring(sep + 1, item.Length() - sep - 1);
			if (kind == "T")
			{
				if (y + 22 + 40 > limit)
				{
					cut = true;
					break;
				}
				pendingTitle = text;
				pendingY = y;
				y += 22;
				continue;
			}
			else if (kind == "P")
			{
				// Patrol: name|place|details, a blue bubble on the right.
				array<string> pp = new array<string>;
				text.Split("|", pp);
				string head = pp[0];
				if (pp.Count() > 1)
					head += "  near " + pp[1];
				string details = "";
				if (pp.Count() > 2)
					details = pp[2];
				array<string> detailLines = AFHM_Ui.Wrap(details, AFHM_Ui.CharsFor(bubbleMax - 28, 13));
				float pw = Math.Min(bubbleMax, Math.Max(AFHM_Ui.TextWidth(head, 15), AFHM_Ui.TextWidth(detailLines[0], 13)) + 28);
				float ph = 12 + 20 + detailLines.Count() * 17 + 10;
				if (y + ph > limit)
				{
					cut = true;
					break;
				}
				AFHM_ReportView.DrawTitle(f, pendingTitle, pendingY, width, label, draw);
				pendingTitle = "";
				if (draw)
				{
					float px = width - PAD - pw;
					new AFHM_Bubble(f, px, y, pw, ph, 16, blue);
					AFHM_Ui.Text(f, head, px + 14, y + 10, pw - 28, 20, 15, white);
					for (int dl = 0; dl < detailLines.Count(); dl++)
						AFHM_Ui.Text(f, detailLines[dl], px + 14, y + 32 + dl * 17, pw - 28, 17, 13, AFHM_Ui.Rgba(255, 255, 255, 0.85));
				}
				y += ph + 6;
			}
			else
			{
				// A finding: a grey bubble on the left.
				array<string> lines = AFHM_Ui.Wrap(text, AFHM_Ui.CharsFor(bubbleMax - 28, 15));
				float longest = 0;
				foreach (string ln: lines)
					longest = Math.Max(longest, AFHM_Ui.TextWidth(ln, 15));
				float bw = Math.Min(bubbleMax, longest + 28);
				float bh = lines.Count() * 19 + 16;
				if (y + bh > limit)
				{
					cut = true;
					break;
				}
				AFHM_ReportView.DrawTitle(f, pendingTitle, pendingY, width, label, draw);
				pendingTitle = "";
				if (draw)
				{
					new AFHM_Bubble(f, PAD, y, bw, bh, 16, grey);
					for (int li = 0; li < lines.Count(); li++)
						AFHM_Ui.Text(f, lines[li], PAD + 14, y + 8 + li * 19, bw - 28, 19, 15, white);
				}
				y += bh + 6;
			}
		}
		// A title with nothing under it gives its place to the note below.
		if (pendingTitle != "")
			y = pendingY;
		if (cut)
		{
			if (draw)
				AFHM_Ui.Text(f, "More in AFHiveMind\\report.txt on the server.", PAD + 8, y, width - 2 * PAD, 18, 12, dim);
			y += 22;
		}
		return y + PAD;
	}

	protected static void DrawTitle(Widget f, string title, float y, float width, int color, bool draw)
	{
		if (!draw || title == "")
			return;
		string upperTitle = title;
		upperTitle.ToUpper();
		AFHM_Ui.Text(f, upperTitle, PAD + 8, y, width - 2 * PAD, 18, 12, color);
	}
};
