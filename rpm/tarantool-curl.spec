Name: tarantool-curl
Version: 1.0.0
Release: 1%{?dist}
Summary: Curl based HTTP client for Tarantool
Group: Applications/Databases
License: BSD
URL: https://github.com/tarantool/tarantool-curl
Source0: https://github.com/tarantool/%{name}/archive/%{version}/%{name}-%{version}.tar.gz
BuildRequires: cmake >= 2.8
BuildRequires: gcc >= 4.5
BuildRequires: libcurl-devel
BuildRequires: tarantool-devel >= 1.6.8.0
BuildRequires: /usr/bin/prove
Requires: tarantool >= 1.6.8.0

%description
This package provides a Curl based HTTP client for Tarantool.

%prep
%setup -q -n %{name}-%{version}

%build
%cmake . -DCMAKE_BUILD_TYPE=RelWithDebInfo
make %{?_smp_mflags}

%install
%make_install

%files
%{_libdir}/tarantool/*/
%{_datarootdir}/tarantool/*/
%doc README.md
%{!?_licensedir:%global license %doc}
%license LICENSE AUTHORS

%changelog
* Wed Aug 31 2016 Andrey Drozdov <roman@tarantool.org> 1.0.0-1
- Initial version of the RPM spec
