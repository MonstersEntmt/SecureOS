include Tools/FontGen/make.mk
include Tools/ImgGen/make.mk
include Tools/LA/make.mk
include Tools/LC/make.mk
include Tools/LL/make.mk

.PHONY: Tools
Tools: Tools/FontGen Tools/ImgGen Tools/LA Tools/LC Tools/LL

FONTGEN := Bin/$(CONFIG)/Tools/FontGen
IMGGEN  := Bin/$(CONFIG)/Tools/ImgGen
LA      := Bin/$(CONFIG)/Tools/LA
LC      := Bin/$(CONFIG)/Tools/LC
LL      := Bin/$(CONFIG)/Tools/LL