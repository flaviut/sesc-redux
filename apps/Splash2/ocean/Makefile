TARGET=ocean

LFLAGS=-lm

include ../../scripts/Makefile.Benchmark.mk

test.params = -n 10 -p [numcpu]

ref.params  = -n 258 -p [numcpu]

all.setup   = rm -f ocean.out ocean.err
all.simopts = -oocean.out -eocean.err
all.appname = ocean[strsuffix]
all.test    = grep "time without init" ocean.out > /dev/null
all.cleanup = rm -f ocean.out ocean.err

include ../../scripts/RunHelper.mk
