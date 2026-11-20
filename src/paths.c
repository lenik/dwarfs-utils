/*
 * Copyright (C) 2026 Lenik <dwarfs-utils@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "paths.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *const dwarfs_suffixes[] = {
    ".dwarfs",
    ".dwfs",
    ".dw",
    NULL,
};

static const char *const convertible_suffixes[] = {
    ".zip",  ".rar",  ".7z",   ".tar",  ".tar.gz", ".tgz",  ".tar.xz",
    ".txz",  ".tar.bz2", ".tbz2", ".tar.zst", ".sqfs", ".sqxz", ".squashfs",
    ".iso",  ".cab",  ".arj",  ".lzh",  ".gz",  ".xz",  ".bz2",  ".zst",
    NULL,
};

static int str_ends_with_ci(const char *s, const char *suf) {
    size_t sl, sufl;
    size_t i;

    if (!s || !suf) {
        return 0;
    }
    sl = strlen(s);
    sufl = strlen(suf);
    if (sufl > sl) {
        return 0;
    }
    for (i = 0; i < sufl; i++) {
        char a = (char)tolower((unsigned char)s[sl - sufl + i]);
        char b = (char)tolower((unsigned char)suf[i]);
        if (a != b) {
            return 0;
        }
    }
    return 1;
}

static const char *strip_known_suffix(const char *path, const char *const *suffixes) {
    size_t i;
    for (i = 0; suffixes[i]; i++) {
        if (str_ends_with_ci(path, suffixes[i])) {
            return suffixes[i];
        }
    }
    return NULL;
}

bool is_dwarfs_image_path(const char *path) {
    return strip_known_suffix(path, dwarfs_suffixes) != NULL;
}

bool is_convertible_archive_path(const char *path) {
    if (is_dwarfs_image_path(path)) {
        return false;
    }
    return strip_known_suffix(path, convertible_suffixes) != NULL;
}

int sibling_mount_path(const char *image_path, char *out, size_t out_sz) {
    const char *suf;
    size_t len;

    if (!image_path || !out || out_sz == 0) {
        return -1;
    }
    suf = strip_known_suffix(image_path, dwarfs_suffixes);
    if (!suf) {
        return -1;
    }
    len = strlen(image_path) - strlen(suf);
    if (len == 0 || len + 1 > out_sz) {
        return -1;
    }
    memcpy(out, image_path, len);
    out[len] = '\0';
    return 0;
}

int resolve_mount_dir(const char *path, char *mount_out, size_t mount_sz) {
    struct stat st;

    if (!path || !mount_out || mount_sz == 0) {
        return -1;
    }
    if (stat(path, &st) != 0) {
        if (is_dwarfs_image_path(path)) {
            return sibling_mount_path(path, mount_out, mount_sz);
        }
        return -1;
    }
    if (S_ISDIR(st.st_mode)) {
        if (strlen(path) + 1 > mount_sz) {
            return -1;
        }
        memcpy(mount_out, path, strlen(path) + 1);
        return 0;
    }
    if (is_dwarfs_image_path(path)) {
        return sibling_mount_path(path, mount_out, mount_sz);
    }
    return -1;
}

bool is_dwarfs_mount_dir(const char *dir) {
    FILE *f;
    char line[4096];
    bool found = false;
    char target[4096];

    if (!dir) {
        return false;
    }
    /* Normalize: compare against mountinfo mount point field. */
    if (realpath(dir, target) == NULL) {
        snprintf(target, sizeof target, "%s", dir);
    }

    f = fopen("/proc/self/mountinfo", "r");
    if (!f) {
        f = fopen("/proc/mounts", "r");
        if (!f) {
            return false;
        }
        while (fgets(line, sizeof line, f)) {
            char mnt[1024], fstype[128];
            if (sscanf(line, "%1023s %*s %127s", mnt, fstype) >= 2) {
                if ((strcmp(fstype, "fuse.dwarfs") == 0 || strstr(fstype, "dwarfs") != NULL) &&
                    strcmp(mnt, target) == 0) {
                    found = true;
                    break;
                }
            }
        }
        fclose(f);
        return found;
    }

    while (fgets(line, sizeof line, f)) {
        /* mountinfo: ... mount_point mount_opts ... - fstype ... */
        char *dash = strstr(line, " - ");
        char *mnt_start;
        char *mnt_end;
        char mnt[1024];
        size_t mlen;
        int field;

        if (!dash) {
            continue;
        }
        /* Field 5 is mount point (1-based); skip 4 fields. */
        mnt_start = line;
        for (field = 1; field < 5; field++) {
            mnt_start = strchr(mnt_start, ' ');
            if (!mnt_start) {
                break;
            }
            mnt_start++;
        }
        if (!mnt_start || mnt_start >= dash) {
            continue;
        }
        mnt_end = strchr(mnt_start, ' ');
        if (!mnt_end || mnt_end > dash) {
            continue;
        }
        mlen = (size_t)(mnt_end - mnt_start);
        if (mlen >= sizeof mnt) {
            continue;
        }
        memcpy(mnt, mnt_start, mlen);
        mnt[mlen] = '\0';
        if (strcmp(mnt, target) != 0) {
            continue;
        }
        if (strstr(dash, "fuse.dwarfs") != NULL || strstr(dash, " dwarfs ") != NULL) {
            found = true;
            break;
        }
    }
    fclose(f);
    return found;
}
