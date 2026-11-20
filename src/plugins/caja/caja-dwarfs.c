/*
 * Copyright (C) 2026 Lenik <dwarfs-utils@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "config.h"

#ifndef GETTEXT_PACKAGE
#define GETTEXT_PACKAGE TEXT_DOMAIN
#endif

#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <libcaja-extension/caja-extension-types.h>
#include <libcaja-extension/caja-file-info.h>
#include <libcaja-extension/caja-menu-provider.h>
#include <string.h>

#define DWARFS_FM "dwarfs-fm"

typedef struct {
    GObject parent;
} CajaDwarfs;

typedef struct {
    GObjectClass parent;
} CajaDwarfsClass;

static GType dwarfs_type = 0;

static gboolean path_has_suffix_ci(const char *name, const char *suf) {
    size_t n, s;
    size_t i;
    if (!name || !suf) {
        return FALSE;
    }
    n = strlen(name);
    s = strlen(suf);
    if (s > n) {
        return FALSE;
    }
    for (i = 0; i < s; i++) {
        if (g_ascii_tolower(name[n - s + i]) != g_ascii_tolower(suf[i])) {
            return FALSE;
        }
    }
    return TRUE;
}

static gboolean is_dwarfs_image_name(const char *name) {
    return path_has_suffix_ci(name, ".dwarfs") || path_has_suffix_ci(name, ".dwfs") ||
           path_has_suffix_ci(name, ".dw");
}

static gboolean is_convertible_name(const char *name) {
    static const char *const sufs[] = {".zip",  ".rar", ".7z",   ".tar", ".tgz", ".txz",
                                       ".tbz2", ".sqfs", ".sqxz", ".squashfs", ".iso", NULL};
    int i;
    if (is_dwarfs_image_name(name)) {
        return FALSE;
    }
    for (i = 0; sufs[i]; i++) {
        if (path_has_suffix_ci(name, sufs[i])) {
            return TRUE;
        }
    }
    return FALSE;
}

static char *file_local_path(CajaFileInfo *file) {
    char *uri = caja_file_info_get_uri(file);
    GFile *gf;
    char *path;

    if (!uri) {
        return NULL;
    }
    gf = g_file_new_for_uri(uri);
    path = g_file_get_path(gf);
    g_object_unref(gf);
    g_free(uri);
    return path;
}

static void spawn_fm(const char *cmd, GList *files) {
    GPtrArray *argv;
    GList *l;
    GError *err = NULL;

    argv = g_ptr_array_new();
    g_ptr_array_add(argv, (gpointer)DWARFS_FM);
    g_ptr_array_add(argv, (gpointer)cmd);
    for (l = files; l; l = l->next) {
        char *path = file_local_path(CAJA_FILE_INFO(l->data));
        if (path) {
            g_ptr_array_add(argv, path);
        }
    }
    g_ptr_array_add(argv, NULL);
    if (!g_spawn_async(NULL, (char **)argv->pdata, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL,
                       &err)) {
        g_warning("dwarfs-fm spawn failed: %s", err->message);
        g_error_free(err);
    }
    /* Free only the path strings we allocated (index >= 2). */
    {
        guint i;
        for (i = 2; i + 1 < argv->len; i++) {
            g_free(argv->pdata[i]);
        }
    }
    g_ptr_array_free(argv, TRUE);
}

static void free_str_notify(gpointer data, GClosure *closure) {
    (void)closure;
    g_free(data);
}

static void on_activate(CajaMenuItem *item, gpointer user_data) {
    const char *cmd = user_data;
    GList *files = g_object_get_data(G_OBJECT(item), "files");
    spawn_fm(cmd, files);
}

static CajaMenuItem *make_item(const char *name, const char *label, const char *tip,
                               const char *cmd, GList *files) {
    CajaMenuItem *item = caja_menu_item_new(name, label, tip, "package-x-generic");
    g_signal_connect_data(item, "activate", G_CALLBACK(on_activate), g_strdup(cmd), free_str_notify,
                          0);
    g_object_set_data_full(G_OBJECT(item), "files", caja_file_info_list_copy(files),
                           (GDestroyNotify)caja_file_info_list_free);
    return item;
}

static GList *caja_dwarfs_get_file_items(CajaMenuProvider *provider, GtkWidget *window,
                                         GList *files) {
    GList *items = NULL;
    CajaMenuItem *root;
    CajaMenu *submenu;
    gboolean any_dwarfs = FALSE;
    gboolean any_dir = FALSE;
    gboolean any_convertible = FALSE;
    gboolean any_local = FALSE;
    GList *l;

    (void)provider;
    (void)window;

    if (!files) {
        return NULL;
    }

    for (l = files; l; l = l->next) {
        CajaFileInfo *fi = l->data;
        char *name;
        char *scheme;

        scheme = caja_file_info_get_uri_scheme(fi);
        if (!scheme || strcmp(scheme, "file") != 0) {
            g_free(scheme);
            continue;
        }
        g_free(scheme);
        any_local = TRUE;
        name = caja_file_info_get_name(fi);
        if (caja_file_info_is_directory(fi)) {
            any_dir = TRUE;
        } else if (name && is_dwarfs_image_name(name)) {
            any_dwarfs = TRUE;
        } else if (name && is_convertible_name(name)) {
            any_convertible = TRUE;
        }
        g_free(name);
    }

    if (!any_local) {
        return NULL;
    }

    root = caja_menu_item_new("CajaDwarfs::root", _("DwarFS"), _("DwarFS tools"),
                              "package-x-generic");
    submenu = caja_menu_new();

    if (any_dwarfs) {
        caja_menu_append_item(submenu, make_item("CajaDwarfs::open", _("Open"),
                                                 _("Open in DwarFS Viewer"), "open", files));
        caja_menu_append_item(submenu,
                              make_item("CajaDwarfs::unarchive", _("Unarchive"),
                                        _("Extract DwarFS image"), "unarchive", files));
        caja_menu_append_item(submenu,
                              make_item("CajaDwarfs::check", _("Check integrity"),
                                        _("Verify image integrity"), "check", files));
        caja_menu_append_item(submenu, make_item("CajaDwarfs::mount", _("Mount"),
                                                 _("FUSE-mount at sibling directory"), "mount",
                                                 files));
    }

    if (any_dwarfs || any_dir) {
        caja_menu_append_item(submenu,
                              make_item("CajaDwarfs::unmount", _("Unmount"),
                                        _("Unmount sibling/mount directory"), "unmount", files));
    }

    if (any_dir || (!any_dwarfs && !any_convertible)) {
        caja_menu_append_item(submenu, make_item("CajaDwarfs::archive", _("Archive"),
                                                 _("Create DwarFS image"), "archive", files));
    }

    if (any_convertible) {
        caja_menu_append_item(submenu,
                              make_item("CajaDwarfs::convert", _("Convert to DwarFS"),
                                        _("Convert archive to DwarFS"), "convert", files));
    }

    caja_menu_item_set_submenu(root, submenu);
    items = g_list_append(items, root);
    return items;
}

static void menu_provider_iface_init(CajaMenuProviderIface *iface) {
    iface->get_file_items = caja_dwarfs_get_file_items;
}

static void caja_dwarfs_register_type(GTypeModule *module) {
    static const GTypeInfo info = {
        sizeof(CajaDwarfsClass),
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        sizeof(CajaDwarfs),
        0,
        NULL,
    };
    static const GInterfaceInfo menu_info = {(GInterfaceInitFunc)menu_provider_iface_init, NULL,
                                             NULL};

    dwarfs_type = g_type_module_register_type(module, G_TYPE_OBJECT, "CajaDwarfs", &info, 0);
    g_type_module_add_interface(module, dwarfs_type, CAJA_TYPE_MENU_PROVIDER, &menu_info);
}

static GType type_list[1];

void caja_module_initialize(GTypeModule *module) {
    bindtextdomain(TEXT_DOMAIN, LOCALEDIR);
    bind_textdomain_codeset(TEXT_DOMAIN, "UTF-8");
    caja_dwarfs_register_type(module);
    type_list[0] = dwarfs_type;
}

void caja_module_shutdown(void) {}

void caja_module_list_types(const GType **types, int *num_types) {
    *types = type_list;
    *num_types = G_N_ELEMENTS(type_list);
}
