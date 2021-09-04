TARGET=radix

include ../../scripts/Makefile.Benchmark.mk

ref.params  = -n 262144 -r 1024 -p [numthread]

test.params = -n 1048576 -r 1024 -p [numthread]

all.setup   = rm -f radix.out radix.err
all.simopts = -oradix.out -eradix.err
all.appname = radix[strsuffix]
all.test    = grep "time without init" radix.out > /dev/null
all.cleanup = rm -f radix.out radix.err

include ../../scripts/RunHelper.mk
