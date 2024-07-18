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
# remove application pathes

for tool in $BIN_FILES $WDF_LINKS
do
    key="/machine/SOFTWARE/Microsoft/Windows/CurrentVersion/App Paths/$tool.exe"
    if regtool check "$key" >/dev/null 2>&1
    then
	echo "* remove application path for '$tool.exe'"
	regtool unset "$key/" "${WDEST}\\${tool}.exe"
	regtool unset "$key/Path" "${WDEST}\\"
	regtool remove "$key"
    fi
done

#------------------------------------------------------------------------------
# remove WIT path to environment 'Path'

echo "* remove WIT path from environment 'Path'"

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
	[[ "$p" = "$WDEST" ]] || new_path="$new_path;$p"
    done

    [[ $new_path = "" ]] || regtool set -e "$key" "${new_path:1}"
}

set_path '/machine/SYSTEM/CurrentControlSet/Control/Session Manager/Environment/Path'
set_path '/user/Environment/Path'

#------------------------------------------------------------------------------

