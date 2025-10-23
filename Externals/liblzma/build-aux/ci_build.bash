#!/bin/bash
# SPDX-License-Identifier: 0BSD

#############################################################################
#
# Script meant to be used for Continuous Integration automation for POSIX
# systems. On GitHub, this is used by Ubuntu and MacOS builds.
#
#############################################################################
#
# Author: Jia Tan
#
#############################################################################

set -e

USAGE="Usage: $0
  -a [autogen flags]
  -b [autotools|cmake]
  -c [crc32|crc64|sha256]
  -d [encoders|decoders|bcj|delta|threads|shared|nls|small|clmul|sandbox]
  -f [CFLAGS]
  -l [destdir]
  -m [compiler]
  -n [ARTIFACTS_DIR_NAME]
  -p [all|build|test]
  -s [srcdir]"

# Absolute path of script directory
ABS_DIR=$(cd -- "$(dirname -- "$0")" && pwd)

# Default CLI option values
AUTOGEN_FLAGS=""
BUILD_SYSTEM="autotools"
CHECK_TYPE="crc32,crc64,sha256"
BCJ="y"
DELTA="y"
ENCODERS="y"
DECODERS="y"
THREADS="y"
SHARED="y"
NATIVE_LANG_SUPPORT="y"
SMALL="n"
CLMUL="y"
SANDBOX="y"
DOXYGEN="y"
SRC_DIR="$ABS_DIR/../"
DEST_DIR="$SRC_DIR/../xz_build"
PHASE="all"
ARTIFACTS_DIR_NAME="output"

[[ -z ${CPU_COUNT} ]] && { CPU_COUNT=$(nproc 2>/dev/null || sysctl -n hw.activecpu); }
[[ -z ${MAKEFLAGS} ]] && export MAKEFLAGS="-j${CPU_COUNT} -l${CPU_COUNT}"
[[ -z ${CFLAGS} ]] && export CFLAGS="-O2"

###################
# Parse arguments #
###################

while getopts a:b:c:d:l:m:n:s:p:f:w:h opt; do
	# b option can have either value "autotools" OR "cmake"
	case ${opt} in
	h)
		echo "$USAGE"
		exit 0
	;;
	a)
		AUTOGEN_FLAGS="$OPTARG"
	;;
	b)
		case "$OPTARG" in
			autotools) ;;
			cmake) ;;
			*) echo "Invalid build system: $OPTARG"; exit 1;;
		esac
		BUILD_SYSTEM="$OPTARG"
	;;
	c) CHECK_TYPE="$OPTARG"
	;;
	# d options can be a comma separated list of things to disable at
	# configure time
	d)
	for disable_arg in $(echo "$OPTARG" | sed "s/,/ /g"); do
		case "$disable_arg" in
		encoders) ENCODERS="n" ;;
		decoders) DECODERS="n" ;;
		bcj) BCJ="n" ;;
		delta) DELTA="n" ;;
		threads) THREADS="n" ;;
		shared) SHARED="n";;
		nls) NATIVE_LANG_SUPPORT="n";;
		small) SMALL="y";;
		clmul) CLMUL="n";;
		sandbox) SANDBOX="n";;
		doxygen) DOXYGEN="n";;
		*) echo "Invalid disable value: $disable_arg"; exit 1 ;;
		esac
	done
	;;
	l) DEST_DIR="$OPTARG"
	;;
	m)
		CC="$OPTARG"
		export CC
	;;
	n) ARTIFACTS_DIR_NAME="$OPTARG"
	;;
	s) SRC_DIR="$OPTARG"
	;;
	p) PHASE="$OPTARG"
	;;
	f)
		CFLAGS+=" $OPTARG"
		export CFLAGS
	;;
	w) WRAPPER="$OPTARG"
	;;
	*)
		echo "Unsupported option: $opt"
		exit 1
	;;
	esac
done


####################
# Helper Functions #
####################

# These two functions essentially implement the ternary "?" operator.
add_extra_option() {
	# First argument is option value ("y" or "n")
	# Second argument is option to set if "y"
	# Third argument is option to set if "n"
	if [ "$1" = "y" ]
	then
		EXTRA_OPTIONS="$EXTRA_OPTIONS $2"
	else
		EXTRA_OPTIONS="$EXTRA_OPTIONS $3"
	fi
}


add_to_filter_list() {
	# First argument is option value ("y" or "n")
	# Second argument is option to set if "y"
	if [ "$1" = "y" ]
	then
		FILTER_LIST="$FILTER_LIST$2"
	fi
}


###############
# Build Phase #
###############

if [ "$PHASE" = "all" ] || [ "$PHASE" = "build" ]
then
	# Checksum options should be specified differently based on the
	# build system. It must be calculated here since we won't know
	# the build system used until all args have been parsed.
	# Autotools - comma separated
	# CMake - semi-colon separated
	if [ "$BUILD_SYSTEM" = "autotools" ]
	then
		SEP=","
	else
		SEP=";"
	fi

	CHECK_TYPE_TEMP=""
	for crc in $(echo "$CHECK_TYPE" | sed "s/,/ /g"); do
			case "$crc" in
			crc32 | crc64 | sha256) ;;
			*) echo "Invalid check type: $crc"; exit 1 ;;
			esac

			CHECK_TYPE_TEMP="$CHECK_TYPE_TEMP$SEP$crc"
	done

	# Remove the first character from $CHECK_TYPE_TEMP since it will
	# always be the delimiter.
	CHECK_TYPE="${CHECK_TYPE_TEMP:1}"

	FILTER_LIST="lzma1$SEP"lzma2

	# Build based on arguments
	mkdir -p "$DEST_DIR"

	# Generate configure option values
	EXTRA_OPTIONS=""

	case $BUILD_SYSTEM in
	autotools)
		cd "$SRC_DIR"

		# Run autogen.sh script if not already run
		if [ ! -f configure ]
		then
			./autogen.sh "$AUTOGEN_FLAGS"
		fi

		cd "$DEST_DIR"

		add_to_filter_list "$BCJ" ",x86,powerpc,ia64,arm,armthumb,arm64,sparc,riscv"
		add_to_filter_list "$DELTA" ",delta"

		add_extra_option "$ENCODERS" "--enable-encoders=$FILTER_LIST" "--disable-encoders"
		add_extra_option "$DECODERS" "--enable-decoders=$FILTER_LIST" "--disable-decoders"
		add_extra_option "$THREADS" "" "--disable-threads"
		add_extra_option "$SHARED" "" "--disable-shared"
		add_extra_option "$NATIVE_LANG_SUPPORT" "" "--disable-nls"
		add_extra_option "$SMALL" "--enable-small" ""
		add_extra_option "$CLMUL" "" "--disable-clmul-crc"
		add_extra_option "$SANDBOX" "" "--disable-sandbox"
		add_extra_option "$DOXYGEN" "--enable-doxygen" ""

		# Workaround a bug in too old config.guess. Version with
		# timestamp='2022-05-08' would be needed but the autotools-dev
		# package has 2022-01-09 in Ubuntu 22.04LTS and 24.04LTS. The
		# bug breaks i386 assembler usage autodetection.
		if "$SRC_DIR/build-aux/config.guess" | grep -q x86_64-pc-linux-gnux32
		then
			EXTRA_OPTIONS="$EXTRA_OPTIONS --build=i686-pc-linux-gnu"
		fi

		# Run configure script
		"$SRC_DIR"/configure --enable-werror --enable-checks="$CHECK_TYPE" $EXTRA_OPTIONS --config-cache

		# Build the project
		make
	;;
	cmake)
		cd "$DEST_DIR"

		add_to_filter_list "$BCJ" ";x86;powerpc;ia64;arm;armthumb;arm64;sparc;riscv"
		add_to_filter_list "$DELTA" ";delta"

		add_extra_option "$THREADS" "-DXZ_THREADS=yes" "-DXZ_THREADS=no"

		# Disable MicroLZMA if encoders are not configured.
		add_extra_option "$ENCODERS" "-DXZ_ENCODERS=$FILTER_LIST" "-DXZ_ENCODERS= -DXZ_MICROLZMA_ENCODER=OFF"

		# Disable MicroLZMA and lzip decoders if decoders are not configured.
		add_extra_option "$DECODERS" "-DXZ_DECODERS=$FILTER_LIST" "-DXZ_DECODERS= -DXZ_MICROLZMA_DECODER=OFF -DXZ_LZIP_DECODER=OFF"

		# CMake disables the shared library by default.
		add_extra_option "$SHARED" "-DBUILD_SHARED_LIBS=ON" ""

		add_extra_option "$NATIVE_LANG_SUPPORT" "" "-DXZ_NLS=OFF"
		add_extra_option "$SMALL" "-DXZ_SMALL=ON" ""
		add_extra_option "$DOXYGEN" "-DXZ_DOXYGEN=ON" ""

		# Remove old cache file to clear previous settings.
		rm -f "CMakeCache.txt"
		cmake "$SRC_DIR/CMakeLists.txt" -B "$DEST_DIR" $EXTRA_OPTIONS -DXZ_CHECKS="$CHECK_TYPE" -G "Unix Makefiles"
		cmake --build "$DEST_DIR"
	;;
	esac
fi


##############
# Test Phase #
##############

if [ "$PHASE" = "all" ] || [ "$PHASE" = "test" ]
then
	case $BUILD_SYSTEM in
	autotools)
		cd "$DEST_DIR"
		# If the tests fail, copy the test logs into the artifacts folder
		if make check VERBOSE=1 LOG_COMPILER="$WRAPPER"
		then
			:
		else
			mkdir -p "$SRC_DIR/build-aux/artifacts/$ARTIFACTS_DIR_NAME"
			cp ./tests/*.log "$SRC_DIR/build-aux/artifacts/$ARTIFACTS_DIR_NAME"
			exit 1
		fi
	;;
	cmake)
		cd "$DEST_DIR"
		if ${WRAPPER} make CTEST_OUTPUT_ON_FAILURE=1 test
		then
			:
		else
			mkdir -p "$SRC_DIR/build-aux/artifacts/$ARTIFACTS_DIR_NAME"
			cp ./Testing/Temporary/*.log "$SRC_DIR/build-aux/artifacts/$ARTIFACTS_DIR_NAME"
			exit 1
		fi
	;;
	esac
fi
