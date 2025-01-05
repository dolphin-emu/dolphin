%define	RELEASE	1
%define rel     %{?CUSTOM_RELEASE} %{!?CUSTOM_RELEASE:%RELEASE}
%define	prefix	/usr

Name: %NAME
Summary: An encoder/decoder for the VCDIFF (RFC 3284) format
Version: %VERSION
Release: %rel
Group: Development/Libraries
URL: https://github.com/google/open-vcdiff
License: Apache 2.0
Vendor: Google
Packager: Google Inc. <opensource@google.com>
Source: http://%{NAME}.googlecode.com/files/%{NAME}-%{VERSION}.tar.gz
Distribution: Redhat 7 and above.
Buildroot: %{_tmppath}/%{name}-root
Prefix: %prefix

%description
A library that provides an encoder and decoder for the VCDIFF
(RFC 3284) format.  Please see http://www.ietf.org/rfc/rfc3284.txt .

%package devel
Summary: An encoder/decoder for the VCDIFF (RFC 3284) format
Group: Development/Libraries
Requires: %{NAME} = %{VERSION}

%description devel
The %name-devel package contains static and debug libraries and header files
for developing applications that use the %name package.

%changelog
    * Mon Jun 16 2008 <opensource@google.com>
    - First draft

%prep
%setup

%build
./configure
make prefix=%prefix

%install
rm -rf $RPM_BUILD_ROOT
make prefix=$RPM_BUILD_ROOT%{prefix} install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)

## Mark all installed files within /usr/share/doc/{package name} as
## documentation.  This depends on the following two lines appearing in
## Makefile.am:
##     docdir = $(prefix)/share/doc/$(PACKAGE)-$(VERSION)
##     dist_doc_DATA = AUTHORS COPYING ChangeLog INSTALL NEWS README
%docdir %{prefix}/share/doc/%{NAME}-%{VERSION}
%{prefix}/share/doc/%{NAME}-%{VERSION}/*

%docdir %{prefix}/share/man/man1/vcdiff.1*
%{prefix}/share/man/man1/vcdiff.1*

%{prefix}/bin/vcdiff
%{prefix}/lib/libvcdcom.so.0
%{prefix}/lib/libvcdcom.so.0.0.0
%{prefix}/lib/libvcdenc.so.0
%{prefix}/lib/libvcdenc.so.0.0.0
%{prefix}/lib/libvcddec.so.0
%{prefix}/lib/libvcddec.so.0.0.0

%files devel
%defattr(-,root,root)

%{prefix}/include/google
%{prefix}/lib/libvcdcom.a
%{prefix}/lib/libvcdcom.la
%{prefix}/lib/libvcdcom.so
%{prefix}/lib/libvcdenc.a
%{prefix}/lib/libvcdenc.la
%{prefix}/lib/libvcdenc.so
%{prefix}/lib/libvcddec.a
%{prefix}/lib/libvcddec.la
%{prefix}/lib/libvcddec.so

