#!/usr/bin/env bash
set -euo pipefail
prefix="${1:?prefix}"
bindir="${2:?bindir}"
datadir="${3:?datadir}"
mandir="${4:?mandir}"
build_root="${5:?build_root}"
source_root="${6:?source_root}"

mkdir -p "$bindir" "$datadir/bash-completion/completions" "$mandir/man1"
sudo ln -sfn "$build_root/dwarfs-fm" "$bindir/dwarfs-fm"
sudo ln -sfn "$build_root/dwarfs-fm.1" "$mandir/man1/dwarfs-fm.1"
sudo ln -sfn "$source_root/dwarfs-fm.bash" "$datadir/bash-completion/completions/dwarfs-fm"
printf "Symlinks installed under %s\n" "$prefix"
