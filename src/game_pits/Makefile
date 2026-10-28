# =============================================================================
# The Puzzle Pits engine for Rocks'n'Diamonds
# -----------------------------------------------------------------------------
# Makefile for game_pits (its game logic is DOS-era C: char is unsigned)
# =============================================================================

SRCS =	pits_main.c		\
	pits_logic.c		\
	pits_gfx.c

OBJS =	pits_main.o		\
	pits_logic.o		\
	pits_gfx.o

GAME_PITS = game_pits.a

all: $(GAME_PITS)

$(GAME_PITS): $(OBJS)
	$(AR) cr $(GAME_PITS) $(OBJS)
	$(RANLIB) $(GAME_PITS)

.c.o:
	$(CC) $(PROFILING) $(CFLAGS) -funsigned-char -c $*.c

# the engine's own headers: all its files use them
$(OBJS): pits_main.h export_pits.h game_pits.h

clean:
	$(RM) $(OBJS)
	$(RM) $(GAME_PITS)

depend:
	for i in $(SRCS); do $(CPP) $(CFLAGS) -M $$i; done > .depend

depend-clean:
	$(RM) .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif
