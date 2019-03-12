AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\" \
	$(PLATFORM_CPPFLAGS)

plugindir = $(libdir)/xfce4/panel/plugins

plugin_LTLIBRARIES = \
	libgooroom-notice-plugin.la

libgooroom_notice_plugin_la_SOURCES = \
	gooroom-notice-plugin.h \
	gooroom-notice-plugin.c \
	$(NULL)

libgooroom_notice_plugin_la_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(LIBXFCE4PANEL_CFLAGS) \
	$(LIBNOTIFY_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(DBUS_GLIB_CFLAGS) \
	$(JSON_C_CFLAGS) \
	$(PLATFORM_CFLAGS)


libgooroom_notice_plugin_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-export-symbols-regex '^xfce_panel_module_(preinit|init|construct)' \
	$(PLATFORM_LDFLAGS)

libgooroom_notice_plugin_la_LIBADD = \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(LIBXFCE4PANEL_LIBS) \
	$(LIBNOTIFY_LIBS) \
	$(DBUS_LIBS) \
	$(DBUS_GLIB_LIBS) \
	$(JSON_C_LIBS)

bin_PROGRAMS = gooroom-notice-popup

gooroom_notice_popup_SOURCES = \
	gooroom-notice-popup.h \
	gooroom-notice-popup.c \
	$(NULL)

gooroom_notice_popup_CFLAGS = \
    -DLOCALEDIR=\""$(localedir)"\"  \
	$(GLIB_CFLAGS) \
	$(GTK3_CFLAGS) \
	$(LIBWEBKIT2GTK_CFLAGS) \
	$(PLATFORM_CFLAGS)

gooroom_notice_popup_LDADD = \
	$(GLIB_LIBS) \
	$(GTK3_LIBS) \
	$(LIBWEBKIT2GTK_LIBS)

gooroom_notice_popup_LDFLAGS = \
	$(PLATFORM_LDFLAGS)

#
# Desktop file
#
desktopdir = \
	$(datadir)/xfce4/panel/plugins

desktop_in_files = \
	gooroom-notice-plugin.desktop.in.in

desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = $(desktop_in_files)

DISTCLEANFILES = $(desktop_DATA)


