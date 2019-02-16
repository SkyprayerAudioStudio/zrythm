/*
 * Copyright (C) 2019 Alexandros Theodotou
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

/**
 * \file
 *
 * Preferences window.
 */

#include "audio/engine.h"
#include "gui/widgets/preferences.h"
#include "settings/settings.h"
#include "utils/resources.h"
#include "zrythm.h"

#include <gtk/gtk.h>

G_DEFINE_TYPE (PreferencesWidget,
               preferences_widget,
               GTK_TYPE_DIALOG)

enum
{
  VALUE_COL,
  TEXT_COL
};

static GtkTreeModel *
create_audio_backend_model (void)
{
  const int values[NUM_ENGINE_BACKENDS] = {
    ENGINE_BACKEND_JACK,
    ENGINE_BACKEND_PORT_AUDIO,
  };
  const gchar *labels[NUM_ENGINE_BACKENDS] = {
    "Jack",
    "Port Audio",
  };

  GtkTreeIter iter;
  GtkListStore *store;
  gint i;

  store = gtk_list_store_new (NUM_ENGINE_BACKENDS,
                              G_TYPE_INT,
                              G_TYPE_STRING);

  for (i = 0; i < G_N_ELEMENTS (values); i++)
    {
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          VALUE_COL, values[i],
                          TEXT_COL, labels[i],
                          -1);
    }

  return GTK_TREE_MODEL (store);
}

static void
setup_audio (PreferencesWidget * self)
{
  GtkCellRenderer *renderer;
  gtk_combo_box_set_model (self->audio_backend,
                           create_audio_backend_model ());
  gtk_cell_layout_clear (GTK_CELL_LAYOUT (self->audio_backend));
  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (self->audio_backend), renderer, TRUE);
  gtk_cell_layout_set_attributes (
    GTK_CELL_LAYOUT (self->audio_backend), renderer,
    "text", TEXT_COL,
    NULL);

  gtk_combo_box_set_active (
    GTK_COMBO_BOX (self->audio_backend),
    g_settings_get_enum (
      S_PREFERENCES,
      "audio-backend"));
}


static void
on_cancel_clicked (GtkWidget * widget,
          gpointer    user_data)
{
  PreferencesWidget * self =
    Z_PREFERENCES_WIDGET (user_data);

  /* TODO confirmation */

  gtk_window_close (GTK_WINDOW (self));
}

static void
on_ok_clicked (GtkWidget * widget,
                gpointer    user_data)
{
  PreferencesWidget * self =
    Z_PREFERENCES_WIDGET (user_data);

  g_settings_set_enum (
    S_PREFERENCES,
    "audio-backend",
    gtk_combo_box_get_active (self->audio_backend));

  gtk_window_close (GTK_WINDOW (self));
}

/**
 * Sets up the preferences widget.
 */
PreferencesWidget *
preferences_widget_new ()
{
  PreferencesWidget * self =
    g_object_new (PREFERENCES_WIDGET_TYPE,
                  NULL);

  setup_audio (self);

  return self;
}

static void
preferences_widget_class_init (
  PreferencesWidgetClass * _klass)
{
  GtkWidgetClass * klass = GTK_WIDGET_CLASS (_klass);
  resources_set_class_template (klass,
                                "preferences.ui");

  gtk_widget_class_bind_template_child (
    klass,
    PreferencesWidget,
    categories);
  gtk_widget_class_bind_template_child (
    klass,
    PreferencesWidget,
    ok);
  gtk_widget_class_bind_template_child (
    klass,
    PreferencesWidget,
    audio_backend);
  gtk_widget_class_bind_template_callback (
    klass,
    on_ok_clicked);
  gtk_widget_class_bind_template_callback (
    klass,
    on_cancel_clicked);
}

static void
preferences_widget_init (PreferencesWidget * self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}