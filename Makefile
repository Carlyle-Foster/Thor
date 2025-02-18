# Options you can call this Makefile with
DEBUG  ?= 0 # Debug builds
LTO    ?= 0 # Link-time optimization
ASAN   ?= 0 # Address sanitizer
TSAN   ?= 0 # Thread sanitizer
UBSAN  ?= 0 # Undefined behavior sanitizer

# Disable all built-in rules and variables
MAKEFLAGS += --no-builtin-rules
MAKEFLAGS += --no-builtin-variables

# Some helper functions
rwildcard = $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))
uniq = $(if $1,$(firstword $1) $(call uniq,$(filter-out $(firstword $1),$1)))

UNAME := $(shell uname)

#
# C and C++ compiler
#
CC := gcc
CC ?= clang
# We use the C frontend with -xc++ to avoid linking in the C++ runtime library
CXX := $(CC) -xc++

# Determine if the C or C++ compiler should be used as the linker frontend
ifneq (,$(findstring -xc++,$(CXX)))
	LD := $(CC)
else
	LD := $(CXX)
endif

# Determine the type of build we're building
ifeq ($(DEBUG),1)
	TYPE  := debug
	STRIP := true
else
	TYPE  := release
	STRIP := strip
endif

OBJDIR := .build/$(TYPE)/objs
DEPDIR := .build/$(TYPE)/deps

SRCS := $(call rwildcard, src, *.cpp)
OBJS := $(filter %.o,$(SRCS:%.cpp=$(OBJDIR)/%.o))
DEPS := $(filter %.d,$(SRCS:%.cpp=$(DEPDIR)/%.d))

BIN := thor

#
# Dependency flags
#
DEPFLAGS := -MMD
DEPFLAGS += -MP

#
# C++ flags
#
CXXFLAGS := -Isrc
CXXFLAGS += -Wall
CXXFLAGS += -Wextra
CXXFLAGS += -std=c++23
CXXFLAGS += -fno-exceptions
CXXFLAGS += -fno-rtti
# Give each function and data its own section so the linker can remove unused
# references to each. This adds to the link time so it's not enabled by default
CXXFLAGS += -ffunction-sections
CXXFLAGS += -fdata-sections
# Enable LTO if requested
ifeq ($(LTO),1)
	CXXFLAGS += -flto
endif
ifeq ($(DEBUG),1)
	# Options for debug builds
	CXXFLAGS += -g
	CXXFLAGS += -O0
	CXXFLAGS += -fno-omit-frame-pointer
else
	# Options for release builds
	CXXFLAGS += -DNDEBUG
	CXXFLAGS += -Os
	CXXFLAGS += -fno-stack-protector
	CXXFLAGS += -fno-stack-check
	CXXFLAGS += -fno-unwind-tables
	CXXFLAGS += -fno-asynchronous-unwind-tables
endif
# Sanitizer selection
ifeq ($(ASAN),1)
	CXXFLAGS += -fsanitize=address
endif
ifeq ($(TSAN),1)
	CXXFLAGS += -fsanitize=thread
endif
ifeq ($(UBSAN),1)
	CXXFLAGS += -fsanitize=undefined
endif

#
# Linker flags
#
LDFLAGS := -ldl
ifneq ($(UNAME),Darwin)
	LDFLAGS += -static-libgcc
endif
ifeq ($(LTO),1)
	LDFLAGS += -flto
endif
# Sanitizer selection
ifeq ($(ASAN),1)
	LDFLAGS += -fsanitize=address
endif
ifeq ($(TSAN),1)
	LDFLAGS += -fsanitize=thread
endif
ifeq ($(UBSAN),1)
	LDFLAGS += -fsanitize=undefined
endif
LDFLAGS += -Wl,--gc-sections

all: $(BIN)

$(DEPDIR):
	@mkdir -p $(addprefix $(DEPDIR)/,$(call uniq,$(dir $(SRCS))))
$(OBJDIR):
	@mkdir -p $(addprefix $(OBJDIR)/,$(call uniq,$(dir $(SRCS))))

# The rule that compiles source files to object files
$(OBJDIR)/%.o: %.cpp $(DEPDIR)/%.d | $(OBJDIR) $(DEPDIR)
	$(CXX) -MT $@ $(DEPFLAGS) -MF $(DEPDIR)/$*.Td $(CXXFLAGS) -c -o $@ $<
	@mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d
	@touch $(DEPDIR)/$*.d

# The rule that links object files and makes the executable
$(BIN): $(OBJS)
	$(LD) $(OBJS) $(LDFLAGS) -o $@
	$(STRIP) $@

clean:
	rm -rf .build $(BIN)

.PHONY: all clean

$(DEPS):
include $(wildcard $(DEPS))