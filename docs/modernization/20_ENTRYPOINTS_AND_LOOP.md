# Entrypoints and Runtime Loop Mapping

## High-level overview

The executable starts in the Windows entrypoint `WinMain` (`Source/diablo.cpp`), initializes engine/UI systems, then enters `mainmenu_loop` (`Source/mainmenu.cpp`).

Starting a game from the menu calls `StartGame` (`Source/diablo.cpp`), which sets up networking/session state and dispatches into `run_game_loop` (`Source/diablo.cpp`).

`run_game_loop` is the top-level in-game loop: it pumps OS messages, advances networking and simulation ticks, and renders via `DrawAndBlit` (`Source/scrollrt.cpp`).

Per simulation tick, `game_loop` (`Source/diablo.cpp`) gates on multiplayer delta sync (`multi_handle_delta`) and then calls `game_logic` (`Source/diablo.cpp`), which executes core subsystems in a fixed order.

## Entry points (with file paths)

Primary process/game flow entrypoints:

1. **Process entry**: `int APIENTRY WinMain(...)` in `Source/diablo.cpp`.
   - Initializes process/window/UI/audio state.
   - Plays intro/title flow.
   - Transfers control to `mainmenu_loop()`.

2. **Menu runtime**: `void mainmenu_loop()` in `Source/mainmenu.cpp`.
   - Runs menu dialog loop (`UiMainMenuDialog`).
   - Routes game-start choices to `mainmenu_single_player` / `mainmenu_multi_player`.
   - These call `mainmenu_init_menu`, which invokes `StartGame(...)`.

3. **Session start**: `BOOL StartGame(BOOL bNewGame, BOOL bSinglePlayer)` in `Source/diablo.cpp`.
   - Initializes/joins networking (`NetInit`).
   - Initializes level/quest/portal state for new sessions.
   - Chooses startup message (`WM_DIABNEWGAME` / `WM_DIABLOADGAME`).
   - Enters `run_game_loop(uMsg)`.

4. **In-game window/event entry**: `LRESULT CALLBACK GM_Game(...)` in `Source/diablo.cpp`.
   - Active game window procedure installed by `run_game_loop` via `SetWindowProc(GM_Game)`.
   - Consumes keyboard/mouse/system messages and mutates top-level run flags (`gbRunGame`, `gbRunGameResult`) and frame startup state (`gbGameLoopStartup`) on level-transition messages.

5. **Top-level game loop**: `static void run_game_loop(unsigned int uMsg)` in `Source/diablo.cpp`.
   - Performs game start setup (`start_game`).
   - Runs message pump + simulation + render loop while `gbRunGame` is true.

## Frame execution order

Observed order inside the live loop (`run_game_loop`):

1. `diablo_color_cyc_logic()`.
2. Pump/process pending Windows messages (`PeekMessage` / `TranslateMessage` / `DispatchMessage`).
3. Tick gate using `nthread_has_500ms_passed(FALSE)`.
4. `multi_process_network_packets()`.
5. `game_loop(gbGameLoopStartup)`.
6. (Non-Hellfire) `msgcmd_send_chat()`.
7. `gbGameLoopStartup = FALSE`.
8. `DrawAndBlit()`.

Within `game_loop(BOOL bStartup)` (`Source/diablo.cpp`):

- Runs up to `60` startup iterations or `3` normal iterations.
- For each iteration:
  1. `multi_handle_delta()`; on failure, show timeout cursor and stop tick loop.
  2. `game_logic()`.
  3. Break early if not running, single-player, or net tick budget says stop (`nthread_has_500ms_passed(TRUE)`).

Within `game_logic()` (`Source/diablo.cpp`), per successful logic tick:

1. Acquire `GameState` facade (`GetGameState`) and increment `gdwGameLogicTick`.
2. Pause/menu guards (`PauseMode`, `gmenu_is_active`).
3. Cursor/input tracking (`CheckCursMove`, `track_process`) when menu/timeout allows.
4. Player simulation: `ProcessPlayers()` (guarded by `gbProcessPlayers`).
5. World simulation (branch by level type):
   - **Non-town**: `ProcessMonsters()`, `ProcessObjects()`, `ProcessMissilesGameState(state)`, `ProcessItems()`, `ProcessLightList()`, `ProcessVisionList()`.
   - **Town**: `ProcessTowners()`, `ProcessItems()`, `ProcessMissilesGameState(state)`.
6. Audio/UI/event checks: `sound_update()`, `ClearPlrMsg()`, `CheckTriggers()`, `CheckQuests()`.
7. Mark redraw (`force_redraw |= 1`) and persist player file state (`pfile_update(FALSE)`).

## State ownership overview

Major global-state clusters and key mutation sites:

1. **Top-level runtime/session flags** (`Source/diablo.h`, `Source/diablo.cpp`)
   - Examples: `gbRunGame`, `gbRunGameResult`, `gbProcessPlayers`, `gbLoadGame`, `force_redraw`, `PauseMode`, `sgbMouseDown`, `gbGameLoopStartup`.
   - Mutated in: `start_game`, `run_game_loop`, `GM_Game`, `game_logic`, and transition/message handlers in `Source/diablo.cpp`.

2. **Entity-state arrays (players/monsters/items/objects/missiles)**
   - Declarations in headers:
     - Players: `plr`, `myplr` (`Source/player.h`)
     - Monsters: `monster`, `monstactive`, `nummonsters` (`Source/monster.h`)
     - Items: `item`, `itemactive`, `numitems` (`Source/items.h`)
     - Objects: `object`, `objectactive`, `nobjects` (`Source/objects.h`)
     - Missiles: `missile`, `missileactive`, `nummissiles` (`Source/missiles.h`)
   - Mutated primarily by their per-tick processors:
     - `ProcessPlayers` (`Source/player.cpp`)
     - `ProcessMonsters` (`Source/monster.cpp`)
     - `ProcessItems` (`Source/items.cpp`)
     - `ProcessObjects` (`Source/objects.cpp`)
     - `ProcessMissilesGameState` (`Source/missiles.cpp`)

3. **Lighting/vision and map-visibility state**
   - Declarations: `LightList`, `VisionList`, `lightactive`, `numlights`, `numvision`, `dolighting`, `dovision` (`Source/lighting.h`).
   - Mutated by: `ProcessLightList`, `ProcessVisionList`, `ChangeLight*`, `ChangeVision*` (`Source/lighting.cpp`).

4. **World/camera/dungeon globals (level and viewport positioning)**
   - Declarations include `currlevel`, `ViewX`, `ViewY` (`Source/gendung.h`).
   - Referenced through `GameState` facade (`Source/gamestate.h` / `Source/gamestate.cpp`), initialized in `InitGameState` and read by logic/render systems.
   - Mutated across systems, including level load/transition flows and gameplay modules (e.g., transition handling in `GM_Game`, quest/teleport/level-generation paths).

5. **Network synchronization state**
   - Tick/data flow updated via `multi_process_network_packets` and `multi_handle_delta` (`Source/multi.cpp`).
   - These mutate player/network synchronization data and can terminate runtime loop state (e.g., setting `gbRunGame = FALSE` on timeout/destruction paths).

## Observed risks or ambiguity

### Observed risks

1. **Tight coupling via writable globals**
   - Runtime, world, and entity state is shared globally across many translation units, so ordering assumptions are implicit and fragile.

2. **Dual-loop timing complexity**
   - Outer loop (`run_game_loop`) and inner tick loop (`game_loop`) both apply timing/network gates, creating non-trivial behavior under lag or heavy message traffic.

3. **Window-proc side effects during simulation**
   - `GM_Game` can asynchronously mutate run flags and transition state while main loop/tick processing is active.

### Ambiguity requiring deeper inspection

1. **Exact authoritative owner per global field**
   - `GameState` is a facade over pointers to globals, not ownership transfer; true ownership remains distributed by subsystem.

2. **Per-frame determinism boundaries in multiplayer**
   - The interaction between `multi_process_network_packets`, `multi_handle_delta`, and subsystem order is clear structurally, but deterministic guarantees need deeper, packet-level and delta-serialization review.

3. **Level-transition sub-loop behavior**
   - Transition messages (`WM_DIAB*`) trigger fade/load/update work in `GM_Game`; full sequencing with respect to current frame rendering and simulation requires trace-level instrumentation.
