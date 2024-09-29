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
    ##   This file installs the distribution on a windows system.      ##
    ##                                                                 ##
    #####################################################################


#------------------------------------------------------------------------------
# simple cygwin check

if [[ $1 != --cygwin ]]
then
    echo "Option --cygwin not set => exit" >&2
    exit 1
fi

#------------------------------------------------------------------------------
# pre definitions

BIN_FILES="wit wwt wdf wfuse"
WDF_LINKS="wdf-cat wdf-dump"
SHARE_FILES="titles.txt titles-de.txt titles-es.txt titles-fr.txt titles-it.txt titles-ja.txt titles-ko.txt titles-nl.txt titles-pt.txt titles-ru.txt titles-zhcn.txt titles-zhtw.txt system-menu.txt magic.txt"
WIN_INSTALL_PATH="Wiimm/WIT"

#------------------------------------------------------------------------------
# setup

echo "* setup"

export PATH=".:$PATH"

key="/machine/SOFTWARE/Microsoft/Windows/CurrentVersion/ProgramFilesDir"
if ! WIN_PROG_PATH="$(regtool get "$key")" || [[ $WIN_PROG_PATH = "" ]]
then
    echo "Can't determine Windows program path => abort" >&2
    exit 1
fi
#CYGWIN_PROG_PATH="$( realpath "$WIN_PROG_PATH" )"
CYGWIN_PROG_PATH="${WIN_PROG_PATH//\\//}"

WDEST="$WIN_PROG_PATH\\${WIN_INSTALL_PATH//\//\\}"
CDEST="$CYGWIN_PROG_PATH/$WIN_INSTALL_PATH"

#------------------------------------------------------------------------------
# install files

# mkdir before testing the directory
mkdir -p "$CDEST"

if [[ $(stat -c%d-%i "$PWD") != "$(stat -c%d-%i "$CDEST")" ]]
then
    echo "* install files to $WDEST"
    cp --preserve=time *.bat *.dll *.exe *.sh *.txt "$CDEST"
fi

#------------------------------------------------------------------------------
# define application pathes

for tool in $BIN_FILES $WDF_LINKS
do
    [[ -s "$CDEST/$tool.exe" ]] || continue
    echo "* define application path for '$tool.exe'"
    key="/machine/SOFTWARE/Microsoft/Windows/CurrentVersion/App Paths/$tool.exe"
    regtool add "$key"
    regtool set -s "$key/" "${WDEST}\\${tool}.exe"
    regtool set -s "$key/Path" "${WDEST}\\"
done

#------------------------------------------------------------------------------
# add WIT path to environment 'Path'

echo "* add WIT path to environment 'Path'"

function set_path()
{
    local key="$1"

    local p=
    local count=0
    local new_path=

    # split at ';' & substitute ' ' temporary to ';' to be space save
    for p in $( regtool --quiet get "$key" | tr '; ' '\n;' )
    do
	p="${p//;/ }"
	#echo " -> |$p|"
	if [[ "$p" == "$WDEST" ]]
	then
	    ((count++)) || new_path="$new_path;$p"
	    #echo "$count $WDEST"
	elif [[ $p != "" ]]
	then
	    new_path="$new_path;$p"
	fi
    done

    if [[ $new_path != "" ]]
    then
	((count)) || new_path="$new_path;$WDEST"
	#echo "count=$count"
	#echo "new_path=${new_path:1}"
	regtool set -e "$key" "${new_path:1}"
    fi
}

set_path '/machine/SYSTEM/CurrentControlSet/Control/Session Manager/Environment/Path'
set_path '/user/Environment/Path'

#------------------------------------------------------------------------------

