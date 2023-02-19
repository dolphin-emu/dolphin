# supported parameters
#  ARCH          architecture - "x86" or "x64" [detected if not set]
#  BUILD         use flags for specified upstream consumer - "c89" or "retroarch" [default to "c89]
#  DEBUG         if set to anything, builds with DEBUG symbols
#  HAVE_LUA      if set to anything, includes the lua functionality

RC_SRC=../src/rcheevos
RC_URL_SRC=../src/rurl
RC_HASH_SRC=../src/rhash
RC_API_SRC=../src/rapi
LUA_SRC=lua/src

# default parameter values
ifeq ($(ARCH),)
    UNAME := $(shell uname -s)
    ifeq ($(findstring MINGW64, $(UNAME)), MINGW64)
        ARCH=x64
    else ifeq ($(findstring MINGW32, $(UNAME)), MINGW32)
        ARCH=x86
    else
        $(error Could not determine ARCH)
    endif
endif

ifeq ($(BUILD),)
    BUILD=c89
endif

# OS specific stuff
ifeq ($(OS),Windows_NT)
    EXE=.exe
else ifeq ($(findstring mingw, $(CC)), mingw)
    EXE=.exe
else
    EXE=
endif

# source files
OBJ=$(RC_SRC)/alloc.o \
    $(RC_SRC)/compat.o \
    $(RC_SRC)/condition.o \
    $(RC_SRC)/condset.o \
    $(RC_SRC)/consoleinfo.o \
    $(RC_SRC)/format.o \
    $(RC_SRC)/lboard.o \
    $(RC_SRC)/memref.o \
    $(RC_SRC)/operand.o \
    $(RC_SRC)/rc_libretro.o \
    $(RC_SRC)/rc_validate.o \
    $(RC_SRC)/richpresence.o \
    $(RC_SRC)/runtime.o \
    $(RC_SRC)/runtime_progress.o \
    $(RC_SRC)/trigger.o \
    $(RC_SRC)/value.o \
    $(RC_HASH_SRC)/cdreader.o \
    $(RC_HASH_SRC)/hash.o \
    $(RC_HASH_SRC)/md5.o \
    $(RC_URL_SRC)/url.o \
    $(RC_API_SRC)/rc_api_common.o \
    $(RC_API_SRC)/rc_api_editor.o \
    $(RC_API_SRC)/rc_api_info.o \
    $(RC_API_SRC)/rc_api_runtime.o \
    $(RC_API_SRC)/rc_api_user.o \
    rcheevos/test_condition.o \
    rcheevos/test_condset.o \
    rcheevos/test_consoleinfo.o \
    rcheevos/test_format.o \
    rcheevos/test_lboard.o \
    rcheevos/test_memref.o \
    rcheevos/test_operand.o \
    rcheevos/test_rc_libretro.o \
    rcheevos/test_rc_validate.o \
    rcheevos/test_richpresence.o \
    rcheevos/test_runtime.o \
    rcheevos/test_runtime_progress.o \
    rcheevos/test_trigger.o \
    rcheevos/test_value.o \
    rurl/test_url.o \
    rhash/data.o \
    rhash/mock_filereader.o \
    rhash/test_cdreader.o \
    rhash/test_hash.o \
    rapi/test_rc_api_common.o \
    rapi/test_rc_api_editor.o \
    rapi/test_rc_api_info.o \
    rapi/test_rc_api_runtime.o \
    rapi/test_rc_api_user.o \
    test.o

# compile flags
CFLAGS=-Wall -Wno-long-long -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
INCLUDES=-I../include -I$(RC_SRC) -I.

ifeq ($(ARCH), x86)
    CFLAGS += -m32
    LDFLAGS += -m32
else ifeq ($(ARCH), x64)
    CFLAGS += -m64
    LDFLAGS += -m64
else
    $(error unknown ARCH "$(ARCH)")
endif

EXTRA=
ifdef DEBUG
    CFLAGS += -O0 -g
    EXTRA +=  DEBUG
else
    CFLAGS += -O3
endif

# more strict validation for source files to eliminate warnings/errors in upstream consumers
# 3DS build (retroarch) doesn't support signed char
SRC_CFLAGS=-pedantic -Wsign-compare -fno-signed-char

ifeq ($(BUILD), c89)
    CFLAGS += -std=c89
else ifeq ($(BUILD), retroarch)
    # RetroArch builds with gcc8 and gnu99 which adds some extra warning validations
    SRC_CFLAGS += -std=gnu99 -D_GNU_SOURCE -Wall -Warray-bounds=2
else
    $(error unknown BUILD "$(BUILD)")
endif

# LUA support
ifdef HAVE_LUA
  OBJ += $(LUA_SRC)/lapi.o $(LUA_SRC)/lcode.o $(LUA_SRC)/lctype.o $(LUA_SRC)/ldebug.o \
         $(LUA_SRC)/ldo.o $(LUA_SRC)/ldump.o $(LUA_SRC)/lfunc.o $(LUA_SRC)/lgc.o $(LUA_SRC)/llex.o \
         $(LUA_SRC)/lmem.o $(LUA_SRC)/lobject.o $(LUA_SRC)/lopcodes.o $(LUA_SRC)/lparser.o \
         $(LUA_SRC)/lstate.o $(LUA_SRC)/lstring.o $(LUA_SRC)/ltable.o $(LUA_SRC)/ltm.o \
         $(LUA_SRC)/lundump.o $(LUA_SRC)/lvm.o $(LUA_SRC)/lzio.o $(LUA_SRC)/lauxlib.o \
         $(LUA_SRC)/lbaselib.o $(LUA_SRC)/lbitlib.o $(LUA_SRC)/lcorolib.o $(LUA_SRC)/ldblib.o \
         $(LUA_SRC)/liolib.o $(LUA_SRC)/lmathlib.o $(LUA_SRC)/loslib.o $(LUA_SRC)/lstrlib.o \
         $(LUA_SRC)/ltablib.o $(LUA_SRC)/lutf8lib.o $(LUA_SRC)/loadlib.o $(LUA_SRC)/linit.o

  CFLAGS += -DLUA_32BITS

  ifeq ($(BUILD), c89)
    CFLAGS += -DLUA_USE_C89
  endif

  INCLUDES += -I$(LUA_SRC)
  EXTRA += |HAVE_LUA
else
  CFLAGS += -DRC_DISABLE_LUA -Werror
endif

# recipes
$(info ==== rcheevos test [$(BUILD)/$(ARCH)$(EXTRA)] ====)

all: test

$(RC_SRC)/%.o: $(RC_SRC)/%.c
	$(CC) $(CFLAGS) $(SRC_CFLAGS) $(INCLUDES) -c $< -o $@

$(RC_HASH_SRC)/%.o: $(RC_HASH_SRC)/%.c
	$(CC) $(CFLAGS) $(SRC_CFLAGS) $(INCLUDES) -c $< -o $@

$(RC_URL_SRC)/%.o: $(RC_URL_SRC)/%.c
	$(CC) $(CFLAGS) $(SRC_CFLAGS) $(INCLUDES) -c $< -o $@

$(RC_API_SRC)/%.o: $(RC_API_SRC)/%.c
	$(CC) $(CFLAGS) $(SRC_CFLAGS) $(INCLUDES) -c $< -o $@

$(LUA_SRC)/%.o: $(LUA_SRC)/%.c
	$(CC) $(CFLAGS) -I$(LUA_SRC) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

test: $(OBJ)
	$(CC) $(LDFLAGS) -o $@$(EXE) $+ -lm
	@echo ---------------------------------

check_ctype:
	@if grep -rnI "isalpha([^(u]" ../src/*; \
	then echo "*** Error: isalpha without unsigned char cast" && false; \
	fi
	@if grep -rnI "isalnum([^(u]" ../src/*; \
	then echo "*** Error: isalnum without unsigned char cast" && false; \
	fi
	@if grep -rnI "isdigit([^(u]" ../src/*; \
	then echo "*** Error: isdigit without unsigned char cast" && false; \
	fi
	@if grep -rnI "isspace([^(u]" ../src/*; \
	then echo "*** Error: isspace without unsigned char cast" && false; \
	fi

runtests: test
	@./test$(EXE)

valgrind: test
	@valgrind --leak-check=full --error-exitcode=1 ./test$(EXE)

clean:
	rm -f test$(EXE) $(OBJ)
