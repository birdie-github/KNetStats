Name:		knetstats
Version:	2.0.0.alpha
Release:	1
Summary:	Network interface and statistics viewer

Group:		Productivity/Network/Viewers
License:	GPL-2
URL:		https://github.com/birdie-github/KNetStats
Source:	    https://github.com/birdie-github/KNetStats/archive/v%{version}.tar.gz

BuildRequires: gcc-c++
BuildRequires: cmake
BuildRequires: pkgconfig
BuildRequires: qt6-qtbase-devel

%description
KNetStats is a simple Qt network interface and statistics viewer with an associated tray icon to monitor activity.

%prep
%autosetup -n KNetStats-%{version}

%build
%cmake -DUSE_QT=6
%cmake_build

%install
%cmake_install

%files
/usr/bin/*
/usr/share/icons/*
/usr/share/applications/*
/usr/share/metainfo/*
