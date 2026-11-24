// The weapons HiveMind hands out when players clearly snipe or rush. Written once, with vanilla
// items only, to Expansion's loadout folder (profile\ExpansionMod\Loadouts) if the files are
// missing. Admins can edit them like any Expansion loadout, or point MarksmanLoadout and
// AssaultLoadout in config.json at loadouts of their own. Each file holds weapon sets only:
// a bot keeps its patrol's clothes and gear, and one set is picked at random.

class AFHM_Loadouts
{
	static void WriteDefaults()
	{
		ExpansionHumanLoadout.Init();
		Marksman();
		Assault();
	}

	protected static void Marksman()
	{
		if (FileExist(ExpansionLoadout.GetPath("AFHM_MarksmanWeapons")))
			return;
		auto loadout = ExpansionLoadout.Create("AFHM_MarksmanWeapons");
		if (!loadout)
			return;

		loadout = loadout.BeginSet("WEAPON");
			loadout = loadout.BeginAttachment("SVD", "Shoulder");
				loadout = loadout.BeginAttachment("Mag_SVD_10Rnd");
				loadout = loadout.End();
				loadout = loadout.BeginAttachment("PSO1Optic");
					loadout = loadout.BeginAttachment("Battery9V");
					loadout = loadout.End();
				loadout = loadout.End();
			loadout = loadout.End();
			loadout = loadout.BeginCargo("Mag_SVD_10Rnd");
			loadout = loadout.End();
			loadout = loadout.BeginCargo("Mag_SVD_10Rnd");
			loadout.Chance = 0.5;
			loadout = loadout.End();
		loadout = loadout.End();

		loadout = loadout.BeginSet("WEAPON");
			loadout = loadout.BeginAttachment("Mosin9130", "Shoulder");
				loadout = loadout.BeginAttachment("PUScopeOptic");
				loadout = loadout.End();
			loadout = loadout.End();
			loadout = loadout.BeginCargo("Ammo_762x54");
			loadout = loadout.End();
			loadout = loadout.BeginCargo("Ammo_762x54");
			loadout = loadout.End();
		loadout = loadout.End();

		loadout = loadout.BeginSet("WEAPON");
			loadout = loadout.BeginAttachment("CZ527", "Shoulder");
				loadout = loadout.BeginAttachment("Mag_CZ527_5rnd");
				loadout = loadout.End();
				loadout = loadout.BeginAttachment("HuntingOptic");
				loadout = loadout.End();
			loadout = loadout.End();
			loadout = loadout.BeginCargo("Mag_CZ527_5rnd");
			loadout = loadout.End();
			loadout = loadout.BeginCargo("Mag_CZ527_5rnd");
			loadout = loadout.End();
		loadout = loadout.End();

		loadout = loadout.BeginSet("WEAPON");
			loadout = loadout.BeginAttachment("Winchester70", "Shoulder");
				loadout = loadout.BeginAttachment("HuntingOptic");
				loadout = loadout.End();
			loadout = loadout.End();
			loadout = loadout.BeginCargo("Ammo_308Win");
			loadout = loadout.End();
			loadout = loadout.BeginCargo("Ammo_308Win");
			loadout = loadout.End();
		loadout = loadout.End();

		loadout.Save();
	}

	protected static void Assault()
	{
		if (FileExist(ExpansionLoadout.GetPath("AFHM_AssaultWeapons")))
			return;
		auto loadout = ExpansionLoadout.Create("AFHM_AssaultWeapons");
		if (!loadout)
			return;

		loadout = loadout.BeginSet("WEAPON");
			loadout = loadout.BeginAttachment("AKM", "Shoulder");
				loadout = loadout.BeginAttachment("Mag_AKM_30Rnd");
				loadout = loadout.End();
				loadout = loadout.BeginAttachment("AK_WoodBttstck");
				loadout = loadout.End();
				loadout = loadout.BeginAttachment("AK_WoodHndgrd");
				loadout = loadout.End();
			loadout = loadout.End();
			loadout = loadout.BeginCargo("Mag_AKM_30Rnd");
			loadout = loadout.End();
			loadout = loadout.BeginCargo("Mag_AKM_30Rnd");
			loadout.Chance = 0.5;
			loadout = loadout.End();
		loadout = loadout.End();

		loadout = loadout.BeginSet("WEAPON");
			loadout = loadout.BeginAttachment("AK74", "Shoulder");
				loadout = loadout.BeginAttachment("Mag_AK74_30Rnd");
				loadout = loadout.End();
				loadout = loadout.BeginAttachment("AK74_WoodBttstck");
				loadout = loadout.End();
				loadout = loadout.BeginAttachment("AK74_Hndgrd");
				loadout = loadout.End();
			loadout = loadout.End();
			loadout = loadout.BeginCargo("Mag_AK74_30Rnd");
			loadout = loadout.End();
			loadout = loadout.BeginCargo("Mag_AK74_30Rnd");
			loadout.Chance = 0.5;
			loadout = loadout.End();
		loadout = loadout.End();

		loadout = loadout.BeginSet("WEAPON");
			loadout = loadout.BeginAttachment("M4A1", "Shoulder");
				loadout = loadout.BeginAttachment("Mag_STANAG_30Rnd");
				loadout = loadout.End();
				loadout = loadout.BeginAttachment("M4_OEBttstck");
				loadout = loadout.End();
				loadout = loadout.BeginAttachment("M4_PlasticHndgrd");
				loadout = loadout.End();
			loadout = loadout.End();
			loadout = loadout.BeginCargo("Mag_STANAG_30Rnd");
			loadout = loadout.End();
			loadout = loadout.BeginCargo("Mag_STANAG_30Rnd");
			loadout.Chance = 0.5;
			loadout = loadout.End();
		loadout = loadout.End();

		loadout = loadout.BeginSet("WEAPON");
			loadout = loadout.BeginAttachment("MP5K", "Shoulder");
				loadout = loadout.BeginAttachment("Mag_MP5_30Rnd");
				loadout = loadout.End();
				loadout = loadout.BeginAttachment("MP5k_StockBttstck");
				loadout = loadout.End();
				loadout = loadout.BeginAttachment("MP5_PlasticHndgrd");
				loadout = loadout.End();
			loadout = loadout.End();
			loadout = loadout.BeginCargo("Mag_MP5_30Rnd");
			loadout = loadout.End();
			loadout = loadout.BeginCargo("Mag_MP5_30Rnd");
			loadout = loadout.End();
		loadout = loadout.End();

		loadout = loadout.BeginSet("WEAPON");
			loadout = loadout.BeginAttachment("UMP45", "Shoulder");
				loadout = loadout.BeginAttachment("Mag_UMP_25Rnd");
				loadout = loadout.End();
			loadout = loadout.End();
			loadout = loadout.BeginCargo("Mag_UMP_25Rnd");
			loadout = loadout.End();
			loadout = loadout.BeginCargo("Mag_UMP_25Rnd");
			loadout = loadout.End();
		loadout = loadout.End();

		loadout.Save();
	}
};
