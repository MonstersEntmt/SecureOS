IMGGEN_SRCS := $(shell find Boot/ImgGen/ -name '*.cpp')
IMGGEN_OBJS := $(IMGGEN_SRCS:%=Bin-Int/%.o)

IMGGEN_CXXFLAGS := -std=c++23 -O0 -g
IMGGEN_LDCXXFLAGS := -fuse-ld=lld

Bin-Int/Boot/ImgGen/%.cpp.o: Boot/ImgGen/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(IMGGEN_CXXFLAGS) -c -o $@ $<
	echo Built $<

Bin/Boot/ImgGen: $(IMGGEN_OBJS)
	mkdir -p $(dir $@)
	$(CXX) $(IMGGEN_LDCXXFLAGS) -o $@ $(IMGGEN_OBJS)
	echo Linked ImgGen