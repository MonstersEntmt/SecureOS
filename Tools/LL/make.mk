TOOLS_LL_CPP_SRCS := $(shell find Tools/LL/src/ -name '*.cpp')
TOOLS_LL_CPP_OBJS := $(TOOLS_LL_CPP_SRCS:%=Bin-Int/$(CONFIG)/%.o)

TOOLS_LL_CXXFLAGS   := -std=c++23 -O3 -ITools/LL/src
TOOLS_LL_LDCXXFLAGS := -fuse-ld=lld

ifeq ($(CONFIG), debug)
TOOLS_LL_CXXFLAGS += -O0 -g -DBUILD_CONFIG=BUILD_CONFIG_DEBUG
else ifeq ($(CONFIG), release)
TOOLS_LL_CXXFLAGS += -O3 -g -DBUILD_CONFIG=BUILD_CONFIG_DEBUG
else ifeq ($(CONFIG), dist)
TOOLS_LL_CXXFLAGS += -O3 -DBUILD_CONFIG=BUILD_CONFIG_DEBUG
endif

$(TOOLS_LL_CPP_OBJS): Bin-Int/$(CONFIG)/%.o: %
	mkdir -p $(dir $@)
	$(CXX) $(TOOLS_LL_CXXFLAGS) -c -o $@ $<
	echo Compiled $<

Bin/$(CONFIG)/Tools/LL: $(TOOLS_LL_CPP_OBJS)
	mkdir -p $(dir $@)
	$(CXX) $(TOOLS_LL_LDCXXFLAGS) -o $@ $(TOOLS_LL_CPP_OBJS)
	echo Linked Tools/LL

Tools/LL: Bin/$(CONFIG)/Tools/LL