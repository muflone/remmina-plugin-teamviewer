/*
 * Project name : Remmina Plugin TEAMVIEWER
 * Remmina protocol plugin to launch a TeamViewer connection.
 * Copyright (C) 2012-2013 Fabio Castelli <muflone@vbsimple.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "plugin_config.h"
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <pthread.h>
#include <remmina/plugin.h>
#if GTK_VERSION == 3
  # include <gtk/gtkx.h>
#endif

typedef struct _RemminaPluginData
{
  GtkWidget *socket;
  gint socket_id;
  GPid pid;
  gchar **output_fd;
  gchar **error_fd;
  gint display;
  gboolean ready;
  gint *exit_status;

#ifdef HAVE_PTHREAD
  pthread_t thread;
#else
  gint thread;
#endif
} RemminaPluginData;

static RemminaPluginService *remmina_plugin_service = NULL;

static void remmina_plugin_on_plug_added(GtkSocket *socket, RemminaProtocolWidget *gp)
{
  RemminaPluginData *gpdata;
  gpdata = (RemminaPluginData*) g_object_get_data(G_OBJECT(gp), "plugin-data");
  remmina_plugin_service->log_printf("[%s] remmina_plugin_on_plug_added socket %d\n", PLUGIN_NAME, gpdata->socket_id);
  remmina_plugin_service->protocol_plugin_emit_signal(gp, "connect");
  gpdata->ready = TRUE;
  return;
}

static void remmina_plugin_on_plug_removed(GtkSocket *socket, RemminaProtocolWidget *gp)
{
  remmina_plugin_service->log_printf("[%s] remmina_plugin_on_plug_removed\n", PLUGIN_NAME);
  remmina_plugin_service->protocol_plugin_close_connection(gp);
}

static void remmina_plugin_init(RemminaProtocolWidget *gp)
{
  remmina_plugin_service->log_printf("[%s] remmina_plugin_init\n", PLUGIN_NAME);
  RemminaPluginData *gpdata;

  gpdata = g_new0(RemminaPluginData, 1);
  g_object_set_data_full(G_OBJECT(gp), "plugin-data", gpdata, g_free);

  gpdata->socket = gtk_socket_new();
  remmina_plugin_service->protocol_plugin_register_hostkey(gp, gpdata->socket);
  gtk_widget_show(gpdata->socket);
  g_signal_connect(G_OBJECT(gpdata->socket), "plug-added", G_CALLBACK(remmina_plugin_on_plug_added), gp);
  g_signal_connect(G_OBJECT(gpdata->socket), "plug-removed", G_CALLBACK(remmina_plugin_on_plug_removed), gp);
  gtk_container_add(GTK_CONTAINER(gp), gpdata->socket);
}

static gboolean remmina_plugin_open_connection(RemminaProtocolWidget *gp)
{
  remmina_plugin_service->log_printf("[%s] remmina_plugin_open_connection\n", PLUGIN_NAME);
  #define GET_PLUGIN_STRING(value) \
    g_strdup(remmina_plugin_service->file_get_string(remminafile, value))
  #define GET_PLUGIN_BOOLEAN(value) \
    remmina_plugin_service->file_get_int(remminafile, value, FALSE)
  #define GET_PLUGIN_INT(value, default_value) \
    remmina_plugin_service->file_get_int(remminafile, value, default_value)
  #define GET_PLUGIN_PASSWORD(value) \
    g_strdup(remmina_plugin_service->file_get_secret(remminafile, value));

  RemminaPluginData *gpdata;
  RemminaFile *remminafile;
  gboolean ret;
  GError *error = NULL;
  gchar *argv[50];
  gint argc;
  gint i;
  
  gchar *option_str;

  gpdata = (RemminaPluginData*) g_object_get_data(G_OBJECT(gp), "plugin-data");
  remminafile = remmina_plugin_service->protocol_plugin_get_file(gp);

  argc = 0;
  argv[argc++] = strdup("teamviewer");

  // Some tvw_main skips the first argument so we're adding a fake argument in the first place
  if (GET_PLUGIN_BOOLEAN("adddashes"))
    argv[argc++] = g_strdup("--");
  argv[argc++] = g_strdup("-i");
  argv[argc++] = GET_PLUGIN_STRING("server");;

  option_str = GET_PLUGIN_PASSWORD("password");
  if (option_str)
  {
    argv[argc++] = g_strdup("--Password");
    argv[argc++] = g_strdup(option_str);
  }

  argv[argc++] = NULL;

  remmina_plugin_service->log_printf("[TEAMVIEWER] starting teamviewer\n");
  ret = g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
    NULL, NULL, &gpdata->pid, &error);
  remmina_plugin_service->log_printf("[TEAMVIEWER] started with pid %d\n", 
    &gpdata->pid);

  for (i = 0; i < argc; i++)
    g_free(argv[i]);

  if (!ret)
    remmina_plugin_service->protocol_plugin_set_error(gp, "%s", error->message);

  return FALSE;
}

static gboolean remmina_plugin_close_connection(RemminaProtocolWidget *gp)
{
  remmina_plugin_service->log_printf("[%s] remmina_plugin_close_connection\n", PLUGIN_NAME);
  remmina_plugin_service->protocol_plugin_emit_signal(gp, "disconnect");
  return FALSE;
}

static gboolean remmina_plugin_query_feature(RemminaProtocolWidget *gp, const RemminaProtocolFeature *feature)
{
  remmina_plugin_service->log_printf("[%s] remmina_plugin_query_feature\n", PLUGIN_NAME);
  return FALSE;
}

static void remmina_plugin_call_feature(RemminaProtocolWidget *gp, const RemminaProtocolFeature *feature)
{
  remmina_plugin_service->log_printf("[%s] remmina_plugin_call_feature\n", PLUGIN_NAME);
  return;
}

static const RemminaProtocolSetting remmina_plugin_basic_settings[] =
{
  { REMMINA_PROTOCOL_SETTING_TYPE_SERVER, NULL, NULL, FALSE, NULL, NULL },
  { REMMINA_PROTOCOL_SETTING_TYPE_PASSWORD, NULL, NULL, FALSE, NULL, NULL },
  { REMMINA_PROTOCOL_SETTING_TYPE_CHECK, "adddashes", N_("Add dashes as first argument"), FALSE, NULL, NULL },
  { REMMINA_PROTOCOL_SETTING_TYPE_END, NULL, NULL, FALSE, NULL, NULL }
};

static RemminaProtocolPlugin remmina_plugin =
{
  REMMINA_PLUGIN_TYPE_PROTOCOL,
  PLUGIN_NAME,
  PLUGIN_DESCRIPTION,
  GETTEXT_PACKAGE,
  PLUGIN_VERSION,
  PLUGIN_APPICON,
  PLUGIN_APPICON,
  remmina_plugin_basic_settings,
  NULL,
  REMMINA_PROTOCOL_SSH_SETTING_NONE,
  NULL,
  remmina_plugin_init,
  remmina_plugin_open_connection,
  remmina_plugin_close_connection,
  remmina_plugin_query_feature,
  remmina_plugin_call_feature
};

G_MODULE_EXPORT gboolean remmina_plugin_entry(RemminaPluginService *service)
{
  remmina_plugin_service = service;

  if (!service->register_plugin((RemminaPlugin *) &remmina_plugin))
  {
    return FALSE;
  }
  return TRUE;
}
