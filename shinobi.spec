%define name	 shinobi
%define version  0.1
%define release  b1

Name:		%{name}
Summary:	Software VPN
Version:	%{version}
Release:	%{release}
License:	GPL
Group:		Applications/Internet
URL:		http://berry-lab.tk/shinobi/
Source:		%{name}-%{version}.tar.bz2

Packager:	Yuichiro Nakada
Vendor:		Berry Linux

Buildroot:	%{_tmppath}/%{name}-%{version}
BuildArchitectures: i686

%description
Shinobi is a useful tunneling application.

Shinobi supports SSL/TLS security, ethernet bridging, TCP or UDP tunnel
transport through proxies or NAT, support for dynamic IP addresses and
DHCP, scalability to hundreds or thousands of users, and portability to
most major OS platforms.

%prep
%setup -q

%build
[ -n "%{buildroot}" -a "%{buildroot}" != / ] && rm -rf %{buildroot}
mkdir -p %{buildroot}
make

%install
make install DEST=$RPM_BUILD_ROOT

%clean
%{__rm} -rf %{buildroot}


%files
/usr/bin/*

%changelog
* Tue Sep 27 2011 Yuichiro Nakada <berry@rberry.co.cc>
- Create for Berry Linux
