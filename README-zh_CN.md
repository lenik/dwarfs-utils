# dwarfs-utils

面向 [DwarFS](https://github.com/mhx/dwarfs) 文件系统镜像的桌面工具集：
文件管理器插件、查看器与打包器 GUI。

## 组件

| 包 / 程序 | 作用 |
|-----------|------|
| `dwarfs-fm` | 命令行助手：打开 / 打包 / 解包 / 校验 / 转换 / 挂载 / 卸载 |
| `dwarfs-viewer` | Tauri GUI — 元数据、内容导航（FUSE）、完整性检查 |
| `dwarfs-archiver` | Tauri GUI — 创建、解压、转换；完成后可打开查看器 |
| `caja-dwarfs` | Caja 右键 **DwarFS** 子菜单 |
| `nemo-dwarfs` | Nemo 右键 **DwarFS** 子菜单 |

### 右键菜单（DwarFS 子菜单）

- **Open** — `.dw` / `.dwfs` / `.dwarfs` 的默认打开方式
- **Archive** / **Unarchive** / **Check integrity**
- **Convert to DwarFS** — 将 zip、rar、7z、squashfs 等转为 DwarFS
- **Mount** — FUSE 挂载到同级目录（去掉镜像后缀）
- **Unmount** — 可对镜像或同级/挂载目录使用

挂载规则：`archive.dwarfs` → 同目录下的 `./archive`。

## 构建

### 原生助手与插件

```bash
sudo apt install meson ninja-build gcc pkg-config asciidoctor \
  libbas-c-dev libcaja-extension-dev libnemo-extension-dev libgtk-3-dev
meson setup /build
ninja -C /build
meson test -C /build
```

### GUI（Tauri + React + Tailwind + Vite）

```bash
./scripts/build-gui.sh
# 或：
cd apps/dwarfs-viewer && npm install && npm run tauri dev
cd apps/dwarfs-archiver && npm install && npm run tauri dev
```

需要 Rust、Node.js 以及 Tauri 的 Linux 依赖（如 `libwebkit2gtk-4.1-dev`）。

## 运行时依赖

- DwarFS 工具：`mkdwarfs`、`dwarfs`、`dwarfsck`、`dwarfsextract`
- `fuse3` / `fusermount3`
- 转换功能建议安装：`7z` 与/或 `unsquashfs`

## 许可证

Copyright (C) 2026 Lenik \<dwarfs-utils@bodz.net\>

采用 **AGPL-3.0-or-later** 许可。完整文本及补充条款（含反 AI 限制）见 `LICENSE`。
