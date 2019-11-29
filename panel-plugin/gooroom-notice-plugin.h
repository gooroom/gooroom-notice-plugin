/*
 *  Copyright (C) 2018-2019 Gooroom <gooroom@gooroom.kr>
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

#ifndef __GOOROOM_NOTICE_PLUGIN_H__
#define __GOOROOM_NOTICE_PLUGIN_H__

#include <glib.h>
#include <libxfce4panel/libxfce4panel.h>

G_BEGIN_DECLS
typedef struct _GooroomNoticePluginClass GooroomNoticePluginClass;
typedef struct _GooroomNoticePlugin      GooroomNoticePlugin;

#define TYPE_GOOROOM_NOTICE_PLUGIN    (gooroom_notice_plugin_get_type ())
#define GOOROOM_NOTICE_PLUGIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_GOOROOM_NOTICE_PLUGIN, GooroomNoticePlugin))
#define GOOROOM_NOTICE_PLUGIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  TYPE_GOOROOM_NOTICE_PLUGIN, GooroomNoticePluginClass))
#define IS_GOOROOM_NOTICE_PLUGIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_GOOROOM_NOTICE_PLUGIN))
#define IS_GOOROOM_NOTICE_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  TYPE_GOOROOM_NOTICE_PLUGIN))
#define GOOROOM_NOTICE_PLUGIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  TYPE_GOOROOM_NOTICE_PLUGIN, GooroomNoticePluginClass))

gboolean  gooroom_notice_plugin_job       (gpointer user_data);

GType gooroom_notice_plugin_get_type      (void) G_GNUC_CONST;

void gooroom_notice_plugin_register_type (XfcePanelTypeModule *type_module);

void gooroom_application_notice_get_data_from_json (gpointer user_data, const gchar *data, gboolean urgency);

void gooroom_log_handler(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data);

G_END_DECLS

#endif /* !__NOTICE_PLUGIN_H__ */
