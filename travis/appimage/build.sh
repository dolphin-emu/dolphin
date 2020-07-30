#!/bin/bash -ex

mkdir -p "$HOME/.ccache"
docker run -e ENABLE_COMPATIBILITY_REPORTING --env-file travis/common/travis-ci.env -v $(pwd):/dolphin -v "$HOME/.ccache":/root/.ccache -u root quriouspixel/yuzu:latest /bin/bash /yuzu/travis/appimage/docker.sh
