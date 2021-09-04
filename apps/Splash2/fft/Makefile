TARGET=fft

LFLAGS=-lm

include ../../scripts/Makefile.Benchmark.mk

test.strm  = 6
ref.strm   = 16

all.setup   = rm -f fft.out fft.err
all.simopts = -offt.out -efft.err
all.params  = -m [strm] -p[numcpu]
all.appname = fft[strsuffix]
all.test    = grep "Overall transpose fraction" fft.out > /dev/null
all.cleanup = rm -f fft.out fft.err

include ../../scripts/RunHelper.mk
