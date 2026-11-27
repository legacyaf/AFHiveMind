// Posts to a Discord channel through a webhook (config.json: DiscordWebhookUrl): threat level
// changes and admin actions, so staff can follow what the HiveMind and the admins do.
// Server side only. The URL is a secret: it is never sent to players and never logged.

class AFHM_DiscordCallback extends RestCallback
{
	override void OnError(int errorCode)
	{
		AFHM_Util.Log("discord: the post failed (error " + errorCode.ToString() + "); check DiscordWebhookUrl");
	}

	override void OnTimeout()
	{
		AFHM_Util.Log("discord: the post timed out");
	}

	override void OnSuccess(string data, int dataSize)
	{
		AFHM_Discord.s_Delivered++;
	}
};

class AFHM_Discord
{
	protected static ref AFHM_DiscordCallback s_Callback;
	protected static bool s_WarnedUrl;
	// Counted for the test mission.
	static int s_Posted;
	static int s_Delivered;

	// A threat level announcement: the title in bold, then the text.
	static void Announcement(AFHiveMind hm, string title, string text, bool area)
	{
		if (!hm || !hm.m_Config)
			return;
		if (area && !hm.m_Config.DiscordAreaChanges)
			return;
		if (!area && !hm.m_Config.DiscordMapChanges)
			return;
		AFHM_Discord.Post(hm, "**" + title + "**\n" + text);
	}

	// An F10 button press: who and what.
	static void AdminAction(AFHiveMind hm, string who, string done)
	{
		if (!hm || !hm.m_Config || !hm.m_Config.DiscordAdminActions)
			return;
		AFHM_Discord.Post(hm, "**Admin** " + who + ": " + done);
	}

	static void Post(AFHiveMind hm, string content)
	{
		string url = hm.m_Config.DiscordWebhookUrl;
		url = url.Trim();
		if (url == "")
			return;
		// https only (a local address is allowed for testing), so the secret URL is never sent in the clear.
		if (url.IndexOf("https://") != 0 && url.IndexOf("http://127.0.0.1") != 0 && url.IndexOf("http://localhost") != 0)
		{
			if (!s_WarnedUrl)
				AFHM_Util.Log("discord: DiscordWebhookUrl must start with https:// ; nothing posted");
			s_WarnedUrl = true;
			return;
		}
		RestApi api = GetRestApi();
		if (!api)
			api = CreateRestApi();
		if (!api)
			return;
		RestContext ctx = api.GetRestContext(url);
		if (!ctx)
			return;
		ctx.SetHeader("application/json");
		if (!s_Callback)
			s_Callback = new AFHM_DiscordCallback();

		string message = content;
		if (message.Length() > 1900)
			message = message.Substring(0, 1897) + "...";
		// allowed_mentions empty: a player name like "@everyone" can never ping the channel.
		string body = "{\"username\":\"" + AFHM_Discord.Escape(hm.m_Config.DiscordName) + "\",\"content\":\"" + AFHM_Discord.Escape(message) + "\",\"allowed_mentions\":{\"parse\":[]}}";
		ctx.POST(s_Callback, "", body);
		s_Posted++;
	}

	// Text made safe inside a JSON string.
	static string Escape(string text)
	{
		string t = text;
		t.Replace("\\", "\\\\");
		t.Replace("\"", "\\\"");
		t.Replace("\r", "");
		t.Replace("\n", "\\n");
		t.Replace("\t", " ");
		return t;
	}
};
