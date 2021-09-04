TARGET=barnes

LFLAGS=-lm

include ../../scripts/Makefile.Benchmark.mk

ref.strinp  = input
test.strinp = input.256

all.simopts = -iInput/[strinp] -obarnes.out -ebarnes.err
all.params  = -p [numcpu]
all.appname = barnes[strsuffix]
all.test    = grep RESTTIME barnes.out > /dev/null
all.cleanup = rm -f barnes.out barnes.err

include ../../scripts/RunHelper.mk
