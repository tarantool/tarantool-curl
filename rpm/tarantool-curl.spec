Name: tarantool-curl
Version: 2.3.0
Release: 1%{?dist}
Summary: Curl based HTTP client for Tarantool
Group: Applications/Databases
License: BSD
URL: https://github.com/tarantool/tarantool-curl
Source0: https://github.com/tarantool/%{name}/archive/%{version}/%{name}-%{version}.tar.gz
BuildRequires: cmake >= 2.8
BuildRequires: gcc >= 4.5
BuildRequires: tarantool >= 1.7.2.0
BuildRequires: tarantool-devel
BuildRequires: libcurl-devel
BuildRequires: libev, libev-devel
BuildRequires: nodejs, libuv
BuildRequires: /usr/bin/prove

Requires: tarantool >= 1.7.2, libev

%description
This package provides a Curl based HTTP client for Tarantool.

%prep
%setup -q -n %{name}-%{version}

%build
%cmake . -DCMAKE_BUILD_TYPE=RelWithDebInfo
make %{?_smp_mflags}
make %{?_smp_mflags} test

%install
%make_install

%files
%{_libdir}/tarantool/*/
%{_datarootdir}/tarantool/*/
%doc README.md
%{!?_licensedir:%global license %doc}
%license LICENSE AUTHORS

%changelog
* Sun Apr 2 2017  V. Soshnikov <dedok.mad@gmail.com>  2.2.9-1
- Build issues have been fixed

* Mon Jan 30 2017 V. Soshnikov <dedok.mad@gmail.com> 2.2.3-1
- libev support
- imported many curl's options, see https://github.com/tarantool/curl/#api-reference

* Wed Aug 31 2016 Andrey Drozdov <andrey@tarantool.org> 1.0.0-1
- Initial version of the RPM spec
