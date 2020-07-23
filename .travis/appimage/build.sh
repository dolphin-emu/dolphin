#!/bin/bash -ex

mkdir -p "$HOME/.ccache"
docker run -e ENABLE_COMPATIBILITY_REPORTING --env-file .travis/common/travis-ci.env -v $(pwd):/dolphin-emu -v "$HOME/.ccache":/root/.ccache quriouspixel/yuzu:latest /bin/bash /yuzu/.travis/appimage/docker.sh
