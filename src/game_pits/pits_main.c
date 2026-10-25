// ============================================================================
// The Puzzle Pits engine for Rocks'n'Diamonds
// ----------------------------------------------------------------------------
// Written from the description of the game in PITS-RULES.md and of its level
// files in PIT-FORMAT.md.
// ----------------------------------------------------------------------------
// pits_main.c: the level files, and the game as R'n'D runs it
// ============================================================================

#include "pits_main.h"


#define PITS_MAGIC			0x0895

#define TEXT_COLUMNS			30	// R'n'D's envelope: 30 by 20
#define TEXT_LINES			20

struct GameInfo_PITS game_pits;
struct LevelInfo_PITS native_pits_level;

struct PitsInfo pits;
struct PitsViewInfo pits_view;

static boolean quiet_text = FALSE;


// ----------------------------------------------------------------------------
// reading a level file
// ----------------------------------------------------------------------------

struct FileReader
{
  unsigned char *data;
  int size;
  int pos;
  boolean error;
};

static int ReadByte(struct FileReader *reader)
{
  if (reader->pos >= reader->size)
  {
    reader->error = TRUE;

    return 0;
  }

  return reader->data[reader->pos++];
}

static int ReadWord(struct FileReader *reader)
{
  int high = ReadByte(reader);

  return (high << 8) | ReadByte(reader);
}

// a string: its length XOR 22, then its bytes XOR "Mindy"
static void ReadString(struct FileReader *reader, char *string, int max_length)
{
  static char *key = "Mindy";
  int length = ReadWord(reader) ^ 22;
  int i;

  for (i = 0; i < length; i++)
  {
    int c = ReadByte(reader) ^ key[i % 5];

    if (i < max_length)
      string[i] = c;
  }

  string[MIN(length, max_length)] = '\0';
}

static void FillMap(int map[PITS_SIZE][PITS_SIZE], int fill)
{
  int x, y;

  for (x = 0; x < PITS_SIZE; x++)
    for (y = 0; y < PITS_SIZE; y++)
      map[x][y] = fill;
}

// a map: the inner squares of map[x][y], run-length coded
static void ReadMap(struct FileReader *reader, int map[PITS_SIZE][PITS_SIZE],
		    int fill)
{
  int cell = PITS_SIZE + 1;
  int end = PITS_SIZE * PITS_SIZE - PITS_SIZE - 1;

  FillMap(map, fill);

  while (cell < end && !reader->error)
  {
    int count = ReadByte(reader);
    int tile = ReadByte(reader);

    while (count-- > 0 && cell < end)
    {
      map[cell / PITS_SIZE][cell % PITS_SIZE] = tile;
      cell++;

      // skip the edge squares at the end of a row and the start of the next
      if (cell % PITS_SIZE == PITS_SIZE - 1)
	cell += 2;
    }
  }
}

boolean ReadLevelFile_PITS(unsigned char *data, int size,
			   struct PitsFileInfo *file, boolean level_info_only)
{
  struct FileReader reader = { data, size, 0, FALSE };
  int num_scripts, code_size;
  int i;

  checked_free(file->code);
  file->code = NULL;
  file->code_size = 0;

  for (i = 0; i < PITS_MAX_SCRIPTS; i++)
  {
    file->script_start[i] = -1;
    file->script_name[i][0] = '\0';
  }

  if (ReadWord(&reader) != PITS_MAGIC)
    return FALSE;

  ReadString(&reader, file->password, PITS_PASSWORD_LEN);
  ReadString(&reader, file->title, PITS_TITLE_LEN);
  ReadMap(&reader, file->under, 0);
  ReadMap(&reader, file->over, 0);

  if (reader.error)
    return FALSE;

  num_scripts = ReadWord(&reader);

  // the script map is only there with scripts
  if (num_scripts > 0)
    ReadMap(&reader, file->script_map, PITS_NO_SCRIPT);
  else
    FillMap(file->script_map, PITS_NO_SCRIPT);

  if (num_scripts == 0 || reader.error || level_info_only)
    return TRUE;

  for (i = 0; i < num_scripts && !reader.error; i++)
  {
    char name[PITS_SCRIPT_NAME_LEN + 1];
    int nr = ReadWord(&reader);
    int script_size, start;

    ReadString(&reader, name, PITS_SCRIPT_NAME_LEN);
    script_size = ReadWord(&reader);
    start = ReadWord(&reader);

    if (nr < 0 || nr >= PITS_MAX_SCRIPTS || nr == PITS_NO_SCRIPT)
      continue;

    strcpy(file->script_name[nr], name);
    file->script_start[nr] = (script_size > 0 ? start : -1);
  }

  code_size = ReadWord(&reader);

  if (reader.error)
    return FALSE;

  file->code = checked_calloc(code_size + 1);
  file->code_size = code_size;

  for (i = 0; i < code_size; i++)
    file->code[i] = ReadByte(&reader) ^ "Amanda"[i % 6];

  for (i = 0; i < PITS_MAX_SCRIPTS; i++)
    if (file->script_start[i] >= code_size)
      file->script_start[i] = -1;

  return TRUE;
}


// ----------------------------------------------------------------------------
// the interface for loading levels
// ----------------------------------------------------------------------------

void setLevelInfoToDefaults_PITS(void)
{
  struct LevelInfo_PITS *level = &native_pits_level;
  int x, y;

  level->title[0] = '\0';
  level->password[0] = '\0';

  for (x = 0; x < PITS_SIZE; x++)
  {
    for (y = 0; y < PITS_SIZE; y++)
    {
      level->under[x][y] = 0;
      level->over[x][y] = 0;
    }
  }

  level->directory[0] = '\0';
  level->file_size = 0;
}

boolean LoadNativeLevel_PITS(char *filename, boolean level_info_only)
{
  static struct PitsFileInfo file;
  struct LevelInfo_PITS *level = &native_pits_level;
  char *last_slash;
  FILE *stream;
  int x, y;

  setLevelInfoToDefaults_PITS();

  if (!(stream = fopen(filename, "rb")))
  {
    if (!level_info_only)
      Warn("cannot read level '%s' -- using empty level", filename);

    return FALSE;
  }

  level->file_size = fread(level->file, 1, PITS_MAX_FILE_SIZE, stream);
  fclose(stream);

  if (!ReadLevelFile_PITS(level->file, level->file_size, &file, TRUE))
  {
    Warn("cannot load level '%s' -- using empty level", filename);

    return FALSE;
  }

  strcpy(level->title, file.title);
  strcpy(level->password, file.password);

  for (x = 0; x < PITS_SIZE; x++)
  {
    for (y = 0; y < PITS_SIZE; y++)
    {
      level->under[x][y] = file.under[x][y];
      level->over[x][y] = file.over[x][y];
    }
  }

  // the pictures and sounds are next to the level files
  strncpy(level->directory, filename, sizeof(level->directory) - 1);
  level->directory[sizeof(level->directory) - 1] = '\0';

  if ((last_slash = strrchr(level->directory, '/')) != NULL)
    *last_slash = '\0';
  else
    strcpy(level->directory, ".");

  return TRUE;
}


// ----------------------------------------------------------------------------
// text: a page at a time
// ----------------------------------------------------------------------------

static void ShowPage(char *page, int *num_lines)
{
  int len = strlen(page);

  // no empty lines at the end of a page
  while (len > 0 && page[len - 1] == '\n')
    page[--len] = '\0';

  if (len > 0)
    ShowText_PITS(page);

  page[0] = '\0';
  *num_lines = 0;
}

void ShowLevelText_PITS(char *text)
{
  char page[TEXT_LINES * (TEXT_COLUMNS + 1) + 1];
  int num_lines = 0;
  char *p = text;

  if (quiet_text)
    return;

  page[0] = '\0';

  while (*p != '\0')
  {
    int len = 0;

    // a line: to the end of the text's line, or wrapped at a space
    while (p[len] != '\0' && p[len] != '\n' && len < TEXT_COLUMNS)
      len++;

    if (p[len] != '\0' && p[len] != '\n' && p[len] != ' ')
    {
      int wrap = len;

      while (wrap > 0 && p[wrap] != ' ')
	wrap--;

      if (wrap > 0)
	len = wrap;
    }

    // no spaces at the end of a line
    while (len > 0 && p[len - 1] == ' ' && p[len] == ' ')
      len--;

    // a page doesn't start with an empty line
    if (num_lines > 0 || len > 0)
    {
      strncat(page, p, len);
      strcat(page, "\n");
      num_lines++;
    }

    p += len;

    if (*p == '\n')
      p++;
    else
      while (*p == ' ')			// where the line was wrapped
	p++;

    if (num_lines == TEXT_LINES)
      ShowPage(page, &num_lines);
  }

  if (num_lines > 0)
    ShowPage(page, &num_lines);
}


// ----------------------------------------------------------------------------
// the view: the pit around the player
// ----------------------------------------------------------------------------

void getViewGeometry_PITS(int px, int py, int *vx0, int *vy0, int *ox, int *oy,
			  int *cols, int *rows, int *scale)
{
  int s = MAX(1, MIN(SXSIZE / (9 * PITS_SQUARE), SYSIZE / (7 * PITS_SQUARE)));
  int square = PITS_SQUARE * s;
  int cx = (SXSIZE - square) / 2;	// the player's square, on the screen
  int cy = (SYSIZE - square) / 2;
  int left = (cx + square - 1) / square;
  int up   = (cy + square - 1) / square;

  *scale = s;
  *vx0 = px - left;
  *vy0 = py - up;
  *ox = cx - left * square;
  *oy = cy - up * square;
  *cols = (SXSIZE - *ox + square - 1) / square;
  *rows = (SYSIZE - *oy + square - 1) / square;
}

void SnapshotView_PITS(void)
{
  int x, y;

  for (x = 0; x < PITS_SIZE; x++)
  {
    for (y = 0; y < PITS_SIZE; y++)
    {
      pits_view.under[x][y] = pits.under[x][y];
      pits_view.over[x][y] = pits.over[x][y];
    }
  }

  pits_view.px = pits.px;
  pits_view.py = pits.py;
  pits_view.facing = pits.facing;
  pits_view.player_shape = getPlayerShape_PITS();
  pits_view.ghost_shape = -1;
  pits_view.pushing_front = (pits.pushing && pits.gem_in_use == PITS_NO_GEM &&
			     (pits.facing == PITS_EAST * 4 ||
			      pits.facing == PITS_SOUTH * 4));
  pits_view.anim_counter = pits.anim_counter;
  pits_view.sparkle = pits.sparkle;
  pits_view.show_all = pits.show_all;

  if (IsGhost_PITS())
  {
    pits_view.ghost_shape = pits_view.player_shape;
    pits_view.player_shape = -1;
  }
}


// ----------------------------------------------------------------------------
// starting a level
// ----------------------------------------------------------------------------

void InitGameEngine_PITS(void)
{
  static char last_directory[1024] = "";
  static char last_title[PITS_TITLE_LEN + 1] = "";
  boolean same_level = (strcmp(last_directory, native_pits_level.directory) == 0 &&
			strcmp(last_title, native_pits_level.title) == 0);
  boolean restart = (same_level && pits.started && pits.moves > 0 && !pits.won);
  struct PitsFileInfo file = pits.file;

  strcpy(last_directory, native_pits_level.directory);
  strcpy(last_title, native_pits_level.title);

  // the whole file, with its scripts
  if (!ReadLevelFile_PITS(native_pits_level.file, native_pits_level.file_size,
			  &file, FALSE))
    Warn("cannot read the scripts of level '%s'", native_pits_level.title);

  memset(&pits, 0, sizeof(pits));
  pits.file = file;

  InitLevel_PITS();

  pits.frame = 0;
  pits.tick = 0;
  pits.last_turn_tick = -PITS_TURN_TICKS;
  pits.next_anim_tick = 0;
  pits.started = FALSE;
  pits.quiet_intro = restart;
  pits.bump_ready = TRUE;
  pits.last_move_tick = -PITS_IDLE_TICKS;
  pits.push_dir = PITS_NO_DIR;

  InitGfx_PITS(native_pits_level.directory);
  InitSound_PITS(native_pits_level.directory);

  game_pits.level_solved = FALSE;
  game_pits.game_over = FALSE;
  game_pits.moves = 0;
  game_pits.targets_still_needed = getTargetsStillNeeded_PITS();
  game_pits.gems = 0;
  game_pits.key[0] = game_pits.key[1] = game_pits.key[2] = FALSE;

  SnapshotView_PITS();
}

unsigned int InitEngineRandom_PITS(int seed)
{
  // the game has no chance in it
  return (seed < 0 ? 0 : seed);
}


// ----------------------------------------------------------------------------
// the game, a frame at a time
// ----------------------------------------------------------------------------

static int getDirFromAction(int action)
{
  int last_dir = pits.facing / 4;

  // a direction held on keeps going, when another is pressed too
  if ((last_dir == PITS_NORTH && (action & MV_UP)) ||
      (last_dir == PITS_EAST  && (action & MV_RIGHT)) ||
      (last_dir == PITS_SOUTH && (action & MV_DOWN)) ||
      (last_dir == PITS_WEST  && (action & MV_LEFT)))
    return last_dir;

  return (action & MV_UP    ? PITS_NORTH :
	  action & MV_RIGHT ? PITS_EAST :
	  action & MV_DOWN  ? PITS_SOUTH :
	  action & MV_LEFT  ? PITS_WEST : PITS_NO_DIR);
}

static int getDirTowards(int x, int y)
{
  int dx = x - pits.px;
  int dy = y - pits.py;

  if (dx == 0 && dy == 0)
    return PITS_NO_DIR;

  if (abs(dx) >= abs(dy))
    return (dx > 0 ? PITS_EAST : PITS_WEST);

  return (dy > 0 ? PITS_SOUTH : PITS_NORTH);
}

static void WalkStep(void)
{
  int dir;

  if (pits.px == pits.walk_x && pits.py == pits.walk_y)
  {
    pits.walking = FALSE;

    return;
  }

  dir = pits.walk_next[pits.px][pits.py];

  // moved off the way (by a script): find it again
  if (dir < PITS_NORTH || dir > PITS_WEST)
  {
    if (!FindWay_PITS(pits.walk_x, pits.walk_y))
      return;

    dir = pits.walk_next[pits.px][pits.py];
  }

  // turning right round goes through a quarter turn first
  if (dir == (pits.facing / 4 + 2) % 4)
  {
    pits.facing = ((pits.facing / 4 + 3) % 4) * 4;

    return;
  }

  TryStep_PITS(dir);

  if (pits.px == pits.walk_x && pits.py == pits.walk_y)
    pits.walking = FALSE;
}

static void HandleChange(void)
{
  if (pits.tick - pits.last_turn_tick < PITS_TURN_TICKS)
    return;

  pits.last_turn_tick = pits.tick;

  if (pits.change_dir > 0)
  {
    if (pits.change_turns > 0)
    {
      // facing south, from north through west
      pits.facing = (pits.facing == PITS_NORTH * 4 ? PITS_WEST * 4 :
		     PITS_SOUTH * 4);
      pits.change_turns--;
    }
    else if (pits.gem_in_use != PITS_GREEN)
    {
      pits.change_dir = 0;
    }
    else if (++pits.green_stage >= PITS_GHOST_STAGE)
    {
      pits.green_stage = PITS_GHOST_STAGE;
      pits.change_dir = 0;
    }
  }
  else
  {
    if (--pits.change_stage <= 1)
      pits.change_dir = 0;
  }
}

static void HandleInput(int action, struct MouseActionInfo mouse)
{
  int dir = getDirFromAction(action);
  boolean snap = ((action & KEY_BUTTON_SNAP) != 0);
  boolean drop = ((action & KEY_BUTTON_DROP) != 0);
  boolean new_action = (action != pits.last_action);
  boolean can_turn = (pits.tick - pits.last_turn_tick >= PITS_TURN_TICKS);
  int button = mouse.button;
  int mx = mouse.lx + 1;		// R'n'D's playfield is the pit's inside
  int my = mouse.ly + 1;

  pits.show_all = (snap && dir == PITS_SOUTH && pits.gems[PITS_YELLOW]);

  // signs and switches: once each time a direction is pressed
  if (dir == PITS_NO_DIR && button != MB_LEFTBUTTON)
  {
    pits.bump_ready = TRUE;
    pits.pushing = FALSE;
  }
  else if (new_action)
  {
    pits.bump_ready = TRUE;
  }

  if (button && PITS_IN_PIT(mx, my))
  {
    pits.target_x = mx;
    pits.target_y = my;
  }

  // a gem (snap) or a key (drop) with a direction
  if ((snap || drop) && dir != PITS_NO_DIR)
  {
    static int gems[4] = { PITS_GREEN, PITS_BLUE, PITS_YELLOW, PITS_RED };
    static int keys[4] = { PITS_GREEN, PITS_BLUE, -1,          PITS_RED };

    pits.walking = FALSE;

    if (new_action)
    {
      if (snap)
	UseGem_PITS(gems[dir]);
      else if (keys[dir] != -1)
	UseKey_PITS(keys[dir]);
    }

    return;
  }

  if (dir != PITS_NO_DIR)
  {
    pits.walking = FALSE;

    if (can_turn)
    {
      pits.last_turn_tick = pits.tick;

      TryStep_PITS(dir);
    }

    return;
  }

  if (snap)				// waiting
  {
    pits.walking = FALSE;

    if (can_turn)
    {
      pits.last_turn_tick = pits.tick;

      Wait_PITS();
    }

    return;
  }

  // the walking button: to the square clicked
  if (button == MB_RIGHTBUTTON && pits.last_button != MB_RIGHTBUTTON &&
      PITS_IN_PIT(mx, my))
    FindWay_PITS(mx, my);

  // the stepping button: towards the square pointed at
  if (button == MB_LEFTBUTTON && PITS_IN_PIT(mx, my))
  {
    int mouse_dir = getDirTowards(mx, my);

    pits.walking = FALSE;

    if (mouse_dir != PITS_NO_DIR && can_turn)
    {
      pits.last_turn_tick = pits.tick;

      if (mouse_dir == (pits.facing / 4 + 2) % 4)
	pits.facing = ((pits.facing / 4 + 1) % 4) * 4;
      else
	TryStep_PITS(mouse_dir);
    }

    return;
  }

  if (pits.walking && can_turn)
  {
    pits.last_turn_tick = pits.tick;

    WalkStep();
  }
}

static void UpdatePanel(void)
{
  game_pits.moves = pits.moves;
  game_pits.targets_still_needed = getTargetsStillNeeded_PITS();
  game_pits.gems = pits.gems[PITS_RED] + pits.gems[PITS_GREEN] +
    pits.gems[PITS_BLUE];
  game_pits.key[0] = (pits.keys[PITS_RED] > 0);
  game_pits.key[1] = (pits.keys[PITS_GREEN] > 0);
  game_pits.key[2] = (pits.keys[PITS_BLUE] > 0);

  if (pits.won)
    game_pits.level_solved = TRUE;
}

void GameActions_PITS(byte action[MAX_PLAYERS], struct MouseActionInfo mouse)
{
  pits.frame++;
  pits.tick = PITS_TICKS(pits.frame);
  pits.sounds = 0;

  if (!pits.started)
  {
    pits.started = TRUE;

    quiet_text = pits.quiet_intro;
    StartLevel_PITS();
    quiet_text = FALSE;

    CheckWin_PITS();
  }
  else if (!pits.won)
  {
    while (pits.tick >= pits.next_anim_tick)
    {
      AnimStep_PITS();

      pits.next_anim_tick += PITS_TURN_TICKS;
    }

    if (pits.change_dir != 0)
      HandleChange();
    else
      HandleInput(action[0], mouse);
  }

  pits.last_action = action[0];
  pits.last_button = mouse.button;

  UpdatePanel();
  SnapshotView_PITS();

  PlaySounds_PITS(pits.sounds);
}
