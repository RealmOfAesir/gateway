#!/bin/bash
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CPUS="$( grep -c ^processor /proc/cpuinfo )"
cd $DIR/external/common_backend
./compile_external.sh
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo .
make -j$CPUS
cd $DIR/external/uWebSockets
make
cd $DIR
