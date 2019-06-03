/*
 * Copyright (C) 2019 Alexandros Theodotou <alex at zrythm dot org>
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "audio/mixer.h"
#include "audio/port.h"
#include "audio/track.h"
#include "audio/tracklist.h"
#include "gui/widgets/port_connections_popover.h"
#include "gui/widgets/port_selector_popover.h"
#include "plugins/plugin.h"
#include "project.h"
#include "utils/resources.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

G_DEFINE_TYPE (PortSelectorPopoverWidget,
               port_selector_popover_widget,
               GTK_TYPE_POPOVER)

/** Used as the first row in the Plugin treeview to
 * indicate if "Track ports is selected or not. */
static const Plugin * dummy_plugin = (const Plugin *) 123;

static void
on_ok_clicked (
  GtkButton * btn,
  PortSelectorPopoverWidget * self)
{
  if (self->port->identifier.flow == FLOW_INPUT)
    port_connect (
      self->selected_port, self->port);
  else if (self->port->identifier.flow == FLOW_OUTPUT)
    port_connect (
      self->port, self->selected_port);

  mixer_recalc_graph (MIXER);

  port_connections_popover_widget_refresh (
    self->owner);
}

static void
on_cancel_clicked (
  GtkButton * btn,
  PortSelectorPopoverWidget * self)
{
  gtk_widget_set_visible (GTK_WIDGET (self), 0);
}

/**
 * @param track Track, if track ports selected.
 * @param pl Plugin, if plugin selected.
 */
static GtkTreeModel *
create_model_for_ports (
  PortSelectorPopoverWidget * self,
  Track *  track,
  Plugin * pl)
{
  GtkListStore *list_store;
  GtkTreeIter iter;

  /* icon, name, pointer to port */
  list_store =
    gtk_list_store_new (3,
                        G_TYPE_STRING,
                        G_TYPE_STRING,
                        G_TYPE_POINTER);

  PortType type = self->port->identifier.type;
  PortFlow flow = self->port->identifier.flow;

  if (track && track->channel)
    {
      /*Channel * ch = track->channel;*/

      /* TODO fader & pan */
    }
  else if (pl)
    {
      Port * port;

      if (flow == FLOW_INPUT)
        {
          for (int i = 0;
                   i < pl->num_out_ports; i++)
            {
              port = pl->out_ports[i];

              if ((port->identifier.type != type) &&
                  !(port->identifier.type == TYPE_CV &&
                   type == TYPE_CONTROL))
                continue;

              // Add a new row to the model
              gtk_list_store_append (
                list_store, &iter);
              gtk_list_store_set (
                list_store, &iter,
                0, "z-plugins",
                1, port->identifier.label,
                2, port,
                -1);
            }
        }
      else if (flow == FLOW_OUTPUT)
        {
          for (int i = 0;
                   i < pl->num_in_ports; i++)
            {
              port = pl->in_ports[i];

              if ((port->identifier.type != type) &&
                  !(type == TYPE_CV &&
                   port->identifier.type == TYPE_CONTROL))
                continue;

              // Add a new row to the model
              gtk_list_store_append (
                list_store, &iter);
              gtk_list_store_set (
                list_store, &iter,
                0, "z-plugins",
                1, port->identifier.label,
                2, port,
                -1);
            }
        }
    }

  return GTK_TREE_MODEL (list_store);
}

static GtkTreeModel *
create_model_for_tracks (
  PortSelectorPopoverWidget * self)
{
  GtkListStore *list_store;
  GtkTreeIter iter;

  /* icon, name, pointer to channel */
  list_store =
    gtk_list_store_new (3,
                        G_TYPE_STRING,
                        G_TYPE_STRING,
                        G_TYPE_POINTER);

  Track * track;
  for (int i = 0; i < TRACKLIST->num_tracks; i++)
    {
      track = TRACKLIST->tracks[i];

      if (!track->channel)
        continue;

      // Add a new row to the model
      gtk_list_store_append (list_store, &iter);
      gtk_list_store_set (
        list_store, &iter,
        0, "z-text-x-csrc",
        1, track->name,
        2, track,
        -1);
    }

  return GTK_TREE_MODEL (list_store);
}

/* FIXME leaking if model already exists */
static GtkTreeModel *
create_model_for_plugins (
  PortSelectorPopoverWidget * self,
  Track *                     track)
{
  GtkListStore *list_store;
  GtkTreeIter iter;

  /* icon, name, pointer to plugin */
  list_store =
    gtk_list_store_new (3,
                        G_TYPE_STRING,
                        G_TYPE_STRING,
                        G_TYPE_POINTER);

  if (track)
    {
      // Add a new row to the model
      gtk_list_store_append (list_store, &iter);
      gtk_list_store_set (
        list_store, &iter,
        0, "z-inode-directory",
        1, _("Track Ports"),
        2, dummy_plugin,
        -1);

      Channel * ch = track->channel;
      Plugin * pl;

      for (int i = 0; i < STRIP_SIZE; i++)
        {
          pl = ch->plugins[i];
          if (!pl)
            continue;

          // Add a new row to the model
          gtk_list_store_append (list_store, &iter);
          gtk_list_store_set (
            list_store, &iter,
            0, "z-plugins",
            1, pl->descr->name,
            2, pl,
            -1);
        }
    }

  return GTK_TREE_MODEL (list_store);
}

static void
tree_view_setup (
  PortSelectorPopoverWidget * self,
  GtkTreeModel * model,
  int            init);

static void
on_selection_changed (
  GtkTreeSelection * ts,
  PortSelectorPopoverWidget * self)
{
  GtkTreeView * tv =
    gtk_tree_selection_get_tree_view (ts);
  GtkTreeModel * model =
    gtk_tree_view_get_model (tv);
  GList * selected_rows =
    gtk_tree_selection_get_selected_rows (ts,
                                          NULL);
  if (selected_rows)
    {
      GtkTreePath * tp =
        (GtkTreePath *)
          g_list_first (selected_rows)->data;
      GtkTreeIter iter;
      gtk_tree_model_get_iter (model,
                               &iter,
                               tp);
      GValue value = G_VALUE_INIT;

      if (model == self->track_model)
        {
          gtk_tree_model_get_value (model,
                                    &iter,
                                    2,
                                    &value);
          self->selected_track =
            g_value_get_pointer (&value);
          self->plugin_model =
            create_model_for_plugins (
              self, self->selected_track);
          tree_view_setup (
            self,
            self->plugin_model, 0);
          self->port_model =
            create_model_for_ports (
              self, NULL, NULL);
          tree_view_setup (
            self,
            self->port_model, 0);
        }
      else if (model == self->plugin_model)
        {
          gtk_tree_model_get_value (model,
                                    &iter,
                                    2,
                                    &value);
          Plugin * selected_pl =
            g_value_get_pointer (&value);
          if (selected_pl == dummy_plugin)
            {
              self->selected_plugin = NULL;
              self->track_ports_selected = 1;
            }
          else
            {
              self->selected_plugin = selected_pl;
              self->track_ports_selected = 0;
            }

          if (self->track_ports_selected)
            self->port_model =
              create_model_for_ports (
                self, self->selected_track,
                NULL);
          else
            self->port_model =
              create_model_for_ports (
                self, NULL,
                self->selected_plugin);
          tree_view_setup (
            self,
            self->port_model, 0);
        }
      else if (model == self->port_model)
        {
          gtk_tree_model_get_value (model,
                                    &iter,
                                    2,
                                    &value);
          self->selected_port =
            g_value_get_pointer (&value);
        }
    }
}

/**
 * @param init Initialize columns, only the first time.
 */
static void
tree_view_setup (
  PortSelectorPopoverWidget * self,
  GtkTreeModel * model,
  int            init)
{
  GtkTreeView * tree_view = NULL;

  if (model == self->track_model)
    tree_view = self->track_treeview;
  else if (model == self->plugin_model)
    tree_view = self->plugin_treeview;
  else if (model == self->port_model)
    tree_view = self->port_treeview;

  /* instantiate tree view using model */
  gtk_tree_view_set_model (
    tree_view, model);

  if (init)
    {
      /* init tree view */
      GtkCellRenderer * renderer;
      GtkTreeViewColumn * column;

      /* column for icon */
      renderer =
        gtk_cell_renderer_pixbuf_new ();
      column =
        gtk_tree_view_column_new_with_attributes (
          "icon", renderer,
          "icon-name", 0,
          NULL);
      gtk_tree_view_append_column (
        GTK_TREE_VIEW (tree_view),
        column);

      /* column for name */
      renderer =
        gtk_cell_renderer_text_new ();
      column =
        gtk_tree_view_column_new_with_attributes (
          "name", renderer,
          "text", 1,
          NULL);
      gtk_tree_view_append_column (
        GTK_TREE_VIEW (tree_view),
        column);

      /* set search column */
      gtk_tree_view_set_search_column (
        GTK_TREE_VIEW (tree_view),
        1);

      /* set headers invisible */
      gtk_tree_view_set_headers_visible (
        GTK_TREE_VIEW (tree_view),
        0);

      g_signal_connect (
        G_OBJECT (gtk_tree_view_get_selection (
                    GTK_TREE_VIEW (tree_view))),
        "changed",
         G_CALLBACK (on_selection_changed),
         self);

      gtk_widget_set_visible (
        GTK_WIDGET (tree_view), 1);
    }
}

PortSelectorPopoverWidget *
port_selector_popover_widget_new (
  PortConnectionsPopoverWidget * owner,
  Port * port)
{
  PortSelectorPopoverWidget * self =
    g_object_new (
      PORT_SELECTOR_POPOVER_WIDGET_TYPE, NULL);

  g_warn_if_fail (port);
  self->port = port;
  self->owner = owner;

  self->track_model =
    create_model_for_tracks (self);
  tree_view_setup (
    self,
    self->track_model,
    1);

  self->plugin_model =
    create_model_for_plugins (self, NULL);
  tree_view_setup (
    self,
    self->plugin_model,
    1);

  self->port_model =
    create_model_for_ports (
      self, NULL, NULL);
  tree_view_setup (
    self,
    self->port_model,
    1);

  return self;
}

static void
port_selector_popover_widget_class_init (
  PortSelectorPopoverWidgetClass * _klass)
{
  GtkWidgetClass * klass = GTK_WIDGET_CLASS (_klass);
  resources_set_class_template (
    klass, "port_selector_popover.ui");

  gtk_widget_class_bind_template_child (
    klass,
    PortSelectorPopoverWidget,
    track_treeview);
  gtk_widget_class_bind_template_child (
    klass,
    PortSelectorPopoverWidget,
    plugin_treeview);
  gtk_widget_class_bind_template_child (
    klass,
    PortSelectorPopoverWidget,
    plugin_separator);
  gtk_widget_class_bind_template_child (
    klass,
    PortSelectorPopoverWidget,
    port_treeview);
  gtk_widget_class_bind_template_child (
    klass,
    PortSelectorPopoverWidget,
    ok);
  gtk_widget_class_bind_template_child (
    klass,
    PortSelectorPopoverWidget,
    cancel);
}

static void
port_selector_popover_widget_init (
  PortSelectorPopoverWidget * self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (
    G_OBJECT (self->ok), "clicked",
    G_CALLBACK (on_ok_clicked), self);
  g_signal_connect (
    G_OBJECT (self->cancel), "clicked",
    G_CALLBACK (on_cancel_clicked), self);
}