#!/bin/bash

#svn co -r 75363 http://svn.wxwidgets.org/svn/wx/wxWidgets/trunk wxWidgets
git clone https://github.com/wxWidgets/wxWidgets.git
cd wxWidgets
git checkout cbb799b1ae3f309c99beb0d287e9bb3b62ea405c

case $OSTYPE in
darwin*)
export CC=clang CXX=clang++ CFLAGS="-stdlib=libc++" CXXFLAGS="-stdlib=libc++" CPPFLAGS="-stdlib=libc++"
BACKEND="osx_cocoa"
ADD_OPTS=--with-macosx-version-min=10.7 --enable-stl
;;
linux*)
BACKEND="gtk"
ADD_OPTS=--without-gtkprint
;;
esac

mkdir build-local
cd build-local

../configure --with-$BACKEND --disable-shared --enable-unicode --disable-compat28 --disable-exceptions --disable-fswatcher --without-regex --without-expat --disable-xml --disable-ribbon --disable-propgrid --disable-stc --disable-html --disable-richtext --without-libjpeg --without-libtiff --disable-webview --disable-markup --without-opengl --disable-printarch --disable-mediactrl --disable-htmlhelp --disable-dialupman --enable-ipv6 $ADD_OPTS
make
