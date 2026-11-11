#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")/.."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
mkdir -p stage/bin stage/data stage/share
cp build/wenuy-studio stage/bin/wenuy-studio
cp build/wenuy-studio-cli stage/bin/wenuy-studio-cli
cp wenuyos-app.toml stage/wenuyos-app.toml
cp data/* stage/data/
cp examples/Demo.wstudio stage/share/
python3 tools/make-wapp.py stage -o WenuyStudio-0.8.0.wapp
