CC = gcc

CFLAGS = -Wall -Wextra -fPIC

LIBDIR = lib

# Define the directory for object files
OBJDIR = objs

# Common source files (assumed to be in the root directory)
COMMON_SRCS = src/cson_debug.c src/cson_common.c src/cson_parser.c src/cson_exec.c
COMMON_OBJS = $(addprefix $(OBJDIR)/, $(notdir $(COMMON_SRCS:.c=.o)))

$(info ${COMMON_OBJS})

STATIC_LIB_NAME = libcson.a
ifeq ($(OS),Windows_NT)
    SHARED_LIB_NAME = libcson.dll
    # For linking, Windows uses .dll, Linux uses .so. Adjust as needed.
    # The -l:libname.dll syntax is common for MinGW.
    # For runtime, you still need to ensure .dlls are in PATH or same dir.
else
    SHARED_LIB_NAME = libcson.so
endif
STATIC_LIB = $(addprefix $(LIBDIR)/, $(STATIC_LIB_NAME))
SHARED_LIB = $(addprefix $(LIBDIR)/, $(SHARED_LIB_NAME))

# Default target: builds all test executables
all: $(OBJDIR) $(STATIC_LIB) $(SHARED_LIB)

# --- Target and rules for building libraries ---
libraries: $(LIBDIR) $(STATIC_LIB) $(SHARED_LIB)

$(OBJDIR):
	@mkdir -p $(OBJDIR)

$(LIBDIR):
	@mkdir -p $(LIBDIR)

$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(STATIC_LIB): $(COMMON_OBJS) | $(LIBDIR)
	ar rcs $@ $^

# Rule for the shared library (.so)
$(SHARED_LIB): $(COMMON_OBJS) | $(LIBDIR)
	$(CC) $(CFLAGS) -shared -o $@ $^ $(LDFLAGS)

clean:
	@echo "Cleaning up..."
	rm -f $(COMMON_OBJS)
	rm -rf $(OBJDIR)
	rm $(STATIC_LIB)
	rm $(SHARED_LIB)
	
.PHONY: all clean run 