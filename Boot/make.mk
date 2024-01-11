IMGGEN_CPP_SRCS := $(shell find Boot/ImgGen/ -name '*.cpp')
IMGGEN_CPP_OBJS := $(IMGGEN_CPP_SRCS:%=Bin-Int/$(CONFIG)/%.o)

IMGGEN_OBJS := $(IMGGEN_CPP_OBJS)

IMGGEN_CXXFLAGS := -std=c++23 -O3
IMGGEN_LDCXXFLAGS := -fuse-ld=lld

$(IMGGEN_CPP_OBJS): Bin-Int/$(CONFIG)/%.o: %
	mkdir -p $(dir $@)
	$(CXX) $(IMGGEN_CXXFLAGS) -c -o $@ $<
	echo Compiled $<

Bin/$(CONFIG)/Boot/ImgGen: $(IMGGEN_OBJS)
	mkdir -p $(dir $@)
	$(CXX) $(IMGGEN_LDCXXFLAGS) -o $@ $(IMGGEN_OBJS)
	echo Linked ImgGen

ImgGen: Bin/$(CONFIG)/Boot/ImgGen