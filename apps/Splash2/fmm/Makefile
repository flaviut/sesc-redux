TUPLE ?= mips-unknown-linux-uclibc-
CC := $(TUPLE)cc
CXX := $(TUPLE)cxx

TARGET_EXEC ?= fmm.mipseb

BUILD_DIR ?= ./build
SRC_DIRS ?= ./Source

SRCS := $(shell find $(SRC_DIRS) -name *.C)
SRCS_OUT := $(SRCS:$(SRC_DIRS)/%.C=$(BUILD_DIR)/%.c)
HDRS := $(shell find $(SRC_DIRS) -name *.H)
HDRS_OUT := $(HDRS:$(SRC_DIRS)/%.H=$(BUILD_DIR)/%.h)
OBJS := $(SRCS_OUT:%.c=%.o)
DEPS := $(OBJS:.o=.d)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_DIRS := $(INC_DIRS) $(BUILD_DIR)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CPPFLAGS ?= $(INC_FLAGS) -MMD -MP
CFLAGS ?= -O3 -g -static -fno-delayed-branch -fno-optimize-sibling-calls -msplit-addresses -mabi=32 -march=mips4
LDFLAGS ?= -lpthread -lm

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.c: $(SRC_DIRS)/%.C
	$(MKDIR_P) $(dir $@)
	m4 ../../scripts/pthread.m4.stougie $< > $@
$(BUILD_DIR)/%.h: $(SRC_DIRS)/%.H
	$(MKDIR_P) $(dir $@)
	m4 ../../scripts/pthread.m4.stougie $< > $@

# c source
$(BUILD_DIR)/%.o: $(BUILD_DIR)/%.c $(HDRS_OUT)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@


.PHONY: clean

clean:
	$(RM) -r $(BUILD_DIR)

-include $(DEPS)

MKDIR_P ?= @mkdir -p

