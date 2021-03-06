#! /bin/bash

mkdir build
cd build
cmake ..
make

rm -rf /tmp/disk
touch /tmp/disk
fallocate --length=16g /tmp/disk

mkdir test
fusermount -u test
./NaiveFS test
