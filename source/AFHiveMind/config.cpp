class CfgPatches
{
	class AFHiveMind
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] =
		{
			"DZ_Data",
			"DayZExpansion_AI_Scripts"
		};
	};
};

class CfgMods
{
	class AFHiveMind
	{
		dir = "AFHiveMind";
		name = "AF HiveMind";
		author = "legacyaf";
		credits = "Built on DayZ Expansion AI";
		version = "0.4.0";
		type = "mod";
		inputs = "AFHiveMind/inputs.xml";
		dependencies[] = {"Game", "World", "Mission"};
		class defs
		{
			class gameScriptModule
			{
				value = "";
				files[] = {"AFHiveMind/Scripts/3_Game"};
			};
			class worldScriptModule
			{
				value = "";
				files[] = {"AFHiveMind/Scripts/4_World"};
			};
			class missionScriptModule
			{
				value = "";
				files[] = {"AFHiveMind/Scripts/5_Mission"};
			};
		};
	};
};
