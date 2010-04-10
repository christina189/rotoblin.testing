# (C)2004-2008 SourceMod Development Team
# Makefile written by David "BAILOPAN" Anderson
# Edited for SDK Hooks by Erik "Tsunami" Minekus

SM12SDK = /home/nsh/sm12sdk
SM13SDK = /home/nsh/sm13sdk
HL2SDK_ORIG = /home/nsh/hl2sdk
HL2SDK_OB = /home/nsh/hl2sdk-ob
HL2SDK_L4D = /home/nsh/hl2sdk-l4d
HL2SDK_L4D2 = /home/nsh/hl2sdk-l4d2
MMSOURCE17 = /home/nsh/mmsource17
MMSOURCE18 = /home/nsh/mmsource18

#####################################
### EDIT BELOW FOR OTHER PROJECTS ###
#####################################

PROJECT = sdkhooks

#Uncomment for Metamod: Source enabled extension
USEMETA = true

OBJECTS = sdk/smsdk_ext.cpp extension.cpp natives.cpp

##############################################
### CONFIGURE ANY OTHER FLAGS/OPTIONS HERE ###
##############################################

C_OPT_FLAGS = -DNDEBUG -O3 -funroll-loops -pipe -fno-strict-aliasing -fno-stack-protector
C_DEBUG_FLAGS = -D_DEBUG -DDEBUG -g -ggdb3
C_GCC4_FLAGS = -fvisibility=hidden
CPP_GCC4_FLAGS = -fvisibility-inlines-hidden
CPP = gcc

override ENGSET = false
override SMSET = false
override MMSSET = false

ifeq "$(MMSVER)" "18"
	MMSDIR = $(MMSOURCE18)
	override MMSSET = true
endif
ifeq "$(MMSVER)" "17"
	MMSDIR = $(MMSOURCE17)
	override MMSSET = true
endif

ifeq "$(SMVER)" "12"
	SMDIR = $(SM12SDK)
	override SMSET = true
endif
ifeq "$(SMVER)" "13"
	SMDIR = $(SM13SDK)
	override SMSET = true
endif

ifeq "$(ENGINE)" "original"
	HL2SDK = $(HL2SDK_ORIG)
	HL2PUB = $(HL2SDK)/public
	HL2LIB = $(HL2SDK)/linux_sdk
	CFLAGS += -DSOURCE_ENGINE=1
	METAMOD = $(MMSDIR)/core-legacy
	INCLUDE += -I$(HL2SDK)/public/dlls -I$(HL2SDK)/dlls -I$(HL2SDK)/game_shared
	override ENGSET = true
endif
ifeq "$(ENGINE)" "orangebox"
	HL2SDK = $(HL2SDK_OB)
	HL2PUB = $(HL2SDK)/public
	HL2LIB = $(HL2SDK)/lib/linux
	CFLAGS += -DSOURCE_ENGINE=3
	METAMOD = $(MMSDIR)/core
	INCLUDE += -I$(HL2SDK)/public/game/server -I$(HL2SDK)/game/server -I$(HL2SDK)/game/shared
	override ENGSET = true
endif
ifeq "$(ENGINE)" "left4dead"
	HL2SDK = $(HL2SDK_L4D)
	HL2PUB = $(HL2SDK)/public
	HL2LIB = $(HL2SDK)/lib/linux
	CFLAGS += -DSOURCE_ENGINE=4
	METAMOD = $(MMSDIR)/core
	INCLUDE += -I$(HL2SDK)/public/game/server -I$(HL2SDK)/game/server -I$(HL2SDK)/game/shared
	override ENGSET = true
endif
ifeq "$(ENGINE)" "left4dead2"
	HL2SDK = $(HL2SDK_L4D2)
	HL2PUB = $(HL2SDK)/public
	HL2LIB = $(HL2SDK)/lib/linux
	CFLAGS += -DSOURCE_ENGINE=6
	METAMOD = $(MMSDIR)/core
	INCLUDE += -I$(HL2SDK)/public/game/server -I$(HL2SDK)/game/server -I$(HL2SDK)/game/shared
	GAMEFIX = 2.14d2
	override ENGSET = true
endif

ifeq "$(ENGINE)" "left4dead2"
	LINK_HL2 = $(HL2LIB)/tier1_i486.a $(HL2LIB)/mathlib_i486.a vstdlib_linux.so tier0_linux.so
else
	LINK_HL2 = $(HL2LIB)/tier1_i486.a $(HL2LIB)/mathlib_i486.a vstdlib_i486.so tier0_i486.so
endif

ifeq "$(USEMETA)" "true"
	LINK += $(LINK_HL2)

	INCLUDE += -I. -I.. -Isdk -I$(HL2PUB) -I$(HL2PUB)/engine -I$(HL2PUB)/tier0 -I$(HL2PUB)/tier1 \
		-I$(METAMOD) -I$(METAMOD)/sourcehook -I$(SMDIR)/public -I$(SMDIR)/public/sourcepawn -I$(SMDIR)/public/extensions
	CFLAGS += -DSE_EPISODEONE=1 -DSE_DARKMESSIAH=2 -DSE_ORANGEBOX=3 -DSE_LEFT4DEAD=4 -DSE_LEFT4DEAD2=6
else
	INCLUDE += -I. -I.. -Isdk -I$(SMDIR)/public -I$(SMDIR)/public/sourcepawn -I$(SMDIR)/public/extensions
endif

LINK += -m32 -lm -ldl -Wl,--hash-style=sysv

CFLAGS += -D_LINUX -Dstricmp=strcasecmp -D_stricmp=strcasecmp -D_strnicmp=strncasecmp -Dstrnicmp=strncasecmp \
	-D_snprintf=snprintf -D_vsnprintf=vsnprintf -D_alloca=alloca -Dstrcmpi=strcasecmp -Wall -Werror \
	-Wno-invalid-offsetof -Wno-switch -Wno-unused -mfpmath=sse -msse -DSOURCEMOD_BUILD -DHAVE_STDINT_H -m32
CPPFLAGS += -Wno-non-virtual-dtor -fno-exceptions -fno-rtti

################################################
### DO NOT EDIT BELOW HERE FOR MOST PROJECTS ###
################################################

ifeq "$(DEBUG)" "true"
	BIN_DIR = Debug
	CFLAGS += $(C_DEBUG_FLAGS)
else
	BIN_DIR = Release
	CFLAGS += $(C_OPT_FLAGS)
endif

ifeq "$(USEMETA)" "true"
	BIN_DIR := $(BIN_DIR).$(ENGINE)-SM$(SMVER)
endif

OS := $(shell uname -s)
ifeq "$(OS)" "Darwin"
	LINK += -dynamiclib
	BINARY = $(PROJECT).ext.dylib
else
	LINK += -static-libgcc -shared
	BINARY = $(PROJECT).ext.so
endif

GCC_VERSION := $(shell $(CPP) -dumpversion >&1 | cut -b1)
ifeq "$(GCC_VERSION)" "4"
	CFLAGS += $(C_GCC4_FLAGS)
	CPPFLAGS += $(CPP_GCC4_FLAGS)
endif

OBJ_LINUX := $(OBJECTS:%.cpp=$(BIN_DIR)/%.o)

$(BIN_DIR)/%.o: %.cpp
	$(CPP) $(INCLUDE) $(CFLAGS) $(CPPFLAGS) -o $@ -c $<

all: check
	mkdir -p $(BIN_DIR)/sdk

ifeq "$(ENGINE)" "left4dead2"
	ln -sf $(HL2SDK)/lib/linux/vstdlib_linux.so vstdlib_linux.so;
	ln -sf $(HL2SDK)/lib/linux/tier0_linux.so tier0_linux.so;
else ifeq "$(ENGINE)" "original"
	ln -sf $(HL2SDK)/linux_sdk/vstdlib_i486.so vstdlib_i486.so;
	ln -sf $(HL2SDK)/linux_sdk/tier0_i486.so tier0_i486.so;
else
	ln -sf $(HL2SDK)/lib/linux/vstdlib_i486.so vstdlib_i486.so;
	ln -sf $(HL2SDK)/lib/linux/tier0_i486.so tier0_i486.so;
endif
	$(MAKE) -f Makefile extension

check:
	if [ "$(USEMETA)" = "true" ] && [ "$(ENGSET)" = "false" ]; then \
		echo "You must supply ENGINE=left4dead or ENGINE=orangebox or ENGINE=original"; \
		exit 1; \
	fi
	if [ "$(MMSSET)" = "false" ]; then \
		echo "You must supply MMSVER=17 or MMSVER=18"; \
		exit 1; \
	fi
	if [ "$(SMSET)" = "false" ]; then \
		echo "You must supply SMVER=12 or SMVER=13"; \
		exit 1; \
	fi
	

extension: check $(OBJ_LINUX)
	$(CPP) $(INCLUDE) $(OBJ_LINUX) $(LINK) -o $(BIN_DIR)/$(BINARY)

debug:
	$(MAKE) -f Makefile all DEBUG=true

default: all

clean: check
	rm -rf $(BIN_DIR)/*.o
	rm -rf $(BIN_DIR)/sdk/*.o
	rm -rf $(BIN_DIR)/$(BINARY)
