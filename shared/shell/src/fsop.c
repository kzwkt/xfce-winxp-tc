#include <glib.h>
#include <wintc/comctl.h>
#include <wintc/comgtk.h>
#include <wintc/shlang.h>

#include "../public/fsop.h"

#define K_TIMEOUT_SHOW_UI 1

//
// PRIVATE ENUMS
//
enum
{
    PROP_LIST_FILES = 1,
    PROP_DESTINATION,
    PROP_OPERATION
};

//
// FORWARD DECLARATIONS
//
static void wintc_sh_fs_operation_constructed(
    GObject* object
);
static void wintc_sh_fs_operation_dispose(
    GObject* object
);
static void wintc_sh_fs_operation_finalize(
    GObject* object
);
static void wintc_sh_fs_operation_get_property(
    GObject*    object,
    guint       prop_id,
    GValue*     value,
    GParamSpec* pspec
);
static void wintc_sh_fs_operation_set_property(
    GObject*      object,
    guint         prop_id,
    const GValue* value,
    GParamSpec*   pspec
);

static void wintc_sh_fs_operation_step(
    WinTCShFSOperation* fs_operation
);

static GFile* get_g_file_for_copymove(
    const gchar* src_path,
    const gchar* dest_path
);
static GFile* get_g_file_for_target(
    const gchar* target
);

static void cb_async_file_op(
    GObject*      source_object,
    GAsyncResult* res,
    gpointer      data
);
static gboolean cb_timeout_show_ui(
    gpointer user_data
);

//
// GTK OOP CLASS/INSTANCE DEFINITIONS
//
typedef struct _WinTCShFSOperation
{
    GObject __parent__;

    // Operation details
    //
    gchar*                 dest;
    GFile*                 dest_file;
    gboolean               done;
    guint                  id_timeout_show_ui;
    GList*                 iter_op;
    GList*                 list_files;
    WinTCShFSOperationKind operation_kind;

    // UI
    //
    GtkWidget* wnd_progress;
    GtkWidget* label_eta;
    GtkWidget* label_filename;
    GtkWidget* label_locations;
    GtkWidget* prg_copymove;

    // Track the current top most window, either the owner window (eg.
    // explorer) or the progress window, if it's visible
    //
    GtkWidget* wnd_topmost;
} WinTCShFSOperation;

//
// GTK TYPE DEFINITIONS & CTORS
//
G_DEFINE_TYPE(
    WinTCShFSOperation,
    wintc_sh_fs_operation,
    G_TYPE_OBJECT
)

static void wintc_sh_fs_operation_class_init(
    WinTCShFSOperationClass* klass
)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->constructed  = wintc_sh_fs_operation_constructed;
    object_class->dispose      = wintc_sh_fs_operation_dispose;
    object_class->finalize     = wintc_sh_fs_operation_finalize;
    object_class->get_property = wintc_sh_fs_operation_get_property;
    object_class->set_property = wintc_sh_fs_operation_set_property;

    g_object_class_install_property(
        object_class,
        PROP_LIST_FILES,
        g_param_spec_pointer(
            "list-files",
            "ListFiles",
            "The list of files to operate on.",
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY
        )
    );
    g_object_class_install_property(
        object_class,
        PROP_DESTINATION,
        g_param_spec_string(
            "destination",
            "Destination",
            "The destination for the files.",
            NULL,
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY
        )
    );
    g_object_class_install_property(
        object_class,
        PROP_OPERATION,
        g_param_spec_int(
            "operation",
            "Operation",
            "The operation to perform.",
            WINTC_SH_FS_OPERATION_INVALID,
            WINTC_SH_FS_OPERATION_TRASH,
            WINTC_SH_FS_OPERATION_INVALID,
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY
        )
    );
}

static void wintc_sh_fs_operation_init(
    WINTC_UNUSED(WinTCShFSOperation* self)
) {}

//
// CLASS VIRTUAL METHODS
//
static void wintc_sh_fs_operation_constructed(
    GObject* object
)
{
    WinTCShFSOperation* fs_operation = WINTC_SH_FS_OPERATION(object);

    // Check things are set
    //
    if (!(fs_operation->list_files) || !(fs_operation->dest))
    {
        fs_operation->operation_kind = WINTC_SH_FS_OPERATION_INVALID;
    }

    // Copying to trash is invalid
    //
    // FIXME: scheme check probs needs to be case insensitive?
    //
    if (
        fs_operation->operation_kind == WINTC_SH_FS_OPERATION_COPY &&
        g_str_has_prefix(fs_operation->dest, "trash://")
    )
    {
        fs_operation->operation_kind = WINTC_SH_FS_OPERATION_INVALID;
    }

    // Moving to trash is just trashing
    //
    // FIXME: Ditto
    //
    if (
        fs_operation->operation_kind == WINTC_SH_FS_OPERATION_MOVE &&
        g_str_has_prefix(fs_operation->dest, "trash://")
    )
    {
        fs_operation->operation_kind = WINTC_SH_FS_OPERATION_TRASH;
    }
}

static void wintc_sh_fs_operation_dispose(
    GObject* object
)
{
    WinTCShFSOperation* fs_operation = WINTC_SH_FS_OPERATION(object);

    g_clear_object(&(fs_operation->dest_file));
    g_clear_object(&(fs_operation->wnd_progress));

    (G_OBJECT_CLASS(wintc_sh_fs_operation_parent_class))->dispose(object);
}

static void wintc_sh_fs_operation_finalize(
    GObject* object
)
{
    WinTCShFSOperation* fs_operation = WINTC_SH_FS_OPERATION(object);

    g_free(fs_operation->dest);
    g_list_free_full(fs_operation->list_files, (GDestroyNotify) g_free);

    (G_OBJECT_CLASS(wintc_sh_fs_operation_parent_class))->finalize(object);
}

static void wintc_sh_fs_operation_get_property(
    GObject*    object,
    guint       prop_id,
    GValue*     value,
    GParamSpec* pspec
)
{
    WinTCShFSOperation* fs_operation = WINTC_SH_FS_OPERATION(object);

    switch (prop_id)
    {
        case PROP_LIST_FILES:
            g_value_set_pointer(value, fs_operation->list_files);
            break;

        case PROP_DESTINATION:
            g_value_set_string(value, fs_operation->dest);
            break;

        case PROP_OPERATION:
            g_value_set_int(value, fs_operation->operation_kind);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void wintc_sh_fs_operation_set_property(
    GObject*      object,
    guint         prop_id,
    const GValue* value,
    GParamSpec*   pspec
)
{
    WinTCShFSOperation* fs_operation = WINTC_SH_FS_OPERATION(object);

    switch (prop_id)
    {
        case PROP_LIST_FILES:
            fs_operation->list_files = g_value_get_pointer(value);
            break;

        case PROP_DESTINATION:
            fs_operation->dest      = g_value_dup_string(value);
            fs_operation->dest_file = get_g_file_for_target(
                                          fs_operation->dest
                                      );
            break;

        case PROP_OPERATION:
            fs_operation->operation_kind = g_value_get_int(value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

//
// PUBLIC FUNCTIONS
//
WinTCShFSOperation* wintc_sh_fs_operation_new(
    GList*                 files,
    const gchar*           dest,
    WinTCShFSOperationKind operation_kind
)
{
    return WINTC_SH_FS_OPERATION(
        g_object_new(
            WINTC_TYPE_SH_FS_OPERATION,
            "list-files",  files,
            "destination", dest,
            "operation",   operation_kind,
            NULL
        )
    );
}

void wintc_sh_fs_operation_do(
    WinTCShFSOperation* fs_operation,
    GtkWindow*          wnd
)
{
    if (fs_operation->operation_kind == WINTC_SH_FS_OPERATION_INVALID)
    {
        g_critical("%s", "shell: fs op: invalid operation");
        return;
    }

    if (fs_operation->iter_op)
    {
        g_critical("%s", "shell fs op: operation already in progress");
        return;
    }

    if (fs_operation->done)
    {
        g_critical("%s", "shell: fs op: operation already complete");
        return;
    }

    // Spawn timeout for UI
    //
    fs_operation->id_timeout_show_ui =
        g_timeout_add_seconds(
            K_TIMEOUT_SHOW_UI,
            (GSourceFunc) cb_timeout_show_ui,
            fs_operation
        );

    // Set up the operation for stepping
    //
    fs_operation->iter_op     = fs_operation->list_files;
    fs_operation->wnd_topmost = GTK_WIDGET(wnd);

    wintc_sh_fs_operation_step(fs_operation);
}

//
// PRIVATE FUNCTIONS
//
static void wintc_sh_fs_operation_step(
    WinTCShFSOperation* fs_operation
)
{
    // Spawn the file operation
    //
    GFile*       dest_file = fs_operation->dest_file;
    const gchar* src_path  = (gchar*) fs_operation->iter_op->data;
    GFile*       src_file  = get_g_file_for_target(src_path);

    g_object_ref(dest_file);

    if (
        (
            fs_operation->operation_kind == WINTC_SH_FS_OPERATION_COPY ||
            fs_operation->operation_kind == WINTC_SH_FS_OPERATION_MOVE
        ) &&
        g_file_test(
            fs_operation->dest,
            G_FILE_TEST_IS_DIR
        )
    )
    {
        g_object_unref(dest_file);

        dest_file =
            get_g_file_for_copymove(
                src_path,
                fs_operation->dest
            );
    }

    switch (fs_operation->operation_kind)
    {
        case WINTC_SH_FS_OPERATION_COPY:
            WINTC_LOG_DEBUG(
                "shell: fsop - copy %s to %s",
                (gchar*) fs_operation->iter_op->data,
                fs_operation->dest
            );

            g_file_copy_async(
                src_file,
                dest_file,
                G_FILE_COPY_NONE,
                G_PRIORITY_DEFAULT,
                NULL, // FIXME: Cancellable, should use this!
                NULL, // FIXME: Progress callback, should use this too!
                NULL,
                (GAsyncReadyCallback) cb_async_file_op,
                fs_operation
            );

            break;

        case WINTC_SH_FS_OPERATION_MOVE:
            WINTC_LOG_DEBUG(
                "shell: fsop - move %s to %s",
                (gchar*) fs_operation->iter_op->data,
                fs_operation->dest
            );

            g_file_move_async(
                src_file,
                dest_file,
                G_FILE_COPY_NONE,
                G_PRIORITY_DEFAULT,
                NULL, // FIXME: Cancellable, should use this!
                NULL, // FIXME: Progress callback, should use this too!
                NULL,
                (GAsyncReadyCallback) cb_async_file_op,
                fs_operation
            );

            break;

        case WINTC_SH_FS_OPERATION_TRASH:
            WINTC_LOG_DEBUG(
                "shell: fsop - trash %s",
                (gchar*) fs_operation->iter_op->data
            );

            g_file_trash_async(
                src_file,
                G_PRIORITY_DEFAULT,
                NULL, // FIXME: Cancellable, should use this!
                (GAsyncReadyCallback) cb_async_file_op,
                fs_operation
            );

            break;

        default:
            g_critical("%s", "shell: fs op - cannot step, unknown op");
            break;
    }

    g_object_unref(dest_file);
}

static GFile* get_g_file_for_copymove(
    const gchar* src_path,
    const gchar* dest_path
)
{
    GFile* ret;

    gchar* filename = g_path_get_basename(src_path);
    gchar* new_dest = g_build_path(
                          G_DIR_SEPARATOR_S,
                          dest_path,
                          filename,
                          NULL
                      );

    ret = get_g_file_for_target(new_dest);

    g_free(filename);
    g_free(new_dest);

    return ret;
}

static GFile* get_g_file_for_target(
    const gchar* target
)
{
    // If the string starts with / it's a local path, otherwise treat it like
    // a URI
    //
    if (strchr(target, G_DIR_SEPARATOR) == target)
    {
        return g_file_new_for_path(target);
    }
    else
    {
        return g_file_new_for_uri(target);
    }
}

//
// CALLBACKS
//
static void cb_async_file_op(
    GObject*      source_object,
    GAsyncResult* res,
    gpointer      data
)
{
    GFile*              file         = (GFile*) source_object;
    WinTCShFSOperation* fs_operation = WINTC_SH_FS_OPERATION(data);

    // Finish up this op
    //
    GError*  error = NULL;
    gboolean success;

    switch (fs_operation->operation_kind)
    {
        case WINTC_SH_FS_OPERATION_COPY:
            success = g_file_copy_finish(file, res, &error);
            break;

        case WINTC_SH_FS_OPERATION_MOVE:
            success = g_file_move_finish(file, res, &error);
            break;

        case WINTC_SH_FS_OPERATION_TRASH:
            success = g_file_trash_finish(file, res, &error);
            break;

        default:
            g_critical("%s", "shell: fs op - impossible finish situation?");
            break;
    }

    if (!success)
    {
        // FIXME: Handle properly, depending on what the error is it could be
        //        possible to request user action for things like:
        //
        //        - overwrite file when moving/copying?
        //        - trashing impossible on file system, delete instead?
        //        - merge directories?
        //
        //        etc. - see copy/move/trash GLib documentation for what needs
        //        to be handled
        //
        wintc_display_error_and_clear(&error);
    }

    // Proceed to next step
    //
    fs_operation->iter_op = fs_operation->iter_op->next;

    if (!(fs_operation->iter_op))
    {
        fs_operation->done = TRUE;

        if (fs_operation->id_timeout_show_ui)
        {
            g_source_remove(fs_operation->id_timeout_show_ui);
            fs_operation->id_timeout_show_ui = 0;
        }

        if (fs_operation->wnd_progress)
        {
            gtk_widget_hide(fs_operation->wnd_progress);
        }

        return;
    }

    wintc_sh_fs_operation_step(fs_operation);
}

static gboolean cb_timeout_show_ui(
    gpointer user_data
)
{
    WinTCShFSOperation* fs_operation = WINTC_SH_FS_OPERATION(user_data);

    GtkBuilder* builder =
        gtk_builder_new_from_resource(
            "/uk/oddmatics/wintc/shell/dlgfsop.ui"
        );

    wintc_ctl_install_default_styles();
    wintc_lc_builder_preprocess_widget_text(builder);

    wintc_builder_get_objects(
        builder,
        "main-wnd",        &(fs_operation->wnd_progress),
        "label-eta",       &(fs_operation->label_eta),
        "label-filename",  &(fs_operation->label_filename),
        "label-locations", &(fs_operation->label_locations),
        "prg-copymove",    &(fs_operation->prg_copymove),
        NULL
    );

    g_object_ref(fs_operation->wnd_progress);

    g_object_unref(builder);

    // Set up window
    //
    GApplication* app = g_application_get_default();

    if (app)
    {
        gtk_window_set_application(
            GTK_WINDOW(fs_operation->wnd_progress),
            GTK_APPLICATION(app)
        );
    }

    gtk_window_set_modal(
        GTK_WINDOW(fs_operation->wnd_progress),
        TRUE
    );
    gtk_window_set_transient_for(
        GTK_WINDOW(fs_operation->wnd_progress),
        GTK_WINDOW(fs_operation->wnd_topmost)
    );

    gtk_widget_show_all(
        fs_operation->wnd_progress
    );

    fs_operation->id_timeout_show_ui = 0;
    fs_operation->wnd_topmost        = fs_operation->wnd_progress;

    return G_SOURCE_REMOVE;
}
