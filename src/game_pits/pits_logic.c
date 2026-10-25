// ============================================================================
// The Puzzle Pits engine for Rocks'n'Diamonds
// ----------------------------------------------------------------------------
// Written from the description of the game in PITS-RULES.md and of its level
// files in PIT-FORMAT.md.
// ----------------------------------------------------------------------------
// pits_logic.c: the rules of the game, its turns and its scripts
// ============================================================================

#include "pits_main.h"


#define SCRIPT_MAX_STEPS		200000	// against scripts that never end
#define SCRIPT_MAX_LOOPS		32

// script commands
#define CMD_END				0
#define CMD_ENDIF			1
#define CMD_NEXT			2
#define CMD_ELSE			3
#define CMD_SWITCHOFF			4
#define CMD_SWITCHON			5
#define CMD_IF_EQ			6
#define CMD_IF_NE			11
#define CMD_IF_INPUT			12
#define CMD_FOR				13
#define CMD_REDRAW			14
#define CMD_APPEAR			15
#define CMD_STOP			16
#define CMD_WIN				17
#define CMD_TARGET			18
#define CMD_REDGEMOFF			19
#define CMD_GREENGEMOFF			20
#define CMD_BLUEGEMOFF			21
#define CMD_POPUP			22
#define CMD_LET				23
#define CMD_ADD				24
#define CMD_SUB				25
#define CMD_MUL				26
#define CMD_DIV				27
#define CMD_SWAPOVER			28
#define CMD_SWAPUNDER			29
#define CMD_GOTO			30
#define CMD_CHANGEMAP			31
#define CMD_PRINT			32
#define CMD_INPUT			33

// script values
#define VAL_REGISTER			201
#define VAL_XGUY			211
#define VAL_YGUY			212
#define VAL_LASTX			213
#define VAL_LASTY			214
#define VAL_FACING			215
#define VAL_REDGEMS			216
#define VAL_GREENGEMS			217
#define VAL_BLUEGEMS			218
#define VAL_YELLOWGEMS			219
#define VAL_REDKEYS			220
#define VAL_GREENKEYS			221
#define VAL_BLUEKEYS			222
#define VAL_INPUTVALUE			223
#define VAL_XPOS			224
#define VAL_YPOS			225
#define VAL_MOVES			226
#define VAL_HECTOMOVES			227
#define VAL_WALKABLE			228
#define VAL_DELTA			229
#define VAL_UNDER			230
#define VAL_OVER			231
#define VAL_LASTPUSHED			232
#define VAL_REDGEMON			233
#define VAL_GREENGEMON			234
#define VAL_BLUEGEMON			235

static int dir_dx[4] = {  0, 1, 0, -1 };
static int dir_dy[4] = { -1, 0, 1,  0 };

// a value that can be set (a register, or one of the game's)
struct ValueRef
{
  int kind;
  int a, b;
};

// a plate pressed or released in this turn
struct PlateNote
{
  int x, y;
  boolean pressed;
};

static void RunScript(int, boolean, int, int);


// ----------------------------------------------------------------------------
// small helpers
// ----------------------------------------------------------------------------

static int Word16(int value)
{
  return (short)(value & 0xffff);
}

static int Clamp(int value, int min, int max)
{
  return (value < min ? min : value > max ? max : value);
}

static boolean IsGhost(void)
{
  return (pits.gem_in_use == PITS_GREEN &&
	  pits.green_stage == PITS_GHOST_STAGE);
}

boolean HasScript_PITS(int nr)
{
  return (nr >= 0 && nr < PITS_MAX_SCRIPTS &&
	  pits.file.script_start[nr] >= 0);
}

static boolean IsWalkable(int tile)
{
  return (tile >= 0 && tile <= 100 && pits.walkable[tile]);
}

static boolean OverIsFree(int x, int y)
{
  return (pits.over[x][y] == PITS_O_NOTHING || IsWalkable(pits.over[x][y]));
}

boolean CanEnter_PITS(int x, int y)
{
  if (!PITS_IN_ARRAY(x, y))
    return FALSE;

  return (OverIsFree(x, y) && IsWalkable(pits.under[x][y]));
}

static void AddArrow(int x, int y)
{
  // in the original, the list of these squares can't hold column 21
  if (x < PITS_MIN || x >= PITS_MAX || y < PITS_MIN || y > PITS_MAX)
    return;

  if (pits.in_arrows[x][y])
    return;

  pits.arrows_x[pits.num_arrows] = x;
  pits.arrows_y[pits.num_arrows] = y;
  pits.num_arrows++;
  pits.in_arrows[x][y] = TRUE;
}

static void SetUnder(int x, int y, int tile)
{
  pits.under[x][y] = tile;

  if (PITS_IS_ARROW(tile) || tile == PITS_U_GLASS)
    AddArrow(x, y);
}

static void PlaySound_PITS(int sound)
{
  pits.sounds |= (1 << sound);
}


// ----------------------------------------------------------------------------
// the tile rules
// ----------------------------------------------------------------------------

static int PlainObject(int over)
{
  switch (over)
  {
    case PITS_O_BARREL_WATER:
    case PITS_O_BARREL_HOLE:
    case PITS_O_BARREL_HOME:
      return PITS_O_BARREL;

    case PITS_O_CRATE_HOME:
      return PITS_O_CRATE;

    case PITS_O_CHEST_HOME:
      return PITS_O_CHEST;

    default:
      return over;
  }
}

static void ApplyTileRules(int x, int y)
{
  int old_over, old_under, over, under;
  int sound = -1;

  if (!PITS_IN_ARRAY(x, y))
    return;

  old_over = pits.over[x][y];
  old_under = pits.under[x][y];
  over = PlainObject(old_over);
  under = old_under;

  if (over != PITS_O_BARREL && over != PITS_O_CRATE && over != PITS_O_CHEST)
    return;

  if (under == PITS_U_VOID)			// falls
  {
    over = PITS_O_NOTHING;
  }
  else if (PITS_IS_WATER(under))
  {
    if (over == PITS_O_BARREL)
    {
      over = PITS_O_BARREL_WATER;
      sound = PITS_SND_WATER;
    }
    else if (over == PITS_O_CRATE)
    {
      over = PITS_O_NOTHING;
      under = PITS_U_CRATE_WATER;
      sound = PITS_SND_WATER;
    }
  }
  else if (PITS_IS_LAVA(under) || PITS_IS_COOLING(under))
  {
    if (over == PITS_O_CHEST)
    {
      under = PITS_U_GLASS;
      sound = PITS_SND_CRACK;
    }
    else
    {
      under = PITS_U_LAVA_COOLING_1;
      sound = PITS_SND_LAVA;
    }

    over = PITS_O_NOTHING;
  }
  else if (under == PITS_U_HOLE)
  {
    if (over == PITS_O_BARREL)
    {
      over = PITS_O_BARREL_HOLE;
      sound = PITS_SND_SPLINTER;
    }
    else
    {
      if (over == PITS_O_CRATE)
	sound = PITS_SND_SPLINTER;

      over = PITS_O_NOTHING;
    }
  }
  else if (under == PITS_U_WOOD)
  {
    if (over == PITS_O_BARREL)
    {
      over = PITS_O_BARREL_HOLE;
      sound = PITS_SND_SPLINTER;
    }
    else if (over == PITS_O_CRATE)
    {
      over = PITS_O_NOTHING;
      under = PITS_U_HOLE;
      sound = PITS_SND_SPLINTER;
    }
  }
  else if (under == PITS_U_TARGET_BLUE && over == PITS_O_BARREL)
  {
    over = PITS_O_BARREL_HOME;
  }
  else if (under == PITS_U_TARGET_RED && over == PITS_O_CRATE)
  {
    over = PITS_O_CRATE_HOME;
  }
  else if (under == PITS_U_TARGET_GREEN && over == PITS_O_CHEST)
  {
    over = PITS_O_CHEST_HOME;
  }

  pits.over[x][y] = over;

  if (under != old_under)
    SetUnder(x, y, under);

  // a sound only for a change (the rules are applied again every turn)
  if (sound != -1 && (over != old_over || under != old_under))
    PlaySound_PITS(sound);
}

// an object goes from one square to the next
static void MoveObject(int x1, int y1, int x2, int y2, int dir)
{
  pits.over[x2][y2] = pits.over[x1][y1];
  pits.over[x1][y1] = PITS_O_NOTHING;

  ApplyTileRules(x1, y1);
  ApplyTileRules(x2, y2);

  pits.last_pushed[x2][y2] = dir;
  pits.last_pushed[x1][y1] = PITS_NO_DIR;
}

// a square set by a script: its tile rules, and no last pushed
static void ChangedByScript(int x, int y)
{
  if (PITS_IS_ARROW(pits.under[x][y]) || pits.under[x][y] == PITS_U_GLASS)
    AddArrow(x, y);

  ApplyTileRules(x, y);

  pits.last_pushed[x][y] = PITS_NO_DIR;
}


// ----------------------------------------------------------------------------
// counting the targets
// ----------------------------------------------------------------------------

static void CountTargets(int *targets, int *at_home)
{
  int x, y;

  *targets = 0;
  *at_home = 0;

  for (y = 0; y < PITS_SIZE; y++)
  {
    for (x = 0; x < PITS_SIZE; x++)
    {
      if (PITS_IS_TARGET(pits.under[x][y]))
	(*targets)++;

      if (PITS_IS_AT_HOME(pits.over[x][y]))
	(*at_home)++;
    }
  }
}

int getTargetsStillNeeded_PITS(void)
{
  int targets, at_home;

  CountTargets(&targets, &at_home);

  return MAX(0, targets - at_home);
}

void CheckWin_PITS(void)
{
  int targets, at_home;

  CountTargets(&targets, &at_home);

  if (targets > 0 && at_home >= targets)
    pits.won = TRUE;
}


// ----------------------------------------------------------------------------
// the level
// ----------------------------------------------------------------------------

static int FindScriptByName(char *name)
{
  int i;

  for (i = 0; i < PITS_MAX_SCRIPTS; i++)
    if (pits.file.script_start[i] >= 0 &&
	strcasecmp(pits.file.script_name[i], name) == 0)
      return i;

  return -1;
}

void InitLevel_PITS(void)
{
  struct PitsFileInfo *file = &pits.file;
  int x, y, i;

  pits.px = PITS_MIN;
  pits.py = PITS_MIN;

  for (x = 0; x < PITS_SIZE; x++)
  {
    for (y = 0; y < PITS_SIZE; y++)
    {
      pits.under[x][y] = file->under[x][y];
      pits.over[x][y] = file->over[x][y];
      pits.last_pushed[x][y] = PITS_NO_DIR;
      pits.in_arrows[x][y] = FALSE;

      if (pits.over[x][y] == PITS_O_PLAYER)
      {
	pits.over[x][y] = PITS_O_NOTHING;
	pits.px = x;
	pits.py = y;
      }
    }
  }

  pits.lastx = pits.px;
  pits.lasty = pits.py;
  pits.facing = PITS_SOUTH * 4;

  for (i = 0; i <= 100; i++)
    pits.walkable[i] = (i >= PITS_U_FLOOR && i <= PITS_U_GRASS);

  pits.moves = 0;

  for (i = 0; i < 4; i++)
    pits.gems[i] = 0;
  for (i = 0; i < 3; i++)
    pits.keys[i] = 0;

  pits.gem_in_use = PITS_NO_GEM;
  pits.green_stage = 0;

  for (i = 0; i < PITS_NUM_REGISTERS; i++)
    pits.reg[i] = 0;

  pits.xpos = 0;
  pits.ypos = 0;
  pits.input[0] = '\0';
  pits.input_value = 0;

  pits.num_arrows = 0;

  for (y = PITS_MIN; y <= PITS_MAX; y++)
    for (x = PITS_MIN; x <= PITS_MAX; x++)
      if (PITS_IS_ARROW(pits.under[x][y]) || pits.under[x][y] == PITS_U_GLASS)
	AddArrow(x, y);

  pits.won = FALSE;

  pits.script_intro  = FindScriptByName("Intro");
  pits.script_master = FindScriptByName("Master");
  pits.script_gem[PITS_RED]   = FindScriptByName("RedGem");
  pits.script_gem[PITS_GREEN] = FindScriptByName("GreenGem");
  pits.script_gem[PITS_BLUE]  = FindScriptByName("BlueGem");
}

static void PickUp(void)
{
  int x = pits.px, y = pits.py;
  int under = pits.under[x][y];

  if (IsGhost())
    return;

  if (under >= PITS_U_GEM_RED && under < PITS_U_GEM_YELLOW)
    pits.gems[under - PITS_U_GEM_RED]++;
  else if (under == PITS_U_GEM_YELLOW)
    pits.gems[PITS_YELLOW] = 1;
  else if (under >= PITS_U_KEY_RED && under <= PITS_U_KEY_BLUE)
    pits.keys[under - PITS_U_KEY_RED]++;
  else
    return;

  SetUnder(x, y, PITS_U_FLOOR);
}

void StartLevel_PITS(void)
{
  if (pits.script_intro >= 0)
    RunScript(pits.script_intro, TRUE, 0, 0);

  if (pits.script_master >= 0)
    RunScript(pits.script_master, TRUE, 0, 0);

  PickUp();
}


// ----------------------------------------------------------------------------
// drawing's side effects: lava cooling, the frames moving on
// ----------------------------------------------------------------------------

void AnimStep_PITS(void)
{
  int vx0, vy0, ox, oy, cols, rows, scale;
  int i, j;

  getViewGeometry_PITS(pits.px, pits.py, &vx0, &vy0, &ox, &oy, &cols, &rows,
		       &scale);

  for (j = 0; j < rows; j++)
  {
    for (i = 0; i < cols; i++)
    {
      int x = vx0 + i, y = vy0 + j;
      int frame = (pits.anim_counter + j * cols + i) & 3;

      if (!PITS_IN_ARRAY(x, y) || frame != 1)
	continue;

      if (pits.under[x][y] == PITS_U_LAVA_COOLING_1)
	pits.under[x][y] = PITS_U_LAVA_COOLING_2;
      else if (pits.under[x][y] == PITS_U_LAVA_COOLING_2)
	pits.under[x][y] = PITS_U_LAVA;
    }
  }

  pits.anim_counter = (pits.anim_counter + cols * rows) & 3;

  if (cols * rows > 0)
    pits.sparkle = (pits.sparkle + 17) % (cols * rows);
}


// ----------------------------------------------------------------------------
// the turn's checks
// ----------------------------------------------------------------------------

static void MoveByArrowsAndGlass(void)
{
  static boolean arrived[PITS_SIZE][PITS_SIZE];
  int i, x, y;

  for (x = 0; x < PITS_SIZE; x++)
    for (y = 0; y < PITS_SIZE; y++)
      arrived[x][y] = FALSE;

  // the list can grow while it is gone through (scripts don't run here)
  for (i = 0; i < pits.num_arrows; i++)
  {
    int under, dir, nx, ny;
    boolean on_glass;

    x = pits.arrows_x[i];
    y = pits.arrows_y[i];
    under = pits.under[x][y];
    on_glass = (under == PITS_U_GLASS);

    if (pits.over[x][y] < PITS_O_BARREL || arrived[x][y])
      continue;

    if (PITS_IS_ARROW(under))
      dir = under - PITS_U_ARROW_UP;
    else if (on_glass)
      dir = pits.last_pushed[x][y];
    else
      continue;

    if (dir < PITS_NORTH || dir > PITS_WEST)
    {
      pits.last_pushed[x][y] = PITS_NO_DIR;

      continue;
    }

    nx = x + dir_dx[dir];
    ny = y + dir_dy[dir];

    if (PITS_IN_ARRAY(nx, ny) &&
	(nx != pits.px || ny != pits.py || IsGhost()) &&
	OverIsFree(nx, ny) &&
	pits.under[nx][ny] != PITS_U_GRASS)
    {
      MoveObject(x, y, nx, ny, dir);

      arrived[nx][ny] = TRUE;

      PlaySound_PITS(on_glass ? PITS_SND_GLASS : PITS_SND_MAGIC);
    }
    else if (on_glass)
    {
      pits.last_pushed[x][y] = PITS_NO_DIR;
    }
  }
}

void TurnChecks_PITS(void)
{
  static struct PlateNote notes[PITS_SIZE * PITS_SIZE];
  int num_notes = 0;
  int x, y, i;

  // 1: the moves are read from the count when a script asks

  // 2: what the player stands on
  PickUp();

  // 3: the scripts, switched on
  if (pits.script_master >= 0)
    RunScript(pits.script_master, TRUE, 0, 0);

  if (pits.gem_in_use >= PITS_RED && pits.gem_in_use <= PITS_BLUE &&
      pits.script_gem[pits.gem_in_use] >= 0)
    RunScript(pits.script_gem[pits.gem_in_use], TRUE, 0, 0);

  // 4: arrows and glass
  MoveByArrowsAndGlass();

  // 5: every square
  for (y = PITS_MIN; y <= PITS_MAX; y++)
  {
    for (x = PITS_MIN; x <= PITS_MAX; x++)
    {
      int over = pits.over[x][y];
      boolean player_here = (x == pits.px && y == pits.py);

      if (PITS_IS_AT_HOME(over))
	ApplyTileRules(x, y);

      if (player_here)
      {
	if (pits.under[x][y] == PITS_U_PLATE_UP && !IsGhost())
	{
	  pits.under[x][y] = PITS_U_PLATE_DOWN;
	  PlaySound_PITS(PITS_SND_PLATEON);

	  notes[num_notes].x = x;
	  notes[num_notes].y = y;
	  notes[num_notes].pressed = TRUE;
	  num_notes++;
	}
      }
      else if (over == PITS_O_NOTHING)
      {
	if (pits.under[x][y] == PITS_U_PLATE_DOWN)
	{
	  notes[num_notes].x = x;
	  notes[num_notes].y = y;
	  notes[num_notes].pressed = FALSE;
	  num_notes++;
	}
      }
      else if (over >= PITS_O_BARREL_WATER && over < 53)
      {
	ApplyTileRules(x, y);

	if (pits.over[x][y] != PITS_O_NOTHING &&
	    pits.under[x][y] == PITS_U_PLATE_UP)
	{
	  pits.under[x][y] = PITS_U_PLATE_DOWN;
	  PlaySound_PITS(PITS_SND_PLATEON);

	  notes[num_notes].x = x;
	  notes[num_notes].y = y;
	  notes[num_notes].pressed = TRUE;
	  num_notes++;
	}
      }
    }
  }

  // 6: the plates noted, the last one first
  for (i = num_notes - 1; i >= 0; i--)
  {
    int nr = pits.file.script_map[notes[i].x][notes[i].y];

    if (notes[i].pressed)
      PlaySound_PITS(PITS_SND_PLATEON);

    if (nr != PITS_NO_SCRIPT)
      RunScript(nr, notes[i].pressed, notes[i].x, notes[i].y);
  }

  // 7: the master script, switched off
  if (pits.script_master >= 0)
    RunScript(pits.script_master, FALSE, 0, 0);

  CheckWin_PITS();
}


// ----------------------------------------------------------------------------
// the player
// ----------------------------------------------------------------------------

// the red and blue gems act on the square the player leaves
static void GemsActOnSquareLeft(int x, int y, int dir)
{
  if (pits.gem_in_use == PITS_RED && pits.script_gem[PITS_RED] < 0)
  {
    int under = pits.under[x][y];

    if (under == PITS_U_FLOOR || PITS_IS_ARROW(under))
      SetUnder(x, y, PITS_U_ARROW_UP + dir);
  }
  else if (pits.gem_in_use == PITS_BLUE && pits.script_gem[PITS_BLUE] < 0)
  {
    int bx = x - dir_dx[dir];
    int by = y - dir_dy[dir];

    if (PITS_IN_ARRAY(bx, by) && pits.over[bx][by] >= PITS_O_BARREL_WATER)
    {
      pits.over[x][y] = pits.over[bx][by];
      pits.over[bx][by] = PITS_O_NOTHING;

      // not a way that glass knows: a pulled box doesn't slide
      pits.last_pushed[x][y] = 4 + dir;
      pits.last_pushed[bx][by] = PITS_NO_DIR;

      ApplyTileRules(bx, by);
      ApplyTileRules(x, y);
    }

    pits.gems[PITS_BLUE]--;
    pits.gem_in_use = PITS_NO_GEM;
  }
}

static void StepTo(int x, int y, int dir)
{
  pits.lastx = pits.px;
  pits.lasty = pits.py;

  GemsActOnSquareLeft(pits.px, pits.py, dir);

  pits.px = x;
  pits.py = y;

  pits.walk_step = (pits.walk_step + 1) & 3;
  pits.last_move_tick = pits.tick;
}

static void Push(int bx, int by, int dir)
{
  int cx = bx + dir_dx[dir];
  int cy = by + dir_dy[dir];
  int box = pits.over[bx][by];
  boolean moved = FALSE;

  if (!pits.pushing || pits.push_dir != dir)
  {
    pits.pushing = TRUE;
    pits.push_dir = dir;
    pits.push_tick = pits.tick;

    return;
  }

  if (pits.tick - pits.push_tick < PITS_PUSH_TICKS)
    return;

  pits.push_tick = pits.tick;

  if (PITS_IN_ARRAY(cx, cy) &&
      ((OverIsFree(cx, cy) && pits.under[cx][cy] != PITS_U_GRASS) ||
       PITS_IS_CHEST(pits.over[cx][cy])))
  {
    if (PITS_IS_CHEST(pits.over[cx][cy]))
    {
      pits.over[cx][cy] = PITS_O_NOTHING;
      SetUnder(cx, cy, PITS_U_GLASS);
      PlaySound_PITS(PITS_SND_CRACK);
    }

    MoveObject(bx, by, cx, cy, dir);
    PlaySound_PITS(PITS_SND_SCRAPE);

    moved = TRUE;
  }
  else if (PITS_IS_CHEST(box))
  {
    pits.over[bx][by] = PITS_O_NOTHING;
    SetUnder(bx, by, PITS_U_GLASS);
    pits.last_pushed[bx][by] = PITS_NO_DIR;
    PlaySound_PITS(PITS_SND_CRACK);

    moved = TRUE;
  }

  if (!moved)
    return;

  pits.moves++;

  if (pits.over[bx][by] == PITS_O_NOTHING && IsWalkable(pits.under[bx][by]))
    StepTo(bx, by, dir);

  TurnChecks_PITS();
}

static void StopWalking(void)
{
  pits.walking = FALSE;
}

void TryStep_PITS(int dir)
{
  int x = pits.px + dir_dx[dir];
  int y = pits.py + dir_dy[dir];
  int over;

  pits.facing = dir * 4;

  if (!PITS_IN_PIT(x, y))
  {
    pits.pushing = FALSE;
    StopWalking();

    return;
  }

  if (IsGhost() || CanEnter_PITS(x, y))
  {
    if (pits.pushing)
    {
      pits.pushing = FALSE;

      return;
    }

    StepTo(x, y, dir);
    pits.moves++;

    TurnChecks_PITS();

    return;
  }

  over = pits.over[x][y];

  if (OverIsFree(x, y))		// the ground can't be entered
  {
    pits.pushing = FALSE;
    StopWalking();

    return;
  }

  if (pits.walking)
  {
    int wx = pits.walk_x, wy = pits.walk_y;

    pits.pushing = FALSE;
    StopWalking();

    if (abs(wx - pits.px) + abs(wy - pits.py) > 1)
      FindWay_PITS(wx, wy);

    return;
  }

  if (PITS_IS_BOX(over))
  {
    Push(x, y, dir);
  }
  else if (over == PITS_O_SIGN)
  {
    pits.pushing = FALSE;

    if (pits.bump_ready && pits.file.script_map[x][y] != PITS_NO_SCRIPT)
    {
      pits.bump_ready = FALSE;
      RunScript(pits.file.script_map[x][y], TRUE, x, y);
    }
  }
  else if (over == PITS_O_SWITCH_OFF || over == PITS_O_SWITCH_ON)
  {
    pits.pushing = FALSE;

    if (pits.bump_ready)
    {
      boolean on = (over == PITS_O_SWITCH_OFF);

      pits.bump_ready = FALSE;
      pits.over[x][y] = (on ? PITS_O_SWITCH_ON : PITS_O_SWITCH_OFF);
      PlaySound_PITS(on ? PITS_SND_PLATEON : PITS_SND_PLATEOFF);

      if (pits.file.script_map[x][y] != PITS_NO_SCRIPT)
	RunScript(pits.file.script_map[x][y], on, x, y);
    }
  }
  else
  {
    pits.pushing = FALSE;
  }
}

void Wait_PITS(void)
{
  pits.pushing = FALSE;

  TurnChecks_PITS();

  pits.moves++;
}


// ----------------------------------------------------------------------------
// walking to a square
// ----------------------------------------------------------------------------

boolean FindWay_PITS(int tx, int ty)
{
  static int queue_x[PITS_SIZE * PITS_SIZE], queue_y[PITS_SIZE * PITS_SIZE];
  static int search_dirs[4] = { PITS_SOUTH, PITS_WEST, PITS_NORTH, PITS_EAST };
  int head = 0, tail = 0;
  int x, y, i;

  pits.walking = FALSE;

  if (!PITS_IN_PIT(tx, ty) || (tx == pits.px && ty == pits.py))
    return FALSE;

  for (x = 0; x < PITS_SIZE; x++)
    for (y = 0; y < PITS_SIZE; y++)
      pits.walk_next[x][y] = PITS_NO_DIR;

  // spread out from the destination; walk_next is the way back to it
  pits.walk_next[tx][ty] = 4;
  queue_x[tail] = tx;
  queue_y[tail] = ty;
  tail++;

  while (head < tail)
  {
    int cx = queue_x[head];
    int cy = queue_y[head];

    head++;

    for (i = 0; i < 4; i++)
    {
      int dir = search_dirs[i];
      int nx = cx + dir_dx[dir];
      int ny = cy + dir_dy[dir];

      if (!PITS_IN_PIT(nx, ny) || pits.walk_next[nx][ny] != PITS_NO_DIR)
	continue;

      if (nx == pits.px && ny == pits.py)
      {
	pits.walk_next[nx][ny] = (dir + 2) % 4;
	pits.walking = TRUE;
	pits.walk_x = tx;
	pits.walk_y = ty;

	return TRUE;
      }

      if (!IsGhost() && !CanEnter_PITS(nx, ny))
	continue;

      pits.walk_next[nx][ny] = (dir + 2) % 4;
      queue_x[tail] = nx;
      queue_y[tail] = ny;
      tail++;
    }
  }

  return FALSE;
}


// ----------------------------------------------------------------------------
// gems and keys
// ----------------------------------------------------------------------------

static void PutGemAway(int gem)
{
  if (pits.gem_in_use != gem)
    return;

  if (gem == PITS_GREEN)
  {
    FinishGhost_PITS();

    return;
  }

  pits.gems[gem]--;
  pits.gem_in_use = PITS_NO_GEM;

  if (pits.script_gem[gem] >= 0)
    RunScript(pits.script_gem[gem], FALSE, 0, 0);
}

// the ghost becomes the player again, where it can
void FinishGhost_PITS(void)
{
  int x = pits.px, y = pits.py;
  int over = pits.over[x][y];
  boolean has_script = (pits.script_gem[PITS_GREEN] >= 0);

  if (!IsGhost())
    return;

  if (!IsWalkable(pits.under[x][y]))
    return;

  if (!(over == PITS_O_NOTHING ||
	(pits.over[pits.green_x][pits.green_y] == PITS_O_NOTHING &&
	 (over >= PITS_O_BARREL_WATER || IsWalkable(over))) ||
	has_script))
    return;

  if (!has_script && over != PITS_O_NOTHING && !IsWalkable(over))
  {
    pits.over[pits.green_x][pits.green_y] = over;
    pits.over[x][y] = PITS_O_NOTHING;

    ApplyTileRules(x, y);
    ApplyTileRules(pits.green_x, pits.green_y);
  }

  pits.facing = PITS_SOUTH * 4;

  // the pictures of the change back
  pits.change_dir = -1;
  pits.change_stage = PITS_GHOST_STAGE;
  pits.change_turns = 0;

  // not a ghost any more for what follows
  pits.green_stage = PITS_GHOST_STAGE - 1;

  PickUp();
  TurnChecks_PITS();

  pits.gems[PITS_GREEN]--;
  pits.gem_in_use = PITS_NO_GEM;
  pits.green_stage = 0;

  if (pits.script_gem[PITS_GREEN] >= 0)
    RunScript(pits.script_gem[PITS_GREEN], FALSE, 0, 0);
}

void UseGem_PITS(int gem)
{
  if (gem == PITS_YELLOW)
    return;				// it only shows the whole pit

  if (pits.gem_in_use == gem)
  {
    PutGemAway(gem);

    return;
  }

  if (pits.gem_in_use != PITS_NO_GEM || pits.gems[gem] <= 0)
    return;

  pits.gem_in_use = gem;
  pits.pushing = FALSE;
  PlaySound_PITS(PITS_SND_MAGIC);

  if (gem == PITS_GREEN)
  {
    pits.green_stage = 1;
    pits.green_x = pits.px;
    pits.green_y = pits.py;

    // the pictures of the change: facing south, then five steps
    pits.change_dir = 1;
    pits.change_stage = 1;
    pits.change_turns = (pits.facing == PITS_NORTH * 4 ? 2 :
			 pits.facing == PITS_SOUTH * 4 ? 0 : 1);
  }

  if (pits.script_gem[gem] >= 0)
    RunScript(pits.script_gem[gem], TRUE, 0, 0);
}

void UseKey_PITS(int key)
{
  int x = pits.px, y = pits.py;

  if (pits.keys[key] <= 0 || pits.under[x][y] != PITS_U_LOCK_RED + key)
    return;

  pits.keys[key]--;
  SetUnder(x, y, PITS_U_PLATE_DOWN);

  if (pits.file.script_map[x][y] != PITS_NO_SCRIPT)
    RunScript(pits.file.script_map[x][y], TRUE, x, y);
}


// ----------------------------------------------------------------------------
// the player's picture
// ----------------------------------------------------------------------------

int getPlayerShape_PITS(void)
{
  static int step_offset[4] = { 0, 16, 0, 32 };
  int step = pits.walk_step;

  if (pits.tick - pits.last_move_tick >= PITS_IDLE_TICKS)
    step = 0;

  // changing back from a ghost
  if (pits.change_dir < 0 && pits.change_stage >= 2)
    return 228 + (pits.change_stage - 2) * 4;

  // changing to a ghost, or a ghost (stage 7)
  if (pits.gem_in_use == PITS_GREEN && pits.green_stage >= 2)
    return 228 + (pits.green_stage - 2) * 4;

  if (pits.gem_in_use != PITS_NO_GEM)
    return 148 + pits.facing + step_offset[step];

  if (pits.pushing)
    return 196 + pits.facing + (step & 1) * 16;

  return 100 + pits.facing + step_offset[step];
}

boolean IsGhost_PITS(void)
{
  return IsGhost();
}


// ----------------------------------------------------------------------------
// the scripts
// ----------------------------------------------------------------------------

static unsigned char *code;
static int code_size;
static boolean script_ended;

static int CodeByte(int *pc)
{
  int value = (*pc < code_size ? code[*pc] : CMD_END);

  (*pc)++;

  return value;
}

static int CodeWord(int *pc)
{
  int high = CodeByte(pc);

  return (high << 8) | CodeByte(pc);
}

static int EvalValue(int *);

static void ParseRef(int *pc, struct ValueRef *ref)
{
  ref->kind = CodeByte(pc);
  ref->a = 0;
  ref->b = 0;

  if (ref->kind == VAL_WALKABLE)
  {
    ref->a = EvalValue(pc);
  }
  else if (ref->kind >= VAL_DELTA && ref->kind <= VAL_LASTPUSHED)
  {
    ref->a = EvalValue(pc);
    ref->b = EvalValue(pc);
  }
}

static int GetRef(struct ValueRef *ref)
{
  int kind = ref->kind;
  int x, y;

  if (kind <= 100)
    return kind;
  if (kind <= 200)
    return kind - 201;
  if (kind < VAL_REGISTER + PITS_NUM_REGISTERS)
    return pits.reg[kind - VAL_REGISTER];

  switch (kind)
  {
    case VAL_XGUY:		return pits.px;
    case VAL_YGUY:		return pits.py;
    case VAL_LASTX:		return pits.lastx;
    case VAL_LASTY:		return pits.lasty;
    case VAL_FACING:		return pits.facing;
    case VAL_REDGEMS:		return pits.gems[PITS_RED];
    case VAL_GREENGEMS:		return pits.gems[PITS_GREEN];
    case VAL_BLUEGEMS:		return pits.gems[PITS_BLUE];
    case VAL_YELLOWGEMS:	return (pits.gems[PITS_YELLOW] ? -1 : 0);
    case VAL_REDKEYS:		return pits.keys[PITS_RED];
    case VAL_GREENKEYS:		return pits.keys[PITS_GREEN];
    case VAL_BLUEKEYS:		return pits.keys[PITS_BLUE];
    case VAL_INPUTVALUE:	return pits.input_value;
    case VAL_XPOS:		return pits.xpos;
    case VAL_YPOS:		return pits.ypos;
    case VAL_MOVES:		return pits.moves % 100;
    case VAL_HECTOMOVES:	return pits.moves / 100;

    case VAL_WALKABLE:
      return (IsWalkable(MAX(0, ref->a)) ? 1 : 0);

    case VAL_DELTA:
      return Word16(ref->a + ref->b);

    case VAL_UNDER:
    case VAL_OVER:
    case VAL_LASTPUSHED:
      x = Clamp(ref->a, PITS_MIN, PITS_MAX);
      y = Clamp(ref->b, PITS_MIN, PITS_MAX);

      if (kind == VAL_UNDER)
	return pits.under[x][y];

      if (kind == VAL_LASTPUSHED)
	return pits.last_pushed[x][y];

      if (x == pits.px && y == pits.py &&
	  (!IsGhost() || pits.script_gem[PITS_GREEN] >= 0))
	return PITS_O_PLAYER;

      return pits.over[x][y];

    case VAL_REDGEMON:
      return (pits.gem_in_use == PITS_RED ? 1 : 0);

    case VAL_GREENGEMON:
      return (pits.gem_in_use == PITS_GREEN ? pits.green_stage : 0);

    case VAL_BLUEGEMON:
      return (pits.gem_in_use == PITS_BLUE ? 1 : 0);

    default:
      return 0;
  }
}

static void SetRef(struct ValueRef *ref, int value)
{
  int kind = ref->kind;
  int x, y;

  value = Word16(value);

  if (kind <= 200)
    return;				// a number

  if (kind < VAL_REGISTER + PITS_NUM_REGISTERS)
  {
    pits.reg[kind - VAL_REGISTER] = value;

    return;
  }

  switch (kind)
  {
    case VAL_XGUY:		pits.px = Clamp(value, 0, PITS_SIZE - 1);	break;
    case VAL_YGUY:		pits.py = Clamp(value, 0, PITS_SIZE - 1);	break;
    case VAL_LASTX:		pits.lastx = value;				break;
    case VAL_LASTY:		pits.lasty = value;				break;
    case VAL_FACING:		pits.facing = value & 12;			break;
    case VAL_REDGEMS:		pits.gems[PITS_RED] = value;			break;
    case VAL_GREENGEMS:		pits.gems[PITS_GREEN] = value;			break;
    case VAL_BLUEGEMS:		pits.gems[PITS_BLUE] = value;			break;
    case VAL_YELLOWGEMS:	pits.gems[PITS_YELLOW] = (value != 0);		break;
    case VAL_REDKEYS:		pits.keys[PITS_RED] = value;			break;
    case VAL_GREENKEYS:		pits.keys[PITS_GREEN] = value;			break;
    case VAL_BLUEKEYS:		pits.keys[PITS_BLUE] = value;			break;
    case VAL_INPUTVALUE:	pits.input_value = value;			break;
    case VAL_XPOS:		pits.xpos = value;				break;
    case VAL_YPOS:		pits.ypos = value;				break;

    case VAL_MOVES:
      pits.moves = (pits.moves / 100) * 100 + value;
      break;

    case VAL_HECTOMOVES:
      pits.moves = value * 100 + pits.moves % 100;
      break;

    case VAL_WALKABLE:
      if (ref->a <= 100)
	pits.walkable[MAX(0, ref->a)] = (value != 0);
      break;

    case VAL_UNDER:
    case VAL_OVER:
    case VAL_LASTPUSHED:
      x = Clamp(ref->a, PITS_MIN, PITS_MAX);
      y = Clamp(ref->b, PITS_MIN, PITS_MAX);

      if (kind == VAL_LASTPUSHED)
      {
	pits.last_pushed[x][y] = value;

	break;
      }

      if (kind == VAL_UNDER)
	pits.under[x][y] = Clamp(value, 0, 100);
      else
	pits.over[x][y] = Clamp(value, 0, 100);

      ChangedByScript(x, y);
      break;

    default:
      break;
  }
}

static int EvalValue(int *pc)
{
  struct ValueRef ref;

  ParseRef(pc, &ref);

  return GetRef(&ref);
}

static void ChangeMap(int x, int y, int tile)
{
  if (!PITS_IN_ARRAY(x, y) || tile < -3 || tile > 100)
    return;

  if (tile == -1 || tile == -3)
    pits.under[x][y] = PITS_U_VOID;
  if (tile == -2 || tile == -3)
    pits.over[x][y] = PITS_O_NOTHING;

  if ((tile >= 0 && tile < PITS_U_FLOOR) || tile == PITS_O_PLAYER)
    pits.over[x][y] = PlainObject(tile);
  else if (tile >= PITS_U_FLOOR)
    pits.under[x][y] = tile;

  ChangedByScript(x, y);
}

static void Swap(int x1, int y1, int x2, int y2, boolean over)
{
  int tmp;

  if (!PITS_IN_PIT(x1, y1) || !PITS_IN_PIT(x2, y2))
    return;

  if (over)
  {
    tmp = pits.over[x1][y1];
    pits.over[x1][y1] = pits.over[x2][y2];
    pits.over[x2][y2] = tmp;

    if (pits.px == x1 && pits.py == y1)
    {
      pits.px = x2;
      pits.py = y2;
    }
    else if (pits.px == x2 && pits.py == y2)
    {
      pits.px = x1;
      pits.py = y1;
    }
  }
  else
  {
    tmp = pits.under[x1][y1];
    pits.under[x1][y1] = pits.under[x2][y2];
    pits.under[x2][y2] = tmp;

    if (PITS_IS_ARROW(pits.under[x1][y1]) || pits.under[x1][y1] == PITS_U_GLASS)
      AddArrow(x1, y1);
    if (PITS_IS_ARROW(pits.under[x2][y2]) || pits.under[x2][y2] == PITS_U_GLASS)
      AddArrow(x2, y2);
  }

  tmp = pits.last_pushed[x1][y1];
  pits.last_pushed[x1][y1] = pits.last_pushed[x2][y2];
  pits.last_pushed[x2][y2] = tmp;

  ApplyTileRules(x1, y1);
  ApplyTileRules(x2, y2);
}

static void ShowScriptText(int pc, int length)
{
  char *text = checked_calloc(length + 1);
  int i;

  for (i = 0; i < length && pc + i < code_size; i++)
    text[i] = code[pc + i];

  ShowLevelText_PITS(text);

  free(text);
}

// TARGET can't wait for the player: it takes the square last clicked,
// or else the one the player faces
static void Target(void)
{
  int x = pits.target_x, y = pits.target_y;

  if (!PITS_IN_PIT(x, y))
  {
    x = pits.px + dir_dx[pits.facing / 4];
    y = pits.py + dir_dy[pits.facing / 4];
  }

  if (!PITS_IN_PIT(x, y))
    x = y = 0;

  pits.xpos = x;
  pits.ypos = y;

  pits.target_x = pits.target_y = 0;
}

static boolean InputStartsWith(int pc, int length)
{
  int i;

  if (length > (int)strlen(pits.input))
    return FALSE;

  for (i = 0; i < length; i++)
    if (tolower((unsigned char)pits.input[i]) !=
	tolower(pc + i < code_size ? code[pc + i] : 0))
      return FALSE;

  return TRUE;
}

static void RunScript(int nr, boolean on, int x, int y)
{
  struct
  {
    int ref_pc;
    int to;
    boolean up;
    int body;
  } loops[SCRIPT_MAX_LOOPS];
  int num_loops = 0;
  int steps = 0;
  int pc;

  if (!HasScript_PITS(nr))
    return;

  code = pits.file.code;
  code_size = pits.file.code_size;

  pc = pits.file.script_start[nr];
  pits.xpos = x;
  pits.ypos = y;
  script_ended = FALSE;

  while (!script_ended && pc < code_size && steps++ < SCRIPT_MAX_STEPS)
  {
    int command = CodeByte(&pc);
    struct ValueRef ref;
    int a, b, c, d, jump_pc, jump, length, value;
    boolean condition;

    switch (command)
    {
      case CMD_END:
      case CMD_STOP:
	script_ended = TRUE;
	break;

      case CMD_WIN:
	pits.won = TRUE;
	script_ended = TRUE;
	break;

      case CMD_ENDIF:
	break;

      case CMD_NEXT:
	if (num_loops == 0)
	  break;

	{
	  int ref_pc = loops[num_loops - 1].ref_pc;

	  ParseRef(&ref_pc, &ref);
	}

	value = GetRef(&ref);

	if (loops[num_loops - 1].up ? value >= loops[num_loops - 1].to :
	    value <= loops[num_loops - 1].to)
	{
	  num_loops--;
	}
	else
	{
	  SetRef(&ref, value + (loops[num_loops - 1].up ? 1 : -1));
	  pc = loops[num_loops - 1].body;
	}
	break;

      case CMD_ELSE:
	jump_pc = pc;
	jump = CodeWord(&pc);
	pc = jump_pc + jump;
	break;

      case CMD_SWITCHOFF:
	if (on)
	  script_ended = TRUE;
	break;

      case CMD_SWITCHON:
	jump_pc = pc;
	jump = CodeWord(&pc);

	if (!on)
	  pc = jump_pc + jump + 1;	// just after its SWITCHOFF
	break;

      case CMD_IF_EQ:
      case CMD_IF_EQ + 1:
      case CMD_IF_EQ + 2:
      case CMD_IF_EQ + 3:
      case CMD_IF_EQ + 4:
      case CMD_IF_NE:
      case CMD_IF_INPUT:
	if (command == CMD_IF_INPUT)
	{
	  length = CodeWord(&pc);
	  jump_pc = pc;
	  jump = CodeWord(&pc);
	  condition = InputStartsWith(pc, length);
	  pc += length;
	}
	else
	{
	  a = EvalValue(&pc);
	  b = EvalValue(&pc);
	  jump_pc = pc;
	  jump = CodeWord(&pc);

	  condition = (command == CMD_IF_EQ     ? a == b :
		       command == CMD_IF_EQ + 1 ? a <  b :
		       command == CMD_IF_EQ + 2 ? a >  b :
		       command == CMD_IF_EQ + 3 ? a <= b :
		       command == CMD_IF_EQ + 4 ? a >= b :
		       a != b);
	}

	if (!condition)
	{
	  pc = jump_pc + jump;

	  // to its ELSE: what follows it runs
	  if (pc < code_size && code[pc] == CMD_ELSE)
	    pc += 3;
	}
	break;

      case CMD_FOR:
	{
	  int ref_pc = pc;

	  ParseRef(&pc, &ref);
	  a = EvalValue(&pc);
	  b = EvalValue(&pc);
	  CodeWord(&pc);			// the jump past its NEXT

	  SetRef(&ref, a);

	  if (num_loops < SCRIPT_MAX_LOOPS)
	  {
	    loops[num_loops].ref_pc = ref_pc;
	    loops[num_loops].to = b;
	    loops[num_loops].up = (b > a);
	    loops[num_loops].body = pc;
	    num_loops++;
	  }
	}
	break;

      case CMD_REDRAW:
	SnapshotView_PITS();
	break;

      case CMD_APPEAR:
	SnapshotView_PITS();
	PlaySound_PITS(PITS_SND_MAGIC);
	break;

      case CMD_TARGET:
	Target();
	break;

      case CMD_REDGEMOFF:
      case CMD_GREENGEMOFF:
      case CMD_BLUEGEMOFF:
	{
	  int gem = command - CMD_REDGEMOFF;
	  int save_xpos = pits.xpos, save_ypos = pits.ypos;
	  unsigned char *save_code = code;

	  // putting a gem away may run other scripts
	  PutGemAway(gem);

	  code = save_code;
	  code_size = pits.file.code_size;
	  pits.xpos = save_xpos;
	  pits.ypos = save_ypos;
	  script_ended = FALSE;
	}
	break;

      case CMD_POPUP:
	if (PITS_IN_ARRAY(pits.xpos, pits.ypos) &&
	    (pits.under[pits.xpos][pits.ypos] == PITS_U_PLATE_DOWN ||
	     pits.under[pits.xpos][pits.ypos] == PITS_U_PLATE_UP))
	  pits.under[pits.xpos][pits.ypos] = PITS_U_PLATE_UP;

	PlaySound_PITS(PITS_SND_PLATEOFF);
	break;

      case CMD_LET:
      case CMD_ADD:
      case CMD_SUB:
      case CMD_MUL:
      case CMD_DIV:
	ParseRef(&pc, &ref);
	b = EvalValue(&pc);

	if (command == CMD_LET)
	{
	  value = b;
	}
	else
	{
	  a = GetRef(&ref);

	  value = (command == CMD_ADD ? a + b :
		   command == CMD_SUB ? a - b :
		   command == CMD_MUL ? a * b :
		   b == 0 ? 0 : a / b);
	}

	SetRef(&ref, value);
	break;

      case CMD_SWAPOVER:
      case CMD_SWAPUNDER:
      case CMD_GOTO:
	a = EvalValue(&pc);
	b = EvalValue(&pc);
	c = EvalValue(&pc);
	d = EvalValue(&pc);

	if (command != CMD_GOTO)
	{
	  Swap(a, b, c, d, command == CMD_SWAPOVER);

	  break;
	}

	if (!PITS_IN_PIT(a, b) ||
	    !HasScript_PITS(pits.file.script_map[a][b]))
	{
	  script_ended = TRUE;

	  break;
	}

	pc = pits.file.script_start[pits.file.script_map[a][b]];
	pits.xpos = c;
	pits.ypos = d;
	num_loops = 0;
	break;

      case CMD_CHANGEMAP:
	length = CodeWord(&pc);
	jump_pc = pc + length;

	while (pc < jump_pc)
	{
	  a = EvalValue(&pc);
	  b = EvalValue(&pc);
	  c = EvalValue(&pc);

	  ChangeMap(a, b, c);
	}

	pc = jump_pc;
	break;

      case CMD_PRINT:
      case CMD_INPUT:
	length = CodeWord(&pc);
	ShowScriptText(pc, length);
	pc += length;

	if (command == CMD_INPUT)
	{
	  // the player can't type here: the line typed is empty
	  pits.input[0] = '\0';
	  pits.input_value = 0;
	}
	break;

      default:
	script_ended = TRUE;
	break;
    }
  }
}
