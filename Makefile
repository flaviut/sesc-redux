TARGET_EXEC ?= sesc

BUILD_DIR ?= ./build
SRC_DIRS ?= ./src

BISON ?= bison
FLEX ?= flex

BOOKSIM_SRCS := src/libbooksim/allocators/allocator.cpp src/libbooksim/allocators/islip.cpp src/libbooksim/allocators/loa.cpp src/libbooksim/allocators/maxsize.cpp src/libbooksim/allocators/pim.cpp src/libbooksim/allocators/selalloc.cpp src/libbooksim/allocators/separable.cpp src/libbooksim/allocators/separable_input_first.cpp src/libbooksim/allocators/separable_output_first.cpp src/libbooksim/allocators/wavefront.cpp src/libbooksim/arbiters/arbiter.cpp src/libbooksim/arbiters/matrix_arb.cpp src/libbooksim/arbiters/prio_arb.cpp src/libbooksim/arbiters/roundrobin_arb.cpp src/libbooksim/arbiters/tree_arb.cpp src/libbooksim/booksim_config.cpp src/libbooksim/buffer.cpp src/libbooksim/buffer_state.cpp src/libbooksim/config_utils.cpp src/libbooksim/credit.cpp src/libbooksim/flit.cpp src/libbooksim/flitchannel.cpp src/libbooksim/injection.cpp src/libbooksim/misc_utils.cpp src/libbooksim/module.cpp src/libbooksim/networks/anynet.cpp src/libbooksim/networks/cmesh.cpp src/libbooksim/networks/dragonfly.cpp src/libbooksim/networks/fattree.cpp src/libbooksim/networks/flatfly_onchip.cpp src/libbooksim/networks/fly.cpp src/libbooksim/networks/kncube.cpp src/libbooksim/networks/network.cpp src/libbooksim/networks/qtree.cpp src/libbooksim/networks/tree4.cpp src/libbooksim/outputset.cpp src/libbooksim/packet_reply_info.cpp src/libbooksim/power/buffer_monitor.cpp src/libbooksim/power/power_module.cpp src/libbooksim/power/switch_monitor.cpp src/libbooksim/rng_double_wrapper.cpp src/libbooksim/rng_wrapper.cpp src/libbooksim/routefunc.cpp src/libbooksim/routers/chaos_router.cpp src/libbooksim/routers/event_router.cpp src/libbooksim/routers/iq_router.cpp src/libbooksim/routers/router.cpp src/libbooksim/stats.cpp src/libbooksim/traffic.cpp src/libbooksim/trafficmanager.cpp src/libbooksim/vc.cpp
CMP_SRCS := src/libcmp/DMESIProtocol.cpp src/libcmp/SMPCache.cpp src/libcmp/SMPMemCtrl.cpp src/libcmp/SMPMemRequest.cpp src/libcmp/SMPNOC.cpp src/libcmp/SMPProtocol.cpp src/libcmp/SMPRouter.cpp src/libcmp/SMPSliceCache.cpp src/libcmp/SMPSystemBus.cpp src/libcmp/SMemorySystem.cpp src/libcmp/cmp.cpp
CORE_SRCS := src/libcore/BPred.cpp src/libcore/Cluster.cpp src/libcore/DInst.cpp src/libcore/DepWindow.cpp src/libcore/FetchEngine.cpp src/libcore/GMemoryOS.cpp src/libcore/GMemorySystem.cpp src/libcore/GProcessor.cpp src/libcore/LDSTBuffer.cpp src/libcore/LDSTQ.cpp src/libcore/MemObj.cpp src/libcore/MemRequest.cpp src/libcore/OSSim.cpp src/libcore/Pipeline.cpp src/libcore/ProcessId.cpp src/libcore/Processor.cpp src/libcore/Resource.cpp src/libcore/RunningProcs.cpp src/libcore/SMTProcessor.cpp
EMUL_SRCS := src/libemul/AddressSpace.cpp src/libemul/ElfObject.cpp src/libemul/EmulInit.cpp src/libemul/FileSys.cpp src/libemul/InstDesc.cpp src/libemul/LinuxSys.cpp src/libemul/SignalHandling.cpp
LL_SRCS := src/libll/ExecutionFlow.cpp src/libll/GFlow.cpp src/libll/Instruction.cpp src/libll/ThreadContext.cpp
MEM_SRCS := src/libmem/AddressPrefetcher.cpp src/libmem/Bank.cpp src/libmem/Bus.cpp src/libmem/Cache.cpp src/libmem/MemCtrl.cpp src/libmem/MemoryOS.cpp src/libmem/MemorySystem.cpp src/libmem/PriorityBus.cpp src/libmem/StridePrefetcher.cpp src/libmem/TLB.cpp
SUC_SRCS := src/libsuc/BloomFilter.cpp src/libsuc/CacheCore.cpp src/libsuc/Config.cpp src/libsuc/GStats.cpp src/libsuc/Port.cpp src/libsuc/ReportGen.cpp src/libsuc/SCTable.cpp src/libsuc/SescConf.cpp src/libsuc/Snippets.cpp src/libsuc/TQueue.cpp src/libsuc/callback.cpp src/libsuc/nanassert.cpp
SRCS := $(BOOKSIM_SRCS) $(CMP_SRCS) $(CORE_SRCS) $(EMUL_SRCS) $(LL_SRCS) $(MEM_SRCS) $(SUC_SRCS)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
OBJS += $(BUILD_DIR)/src/libsuc/conflexlex.o $(BUILD_DIR)/src/libbooksim/configlex.o
OBJS += $(BUILD_DIR)/src/libsuc/conflex.tab.o $(BUILD_DIR)/src/libbooksim/config.tab.o
DEPS := $(OBJS:.o=.d)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_DIRS += $(INC_DIRS:%=$(BUILD_DIR)/%)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

D_FLAGS := -DLINUX -DPOSIX_MEMALIGN -DMIPS_EMUL -DSESC_SMP -DSESC_CMP -DCHECK_STALL

CPPFLAGS ?= $(D_FLAGS) $(INC_FLAGS) -MMD -MP
CXXFLAGS ?= -g -std=c++11 -Wmissing-field-initializers -Woverloaded-virtual -Wmissing-declarations -Wdelete-non-virtual-dtor -Wall -Wno-unused -Wno-sign-compare -O3 -fno-strict-aliasing -ffast-math
LDFLAGS ?= $(CXXFLAGS) -lm -lstdc++

.PHONY: all
all: $(BUILD_DIR)/$(TARGET_EXEC)

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# flex
$(BUILD_DIR)/%lex.cpp: %.l $(BUILD_DIR)/%.tab.hpp
	$(MKDIR_P) $(dir $@)
	$(FLEX) -o $@ -c $<

# bison
$(BUILD_DIR)/src/libbooksim/config.tab.hpp $(BUILD_DIR)/src/libbooksim/config.tab.cpp: src/libbooksim/config.y
	$(MKDIR_P) $(dir $@)
	$(BISON) -o $@ -d $<
$(BUILD_DIR)/src/libsuc/conflex.tab.hpp $(BUILD_DIR)/src/libsuc/conflex.tab.cpp: src/libsuc/conflex.y
	$(MKDIR_P) $(dir $@)
	$(BISON) -p yyConf -o $@ -d $<

# c++ source
$(BUILD_DIR)/%.cpp.o: %.cpp
	$(MKDIR_P) $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

.PHONY: clean

clean:
	$(RM) -r $(BUILD_DIR)

-include $(DEPS)

MKDIR_P ?= @mkdir -p
