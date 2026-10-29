// ============================================================================
// The Puzzle Pits engine for Rocks'n'Diamonds
// ----------------------------------------------------------------------------
// Written from the description of the game in PITS-RULES.md and of its level
// files in PIT-FORMAT.md.
// ----------------------------------------------------------------------------
// pits_main.h: what the parts of game_pits share
// ============================================================================

#ifndef PITS_MAIN_H
#define PITS_MAIN_H

#include "../libgame/libgame.h"
#include "../game.h"

#include "export_pits.h"


// ----------------------------------------------------------------------------
// imported from the main program
// ----------------------------------------------------------------------------

extern int			SX, SY;
extern int			SXSIZE, SYSIZE;


// ----------------------------------------------------------------------------
// constant definitions
// ----------------------------------------------------------------------------

#define PITS_MIN			1	// the playable squares
#define PITS_MAX			(PITS_SIZE - 2)
#define PITS_IN_PIT(x, y)		((x) >= PITS_MIN && (x) <= PITS_MAX && \
					 (y) >= PITS_MIN && (y) <= PITS_MAX)
#define PITS_IN_ARRAY(x, y)		((x) >= 0 && (x) < PITS_SIZE &&	\
					 (y) >= 0 && (y) < PITS_SIZE)

#define PITS_MAX_SCRIPTS		256
#define PITS_NO_SCRIPT			255
#define PITS_SCRIPT_NAME_LEN		32
#define PITS_INPUT_LEN			16
#define PITS_NUM_REGISTERS		10

// the game's clock: about 60 ticks a second, R'n'D's frames 50 a second
#define PITS_TICKS(frames)		((frames) * 6 / 5)
#define PITS_TURN_TICKS			9
#define PITS_PUSH_TICKS			25
#define PITS_IDLE_TICKS			90

#define PITS_SQUARE			24	// a square's pictures, in pixels

// directions: the way a thing goes (north 0, east 1, south 2, west 3);
// the player's facing is four times that
#define PITS_NORTH			0
#define PITS_EAST			1
#define PITS_SOUTH			2
#define PITS_WEST			3
#define PITS_NO_DIR			-1

// what stands on a square
#define PITS_O_NOTHING			0
#define PITS_O_WALL			1
#define PITS_O_SIGN			5
#define PITS_O_PILLAR			9
#define PITS_O_SWITCH_OFF		13
#define PITS_O_SWITCH_ON		17
#define PITS_O_BARREL_WATER		21
#define PITS_O_BARREL_HOLE		25
#define PITS_O_BARREL			29
#define PITS_O_BARREL_HOME		33
#define PITS_O_CRATE			37
#define PITS_O_CRATE_HOME		41
#define PITS_O_CHEST			45
#define PITS_O_CHEST_HOME		49
#define PITS_O_PLAYER			100

#define PITS_IS_BOX(o)			((o) >= PITS_O_BARREL && (o) < 53)
#define PITS_IS_CHEST(o)		((o) == PITS_O_CHEST || (o) == PITS_O_CHEST_HOME)
#define PITS_IS_AT_HOME(o)		((o) == PITS_O_BARREL_HOME ||	\
					 (o) == PITS_O_CRATE_HOME ||	\
					 (o) == PITS_O_CHEST_HOME)

// the ground
#define PITS_U_VOID			0
#define PITS_U_FLOOR			53
#define PITS_U_TARGET_RED		54
#define PITS_U_TARGET_GREEN		55
#define PITS_U_TARGET_BLUE		56
#define PITS_U_MARKED			57
#define PITS_U_PLATE_UP			58
#define PITS_U_PLATE_DOWN		59
#define PITS_U_ARROW_UP			60
#define PITS_U_ARROW_LEFT		63
#define PITS_U_GEM_RED			64
#define PITS_U_GEM_YELLOW		67
#define PITS_U_KEY_RED			68
#define PITS_U_KEY_BLUE			70
#define PITS_U_GLASS			71
#define PITS_U_LOCK_RED			76
#define PITS_U_CRATE_WATER		79
#define PITS_U_CRATE_HOLE		80
#define PITS_U_WOOD			81
#define PITS_U_GRASS			82
#define PITS_U_HOLE			83
#define PITS_U_WATER			84
#define PITS_U_LAVA			88
#define PITS_U_LAVA_COOLING_1		92
#define PITS_U_LAVA_COOLING_2		96

#define PITS_IS_ARROW(u)		((u) >= PITS_U_ARROW_UP && (u) <= PITS_U_ARROW_LEFT)
#define PITS_IS_WATER(u)		((u) >= PITS_U_WATER && (u) < PITS_U_LAVA)
#define PITS_IS_LAVA(u)			((u) >= PITS_U_LAVA && (u) < PITS_U_LAVA_COOLING_1)
#define PITS_IS_COOLING(u)		((u) >= PITS_U_LAVA_COOLING_1 && (u) < 100)
#define PITS_IS_TARGET(u)		((u) >= PITS_U_TARGET_RED && (u) <= PITS_U_TARGET_BLUE)

// gems and keys
#define PITS_RED			0
#define PITS_GREEN			1
#define PITS_BLUE			2
#define PITS_YELLOW			3
#define PITS_NO_GEM			-1

#define PITS_GHOST_STAGE		7

// the sounds
enum
{
  PITS_SND_CRACK,
  PITS_SND_GLASS,
  PITS_SND_LAVA,
  PITS_SND_MAGIC,
  PITS_SND_PLATEOFF,
  PITS_SND_PLATEON,
  PITS_SND_SCRAPE,
  PITS_SND_SPLINTER,
  PITS_SND_WATER,

  PITS_NUM_SOUNDS
};


// ----------------------------------------------------------------------------
// data structure definitions
// ----------------------------------------------------------------------------

// a level file, as read
struct PitsFileInfo
{
  char title[PITS_TITLE_LEN + 1];
  char password[PITS_PASSWORD_LEN + 1];
  int under[PITS_SIZE][PITS_SIZE];
  int over[PITS_SIZE][PITS_SIZE];
  int script_map[PITS_SIZE][PITS_SIZE];

  // the scripts, by their number: where their code starts, or -1
  int script_start[PITS_MAX_SCRIPTS];
  char script_name[PITS_MAX_SCRIPTS][PITS_SCRIPT_NAME_LEN + 1];

  unsigned char *code;
  int code_size;
};

// the game, as it is played
struct PitsInfo
{
  struct PitsFileInfo file;

  int under[PITS_SIZE][PITS_SIZE];
  int over[PITS_SIZE][PITS_SIZE];
  int last_pushed[PITS_SIZE][PITS_SIZE];

  int px, py;				// the player
  int lastx, lasty;
  int facing;				// 0, 4, 8, 12

  boolean walkable[101];

  int moves;
  int gems[4];				// red, green, blue; yellow: 0 or 1
  int keys[3];
  boolean gem_on[3];			// red, green, blue: each in use on its own
  int gem_in_use;			// the one put in use last, or PITS_NO_GEM
  int green_stage;			// 1 to 7 while the green gem is in use
  int green_x, green_y;			// where the player became a ghost

  int reg[PITS_NUM_REGISTERS];
  int xpos, ypos;			// the running script's square
  char input[PITS_INPUT_LEN + 1];
  int input_value;

  // the squares with arrows and glass, in the order they move things
  int arrows_x[PITS_SIZE * PITS_SIZE];
  int arrows_y[PITS_SIZE * PITS_SIZE];
  boolean in_arrows[PITS_SIZE][PITS_SIZE];
  int num_arrows;
  boolean arrived[PITS_SIZE][PITS_SIZE];	// objects moved here this turn

  boolean won;

  // the scripts that the game knows by name
  int script_intro, script_master;
  int script_gem[3];

  // time and input
  int frame;
  int tick;
  int last_turn_tick;
  int next_anim_tick;
  boolean started;
  boolean quiet_intro;			// a restart: Intro's text isn't shown again

  boolean pushing;
  int push_dir;
  int push_tick;
  boolean bump_ready;			// a sign or switch may be walked into

  boolean walking;
  int walk_x, walk_y;
  int walk_next[PITS_SIZE][PITS_SIZE];	// the way to go from each square

  int last_action;
  int last_button;


  // changing to a ghost and back (pictures only, a stage every turn's time)
  int change_dir;			// +1 to a ghost, -1 back, 0 none
  int change_stage;
  int change_turns;			// quarter turns to make first
  int pending_use;			// a gem or key pressed meanwhile

  // the pictures
  int walk_step;
  int last_move_tick;
  int anim_counter;
  int sparkle;
  boolean show_all;

  int sounds;				// played after the pit is drawn
};

// a script asking the player (INPUT, TARGET): the frame it happened in is
// played again from its start once the player has answered, with the answers
#define PITS_PROMPT_NONE		0
#define PITS_PROMPT_INPUT		1
#define PITS_PROMPT_TARGET		2

#define PITS_MAX_ANSWERS		16

// the typing room: its keys, ten across and four down
#define PITS_ROOM_COLS			10
#define PITS_ROOM_ROWS			4
#define PITS_KEY_SPACE			' '
#define PITS_KEY_RUBOUT			'<'
#define PITS_KEY_DONE			'>'

struct PitsPromptInfo
{
  int mode;

  int frame_action;			// the frame to play again
  struct MouseActionInfo frame_mouse;

  int num_answers;
  int next_answer;
  char answer_text[PITS_MAX_ANSWERS][PITS_INPUT_LEN + 1];
  int answer_x[PITS_MAX_ANSWERS];
  int answer_y[PITS_MAX_ANSWERS];

  int pages_shown;			// text pages already shown in this frame
  int page_counter;

  int cx, cy;				// the cursor, or the player in the room
  int facing;
  char typed[PITS_INPUT_LEN + 1];

  int last_action;
  int last_button;
  int hold_frames;
  boolean wait_release;			// no input until all is let go
};

// what was shown last (REDRAW, APPEAR, and every frame between turns)
struct PitsViewInfo
{
  int under[PITS_SIZE][PITS_SIZE];
  int over[PITS_SIZE][PITS_SIZE];
  int px, py;
  int player_shape;			// -1: not drawn as an object
  int ghost_shape;			// -1: no ghost
  boolean pushing_front;		// facing east or south, pushing
  int facing;
  int anim_counter;
  int sparkle;
  boolean show_all;

  int gems[4];				// for the strip of gems and keys
  int keys[3];
  boolean gem_on[3];
  int gem_in_use;
};


// ----------------------------------------------------------------------------
// exported variables
// ----------------------------------------------------------------------------

extern struct PitsInfo pits;
extern struct PitsViewInfo pits_view;
extern struct PitsPromptInfo pits_prompt;


// ----------------------------------------------------------------------------
// exported functions
// ----------------------------------------------------------------------------

// pits_main.c
boolean ReadLevelFile_PITS(unsigned char *, int, struct PitsFileInfo *, boolean);
void ShowLevelText_PITS(char *);
void SnapshotView_PITS(void);
void RememberCarry_PITS(void);
void getViewGeometry_PITS(int, int, int *, int *, int *, int *, int *, int *, int *);
void AnswerInput_PITS(char *);
void AnswerTarget_PITS(int *, int *);
int getRoomKey_PITS(int, int);

// pits_logic.c
void InitLevel_PITS(void);
void StartLevel_PITS(void);
void AnimStep_PITS(void);
void TryStep_PITS(int);
void Wait_PITS(void);
void UseGem_PITS(int);
void UseKey_PITS(int);
void TurnChecks_PITS(void);
void FinishGhost_PITS(void);
boolean CanEnter_PITS(int, int);
boolean FindWay_PITS(int, int);
boolean HasScript_PITS(int);
void CheckWin_PITS(void);
int getPlayerShape_PITS(void);
boolean IsGhost_PITS(void);
int getTargetsStillNeeded_PITS(void);

// pits_gfx.c
void InitGfx_PITS(char *);
void InitSound_PITS(char *);
void PlaySounds_PITS(int);
char *getKeyName_PITS(boolean);

#endif	// PITS_MAIN_H
