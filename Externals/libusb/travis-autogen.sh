#!/bin/bash

# Warnings enabled
CFLAGS="-Wall -Wextra"

CFLAGS+=" -Wbad-function-cast"
#CFLAGS+=" -Wcast-align"
CFLAGS+=" -Wchar-subscripts"
CFLAGS+=" -Wempty-body"
CFLAGS+=" -Wformat"
CFLAGS+=" -Wformat-security"
CFLAGS+=" -Winit-self"
CFLAGS+=" -Winline"
CFLAGS+=" -Wmissing-declarations"
CFLAGS+=" -Wmissing-include-dirs"
CFLAGS+=" -Wmissing-prototypes"
CFLAGS+=" -Wnested-externs"
CFLAGS+=" -Wold-style-definition"
CFLAGS+=" -Wpointer-arith"
CFLAGS+=" -Wredundant-decls"
CFLAGS+=" -Wshadow"
CFLAGS+=" -Wstrict-prototypes"
CFLAGS+=" -Wswitch-enum"
CFLAGS+=" -Wundef"
CFLAGS+=" -Wuninitialized"
CFLAGS+=" -Wunused"
CFLAGS+=" -Wwrite-strings"

# warnings disabled on purpose
CFLAGS+=" -Wno-unused-parameter"
CFLAGS+=" -Wno-unused-function"
CFLAGS+=" -Wno-deprecated-declarations"

# should be removed and the code fixed
CFLAGS+=" -Wno-incompatible-pointer-types-discards-qualifiers"

export CFLAGS

./autogen.sh "$@"
