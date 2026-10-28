// ============================================================================
// The Puzzle Pits engine for Rocks'n'Diamonds
// ----------------------------------------------------------------------------
// export_pits.h: what game_pits gives the main program
// ============================================================================

#ifndef EXPORT_PITS_H
#define EXPORT_PITS_H

#define PITS_SIZE			23	// a pit, its edge always empty
#define PITS_TITLE_LEN			100
#define PITS_PASSWORD_LEN		20
#define PITS_MAX_FILE_SIZE		65536

// what gems and keys a level starts with (SetLevelCarry_PITS)
#define PITS_CARRY_NONE			0	// none (a level started on its own)
#define PITS_CARRY_NEXT			1	// those held when a level was last won
#define PITS_CARRY_RESTART		2	// those this level started with last time

struct GameInfo_PITS
{
  boolean level_solved;
  boolean game_over;
  int moves;
  int targets_still_needed;		// the targets without their colour on them
  int gems;				// red, green and blue gems carried
  boolean key[3];			// red, green and blue keys
};

struct LevelInfo_PITS
{
  char title[PITS_TITLE_LEN + 1];
  char password[PITS_PASSWORD_LEN + 1];
  int under[PITS_SIZE][PITS_SIZE];	// the ground
  int over[PITS_SIZE][PITS_SIZE];	// what stands on it (100: the player)

  char directory[1024];			// where the pictures are (TILES1.DAT ...)
  unsigned char file[PITS_MAX_FILE_SIZE];
  int file_size;
};

extern struct GameInfo_PITS game_pits;
extern struct LevelInfo_PITS native_pits_level;

void setLevelInfoToDefaults_PITS(void);
boolean LoadNativeLevel_PITS(char *, boolean);

void SetLevelCarry_PITS(int);		// just before InitGameEngine_PITS()
void InitGameEngine_PITS(void);
void GameActions_PITS(byte[MAX_PLAYERS], struct MouseActionInfo);
void RedrawPlayfield_PITS(boolean);
void BlitScreenToBitmap_PITS(Bitmap *);
unsigned int InitEngineRandom_PITS(int);
int getLevelFromScreenX_PITS(int);
int getLevelFromScreenY_PITS(int);

#endif	// EXPORT_PITS_H
