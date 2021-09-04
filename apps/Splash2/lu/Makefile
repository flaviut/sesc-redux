TARGET=lu

LFLAGS=-lm

include ../../scripts/Makefile.Benchmark.mk

ref.params  = -n 512 -p [numthread]

test.params = -n 32 -p [numthread]

all.setup   = rm -f lu.out lu.err
all.simopts = -olu.out -elu.err
all.appname = lu[strsuffix]
all.test    = grep "time without init" lu.out > /dev/null
all.cleanup = rm -f lu.out lu.err

include ../../scripts/RunHelper.mk
