#############################################################################
# MAKEFILE - MAZE RUNNER 2 - P4 ERA DOS RAYCASTER
#
# Targeting Pentium 4 era hardware (1.5GHz+)
# High-res textures, enemies that shoot back, advanced combat
#
# By: VonHoltenCodes (2025)
#############################################################################

# DJGPP Cross-compiler
CC = i586-pc-msdosdjgpp-gcc

# Optimizations - compatible with Pentium and above
# Note: -march=pentium4 causes SIGILL on older CPUs, use i586 for compatibility
CFLAGS = -Wall -O2 -march=i586 -ffast-math -funroll-loops -DMAZE_RUNNER_2

LDFLAGS = -lm -s

# Target executable
TARGET = MAZE2.EXE

# Source files
SOURCES = maze2.c

# Object files
OBJECTS = $(SOURCES:.c=.o)

# Default target
all: $(TARGET)

$(TARGET): $(OBJECTS)
	@echo ""
	@echo "========================================="
	@echo " LINKING MAZE RUNNER 2..."
	@echo "========================================="
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo ""
	@echo "========================================="
	@echo " MAZE RUNNER 2 - BUILD COMPLETE!"
	@echo "========================================="
	@echo " Executable: $(TARGET)"
	@echo " Size: $$(ls -lh $(TARGET) 2>/dev/null | awk '{print $$5}' || echo 'N/A')"
	@echo ""
	@echo " P4-ERA FEATURES:"
	@echo "  - 64x64 procedural textures"
	@echo "  - Enemies that SHOOT BACK"
	@echo "  - Projectile system"
	@echo "  - WASD + Mouse controls"
	@echo "  - Distance fog effects"
	@echo "========================================="

%.o: %.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
	@echo "Build files cleaned"

# Create floppy image
floppy: $(TARGET)
	@echo "Creating floppy disk image..."
	mkfs.fat -C MAZE2.IMG 1440
	mcopy -i MAZE2.IMG $(TARGET) ::
	@echo "Floppy image: MAZE2.IMG"

.PHONY: all clean floppy
