/*
 * Copyright (C) 2026 Lenik <dwarfs-utils@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef DWARFS_UTILS_PATHS_H
#define DWARFS_UTILS_PATHS_H

#include <stdbool.h>
#include <stddef.h>

/* Known dwarfs image suffixes (longest first). */
bool is_dwarfs_image_path(const char *path);
bool is_convertible_archive_path(const char *path);

/*
 * Sibling mount directory for an image: strip .dw / .dwfs / .dwarfs.
 * Writes into out (size out_sz). Returns 0 on success, -1 on error.
 */
int sibling_mount_path(const char *image_path, char *out, size_t out_sz);

/*
 * If path is a dwarfs image, fill mount_out with sibling dir.
 * If path is a directory, copy it to mount_out (caller may verify mount).
 * Returns 0 on success.
 */
int resolve_mount_dir(const char *path, char *mount_out, size_t mount_sz);

/* True if path appears in /proc/self/mountinfo as a fuse.dwarfs mount. */
bool is_dwarfs_mount_dir(const char *dir);

#endif /* DWARFS_UTILS_PATHS_H */
