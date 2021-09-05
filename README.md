# Super ESCalar simulator REDUX

Superscalar processor simulator for in-order and out-of-order processors. It
also simulates several configurations of multiprocessors. Currently supports
the MIPS instructions set.

This version has been updated to support modern compilers, libraries, and
operating systems. It is derived from the version published by Milos Prvulovic
for the Georgia Tech CS 6290 class. [The upstream
version](https://github.com/flaviut/sesc-mirror) appears to have some
additional functionallity, but it has also been last updated a long time ago
(2009).

## Building

You need a cross-compiler to build the binaries that are run, and you need to
actually build the simulator.

### Compile SESC

You will need a c++ compiler, make, Bison, and Flex installed.

```
$ make -j$(nproc)
```

The executable can be found in `./build/sesc`.

### Cross Compiler

Get a cross-compiler by executing the following:

```console
$ cd cross-compiler
$ bash ./cross-compiler.sh
```

This uses [Crosstool-NG](https://github.com/crosstool-ng/crosstool-ng) to build
a new `mips-unknown-linux-uclibc` toolchain.

This toolchain is by default installed in `~/x-tools`, and you must run
something like `export PATH=$PATH:$HOME/x-tools/mips-unknown-linux-uclibc/bin/`
before building any of the MIPS binaries.
