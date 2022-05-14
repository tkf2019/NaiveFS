#! /bin/bash

cd build
cmake .. && make

rm -rf /tmp/disk
touch /tmp/disk
fallocate --length=16m /tmp/disk

fusermount -u test
./NaiveFS test

# code test.log