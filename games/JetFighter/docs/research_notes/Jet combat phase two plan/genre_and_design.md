# Air-to-air jet combat: genre landscape and what makes dogfighting fun

Research scope: design input for adding original jet fighting to an indie UE5 game (Boat Race). Research done 19 Nov 2026 with about 18 search/fetch calls; several reviews (PC Gamer Project Wingman, PC Gamer Nuclear Option) came back truncated, and some wiki pages returned 404 or 402. Gaps are listed honestly under each question. Items marked "(unverified, from background knowledge)" are not sourced this session and must be checked before being relied on.

## 1. How arcade, hybrid and sim jet games differ, and what each audience values

### Takeaway
The spectrum runs from "flies itself, missiles everywhere, infinite gun, respawn" (Ace Combat, H.A.W.X. assisted, War Thunder Air Arcade, Star Fox) through "physically based but simplified and exaggerated" (Nuclear Option, Tiny Combat Arena, Sky Rogue's realistic mode) to full sims (DCS, VTOL VR). The most-praised hybrid insight is that a game should *feel* realistic rather than *be* realistic, and that arcade games risk being too easy to fly to be thrilling.

### Cited Findings
**Arcade**
- Ace Combat 7 is "brilliantly accessible" because it is "so easy to keep your plane in the air", but the reviewer says this means "you have to find your thrills elsewhere"; "Combat is easy: fly into missile range of the target" and "There's little challenge to it." — [PCGamesN AC7 PC review](https://www.pcgamesn.com/ace-combat-7-skies-unknown-pc-review)
- AC7 campaign praised for mission variety: "The ingenuity of the missions hooked me"; "from the fourth mission onwards, every scenario could feel at home in an action movie". Criticised: weak first missions, missions repeated "ten times or more", "awful checkpointing". — [PCGamesN](https://www.pcgamesn.com/ace-combat-7-skies-unknown-pc-review)
- AC7 critical consensus: OpenCritic top-critic average 81, 79% recommend; Game Informer calls the dogfights "excellently crafted" but missions have a lot of downtime and score challenges feel like filler; PC Gamer calls flying "uncomplicated but gratifying". — [OpenCritic](https://opencritic.com/game/7116/score), [Game Informer](https://www.gameinformer.com/review/ace-combat-7-skies-unknown/a-worthy-throwback?amp), [PC Gamer](https://pcgamer.com/ace-combat-7-skies-unknown-review)
- Project Wingman (Sector D2, Unreal Engine 4, published by Humble Games): PC Gamer 78/100, "A fun and detailed aerial combat game (but don't come to it looking for a sim)"; described as "pulpy, cinematic dogfighting" as a "superhuman mercenary ace". The reviewer wished for better physics, smarter AI allies and more varied objectives. — [PC Gamer review](https://pcgamer.com/project-wingman-review), [Wikipedia](https://www.wikipedia.org/wiki/Project_Wingman)
- Project Wingman's developer takes real-world effects and gives them "an exaggerated game design function" (example: rain on the canopy). — [PCGamesN](https://pcgamesn.com/project-wingman/unreal-engine-4-flight-simulator)
- Project Wingman was built largely by a very small team (developer Abi Rahmani focused on PC "due to the lack of manpower"). — [Memeburn interview](https://memeburn.com/gearburn/2018/01/interview-project-wingman-dev/)
- War Thunder Air Arcade: aircraft have "boosted mobility and weapons targeting characteristics", abrupt manoeuvres cannot cause spins or structural failure ("No stalling"), expended ammunition is replenished in flight, unlimited spawns (up to 20 in one battle with a full crew), lead marker shown, spotted enemies marked on screen and minimap. — [War Thunder wiki: Arcade Battles](https://wiki.warthunder.com/arcade_battles)
- War Thunder Realistic: no lead markers; one aircraft, no respawn; marker visibility depends on view, cloud, sun glare. — [War Thunder wiki: Realistic Battles](https://wiki.warthunder.com/realistic_battles), [Arcade Battles](https://wiki.warthunder.com/arcade_battles)
- Developer framing: Realistic is not "superior"; it "better fits the fantasy of a military simulation" while Arcade fits direct engagement. — [War Thunder wiki](https://wiki.warthunder.com/gamemode/realistic_battles)

**Hybrid**
- Nuclear Option (Shockfront, Early Access): "realistic flight dynamics, with stalls, spins and g-force effects fully implemented"; up to 50 simulated physics parts per aircraft, and part damage changes how it flies; flyable with a mouse "virtual joystick"; player-hosted multiplayer, co-op or competitive, PvP or PvE; IR flares and radar jammer; 95% positive of 9,083 Steam reviews ("Overwhelmingly Positive"). — [Steam store](https://store.steampowered.com/app/2168680)
- PC Gamer: Nuclear Option "rides the line between arcade combat and flight sim in a very good way" ("accessible near-future flight combat sim"). — [PC Gamer](https://pcgamer.com/fly-hard-fly-fast-drop-nukes-in-this-accessible-near-future-flight-combat-sim)
- Tiny Combat Arena developer (Why485): "Realism is way overrated." "I think it's far more important that your game feels realistic, rather than actually being realistic." Flight model is physically based "necessarily so because of VTOL and damage", but flying, combat and sensors are simplified. — [Skyward FM interview pt 2](https://www.skywardfm.com/post/interview-why485-developer-of-tiny-combat-arena-part-2)
- Sky Rogue (2017 roguelite): OpenCritic ~75; dogfighting called "superb" and "high-octane and tight"; offers an arcade mode for newcomers and a realistic mode that rewards mastery; criticised as repetitive and punishing without difficulty options. — [OpenCritic](https://opencritic.com/game/5017/sky-rogue), [Nintendo Life](https://nintendolife.com/reviews/switch-eshop/sky_rogue), [Bonus Stage](https://www.bonusstage.co.uk/archives/120964)

**Star Fox-style all-range**
- Star Fox 64's "All-Range Mode" is free 3D movement in a bounded arena, with a U-turn manoeuvre; multiplayer and most bosses use it. — [Wikipedia](https://en.wikipedia.org/wiki/Star_Fox_64)
- A Star Fox remake (Velan Studios / Nintendo, Switch 2, 25 June 2026) adds one-button somersault and U-turn, up to 8-player multiplayer with objective maps (zone capture; crystal collection; 4v4 cargo delivery against AI pirates) and bots that fill empty slots ("surprisingly competent"). — [Game Informer preview, May 2026](https://gameinformer.com/preview/2026/05/30/pretty-smooth-flying)

### Inferences
- Audience values: arcade players want spectacle, many kills, cinematic missions and low failure cost; hybrid players want planes that behave plausibly (stall, energy, damage that changes handling) but are flyable with mouse/gamepad; sim players want procedural depth (systems, sensors). For a boat game adding jets, the arcade-to-light-hybrid band is the right target.
- The PCGamesN AC7 complaint ("so easy to keep your plane in the air") suggests a small amount of real risk (stall, terrain, energy) is part of the thrill even in arcade.

### Gaps
- No primary-source detail fetched for DCS or VTOL VR (both full sims; VTOL VR is VR-only with hands-on cockpit). Treat them as out of scope for a 4-player LAN arcade mode.
- Vector Thunder: no sources found.
- No developer GDC talk/postmortem for Ace Combat or Project Wingman was found in this session.

## 2. Weapon loop design: lock-on, missile tracking/evasion, guns, ammo

### Takeaway
Arcade games use large lock cones, lots of fast missiles and infinite guns; the skill lies in *firing angle* (behind/at screen edge) and in *evading by sustained turns* rather than countermeasure spam. Hybrids make missiles fewer, slower, shorter-ranged and each a bigger threat, so they can be beaten kinematically. Flares work best when combined with a hard turn and reduced engine heat.

### Cited Findings
**Ace Combat 7 (multiplayer guide)**
- Most missiles have a lock cone "bigger than your screen"; QAAMs extend "way past the aircraft's nose". LAAMs are "Useless within 1km". — [AC7 MP guide (rentry)](https://rentry.org/acegMPguide)
- Missiles "will almost never hit from the side unless you're going under 600 kph"; best to fire when the target is at/beyond the screen edge or when "you can see up the target's engines". — [rentry guide](https://rentry.org/acegMPguide)
- Evasion: keep turning the same way; "Reversing or barrel rolling helps the missile track"; in a head-on, pitch down to evade. — [rentry guide](https://rentry.org/acegMPguide)
- QAAMs keep tracking for ten seconds (fifteen with an upgrade); a QAAM user fires "one every 4.5 seconds"; EML has a 7 s reload. A QAAM "will oneshot most planes without Armor 2"; a 6AAM single hit kills an unarmoured target; SASM does half a standard missile's damage. — [rentry guide](https://rentry.org/acegMPguide)
- Gun: infinite ammunition, effective to about 1,700 m (beyond the gunsight's display); lead "roughly twice" what the reticle suggests. Pulse laser bolts are twice gun-round speed, 5 km range unupgraded, stopped by clouds. — [rentry guide](https://rentry.org/acegMPguide)
- 4AAM carries four at a time; a MiG-29 build with "22 HPAAs" is "just barely enough" for a normal time limit (shows special-weapon scarcity as a pacing lever). — [rentry guide](https://rentry.org/acegMPguide)
- AC7 parts upgrade standard-missile lock range (Lv1 "increases", Lv2 "greatly increases"); 8AAM locks up to eight targets; 6AAM struggles against agile targets; LAAM is the longest-range special weapon, fired in pairs in AC7. — [AC7 parts (wiki.gg)](https://acecombat.wiki.gg/wiki/Ace_Combat_7:_Skies_Unknown/Parts), [8AAM](https://acecombat.wiki.gg/wiki/8_Target_Air-to-Air_Missile), [6AAM](https://acecombat.wiki.gg/wiki/6AAM), [LAAM](https://acecombat.wiki.gg/wiki/Long-Range_Air-to-Air_Missile)
- Stealth: firing any weapon (even guns) shows you on radar for a second. — [rentry guide](https://rentry.org/acegMPguide)
- AC7 in canyons: "The missiles travel quickly, which is essential when you're dogfighting in the open sky but work against me in this maze of narrow passages"; enemies use rock spires "as cover from my missile lock"; the cannon needs you to "practically tailgate the enemy". — [PCGamesN](https://www.pcgamesn.com/ace-combat-7-skies-unknown-pc-review)
- Sentiment split on missile dominance: one critic says most of your time is spent firing standard missiles; another says you "can't just rely on spamming missiles". — [search summary of reviews incl. inkl/Forbes](https://www.inkl.com/news/ace-combat-7-review-it-s-time-to-return-to-the-intense-world-of-strangereal-flight-combat)

**Hybrid philosophy (Tiny Combat Arena)**
- "Missile ranges will be shorter than real life, and missiles slower so that they're easier to see and evade kinematically"; fewer missiles than arcade games but each a bigger threat; emphasis on "traditional dogfighting, and close range gun kills". — [Skyward FM interview](https://www.skywardfm.com/post/interview-why485-developer-of-tiny-combat-arena-part-2)
- "In real life, missiles are affected by gravity, their motors burn out, and they lose speed when they turn." — [same](https://www.skywardfm.com/post/interview-why485-developer-of-tiny-combat-arena-part-2)

**Countermeasures and evasion (War Thunder)**
- Heat is based on thrust: higher throttle needs more flares; cutting afterburner "significantly decreases" IR signature. Modern IR seekers are less fooled. — [WT wiki: flares](https://wiki.warthunder.com/mechanics/5026-aerial-infrared-countermeasures-flares)
- Evasion: flare *and* turn sharply out of the seeker's view; early heat-seekers beaten by out-turning; a hard turn then reversal makes a missile bleed energy so it cannot follow the next change; low head-on: nose down so the missile's intercept point is underground; flying low loses missiles in ground clutter; notching works against pulse-Doppler radars. "You may not get a visual warning (depending on missile type)." — [WT wiki: avoiding missiles](https://wiki.warthunder.com/212-avoiding-air-to-air-missiles)
- Markerless modes also hide missile launch markers and text warnings. — [WT search summary / dev post](https://warthunder.com/en/news/6761-new-detection-system-for-missile-launch-en?page=9)
- Players debate both directions: "Are flares too powerful?" and "Does missile lethality need a rethink" threads exist; chaff effectiveness is disputed (some say "chaff does absolutely nothing"). Anecdotal. — [WT forum: flares too powerful](https://forum.warthunder.com/t/are-flares-too-powerful/261161), [WT forum: missile lethality](https://forum.warthunder.com/t/does-missile-lethality-need-a-rethink/253441)

**Nuclear Option**
- Countermeasures are IR flares and a radar jammer (not chaff). — [Steam](https://store.steampowered.com/app/2168680)

### Inferences
- A clean arcade loop for 2–4 players: infinite gun with lead marker + a small standard missile magazine that regenerates + one scarce special weapon. Missiles should hit from behind and miss from the side at speed, so positioning (getting on someone's six) matters more than lock spam.
- Make a sustained hard turn the primary missile counter, with flares as a limited, cooldown-gated backup that needs a turn to work. This avoids both "missiles too strong" and "flare spam".
- Short missile range and visible, slower missiles fit a small LAN arena and keep kills readable ("never fun dying ... without knowing what happened").
- The project already has homing missiles with lock-over-a-second, flares, and a deck gun with aim assist and heat (per CLAUDE.md); the jet version can reuse these patterns.

### Gaps
- No sourced numeric lock times, missile speeds, or gun rates of fire for any title. (Unverified, from background knowledge: AC7 planes typically carry dozens of standard missiles with two on the rails reloading in a few seconds; check before use.)
- No sourced flare counts per aircraft.

## 3. Flight feel: stall, energy, high-G, post-stall, assisted vs expert controls

### Takeaway
Arcade games keep stall and energy as soft, learnable risks rather than hard failures; assistance layers (H.A.W.X. ERS, War Thunder's mouse-aim "Instructor") protect newcomers, and an optional "assistance off" mode unlocks extreme moves with real risk. Reviewers single out H.A.W.X.'s Assistance Off as its stand-out feature.

### Cited Findings
- AC7: stay in your plane's ideal cornering speed; flying slow makes you easy prey. Stalling pulls the nose down quickly and a stalled plane is "a sitting duck". Post-stall manoeuvre (PSM): about 400–500 kph, throttle up, brake + pitch up; pitch down cancels; can be chained under 500 kph. Dive to gain speed, climb to lose it; high-G turns when below cornering speed. Parts: one lowers stall speed, another increases deceleration. — [rentry guide](https://rentry.org/acegMPguide)
- H.A.W.X. ERS: assistance layer makes it "harder to stall the aircraft because assistance systems are running to help stabilize flight" and adds damage reduction; ERS Tunnel draws a path of triangular gates to intercept targets or outmanoeuvre missiles (late activation can still fail). — [Skyward FM: ERS](https://www.skywardfm.com/post/enhanced-reality-system-augmented-reality-flight-assists-in-hawx)
- H.A.W.X. Assistance Off: "reserved for only the most deft pilots willing to take risks"; allows "drifting"; camera defaults to a zoomed-out third-person view; radar hidden; no path prediction; no damage reduction ("One or two well placed enemy missiles is enough to destroy the player's aircraft"). — [Skyward FM](https://www.skywardfm.com/post/enhanced-reality-system-augmented-reality-flight-assists-in-hawx)
- Reviews: Assistance Off "disables all of your jet's built in safety features, allowing some ferocious and dangerous manoeuvres" ([Den of Geek](https://www.denofgeek.com/?p=395729)); "it feels like you're playing a different game" ([HEXUS](https://hexus.net/gaming/reviews/xbox-360/17564-tom-clancys-hawx-xbox-360-ps3/?page=2)); assistance on comes with reduced manoeuvrability ([GamesRadar hands-on](https://www.gamesradar.com/tom-clancys-hawx-hands-on)); ERS makes parts "significantly easier", hence the off option ([GameSpot](https://gamespot.com/reviews/tom-clancys-hawx-review/1900-6205791/)); advanced players rarely use ERS except for steep bombing runs and evading missiles ([CheatCC](https://www.cheatcc.com/?p=73396)).
- War Thunder's Instructor: the player points with the mouse where to fly, the Instructor flies the aircraft, and one function is "to prevent the wing from moving to a critical angle of attack, thus preserving lift". — [War Thunder search summary of wiki](https://wiki.warthunder.com/Instructor/How_the_instructor_works) (page 404'd on direct fetch; quoted from search snippet)
- Nuclear Option offers stalls, spins and g-force effects but stays accessible via a mouse virtual joystick. — [Steam](https://store.steampowered.com/app/2168680)
- Star Fox remake: one-button somersault and U-turn make the signature manoeuvres accessible. — [Game Informer](https://gameinformer.com/preview/2026/05/30/pretty-smooth-flying)

### Inferences
- Recommended model for the boat game: default "assisted" mode (auto-level, AoA limiter preventing stalls, terrain warning), plus an optional "expert" toggle that removes the limiter, enables a drift/post-stall button and widens the camera. Map canned manoeuvres (U-turn, loop) to single buttons for gamepad players.
- Energy should be legible: speed loss in climbs and hard turns, gain in dives; boost/afterburner as the manager.

### Gaps
- No sourced numbers for turn rates, top speeds or G limits in any arcade title (AC7 aircraft stat bars are relative, not physical units). No blackout/redout mechanic details found for arcade games.

## 4. Camera and HUD

### Takeaway
Third-person chase is the arcade default; a wider pulled-back camera suits high-manoeuvre "expert" flying. The essentials are a lead marker for guns, on-screen/minimap markers for spotted enemies, missile warnings, and clear cause-of-death feedback.

### Cited Findings
- H.A.W.X. Assistance Off zooms "the camera way out for a more cinematic approach" and removes the course tracker. — [Escapist](https://www.escapistmagazine.com/?p=51573)
- War Thunder Arcade shows a lead marker computed from target trajectory and marks spotted enemies on screen and minimap; Realistic removes the lead marker. — [WT wiki Arcade](https://wiki.warthunder.com/arcade_battles)
- AC7 gunsight under-shows effective range (gun reaches 1,700 m) and under-leads by about half, which experienced players have to learn around. — [rentry guide](https://rentry.org/acegMPguide)
- "It's never fun dying in a game without knowing what happened, or if it was even your fault" — wants clear HUD cues, visual cues and warnings. — [Skyward FM interview](https://www.skywardfm.com/post/interview-why485-developer-of-tiny-combat-arena-part-2)
- H.A.W.X. assistance off leaves only "minimal missile warning cues". — [Skyward FM ERS](https://www.skywardfm.com/post/enhanced-reality-system-augmented-reality-flight-assists-in-hawx)

### Inferences
- HUD set for this project (which already draws a compass tape and waypoint): lead pipper, target box with lock progress ring and tone, off-screen arrows to the nearest enemy and to incoming missiles, missile-warning tone that rises as the missile closes, and a kill-cam/cause-of-death line. Make the lead marker accurate (avoid AC7's under-lead).
- Optional "look at target" button (padlock camera) helps players track a target after overshooting.

### Gaps
- No sources gathered on cockpit vs chase preference statistics or target-lock camera design specifics.

## 5. Game modes for 2–4 player LAN, respawn and match length

### Takeaway
Small-count dogfighting works best with a bounded arena, simple goals (timed kills or point target), quick respawns and optional AI bots to fill slots or provide co-op targets. Objective modes (zones, collectibles, cargo) add variety.

### Cited Findings
- Star Fox 64 Versus: 2–4 players in an all-range arena, modes include a timed kill mode and a point match; reviewers split between "feels like an afterthought" (open, empty arenas) and praise for its simple single goal. — [search summary incl. Wikipedia/reviews](https://en.wikipedia.org/wiki/Star_Fox_64)
- Star Fox (2026): up to 8 players; zone capture, crystal collection, and 4v4 team cargo delivery while fighting AI pirates; bots fill empty slots. — [Game Informer](https://gameinformer.com/preview/2026/05/30/pretty-smooth-flying)
- War Thunder Air Arcade modes: Ground Strike (destroy ground units/bases to drain tickets), Air Domination (hold a central zone by having more planes in it), Domination (capture airfields by landing), Air Conquest (mixed); unlimited respawns. — [WT wiki Arcade](https://wiki.warthunder.com/arcade_battles)
- AC7 multiplayer: scoring gives up to 70% of a kill's value for damage dealt and 30% for the kill itself (rewards assists); lobbies run to a time limit; battle royal with about 8 players awards stars to the top three. — [rentry guide](https://rentry.org/acegMPguide)
- AC7 MP hit registration is client-side, sent to host; latency causes desync. — [rentry guide](https://rentry.org/acegMPguide)
- Nuclear Option supports co-op and competitive, PvP and PvE, player-hosted. — [Steam](https://store.steampowered.com/app/2168680)

### Inferences
- For Boat Race's 2–4 LAN: (a) free-for-all timed dogfight (e.g. 5 min) with damage-share scoring; (b) 2v2; (c) co-op versus AI waves; (d) ring/canyon air race reusing the existing course and gate system; (e) mixed mode where jets attack the boat race. Respawn after a few seconds, away from enemies, with brief invulnerability.
- Split damage credit avoids kill-steal frustration in small lobbies.
- The server-decides-damage rule already in the project avoids AC7's client-side desync complaints.

### Gaps
- No sourced standard match lengths or respawn timers for any title (AC7 time limits are lobby options; values not found).

## 6. What reviewers and players find most fun and most frustrating

### Takeaway
Fun: speed, chasing an enemy through a dive or canyon, the "glee" of escaping as a target burns, cinematic scenarios, and assistance-off freedom. Frustration: too easy/no challenge, repetition, bad checkpoints, deaths without understanding, missiles that dominate or flares that negate them, and netcode desync.

### Cited Findings
- Fun: "There's a glee in wheeling away to safety, your target a burning wreckage behind you"; "something about following an enemy plane into a steep dive". — [PCGamesN AC7](https://www.pcgamesn.com/ace-combat-7-skies-unknown-pc-review)
- Fun: Star Fox remake's "tight gameplay ... and frenetic action"; "I can't wait to spend hours matchmaking in this mode". — [Game Informer](https://gameinformer.com/preview/2026/05/30/pretty-smooth-flying)
- Fun: Sky Rogue makes you "feel like an ace pilot"; randomised missions keep it fresh. — [The Xbox Hub](https://www.thexboxhub.com/sky-rogue-review/), [Nintendo Life](https://nintendolife.com/reviews/switch-eshop/sky_rogue)
- Fun: H.A.W.X. Assistance Off adds "variety and more of a challenge". — [HEXUS](https://hexus.net/gaming/reviews/xbox-360/17564-tom-clancys-hawx-xbox-360-ps3/?page=2)
- Frustrating: "There's little challenge"; repeated missions; checkpoints. — [PCGamesN](https://www.pcgamesn.com/ace-combat-7-skies-unknown-pc-review)
- Frustrating: Project Wingman reviewer wanted smarter AI allies and more varied objectives. — [PC Gamer](https://pcgamer.com/project-wingman-review) (via search summary; page truncated)
- Frustrating: Sky Rogue repetitive, punishing without difficulty settings. — [OpenCritic summary](https://opencritic.com/game/5017/sky-rogue)
- Frustrating: dying without knowing why. — [Skyward FM](https://www.skywardfm.com/post/interview-why485-developer-of-tiny-combat-arena-part-2)
- Frustrating/contested: flare strength and missile lethality (WT forum threads); AC7 level-3 missiles called "completely broken" by one Metacritic user. — [WT forum](https://forum.warthunder.com/t/are-flares-too-powerful/261161), [Metacritic user reviews](https://legacy.prod.metacritic.com/game/playstation-4/ace-combat-7-skies-unknown/user-reviews)

### Inferences
- Design priorities for an original mode: keep flight easy but not risk-free; make missile avoidance a skill with visible missiles; give a reason to use the gun; tune AI to be beatable but present; give clear feedback on every hit and death; use terrain (canyons, sea stacks the game already has) as cover that breaks locks.

### Gaps
- Reddit/Steam sentiment threads were not fetched directly (budget); sentiment above relies on reviews and War Thunder forum thread titles.
