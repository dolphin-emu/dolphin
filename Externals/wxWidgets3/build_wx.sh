#!/bin/bash

svn co -r 70933 http://svn.wxwidgets.org/svn/wx/wxWidgets/trunk wxWidgets
cd wxWidgets

case $OSTYPE in
darwin*)
BACKEND="osx_cocoa"
;;
linux*)
BACKEND="gtk"
;;
esac

mkdir build-local
cd build-local

../configure --with-$BACKEND --disable-shared --enable-unicode --disable-compat28 --disable-exceptions --disable-fswatcher --without-regex --without-expat --disable-xml --disable-ribbon --disable-propgrid --disable-stc --disable-html --disable-richtext --without-libjpeg --without-libtiff --disable-webview --disable-markup
make
