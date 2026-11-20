# dwarfs-utils

Desktop suite for [DwarFS](https://github.com/mhx/dwarfs) filesystem images:
file-manager helpers, a Viewer, and an Archiver GUI.

## Components

| Package / binary | Role |
|------------------|------|
| `dwarfs-fm` | CLI helper: open / archive / unarchive / check / convert / mount / unmount |
| `dwarfs-viewer` | Tauri GUI — metadata, content navigator (FUSE), integrity |
| `dwarfs-archiver` | Tauri GUI — create, extract, convert; optional open Viewer when done |
| `caja-dwarfs` | Caja **DwarFS** context submenu |
| `nemo-dwarfs` | Nemo **DwarFS** context submenu |

### Context menu (DwarFS submenu)

- **Open** — default for `.dw` / `.dwfs` / `.dwarfs`
- **Archive** / **Unarchive** / **Check integrity**
- **Convert to DwarFS** — other archives (zip, rar, 7z, squashfs, …)
- **Mount** — FUSE mount at sibling directory (strip image suffix)
- **Unmount** — works on the image or the sibling/mount directory

Mount rule: `archive.dwarfs` → `./archive` next to the file.

## Build

### Native helper + plugins

```bash
sudo apt install meson ninja-build gcc pkg-config asciidoctor \
  libbas-c-dev libcaja-extension-dev libnemo-extension-dev libgtk-3-dev
meson setup /build
ninja -C /build
meson test -C /build
```

### GUI apps (Tauri + React + Tailwind + Vite)

```bash
./scripts/build-gui.sh
# or per app:
cd apps/dwarfs-viewer && npm install && npm run tauri dev
cd apps/dwarfs-archiver && npm install && npm run tauri dev
```

Requires Rust, Node.js, and Tauri Linux prerequisites (`libwebkit2gtk-4.1-dev`, …).

## Runtime dependencies

- DwarFS tools: `mkdwarfs`, `dwarfs`, `dwarfsck`, `dwarfsextract`
- `fuse3` / `fusermount3`
- Convert: `7z` and/or `unsquashfs` (recommended)

## License

Copyright (C) 2026 Lenik \<dwarfs-utils@bodz.net\>

Licensed under **AGPL-3.0-or-later**.  
See `LICENSE` for the full text and supplemental project terms (including the anti-AI restriction).
