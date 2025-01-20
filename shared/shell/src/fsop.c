#include <glib.h>
#include <wintc/comgtk.h>

#include "../public/fsop.h"

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

//
// GTK OOP CLASS/INSTANCE DEFINITIONS
//
typedef struct _WinTCShFSOperation
{
    GObject __parent__;

    // Operation details
    //
    gchar*                 dest;
    GList*                 list_files;
    WinTCShFSOperationKind operation_kind;
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
            fs_operation->dest = g_value_dup_string(value);
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
    WINTC_UNUSED(GtkWindow* wnd)
)
{
    if (fs_operation->operation_kind == WINTC_SH_FS_OPERATION_INVALID)
    {
        // TODO: Display error
        g_critical("%s", "Invalid operation.");
        return;
    }

    // Iterate over files to perform the operation
    //
    for (GList* iter = fs_operation->list_files; iter; iter = iter->next)
    {
        switch (fs_operation->operation_kind)
        {
            case WINTC_SH_FS_OPERATION_COPY:
                g_message(
                    "shell: fsop - would copy %s to %s",
                    (gchar*) iter->data,
                    fs_operation->dest
                );
                break;

            case WINTC_SH_FS_OPERATION_MOVE:
                g_message(
                    "shell: fsop - would move %s to %s",
                    (gchar*) iter->data,
                    fs_operation->dest
                );
                break;

            case WINTC_SH_FS_OPERATION_TRASH:
                g_message(
                    "shell: fsop - would trash %s",
                    (gchar*) iter->data
                );
                break;

            default:
                g_critical("%s", "shell: fsop - invalid operation!");
                break;
        }
    }
}
