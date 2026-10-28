// ============================================================================
// The Puzzle Pits engine for Rocks'n'Diamonds
// ----------------------------------------------------------------------------
// Written from the description of the game in PITS-RULES.md and of its level
// files in PIT-FORMAT.md.
// ----------------------------------------------------------------------------
// pits_gfx.c: the pit's pictures (TILES1.DAT to TILES3.DAT) and its sounds
// ============================================================================

#include "pits_main.h"

#include "../main.h"			// R'n'D's fonts, for the typing room


#define MAX_SHAPES			400
#define NUM_COLORS			64

#define SHAPE_VOID			0
#define SHAPE_SPARKLE			72

struct ShapeInfo
{
  int width, height;
  unsigned char *pixels;
};

static struct ShapeInfo shapes[MAX_SHAPES];
static int num_shapes = 0;
static char shapes_directory[1024] = "";

static Bitmap *pits_bitmap = NULL;
static Uint32 colors[256];

static Mix_Chunk *sounds[PITS_NUM_SOUNDS];
static char sounds_directory[1024] = "";

// the palette, 6 bits a component
static unsigned char palette[NUM_COLORS][3] =
{
  {  0,  0,  0 }, { 63, 63, 63 }, { 54, 54, 54 }, { 50, 50, 50 },
  { 42, 42, 42 }, { 37, 37, 37 }, { 25, 25, 25 }, { 63,  0,  0 },
  { 54,  0,  0 }, { 46,  0,  0 }, { 37,  0,  0 }, { 63, 63,  0 },
  { 63, 42,  0 }, {  0, 63, 63 }, { 29, 46, 63 }, { 33,  0, 33 },
  { 33, 63, 33 }, {  0, 63,  0 }, {  0, 46,  0 }, {  0, 33,  0 },
  {  0, 33, 63 }, {  0, 25, 58 }, {  0, 16, 50 }, {  0,  8, 46 },
  {  0,  4, 37 }, {  0,  0, 33 }, { 63, 37, 33 }, { 46, 21, 21 },
  { 33, 16,  0 }, { 29, 12,  0 }, { 25,  8,  0 }, { 21,  4,  0 },
  {  0,  0,  0 }, { 31, 31, 31 }, { 27, 27, 27 }, { 25, 25, 25 },
  { 21, 21, 21 }, { 18, 18, 18 }, { 12, 12, 12 }, { 31,  0,  0 },
  { 27,  0,  0 }, { 23,  0,  0 }, { 18,  0,  0 }, { 31, 31,  0 },
  { 31, 21,  0 }, {  0, 31, 31 }, { 14, 23, 31 }, { 16,  0, 16 },
  { 16, 31, 16 }, {  0, 31,  0 }, {  0, 23,  0 }, {  0, 16,  0 },
  {  0, 16, 31 }, {  0, 12, 29 }, {  0,  8, 25 }, {  0,  4, 23 },
  {  0,  2, 18 }, {  0,  0, 16 }, { 31, 18, 16 }, { 23, 10, 10 },
  { 16,  8,  0 }, { 14,  6,  0 }, { 12,  4,  0 }, { 10,  2,  0 },
};

static void getRoomGeometry(int *, int *, int *);
static void DrawItems(void);

static char *sound_names[PITS_NUM_SOUNDS] =
{
  "CRACK.WAV",
  "GLASS.WAV",
  "LAVA.WAV",
  "MAGIC.WAV",
  "PLATEOFF.WAV",
  "PLATEON.WAV",
  "SCRAPE.WAV",
  "SPLINTER.WAV",
  "WATER.WAV",
};


// ----------------------------------------------------------------------------
// files next to the levels, in capitals or not
// ----------------------------------------------------------------------------

static char *getPitsFilename(char *directory, char *name)
{
  char *filename = getPath2(directory, name);
  char lower[64];
  int i;

  if (fileExists(filename))
    return filename;

  free(filename);

  for (i = 0; name[i] != '\0' && i < 63; i++)
    lower[i] = tolower((unsigned char)name[i]);
  lower[i] = '\0';

  return getPath2(directory, lower);
}


// ----------------------------------------------------------------------------
// the shapes
// ----------------------------------------------------------------------------

static void FreeShapes(void)
{
  int i;

  for (i = 0; i < num_shapes; i++)
    checked_free(shapes[i].pixels);

  num_shapes = 0;
}

static void LoadShapeFile(char *filename)
{
  FILE *file = fopen(filename, "rb");

  if (file == NULL)
  {
    Warn("cannot read Puzzle Pits pictures '%s'", filename);

    return;
  }

  while (num_shapes < MAX_SHAPES)
  {
    int width = fgetc(file);
    int height = fgetc(file);
    struct ShapeInfo *shape = &shapes[num_shapes];

    if (width <= 0 || height < 0)		// width 0 (or the end): no more
      break;

    shape->width = width;
    shape->height = height;
    shape->pixels = checked_calloc(width * height + 1);

    if (fread(shape->pixels, 1, width * height, file) != width * height)
    {
      free(shape->pixels);

      break;
    }

    num_shapes++;
  }

  fclose(file);
}

void InitGfx_PITS(char *directory)
{
  int i;

  for (i = 0; i < 256; i++)
  {
    int r = (i < NUM_COLORS ? palette[i][0] : 0);
    int g = (i < NUM_COLORS ? palette[i][1] : 0);
    int b = (i < NUM_COLORS ? palette[i][2] : 0);

    colors[i] = (0xff000000 |
		 ((r * 4 + r / 16) << 16) |
		 ((g * 4 + g / 16) << 8) |
		 (b * 4 + b / 16));
  }

  if (strcmp(directory, shapes_directory) == 0 && num_shapes > 0)
    return;

  FreeShapes();

  for (i = 1; i <= 3; i++)
  {
    char name[32];
    char *filename;

    sprintf(name, "TILES%d.DAT", i);
    filename = getPitsFilename(directory, name);
    LoadShapeFile(filename);
    free(filename);
  }

  strcpy(shapes_directory, directory);
}


// ----------------------------------------------------------------------------
// drawing
// ----------------------------------------------------------------------------

static Uint32 *draw_pixels;
static int draw_pitch;			// in pixels
static int draw_width, draw_height;
static int draw_scale;
static boolean draw_dimmed;		// the strip's gems and keys not held

// a colour as a thing not held: darker, and greyer
static Uint32 getDimmedColor(Uint32 color)
{
  int r = (color >> 16) & 0xff, g = (color >> 8) & 0xff, b = color & 0xff;
  int grey = (r + g + b) / 3;

  r = (r + grey) / 4;
  g = (g + grey) / 4;
  b = (b + grey) / 4;

  return (0xff000000 | (r << 16) | (g << 8) | b);
}

static void DrawShape(int nr, int x, int y, boolean masked)
{
  struct ShapeInfo *shape;
  int sx, sy, i, j;

  if (nr < 0 || nr >= num_shapes)
    return;

  shape = &shapes[nr];

  for (sy = 0; sy < shape->height; sy++)
  {
    for (sx = 0; sx < shape->width; sx++)
    {
      int index = shape->pixels[sy * shape->width + sx];
      Uint32 color = colors[index];

      if (masked && index == 0)
	continue;

      if (draw_dimmed)
	color = getDimmedColor(color);

      for (j = 0; j < draw_scale; j++)
      {
	int yy = y + sy * draw_scale + j;

	if (yy < 0 || yy >= draw_height)
	  continue;

	for (i = 0; i < draw_scale; i++)
	{
	  int xx = x + sx * draw_scale + i;

	  if (xx >= 0 && xx < draw_width)
	    draw_pixels[yy * draw_pitch + xx] = color;
	}
      }
    }
  }
}

// what stands on a square, as a shape (the first of its four)
static int getObjectShape(int x, int y)
{
  if (!PITS_IN_ARRAY(x, y))
    return 0;

  if (x == pits_view.px && y == pits_view.py && pits_view.player_shape >= 0)
    return pits_view.player_shape;

  return pits_view.over[x][y];
}

static int getGroundShape(int x, int y, int frame, boolean sparkle)
{
  int under;

  // outside the pit: the void
  if (x < 0 || x > PITS_MAX || y < 0 || y > PITS_MAX)
    return SHAPE_VOID;

  under = pits_view.under[x][y];

  if (PITS_IS_WATER(under))
    return PITS_U_WATER + frame;
  if (PITS_IS_LAVA(under))
    return PITS_U_LAVA + frame;
  if (under >= PITS_U_LAVA_COOLING_1 && under < PITS_U_LAVA_COOLING_2)
    return PITS_U_LAVA_COOLING_1 + frame;
  if (under >= PITS_U_LAVA_COOLING_2 && under < 100)
    return PITS_U_LAVA_COOLING_2 + frame;
  if (under == PITS_U_GLASS && sparkle)
    return SHAPE_SPARKLE + frame;

  return under;
}

// the parts of the objects that reach into a square, drawn on it
static void DrawObjectParts(int x, int y, int sx, int sy)
{
  static int dx[4] = { 0, 1, 0, 1 };
  static int dy[4] = { 0, 0, 1, 1 };
  static int quarter[4] = { 3, 2, 1, 0 };
  int i;

  for (i = 0; i < 4; i++)
  {
    int shape = getObjectShape(x + dx[i], y + dy[i]);

    if (shape > 0)
      DrawShape(shape + quarter[i], sx, sy, TRUE);
  }
}

static void DrawPit(int vx0, int vy0, int ox, int oy, int cols, int rows)
{
  int square = PITS_SQUARE * draw_scale;
  int px = pits_view.px, py = pits_view.py;
  int i, j;

  for (j = 0; j < rows; j++)
  {
    for (i = 0; i < cols; i++)
    {
      int x = vx0 + i, y = vy0 + j;
      int sx = ox + i * square, sy = oy + j * square;
      int nr = j * cols + i;
      int frame = (pits_view.anim_counter + nr) & 3;

      DrawShape(getGroundShape(x, y, frame, nr == pits_view.sparkle),
		sx, sy, FALSE);

      DrawObjectParts(x, y, sx, sy);
    }
  }

  // pushing east or south: the player's front over the box
  if (pits_view.pushing_front && pits_view.player_shape >= 0)
  {
    boolean east = (pits_view.facing == PITS_EAST * 4);
    int qx[2], qy[2], q[2];
    int bx = px + (east ? 1 : 0), by = py + (east ? 0 : 1);

    qx[0] = px;			qy[0] = (east ? py - 1 : py);
    q[0] = (east ? 1 : 2);
    qx[1] = px;			qy[1] = py;
    q[1] = 3;

    if (!east)
      qx[0] = px - 1;

    for (i = 0; i < 2; i++)
    {
      int sx = ox + (qx[i] - vx0) * square;
      int sy = oy + (qy[i] - vy0) * square;
      int below_x[2] = { qx[i], qx[i] + 1 };
      int below_y[2] = { qy[i] + 1, qy[i] + 1 };
      int below_q[2] = { 1, 0 };
      int k;

      DrawShape(pits_view.player_shape + q[i], sx, sy, TRUE);

      for (k = 0; k < 2; k++)
      {
	int shape;

	if ((below_x[k] == bx && below_y[k] == by) ||
	    (below_x[k] == px && below_y[k] == py))
	  continue;

	shape = getObjectShape(below_x[k], below_y[k]);

	if (shape > 0)
	  DrawShape(shape + below_q[k], sx, sy, TRUE);
      }
    }
  }

  // a ghost: over everything
  if (pits_view.ghost_shape >= 0)
  {
    int sx = ox + (px - vx0) * square;
    int sy = oy + (py - vy0) * square;

    DrawShape(pits_view.ghost_shape + 0, sx - square, sy - square, TRUE);
    DrawShape(pits_view.ghost_shape + 1, sx,          sy - square, TRUE);
    DrawShape(pits_view.ghost_shape + 2, sx - square, sy,          TRUE);
    DrawShape(pits_view.ghost_shape + 3, sx,          sy,          TRUE);
  }
}

// the typing room: floor squares with the keys, the player on one of them
static void DrawRoom(void)
{
  int ox, oy, scale, square, x, y;

  getRoomGeometry(&ox, &oy, &scale);
  draw_scale = scale;
  square = PITS_SQUARE * scale;

  for (y = 0; y < PITS_ROOM_ROWS; y++)
    for (x = 0; x < PITS_ROOM_COLS; x++)
      DrawShape(PITS_U_FLOOR, ox + x * square, oy + y * square, FALSE);

  // the player: an object over two by two squares, up and left
  x = ox + pits_prompt.cx * square;
  y = oy + pits_prompt.cy * square;

  DrawShape(100 + pits_prompt.facing + 0, x - square, y - square, TRUE);
  DrawShape(100 + pits_prompt.facing + 1, x,          y - square, TRUE);
  DrawShape(100 + pits_prompt.facing + 2, x - square, y,          TRUE);
  DrawShape(100 + pits_prompt.facing + 3, x,          y,          TRUE);
}

// a frame on a square: the TARGET cursor, the gem in use
static void DrawFrame(int x, int y, int size, Uint32 color)
{
  int width = 2 * draw_scale;
  int i, j;

  for (j = 0; j < size; j++)
  {
    for (i = 0; i < size; i++)
    {
      int xx = x + i, yy = y + j;

      if (i >= width && i < size - width && j >= width && j < size - width)
	continue;

      if (xx >= 0 && xx < draw_width && yy >= 0 && yy < draw_height)
	draw_pixels[yy * draw_pitch + xx] = color;
    }
  }
}

// the whole pit (the yellow gem), as small as it fits
static void getWholePitGeometry(int *vx0, int *vy0, int *ox, int *oy,
				int *cols, int *rows, int *scale)
{
  int s = MAX(1, MIN(SXSIZE, SYSIZE) / (PITS_SIZE * PITS_SQUARE));

  *scale = s;
  *vx0 = 0;
  *vy0 = 0;
  *cols = PITS_SIZE;
  *rows = PITS_SIZE;
  *ox = (SXSIZE - PITS_SIZE * PITS_SQUARE * s) / 2;
  *oy = (SYSIZE - PITS_SIZE * PITS_SQUARE * s) / 2;
}

// the typing room: its keys in the middle, the line typed above them
static void getRoomGeometry(int *ox, int *oy, int *scale)
{
  int vx0, vy0, ox_pit, oy_pit, cols, rows;
  int square;

  getViewGeometry_PITS(0, 0, &vx0, &vy0, &ox_pit, &oy_pit, &cols, &rows, scale);

  square = PITS_SQUARE * *scale;
  *ox = (SXSIZE - PITS_ROOM_COLS * square) / 2;
  *oy = (SYSIZE - PITS_ROOM_ROWS * square) / 2 + square / 2;
}

// the strip of gems and keys: a column along the left edge, darker, with
// the gems and keys in its middle, as big as the pit's squares
static void getItemsGeometry(int *width, int *size, int *gap)
{
  int vx0, vy0, ox, oy, cols, rows, scale;

  getViewGeometry_PITS(0, 0, &vx0, &vy0, &ox, &oy, &cols, &rows, &scale);

  *size = PITS_SQUARE * scale;
  *gap = 3 * scale;
  *width = *size + 2 * *gap;
}

// where an item is: the gems, then a little apart the keys
static int getItemY(int item, int size, int gap)
{
  int height = PITS_NUM_ITEMS * (size + gap) + gap;

  return ((SYSIZE - height) / 2 + item * (size + gap) +
	  (item >= 4 ? 2 * gap : 0));
}

// the item at a place on the screen (in the playfield): -1 off the strip,
// PITS_NUM_ITEMS on it between the items
static int getItemFromScreen(int x, int y)
{
  int width, size, gap, i;

  getItemsGeometry(&width, &size, &gap);

  if (x < SX || x >= SX + width)
    return -1;

  for (i = 0; i < PITS_NUM_ITEMS; i++)
  {
    int item_y = SY + getItemY(i, size, gap);

    if (y >= item_y && y < item_y + size)
      return i;
  }

  return PITS_NUM_ITEMS;
}

static void DrawItems(void)
{
  int width, size, gap, i, x, y;

  getItemsGeometry(&width, &size, &gap);

  // the column, the pit darker under it
  for (y = 0; y < draw_height; y++)
    for (x = 0; x < width && x < draw_width; x++)
      draw_pixels[y * draw_pitch + x] =
	0xff000000 | ((draw_pixels[y * draw_pitch + x] >> 2) & 0x3f3f3f);

  draw_scale = size / PITS_SQUARE;

  for (i = 0; i < PITS_NUM_ITEMS; i++)
  {
    boolean gem = (i < 4);
    int count = (gem ? pits_view.gems[i] : pits_view.keys[i - 4]);
    boolean in_use = (gem && (pits_view.gem_in_use == i ||
			      (i == PITS_YELLOW && pits_view.show_all)));
    int shape = (gem ? PITS_U_GEM_RED + i : PITS_U_KEY_RED + i - 4);

    x = gap;
    y = getItemY(i, size, gap);

    draw_dimmed = (count <= 0);
    DrawShape(shape, x, y, FALSE);
    draw_dimmed = FALSE;

    if (in_use)
      DrawFrame(x - gap, y - gap, size + 2 * gap, colors[1]);	// white
  }
}

// how many of each are held, on the strip (the yellow gem: held or not)
static void DrawItemCounts(Bitmap *target_bitmap)
{
  int font_nr = FONT_INPUT_1;
  int font_width = getFontWidth(font_nr);
  int font_height = getFontHeight(font_nr);
  int width, size, gap, i;

  getItemsGeometry(&width, &size, &gap);

  for (i = 0; i < PITS_NUM_ITEMS; i++)
  {
    int count = (i < 4 ? pits_view.gems[i] : pits_view.keys[i - 4]);
    char text[16];

    if (i == PITS_YELLOW)
      continue;

    sprintf(text, "%d", MIN(count, 99));

    DrawTextExt(target_bitmap,
		SX + gap + size - strlen(text) * font_width,
		SY + getItemY(i, size, gap) + size - font_height,
		text, font_nr, BLIT_MASKED);
  }
}

static void getShownGeometry(int *vx0, int *vy0, int *ox, int *oy,
			     int *cols, int *rows, int *scale)
{
  if (pits_prompt.mode == PITS_PROMPT_TARGET)
    getViewGeometry_PITS(pits_prompt.cx, pits_prompt.cy, vx0, vy0, ox, oy,
			 cols, rows, scale);
  else if (pits_view.show_all)
    getWholePitGeometry(vx0, vy0, ox, oy, cols, rows, scale);
  else
    getViewGeometry_PITS(pits_view.px, pits_view.py, vx0, vy0, ox, oy,
			 cols, rows, scale);
}

static void RenderView(void)
{
  SDL_Surface *surface;
  int vx0, vy0, ox, oy, cols, rows, scale;

  if (program.headless)
    return;

  if (pits_bitmap == NULL ||
      pits_bitmap->width != SXSIZE || pits_bitmap->height != SYSIZE)
  {
    if (pits_bitmap != NULL)
      FreeBitmap(pits_bitmap);

    pits_bitmap = CreateBitmapStruct();
    pits_bitmap->surface =
      SDL_CreateRGBSurfaceWithFormat(0, SXSIZE, SYSIZE, 32,
				     SDL_PIXELFORMAT_ARGB8888);
    pits_bitmap->width = SXSIZE;
    pits_bitmap->height = SYSIZE;

    if (pits_bitmap->surface == NULL)
      Fail("SDL_CreateRGBSurfaceWithFormat() failed: %s", SDL_GetError());
  }

  surface = pits_bitmap->surface;

  SDL_FillRect(surface, NULL, colors[0]);

  SDL_LockSurface(surface);

  draw_pixels = surface->pixels;
  draw_pitch = surface->pitch / 4;
  draw_width = SXSIZE;
  draw_height = SYSIZE;


  if (pits_prompt.mode == PITS_PROMPT_INPUT)
  {
    DrawRoom();
  }
  else
  {
    getShownGeometry(&vx0, &vy0, &ox, &oy, &cols, &rows, &scale);
    draw_scale = scale;

    DrawPit(vx0, vy0, ox, oy, cols, rows);

    if (pits_prompt.mode == PITS_PROMPT_TARGET)
      DrawFrame(ox + (pits_prompt.cx - vx0) * PITS_SQUARE * scale,
		oy + (pits_prompt.cy - vy0) * PITS_SQUARE * scale,
		PITS_SQUARE * scale, colors[11]);	// yellow
    else
      DrawItems();
  }

  SDL_UnlockSurface(surface);
}

// a line of help in the typing room: the longest that fits
static void DrawRoomHelp(Bitmap *target_bitmap, int y)
{
  static char *formats[] =
  {
    "%s: type the letter you stand on   OK: done",
    "%s: type the letter   OK: done",
    "%s: type   OK: done",
    NULL
  };
  int font_nr = FONT_INPUT_1;
  int font_width = getFontWidth(font_nr);
  char *snap_key = getKeyName_PITS(TRUE);
  char names[3][64];
  char text[128] = "";
  int i, j;

  // the snap key's name, with the action's or without; or the action's
  if (snap_key != NULL)
  {
    sprintf(names[0], "Snap (%s)", snap_key);
    strcpy(names[1], snap_key);
  }
  else
  {
    strcpy(names[0], "Snap");
    strcpy(names[1], "Snap");
  }
  strcpy(names[2], "Snap");

  for (j = 0; j < 3; j++)
  {
    for (i = 0; formats[i] != NULL; i++)
    {
      sprintf(text, formats[i], names[j]);

      if (strlen(text) * font_width <= SXSIZE)
	break;
    }

    if (formats[i] != NULL)
      break;
  }

  DrawTextExt(target_bitmap, SX + (SXSIZE - strlen(text) * font_width) / 2, y,
	      text, font_nr, BLIT_MASKED);
}

// the words on the room's keys, and what the player is asked
static void DrawPromptText(Bitmap *target_bitmap)
{
  int font_nr = FONT_INPUT_1;
  int font_width = getFontWidth(font_nr);
  int font_height = getFontHeight(font_nr);
  int ox, oy, scale, square, x, y;
  char line[PITS_INPUT_LEN + 4];

  if (pits_prompt.mode == PITS_PROMPT_TARGET)
  {
    char *text = "CHOOSE A SQUARE";

    DrawTextExt(target_bitmap,
		SX + (SXSIZE - strlen(text) * font_width) / 2,
		SY + font_height, text, font_nr, BLIT_MASKED);

    return;
  }

  getRoomGeometry(&ox, &oy, &scale);
  square = PITS_SQUARE * scale;

  for (y = 0; y < PITS_ROOM_ROWS; y++)
  {
    for (x = 0; x < PITS_ROOM_COLS; x++)
    {
      int key = getRoomKey_PITS(x, y);
      char text[3] = { key, '\0', '\0' };
      int sx = SX + ox + x * square;
      int sy = SY + oy + y * square;

      if (key == PITS_KEY_DONE)
      {
	// "done" takes two squares: its word once, across them
	if (getRoomKey_PITS(x - 1, y) == PITS_KEY_DONE)
	  continue;

	strcpy(text, "OK");
	sx += square / 2;
      }
      else if (key == PITS_KEY_SPACE)
      {
	text[0] = '_';
      }

      DrawTextExt(target_bitmap,
		  sx + (square - strlen(text) * font_width) / 2,
		  sy + (square - font_height) / 2, text, font_nr, BLIT_MASKED);
    }
  }

  // the line typed so far
  sprintf(line, "%s_", pits_prompt.typed);
  DrawTextExt(target_bitmap,
	      SX + (SXSIZE - (PITS_INPUT_LEN + 1) * font_width) / 2,
	      SY + oy - square - font_height, line, font_nr, BLIT_MASKED);

  // how to type, below the keys
  DrawRoomHelp(target_bitmap, SY + oy + PITS_ROOM_ROWS * square + square / 2);
}

void RedrawPlayfield_PITS(boolean force_redraw)
{
  RenderView();
}

void BlitScreenToBitmap_PITS(Bitmap *target_bitmap)
{
  RenderView();

  if (pits_bitmap != NULL)
    BlitBitmap(pits_bitmap, target_bitmap, 0, 0, SXSIZE, SYSIZE, SX, SY);

  if (program.headless)
    return;

  if (pits_prompt.mode != PITS_PROMPT_NONE)
    DrawPromptText(target_bitmap);
  else
    DrawItemCounts(target_bitmap);
}

static int getLevelFromScreen(int screen_pos, boolean horizontal)
{
  // R'n'D asks for x, then for y: x is kept for the strip of gems and keys
  static int last_screen_x = -1;
  int vx0, vy0, ox, oy, cols, rows, scale;
  int square, pos;

  if (horizontal)
    last_screen_x = screen_pos;

  // the strip of gems and keys: outside the level (see export_pits.h)
  if (pits_prompt.mode == PITS_PROMPT_NONE)
  {
    int item = getItemFromScreen(last_screen_x, screen_pos);

    if (item != -1)
      return (horizontal ? PITS_ITEMS_LX : PITS_ITEMS_LY + item);
  }

  // the typing room: its keys' places
  if (pits_prompt.mode == PITS_PROMPT_INPUT)
  {
    getRoomGeometry(&ox, &oy, &scale);
    square = PITS_SQUARE * scale;
    pos = screen_pos - (horizontal ? SX + ox : SY + oy);

    return (pos < 0 ? -1 : pos / square);
  }

  getShownGeometry(&vx0, &vy0, &ox, &oy, &cols, &rows, &scale);

  square = PITS_SQUARE * scale;
  pos = screen_pos - (horizontal ? SX + ox : SY + oy);

  if (pos < 0)
    return -1;

  // R'n'D's playfield is the inside of the pit (1 to 21)
  return (horizontal ? vx0 : vy0) + pos / square - 1;
}

int getLevelFromScreenX_PITS(int x)
{
  return getLevelFromScreen(x, TRUE);
}

int getLevelFromScreenY_PITS(int y)
{
  return getLevelFromScreen(y, FALSE);
}


// the name of the snap or drop key, as "Left Ctrl"; NULL if it has none
char *getKeyName_PITS(boolean snap)
{
  static char names[2][64];
  char *name = names[snap ? 1 : 0];
  Key key = (snap ? setup.input[0].key.snap : setup.input[0].key.drop);
  char *key_name;
  int i;

  if (setup.input[0].use_joystick || key == KSYM_UNDEFINED)
    return NULL;

  key_name = getKeyNameFromKey(key);

  if (key_name == NULL || key_name[0] == '\0' || strlen(key_name) > 40)
    return NULL;

  // "left control": "Left Ctrl"
  for (i = 0; key_name[i] != '\0'; i++)
    name[i] = (i == 0 || key_name[i - 1] == ' ' ?
	       toupper((unsigned char)key_name[i]) : key_name[i]);
  name[i] = '\0';

  if (strstr(name, "Control") != NULL)
  {
    char *control = strstr(name, "Control");

    memmove(control + 4, control + 7, strlen(control + 7) + 1);
    memcpy(control, "Ctrl", 4);
  }

  return name;
}


// ----------------------------------------------------------------------------
// the sounds
// ----------------------------------------------------------------------------

void InitSound_PITS(char *directory)
{
  int i;

  if (!audio.sound_available)
    return;

  if (strcmp(directory, sounds_directory) == 0)
    return;

  for (i = 0; i < PITS_NUM_SOUNDS; i++)
  {
    char *filename = getPitsFilename(directory, sound_names[i]);

    if (sounds[i] != NULL)
      Mix_FreeChunk(sounds[i]);

    sounds[i] = Mix_LoadWAV(filename);

    free(filename);
  }

  strcpy(sounds_directory, directory);
}

void PlaySounds_PITS(int sound_mask)
{
  int i;

  if (sound_mask == 0 || !audio.sound_available || !audio.sound_enabled ||
      !setup.sound_simple)
    return;

  for (i = 0; i < PITS_NUM_SOUNDS; i++)
    if ((sound_mask & (1 << i)) && sounds[i] != NULL)
      Mix_PlayChannel(-1, sounds[i], 0);
}
