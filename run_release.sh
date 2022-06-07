#! /bin/bash

mkdir build
cd build
rm CMakeCache.txt
cmake -DCMAKE_BUILD_TYPE=Release -DNO_ASAN=1 ..
make -j 4

rm -rf /tmp/disk
touch /tmp/disk
fallocate --length=1g /tmp/disk

mkdir test
fusermount -u test
./NaiveFS --init test
