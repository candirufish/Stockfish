#!/bin/sh

echo
echo "clean"
echo

make clean

echo
echo "build"
echo

make build ARCH=x86-64 optimize=no

echo
echo "bench"
echo
./stockfish bench