/*
 *  Copyright (C) 2015-2019 Gooroom <gooroom@gooroom.kr>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <glib-object.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/xfce-panel-plugin.h>
#include <libnotify/notify.h>

#include <json-c/json.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "gooroom-notice-plugin.h"

#define DEFAULT_USER_IMAGE_SIZE (32)
#define PANEL_TRAY_ICON_SIZE    (22)
#define NOTIFICATION_LIMIT      (5)
#define NOTIFICATION_TEXT_LIMIT (17)
#define NOTIFICATION_SIGNAL     "set_noti"
#define NOTIFICATION_TIMEOUT    (5000)
#define NOTIFICATION_MSG_ICON   "notice-plugin-msg"
#define NOTIFICATION_MSG_URGENCY_ICON   "notice-plugin-msg-urgency"
#define DEFAULT_TRAY_ICON       "notice-plugin-panel"
#define DEFAULT_NOTICE_TRAY_ICON "notice-plugin-event-panel"

struct _GooroomNoticePluginClass
{
  XfcePanelPluginClass __parent__;
};

/* plugin structure */
struct _GooroomNoticePlugin
{
    XfcePanelPlugin      __parent__;

    GtkWidget       *button;
    GtkWidget       *tray;

    gboolean         img_status;

    GQueue          *queue;
    GHashTable      *data_list;
    gint             total;

    gchar           *signing;
    gchar           *session_id;
    gchar           *client_id;
    gchar           *default_domain;
    gint             disabled_cnt;
};

typedef struct
{
    gchar     *url;
    gchar     *title;
    gchar     *icon;
}NoticeData;

static GPid spid = -1;
static uint log_handler = 0;
static gboolean is_job = FALSE;
static gboolean is_agent = FALSE;
static gboolean is_connected = FALSE;
/* define the plugin */
XFCE_PANEL_DEFINE_PLUGIN (GooroomNoticePlugin, gooroom_notice_plugin)

void
gooroom_log_handler(const gchar *log_domain,
            GLogLevelFlags log_level,
            const gchar *message,
            gpointer user_data)
{
#ifdef DEBUG_MSG
    FILE *file = NULL;
    file = fopen ("/var/tmp/notice.debug", "a");
    if (file == NULL)
        return;

    fputs(message, file);
    fclose (file);
#endif
}

static void
gooroom_tray_icon_change (gpointer user_data)
{
    GooroomNoticePlugin *plugin = GOOROOM_NOTICE_PLUGIN (user_data);

    if (is_connected && is_agent)
    {
        gtk_container_remove (GTK_CONTAINER (plugin->button), plugin->tray);

        gchar *icon = NULL;

        if (plugin->img_status)
            icon = g_strdup(DEFAULT_NOTICE_TRAY_ICON);
        else
            icon = g_strdup(DEFAULT_TRAY_ICON);

        GdkPixbuf *pix;
        pix = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                icon,
                PANEL_TRAY_ICON_SIZE,
                GTK_ICON_LOOKUP_FORCE_SIZE, NULL);

        g_free (icon);

        if (pix)
        {
            plugin->tray = gtk_image_new_from_pixbuf (pix);
            gtk_image_set_pixel_size (GTK_IMAGE (plugin->tray), PANEL_TRAY_ICON_SIZE);
            gtk_container_add (GTK_CONTAINER (plugin->button), plugin->tray);
            g_object_unref (G_OBJECT (pix));
        }

        gtk_widget_show_all (plugin->button);
    }
    else
    {
        gtk_widget_hide_all (plugin->button);
    }
}

static gchar*
gooroom_notice_limit_text (gchar *text, gint limit)
{
    gchar *title = g_strstrip (text);
    glong len = g_utf8_strlen (title, -1);

    if (limit < len)
    {
        g_autofree gchar *t = g_utf8_substring (title, 0, limit);
        title = g_strdup_printf ("%s...", t);
    }

   return title;
}

static gchar*
gooroom_notice_other_text (gchar *text, gint other_cnt)
{
    gchar *title = g_strstrip (text);

    if (1 < other_cnt)
    {
        g_autofree gchar *other = g_strdup_printf (_("other %d cases"), other_cnt);
        title = g_strdup_printf ("%s %s", title, other);
    }
    return title;
}

json_object *
JSON_OBJECT_GET (json_object *root_obj, const char *key)
{
    if (!root_obj) return NULL;

    json_object *ret_obj = NULL;

    json_object_object_get_ex (root_obj, key, &ret_obj);

    return ret_obj;
}

static void
gooroom_notice_child_proc (GPid pid, gint status, gpointer user_data)
{
    if (status == 0)
        spid = -1;
}

static GDBusProxy *agent_proxy = NULL;

static void
gooroom_agent_signal_cb (GDBusProxy *proxy,
                         gchar *sender_name,
                         gchar *signal_name,
                         GVariant *parameters,
                         gpointer user_data)
{
    g_autofree gchar *signal = g_strdup (NOTIFICATION_SIGNAL);
    if (g_strcmp0 (signal_name, signal) == 0)
    {
        g_return_if_fail (user_data != NULL);

        GVariant *v;
        g_variant_get (parameters, "(v)", &v);
        gchar *res = g_variant_dup_string (v, NULL);

        g_debug ("gooroom_agent_signal_cb : signal name [%s], param [%s] \n",signal_name, res);
        gooroom_application_notice_get_data_from_json (user_data, res, TRUE);

        GooroomNoticePlugin *plugin = GOOROOM_NOTICE_PLUGIN (user_data);
        guint total = g_queue_get_length (plugin->queue);
        if (0 < total || 0 < plugin->disabled_cnt)
        {
            plugin->img_status = TRUE;
            gooroom_tray_icon_change (user_data);
            if (!is_job)
            {
                g_timeout_add (500, (GSourceFunc) gooroom_notice_plugin_job,(gpointer)user_data);
            }
        }
    }
}

static GDBusProxy*
gooroom_agent_proxy_get (void)
{
    if (agent_proxy == NULL)
    {
        agent_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                  G_DBUS_CALL_FLAGS_NONE,
                  NULL,
                  "kr.gooroom.agent",
                  "/kr/gooroom/agent",
                  "kr.gooroom.agent",
                  NULL,
                  NULL);
    }

    return agent_proxy;
}

static void
gooroom_agent_bind_signal (gpointer data)
{
    agent_proxy = gooroom_agent_proxy_get();
    if (agent_proxy)
        g_signal_connect (agent_proxy, "g-signal", G_CALLBACK (gooroom_agent_signal_cb), data);
}

void
gooroom_application_notice_get_data_from_json (gpointer user_data, const gchar *data, gboolean urgency)
{
    GooroomNoticePlugin *plugin = GOOROOM_NOTICE_PLUGIN (user_data);

    gchar *ret = NULL;

    enum json_tokener_error jerr = json_tokener_success;
    json_object *root_obj = json_tokener_parse_verbose (data, &jerr);

    if (jerr != json_tokener_success)
        goto done;

    json_object *obj1 = NULL, *obj2 = NULL, *obj3 = NULL, *obj4 = NULL, *noti_obj = NULL;

    if (!urgency)
    {
        obj1 = JSON_OBJECT_GET (root_obj, "module");
        obj2 = JSON_OBJECT_GET (obj1, "task");
        obj3 = JSON_OBJECT_GET (obj2, "out");
        obj4 = JSON_OBJECT_GET (obj3, "status");

        if (!obj4)
            goto done;

        const char *val = json_object_get_string (obj4);
        if (val && g_strcmp0 (val, "200") != 0)
            goto done;

        noti_obj = JSON_OBJECT_GET (obj3, "noti_info");
        if (!noti_obj)
            goto done;
    }
    else
    {
        noti_obj = root_obj;
    }

    json_object *enable_view = JSON_OBJECT_GET (noti_obj, "enabled_title_view_notis");
    json_object *disable_view = JSON_OBJECT_GET (noti_obj, "disabled_title_view_cnt");
    json_object *signing = JSON_OBJECT_GET (noti_obj, "signing");
    json_object *client_id = JSON_OBJECT_GET (noti_obj, "client_id");
    json_object *session_id = JSON_OBJECT_GET (noti_obj, "session_id");
    json_object *default_domain = JSON_OBJECT_GET (noti_obj, "default_noti_domain");

    if (signing)
        plugin->signing = g_strdup_printf ("%s", json_object_get_string (signing));

    if (client_id)
        plugin->client_id = g_strdup_printf ("%s", json_object_get_string (client_id));

    if (session_id)
        plugin->session_id = g_strdup_printf ("%s", json_object_get_string (session_id));

    if (disable_view)
        plugin->disabled_cnt = json_object_get_int (disable_view);

    if (default_domain)
        plugin->default_domain = g_strdup_printf ("%s", json_object_get_string (default_domain));

    glong client_len = g_utf8_strlen (plugin->client_id, -1);
    if (client_len == 0)
        plugin->client_id = "NULL";

    glong session_len = g_utf8_strlen (plugin->session_id, -1);
    if (session_len == 0)
        plugin->session_id = "NULL";

    if (enable_view)
    {
        gint i = 0;
        gint len = json_object_array_length (enable_view);

        for (i = 0; i < len; i++)
        {
            json_object *v_obj = json_object_array_get_idx (enable_view, i);

            if (!v_obj)
                continue;

            json_object *title = JSON_OBJECT_GET (v_obj, "title");
            json_object *url = JSON_OBJECT_GET (v_obj, "url");

            NoticeData *n;
            n = g_try_new0 (NoticeData, 1);
            n->title = g_strdup_printf ("%s", json_object_get_string (title));
            n->url = g_strdup_printf ("%s", json_object_get_string (url));
            n->icon = urgency ? g_strdup (NOTIFICATION_MSG_URGENCY_ICON) : g_strdup (NOTIFICATION_MSG_ICON);
            g_queue_push_tail (plugin->queue, n);
        }
    }
    json_object_put (root_obj);
done:
    json_object_put (root_obj);
    return;
}

static void
gooroom_application_notice_done_cb (GObject *source_object,
                               GAsyncResult *res,
                               gpointer user_data)
{
    GVariant *variant;
    gchar *data = NULL;

    variant = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, NULL);

    if (variant)
    {
        GVariant *v;
        g_variant_get (variant, "(v)", &v);
        if (v)
        {
            data = g_variant_dup_string (v, NULL);
            g_variant_unref (v);
        }
        g_variant_unref (v);
    }

    if (data)
    {
        g_debug ("gooroom_application_notice_done_cb : agent param [%s]\n", data);

        gooroom_application_notice_get_data_from_json (user_data, data, FALSE);

        GooroomNoticePlugin *plugin = GOOROOM_NOTICE_PLUGIN (user_data);
        guint total = g_queue_get_length (plugin->queue);
        if (0 < total || 0 < plugin->disabled_cnt)
        {
            plugin->img_status = TRUE;

            if (!is_job)
                g_timeout_add (500, (GSourceFunc) gooroom_notice_plugin_job,(gpointer)user_data);
        }

        is_agent = TRUE;
    }

    gooroom_tray_icon_change (user_data);
}

static void
gooroom_application_notice_update (gpointer user_data)
{
    agent_proxy = gooroom_agent_proxy_get ();

    if (agent_proxy)
    {
        const gchar *json = "{\"module\":{\"module_name\":\"noti\",\"task\":{\"task_name\":\"get_noti\",\"in\":{\"login_id\":\"%s\"}}}}";

        gchar *arg = g_strdup_printf (json, g_get_user_name ());

        g_dbus_proxy_call (agent_proxy,
                           "do_task",
                           g_variant_new ("(s)", arg),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           gooroom_application_notice_done_cb,
                           user_data);

        g_free (arg);
    }
}

static gboolean
gooroom_application_notice_update_delay (gpointer user_data)
{
    gooroom_agent_bind_signal(user_data);
    gooroom_application_notice_update (user_data);
    return FALSE;
}

static void
on_notification_closed (NotifyNotification *notification, gpointer user_data)
{
    GooroomNoticePlugin *plugin = GOOROOM_NOTICE_PLUGIN (user_data);
    plugin->total--;

    g_hash_table_remove (plugin->data_list, notification);
    g_object_unref (notification);
}

static void
on_notification_popup_webview_cancelled (GCancellable *cancellable, gpointer user_data)
{
}

static void
on_notification_popup_webview_readycallback (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
}

static void
on_notification_popup_opened (NotifyNotification *notification, char *action, gpointer user_data)
{
    if (spid != -1)
    {
        kill (spid, 9);
        spid = -1;
    }

    GooroomNoticePlugin *plugin = GOOROOM_NOTICE_PLUGIN (user_data);

    plugin->img_status = FALSE;
    gooroom_tray_icon_change (user_data);

    NoticeData *n;
    NotifyNotification *key;
    if (g_hash_table_lookup_extended (plugin->data_list,(gpointer)notification, (gpointer)&key, (gpointer)&n))
    {
        g_autofree gchar *cmd  = g_strdup ("/usr/bin/gooroom-notice-popup %s %s %s %s");
        gchar *args = g_strdup_printf (cmd, n->url, plugin->signing, plugin->session_id, plugin->client_id);

        g_queue_clear (plugin->queue);
        g_hash_table_remove_all (plugin->data_list);

        gchar **argv;
        GError *error = NULL;

        if (!g_shell_parse_argv (args, NULL, &argv, &error))
        {
            g_warning ("%s", error->message);
            g_error_free (error);
            goto done;
        }

        if (!g_spawn_async (NULL, argv, NULL, G_SPAWN_LEAVE_DESCRIPTORS_OPEN | G_SPAWN_DO_NOT_REAP_CHILD,
                            NULL, NULL, &spid, &error))
        {
            g_warning ("%s", error->message);
            g_error_free (error);
            goto done;
        }

        g_child_watch_add (spid, gooroom_notice_child_proc, NULL);
done :
        g_free (args);
    }
}

static NotifyNotification *
notification_opened (gpointer user_data, gchar *title, gchar *icon)
{
    NotifyNotification *notification;
    notify_init (PACKAGE_NAME);
    notification = notify_notification_new (title, "", icon);
    notify_notification_add_action (notification, "default", _("detail view"), (NotifyActionCallback)on_notification_popup_opened, user_data, NULL);
    notify_notification_set_urgency (notification, NOTIFY_URGENCY_NORMAL);
    notify_notification_set_timeout (notification, NOTIFICATION_TIMEOUT);
    notify_notification_show (notification, NULL);

    return notification;
}

static void
on_notice_plugin_hash_key_destroy (gpointer data)
{
    NotifyNotification *n = (NotifyNotification*)data;
    notify_notification_close (n, NULL);
}

static void
on_notice_plugin_hash_value_destroy (gpointer data)
{
    NoticeData *n = (NoticeData *)data;

    if (n->title)
        g_free (n->title);

    if (n->url)
        g_free (n->url);

    if (n->icon)
        g_free (n->icon);
}

static int cnt = 0;
static gboolean
on_notice_plugin_button_pressed (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    if (spid != -1)
    {
        kill (spid, 9);
        spid = -1;
    }

    GooroomNoticePlugin *plugin = GOOROOM_NOTICE_PLUGIN (user_data);
    plugin->img_status = FALSE;
    gooroom_tray_icon_change (user_data);

    g_autofree gchar *cmd  = g_strdup ("/usr/bin/gooroom-notice-popup %s %s %s %s");
    gchar *args = g_strdup_printf (cmd, plugin->default_domain, plugin->signing, plugin->session_id, plugin->client_id);

    g_debug ("on_notice_plugin_button_pressed : popup args [%s]\n", args);
    g_queue_clear (plugin->queue);
    g_hash_table_remove_all (plugin->data_list);

    gchar **argv;
    GError *error = NULL;

    if (!g_shell_parse_argv (args, NULL, &argv, &error))
    {
        g_warning ("%s", error->message);
        g_error_free (error);
        goto done;
    }

    if (!g_spawn_async (NULL, argv, NULL, G_SPAWN_LEAVE_DESCRIPTORS_OPEN | G_SPAWN_DO_NOT_REAP_CHILD,
                NULL, NULL, &spid, &error))
    {
        g_warning ("%s", error->message);
        g_error_free (error);
        goto done;
    }

    g_child_watch_add (spid, gooroom_notice_child_proc, NULL);
done :
    g_free (args);
    return FALSE;
}

gboolean
gooroom_notice_plugin_job (gpointer data)
{
    GooroomNoticePlugin *plugin = GOOROOM_NOTICE_PLUGIN (data);

    is_job = TRUE;

    if (NOTIFICATION_LIMIT <= plugin->total)
    {
        guint total = g_queue_get_length (plugin->queue);
        if (0 == total)
            is_job = FALSE;

        return is_job;
    }

    NoticeData *n;
    while ((n = g_queue_pop_head (plugin->queue)))
    {
        if (!n)
            continue;

        plugin->total++;

        gchar *title = NULL;
        title = gooroom_notice_limit_text (n->title, NOTIFICATION_TEXT_LIMIT);

        NotifyNotification *notification =  notification_opened (plugin, title, n->icon);
        g_hash_table_insert (plugin->data_list, notification, n);
        g_signal_connect (G_OBJECT (notification), "closed", G_CALLBACK (on_notification_closed), plugin);

        return is_job;
    }

    guint total = g_queue_get_length (plugin->queue);

    if (0 !=  total)
        return is_job;

    if (0 != plugin->disabled_cnt)
    {
        plugin->total++;

        gchar *tmp = g_strdup (_("Notice"));
        gchar *no_title = gooroom_notice_other_text (tmp, plugin->disabled_cnt);

        NoticeData *v;
        n = g_try_new0 (NoticeData, 1);
        n->title = no_title;
        n->url = g_strdup_printf ("%s", plugin->default_domain);
        n->icon = g_strdup (NOTIFICATION_MSG_ICON);

        NotifyNotification *notification =  notification_opened (plugin, no_title, n->icon);
        g_hash_table_insert (plugin->data_list, notification, n);
        g_signal_connect (G_OBJECT (notification), "closed", G_CALLBACK (on_notification_closed), plugin);
    }

    is_job = FALSE;
    return is_job;
}

static void
gooroom_notice_plugin_network_changed (GNetworkMonitor *monitor,
                                       gboolean network_available,
                                       gpointer user_data)
{
    if (is_connected == network_available)
        return;

    is_connected = network_available;
    gooroom_tray_icon_change (user_data);

    if (is_connected)
    {
        if (agent_proxy == NULL || !is_agent)
            g_timeout_add (500, (GSourceFunc)gooroom_application_notice_update_delay, user_data);
    }
}

static void
gooroom_notice_plugin_free_data (XfcePanelPlugin *panel_plugin)
{
    GooroomNoticePlugin *plugin = GOOROOM_NOTICE_PLUGIN (panel_plugin);

    if (plugin->signing)
        g_free (plugin->signing);

    if (plugin->session_id)
        g_free (plugin->session_id);

    if (plugin->client_id)
        g_free (plugin->client_id);

    if (plugin->default_domain)
        g_free (plugin->default_domain);

    if (plugin->queue)
    {
        g_queue_clear (plugin->queue);
        g_queue_free (plugin->queue);
        plugin->queue = NULL;
    }

    if (plugin->data_list)
    {
        g_hash_table_destroy (plugin->data_list);
        plugin->data_list = NULL;
    }

    if (agent_proxy)
        g_object_unref (agent_proxy);

    if (log_handler != 0)
    {
        g_log_remove_handler (NULL, log_handler);
        log_handler = 0;
    }
}

static gboolean
gooroom_notice_plugin_size_changed (XfcePanelPlugin *panel_plugin,
                                    gint size)
{
    GooroomNoticePlugin *plugin = GOOROOM_NOTICE_PLUGIN (panel_plugin);

    /* set the clock size */
    if (xfce_panel_plugin_get_mode (panel_plugin) == XFCE_PANEL_PLUGIN_MODE_HORIZONTAL)
        gtk_widget_set_size_request (GTK_WIDGET (panel_plugin), -1, size);
    else
        gtk_widget_set_size_request (GTK_WIDGET (panel_plugin), size, -1);

    return TRUE;
}

static void
gooroom_notice_plugin_mode_changed (XfcePanelPlugin *plugin, XfcePanelPluginMode mode)
{
    gooroom_notice_plugin_size_changed (plugin, xfce_panel_plugin_get_size (plugin));
}

static void
gooroom_notice_plugin_init (GooroomNoticePlugin *plugin)
{
    plugin->button       = NULL;
    plugin->tray         = NULL;

    plugin->img_status   = FALSE;
    plugin->total        = 0;
    plugin->queue        = g_queue_new ();
    plugin->data_list    = g_hash_table_new_full (g_direct_hash, g_direct_equal,(GDestroyNotify)on_notice_plugin_hash_key_destroy, (GDestroyNotify)on_notice_plugin_hash_value_destroy);

    plugin->signing      = NULL;
    plugin->session_id   = NULL;
    plugin->client_id    = NULL;
    plugin->default_domain = NULL;
    plugin->disabled_cnt = 0;

    xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

    plugin->button = xfce_panel_create_button ();
    xfce_panel_plugin_add_action_widget (XFCE_PANEL_PLUGIN (plugin), plugin->button);
    gtk_container_add (GTK_CONTAINER (plugin), plugin->button);
    g_signal_connect (G_OBJECT (plugin->button), "button-press-event", G_CALLBACK (on_notice_plugin_button_pressed), plugin);

    GdkPixbuf *pix;
    pix = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
            DEFAULT_TRAY_ICON,
            PANEL_TRAY_ICON_SIZE,
            GTK_ICON_LOOKUP_FORCE_SIZE, NULL);

    if (pix) {
        plugin->tray = gtk_image_new_from_pixbuf (pix);
        gtk_image_set_pixel_size (GTK_IMAGE (plugin->tray), PANEL_TRAY_ICON_SIZE);
        gtk_container_add (GTK_CONTAINER (plugin->button), plugin->tray);
        g_object_unref (G_OBJECT (pix));
    }

    gtk_widget_hide_all (plugin->button);
    log_handler = g_log_set_handler (NULL,
                                     G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
                                     gooroom_log_handler, NULL);

    GNetworkMonitor *monitor = g_network_monitor_get_default();
    g_signal_connect (monitor, "network-changed", G_CALLBACK (gooroom_notice_plugin_network_changed), plugin);

    is_connected = g_network_monitor_get_network_available(monitor);

    if (is_connected)
        g_timeout_add (500, (GSourceFunc)gooroom_application_notice_update_delay, plugin);
}

static void
gooroom_notice_plugin_class_init (GooroomNoticePluginClass *klass)
{
    XfcePanelPluginClass *plugin_class;

    plugin_class = XFCE_PANEL_PLUGIN_CLASS (klass);
    plugin_class->free_data = gooroom_notice_plugin_free_data;
    plugin_class->size_changed = gooroom_notice_plugin_size_changed;
    plugin_class->mode_changed = gooroom_notice_plugin_mode_changed;
}
