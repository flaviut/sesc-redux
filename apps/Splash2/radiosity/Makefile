TARGET=radiosity

LFLAGS=-lm

include ../../scripts/Makefile.Benchmark.mk

large.params  = -p [numcpu] -batch -room

ref.params    = -p [numcpu] -batch -room -ae 5000.0 -en 0.050 -bf 0.10

test.params   = -p [numcpu] -batch -test -ae 5000.0 -en 0.050 -bf 0.10

all.setup   = rm -f radiosity.out radiosity.err
all.simopts = -oradiosity.out -eradiosity.err
all.appname = radiosity[strsuffix]
all.test    = grep "Elem(hierarchical)" radiosity.out > /dev/null
all.cleanup = rm -f radiosity.out radiosity.err

include ../../scripts/RunHelper.mk
