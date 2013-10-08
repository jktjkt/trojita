#
# spec file for package trojita
#
# Copyright (c) 2011 SUSE LINUX Products GmbH, Nuernberg, Germany.
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.
 	
# Please submit bugfixes or comments via http://bugs.opensuse.org/
#
 
Name:           trojita
Version:        0.3.96
Release:        1
# Almost everything: dual-licensed under the GPLv2 or GPLv3
# (with KDE e.V. provision for relicensing)
# src/XtConnect: BSD
# src/Imap/Parser/3rdparty/kcodecs.*: LGPLv2
# Nokia imports: LGPLv2.1 or GPLv3
# src/Imap/Parser/3rdparty/rfccodecs.cpp: LGPLv2+
# src/qwwsmtpclient/: GPLv2
License:        (GPLv2 or GPLv3) and BSD and LGPLv2 and (LGPLv2.1 or GPLv3) and LGPLv2+ and GPLv2
Summary:        Qt IMAP e-mail client
Url:            http://trojita.flaska.net/
Group:          Productivity/Networking/Email/Clients
Source:         http://sourceforge.net/projects/trojita/files/src/%{name}-%{version}.tar.bz2
%if 0%{?fedora}
BuildRequires: qt-webkit-devel >= 4.6
BuildRequires: libstdc++-devel gcc-c++
BuildRequires: cmake >= 2.8.7
BuildRequires: xorg-x11-server-Xvfb
%endif
%if 0%{?rhel_version} || 0%{?centos_version}
BuildRequires: qtwebkit-devel >= 2.1
BuildRequires: libstdc++-devel gcc-c++
BuildRequires: cmake >= 2.8.7
BuildRequires: xorg-x11-server-Xvfb
%endif
%if 0%{?suse_version} || 0%{?sles_version}
BuildRequires: pkgconfig(QtGui) >= 4.6
BuildRequires: pkgconfig(QtWebKit) >= 4.6
BuildRequires: libQtWebKit-devel
BuildRequires: update-desktop-files
BuildRequires: xorg-x11-Xvfb xkeyboard-config
BuildRequires: cmake >= 2.8.7
%endif
%define         X_display         ":98"
BuildRoot:      %{_tmppath}/%{name}-%{version}-build

%if "%{?_lib}" == "lib64"
%define my_cmake_lib_suffix "-DLIB_SUFFIX=64"
%else
%define my_cmake_lib_suffix "-ULIB_SUFFIX"
%endif

%if 0%{?fedora} || 0%{?rhel_version} || 0%{?centos_version}
%global _hardened_build 1
%endif

%description
Trojita is a Qt IMAP e-mail client which:
  * Enables you to access your mail anytime, anywhere.
  * Does not slow you down. If we can improve the productivity of an e-mail user, we better do.
  * Respects open standards and facilitates modern technologies. We value the vendor-neutrality that IMAP provides and are committed to be as interoperable as possible.
  * Is efficient — be it at conserving the network bandwidth, keeping memory use at a reasonable level or not hogging the system's CPU.
  * Can be used on many platforms. One UI is not enough for everyone, but our IMAP core works fine on anything from desktop computers to cell phones and big ERP systems.
  * Plays well with the rest of the ecosystem. We don't like reinventing wheels, but when the existing wheels quite don't fit the tracks, we're not afraid of making them work.
 
%prep
%setup -q
 
%build
%if 0%{?suse_version} || 0%{?sles_version}
export CXXFLAGS="${RPM_OPT_FLAGS} -fPIC"
export LDFLAGS="-pie"
%else
export CXXFLAGS="${CXXFLAGS:-%optflags}"
%endif
cmake \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_VERBOSE_MAKEFILE=ON \
    -DCMAKE_INSTALL_PREFIX:PATH=%{_prefix} \
    -DCMAKE_INSTALL_LIBDIR:PATH=%{_libdir} %{my_cmake_lib_suffix} \
    -DSHARE_INSTALL_PREFIX:PATH=%{_datadir}
make %{?_smp_mflags}
 
%install
make %{?_smp_mflags} DESTDIR=%{buildroot} install
%if 0%{?suse_version} || 0%{?sles_version}
%suse_update_desktop_file %{buildroot}/%{_datadir}/applications/trojita.desktop
%endif
 
%clean
%{?buildroot:rm -rf "%{buildroot}"}
 
%files
%defattr(-,root,root)
%doc LICENSE README
%{_libdir}/libtrojita_plugins.so
%{_bindir}/trojita
%{_bindir}/be.contacts
%{_datadir}/applications/trojita.desktop
%dir %{_datadir}/icons/hicolor
%dir %{_datadir}/icons/hicolor/*
%dir %{_datadir}/icons/hicolor/*/apps
%{_datadir}/icons/hicolor/32x32/apps/trojita.png
%{_datadir}/icons/hicolor/scalable/apps/trojita.svg
%dir %{_datadir}/trojita
%dir %{_datadir}/trojita/locale
%{_datadir}/trojita/locale/trojita_common_*.qm

%check
export DISPLAY=%{X_display}
Xvfb %{X_display} &
trap "kill $! || true" EXIT
ctest --output-on-failure

%changelog
