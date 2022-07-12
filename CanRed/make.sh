#!/usr/bin/env bash
# Only run this from the <project_root>/CanRed Folder!

COMPILE_MODE="Debug"

if [[ $1 == "--release" ]]
then
    COMPILE_MODE="Release"
fi

if [[ ! -f "./build/conanbuildinfo.cmake" ]]
then
    ./installdependencies.sh
fi

# From what I can gather, running cmake when nothing has changed, is a 
# harmless no-op, that doesn't take much time. As such, I see no issue
# always running it when compiling, to make development and generally
# compiling it on a fresh enviorment easier.
cmake -S . -B ./build -G"Unix Makefiles" -DCMAKE_BUILD_TYPE="$COMPILE_MODE"
make -j$(nproc) --no-print-directory -C ./build