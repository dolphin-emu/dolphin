#!/bin/bash
#
# https://github.com/hrydgard/ppsspp-ffmpeg
#
#ffmpeg for windows x64
#windows_x64-build.sh requirements.
# MinGW
#
# WARNING: If cc_check fails, rename MingW/msys/1.0/bin/link.exe as it clashes with
# the MSVC one.
#
# MSYS
# 1. open x64 Native Tools Command Prompt for Visual Studio
# 2. $ cd /d path/to/msys2
# 3. $ msys2_shell.cmd -use-full-path
# 4. MSYS shell appears
# 5. $ cd /path/to/this/script
# 6. $ ./windows_x64-build.sh
#
#build requements.
# http://ffmpeg.org/platform.html#Windows

#///////////////////////////////////////////////////////////////////////////////

cd ffmpeg

set -e
rm -f config.mak

ARCH=x86_64

PREFIX=".."

mkdir -p ${PREFIX}

IS_STATIC_LIB=TRUE

IS_SHARED_LIB_INTO_BIN_DIR=FALSE

GENERAL="
    --toolchain=msvc
    --prefix=$PREFIX
    --arch=${ARCH}
    --disable-avdevice
    --disable-programs
    --disable-avfilter
    --disable-postproc
    --disable-doc
    --disable-pthreads
    --enable-w32threads
    --disable-network
    --disable-everything
    --disable-encoders
    --disable-muxers
    --disable-hwaccels
    --disable-parsers
    --disable-protocols
"

AUDIO_DECODERS=""

VIDEO_DECODERS=""

AUDIO_ENCODERS=""

VIDEO_ENCODERS="
    --enable-encoder=ffv1
    --enable-encoder=utvideo
    --enable-encoder=mpeg4
"

HARDWARE_ACCELS=""

MUXERS="
    --enable-muxer=avi
"

DEMUXERS=""

PARSERS=""

PROTOCOLS="
	--enable-protocol=file
"

BSFS=""

INPUT_DEVICES=""

OUTPUT_DEVICES=""

FILTERS=""

#///////////////////////////////////////////////////////////////////////////////

append() {
    var=$1
    shift
    eval "$var=\"\$$var $*\""
}

isstaticlib() {
    case "$IS_STATIC_LIB" in
        "TRUE" | "true" | "1" ) return 0 ;;
        *) return 1 ;;
    esac
}

isintobin() {
    case "$IS_SHARED_LIB_INTO_BIN_DIR" in
        "TRUE" | "true" | "1" ) return 0 ;;
        *) return 1 ;;
    esac
}

genelatelibparams() {
    ret=""
    if (isstaticlib) then
        ret=" --enable-static --disable-shared"
    else
        ret=" --enable-shared --disable-static"
    fi
    echo "$ret"
}

genelateparams() {
    eval "value=\"\$$1\""
    ret=""
    value=$(echo "$value" | sed "s/ //g")
    for var in $value ; do
        if [ ! `echo "$var" | fgrep -o "#"` ]; then
            ret="$ret $var"
        fi
    done
    echo "$ret"
}

params_dump() {
    eval "value=\"\$$1\""
    echo "---- dump configure params ----"
    IFS=" "
    for var in $value ; do
        echo "$var"
    done
    echo "---- end dump ----"
}


function build_ffmpeg
{
echo "Converting From CRLF To LF."
find ./ -regex "\(.*\.mak\|.*Makefile\)" | xargs dos2unix

echo "Generate configure params."

PARAMS="$(genelateparams GENERAL)\
$(genelatelibparams)\
$(echo -e "$(genelateparams AUDIO_DECODERS)")\
$(echo -e "$(genelateparams VIDEO_DECODERS)")\
$(echo -e "$(genelateparams AUDIO_ENCODERS)")\
$(echo -e "$(genelateparams VIDEO_ENCODERS)")\
$(echo -e "$(genelateparams BSFS)")\
$(echo -e "$(genelateparams PARSERS)")\
$(echo -e "$(genelateparams MUXERS)")\
$(echo -e "$(genelateparams DEMUXERS)")\
$(echo -e "$(genelateparams HARDWARE_ACCELS)")\
$(echo -e "$(genelateparams INPUT_DEVICES)")\
$(echo -e "$(genelateparams PROTOCOLS)")
"
params_dump PARAMS

# these are not necessary
# $(echo -e "$(genelateparams MUXERS)")
# $(echo -e "$(genelateparams AUDIO_ENCODERS)")\
# $(echo -e "$(genelateparams VIDEO_ENCODERS)")\


echo "---- configure ----"
./configure --extra-cflags="-MT -IWindowsInclude -GS-" $PARAMS

echo "---- make clean ----"
make clean
echo "---- make install ----"
make install 2>&1 | tee build.log
echo "---- rename and copy for ppsspp ----"
if (isstaticlib) then
    pushd $PREFIX/lib
    echo "Renaming "foo.a" to "foo.lib" in the build-directory."
    for fname in *.a; do
        mv -fv $fname $(echo "$fname" | sed -e "s/lib\(.*\)\.a/\1/").lib
    done
    popd
else
    binpath=$PREFIX/bin
    libpath=$PREFIX/lib
    absbin=$(cd $(dirname "$binpath/*") && pwd)
    absbin=$(echo "${absbin//\//\\}" | sed -e "s/\\\\\(.\)/\1:/")
    pushd $libpath
    for fname in *.def; do
        outname=${fname%%-*}
        if (isintobin) then
            abspath="$absbin\\$outname.lib"
            lib \/machine:i386 \/def:$fname \/out:$abspath
            popd
        	rm -fv $binpath/$outname.exp
        	pushd $libpath
        else
            lib \/machine:i386 \/def:$fname \/out:$outname.lib
            rm -fv $outname.exp
        fi
    done
    popd
fi
echo "---- windows_x64-build.sh finished ----"
}
build_ffmpeg
