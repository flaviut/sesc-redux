TypeOfTARGET=Benchmark
TARGET=water-sp

LFLAGS=-lm

include ../../scripts/Makefile.Benchmark.mk

ref.strnam    = ref
ref.simopts   = -iInput/input <>
ref.params    = -p [numcpu] -n 512

test.strnam   = test
test.simopts  = -iInput/input <>
tes.params    = -p [numcpu] -n 64

large.strnam  = large
large.simopts = -iInput/input <>
large.params  = -p [numcpu] -n 4096

all.setup   = rm -f water-sp.out water-sp.err; cp Input/random.in .; chmod u+w random.in
all.simopts = -owater-sp.out -ewater-sp.err
all.appname = water-sp[strsuffix]
all.test    = grep "Exited Happily" water-sp.out > /dev/null
all.cleanup = rm random.in water-sp.out water-sp.err

include ../../scripts/RunHelper.mk
