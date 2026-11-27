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

It moves slowly. A single bad fight won't swing it; it needs a pattern.

### It listens to the whole community

The difficulty is shared by everyone, so no single player gets to set it:

- It doesn't move at all until about **20 fights** count across the server.
- One player's fights count for **8 at most**, so a single strong player (or a single struggling one) can't drag the bots up or down for everyone. With these settings it takes at least three players who fight bots before anything changes.
- One player adds **3 fights at most** to each learning step, so one long killing spree is spread out instead of landing all at once.

### It forgets, too

Old fights fade. After **72 hours**, a fight counts half as much as a fresh one. So the AI adapts to how the community plays **now**, not how it played a month ago. If everyone gets better, the bots catch up. If a wave of new players joins, the bots ease off.

## What changes for the bots

When a bot spawns, HiveMind takes the settings the admins gave that bot and scales them by the current difficulty:

| Setting | At difficulty 0 | At 0.5 | At 1.0 |
|---|---|---|---|
| **Accuracy** | 60% of normal | normal | 150% of normal (capped at perfect) |
| **Damage dealt** | 75% of normal | normal | 130% of normal |
| **How far they see you** | 75% of normal | normal | 110% of normal (never past about 1.1 km) |
| **How far they come to check a gunshot** | 70% of normal | normal | 130% of normal |

There is also a setting for how much damage bots *take*, but it is **switched off** on our server for now.

### Hot zones

If players keep farming bots in one area (the map is split into 500 m squares), bots spawning there get a bit tougher (up to +0.15 difficulty), and **patrols there come out bigger**: up to half as many bots again, and never more than 3 extra. If bots keep winning somewhere, that area eases off instead.

So the bots in a popular farming spot start fighting back. This only affects ordinary roaming patrols: quest objectives and fixed camps keep their size, and so do patrols the server saves across restarts.

There is **no personal adjustment**: the bots near you are not made harder because you are good, or easier because you are struggling. The difficulty follows the community, not one player.


### You'll be told when it changes

When the bots change, a notification pops up for everyone. It shows the **threat level from 1 to 10** (5 is the bots as the admins set them up), whether it's the **whole map** or **one area** (named after the nearest town), and **what changed**: accuracy, damage, how far bots see and hear you, and for a farmed area how much bigger patrols get. Red with a skull means tougher, green with a shield means they eased off.

Only real changes are announced, a whole level at a time, so it won't spam you.

### Bots answer how you fight

HiveMind sorts every kill on a bot by range: beyond 150 m counts as sniping, within 40 m as rushing. When the community clearly leans one way, bots change their approach:

- **If you mostly snipe:** bots flank up to 1.5x wider to get around your position, go up to 1.5x further to hunt down a shot they heard, and up to 40% of newly spawned patrol bots carry a scoped rifle (SVD, Mosin, CZ527 or Winchester 70) instead of their usual gun.
- **If you mostly rush:** bots push in more directly instead of swinging wide, hold closer to their position, and up to 40% of new patrol bots carry an automatic (AKM, AK74, M4A1, MP5K or UMP45).
- **If there's no clear pattern,** nothing changes.

Only the weapon changes. Bots keep their faction's clothes and gear, so a bandit still looks like a bandit. Bots riding in convoys and vehicles keep their own weapons. Like everything else, this fades over time and follows the community's current habits.

### Owning the night

At night, bots without night vision are close to blind, while a player carrying a lit torch or headlamp can be spotted from 250 m. If a large share of the community's fights with bots happen at night, HiveMind starts handing night-vision goggles to some newly spawned patrol bots (up to half of them). Bots switch them on by themselves when it gets dark.

The goggles **can't be looted**: they're removed when the bot dies, so this doesn't flood the server with NVGs.

### Bounty loot

Harder bots are worth more. A bot spawned at above-normal difficulty may drop **one extra item** where it dies, but only when a player killed it. The harder the bot, the better the odds (up to 75%), and the more likely the item comes from the rare list:

- **Common:** bandages, morphine, epinephrine, ammo boxes, rifle magazines
- **Rare:** night-vision goggles, rangefinder, ACOG or PSO-1 scope, radio, saline bag, 60-round STANAG magazine, plate carrier

So when the bots get tougher, fighting them pays more.

### Which bots are affected

- **Expansion patrols** across the map
- **Expansion object patrols** at buildings, and quest bots
- **Bots from other mods** that create Expansion AI directly, such as the convoy and the helicopter and tank patrols. They get the difficulty and tactics changes, but keep their own weapons and gear.

**Not affected:** bots you have recruited to follow you (your squadmates are yours), and quest givers and traders. Fights with quest givers and traders don't count either.

## What it does NOT do

- It doesn't give bots wallhacks, aimbots, or knowledge of where you are.
- It doesn't change loot, spawns, prices, traders, or anything outside AI combat. The only extra loot is the bounty drop from harder bots, plus the rifles some bots carry, which can be looted like any bot's gear.
- It doesn't track player-versus-player fights.
- It doesn't read chat, voice, or anything you type.
- It doesn't make bots invincible or one-shot you. Every value has a hard limit set by the admins.
- It doesn't spawn extra patrols. The only bigger groups are ordinary patrols in hot zones, up to 3 extra bots each.

## Your data

HiveMind keeps one file on the server with what it has learned. For each player who has fought bots, it stores:

- your in-game name and Steam ID
- how many bots you've killed and how many times bots have killed you (fading over time like everything else)
- your average kill distance

Nothing leaves the server. It isn't shared with other servers or any outside service. Entries for players who stop fighting bots fade away and are removed automatically. If you would like your entry deleted, ask an admin.

## For admins

- **Debug panel.** Press **F9** in game to see the version, the current difficulty, what HiveMind has learned (in plain words), and the last patrols it tuned with the reason for each number. Only listed admins can see it.
- **Admin controls.** Press **F10** for the same report with buttons: freeze learning, pin the difficulty (or move it one threat level up or down), unpin it, run a learning step now, or reset the brain (two clicks; the old brain is kept as a backup). Every press is logged. If the key does nothing, bind it under Controls > AF HiveMind.
- **Readable files.** On the server, in `profile\AFHiveMind\`:
  - `brain.json`: everything it has learned
  - `report.txt`: the same in plain English
  - `hivemind.log`: every fight it saw and every change of mind
- **Every limit is a setting** in `config.json`: the target win rate, how fast it learns, how fast it forgets, every scaling limit, and an on/off switch. It is re-read every 10 minutes, so changes take effect without a restart.
- **Reset.** The F10 reset button, or deleting `brain.json` while the server is stopped, makes it start over from "as configured". A daily backup of `brain.json` is kept in `AFHiveMind\backups` (the last 7).

## Planned

These are ideas, not promises. They will be announced when they land:

- **Patrol placement.** More patrols where players actually fight, and at the hours they're online.
- **Headshot counter.** If players mostly land headshots, more bots wear helmets.

## FAQ

**Will the bots become impossible?**
No. The target is that players win about 80% of their fights with bots, and every setting has a ceiling.

**Does dying a lot make the game easier for me?**
No, not personally. Your fights count towards the community's difficulty like everyone else's, and no more than anyone else's.

**Can we game it by dying on purpose?**
Not really. It looks at the whole server over days, one player counts for at most 8 fights, and it needs fights from several players before it moves at all.

**Does my PC need anything extra?**
No. HiveMind runs on the server. If we also load it on clients, the only extra is the admin panel, which regular players won't see.

**Is it "real" AI?**
It's statistics, not a neural network: it counts, averages, and adjusts. That keeps it predictable, fair, and fully explainable. An admin can always see exactly why a bot got the numbers it has.

---

*Questions or feedback? Let the admin team know. HiveMind learns from how you play, and we learn from what you tell us.*
