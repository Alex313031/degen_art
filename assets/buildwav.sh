#!/bin/bash

export HERE=${PWD} &&
export TARGET=../src/res &&

ffmpeg -y -i ${HERE}/watersky.xm -filter:a "volume=0.25" -c:a adpcm_ms -ar 48000 -ac 2 ${TARGET}/watersky.wav

exit 0
