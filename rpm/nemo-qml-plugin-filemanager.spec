Name:       nemo-qml-plugin-filemanager
Summary:    File manager plugin for Nemo Mobile
Version:    0.0.0
Release:    1
Group:      System/Libraries
License:    BSD
URL:        https://git.merproject.org/mer-core/nemo-qml-plugin-filemanager
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Concurrent)
BuildRequires:  pkgconfig(Qt5Gui)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Test)
BuildRequires:  pkgconfig(contactcache-qt5)
BuildRequires:  pkgconfig(qt5-boostable)
BuildRequires:  pkgconfig(KF5Archive)

%description
%{summary}.

%package tests
Summary:    File manager plugin tests
Group:      System/Libraries
Requires:   %{name} = %{version}-%{release}

%description tests
%{summary}.

%prep
%setup -q -n %{name}-%{version}

%build

%qmake5

make %{?_smp_mflags}

%install
rm -rf %{buildroot}
%qmake5_install
chmod o+w -R %{buildroot}/opt/tests/nemo-qml-plugins/filemanager/auto/folder
chmod o-r -R %{buildroot}/opt/tests/nemo-qml-plugins/filemanager/auto/hiddenfolder

%files
%defattr(-,root,root,-)
%{_libdir}/qt5/qml/Nemo/FileManager/libnemofilemanager.so
%{_libdir}/qt5/qml/Nemo/FileManager/plugins.qmltypes
%{_libdir}/qt5/qml/Nemo/FileManager/qmldir
%{_bindir}/fileoperationsd
%{_datadir}/dbus-1/services/org.nemomobile.FileOperations.service
%{_datadir}/dbus-1/interfaces/org.nemomobile.FileOperations.xml

%files tests
%defattr(-,root,root,-)
/opt/tests/nemo-qml-plugins/filemanager/
