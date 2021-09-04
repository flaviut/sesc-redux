TARGET = cholesky

LFLAGS=-lm

include ../../scripts/Makefile.Benchmark.mk

large.strfil  = tk29.O
ref.strfil    = tk15.O
test.strfil   = tk14.O

all.setup   = rm -f cholesky.out cholesky.err; cp Input/[strfil] .; chmod u+w [strfil]
all.simopts = -ocholesky.out -echolesky.err
all.appname = cholesky[strsuffix]
all.params  = [strfil] -p[numcpu]
all.test    = grep "time without init" cholesky.out > /dev/null
all.cleanup = rm -f [strfil] cholesky.out cholesky.err

include ../../scripts/RunHelper.mk
