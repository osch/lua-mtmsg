.PHONY: default mtmsg
default: mtmsg

BUILD_DATE  := $(shell date "+%Y-%m-%dT%H:%M:%S")

LNX_GCC_RUN := gcc -shared -fPIC -O2 -g
WIN_GCC_RUN := gcc -shared -fPIC -O2
MAC_GCC_RUN := MACOSX_DEPLOYMENT_TARGET=10.8 gcc -O2 -bundle -undefined dynamic_lookup -all_load

LNX_COPTS   :=
WIN_COPTS   := -I/mingw64/include/lua5.1 
MAC_COPTS   := -I/usr/local/opt/lua/include/lua5.3 

LNX_LOPTS   := -lpthread
WIN_LOPTS   := -lkernel32
MAC_LOPTS   := -lpthread

LNX_SO_EXT  := so
WIN_SO_EXT  := dll
MAC_SO_EXT  := so

GCC_RUN     :=
SO_EXT      :=
COPTS       :=
LOPTS       :=

# platforms: LNX, WIN, MAC
# (may be set in sandbox.mk)

PLATFORM    := LNX
LUA_VERSION := 5.4

-include sandbox.mk

GCC_RUN       := $(or $(GCC_RUN),       $($(PLATFORM)_GCC_RUN))
SO_EXT        := $(or $(SO_EXT),        $($(PLATFORM)_SO_EXT))
COPTS         := $(or $(COPTS),         $($(PLATFORM)_COPTS))
LOPTS         := $(or $(LOPTS),         $($(PLATFORM)_LOPTS))

mtmsg:
	@mkdir -p build/lua$(LUA_VERSION)/
	$(GCC_RUN) $(COPTS) \
	    -D MTMSG_VERSION=Makefile"-$(BUILD_DATE)" \
	    main.c         buffer.c       listener.c   writer.c \
	    reader.c       serialize.c    error.c      util.c   \
	    async_util.c   mtmsg_compat.c \
	    receiver_capi_impl.c notify_capi_impl.c \
	    $(LOPTS) \
	    -o build/lua$(LUA_VERSION)/mtmsg.$(SO_EXT)
	    

