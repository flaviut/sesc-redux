TARGET=raytrace

LFLAGS=-lm

include ../../scripts/Makefile.Benchmark.mk

ref.strinp  = car

test.strinp = teapot

all.setup   = rm -f raytrace.out raytrace.err
all.simopts = -oraytrace.out -eraytrace.err
all.params  = -p[numcpu] -m128 Input/[strinp].env
all.appname = raytrace[strsuffix]
all.test    = grep "time without init" raytrace.out > /dev/null
all.cleanup = rm -f raytrace.out raytrace.err

include ../../scripts/RunHelper.mk
