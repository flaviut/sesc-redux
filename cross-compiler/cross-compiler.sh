#!/bin/bash

git clone https://github.com/crosstool-ng/crosstool-ng
cd crosstool-ng || exit 1
git checkout 4de586cd2ee8fe7de53bc0ede6ab25e3c71412cd
./bootstrap
./configure --enable-local
make "-j$(nproc)"

./ct-ng mips-unknown-linux-uclibc
./ct-ng build
