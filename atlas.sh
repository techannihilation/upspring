#!/usr/bin/env bash
set -e;

PROCS=$(nproc)

printf "%s\0" "arm" "core" "gok" "tll" "talon" "rumad" | xargs -0 -I {} -P ${PROCS} /bin/bash -c '
    ./out/build/gcc-debug/src/Upspring -r runscripts/many_to_atlas.lua -- ../TA ../TA/unittextures/$1.yaml ../TA/objects3d/$1/*.3do
' '_' {}

./out/build/gcc-debug/src/Upspring -r runscripts/many_to_atlas.lua -- ../TA ../TA/unittextures/other.yaml ../TA/objects3d/*.3do