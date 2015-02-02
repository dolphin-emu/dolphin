#!/bin/sh
set -xe
rm -rf _enet
git clone https://github.com/lsalzman/enet.git _enet
if [ -a git-revision ]; then
    cd _enet
    git checkout $(cat ../git-revision)
    cp ../*.h include/
    cp ../*.c .
    git stash
    git checkout MASTER
    git stash pop
    cd ..
fi
cd _enet; git rev-parse HEAD > ../git-revision; cd ..
git rm -rf --ignore-unmatch *.[ch]
mv _enet/*.c _enet/include/enet/*.h _enet/LICENSE .
git add *.[ch] LICENSE git-revision
rm -rf _enet
echo 'Make sure to update CMakeLists.txt.'
