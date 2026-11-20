# Version is injected by packaging/rpm/Makefile via `zfr version`.
# RPM Version cannot contain '-'; use `zfr version -r` (hyphens → '_').
# srcversion is the unsanitized Meson/git version and names the tarball.
%{!?version:%global version 0.0.0}
%{!?srcversion:%global srcversion %{version}}

Name:           dwarfs-utils
Version:        %{version}
Release:        1%{?dist}
Summary:        File-manager helper for DwarFS images

License:        AGPL-3.0-or-later
URL:            https://github.com/lenik/dwarfs-utils
Packager:       Lenik <dwarfs-utils@bodz.net>
Source0:        %{name}-%{srcversion}.tar.xz

BuildRequires:  meson
BuildRequires:  ninja-build
BuildRequires:  pkgconf
BuildRequires:  asciidoctor
BuildRequires:  pkgconfig(bas-c)

%description
Desktop suite helper and plugins for DwarFS filesystem images. Ships
dwarfs-fm, MIME/desktop data, and optional Caja/Nemo extensions. GUI
apps (dwarfs-viewer, dwarfs-archiver) are built separately via
scripts/build-gui.sh.

%prep
%setup -q -n %{name}-%{srcversion}

%build
meson setup build \
    --prefix=%{_prefix} \
    --bindir=%{_bindir} \
    --datadir=%{_datadir} \
    --libdir=%{_libdir} \
    --mandir=%{_mandir} \
    --sysconfdir=%{_sysconfdir} \
    --localstatedir=%{_localstatedir} \
    --buildtype=plain
meson compile -C build

%install
meson install -C build --destdir=%{buildroot}

%files
%{_bindir}/dwarfs-fm
%{_datadir}/bash-completion/completions/dwarfs-fm
%{_mandir}/man1/dwarfs-fm.1*
%{_datadir}/mime/*
%{_datadir}/doc/dwarfs-utils/
%changelog
* Fri Nov 20 2026 Lenik <dwarfs-utils@bodz.net>
- Desktop suite: dwarfs-fm, Viewer/Archiver desktop entries, FM plugins.
