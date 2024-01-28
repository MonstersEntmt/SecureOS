TOOLS_LA_CPP_SRCS := $(shell find Tools/LA/src/ -name '*.cpp')
TOOLS_LA_CPP_OBJS := $(TOOLS_LA_CPP_SRCS:%=Bin-Int/$(CONFIG)/%.o)

TOOLS_LA_CXXFLAGS   := -std=c++23 -O3 -ITools/LA/src
TOOLS_LA_LDCXXFLAGS := -fuse-ld=lld

ifeq ($(CONFIG), debug)
TOOLS_LA_CXXFLAGS += -O0 -g -DBUILD_CONFIG=BUILD_CONFIG_DEBUG
else ifeq ($(CONFIG), release)
TOOLS_LA_CXXFLAGS += -O3 -g -DBUILD_CONFIG=BUILD_CONFIG_DEBUG
else ifeq ($(CONFIG), dist)
TOOLS_LA_CXXFLAGS += -O3 -DBUILD_CONFIG=BUILD_CONFIG_DEBUG
endif

$(TOOLS_LA_CPP_OBJS): Bin-Int/$(CONFIG)/%.o: %
	mkdir -p $(dir $@)
	$(CXX) $(TOOLS_LA_CXXFLAGS) -c -o $@ $<
	echo Compiled $<

Bin/$(CONFIG)/Tools/LA: $(TOOLS_LA_CPP_OBJS)
	mkdir -p $(dir $@)
	$(CXX) $(TOOLS_LA_LDCXXFLAGS) -o $@ $(TOOLS_LA_CPP_OBJS)
	echo Linked Tools/LA

Tools/LA: Bin/$(CONFIG)/Tools/LA