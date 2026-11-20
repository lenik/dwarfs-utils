/*
 * Copyright (C) 2026 Lenik <dwarfs-utils@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "paths.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void expect_true(const char *msg, int cond) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        failures++;
    }
}

static void expect_streq(const char *msg, const char *a, const char *b) {
    if (!a || !b || strcmp(a, b) != 0) {
        fprintf(stderr, "FAIL: %s (got '%s' expected '%s')\n", msg, a ? a : "(null)",
                b ? b : "(null)");
        failures++;
    }
}

int main(void) {
    char buf[512];

    expect_true("image .dwarfs", is_dwarfs_image_path("/tmp/foo.dwarfs"));
    expect_true("image .DWFS", is_dwarfs_image_path("/tmp/foo.DWFS"));
    expect_true("image .dw", is_dwarfs_image_path("/tmp/foo.dw"));
    expect_true("not image zip", !is_dwarfs_image_path("/tmp/foo.zip"));

    expect_true("convertible zip", is_convertible_archive_path("/a/b.zip"));
    expect_true("not convert dwarfs", !is_convertible_archive_path("/a/b.dwarfs"));

    expect_true("sibling ok", sibling_mount_path("/data/pkg.dwarfs", buf, sizeof buf) == 0);
    expect_streq("sibling path", buf, "/data/pkg");

    expect_true("sibling dwfs", sibling_mount_path("/data/pkg.dwfs", buf, sizeof buf) == 0);
    expect_streq("sibling dwfs path", buf, "/data/pkg");

    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    puts("ok");
    return 0;
}
