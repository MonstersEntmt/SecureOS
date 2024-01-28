TOOLS_IMGGEN_CPP_SRCS := $(shell find Tools/ImgGen/src/ -name '*.cpp')
TOOLS_IMGGEN_CPP_OBJS := $(TOOLS_IMGGEN_CPP_SRCS:%=Bin-Int/$(CONFIG)/%.o)

TOOLS_IMGGEN_CXXFLAGS   := -std=c++23 -O3 -ITools/ImgGen/src
TOOLS_IMGGEN_LDCXXFLAGS := -fuse-ld=lld

ifeq ($(CONFIG), debug)
TOOLS_IMGGEN_CXXFLAGS += -O0 -g -DBUILD_CONFIG=BUILD_CONFIG_DEBUG
else ifeq ($(CONFIG), release)
TOOLS_IMGGEN_CXXFLAGS += -O3 -g -DBUILD_CONFIG=BUILD_CONFIG_DEBUG
else ifeq ($(CONFIG), dist)
TOOLS_IMGGEN_CXXFLAGS += -O3 -DBUILD_CONFIG=BUILD_CONFIG_DEBUG
endif

$(TOOLS_IMGGEN_CPP_OBJS): Bin-Int/$(CONFIG)/%.o: %
	mkdir -p $(dir $@)
	$(CXX) $(TOOLS_IMGGEN_CXXFLAGS) -c -o $@ $<
	echo Compiled $<

Bin/$(CONFIG)/Tools/ImgGen: $(TOOLS_IMGGEN_CPP_OBJS)
	mkdir -p $(dir $@)
	$(CXX) $(TOOLS_IMGGEN_LDCXXFLAGS) -o $@ $(TOOLS_IMGGEN_CPP_OBJS)
	echo Linked Tools/ImgGen

Tools/ImgGen: Bin/$(CONFIG)/Tools/ImgGen