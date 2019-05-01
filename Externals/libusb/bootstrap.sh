#!/bin/sh

if ! test -d m4 ; then
    mkdir m4
fi
autoreconf -ivf || exit 1
