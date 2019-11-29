/*
 * Copyright (C) 2018-2019 Gooroom <gooroom@gooroom.kr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <webkit2/webkit2.h>

typedef struct
{
    gchar *client_id;
    gchar *session_id;
    gchar *signing;
    gchar *lang;
}CookieData;

static void
on_notification_popup_webview_cancelled (GCancellable *cancellable, gpointer user_data)
{
}

static void
on_notification_popup_cookie_cb(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
}

static gboolean
on_notification_popup_webview_cb (WebKitWebView* web_view, GtkWidget* window)
{
    gtk_widget_destroy (window);
    return TRUE;
}

static gboolean
on_notification_popup_window_cb (GtkWidget* widget, GtkWidget* window)
{
    gtk_main_quit();
    return TRUE;
}

static void
on_webview_load_started_cb (WebKitWebView* view, WebKitLoadEvent load_event, gpointer user_data)
{
    switch (load_event)
    {
        case WEBKIT_LOAD_COMMITTED:
            {
                CookieData *cookie = (CookieData *)user_data;
                if (g_utf8_strlen(cookie->client_id, -1) != 0)
                {
                    g_autofree gchar *script = g_strdup_printf ("document.cookie ='CLIENT_ID=%s;1'", cookie->client_id);
                    webkit_web_view_run_javascript (view, script, NULL, NULL, NULL);
                }

                if (g_utf8_strlen(cookie->session_id, -1) != 0)
                {
                    g_autofree gchar *script = g_strdup_printf ("document.cookie ='SESSION_ID=%s;1'", cookie->session_id);
                    webkit_web_view_run_javascript (view, script, NULL, NULL, NULL);
                }

                if (g_utf8_strlen(cookie->signing, -1) != 0)
                {
                    g_autofree gchar *script = g_strdup_printf ("document.cookie ='SIGNING=%s;1'", cookie->signing);
                    webkit_web_view_run_javascript (view, script, NULL, NULL, NULL);
                }

                if (g_utf8_strlen(cookie->lang, -1) != 0)
                {
                    g_autofree gchar *script = g_strdup_printf ("document.cookie ='LANG_CODE=%s;1'", cookie->lang);
                    webkit_web_view_run_javascript (view, script, NULL, NULL, NULL);
                    g_free (cookie->lang);
                }

                break;
            }
    }
}

static gchar*
gooroom_notice_get_language ()
{
    gchar *lang = NULL;

    PangoLanguage *language = gtk_get_default_language();

    if (language)
    {
        const gchar *plang = pango_language_to_string (language);

        if (g_strcmp0 (plang, "ko-kr") == 0)
            lang = g_strdup ("ko");
        else
            lang = g_strdup ("en");
    }
    return lang;
}

int main(int argc, char* argv[])
{
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    gtk_init(&argc, &argv);

    gchar *url = argv[1];
    gchar *signing = argv[2];
    gchar *session_id = g_strcmp0 (argv[3], "NULL") == 0 ? "" : argv[3];
    gchar *client_id = g_strcmp0 (argv[4], "NULL") == 0 ? "" : argv[4];

    GtkWidget *window;
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width (GTK_CONTAINER (window), 5);
    gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW (window), FALSE);
    gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
    gtk_window_set_title (GTK_WINDOW (window), _("Notice"));

    gtk_window_set_icon_name (GTK_WINDOW (window), "notice-plugin");

    GtkWidget *main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add (GTK_CONTAINER (window), main_vbox);
    gtk_widget_show (main_vbox);

    GtkWidget *scroll_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start (GTK_BOX (main_vbox), scroll_window, TRUE, TRUE, 0);
    gtk_widget_show (scroll_window);

    WebKitWebView *view = WEBKIT_WEB_VIEW (webkit_web_view_new());
    gtk_container_add (GTK_CONTAINER (scroll_window), GTK_WIDGET(view));

    WebKitSettings *web_setting = webkit_web_view_get_settings (view);
    webkit_settings_set_enable_developer_extras (web_setting, true);
    webkit_web_view_set_settings (view, web_setting);

    gchar* lang = gooroom_notice_get_language();

    CookieData *cookie;
    cookie = g_try_new0 (CookieData, 1);
    cookie->client_id = client_id;
    cookie->session_id = session_id;
    cookie->signing = signing;
    cookie->lang = lang;

    g_signal_connect (window, "destroy", G_CALLBACK (on_notification_popup_window_cb), NULL);
    g_signal_connect (view, "close", G_CALLBACK (on_notification_popup_webview_cb), window);
    g_signal_connect (view, "load-changed", G_CALLBACK (on_webview_load_started_cb), cookie);

    webkit_web_view_load_uri (view, url);

    gtk_widget_grab_focus (GTK_WIDGET (view));
    gtk_widget_show (GTK_WIDGET(view));

    GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_end (GTK_BOX (main_vbox), hbox, FALSE, TRUE, 0);
    gtk_widget_show (hbox);

    GtkWidget *button = gtk_button_new_with_label (_("Close"));
    gtk_widget_set_can_focus (button, TRUE);
    gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (on_notification_popup_window_cb), (gpointer)window);

    gtk_widget_show (button);

    gtk_window_set_default_size (GTK_WINDOW (window), 600, 550);
    gtk_widget_show_all (window);

    gtk_main();
    return 0;
}
