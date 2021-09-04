TARGET=fmm

LFLAGS=-lm

include ../../scripts/Makefile.Benchmark.mk

ref.strnam  = ref
ref.simopts = -iInput/input <>

test.strnam = test
test.simopts= -iInput/input.256 <>

all.setup   = rm -f fmm.out fmm.err
all.simopts = -ofmm.out -efmm.err
all.params  = -p [numcpu]
all.appname = fmm[strsuffix]
all.test    = grep "Total time for steps" fmm.out > /dev/null
all.cleanup = rm -f times fmm.out fmm.err

include ../../scripts/RunHelper.mk
