all.strbuild  = runsim
#all.simenv    = SESC_procsPerNode="[numcpu]"
all.simname   = ~/sesc/sesc.opt
all.numcpu    = 1
all.numthread = 1
all.strsuffix = .mipseb

noc.base      =
noc.simopts   = -c ~/sesc/confs/cmp[numcpu]-noc.conf <>

ff.base       =
ff.simopts    = -w1000000000000 <>

cut.base      =
cut.simopts   = -w5000000000 -y1000000000 <>
cut.test      = echo Not doing any output tests for cut runs.

t1.base       =
t1.numthread  = 1

p1.base       =
p1.numcpu     = 1

t2. base      =
t2.numthread  = 2

p2.base       =
p2.numcpu     = 2

t4.base       =
t4.numthread  = 4

p4.base       =
p4.numcpu     = 4

t8.base       =
t8.numthread  = 8

p8.base       =
p8.numcpu     = 8

t16.base      =
t16.numthread = 16

p16.base      =
p16.numcpu    = 16

t32.base      =
t32.numthread = 32

p32.base      =
p32.numcpu    = 32

t64.base      =
t64.numthread = 64

p64.base      =
p64.numcpu    = 64

t128.base     =
t128.numthread= 128

p128.base     =
p128.numcpu   = 128

t256.base     =
t256.numthread= 256

p256.base     =
p256.numcpu   = 256

t512.base     =
t512.numthread= 512

p512.base     =
p512.numcpu   = 512

t1024.base     =
t1024.numthread= 1024

p1024.base    =
p1024.numcpu  = 1024

mipseb.base       =
mipseb.strsuffix  = .mipseb
mipseb.strendianc = b
mipseb.strroot    = /local/milos/mipsrt

mipseb64.base       =
mipseb64.strsuffix  = .mipseb64
mipseb64.strendianc = b

mipsel.base       =
mipsel.strsuffix  = .mipsel
mipsel.strendianc = l
mipsel.strroot    = /home/milos/sim/mipselroot

mipsel64.base       =
mipsel64.strsuffix  = .mipsel64
mipsel64.strendianc = l

nopad.base     =
nopad.strsuffix   = <>-nopad

valgrind.base   = 
valgrind.simpfx = <> valgrind --leak-check=full

valgrinddb.base     = 
valgrinddb.simpfx   = valgrind --gen-suppressions=yes --db-attach=yes
valgrinddb.strbuild = dbgsim

gdb.base =
gdb.simpfx = gdb -x ~/bin/include/gdb/gdbscript --args <>
gdb.strbuild  = dbgsim

leave.base =
leave.cleanup = echo Not doing any cleanup for 'leave' runs.
