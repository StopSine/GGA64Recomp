# Patch build for Goemon's Great Adventure.
#
# Every .c and .s here is built into patches.elf, which N64Recomp then
# recompiles. A patch names the game function it replaces, so dummy.c exists to
# keep the elf valid when nothing else is being patched. The headers alongside
# it declare the host API that both the patches and src/ compile against.

# Output definitions
TARGET_NAME := patches.elf
BUILD_DIR   := build
TARGET      := $(BUILD_DIR)/$(TARGET_NAME)

# Tools
CC      ?= clang
LD      ?= ld.lld
CMAKE   ?= cmake

# Flags
CFLAGS   := -target mips -mips2 -mabi=32 -O2 -G0 \
            -mno-abicalls -mno-odd-spreg -mno-check-zero-division \
            -fomit-frame-pointer -ffast-math -fno-unsafe-math-optimizations \
            -fno-builtin-memset -Wall -Wextra \
            -Wno-incompatible-library-redeclaration -Wno-unused-parameter \
            -Wno-unknown-pragmas -Wno-unused-variable \
            -Wno-missing-braces -Wno-unsupported-floating-point-opt

CPPFLAGS := -nostdinc -D_LANGUAGE_C -DMIPS -DNDEBUG \
            -I ../lib/gga/include \
            -I ../lib/gga/libultra/include \
            -I ../lib/gga/libultra/include/PR \
            -I ../lib/rt64/include

ASFLAGS  := -target mips -mips2 -mabi=32 -G0 \
            -mno-abicalls -mno-check-zero-division \
            -x assembler-with-cpp -modd-spreg

LDSCRIPTS := patches.ld syms.ld
LDFLAGS  := -nostdlib $(addprefix -T ,$(LDSCRIPTS)) \
            -Map $(BUILD_DIR)/patches.map \
            --unresolved-symbols=ignore-all --emit-relocs

# Source processing
C_SRCS := $(wildcard *.c)
C_OBJS := $(C_SRCS:%.c=$(BUILD_DIR)/%.o)
C_DEPS := $(C_OBJS:.o=.d)

# Assembly processing
S_SRCS := $(wildcard *.s)
S_OBJS := $(addprefix $(BUILD_DIR)/, $(S_SRCS:.s=.o))

# Default target
all: $(TARGET)

# Linker Rule
$(TARGET): $(C_OBJS) $(S_OBJS) $(LDSCRIPTS)
	@$(CMAKE) -E echo "Linking $@"
	@$(CMAKE) -E make_directory $(BUILD_DIR)
	$(LD) $(C_OBJS) $(S_OBJS) $(LDFLAGS) -o $@

# Compiler Rule
$(BUILD_DIR)/%.o: %.c
	@$(CMAKE) -E echo "Compiling $<"
	@$(CMAKE) -E make_directory $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -MMD -MF $(@:.o=.d) -c -o $@

$(BUILD_DIR)/%.o: %.s
	@$(CMAKE) -E echo "Compiling $<"
	@$(CMAKE) -E make_directory $(BUILD_DIR)
	$(CC) $(ASFLAGS) $(CPPFLAGS) $< -MMD -MF $(@:.o=.d) -c -o $@

clean:
	@$(CMAKE) -E echo "Cleaning build directory..."
	$(CMAKE) -E remove_directory $(BUILD_DIR)

-include $(C_DEPS)

.PHONY: all clean