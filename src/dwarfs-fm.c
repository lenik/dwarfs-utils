/*
 * Copyright (C) 2026 Lenik <dwarfs-utils@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L

#include "config.h"
#include "paths.h"

#include <bas/locale/i18n.h>
#include <bas/log/deflog.h>
#include <bas/log/log.h>
#include <bas/proc/env.h>

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

define_logger();

enum { OPT_VERSION = 256 };

static void usage(FILE *out) {
    fputs(_("Usage: dwarfs-fm <command> [PATH]...\n"
            "File-manager helper for DwarFS images and archives.\n\n"
            "Commands:\n"
            "  open PATH              Open image in dwarfs-viewer\n"
            "  archive [PATH...]      Create archive (dwarfs-archiver)\n"
            "  unarchive PATH         Extract image (dwarfs-archiver)\n"
            "  check PATH             Integrity check in viewer\n"
            "  convert PATH           Convert other archive to DwarFS\n"
            "  mount PATH             FUSE-mount image at sibling dir\n"
            "  unmount PATH           Unmount sibling dir or mount dir\n"),
          out);
    fputs("\n", out);
    fputs("  -v, --verbose      ", out);
    fputs(_("repeat for more verbose loggings\n"), out);
    fputs("  -q, --quiet        ", out);
    fputs(_("show less logging messages\n"), out);
    fputs("  -h, --help         ", out);
    fputs(_("display this help and exit\n"), out);
    fputs("      --version      ", out);
    fputs(_("output version information and exit\n"), out);
}

static int spawn_detached(char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid == 0) {
        /* Detach from file manager. */
        if (setsid() < 0) {
            /* continue anyway */
        }
        execvp(argv[0], argv);
        _exit(127);
    }
    return 0;
}

static int run_wait(char *const argv[]) {
    pid_t pid = fork();
    int status;

    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid == 0) {
        execvp(argv[0], argv);
        _exit(127);
    }
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return -1;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}

static int cmd_open(int argc, char **argv) {
    char *av[8];
    int i = 0;

    if (argc < 1) {
        fprintf(stderr, "dwarfs-fm open: missing PATH\n");
        return 1;
    }
    av[i++] = "dwarfs-viewer";
    av[i++] = argv[0];
    av[i] = NULL;
    return spawn_detached(av) == 0 ? 0 : 1;
}

static int cmd_check(int argc, char **argv) {
    char *av[8];
    int i = 0;

    if (argc < 1) {
        fprintf(stderr, "dwarfs-fm check: missing PATH\n");
        return 1;
    }
    av[i++] = "dwarfs-viewer";
    av[i++] = "--tab";
    av[i++] = "integrity";
    av[i++] = argv[0];
    av[i] = NULL;
    return spawn_detached(av) == 0 ? 0 : 1;
}

static int cmd_archive(int argc, char **argv) {
    char **av;
    int i;

    av = calloc((size_t)argc + 3, sizeof(char *));
    if (!av) {
        return 1;
    }
    av[0] = "dwarfs-archiver";
    av[1] = "--mode";
    av[2] = "archive";
    for (i = 0; i < argc; i++) {
        av[3 + i] = argv[i];
    }
    i = spawn_detached(av) == 0 ? 0 : 1;
    free(av);
    return i;
}

static int cmd_unarchive(int argc, char **argv) {
    char *av[10];
    int i = 0;

    if (argc < 1) {
        fprintf(stderr, "dwarfs-fm unarchive: missing PATH\n");
        return 1;
    }
    av[i++] = "dwarfs-archiver";
    av[i++] = "--mode";
    av[i++] = "unarchive";
    av[i++] = argv[0];
    av[i] = NULL;
    return spawn_detached(av) == 0 ? 0 : 1;
}

static int cmd_convert(int argc, char **argv) {
    char *av[10];
    int i = 0;

    if (argc < 1) {
        fprintf(stderr, "dwarfs-fm convert: missing PATH\n");
        return 1;
    }
    av[i++] = "dwarfs-archiver";
    av[i++] = "--mode";
    av[i++] = "convert";
    av[i++] = argv[0];
    av[i] = NULL;
    return spawn_detached(av) == 0 ? 0 : 1;
}

static int ensure_dir(const char *path, int *created) {
    struct stat st;

    *created = 0;
    if (stat(path, &st) == 0) {
        if (!S_ISDIR(st.st_mode)) {
            fprintf(stderr, "dwarfs-fm: not a directory: %s\n", path);
            return -1;
        }
        return 0;
    }
    if (errno != ENOENT) {
        perror(path);
        return -1;
    }
    if (mkdir(path, 0755) != 0) {
        perror(path);
        return -1;
    }
    *created = 1;
    return 0;
}

static int cmd_mount(int argc, char **argv) {
    char mount[PATH_MAX];
    char *av[8];
    int created = 0;
    int i = 0;

    if (argc < 1) {
        fprintf(stderr, "dwarfs-fm mount: missing PATH\n");
        return 1;
    }
    if (!is_dwarfs_image_path(argv[0])) {
        fprintf(stderr, "dwarfs-fm mount: not a DwarFS image: %s\n", argv[0]);
        return 1;
    }
    if (sibling_mount_path(argv[0], mount, sizeof mount) != 0) {
        fprintf(stderr, "dwarfs-fm mount: cannot resolve mountpoint\n");
        return 1;
    }
    if (is_dwarfs_mount_dir(mount)) {
        log_info("already mounted: %s", mount);
        return 0;
    }
    if (ensure_dir(mount, &created) != 0) {
        return 1;
    }
    av[i++] = "dwarfs";
    av[i++] = argv[0];
    av[i++] = mount;
    av[i] = NULL;
    if (spawn_detached(av) != 0) {
        if (created) {
            rmdir(mount);
        }
        return 1;
    }
    log_info("mounting %s -> %s", argv[0], mount);
    return 0;
}

static int cmd_unmount(int argc, char **argv) {
    char mount[PATH_MAX];
    char *av[6];
    int rc;
    int i = 0;

    if (argc < 1) {
        fprintf(stderr, "dwarfs-fm unmount: missing PATH\n");
        return 1;
    }
    if (resolve_mount_dir(argv[0], mount, sizeof mount) != 0) {
        fprintf(stderr, "dwarfs-fm unmount: cannot resolve mount dir for %s\n", argv[0]);
        return 1;
    }
    if (!is_dwarfs_mount_dir(mount)) {
        /* Still try fusermount in case detection failed. */
        log_warn("not detected as dwarfs mount: %s", mount);
    }
    av[i++] = "fusermount3";
    av[i++] = "-u";
    av[i++] = mount;
    av[i] = NULL;
    rc = run_wait(av);
    if (rc != 0) {
        /* Fallback for older fuse. */
        av[0] = "fusermount";
        rc = run_wait(av);
    }
    if (rc == 0) {
        /* Remove empty sibling dir if vacant. */
        if (rmdir(mount) == 0) {
            log_info("removed empty mount dir %s", mount);
        }
    }
    return rc == 0 ? 0 : 1;
}

int main(int argc, char **argv) {
    const char *cmd;
    int opt_end = 1;

    init_i18n(LOCALEDIR);
    (void)self_exe();

    static const struct option long_opts[] = {
        {"verbose", no_argument, NULL, 'v'},
        {"quiet", no_argument, NULL, 'q'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, OPT_VERSION},
        {NULL, 0, NULL, 0},
    };

    for (;;) {
        int c = getopt_long(argc, argv, "+vqh", long_opts, NULL);
        if (c == -1) {
            break;
        }
        switch (c) {
        case 'v':
            log_more();
            break;
        case 'q':
            log_less();
            break;
        case 'h':
            usage(stdout);
            return 0;
        case OPT_VERSION:
            printf("dwarfs-fm %s\n", PROJECT_VERSION);
            printf(_("Copyright (C) %d %s\n"), PROJECT_YEAR, PROJECT_AUTHOR);
            fputs(_("License AGPL-3.0-or-later: <https://www.gnu.org/licenses/agpl-3.0.html>\n"),
                  stdout);
            return 0;
        default:
            usage(stderr);
            return 1;
        }
    }

    opt_end = optind;
    if (opt_end >= argc) {
        usage(stderr);
        return 1;
    }
    cmd = argv[opt_end];
    argc -= opt_end + 1;
    argv += opt_end + 1;

    if (strcmp(cmd, "open") == 0) {
        return cmd_open(argc, argv);
    }
    if (strcmp(cmd, "archive") == 0) {
        return cmd_archive(argc, argv);
    }
    if (strcmp(cmd, "unarchive") == 0) {
        return cmd_unarchive(argc, argv);
    }
    if (strcmp(cmd, "check") == 0) {
        return cmd_check(argc, argv);
    }
    if (strcmp(cmd, "convert") == 0) {
        return cmd_convert(argc, argv);
    }
    if (strcmp(cmd, "mount") == 0) {
        return cmd_mount(argc, argv);
    }
    if (strcmp(cmd, "unmount") == 0) {
        return cmd_unmount(argc, argv);
    }

    fprintf(stderr, "dwarfs-fm: unknown command: %s\n", cmd);
    usage(stderr);
    return 1;
}
