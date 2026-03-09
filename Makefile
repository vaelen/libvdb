CC = gcc
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -I./include
LDFLAGS =

# Directories
SRCDIR = src
OBJDIR = obj
BINDIR = bin
LIBDIR = lib
TESTDIR = tests
INCDIR = include

# Source files
LIB_SOURCES = $(SRCDIR)/platform.c $(SRCDIR)/hash.c $(SRCDIR)/btree.c $(SRCDIR)/db.c
LIB_OBJECTS = $(LIB_SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
TEST_SOURCES = $(wildcard $(TESTDIR)/*.c)
TEST_OBJECTS = $(TEST_SOURCES:$(TESTDIR)/%.c=$(OBJDIR)/%.o)
TEST_TARGETS = $(TEST_SOURCES:$(TESTDIR)/%.c=$(BINDIR)/%)

# Target library
LIBRARY = $(LIBDIR)/libvdb.a

# Default target
all: $(LIBRARY) $(TEST_TARGETS)

# Create directories if they don't exist
$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

$(LIBDIR):
	mkdir -p $(LIBDIR)

# Build the static library
$(LIBRARY): $(LIB_OBJECTS) | $(LIBDIR)
	ar rcs $(LIBRARY) $(LIB_OBJECTS)

# Build test executables
$(BINDIR)/%: $(OBJDIR)/%.o $(LIBRARY) | $(BINDIR)
	$(CC) $< $(LIBRARY) -o $@ $(LDFLAGS)

# Compile source files to object files
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile test files to object files
$(OBJDIR)/%.o: $(TESTDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -rf $(OBJDIR) $(BINDIR) $(LIBDIR) tmp

# Run tests
test: $(TEST_TARGETS)
	@for t in $(TEST_TARGETS); do echo "=== $$t ==="; ./$$t || exit 1; done

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: all

# Release build
release: CFLAGS += -O2 -DNDEBUG
release: all

# Unix build (removes -Wextra and -std=c89 for compatibility)
unix: CFLAGS = -Wall -pedantic -I./include
unix: all

# Check for memory leaks with valgrind
valgrind: $(TEST_TARGETS)
	@for t in $(TEST_TARGETS); do echo "=== valgrind $$t ==="; valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 ./$$t || exit 1; done

# Help target
help:
	@echo "Available targets:"
	@echo "  all       - Build the library and tests (default)"
	@echo "  test      - Build and run tests"
	@echo "  clean     - Remove build artifacts"
	@echo "  debug     - Build with debug symbols"
	@echo "  release   - Build optimized release version"
	@echo "  unix      - Build for older Unix systems (removes -Wextra and -std=c89)"
	@echo "  valgrind  - Run tests with valgrind memory checker"
	@echo "  help      - Show this help message"

.PHONY: all clean test debug release unix valgrind help
