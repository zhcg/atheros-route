Summary: 	SAR, SADF, MPSTAT and IOSTAT for Linux
Name: 		sysstat
Version: 	6.0.1
Release: 	1
Copyright: 	GPL
Group: 		Applications/System
Source0: 	sysstat-6.0.1.tar.gz
URL:		http://perso.wanadoo.fr/sebastien.godard
Packager:	Damien Faure <damien-jn.faure@bull.net>
BuildRoot:	%{_tmppath}/%{name}-%{version}-root-%(id -u -n)

%description
The sysstat package contains the sar, sadf, mpstat, iostat and sa 
tools for Linux.
The sar command collects and reports system activity information.
The sadf command may  be used to display data collected by sar in
various formats (XML, database-friendly, etc.).
The iostat command reports CPU utilization and I/O statistics for disks.
The mpstat command reports global and per-processor statistics.
The information collected by sar can be saved in a file in a binary 
format for future inspection. The statistics reported by sar concern 
I/O transfer rates, paging activity, process-related activities, 
interrupts, network activity, memory and swap space utilization, CPU
utilization, kernel activities and TTY statistics, among others. Both 
UP and SMP machines are fully supported.

%prep 
%setup 

%build
make PREFIX=%{_prefix} \
	SA_LIB_DIR=%{_libdir}/sa \
	MAN_DIR=%{_mandir} 

%install
rm -rf $RPM_BUILD_ROOT
install -d $RPM_BUILD_ROOT/var/log/sa

make IGNORE_MAN_GROUP=y \
	DESTDIR=$RPM_BUILD_ROOT \
	PREFIX=%{_prefix} \
	MAN_DIR=%{_mandir} \
	SA_LIB_DIR=%{_libdir}/sa \
	install

mkdir -p $RPM_BUILD_ROOT/etc/rc.d/init.d
install -m 755  sysstat $RPM_BUILD_ROOT/etc/rc.d/init.d/sysstat
mkdir -p $RPM_BUILD_ROOT/etc/sysconfig
install -m 644 sysstat.sysconfig $RPM_BUILD_ROOT/etc/sysconfig/sysstat
install -m 644 sysstat.ioconf $RPM_BUILD_ROOT/etc/sysconfig/sysstat.ioconf
mkdir -p $RPM_BUILD_ROOT/etc/rc2.d
cd $RPM_BUILD_ROOT/etc/rc2.d && ln -sf ../init.d/sysstat S03sysstat
mkdir -p $RPM_BUILD_ROOT/etc/rc3.d
cd $RPM_BUILD_ROOT/etc/rc3.d && ln -sf ../init.d/sysstat S03sysstat
mkdir -p $RPM_BUILD_ROOT/etc/rc5.d
cd $RPM_BUILD_ROOT/etc/rc5.d && ln -sf ../init.d/sysstat S03sysstat

%clean
rm -rf $RPM_BUILD_ROOT

%files 
%defattr(644,root,root,755)
%doc %{_prefix}/doc/sysstat-%{version}/*
%attr(755,root,root) %{_bindir}/*
%attr(755,root,root) %{_libdir}/sa/*
%attr(644,root,root) %{_mandir}/man*/*
%attr(644,root,root) %{_datadir}/locale/*/LC_MESSAGES/sysstat.mo
%attr(755,root,root) %dir /var/log/sa
%attr(755,root,root) /etc/rc.d/init.d/sysstat
%attr(644,root,root) /etc/sysconfig/sysstat
%attr(644,root,root) /etc/sysconfig/sysstat.ioconf
%attr(755,root,root) /etc/rc2.d/S03sysstat
%attr(755,root,root) /etc/rc3.d/S03sysstat
%attr(755,root,root) /etc/rc5.d/S03sysstat

