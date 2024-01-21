FONTBITMAP_CPP_SRCS := $(shell find FontBitmap/src/ -name '*.cpp')
FONTBITMAP_CPP_OBJS := $(FONTBITMAP_CPP_SRCS:%=Bin-Int/$(CONFIG)/%.o)

FONTBITMAP_CXXFLAGS := -std=c++23 -O3
FONTBITMAP_LDCXXFLAGS := -fuse-ld=lld

$(FONTBITMAP_CPP_OBJS): Bin-Int/$(CONFIG)/%.o: %
	mkdir -p $(dir $@)
	$(CXX) $(FONTBITMAP_CXXFLAGS) -c -o $@ $<
	echo Compiled $<

Bin/$(CONFIG)/FontBitmap: $(FONTBITMAP_CPP_OBJS)
	mkdir -p $(dir $@)
	$(CXX) $(FONTBITMAP_LDCXXFLAGS) -o $@ $(FONTBITMAP_CPP_OBJS)
	echo Linked FontBitmap

FontBitmap: Bin/$(CONFIG)/FontBitmap