# Configuration for project.

# Main C file, does not have an associated include file
C_MAIN_FILE = zc.c

# More C files, each has an associated include file
C_MORE_FILES = \
	getopt.c \
	zc_zmq.c \

# CFLAGS += -Wall -O
CFLAGS += -Wall -g


#####
# Everything from here is generic!!! DO NOT EDITH ANYTHING BELOW!
#####

# Compiler and linker configuration:

ZMQ_DIR = $(HOME)/src/libzmq
ZMQ_DLL = libzmq_d

CPPFLAGS += -I$(ZMQ_DIR)/include

LDLIBS += $(ZMQ_DIR)/lib/Win32/$(ZMQ_DLL).lib


# Expansions into useful variables:

O_MAIN_FILE = $(C_MAIN_FILE:.c=.o)
EXE_MAIN_FILE = $(C_MAIN_FILE:.c=)

H_MORE_FILES = $(C_MORE_FILES:.c=.h)
O_MORE_FILES = $(C_MORE_FILES:.c=.o)


# Targets:

.PHONY: all clean

all: $(EXE_MAIN_FILE)

$(O_MAIN_FILE): $(C_MAIN_FILE) $(H_MORE_FILES)

$(EXE_MAIN_FILE): $(O_MAIN_FILE) $(O_MORE_FILES)

define generateC
$(1).o: $(1).c $(1).h
endef

$(foreach c_file, $(C_MORE_FILES:.c=), $(eval $(call generateC, $(c_file))))

clean:
	$(RM) $(O_MAIN_FILE) $(O_MORE_FILES) *~ *.stackdump
	$(RM) $(EXE_MAIN_FILE)
