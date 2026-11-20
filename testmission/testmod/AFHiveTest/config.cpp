// Test-only helper for testmission\run_server_test.ps1. Never ship this.
// On the client it holds the fire button and opens the HiveMind panel when the test server asks.
class CfgPatches
{
	class AFHiveTest
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] = {"AFHiveMind"};
	};
};

class CfgMods
{
	class AFHiveTest
	{
		dir = "AFHiveTest";
		name = "AF HiveMind test helper";
		type = "mod";
		dependencies[] = {"World", "Mission"};
		class defs
		{
			class worldScriptModule
			{
				value = "";
				files[] = {"AFHiveTest/Scripts/4_World"};
			};
			class missionScriptModule
			{
				value = "";
				files[] = {"AFHiveTest/Scripts/5_Mission"};
			};
		};
	};
};
