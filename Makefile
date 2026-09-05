# MAZE RUNNER 2 - DOS build (DJGPP cross-compiler)
# Requires i586-pc-msdosdjgpp-gcc on PATH (~/djgpp/bin on devbase1), or set DJGPP_CC.
#   make            -> build/MAZE2.EXE (self-contained: CWSDPMI stub bound in)
#   make dist       -> dist/MAZE2.IMG (bootable-floppy-ready 1.44 MB image) + dist/MAZE2.zip
#   make clean
CC       = $(or $(DJGPP_CC),i586-pc-msdosdjgpp-gcc)
CFLAGS  ?= -std=gnu99 -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-implicit-int -Wno-empty-body \
           -O2 -march=i586 -ffast-math -funroll-loops -Isrc -Ithird_party/midiplay
BUILD   ?= build
DIST    ?= dist
VERSION ?= $(shell grep -m1 '^#define MAZE2_VERSION' src/maze2.c | cut -d'"' -f2)

SRC     = src/maze2.c src/sound.c src/keyboard.c
OBJ     = $(patsubst src/%.c,$(BUILD)/%.o,$(SRC))
TARGET  = $(BUILD)/MAZE2.EXE
DATA    = assets/music/1.MID assets/music/FM.DAT
DOCS    = dos/README.TXT dos/MAZE2.BAT

all: $(TARGET)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/maze2.o: src/maze2.c src/sprites/*.h third_party/midiplay/MIDIPLAY.C src/sound.h src/keyboard.h

# Link, then swap the default go32 stub for CWSDSTUB (CWSDPMI r7 built in) so
# MAZE2.EXE needs no CWSDPMI.EXE beside it on bare DOS, floppies or DOSBox.
# build/sym/MAZE2.dbg keeps the symbols: decode a DJGPP crash traceback with
#   i586-pc-msdosdjgpp-addr2line -f -e build/sym/MAZE2.dbg 0x<eip> ...
# Both links go to build/sym/ first: the DJGPP driver also drops a lowercase
# "<name>.exe" with the plain stub next to any output, and DOS would pick that
# one over MAZE2.EXE if it landed in build/.
$(TARGET): $(OBJ) Makefile dos/CWSDSTUB.EXE
	mkdir -p $(BUILD)/sym
	$(CC) -o $(BUILD)/sym/MAZE2.dbg $(OBJ) -lm
	$(CC) -s -o $(BUILD)/sym/MAZE2_STUB.EXE $(OBJ) -lm
	python3 tools/exe2coff.py $(BUILD)/sym/MAZE2_STUB.EXE dos/CWSDSTUB.EXE $@
	@cp $(DATA) $(BUILD)/
	@ls -l $(TARGET)

# Release bundle: floppy image + zip, both carrying EXE, music data and docs.
dist: $(TARGET)
	mkdir -p $(DIST)
	rm -f $(DIST)/MAZE2.IMG $(DIST)/MAZE2.zip
	mkfs.fat -C $(DIST)/MAZE2.IMG 1440 >/dev/null
	mcopy -i $(DIST)/MAZE2.IMG $(TARGET) $(DATA) $(DOCS) ::
	cd $(BUILD) && zip -q -j ../$(DIST)/MAZE2.zip MAZE2.EXE 1.MID FM.DAT ../dos/README.TXT ../dos/MAZE2.BAT
	@echo "version $(VERSION)"; ls -l $(DIST)

# Diagnostic builds for real-hardware triage (tools/stage-dos.sh diag copies them):
#   MAZE2NL.EXE  = same game without the start-up memory lock
#   HELLO.EXE    = prints the DPMI host/version and waits, same stub binding as the game
diag: $(TARGET)
	mkdir -p $(BUILD)/diag $(BUILD)/sym
	$(CC) $(CFLAGS) -DNO_LOCK -c src/maze2.c -o $(BUILD)/sym/maze2_nolock.o
	$(CC) -s -o $(BUILD)/sym/MAZE2NL_STUB.EXE $(BUILD)/sym/maze2_nolock.o $(BUILD)/sound.o $(BUILD)/keyboard.o -lm
	python3 tools/exe2coff.py $(BUILD)/sym/MAZE2NL_STUB.EXE dos/CWSDSTUB.EXE $(BUILD)/diag/MAZE2NL.EXE
	$(CC) -s -o $(BUILD)/sym/HELLO_STUB.EXE tools/hello.c
	python3 tools/exe2coff.py $(BUILD)/sym/HELLO_STUB.EXE dos/CWSDSTUB.EXE $(BUILD)/diag/HELLO.EXE
	@ls -l $(BUILD)/diag

clean:
	rm -rf $(BUILD) $(DIST)

.PHONY: all dist diag clean
