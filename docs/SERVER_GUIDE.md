# AF HiveMind: guide for server owners

This guide covers installing AF HiveMind, using it, tuning it and fixing common problems. It applies to version 0.5.0 and later.

## Contents

1. [What AF HiveMind does](#1-what-af-hivemind-does)
2. [Requirements](#2-requirements)
3. [Installing](#3-installing)
4. [First start and becoming an admin](#4-first-start-and-becoming-an-admin)
5. [The F9 report and the F10 admin controls](#5-the-f9-report-and-the-f10-admin-controls)
6. [How it learns](#6-how-it-learns)
7. [What it changes on the bots](#7-what-it-changes-on-the-bots)
8. [Its files](#8-its-files)
9. [Settings (config.json)](#9-settings-configjson)
10. [Common recipes](#10-common-recipes)
11. [Updating, backups and starting over](#11-updating-backups-and-starting-over)
12. [Troubleshooting](#12-troubleshooting)
13. [Uninstalling](#13-uninstalling)

---

## 1. What AF HiveMind does

AF HiveMind runs on top of **DayZ Expansion AI**. It watches every fight between players and AI bots, remembers how those fights go, and tunes each newly spawned bot from what it has learned:

- When players farm the bots, the bots get sharper.
- When the bots dominate, they ease off.
- When players mostly snipe, or mostly rush, or mostly fight at night, the bots adapt to that.

It does not cheat. Bots get no wallhacks, no aimbot and no knowledge of where players are. HiveMind turns the same dials an admin would turn by hand in Expansion's settings, and keeps turning them as the community's play changes.

It only adjusts bots. It never touches players, loot tables, vehicles or bases, except for one optional extra item that a hard bot may drop when killed (see bounty loot).

## 2. Requirements

On the server, and on clients if you use the in-game panels:

| Mod | Why |
|---|---|
| Community Framework (CF) | Required by Expansion |
| Dabs Framework | Required by Expansion |
| DayZ-Expansion-Core | Required by Expansion AI |
| DayZ-Expansion-AI | The bots HiveMind tunes |

The Expansion Bundle includes Core and AI, so it works as well.

The following are optional. HiveMind tunes their bots automatically when they are loaded:

- Moving AI Convoy
- BS Patrol (helicopter and tank)
- any other mod that creates Expansion AI bots

## 3. Installing

### Choose how to load it

| | Server only (`-serverMod`) | Server and clients (`-mod`) |
|---|---|---|
| Players download it | No | Yes |
| F9 report and F10 admin controls in game | No | Yes |
| Learning and bot tuning | Yes | Yes |
| Where you read what it learned | `report.txt` and `hivemind.log` on the server | In game, plus the same files |

Most servers should use `-mod`. The admin controls are the easiest way to manage it.

### Self-hosted server (your own PC or dedicated box)

1. Subscribe to **AF HiveMind** on the Steam Workshop. Then copy its folder from `steamapps\workshop\content\221100\<id>` to your server folder and name it `@AFHiveMind`.
2. Copy `@AFHiveMind\Keys\AFHiveMind.bikey` to the server's `keys` folder. If you skip this, the server kicks players for an unsigned mod.
3. Add the mod to your start line, after its requirements:
   ```
   -mod=@CF;@Dabs Framework;@DayZ-Expansion-Core;@DayZ-Expansion-AI;@AFHiveMind
   ```
   For server only, leave it out of `-mod` and add `-serverMod=@AFHiveMind` instead.
4. Start the server.

### Hosted server (GTX Gaming, Nitrado and similar)

1. In the host's control panel, open the mod manager or Workshop mods page and add **AF HiveMind** by its Workshop id, next to CF, Dabs Framework and Expansion.
2. Make sure it loads **after** Expansion. Most panels let you order mods; HiveMind goes near the end.
3. Most panels copy the `.bikey` for you. If players get kicked for "AFHiveMind.pbo is not signed", upload `Keys\AFHiveMind.bikey` to the server's `keys` folder through the File Manager or FTP.
4. Restart the server.

Hosts that install mods from the Workshop download the latest version on each restart. To update HiveMind on such a server, restart it after a new version is published.

## 4. First start and becoming an admin

On its first start, HiveMind creates the folder `AFHiveMind` in the **server profile** folder. That is the folder given with `-profiles=`; on hosted servers it is usually `profiles` or `config`. It holds `config.json`, which contains every setting.

To use F9 and F10 on a live server, add your Steam64 id under `Admins`:

1. Find your Steam64 id: it is the 17-digit number in your Steam profile link, or look it up on steamid.io.
2. Open `AFHiveMind\config.json` and fill in `Admins`:
   ```json
   "Admins": ["76561190000000000", "76561190000000001"],
   ```
3. Save the file. HiveMind re-reads `config.json` every 10 minutes, or restart the server for it to take effect at once.

Players who are not listed see "you are not listed under Admins in config.json". Any button presses they send are refused and logged.

## 5. The F9 report and the F10 admin controls

### F9: the report

**F9** opens a read-only report in the top left corner. It refreshes every 3 seconds while open, and you can keep playing with it open. Press F9 again to close it. The report shows:

- **The top line:** the version, the generation (how many learning steps it has taken) and the time.
- **The state:** whether tuning is on, the current difficulty, and whether learning is FROZEN or the difficulty is PINNED.
- **What it remembers:** fights still in memory, lifetime totals, and how many bots it has tuned.
- **What it learned:** the same conclusions as `report.txt`, in plain words.
- **The latest patrols it tuned:** each with its difficulty and the reason, for example "global 0.6, zone +0.1".

### F10: the admin controls

**F10** opens the admin controls in the middle of the screen: one see-through rounded window, with the report on the left and the buttons on the right. **Esc** or **Close** shuts it.

The report on both F9 and F10 is laid out like a chat:

- **Threat level card** (blue): the level out of 10 with a 10-step bar, and status chips: learning on (green) or frozen (orange), pinned (orange), notifications on.
- **Key facts** (grey card): difficulty, the players' win rate against the target, how many fights count and from how many players, what it remembers, lifetime totals, and when the next learning step runs.
- **What it learned**, sorted into topics (Difficulty, How players fight, Hot zones, Time of day, Loot), each finding in its own grey bubble.
- **Latest patrols** as blue bubbles: the patrol, the nearest town, and the numbers it gave the bots.

What doesn't fit on the screen is in `report.txt` on the server.

The buttons are grouped:

| Group | Button | What it does |
|---|---|---|
| Learning | **Freeze learning** | Stops learning. No fights are recorded and nothing fades. Bots are still tuned from what it already knows, and hard bots still drop bounty loot. Use it during events, when you test things yourself, or when you are happy with how the bots are. |
| | **Resume learning** | Starts learning again. |
| | **Run a learning step now** | Runs the 10-minute learning step at once. |
| Difficulty | **Pin the current level** | Holds the difficulty at its current value. Fights are still learned from for tactics, zones and so on, but they no longer move the difficulty. |
| | **Easier -0.05 / Harder +0.05** | Pins the difficulty 0.05 lower or higher. Click several times for bigger steps. |
| | **Unpin (learn again)** | Lets the difficulty learn again, starting from where it was pinned. |
| Danger zone | **Reset brain** | Takes two clicks. Forgets everything and starts again from "as configured". The old brain is kept in `AFHiveMind\backups` as `reset_brain_<date>_<time>.json`. Settings, the freeze and the pin are kept. |

A short note under the buttons confirms each press.

Every button press is checked against `Admins` on the server and written to `hivemind.log` with the admin's name. Freeze and pin are saved in `config.json` as `Frozen` and `PinnedDifficulty`, so they survive restarts.

### If F9 or F10 do nothing

DayZ only gives a new key its default for players who have no saved key settings. Most admins who had an earlier version will find F10 blank. To bind it, go to **Options > Controls > AF HiveMind**, bind "AF HiveMind admin controls" (and the debug panel if it is blank too), and save.

## 6. How it learns

### What it watches

It only watches fights between players and bots:

- who killed whom, from how far, with which weapon, where on the map and at what in-game hour;
- hits and headshots;
- where players spend their time.

Player-versus-player fights, bot-versus-bot fights, quest givers and traders are ignored. A player who runs a bot over with a vehicle counts as the killer, and so does a bot that runs a player over.

### The difficulty figure

Everything hangs on one number between **0** and **1**:

- **0**: the easiest bots it will make.
- **0.5**: bots exactly as you configured them in Expansion.
- **1**: the hardest bots it will make.

### The learning step (every 10 minutes)

1. Old fights fade a little. Memory has a half-life of 72 hours by real time, so downtime counts too. The bots follow how people play now, not last month.
2. It compares the share of fights players win with the target, 80% by default.
   - Players win more than the target: the bots are too easy, so the difficulty goes up.
   - Players win less: the bots are too hard, so the difficulty goes down.
3. It moves the difficulty at most 0.15 per step, less when there were only a few new fights.

### It follows the community, not one player

One strong player cannot drive the difficulty up for everyone:

- **Each player counts for at most 8 fights.** A player with more still counts with their own win rate, just scaled down to 8 fights.
- **The difficulty only moves once 20 fights count**, all players together. With the defaults, that takes at least 3 players who have fought bots.
- **No player adds more than 3 fights** to one learning step.

Until then, the report says "Not enough fights across all players yet", and the bots stay at the current difficulty.

### Local corrections

On top of the global figure:

- **Zones (500 m squares):** where players farm bots, bots get up to +0.15. Where bots dominate, they get up to -0.15.
- **Near a player (off by default):** `PlayerOffsetMax` makes bots harder near a player who rarely loses. Since 0.4.0 it is 0, because the difficulty is meant to follow the community.

## 7. What it changes on the bots

HiveMind tunes each bot when it spawns, starting from the bot's own Expansion settings. Bots that are already alive are not changed.

### Scaled by difficulty

| Knob | At 0 | At 0.5 | At 1 |
|---|---|---|---|
| Accuracy | x0.6 | as configured | x1.5 |
| Damage dealt | x0.75 | as configured | x1.3 |
| Damage received | as configured (off by default, see settings) | | |
| Sight (threat range) | x0.75 | as configured | x1.1, never past about 1,100 m |
| Hearing (how far a bot goes to check a gunshot) | x0.7 | as configured | x1.3 |

### Hot zones: more bots where players farm them

When players win far more often than the target in a zone, ordinary patrols spawning there come out **bigger**. Quest and object patrols keep their size:

- up to 50% more bots;
- never more than 3 extra per patrol.

For example, in a zone where players win almost every fight, a 4-bot patrol comes out with 6 bots. The extra bots join the patrol like any other member. Saved (persistent) patrols are never made bigger. The report's "Bots die most around ..." line says how much bigger patrols get in that zone, and with `DebugLevel` 1 or higher `hivemind.log` lists each patrol that grew.

### Range counter

Every bot kill is sorted by distance: beyond 150 m counts as sniping, within 40 m as rushing. Once the community clearly leans one way:

- **Against snipers:** bots flank wider, hunt down a gunshot from further away, and up to 40% of new patrol bots carry a scoped rifle.
- **Against rushers:** bots push straight in, and up to 40% carry automatics.

Bots keep their patrol's clothes and gear; only the long gun changes. The weapons come from two Expansion loadouts that HiveMind writes once to `profile\ExpansionMod\Loadouts`: `AFHM_MarksmanWeapons.json` and `AFHM_AssaultWeapons.json`. They contain vanilla weapons only. Edit them like any Expansion loadout, for example to add modded weapons.

### Night counter

Bots without night vision are nearly blind at night. Once more than 30% of fights happen after dark, some new patrol bots get night-vision goggles, rising to half of them at 70%. Bots switch the goggles on themselves at dusk. The goggles are removed when the bot dies, so they never become loot, unless you set `NightGearLootable` to true.

### Bounty loot

A bot tuned above difficulty 0.6 that is killed by a player may drop one extra item where it falls. The chance rises to 75% at difficulty 1, and so does the share taken from the rare list instead of the common one. Both lists are in `config.json`. Unknown item names are skipped and logged.

### Threat level announcements

When the bots change, players hear about it through Expansion's notifications. The **threat level** runs from 1 to 10 and follows the difficulty: **5 is the bots exactly as you configured them**. Only a change of a whole level is announced; small adjustments stay quiet.

- **Whole map:** "AI THREAT LEVEL 6/10: Bots across the whole map got tougher (level 5 to 6). Now: accuracy x1.05 (was x1.00), damage x1.03 (was x1.00), ..." The figures are multipliers of how you configured the bots.
- **An area:** a farmed zone that sits above or below the map's level is named after the nearest town or village: "AI THREAT LEVEL 7/10: GRISHINO: Players keep winning around Grishino, so bots there are tougher than the rest of the map (level 7, map 6). ... Patrols there come out up to x1.4 as big (at most 3 extra bots)." When it drops back, players are told it is back to the map's level.

Tougher is shown in red with a skull, easier in green with a shield. At most `NotifyAreasMax` area announcements go out per learning step, biggest changes first. Pinning, resetting or running a step from F10 is announced the same way. The first start after installing or updating announces nothing; it only records the current level. Every announcement is also written to `hivemind.log` ("announced: ..."). The F9 report shows the current level on its second line.

Notifications use Expansion's own system, so they work with HiveMind loaded on the server only, and they follow Expansion's notification setting (`EnableNotification`).

| Bots | Tuned? |
|---|---|
| Expansion patrols, object patrols, quest enemies | Yes |
| Bots other mods create from Expansion AI (convoys, helicopter or tank crews) | Yes, a few seconds after they appear. They get the flanking change but keep their own weapons. |
| Saved (persistent) patrols | Tuned, but their weapons and gear are never changed, and they are never made bigger |
| Quest and object patrols | Tuned, but never made bigger |
| Bots following a player (recruited) | No |
| Quest givers and traders | No, and fights with them don't count |

## 8. Its files

All of these are in the server profile, folder `AFHiveMind`:

| File | What it is |
|---|---|
| `config.json` | All settings. Re-read every 10 minutes, so you can edit it while the server runs. Settings added by an update appear in it automatically, and your own values are kept. |
| `brain.json` | Everything it has learned. Readable, and editable by hand while the server is stopped. |
| `report.txt` | The same as the F9 report, plus a list of players. Rewritten every couple of minutes. |
| `hivemind.log` | A journal of every change of mind, every fight it saw, every admin button press, and, with `DebugLevel` 2, every bot it tuned. |
| `backups\brain_<date>.json` | One copy of the brain per day; the last 7 are kept. |
| `backups\reset_brain_<date>_<time>.json` | The brain as it was before each Reset brain. These are never deleted automatically. |

## 9. Settings (config.json)

Change a value, save, and it applies at the next learning step, or at once after a restart. Leave `ConfigVersion` alone; HiveMind uses it to update older files.

### General

| Setting | Default | Meaning |
|---|---|---|
| `Enabled` | true | false switches HiveMind off: no learning, no tuning. |
| `DebugLevel` | 1 | 0 logs only changes of mind and admin actions. 1 also logs every fight and every patrol a hot zone made bigger. 2 also logs every tuned bot (busy). |
| `Admins` | [] | Steam64 ids allowed to use F9 and F10. |
| `TuneOtherBots` | true | Tune bots that other mods create from Expansion AI. |
| `OtherBotsDelaySeconds` | 3 | How long after such a bot appears it is tuned, so its own mod can set it up first. |
| `SaveIntervalSeconds` | 120 | How often the brain and report are saved. |
| `EvolveIntervalSeconds` | 600 | How often it learns, in seconds. |
| `MemoryHalfLifeHours` | 72 | After this many hours a fight counts half. Lower means it reacts faster but is more jumpy. |

### Learning

| Setting | Default | Meaning |
|---|---|---|
| `TargetPlayerWinRate` | 0.8 | Share of fights players should win. 0.7 gives harder bots; 0.9 gives easier ones. |
| `LearnRate` | 0.15 | Most the difficulty can move per step. |
| `MinSamples` | 3 | Fights needed before it trusts a zone's or a player's figures, or the tactics. |
| `PlayerFightsCap` | 8 | Most fights one player counts for in the difficulty. 0 means no cap. |
| `DifficultyMinFights` | 20 | Fights needed, all players together, before the difficulty moves. |
| `PlayerFightsPerStepMax` | 3 | Most fights one player adds to one step. 0 means no limit. |
| `Frozen` | false | The F10 freeze. |
| `PinnedDifficulty` | -1 | The F10 pin. 0 to 1 holds the difficulty there; -1 means not pinned. |

### Strength

| Setting | Default | Meaning |
|---|---|---|
| `AccuracyScaleMin` / `AccuracyScaleMax` | 0.6 / 1.5 | Accuracy multiplier at difficulty 0 and at 1. |
| `DamageScaleMin` / `DamageScaleMax` | 0.75 / 1.3 | Damage dealt by bots, at 0 and at 1. |
| `DamageReceivedScaleEasy` / `DamageReceivedScaleHard` | 1 / 1 | Damage bots take, at 0 and at 1. Off by default. Try 1.15 and 0.9 to make bots softer when easy and tougher when hard. |
| `ThreatScaleEasy` / `ThreatScaleHard` | 0.75 / 1.1 | How far bots notice players, at 0 and at 1. |
| `NoiseScaleEasy` / `NoiseScaleHard` | 0.7 / 1.3 | How far bots go to check a gunshot, at 0 and at 1. |

### Zones and players

| Setting | Default | Meaning |
|---|---|---|
| `ZoneSize` | 500 | Size of a zone, in metres. |
| `ZoneOffsetMax` | 0.15 | Most a zone can add to or take from the difficulty. 0 switches zone corrections off. |
| `PlayerOffsetMax` | 0 | Most a nearby player's record can add or take. Off by default. |
| `PlayerInfluenceRadius` | 1200 | How near, in metres, a player must be to count as "near". |
| `HotZoneExtraBots` | true | Bigger patrols in hot zones. |
| `HotZoneExtraShareMax` | 0.5 | Up to this much bigger (0.5 = half as many again). |
| `HotZoneExtraBotsMax` | 3 | Never more than this many extra bots per patrol. |

### Range counter

| Setting | Default | Meaning |
|---|---|---|
| `AdaptTactics` | true | false switches the whole range counter off. |
| `LongRangeDistance` | 150 | A kill beyond this counts as sniping. |
| `CloseRangeDistance` | 40 | A kill within this counts as rushing. |
| `RoleShareMax` | 0.4 | Most bots that swap weapons. 0 keeps the flanking change but never swaps weapons. |
| `MarksmanLoadout` / `AssaultLoadout` | AFHM_MarksmanWeapons / AFHM_AssaultWeapons | The Expansion loadouts the weapons come from. |
| `FlankWiderMax` / `FlankNarrowerMin` | 1.5 / 0.6 | Flanking distance against snipers and against rushers. |
| `HearingWiderMax` / `HearingNarrowerMin` | 1.5 / 0.8 | Gunshot-hunting distance against snipers and against rushers. |

### Night counter

| Setting | Default | Meaning |
|---|---|---|
| `NightCounter` | true | false switches it off. |
| `NightShareStart` | 0.3 | Night share of fights where goggles start appearing. |
| `NightShareFull` | 0.7 | Night share where the full share of bots get them. |
| `NightGearShareMax` | 0.5 | Most bots that get goggles. |
| `NightGearLootable` | false | true lets players loot the goggles. |

### Bounty loot

| Setting | Default | Meaning |
|---|---|---|
| `BountyLoot` | true | false switches it off. |
| `BountyFromDifficulty` | 0.6 | Bots tuned above this may drop a bounty. |
| `BountyChanceMax` | 0.75 | Drop chance at difficulty 1. |
| `BountyRareShareMax` | 0.5 | Share of drops from the rare list at difficulty 1. |
| `BountyCommon` / `BountyRare` | medical, ammo... / NVG, scopes... | Item class names. Use any item from your mods. |

### Announcements

| Setting | Default | Meaning |
|---|---|---|
| `Notify` | true | false: no threat level notifications at all. |
| `NotifyPlayers` | true | false: only the admins listed in `Admins` get them. |
| `NotifyAreasMax` | 2 | Area announcements per learning step at most. 0: whole map only. |
| `NotifySeconds` | 12 | How long each notification stays on screen. |

## 10. Common recipes

| You want | Do this |
|---|---|
| Harder bots overall | Lower `TargetPlayerWinRate` to 0.7, or pin the difficulty at 0.7 with F10. |
| Easier bots overall | Raise `TargetPlayerWinRate` to 0.9, or pin lower. |
| Bots exactly as configured for an event | F10: Freeze learning, then pin at 0.5. Unpin and resume afterwards. |
| A small server (1-2 players) where the difficulty never moves | Lower `DifficultyMinFights` to 10 and `PlayerFightsCap` to 5, or set `PlayerFightsCap` to 0 (no cap). |
| It reacts too slowly | Lower `MemoryHalfLifeHours` to 24, or raise `LearnRate` to 0.25. |
| No extra bots in hot zones | `HotZoneExtraBots`: false. |
| Bots that never change weapons | `RoleShareMax`: 0. |
| No extra loot | `BountyLoot`: false. |
| No threat level notifications | `Notify`: false. Admins only: `NotifyPlayers`: false. |
| Fewer area notifications | `NotifyAreasMax`: 1, or 0 for the whole map only. |
| Leave other mods' bots alone | `TuneOtherBots`: false. |
| Switch it all off without uninstalling | `Enabled`: false. Bots spawned afterwards are exactly as configured. |

## 11. Updating, backups and starting over

- **Updating:** Workshop servers get the new version when they restart after it is published. Self-hosted servers need the new `@AFHiveMind` copied over, then a restart. `config.json` and `brain.json` are kept, and new settings are added automatically.
- **Rolling back the brain:** stop the server, copy a file from `AFHiveMind\backups` over `AFHiveMind\brain.json`, and start the server.
- **Starting over:** use F10 > Reset brain, or stop the server and delete `brain.json`.
- **Resetting the settings:** stop the server and delete `config.json`. It is recreated with the defaults; add your `Admins` again.

## 12. Troubleshooting

| Problem | Cause and fix |
|---|---|
| The server stops at start with "Can't compile ... script module" | Usually a load-order or version mismatch. Check that HiveMind loads after CF, Dabs Framework and Expansion, and that Expansion is up to date. If the error names an `AFHiveMind/` file, report it with the crash log. |
| Players are kicked: "AFHiveMind.pbo is not signed" | Copy `Keys\AFHiveMind.bikey` to the server's `keys` folder. |
| F10 does nothing | Bind it: Options > Controls > AF HiveMind > "AF HiveMind admin controls". Check that the first line of F9 shows the same version as the Workshop. If not, the server or your game is running an older copy. |
| F9 says "you are not listed under Admins" | Add your Steam64 id to `Admins` in `config.json`, then wait 10 minutes or restart. |
| No F9 or F10 at all | The mod is loaded with `-serverMod`. Load it with `-mod` instead. |
| The difficulty never changes | Check the report: "Not enough fights across all players yet" means fewer than 20 fights count. Learning may also be frozen or the difficulty pinned (shown at the top). |
| Bots don't seem tuned | Set `DebugLevel` to 2 and check `hivemind.log` for "tuned:" lines. Make sure `Enabled` is true. |
| A modded item never drops as bounty | `hivemind.log` says "bounty: no item called ...". Fix the class name in `BountyCommon` or `BountyRare`. |
| The range counter weapon swap does nothing | `hivemind.log` says "could not apply loadout". Check that the loadout files are in `profile\ExpansionMod\Loadouts`. |

When asking for help, include the first line of the F9 report, `hivemind.log`, `config.json`, and the server's `crash_*.log` or `script_*.log` if there is an error.

## 13. Uninstalling

1. Remove `@AFHiveMind` from `-mod` or `-serverMod`, or from the host's mod list.
2. Restart. Bots go back to exactly what Expansion's own settings say.
3. Optionally, delete the `AFHiveMind` folder from the server profile, and the two `AFHM_*` loadouts from `ExpansionMod\Loadouts`.
