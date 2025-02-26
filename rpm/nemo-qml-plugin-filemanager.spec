Name:       nemo-qml-plugin-filemanager
Summary:    File manager plugin for Nemo Mobile
Version:    0.1.34
Release:    1
License:    BSD
URL:        https://github.com/sailfishos/nemo-qml-plugin-filemanager
Source0:    %{name}-%{version}.tar.bz2
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Concurrent)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Test)
BuildRequires:  pkgconfig(qt5-boostable)
BuildRequires:  pkgconfig(KF5Archive)

%description
%{summary}.

%package devel
Summary:    Filemanager C++ library
Requires:   %{name} = %{version}-%{release}

%description devel
%{summary}.

%package tests
Summary:    File manager plugin tests
Requires:   %{name} = %{version}-%{release}

%description tests
%{summary}.

%prep
%setup -q -n %{name}-%{version}

%build
%qmake5 "VERSION=%{version}"
%make_build

%install
%qmake5_install
chmod o+w -R %{buildroot}/opt/tests/%{name}/auto/folder
chmod o-r -R %{buildroot}/opt/tests/%{name}/auto/hiddenfolder

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%license LICENSE.BSD
%{_libdir}/qt5/qml/Nemo/FileManager
%{_libdir}/libfilemanager.so.*
%{_bindir}/fileoperationsd
%{_datadir}/dbus-1/services/org.nemomobile.FileOperations.service
%{_datadir}/dbus-1/interfaces/org.nemomobile.FileOperations.xml

%files devel
%{_libdir}/pkgconfig/filemanager.pc
%{_includedir}/filemanager
%{_libdir}/libfilemanager.so

%files tests
/opt/tests/%{name}
/opt/tests/%{name}/tests.xml

