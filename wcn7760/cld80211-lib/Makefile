# ---------------------------------------------------------------------------------
# Makefile to compile CLD-80211 library
# By default no compiler flags are added to generate debug information. Can be added if required.
# $Header: $
# $DateTime: $
# ---------------------------------------------------------------------------------
TOP = ../..
OUT_INT = $(TOP)/gps-internal/location-x86/prebuilts/

PREBUILT_DIRECTORIES = $(TOP)/gps-internal/location-x86/prebuilts/

CC = gcc
CXX = g++

COMMON_CFLAGS = \
     -D__USE_GNU \
     -D__LINUX__ \
     -D__STDC_FORMAT_MACROS \
     -Werror=format-extra-args \
     -Werror=overflow

COMMON_CPPFLAGS = \
     -std=c++14


#=============== STATIC_LIB ================
STATIC_LIB_INCLUDE_DIRECTORIES = \
    -I$(TOP)/h_q_wlan/cld80211-lib

STATIC_LIB_SOURCE_PATH = $(TOP)/h_q_wlan/cld80211-lib

STATIC_LIB_SOURCES = \
        $(STATIC_LIB_SOURCE_PATH)/cld80211_lib.c

INCLUDE_DIRECTORIES = $(STATIC_LIB_INCLUDE_DIRECTORIES)
SOURCE_FILES = $(STATIC_LIB_SOURCES)
CFLAGS = -fPIC -Wall $(COMMON_CFLAGS) $(INCLUDE_DIRECTORIES)
CXXFLAGS = $(CFLAGS) -std=c++14
LDFLAGS = -shared -ldl $(GLIB_LIBS)
TARGET_LIB = libcld80211.so
TARGET_STATIC_LIB = libcld80211.a

LDFLAGS+= -lpthread -lc -lnl-genl-3 -lnl-3

INC=-I$(TOP)/core/libnl-3.2.25/include \
    -I/usr/include/libnl3

STATIC_LIB_HEADERS=$(wildcard *.h)
STATIC_LIB_SRCS=$(wildcard *.c)
STATIC_LIB_OBJS=$(patsubst %.c,%.o,$(STATIC_LIB_SRCS))
%.o: %.c
	$(CC) $(INC) -c -fPIC -o $@ $(CFLAGS) ${COPTS} $<
	@echo "  CC " $<

LIB_OBJS_INT = $(filter %.o_int,$(SOURCE_FILES:.cpp=.o_int) $(SOURCE_FILES:.c=.o_int))
%.o_int: %.cpp
	$(CXX) $(INC) -c -fPIC -o $@ ${CXXFLAGS} -DFEATURE_INTERNAL_BUILD $<
	@echo "  CXX " $<
%.o_int: %.c
	$(CC) $(INC) -c -fPIC -o $@ $(CFLAGS) -DFEATURE_INTERNAL_BUILD $<
	@echo "  CC " $<

.phony: all internal clean

all: internal libcld80211.a
all: internal

libcld80211.so: internal

internal:$(LIB_OBJS_INT)
	$(CC) -s -fPIC -o $(TARGET_LIB) -Wl,-whole-archive $^ -Wl,-no-whole-archive -lc $(LDFLAGS)
	mkdir -p $(OUT_INT) && mv $(TARGET_LIB) $(OUT_INT)/$(TARGET_LIB)

libcld80211.a: $(STATIC_LIB_OBJS)
	$(AR) crT $@ $?

clean:
	rm -f ${TARGET_LIB} $(LIB_OBJS_INT) $(STATIC_LIB_OBJS) ${TARGET_STATIC_LIB}
	rm -f $(OUT_INT)/$(TARGET_LIB)
