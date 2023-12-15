#!/usr/bin/env bash
PROCS=$(nproc)

for f in "arm" "core" "gok" "tll" "talon" "rumad"; do
    find "../TA/objects3d/${f}" -iname '*.3do' -print0 | xargs -0 -I {} -P ${PROCS} /bin/bash -c '
        if [[ ! -f "${2:0:-4}.s3o" ]]; then 
            ./Upspring -r runscripts/convert_with_atlas.lua -- "../TA/unittextures/${1}.yaml" "$2";
            # blender -b -P ./blender/s3o_optimize.py -- "${2:0:-4}.s3o"
        fi' '_' "${f}" {}
done

find "../TA/objects3d" -maxdepth 1 -iname '*.3do' -print0 | xargs -0 -I {} -P ${PROCS} /bin/bash -c '
    if [[ ! -f "${1:0:-4}.s3o" ]]; then 
        ./Upspring -r runscripts/convert_with_atlas.lua -- ../TA/unittextures/other.yaml "$1";
        # blender -b -P ./blender/s3o_optimize.py -- "${2:0:-4}.s3o"
    fi' '_' {}