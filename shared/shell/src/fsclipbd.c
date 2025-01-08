#include <gdk/gdk.h>
#include <glib.h>
#include <wintc/comgtk.h>

#include "../public/error.h"
#include "../public/fsclipbd.h"

//
// PRIVATE ENUMS
//
enum
{
    PROP_CAN_PASTE = 1
};

//
// FORWARD DECLARATIONS
//
static void wintc_sh_fs_clipboard_get_property(
    GObject*    object,
    guint       prop_id,
    GValue*     value,
    GParamSpec* pspec
);

static void wintc_sh_fs_clipboard_update_state(
    WinTCShFSClipboard* fs_clipboard
);

static void on_clipboard_owner_change(
    GtkClipboard*        self,
    GdkEventOwnerChange* event,
    gpointer             user_data
);

//
// STATIC DATA
//
static GdkAtom S_ATOM_TEXT_URI_LIST;
static GdkAtom S_ATOM_X_SPECIAL_GNOME_COPIED_FILES;

//
// GTK OOP CLASS/INSTANCE DEFINITIONS
//
typedef struct _WinTCShFSClipboard
{
    GObject __parent__;

    GtkClipboard* clipboard;
    GdkAtom       preferred_target;
} WinTCShFSClipboard;

//
// GTK TYPE DEFINITIONS & CTORS
//
G_DEFINE_TYPE(
    WinTCShFSClipboard,
    wintc_sh_fs_clipboard,
    G_TYPE_OBJECT
)

static void wintc_sh_fs_clipboard_class_init(
    WinTCShFSClipboardClass* klass
)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = wintc_sh_fs_clipboard_get_property;

    g_object_class_install_property(
        object_class,
        PROP_CAN_PASTE,
        g_param_spec_boolean(
            "can-paste",
            "CanPaste",
            "Determines whether the clipboard has pastable content.",
            FALSE,
            G_PARAM_READABLE
        )
    );

    S_ATOM_TEXT_URI_LIST =
        gdk_atom_intern_static_string("text/uri-list");
    S_ATOM_X_SPECIAL_GNOME_COPIED_FILES =
        gdk_atom_intern_static_string("x-special/gnome-copied-files");
}

static void wintc_sh_fs_clipboard_init(
    WinTCShFSClipboard* self
)
{
    WINTC_LOG_DEBUG("Creating clipboard.");

    self->clipboard =
        gtk_clipboard_get_default(gdk_display_get_default());

    wintc_sh_fs_clipboard_update_state(self);

    g_signal_connect(
        self->clipboard,
        "owner-change",
        G_CALLBACK(on_clipboard_owner_change),
        self
    );
}

//
// CLASS VIRTUAL METHODS
//
static void wintc_sh_fs_clipboard_get_property(
    GObject*    object,
    guint       prop_id,
    GValue*     value,
    GParamSpec* pspec
)
{
    WinTCShFSClipboard* fs_clipboard = WINTC_SH_FS_CLIPBOARD(object);

    switch (prop_id)
    {
        case PROP_CAN_PASTE:
            g_value_set_boolean(
                value,
                fs_clipboard->preferred_target != GDK_NONE
            );
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

//
// PUBLIC FUNCTIONS
//
WinTCShFSClipboard* wintc_sh_fs_clipboard_new(void)
{
    // We are using a singleton instance here, but as far as programs are
    // concerned it's an ordinary object they own a reference to
    //
    // This gives some flexibility if any changes are needed in future, but
    // since the clipboard is a shared resource it makes sense to only hold
    // one instance
    //
    static WinTCShFSClipboard* singleton_clipboard = NULL;

    if (!singleton_clipboard)
    {
        singleton_clipboard =
            WINTC_SH_FS_CLIPBOARD(
                g_object_new(
                    WINTC_TYPE_SH_FS_CLIPBOARD,
                    NULL
                )
            );
    }

    g_object_ref(singleton_clipboard);

    return singleton_clipboard;
}

gboolean wintc_sh_fs_clipboard_paste(
    WinTCShFSClipboard* fs_clipboard,
    WINTC_UNUSED(const gchar* dest),
    GError**            error
)
{
    if (fs_clipboard->preferred_target == GDK_NONE)
    {
        g_set_error(
            error,
            wintc_shell_error_quark(),
            WINTC_SHELL_ERROR_CLIPBOARD_EMPTY,
            "%s",
            "There are no files on the clipboard." // FIXME: Localise
        );

        return FALSE;
    }

    // Retrieve the contents on the clipboard
    //
    GtkSelectionData* selection_data =
        gtk_clipboard_wait_for_contents(
            fs_clipboard->clipboard,
            fs_clipboard->preferred_target
        );

    if (!selection_data)
    {
        g_set_error(
            error,
            wintc_shell_error_quark(),
            WINTC_SHELL_ERROR_CLIPBOARD_GET_FAILED,
            "%s",
            "Failed to retrieve data on the clipboard." // FIXME: Localise
        );

        return FALSE;
    }

    // Parse the contents
    //
    const guchar* data;
    const guchar* data_end;
    gint          len         = 0;
    gboolean      should_copy = TRUE;
    GList*        uris        = NULL;

    data =
        gtk_selection_data_get_data_with_length(
            selection_data,
            &len
        );

    data_end = data + len;

    if (fs_clipboard->preferred_target == S_ATOM_X_SPECIAL_GNOME_COPIED_FILES)
    {
        if (g_ascii_strncasecmp((const gchar*) data, "copy\n", 5) == 0)
        {
            should_copy = TRUE;
            data += 5;
        }
        else if (g_ascii_strncasecmp((const gchar*) data, "cut\n", 4) == 0)
        {
            should_copy = FALSE;
            data += 4;
        }
    }

    while (data < data_end)
    {
        // Search for next \n
        //
        const guchar* next_lf = memchr(data, '\n', data_end - data);

        if (!next_lf) // gnome-copied-files has no final \n
        {
            next_lf =  data_end;
        }

        // Work out how much we need to slice
        //
        gint copy_len = next_lf - data;

        if (fs_clipboard->preferred_target == S_ATOM_TEXT_URI_LIST)
        {
            copy_len--; // text/uri-list uses \r\n rather than just \n
        }

        // Slice and store the URI in our list
        //
        gchar* buf = g_malloc(copy_len);

        memcpy(buf, data, copy_len);

        uris = g_list_append(uris, buf);

        // Iter
        //
        data = next_lf + 1;
    }

    // Attempt to paste these files
    //
    for (GList* iter = uris; iter; iter = iter->next)
    {
        if (should_copy)
        {
            WINTC_LOG_DEBUG("Would copy the following...");
        }
        else
        {
            WINTC_LOG_DEBUG("Would move the following...");
        }

        WINTC_LOG_DEBUG(iter->data);
    }

    g_list_free_full(uris, (GDestroyNotify) g_free);
    gtk_selection_data_free(selection_data);

    return TRUE;
}

//
// PRIVATE FUNCTIONS
//
static void wintc_sh_fs_clipboard_update_state(
    WinTCShFSClipboard* fs_clipboard
)
{
    fs_clipboard->preferred_target = GDK_NONE;

    if (
        gtk_clipboard_wait_is_target_available(
            fs_clipboard->clipboard,
            S_ATOM_X_SPECIAL_GNOME_COPIED_FILES
        )
    )
    {
        fs_clipboard->preferred_target = S_ATOM_X_SPECIAL_GNOME_COPIED_FILES;

        WINTC_LOG_DEBUG(
            "shell: fsclipbd - has data: x-special/gnome-copied-files"
        );
    }
    else if (
        gtk_clipboard_wait_is_target_available(
            fs_clipboard->clipboard,
            S_ATOM_TEXT_URI_LIST
        )
    )
    {
        fs_clipboard->preferred_target = S_ATOM_TEXT_URI_LIST;

        WINTC_LOG_DEBUG(
            "shell: fsclipbd - has data: text/uri-list"
        );
    }
    else
    {
        WINTC_LOG_DEBUG(
            "shell: fsclipbd - has data: no"
        );
    }

    g_object_notify(
        G_OBJECT(fs_clipboard),
        "can-paste"
    );
}

//
// CALLBACKS
//
static void on_clipboard_owner_change(
    WINTC_UNUSED(GtkClipboard* self),
    WINTC_UNUSED(GdkEventOwnerChange* event),
    gpointer user_data
)
{
    WINTC_LOG_DEBUG("shell: fsclipbd - owner changed");

    wintc_sh_fs_clipboard_update_state(
        WINTC_SH_FS_CLIPBOARD(user_data)
    );
}
