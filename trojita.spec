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
Version:        0.2.9.4
Release:        1
# Almost everything: dual-licensed under the GPLv2 or GPLv3
# src/src/XtConnect: BSD
# src/Imap/Parser/3rdparty/kcodecs.*: LGPLv2
# src/Imap/Parser/3rdparty/qmailcodec.*: LGPLv2.1 or GPLv3
# src/Imap/Parser/3rdparty/rfccodecs.cpp: LGPLv2+
# src/qwwsmtpclient/: GPLv2
License:        (GPLv2 or GPLv3) and BSD and LGPLv2 and (LGPLv2.1 or GPLv3) and LGPLv2+ and GPLv2
Summary:        Qt IMAP e-mail client
Url:            http://trojita.flaska.net/
Group:          Productivity/Networking/Email/Clients
Source:         http://sourceforge.net/projects/trojita/files/src/%{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(QtGui) >= 4.6
BuildRequires:  pkgconfig(QtWebKit) >= 4.6
BuildRequires:  libQtWebKit-devel
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
 
%description
Trojita is a Qt IMAP e-mail client:
  * a pure Qt4 application with no additional dependencies
  * robust IMAP core implemented using Qt's Model-View framework
  * standards compliance is a design goal
  * on-demand message list and body part loading
  * offline IMAP support (you can access data you already have; there's no complete "offline mail access" yet, though)
  * support for bandwidth-saving mode aimed at mobile users with expensive connection
  * IMAP over SSH -- in addition to usual SSL/TLS connections, the server could be accessed via SSH
  * safe dealing with HTML mail (actually more robust than Thunderbird's)
 
%prep
%setup -q
 
%build
qmake PREFIX=/usr
make %{?_smp_mflags}
 
%install
make %{?_smp_mflags} INSTALL_ROOT=%{buildroot} install
 
%clean
%{?buildroot:rm -rf "%{buildroot}"}
 
%files
%defattr(-,root,root)
%doc LICENSE README
%{_bindir}/trojita
%{_datadir}/applications/trojita.desktop
%dir %{_datadir}/icons/hicolor
%dir %{_datadir}/icons/hicolor/*
%dir %{_datadir}/icons/hicolor/*/apps
%{_datadir}/icons/hicolor/32x32/apps/trojita.png
%{_datadir}/icons/hicolor/scalable/apps/trojita.svg
 
%changelog
