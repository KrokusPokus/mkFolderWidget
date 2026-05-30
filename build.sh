#!/bin/bash

if [ ! -d "build" ]; then
    mkdir build
fi

cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --parallel

if [ -f "./bin/mkFolderWidget" ]; then
    strip ./bin/mkFolderWidget

    if [ ! -d "$HOME/.local/bin" ]; then
        mkdir "$HOME/.local/bin"
    fi

    cp ./bin/mkFolderWidget $HOME/.local/bin/
fi
