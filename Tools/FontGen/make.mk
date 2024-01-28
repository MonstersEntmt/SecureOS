TOOLS_FONTGEN_CPP_SRCS := $(shell find Tools/FontGen/src/ -name '*.cpp')
TOOLS_FONTGEN_CPP_OBJS := $(TOOLS_FONTGEN_CPP_SRCS:%=Bin-Int/$(CONFIG)/%.o)

TOOLS_FONTGEN_CXXFLAGS   := -std=c++23 -O3 -ITools/FontGen/src
TOOLS_FONTGEN_LDCXXFLAGS := -fuse-ld=lld

ifeq ($(CONFIG), debug)
TOOLS_FONTGEN_CXXFLAGS += -O0 -g -DBUILD_CONFIG=BUILD_CONFIG_DEBUG
else ifeq ($(CONFIG), release)
TOOLS_FONTGEN_CXXFLAGS += -O3 -g -DBUILD_CONFIG=BUILD_CONFIG_DEBUG
else ifeq ($(CONFIG), dist)
TOOLS_FONTGEN_CXXFLAGS += -O3 -DBUILD_CONFIG=BUILD_CONFIG_DEBUG
endif

$(TOOLS_FONTGEN_CPP_OBJS): Bin-Int/$(CONFIG)/%.o: %
	mkdir -p $(dir $@)
	$(CXX) $(TOOLS_FONTGEN_CXXFLAGS) -c -o $@ $<
	echo Compiled $<

Bin/$(CONFIG)/Tools/FontGen: $(TOOLS_FONTGEN_CPP_OBJS)
	mkdir -p $(dir $@)
	$(CXX) $(TOOLS_FONTGEN_LDCXXFLAGS) -o $@ $(TOOLS_FONTGEN_CPP_OBJS)
	echo Linked Tools/FontGen

Tools/FontGen: Bin/$(CONFIG)/Tools/FontGen