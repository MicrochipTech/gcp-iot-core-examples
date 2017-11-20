.PHONY: all libcryptoauth dist install clean

OPTIONS := ATCAPRINTF

OUTDIR := $(abspath .build)

CRYPTOAUTHDIR := src/cryptoauthlib

# Check platform
ifeq ($(OS),Windows_NT)
# Special check for simulated windows environments
uname_S := $(shell cmd /C 'uname -s' 2>nul)
ifeq ($(uname_S),)
# Straight-up windows detected
uname_S := Windows
endif
else
uname_S := $(shell uname -s 2>/dev/null)
endif

# Define helpful macros for interacting with the specific environment
BACK2SLASH = $(subst \,/,$(1))
SLASH2BACK = $(subst /,\,$(1))

ifeq ($(uname_S),Windows)
# Windows commands
FIND = $(shell dir $(call SLASH2BACK,$(1)\$(2)) /W /B /S)
MKDIR = $(shell mkdir $(call SLASH2BACK,$(1)))
else
# Assume *nix like commands
FIND = $(shell find $(abspath $(1)) -name $(2))
MKDIR = $(shell mkdir -p $(1) 2>/dev/null)
endif

# If the target wasn't specified assume the target is the build machine
ifeq ($(TARGET_ARCH),)
ifeq ($(OS),Windows_NT)
TARGET_ARCH = Windows
endif
endif

ifeq ($(uname_S),Linux)
CFLAGS += -g -O1 -Wall -fPIC 
TARGET_ARCH := Linux
endif

DEPFLAGS = -MT $@ -MMD -MP -MF $(OUTDIR)/$*.d
ARFLAGS = rcs

# Wildcard all the sources and headers
SOURCES := $(call FIND,$(CRYPTOAUTHDIR)/lib,*.c)
INCLUDE := $(sort $(dir $(call FIND, $(CRYPTOAUTHDIR)/lib, *.h)))

# Gather libcryptoauth objects
LIBCRYPTOAUTH_OBJECTS := $(filter-out $(abspath $(CRYPTOAUTHDIR)/lib/hal)/%, $(SOURCES))
LIBCRYPTOAUTH_OBJECTS := $(filter-out $(abspath $(CRYPTOAUTHDIR)/lib/pkcs11)/%, $(LIBCRYPTOAUTH_OBJECTS))
LIBCRYPTOAUTH_OBJECTS := $(filter-out $(abspath $(CRYPTOAUTHDIR)/lib/openssl)/%, $(LIBCRYPTOAUTH_OBJECTS))
LIBCRYPTOAUTH_OBJECTS += atca_hal.c

ifeq ($(TARGET_ARCH),Windows)
# Only kit protocol hals are available on windows
HAL_PREFIX := hal_win
LIBCRYPTOAUTH_OBJECTS += hal_win_timer.c hal_win_os.c
endif

ifeq ($(TARGET_ARCH),Linux)
# General Linux Support
HAL_PREFIX := hal_linux
LIBCRYPTOAUTH_OBJECTS += hal_linux_timer.c hal_linux_os.c
endif

ifeq ($(TARGET_HAL),I2C)
# Native I2C hardware/driver
OPTIONS += ATCA_HAL_I2C
LIBCRYPTOAUTH_OBJECTS += $(addprefix $(HAL_PREFIX),_i2c_userspace.c)
endif

ifeq ($(TARGET_HAL),HID)
OPTIONS += ATCA_HAL_KIT_HID
LIBCRYPTOAUTH_OBJECTS += $(addprefix $(HAL_PREFIX),_kit_hid.c) kit_protocol.c
LIBCRYPTOAUTH_LDFLAGS += -ludev
endif

ifeq ($(TARGET_HAL),CDC)
OPTIONS += ATCA_HAL_KIT_CDC
LIBCRYPTOAUTH_OBJECTS += $(addprefix $(HAL_PREFIX),_kit_cdc.c) kit_protocol.c
LIBCRYPTOAUTH_LDFLAGS += -ludev
endif

TEST_SOURCES := $(call FIND,$(CRYPTOAUTHDIR)/test,*.c)
TEST_SOURCES := $(filter-out $(abspath $(CRYPTOAUTHDIR)/test/openssl)/%, $(TEST_SOURCES)) 
#TEST_INCLUDE := $(sort $(dir $(call FIND, test, *.h)))
TEST_INCLUDE := $(abspath .) $(abspath $(CRYPTOAUTHDIR))
TEST_OBJECTS := $(addprefix $(OUTDIR)/,$(notdir $(TEST_SOURCES:.c=.o)))

LIBCRYPTOAUTH_OBJECTS := $(addprefix $(OUTDIR)/,$(notdir $(LIBCRYPTOAUTH_OBJECTS:.c=.o)))

CFLAGS += $(addprefix -I, $(INCLUDE) $(TEST_INCLUDE)) $(addprefix -D,$(OPTIONS))

# Regardless of platform set the vpath correctly
vpath %.c $(call BACK2SLASH,$(sort $(dir $(SOURCES) $(TEST_SOURCES))))

$(OUTDIR):
	$(call MKDIR, $(OUTDIR))

$(OUTDIR)/%.o : %.c $(OUTDIR)/%.d | $(OUTDIR)
	$(CC) $(DEPFLAGS) $(CFLAGS) -c $< -o $@

$(OUTDIR)/%.d: ;
.PRECIOUS: $(OUTDIR)/%.d

$(OUTDIR)/libcryptoauth.so: $(LIBCRYPTOAUTH_OBJECTS) | $(OUTDIR)
	$(LD) -dll -shared  $(LIBCRYPTOAUTH_OBJECTS) -o $@ $(LIBCRYPTOAUTH_LDFLAGS) -lrt 


$(OUTDIR)/test: $(TEST_OBJECTS) $(LIBCRYPTOAUTH_OBJECTS) | $(OUTDIR)
	$(CC) -o $@ $(TEST_OBJECTS) $(LIBCRYPTOAUTH_OBJECTS) -L$(OUTDIR) -lpthread -lrt -ludev
	
include $(wildcard $(patsubst %,$(OUTDIR)/%.d,$(basename $(SOURCES))))

libcryptoauth: $(OUTDIR)/libcryptoauth.so | $(OUTDIR)

all: libcryptoauth | $(OUTDIR)

test: $(OUTDIR)/test | $(OUTDIR)
	env LD_LIBRARY_PATH=$(OUTDIR) $(OUTDIR)/test

clean:
	rm -r $(OUTDIR)
