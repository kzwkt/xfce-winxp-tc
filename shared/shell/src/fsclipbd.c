#include <gdk/gdk.h>
#include <glib.h>
#include <wintc/comgtk.h>

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
// GTK OOP CLASS/INSTANCE DEFINITIONS
//
typedef struct _WinTCShFSClipboard
{
    GObject __parent__;

    GtkClipboard* clipboard;
    gboolean      has_content;
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
            g_value_set_boolean(value, fs_clipboard->has_content);
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

//
// PRIVATE FUNCTIONS
//
static void wintc_sh_fs_clipboard_update_state(
    WinTCShFSClipboard* fs_clipboard
)
{
    fs_clipboard->has_content =
        gtk_clipboard_wait_is_uris_available(fs_clipboard->clipboard);

    g_object_notify(
        G_OBJECT(fs_clipboard),
        "can-paste"
    );

    WINTC_LOG_DEBUG(
        "shell: fsclipbd - has data: %d",
        fs_clipboard->has_content
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
