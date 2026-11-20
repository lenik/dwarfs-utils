#!/usr/bin/env bash
set -euo pipefail
# args: prefix bindir datadir mandir (prefix unused but kept for symmetry)
bindir="${2:?bindir}"
datadir="${3:?datadir}"
mandir="${4:?mandir}"

for p in \
    "$bindir/dwarfs-fm" \
    "$mandir/man1/dwarfs-fm.1" \
    "$datadir/bash-completion/completions/dwarfs-fm"
do
    if [ -L "$p" ]; then
        sudo rm -f "$p"
        printf "Removed %s\n" "$p"
    else
        printf "Skipped (not a symlink): %s\n" "$p"
    fi
done
