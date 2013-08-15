#!/bin/bash

VERSION=""
SOVERSION=""

# Parse arguments
#
until [ -z "$1" ]
do
  case "$1" in
    --version)
      # Version to use
      shift
      VERSION=$1
      ;;
    --soversion)
      shift
      SOVERSION=$1
      ;;
    -v|--verbose)
      # Be verbose
      VERBOSE="1"
      ;;
    -h|--help)
      # print help
      echo "Usage: $0"
      echo -e "  -h|--help\t\t\tPrint this help."
      echo -e "  --version <version>\tVersion to bump to."
      echo -e "  -v|--verbose\t\tVerbose."
      exit 1
      ;;
    *)
      # print error
      echo "Unknown argument: '$1'"
      exit 1
      ;;
  esac
  shift
done

if [ "X" = "X$VERSION" ];
then
  echo "No version specified. Unable to continue."
  exit 1
fi

[ $VERBOSE ] && echo "Bumping VERSION in library/CMakeLists.txt"
sed -e "s/ VERSION [0-9.]\+/ VERSION $VERSION/g" < library/CMakeLists.txt > tmp
mv tmp library/CMakeLists.txt

if [ "X" != "X$SOVERSION" ];
then
  [ $VERBOSE ] && echo "Bumping SOVERSION in library/CMakeLists.txt"
  sed -e "s/ SOVERSION [0-9]\+/ SOVERSION $SOVERSION/g" < library/CMakeLists.txt > tmp
  mv tmp library/CMakeLists.txt
fi

[ $VERBOSE ] && echo "Bumping VERSION in include/polarssl/version.h"
read MAJOR MINOR PATCH <<<$(IFS="."; echo $VERSION)
VERSION_NR="$( printf "0x%02X%02X%02X00" $MAJOR $MINOR $PATCH )"
cat include/polarssl/version.h |                                    \
    sed -e "s/_VERSION_MAJOR .\+/_VERSION_MAJOR  $MAJOR/" |    \
    sed -e "s/_VERSION_MINOR .\+/_VERSION_MINOR  $MINOR/" |    \
    sed -e "s/_VERSION_PATCH .\+/_VERSION_PATCH  $PATCH/" |    \
    sed -e "s/_VERSION_NUMBER .\+/_VERSION_NUMBER         $VERSION_NR/" |    \
    sed -e "s/_VERSION_STRING .\+/_VERSION_STRING         \"$VERSION\"/" |    \
    sed -e "s/_VERSION_STRING_FULL .\+/_VERSION_STRING_FULL    \"PolarSSL $VERSION\"/" \
    > tmp
mv tmp include/polarssl/version.h

[ $VERBOSE ] && echo "Bumping version in tests/suites/test_suite_version.data"
sed -e "s/version:\".\+/version:\"$VERSION\"/g" < tests/suites/test_suite_version.data > tmp
mv tmp tests/suites/test_suite_version.data

[ $VERBOSE ] && echo "Bumping PROJECT_NAME in doxygen/polarssl.doxyfile and doxygen/input/doc_mainpage.h"
for i in doxygen/polarssl.doxyfile doxygen/input/doc_mainpage.h;
do
  sed -e "s/PolarSSL v[0-9\.]\+/PolarSSL v$VERSION/g" < $i > tmp
  mv tmp $i
done

