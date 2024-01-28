TOOLS_LC_CPP_SRCS := $(shell find Tools/LC/src/ -name '*.cpp')
TOOLS_LC_CPP_OBJS := $(TOOLS_LC_CPP_SRCS:%=Bin-Int/$(CONFIG)/%.o)

TOOLS_LC_CXXFLAGS   := -std=c++23 -O3 -ITools/LC/src
TOOLS_LC_LDCXXFLAGS := -fuse-ld=lld

ifeq ($(CONFIG), debug)
TOOLS_LC_CXXFLAGS += -O0 -g -DBUILD_CONFIG=BUILD_CONFIG_DEBUG
else ifeq ($(CONFIG), release)
TOOLS_LC_CXXFLAGS += -O3 -g -DBUILD_CONFIG=BUILD_CONFIG_DEBUG
else ifeq ($(CONFIG), dist)
TOOLS_LC_CXXFLAGS += -O3 -DBUILD_CONFIG=BUILD_CONFIG_DEBUG
endif

$(TOOLS_LC_CPP_OBJS): Bin-Int/$(CONFIG)/%.o: %
	mkdir -p $(dir $@)
	$(CXX) $(TOOLS_LC_CXXFLAGS) -c -o $@ $<
	echo Compiled $<

Bin/$(CONFIG)/Tools/LC: $(TOOLS_LC_CPP_OBJS)
	mkdir -p $(dir $@)
	$(CXX) $(TOOLS_LC_LDCXXFLAGS) -o $@ $(TOOLS_LC_CPP_OBJS)
	echo Linked Tools/LC

Tools/LC: Bin/$(CONFIG)/Tools/LC