# AF HiveMind

A DayZ mod that watches how players and [DayZ Expansion AI](https://steamcommunity.com/sharedfiles/filedetails/?id=2792982069) bots fight, remembers it in a file that keeps evolving, and tunes every newly spawned patrol bot from what it has learned.

![The in-game debug panel](docs/panel.jpg)

## What it does

**Observes** (server side, players versus bots only):

- who killed whom, from how far, with which weapon, where on the map and at what in-game hour
- hits and headshots players land on bots, hits bots land on players
- where players spend their time

**Learns.** Every ten minutes (the evolve step) it compares the share of lethal fights the players win with a target (80% by default) and moves a single difficulty figure between 0 and 1. Memory fades with a half-life of 72 hours of server uptime, so the bots follow how people play now, not last month.

**Adjusts.** Each bot a patrol spawns keeps the patrol's own settings, scaled by the difficulty:

| Knob | At difficulty 0 | At 0.5 | At 1 |
|---|---|---|---|
| Accuracy | x0.6 | as configured | x1.5 |
| Damage dealt | x0.75 | as configured | x1.3 |
| Damage received | as configured (scaling is off by default) | | |
| Threat range | grows to cover the distance players kill from, up to 1200 m | | |

Two local corrections sit on top of the global figure: bots in a 500 m zone where players farm them get up to +0.15, and bots spawning near a player who rarely loses get up to +0.2 (and the reverse for a zone or a player the bots dominate).

Every limit above is a number in `config.json`.

## Files it keeps

In the server profile folder, under `AFHiveMind\`:

| File | What it is |
|---|---|
| `config.json` | Switches and limits. Written with defaults on first start, re-read at every evolve step, so it can be edited while the server runs. |
| `brain.json` | Everything it has learned. Readable, hand-editable; delete it to start over. |
| `report.txt` | The same knowledge in plain words, rewritten at every save. |
| `hivemind.log` | Journal: every change of mind, every fight it saw (`DebugLevel` 1), every bot it tuned (`DebugLevel` 2). |

## Debug panel

Press **F9** in game (rebindable under Controls > AF HiveMind). It shows the current generation and difficulty, what was learned in plain words, and the latest patrols it tuned with the reason for each figure. On a server, only Steam64 ids listed under `Admins` in `config.json` get the report.

## Installing

Requires CF, Dabs Framework, DayZ-Expansion-Core and DayZ-Expansion-AI.

- **With the debug panel:** load `@AFHiveMind` with `-mod=` on the server and on every client, and copy `Keys\AFHiveMind.bikey` to the server's `keys` folder.
- **Without it:** load it with `-serverMod=` on the server only. Players download nothing; use `report.txt` and `hivemind.log` instead of the panel.

It only touches bots spawned by Expansion patrols (including object patrols and quest AI, which use the same patrol class).

## Building and testing

```powershell
powershell -ExecutionPolicy Bypass -File tools\build.ps1
```

packs `source\AFHiveMind` into `@AFHiveMind\Addons` and signs it with `keys\AFHiveMind.biprivatekey` (not in git; create one with DayZ Tools' DSCreateKey).

```powershell
powershell -ExecutionPolicy Bypass -File testmission\run_test.ps1 -Fresh
```

runs an offline mission in DayZDiag: a patrol spawns on an empty brain, the player's rifle kills it, the HiveMind evolves, a second patrol must come out harder, and a bot then kills the player. Leave out `-Fresh` to run again on the brain the last run saved.

What the offline test cannot show: bots do not manage to fire in offline DayZDiag (their weapons fail to chamber), so the player's death is dealt by script as melee damage from a bot, and bullet hits on bots are dealt by script with the player's rifle as the source. Real firefights have only been exercised through these stand-ins, not on a live server yet.

## How it hooks into Expansion

Three overrides, in `Scripts\4_World\AFHiveMind\AFHM_Hooks.c`:

- `eAIDynamicPatrol.SetupAI` - tune a bot after Expansion applied the patrol's settings
- `PlayerBase.EEKilled` and `PlayerBase.EEHitBy` - record player-versus-bot kills and hits
- `PlayerBase.OnRPC` - the debug panel's request and report

If an Expansion update breaks the mod, these are the places to look.
