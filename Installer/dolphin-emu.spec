#
# Spec file for package Dolphin Emulator
#
# Copyright © 2014–2018 Markus S. <kamikazow@opensuse.org>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#

%define _localname dolphin-emu
%define _prettyname Dolphin
%global debug_package %{nil}

Name:             %{_localname}
Summary:          %{_prettyname}, a GameCube and Wii Emulator
Version:          5.0.0
Release:          0
Group:            System/Emulators/Other
License:          GPL-2.0-or-later
URL:              https://dolphin-emu.org/
Source0:          %{name}-%{version}.tar.xz
BuildArch:        x86_64 aarch64

# Package names verified with Fedora and openSUSE.
# Should the packages in your distro be named differently,
# see http://en.opensuse.org/openSUSE:Build_Service_cross_distribution_howto
#
# All other distros should work as well as Dolphin bundles
# its dependencies for static linking.

BuildRequires:    cmake >= 3.5
BuildRequires:    desktop-file-utils
BuildRequires:    libevdev-devel
BuildRequires:    libSM-devel
BuildRequires:    libcurl-devel
BuildRequires:    lzo-devel
BuildRequires:    mbedtls-devel
BuildRequires:    pkgconfig(alsa)
BuildRequires:    pkgconfig(ao)
BuildRequires:    pkgconfig(bluez)
BuildRequires:    pkgconfig(gl)
BuildRequires:    pkgconfig(libpng)
BuildRequires:    pkgconfig(libpulse)
BuildRequires:    pkgconfig(libudev)
BuildRequires:    pkgconfig(libusb-1.0)
BuildRequires:    pkgconfig(sdl2)
BuildRequires:    pkgconfig(sfml-all)
BuildRequires:    pkgconfig(soundtouch)
BuildRequires:    pkgconfig(xi)
BuildRequires:    pkgconfig(xext)
BuildRequires:    pkgconfig(xinerama)
BuildRequires:    pkgconfig(xrandr)
BuildRequires:    pkgconfig(xxf86vm)
BuildRequires:    pkgconfig(zlib)

## Qt GUI
BuildRequires:    pkgconfig(Qt5Core)
BuildRequires:    pkgconfig(Qt5Widgets)

%if 0%{?fedora} || 0%{?rhel_version} || 0%{?centos_version}
BuildRequires:    gettext
BuildRequires:    gtest-devel
BuildRequires:    hidapi-devel
BuildRequires:    mesa-libGL-devel
BuildRequires:    miniupnpc-devel
BuildRequires:    SOIL-devel
%endif

%if 0%{?suse_version}
BuildRequires:    libQt5Gui-private-headers-devel
BuildRequires:    libhidapi-devel
BuildRequires:    libminiupnpc-devel
BuildRequires:    update-desktop-files
BuildRequires:    pkg-config
BuildRequires:    pkgconfig(libavcodec)
BuildRequires:    pkgconfig(libavformat)
BuildRequires:    pkgconfig(libavutil)
BuildRequires:    pkgconfig(libswscale)

# Not in default repos:
BuildRequires:    libSOIL-devel
## wx GUI
#BuildRequires:    wxWidgets-3_2-devel
%endif
#BuildRequires:    gtk2-devel

# openSUSE Leap 42.x defaults to GCC 4
%if 0%{?suse_version} == 1315
%define _cxx g++-7
BuildRequires:    gcc7 gcc7-c++
%else
%define _cxx g++
BuildRequires:    gcc gcc-c++
%endif

Requires(post):   hicolor-icon-theme
Requires(postun): hicolor-icon-theme

%description
%{_prettyname} is an emulator for Nintendo GameCube and Wii.
It allows PC gamers to enjoy games for these two consoles in full HD with
several enhancements such as compatibility with all PC controllers, turbo
speed, networked multiplayer, and more.
Most games run perfectly or with minor bugs.

# -----------------------------------------------------

%package lang
Summary:          Translations for %{_prettyname} Emulator
BuildArch:        noarch

%description lang
Translations into various languages for %{_prettyname} Emulator

%files lang
%{_datadir}/locale

# -----------------------------------------------------

%prep
%setup -q -n %{name}-%{version}

%build
export CCFLAGS='%{optflags}'

# CMake options:
# - CMAKE_CXX_COMPILER: Set GCC version
# - ENABLE_ALSA: ALSA sound back-end (on by default, crashes Qt port)
# - ENABLE_ANALYTICS: Analytics on (turn off for forks)
# - ENABLE_QT2: Qt GUI (on by default, turn off for wx-only builds)
# - ENABLE_WX: wxWidgets GUI (on by default)
# - DOLPHIN_WC_REVISION: Set vesion number for About window
# - DOLPHIN_WC_BRANCH: Set branch name for About window (usually set to "master")

cmake \
      -DCMAKE_CXX_COMPILER=%{_cxx} \
      -DENABLE_ALSA=OFF \
      -DENABLE_QT2=ON \
      -DENABLE_WX=OFF \
      -DENABLE_ANALYTICS=ON \
      -DDOLPHIN_WC_REVISION=%{version} \
      -DDOLPHIN_WC_BRANCH=master \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=%{_prefix} \
      .
make -j1

%install
export CCFLAGS='%{optflags}'
make %{?_smp_mflags} install DESTDIR="%{?buildroot}"

mkdir -p %{buildroot}%{_udevrulesdir}
install -m 0644 Data/51-usb-device.rules %{buildroot}%{_udevrulesdir}/51-dolphin-usb-device.rules

# Get rid of static libraries
cd ..
find %{buildroot} -name '*.a' -delete
# Don't want dev files
rm -rf %{buildroot}%{_includedir}/

# ------------ Workarounds for the Qt port ------------
## Delete nongui binary if built without wx GUI.
if [ ! -f "%{buildroot}%{_bindir}/%{_localname}-wx" ]
then
 cd %{buildroot}%{_bindir}
 rm -rf %{_localname}-nogui
fi
# -----------------------------------------------------

%if 0%{?suse_version}
# Replace desktop file category 'Game;Emulator;' with 'System;Emulator;'
# under openSUSE or else build fails.
# See https://en.opensuse.org/openSUSE:Packaging_Conventions_RPM_Macros#.25suse_update_desktop_file
# and https://build.opensuse.org/package/view_file/X11:common:Factory/update-desktop-files/suse_update_desktop_file.sh
%suse_update_desktop_file '%{_localname}' 'System;Emulator;'
%endif

%post
/bin/touch --no-create %{_datadir}/icons/hicolor &>/dev/null || :

%postun
if [ $1 -eq 0 ] ; then
    /bin/touch --no-create %{_datadir}/icons/hicolor &>/dev/null
    /usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :
fi

%posttrans
/usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%doc license.txt Readme.md
%{_bindir}/%{_localname}*
%{_datadir}/%{_localname}
%{_datadir}/applications/%{_localname}.desktop
%{_datadir}/icons/hicolor/256x256/apps/%{_localname}.png
%{_datadir}/icons/hicolor/scalable/apps/%{_localname}.svg
%{_mandir}/man6/%{_localname}*
%{_udevrulesdir}/51-dolphin-usb-device.rules

%changelog
