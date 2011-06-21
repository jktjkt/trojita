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
 
# norootforbuild
 
Name:           trojita
BuildRequires:  libqt4-devel libQtWebKit-devel update-desktop-files
License:        GPL v2 or later  
Group:          Productivity/Networking/Email/Clients
Summary:        A simple, Qt4 IMAP client
Version:        0.2.9.3
Release:        1
Url:            http://trojita.flaska.net/
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
Source0:        %{name}-%{version}.tar.bz2
 
%description
Trojita is a pure Qt4 IMAP email client.  It features:
  * Robust IMAP core, using Qt's Model-View
  * Standards Compliant
  * On-demand message list and body part loading
  * Offline IMAP support
  * Support for bandwidth-saving mode
  * IMAP over SSH -- instead of going over an SSL socket, the server 
    could be accessed via SSH
  * Safe dealing with HTML mail
 
 
 
Authors:
--------
    Jan Kundr�t
 
%prep
%setup -q -n %{name}-%{version}
 
%build
  qmake -r PREFIX=/usr
  make
 
%install
  make INSTALL_ROOT=$RPM_BUILD_ROOT install
 
%suse_update_desktop_file trojita
 
%clean
rm -rf $RPM_BUILD_ROOT
 
%files
%defattr(-,root,root)
/usr/bin/trojita
%{_datadir}/applications/trojita.desktop
%{_datadir}/icons/hicolor/32x32/apps/trojita.png
%{_datadir}/icons/hicolor/scalable/apps/trojita.svg
 
%changelog
