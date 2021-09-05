TUPLE ?= mips-unknown-linux-gnu-
CC := $(TUPLE)cc
CXX := $(TUPLE)cxx

TARGET_EXEC ?= lu.mipseb

BUILD_DIR ?= ./build
SRC_DIRS ?= ./Source

SRCS := $(shell find $(SRC_DIRS) -name *.C)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CPPFLAGS ?= $(INC_FLAGS) -MMD -MP
CFLAGS ?= -O3 -g -static -fno-delayed-branch -fno-optimize-sibling-calls -msplit-addresses -mabi=32 -march=mips4
LDFLAGS ?= -lpthread -lm

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# c source
$(BUILD_DIR)/%.C.o: %.C
	$(MKDIR_P) $(dir $@)
	m4 ../../scripts/pthread.m4.stougie Source/lu.C > $(BUILD_DIR)/$<.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $(BUILD_DIR)/$<.c -o $@


.PHONY: clean

clean:
	$(RM) -r $(BUILD_DIR)

-include $(DEPS)

MKDIR_P ?= @mkdir -p

