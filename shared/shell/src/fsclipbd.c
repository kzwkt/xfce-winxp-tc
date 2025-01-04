#include <gdk/gdk.h>
#include <glib.h>
#include <wintc/comgtk.h>

#include "../public/fsclipbd.h"

//
// FORWARD DECLARATIONS
//
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
    WINTC_UNUSED(WinTCShFSClipboardClass* klass)
) {}

static void wintc_sh_fs_clipboard_init(
    WinTCShFSClipboard* self
)
{
    WINTC_LOG_DEBUG("Creating clipboard.");

    self->clipboard =
        gtk_clipboard_get_default(gdk_display_get_default());

    g_signal_connect(
        self->clipboard,
        "owner-change",
        G_CALLBACK(on_clipboard_owner_change),
        self
    );
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
// CALLBACKS
//
static void on_clipboard_owner_change(
    WINTC_UNUSED(GtkClipboard*        self),
    WINTC_UNUSED(GdkEventOwnerChange* event),
    WINTC_UNUSED(gpointer             user_data)
)
{
    WINTC_LOG_DEBUG("Clipboard owner changed.");
}
