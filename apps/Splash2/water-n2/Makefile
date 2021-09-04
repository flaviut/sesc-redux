TypeOfTARGET=Benchmark
TARGET=water-n2

LFLAGS=-lm

include ../../scripts/Makefile.Benchmark.mk

ref.strnam    = ref
ref.simopts   = -iInput/input <>
ref.params    = -p [numcpu] -n 512

test.strnam    = test
test.simopts   = -iInput/input <>
tes.params    = -p [numcpu] -n 64

large.strnam  = large
large.simopts = -iInput/input <>
large.params  = -p [numcpu] -n 4096

all.setup   = rm -f water-n2.out water-n2.err; cp Input/random.in .; chmod u+w random.in
all.simopts = -owater-n2.out -ewater-n2.err
all.appname = water-n2[strsuffix]
all.test    = grep "Exited Happily" water-n2.out > /dev/null
all.cleanup = rm -f random.in water-n2.out water-n2.err

include ../../scripts/RunHelper.mk
