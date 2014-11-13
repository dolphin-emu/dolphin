#
# Spec file for package Dolphin Emulator
#
# Copyright © 2014 Markus S. <kamikazow@web.de>
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

Name:       dolphin-emu
Summary:    Dolphin Emulator
Version:    4.0.2
Release:    0%{?dist}
Group:      System/Emulators/Other
License:    GPL-2.0
URL:        http://www.dolphin-emu.org/
BuildArch:  x86_64 armv7l aarch64

# For this spec file to work, the Dolphin Emulator sources must be located
# in a directory named dolphin-emu-4.0 (with "4.0" being the version
# number defined above).
# If the sources are compressed in another format than .tar.xz, change the
# file extension accordingly.
Source0:    %{name}-%{version}.tar.xz

# Package names verified with, CentOS, Fedora and openSUSE.
# Should the packages in your distro be named differently,
# see http://en.opensuse.org/openSUSE:Build_Service_cross_distribution_howto
#
# All other distros should work as well as Dolphin bundles
# its dependencies for static linking.

BuildRequires:  desktop-file-utils
BuildRequires:  cmake >= 2.8
BuildRequires:  gcc-c++
BuildRequires:  gtk2-devel
BuildRequires:  pkgconfig(alsa)
BuildRequires:  pkgconfig(ao)
BuildRequires:  pkgconfig(bluez)
BuildRequires:  pkgconfig(libpulse)
BuildRequires:  pkgconfig(xrandr)
BuildRequires:  pkgconfig(zlib)

%if 0%{?fedora}
BuildRequires:  libusb-devel
BuildRequires:  lzo-devel
# Disable miniupnpc in OBS for F20
BuildRequires:  miniupnpc-devel
BuildRequires:  openal-soft-devel
BuildRequires:  polarssl-devel
BuildRequires:  portaudio-devel
BuildRequires:  SDL2-devel
BuildRequires:  SFML-devel
BuildRequires:  SOIL-devel
BuildRequires:  soundtouch-devel
%endif

%if 0%{?suse_version}
BuildRequires:  libminiupnpc-devel
BuildRequires:  libSDL2-devel
BuildRequires:  libSOIL-devel
BuildRequires:  lzo-devel
BuildRequires:  openal-devel
BuildRequires:  portaudio-devel
BuildRequires:  sfml-devel
BuildRequires:  soundtouch-devel
BuildRequires:  update-desktop-files
%endif

# Use bundled wxGTK 3 except under the following distros:
%if 0%{?fedora_version} > 20
BuildRequires:  wxGTK3-devel
%endif

%description
Dolphin is an emulator for two Nintendo video game consoles, GameCube and the Wii.
It allows PC gamers to enjoy games for these two consoles in full HD with several
enhancements such as compatibility with all PC controllers, turbo speed,
networked multiplayer, and more.
Most games run perfectly or with minor bugs.

# ------------------------------------------------------

%package lang
Summary:        Translations for Dolphin Emulator
BuildArch:      noarch

%description lang
Translations into various languages for Dolphin Emulator

%files lang
%{_datadir}/locale

# ------------------------------------------------------

%package nogui
Summary:        Dolphin Emulator without a graphical user interface

%description nogui
Dolphin Emulator without a graphical user interface

%files nogui
%{_bindir}/%{name}-nogui

# ------------------------------------------------------

%prep
%setup -q

%build
export CCFLAGS='%{optflags}'
cmake . -DCMAKE_INSTALL_PREFIX=/usr
make %{?_smp_mflags}

%install
export CCFLAGS='%{optflags}'
make %{?_smp_mflags} install DESTDIR="%{?buildroot}"

%if 0%{?suse_version}
# Replace desktop file category 'Game;Emulator;' with 'System;Emulator;'
# under openSUSE or else build fails.
%suse_update_desktop_file -c %name Dolphin 'GameCube and Wii emulator' %{name} %{name} 'System;Emulator;'
%endif

%files
%defattr(-,root,root,-)
%doc license.txt Readme.md
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/pixmaps/dolphin-emu.xpm
%{_datadir}/applications/%{name}.desktop

%clean
rm -rf %{buildroot}

%changelog
