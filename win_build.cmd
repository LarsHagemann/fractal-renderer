@echo off
git submodule update --init
cd SFML
git checkout 2.6.x
cd ..
cmake -B build/ -S .
cmake --build build --target install --config Release