TARGET=volrend

CFLAGS.Linux = -DUSE_PROTOTYPES=1 -DUSE_VARARGS=0 -DMMAP_SUPPORT -DJPEG_SUPPORT -DLZW_SUPPORT -DHAVE_IEEEFP
LFLAGS=-lm

#-DCOLORIMETRY_SUPPORT -DYCBCR_SUPPORT -DCMYK_SUPPORT

include ../../scripts/Makefile.Benchmark.mk

ref.strinp  = head
test.strinp = head-scaleddown4

all.setup   = rm -f volrend.out volrend.err [strinp]_*.tiff; cp Input/[strinp].* .; chmod u+w [strinp].*
all.simopts = -ovolrend.out -evolrend.err
all.appname = volrend[strsuffix]
all.params  = [numcpu] [strinp]
all.test    = file [strinp]_1003.tiff | grep -q "TIFF image data"
all.cleanup = rm -f volrend.out volrend.err [strinp].den [strinp].norm [strinp].opc [strinp].pyr [strinp]_????.tiff

include ../../scripts/RunHelper.mk
