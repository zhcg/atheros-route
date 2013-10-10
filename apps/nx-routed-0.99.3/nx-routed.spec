Summary: Nexus Routing Daemon 
Name: nx-routed
Version: 0.99.3
Release: 1
Group: Applications/Daemons
Copyright: (C) 2003,2005 Valery Kholodkov
Packager: Valery Kholodkov <valery@users.sourceforge.net>
URL: http://www.sf.net/projects/nx-routed
Source: %{name}-%{version}.tar.gz
%description
A Routing daemon for RIP-2 routing protocol.
%prep
%setup

%build
autoconf
./configure
make

%install
make install

%files
%defattr(-,root,root)
%config /etc/routed.conf
%attr(755,root,root) /usr/local/sbin/nx-routed
%attr(755,root,root) /etc/rc.d/init.d/routed
%doc /usr/share/man/man8/routed.8
%doc /usr/share/man/man5/routed.conf.5

