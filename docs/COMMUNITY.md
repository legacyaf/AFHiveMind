# AF HiveMind: AI that learns how you fight

> **Status: in testing.** HiveMind has been tested on a local test server but not yet on our live server. Everything below describes how it works today. Features marked *planned* are not in yet.

## What is it?

HiveMind is a server-side mod that sits on top of DayZ Expansion AI. It watches every fight between players and AI bots, keeps a running memory of how those fights go, and adjusts newly spawned bots based on what it has learned.

It does not script bots to cheat or know where you are. It turns the same dials a server admin would turn by hand: how accurately bots shoot, how hard they hit, and how far away they notice you. The difference is that it does this continuously, based on how you all actually play.

The goal is simple: **fights against AI should stay challenging but winnable**, whether the server is full of veterans or fresh spawns.

## How it learns

### What it watches

Every time a player and a bot fight, HiveMind notes:

- **Who won.** Did the player kill the bot, or the bot kill the player?
- **Distance.** How far away was the kill?
- **Weapon.** What did the player use?
- **Location.** Which part of the map? The map is split into 500 m squares.
- **Time.** What in-game hour was it?
- **Hits.** How often players hit bots, and how many of those hits were headshots.
- **Where players spend their time.** Sampled once a minute.

It only records **player-versus-bot** fights. Player-versus-player fights and bot-versus-bot fights are ignored.

### How it decides

Every 10 minutes, HiveMind runs a short learning step:

1. It looks at the share of lethal fights players have won recently.
2. It compares that with a **target: players should win about 80% of the time.**
3. If players are winning more than that, bots get a little harder. If players are losing more, bots get a little easier.
4. It writes down what it concluded, in plain words, for admins to read.

All of this is boiled down into one **difficulty** number between 0 and 1:

| Difficulty | Meaning |
|---|---|
| 0.0 | Easiest the server allows |
| 0.5 | Bots exactly as the admins configured them |
| 1.0 | Hardest the server allows |

It moves slowly. A single bad fight won't swing it; it needs a pattern. Until it has seen at least 3 fights, it changes nothing.

### It forgets, too

Old fights fade. After **72 hours of server uptime**, a fight counts half as much as a fresh one. So the AI adapts to how the community plays **now**, not how it played a month ago. If everyone gets better, the bots catch up. If a wave of new players joins, the bots ease off.

## What changes for the bots

When a bot spawns, HiveMind takes the settings the admins gave that bot and scales them by the current difficulty:

| Setting | At difficulty 0 | At 0.5 | At 1.0 |
|---|---|---|---|
| **Accuracy** | 60% of normal | normal | 150% of normal (capped at perfect) |
| **Damage dealt** | 75% of normal | normal | 130% of normal |
| **Detection range** | grows if players start killing from far away (up to 1,200 m) | | |

There is also a setting for how much damage bots *take*, but it is **switched off** on our server for now.

### Local adjustments

On top of the server-wide difficulty, two smaller corrections apply:

- **Hot zones.** If players keep farming bots in one area, bots spawning there get a bit tougher (up to +0.15 difficulty). If bots keep winning somewhere, that area eases off by the same amount.
- **Nearby players.** If a player who rarely loses to bots is within 1.2 km of where a bot spawns, that bot gets a bit tougher (up to +0.2). A player who struggles gets bots that are a bit easier.

That means a skilled player roaming alone may run into stiffer resistance than a new player nearby, and the bots in a popular farming spot will start fighting back.

### Sniping makes bots more alert

If players start landing kills from long range, bots learn to keep watch further out. They won't see through walls, but they'll react to threats at the distances you have actually been killing them from.

### Which bots are affected

- **Expansion patrols** across the map
- **Expansion object patrols** at buildings, and quest bots
- **Bots from other mods** that create Expansion AI directly, such as the convoy and the helicopter and tank patrols (being tested now)

**Not affected:** bots you have recruited to follow you. Your squadmates are yours, and HiveMind leaves them alone.

## What it does NOT do

- It doesn't give bots wallhacks, aimbots, or knowledge of where you are.
- It doesn't change loot, spawns, prices, traders, or anything outside AI combat.
- It doesn't track player-versus-player fights.
- It doesn't read chat, voice, or anything you type.
- It doesn't make bots invincible or one-shot you. Every value has a hard limit set by the admins.
- It doesn't spawn more bots. *(Planned: see below.)*

## Your data

HiveMind keeps one file on the server with what it has learned. For each player who has fought bots, it stores:

- your in-game name and Steam ID
- how many bots you've killed and how many times bots have killed you (fading over time like everything else)
- your average kill distance

Nothing leaves the server. It isn't shared with other servers or any outside service. Entries for players who stop fighting bots fade away and are removed automatically. If you would like your entry deleted, ask an admin.

## For admins

- **Debug panel.** Press **F9** in game to see the current difficulty, what HiveMind has learned (in plain words), and the last patrols it tuned with the reason for each number. Only listed admins can see it.
- **Readable files.** On the server, in `profile\AFHiveMind\`:
  - `brain.json`: everything it has learned
  - `report.txt`: the same in plain English
  - `hivemind.log`: every fight it saw and every change of mind
- **Every limit is a setting** in `config.json`: the target win rate, how fast it learns, how fast it forgets, every scaling limit, and an on/off switch. It is re-read every 10 minutes, so changes take effect without a restart.
- **Reset.** Deleting `brain.json` makes it start over from "as configured".

## Planned

These are ideas, not promises. They will be announced when they land:

- **Tactics, not just difficulty.** If the community mostly snipes, more bots carry scoped rifles and flank wider. If you mostly rush, more automatics and tighter groups.
- **Group size.** Adding or removing a bot per patrol instead of only making each one stronger.
- **Patrol placement.** More patrols where players actually fight, and at the hours they're online.
- **Admin controls** in the panel: freeze learning, pin a difficulty, or reset with a button.

## FAQ

**Will the bots become impossible?**
No. The target is that players win about 80% of their fights with bots, and every setting has a ceiling.

**Does dying a lot make the game easier for me?**
A little, for bots that spawn near you, and only within limits. It is meant to keep fights fair, not to reward dying.

**Can we game it by dying on purpose?**
Not really. It looks at the whole server over days, and one person's deaths barely move the global number. The personal adjustment is capped too.

**Does my PC need anything extra?**
No. HiveMind runs on the server. If we also load it on clients, the only extra is the admin panel, which regular players won't see.

**Is it "real" AI?**
It's statistics, not a neural network: it counts, averages, and adjusts. That keeps it predictable, fair, and fully explainable. An admin can always see exactly why a bot got the numbers it has.

---

*Questions or feedback? Let the admin team know. HiveMind learns from how you play, and we learn from what you tell us.*
