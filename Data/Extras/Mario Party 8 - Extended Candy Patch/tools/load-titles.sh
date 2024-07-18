#!/usr/bin/env bash

    #####################################################################
    ##                 __            __ _ ___________                  ##
    ##                 \ \          / /| |____   ____|                 ##
    ##                  \ \        / / | |    | |                      ##
    ##                   \ \  /\  / /  | |    | |                      ##
    ##                    \ \/  \/ /   | |    | |                      ##
    ##                     \  /\  /    | |    | |                      ##
    ##                      \/  \/     |_|    |_|                      ##
    ##                                                                 ##
    ##                        Wiimms ISO Tools                         ##
    ##                      http://wit.wiimm.de/                       ##
    ##                                                                 ##
    #####################################################################
    ##                                                                 ##
    ##   This file is part of the WIT project.                         ##
    ##   Visit http://wit.wiimm.de/ for project details and sources.   ##
    ##                                                                 ##
    ##   Copyright (c) 2009-2017 by Dirk Clemens <wiimm@wiimm.de>      ##
    ##                                                                 ##
    #####################################################################
    ##                                                                 ##
    ##   This file loads the title files from WiiTDB.com               ##
    ##                                                                 ##
    #####################################################################


#------------------------------------------------------------------------------

NEEDED="wit wget tr"

BASE_PATH="/usr/local"
SHARE_PATH="/usr/local/share/wit"
URI_TITLES=http://gametdb.com/titles.txt
LANGUAGES="de es fr it ja ko nl pt ru zhcn zhtw"

SHARE_DIR=./share

#------------------------------------------------------------------------------

CYGWIN=0
if [[ $1 = --cygwin ]]
then
    shift
    CYGWIN=1
    SHARE_DIR=.
    export PATH=".:$PATH"
fi

#------------------------------------------------------------------------------

MAKE=0
if [[ $1 = --make ]]
then
    # it's called from make
    shift
    MAKE=1
fi

#------------------------------------------------------------------------------

function load_and_store()
{
    local URI="$1"
    local DEST="$2"
    local ADD="$3"

    echo "***    load $DEST from $URI"

    if wget -q -O- "$URI" | wit titles / - >"$DEST.tmp" && test -s "$DEST.tmp"
    then
	if [[ $ADD != "" ]]
	then
	    wit titles / "$ADD" "$DEST.tmp" >"$DEST.tmp.2"
	    mv "$DEST.tmp.2" "$DEST.tmp"
	fi

	if [[ -s $DEST ]]
	then
	    grep -v ^TITLES "$DEST"     >"$DEST.tmp.1"
	    grep -v ^TITLES "$DEST.tmp" >"$DEST.tmp.2"
	    if ! diff -q "$DEST.tmp.1" "$DEST.tmp.2" >/dev/null
	    then
		#echo "            => content changed!"
		mv "$DEST.tmp" "$DEST"
	    fi
	else
	    mv "$DEST.tmp" "$DEST"
	fi
    fi
    rm -f "$DEST.tmp" "$DEST.tmp.1" "$DEST.tmp.2"
}

#------------------------------------------------------------------------------

errtool=
for tool in $NEEDED
do
    ((CYGWIN)) && [[ -x $tool.exe ]] && continue
    which $tool >/dev/null 2>&1 || errtool="$errtool $tool"
done

if [[ $errtool != "" ]]
then
    echo "missing tools in PATH:$errtool" >&2
    exit 2
fi

#------------------------------------------------------------------------------

mkdir -p "$SHARE_DIR"

load_and_store "$URI_TITLES" "$SHARE_DIR/titles.txt"

# load language specific title files

for lang in $LANGUAGES
do
    LANG="$( echo $lang | tr '[a-z]' '[A-Z]' )"
    load_and_store $URI_TITLES?LANG=$LANG "$SHARE_DIR/titles-$lang.txt" "$SHARE_DIR/titles.txt"
done

if (( !MAKE && !CYGWIN ))
then
    echo "*** install titles to $SHARE_PATH"
    mkdir -p "$SHARE_PATH"
    cp -p "$SHARE_DIR"/titles*.txt "$SHARE_PATH"
fi

# remove a possible temp dir in SHARE_PATH
((CYGWIN)) || rm -rf "$SHARE_PATH/$SHARE_DIR"

