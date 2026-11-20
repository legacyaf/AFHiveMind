class AFHT_State
{
	static const int RPC_FIRE = 874410101;
	static const int RPC_PANEL = 874410102;

	static bool s_Fire;
	static bool s_Panel;
};

modded class PlayerBase
{
	override void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
	{
		super.OnRPC(sender, rpc_type, ctx);

		if (GetGame().IsDedicatedServer())
			return;
		if (rpc_type != AFHT_State.RPC_FIRE && rpc_type != AFHT_State.RPC_PANEL)
			return;

		Param1<bool> on = new Param1<bool>(false);
		if (!ctx.Read(on))
			return;
		if (rpc_type == AFHT_State.RPC_FIRE)
			AFHT_State.s_Fire = on.param1;
		else
			AFHT_State.s_Panel = on.param1;
		Print("[AFHiveTest] client got order " + rpc_type.ToString() + " = " + on.param1.ToString());
	}
};
