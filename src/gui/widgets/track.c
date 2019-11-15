/*
 * Copyright (C) 2018-2019 Alexandros Theodotou <alex at zrythm dot org>
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "audio/automatable.h"
#include "audio/automation_track.h"
#include "audio/audio_bus_track.h"
#include "audio/audio_group_track.h"
#include "audio/master_track.h"
#include "audio/instrument_track.h"
#include "audio/track.h"
#include "audio/tracklist.h"
#include "audio/region.h"
#include "gui/backend/tracklist_selections.h"
#include "gui/widgets/arranger.h"
#include "gui/widgets/bot_bar.h"
#include "gui/widgets/bot_dock_edge.h"
#include "gui/widgets/center_dock.h"
#include "gui/widgets/color_area.h"
#include "gui/widgets/custom_button.h"
#include "gui/widgets/editable_label.h"
#include "gui/widgets/main_window.h"
#include "gui/widgets/midi_activity_bar.h"
#include "gui/widgets/mixer.h"
#include "gui/widgets/timeline_arranger.h"
#include "gui/widgets/timeline_bg.h"
#include "gui/widgets/timeline_panel.h"
#include "gui/widgets/track.h"
#include "gui/widgets/track_top_grid.h"
#include "gui/widgets/tracklist.h"
#include "project.h"
#include "utils/cairo.h"
#include "utils/flags.h"
#include "utils/gtk.h"
#include "utils/resources.h"
#include "utils/string.h"
#include "utils/ui.h"

#include <gtk/gtk.h>

#include <glib/gi18n.h>

G_DEFINE_TYPE (
  TrackWidget, track_widget, GTK_TYPE_BOX)

#define ICON_NAME_RECORD "z-media-record"
#define ICON_NAME_SOLO "solo"
#define ICON_NAME_MUTE "mute"
#define ICON_NAME_SHOW_UI "instrument"
#define ICON_NAME_SHOW_AUTOMATION_LANES \
  "z-node-type-cusp"
#define ICON_NAME_SHOW_TRACK_LANES \
  "z-format-justify-fill"
#define ICON_NAME_LOCK "z-object-unlocked"
#define ICON_NAME_FREEZE "snowflake-o"
#define ICON_NAME_WRITE_AUTOMATION "rw_read_write"
#define ICON_NAME_READ_AUTOMATION "r_read"
#define ICON_NAME_PLUS "plus"
#define ICON_NAME_MINUS "minus"

#define NAME_FONT "10"

/**
 * Width of each meter: total 8 for MIDI, total
 * 16 for audio.
 */
#define METER_WIDTH 8

#define BUTTON_SIZE 18

/** Padding between each button. */
#define BUTTON_PADDING 6

/** Padding between the track edges and the
 * buttons */
#define BUTTON_PADDING_FROM_EDGE 3

#define COLOR_AREA_WIDTH 18

/** Width of the "label" to show the automation
 * value. */
#define AUTOMATION_VALUE_WIDTH 32

#define BOT_BUTTONS_SHOULD_BE_VISIBLE(height) \
  (height >= \
     (BUTTON_SIZE + \
        BUTTON_PADDING_FROM_EDGE) * 2 + \
     BUTTON_PADDING)

static CustomButtonWidget *
get_hovered_button (
  TrackWidget * self,
  int           x,
  int           y)
{
#define IS_BUTTON_HOVERED \
  (x >= cb->x && \
   x <= cb->x + \
     (cb->width ? cb->width : BUTTON_SIZE) && \
   y >= cb->y && y <= cb->y + BUTTON_SIZE)
#define RETURN_IF_HOVERED \
  if (IS_BUTTON_HOVERED) return cb;

  CustomButtonWidget * cb = NULL;
  for (int i = 0; i < self->num_top_buttons; i++)
    {
      cb = self->top_buttons[i];
      RETURN_IF_HOVERED;
    }
  for (int i = 0; i < self->num_bot_buttons; i++)
    {
      cb = self->bot_buttons[i];
      RETURN_IF_HOVERED;
    }

  Track * track = self->track;
  if (track->lanes_visible)
    {
      for (int i = 0; i < track->num_lanes; i++)
        {
          TrackLane * lane = track->lanes[i];

          for (int j = 0; j < lane->num_buttons;
               j++)
            {
              cb = lane->buttons[j];
              RETURN_IF_HOVERED;
            }
        }
    }

  AutomationTracklist * atl =
    track_get_automation_tracklist (track);
  if (atl && track->automation_visible)
    {
      for (int i = 0; i < atl->num_ats; i++)
        {
          AutomationTrack * at = atl->ats[i];

          for (int j = 0;
               j < at->num_top_left_buttons;
               j++)
            {
              cb = at->top_left_buttons[j];
              RETURN_IF_HOVERED;
            }
          for (int j = 0;
               j < at->num_top_right_buttons;
               j++)
            {
              cb = at->top_right_buttons[j];
              RETURN_IF_HOVERED;
            }
          for (int j = 0;
               j < at->num_bot_left_buttons;
               j++)
            {
              cb = at->bot_left_buttons[j];
              RETURN_IF_HOVERED;
            }
          for (int j = 0;
               j < at->num_bot_right_buttons;
               j++)
            {
              cb = at->bot_right_buttons[j];
              RETURN_IF_HOVERED;
            }
        }
    }
  return NULL;
}

static void
draw_color_area (
  TrackWidget * self,
  cairo_t *     cr,
  int           width,
  int           height)
{
  cairo_surface_t * surface =
    z_cairo_get_surface_from_icon_name (
      self->icon_name, 16, 0);

  gdk_cairo_set_source_rgba (
    cr, &self->track->color);
  cairo_rectangle (
    cr, 0, 0, COLOR_AREA_WIDTH, height);
  cairo_fill (cr);

  GdkRGBA c2, c3;
  ui_get_contrast_color (
    &self->track->color, &c2);
  ui_get_contrast_color (
    &c2, &c3);

  /* add shadow in the back */
  cairo_set_source_rgba (
    cr, c3.red, c3.green, c3.blue, 0.4);
  cairo_mask_surface (
    cr, surface, 2, 2);
  cairo_fill (cr);

  /* add main icon */
  cairo_set_source_rgba (
    cr, c2.red, c2.green, c2.blue, 1);
  /*cairo_set_source_surface (*/
    /*self->cached_cr, surface, 1, 1);*/
  cairo_mask_surface(
    self->cached_cr, surface, 1, 1);
  cairo_fill (self->cached_cr);
}

static void
draw_name (
  TrackWidget * self,
  cairo_t *     cr)
{
  /* draw text */
  cairo_set_source_rgba (
    cr, 1, 1, 1, 1);
  cairo_move_to (cr, 22, 2);
  PangoLayout * layout = self->layout;
  pango_layout_set_text (
    layout, self->track->name, -1);
  pango_cairo_show_layout (cr, layout);
}

/**
 * @param top 1 to draw top, 0 to draw bottom.
 * @param width Track width.
 */
static void
draw_buttons (
  TrackWidget * self,
  cairo_t *     cr,
  int           top,
  int           width)
{
  CustomButtonWidget * hovered_cb =
    get_hovered_button (
      self, (int) self->last_x, (int) self->last_y);
  int num_buttons =
    top? self->num_top_buttons :
    self->num_bot_buttons;
  CustomButtonWidget ** buttons =
    top? self->top_buttons :
    self->bot_buttons;
  for (int i = 0; i < num_buttons; i++)
    {
      CustomButtonWidget * cb = buttons[i];

      if (top)
        {
          cb->x =
            width - (BUTTON_SIZE + BUTTON_PADDING) *
            (num_buttons - i);
          cb->y = BUTTON_PADDING_FROM_EDGE;
        }
      else
        {
          cb->x =
            width -
              (BUTTON_SIZE + BUTTON_PADDING) *
              (self->num_bot_buttons - i);
          cb->y =
            self->track->main_height -
              (BUTTON_PADDING_FROM_EDGE +
               BUTTON_SIZE);
        }

      CustomButtonWidgetState state =
        CUSTOM_BUTTON_WIDGET_STATE_NORMAL;

      if (cb == self->clicked_button)
        {
          /* currently clicked button */
          state =
            CUSTOM_BUTTON_WIDGET_STATE_ACTIVE;
        }
      else if (string_is_equal (
                cb->icon_name,
                ICON_NAME_SOLO, 1) &&
               self->track->solo)
        {
          state =
            CUSTOM_BUTTON_WIDGET_STATE_TOGGLED;
        }
      else if (string_is_equal (
                cb->icon_name,
                ICON_NAME_MUTE, 1) &&
               self->track->mute)
        {
          state =
            CUSTOM_BUTTON_WIDGET_STATE_TOGGLED;
        }
      else if (string_is_equal (
                cb->icon_name,
                ICON_NAME_RECORD, 1) &&
               self->track->recording)
        {
          state =
            CUSTOM_BUTTON_WIDGET_STATE_TOGGLED;
        }
      else if (string_is_equal (
                cb->icon_name,
                ICON_NAME_SHOW_TRACK_LANES, 1) &&
               self->track->lanes_visible)
        {
          state =
            CUSTOM_BUTTON_WIDGET_STATE_TOGGLED;
        }
      else if (string_is_equal (
                cb->icon_name,
                ICON_NAME_SHOW_AUTOMATION_LANES,
                1) &&
               self->track->automation_visible)
        {
          state =
            CUSTOM_BUTTON_WIDGET_STATE_TOGGLED;
        }
      else if (hovered_cb == cb)
        {
          state =
            CUSTOM_BUTTON_WIDGET_STATE_HOVERED;
        }

      if (state != cb->last_state)
        {
          /* add another cycle to draw transition of
           * 1 frame */
          self->redraw =
            CUSTOM_BUTTON_WIDGET_MAX_TRANSITION_FRAMES;
          track_widget_force_redraw (self);
        }
      custom_button_widget_draw (
        cb, cr, cb->x, cb->y, state);
    }
}

static void
draw_lanes (
  TrackWidget * self,
  cairo_t *     cr,
  int           width)
{
  Track * track = self->track;
  g_return_if_fail (track);

  if (!track->lanes_visible)
    return;

  for (int i = 0; i < track->num_lanes; i++)
    {
      TrackLane * lane = track->lanes[i];

      int top =
        track->main_height + i * lane->height;

      /* draw separator */
      cairo_set_source_rgba (
        cr, 1, 1, 1, 0.3);
      cairo_set_line_width (cr, 0.5);
      cairo_move_to (cr, COLOR_AREA_WIDTH, top);
      cairo_line_to (cr, width, top);
      cairo_stroke (cr);

      /* draw text */
      cairo_set_source_rgba (
        cr, 1, 1, 1, 1);
      cairo_move_to (
        cr,
        COLOR_AREA_WIDTH +
          BUTTON_PADDING_FROM_EDGE,
        top + BUTTON_PADDING_FROM_EDGE);
      PangoLayout * layout = self->layout;
      char str[50];
      sprintf (str, _("Lane %d"), i + 1);
      pango_layout_set_text (
        layout, str, -1);
      pango_cairo_show_layout (cr, layout);

      /* create buttons if necessary */
      CustomButtonWidget * cb;
      if (lane->num_buttons == 0)
        {
          lane->buttons[0] =
            custom_button_widget_new (
              ICON_NAME_SOLO, BUTTON_SIZE);
          cb = lane->buttons[0];
          cb->owner_type =
            CUSTOM_BUTTON_WIDGET_OWNER_LANE;
          cb->owner = lane;
          gdk_rgba_parse (
            &cb->toggled_color,
            UI_COLOR_SOLO_CHECKED);
          gdk_rgba_parse (
            &cb->held_color, UI_COLOR_SOLO_ACTIVE);
          lane->buttons[1] =
            custom_button_widget_new (
              ICON_NAME_MUTE, BUTTON_SIZE);
          cb = lane->buttons[1];
          cb->owner_type =
            CUSTOM_BUTTON_WIDGET_OWNER_LANE;
          cb->owner = lane;
          lane->num_buttons = 2;
        }

      /* draw buttons */
      CustomButtonWidget * hovered_cb =
        get_hovered_button (
          self, (int) self->last_x,
          (int) self->last_y);
      for (int j = 0; j < lane->num_buttons; j++)
        {
          cb =
            lane->buttons[j];

          cb->x =
            width - (BUTTON_SIZE + BUTTON_PADDING) *
            (lane->num_buttons - j);
          cb->y = top + BUTTON_PADDING_FROM_EDGE;

          CustomButtonWidgetState state =
            CUSTOM_BUTTON_WIDGET_STATE_NORMAL;

          if (cb == self->clicked_button)
            {
              /* currently clicked button */
              state =
                CUSTOM_BUTTON_WIDGET_STATE_ACTIVE;
            }
          else if (string_is_equal (
                    cb->icon_name,
                    ICON_NAME_SOLO, 1) &&
                   lane->solo)
            {
              state =
                CUSTOM_BUTTON_WIDGET_STATE_TOGGLED;
            }
          else if (string_is_equal (
                    cb->icon_name,
                    ICON_NAME_MUTE, 1) &&
                   lane->mute)
            {
              state =
                CUSTOM_BUTTON_WIDGET_STATE_TOGGLED;
            }
          else if (hovered_cb == cb)
            {
              state =
                CUSTOM_BUTTON_WIDGET_STATE_HOVERED;
            }

          if (state != cb->last_state)
            {
              /* add another cycle to draw
               * transition */
              self->redraw =
                CUSTOM_BUTTON_WIDGET_MAX_TRANSITION_FRAMES;
              track_widget_force_redraw (self);
            }
          custom_button_widget_draw (
            cb, cr, cb->x, cb->y, state);
        }
    }
}

static void
draw_automation (
  TrackWidget * self,
  cairo_t *     cr,
  int           width)
{
  Track * track = self->track;
  g_return_if_fail (track);

  if (!track->automation_visible)
    return;

  AutomationTracklist * atl =
    track_get_automation_tracklist (track);
  g_return_if_fail (atl);
  for (int i = 0; i < atl->num_ats; i++)
    {
      AutomationTrack * at = atl->ats[i];

      int top =
        track->main_height + i * at->height;

      if (track->lanes_visible)
        {
          for (int j = 0; j < track->num_lanes; j++)
            {
              TrackLane * lane = track->lanes[j];
              top += lane->height;
            }
        }

      /* draw separator */
      cairo_set_source_rgba (
        cr, 1, 1, 1, 0.3);
      cairo_set_line_width (cr, 0.5);
      cairo_move_to (cr, COLOR_AREA_WIDTH, top);
      cairo_line_to (cr, width, top);
      cairo_stroke (cr);

      /* create buttons if necessary */
      CustomButtonWidget * cb;
      if (at->num_top_left_buttons == 0)
        {
          at->top_left_buttons[0] =
            custom_button_widget_new (
              ICON_NAME_SHOW_AUTOMATION_LANES,
              BUTTON_SIZE);
          cb = at->top_left_buttons[0];
          cb->owner_type =
            CUSTOM_BUTTON_WIDGET_OWNER_AT;
          cb->owner = at;
          custom_button_widget_set_text (
            cb, self->layout,
            at->automatable->label);
          at->num_top_left_buttons = 1;
        }
      if (at->num_top_right_buttons == 0)
        {
          at->top_right_buttons[0] =
            custom_button_widget_new (
              ICON_NAME_MUTE, BUTTON_SIZE);
          at->top_right_buttons[0]->owner_type =
            CUSTOM_BUTTON_WIDGET_OWNER_AT;
          at->top_right_buttons[0]->owner = at;
          at->num_top_right_buttons = 1;
        }
      if (at->num_bot_left_buttons == 0)
        {
          at->bot_left_buttons[0] =
            custom_button_widget_new (
              ICON_NAME_WRITE_AUTOMATION,
              BUTTON_SIZE);
          cb = at->bot_left_buttons[0];
          gdk_rgba_parse (
            &cb->toggled_color,
            UI_COLOR_RECORD_CHECKED);
          gdk_rgba_parse (
            &cb->held_color,
            UI_COLOR_RECORD_ACTIVE);
          cb->owner_type =
            CUSTOM_BUTTON_WIDGET_OWNER_AT;
          cb->owner = at;
          at->bot_left_buttons[1] =
            custom_button_widget_new (
              ICON_NAME_READ_AUTOMATION,
              BUTTON_SIZE);
          cb = at->bot_left_buttons[1];
          cb->owner_type =
            CUSTOM_BUTTON_WIDGET_OWNER_AT;
          cb->owner = at;
          at->num_bot_left_buttons = 2;
        }
      if (at->num_bot_right_buttons == 0)
        {
          at->bot_right_buttons[0] =
            custom_button_widget_new (
              ICON_NAME_MINUS,
              BUTTON_SIZE);
          at->bot_right_buttons[0]->owner_type =
            CUSTOM_BUTTON_WIDGET_OWNER_AT;
          at->bot_right_buttons[0]->owner = at;
          at->bot_right_buttons[1] =
            custom_button_widget_new (
              ICON_NAME_PLUS,
              BUTTON_SIZE);
          at->bot_right_buttons[1]->owner_type =
            CUSTOM_BUTTON_WIDGET_OWNER_AT;
          at->bot_right_buttons[1]->owner = at;
          at->num_bot_right_buttons = 2;
        }

      /* draw buttons */
      CustomButtonWidget * hovered_cb =
        get_hovered_button (
          self, (int) self->last_x,
          (int) self->last_y);
      for (int j = 0; j < at->num_top_left_buttons;
           j++)
        {
          cb =
            at->top_left_buttons[j];

          cb->x =
            BUTTON_PADDING_FROM_EDGE +
            COLOR_AREA_WIDTH;
          cb->y = top + BUTTON_PADDING_FROM_EDGE;

          CustomButtonWidgetState state =
            CUSTOM_BUTTON_WIDGET_STATE_NORMAL;

          if (cb == self->clicked_button)
            {
              /* currently clicked button */
              state =
                CUSTOM_BUTTON_WIDGET_STATE_ACTIVE;
            }
          else if (hovered_cb == cb)
            {
              state =
                CUSTOM_BUTTON_WIDGET_STATE_HOVERED;
            }

          if (state != cb->last_state)
            {
              /* add another cycle to draw
               * transition */
              self->redraw =
                CUSTOM_BUTTON_WIDGET_MAX_TRANSITION_FRAMES;
              track_widget_force_redraw (self);
            }
          custom_button_widget_draw_with_text (
            cb, cr, cb->x, cb->y,
            width -
              (COLOR_AREA_WIDTH +
               BUTTON_PADDING_FROM_EDGE * 2 +
               at->num_top_right_buttons *
                 (BUTTON_SIZE + BUTTON_PADDING) +
               AUTOMATION_VALUE_WIDTH +
               BUTTON_PADDING),
            state);
        }

      /* draw automation value */
      PangoLayout * layout = self->layout;
      char str[50];
      sprintf (
        str, "%.2f",
        (double)
        automatable_get_val (at->automatable));
      cb = at->top_left_buttons[0];
      cairo_move_to (
        cr,
        cb->x + cb->width + BUTTON_PADDING,
        top + BUTTON_PADDING_FROM_EDGE);
      pango_layout_set_text (
        layout, str, -1);
      pango_cairo_show_layout (cr, layout);

      for (int j = 0; j < at->num_top_right_buttons;
           j++)
        {
          cb = at->top_right_buttons[j];

          cb->x =
            width - (BUTTON_SIZE + BUTTON_PADDING) *
            (at->num_top_right_buttons - j);
          cb->y = top + BUTTON_PADDING_FROM_EDGE;

          CustomButtonWidgetState state =
            CUSTOM_BUTTON_WIDGET_STATE_NORMAL;

          if (cb == self->clicked_button)
            {
              /* currently clicked button */
              state =
                CUSTOM_BUTTON_WIDGET_STATE_ACTIVE;
            }
          else if (hovered_cb == cb)
            {
              state =
                CUSTOM_BUTTON_WIDGET_STATE_HOVERED;
            }

          if (state != cb->last_state)
            {
              /* add another cycle to draw
               * transition */
              self->redraw =
                CUSTOM_BUTTON_WIDGET_MAX_TRANSITION_FRAMES;
              track_widget_force_redraw (self);
            }
          custom_button_widget_draw (
            cb, cr, cb->x, cb->y, state);
        }

      for (int j = 0; j < at->num_bot_left_buttons;
           j++)
        {
          cb =
            at->bot_left_buttons[j];

          cb->x =
            BUTTON_PADDING_FROM_EDGE +
            COLOR_AREA_WIDTH +
            j * (BUTTON_SIZE + BUTTON_PADDING);
          cb->y =
            (top + at->height) -
              (BUTTON_PADDING_FROM_EDGE +
               BUTTON_SIZE);

          CustomButtonWidgetState state =
            CUSTOM_BUTTON_WIDGET_STATE_NORMAL;

          if (cb == self->clicked_button)
            {
              /* currently clicked button */
              state =
                CUSTOM_BUTTON_WIDGET_STATE_ACTIVE;
            }
          else if (hovered_cb == cb)
            {
              state =
                CUSTOM_BUTTON_WIDGET_STATE_HOVERED;
            }

          if (state != cb->last_state)
            {
              /* add another cycle to draw
               * transition */
              self->redraw =
                CUSTOM_BUTTON_WIDGET_MAX_TRANSITION_FRAMES;
              track_widget_force_redraw (self);
            }
          custom_button_widget_draw (
            cb, cr, cb->x, cb->y, state);
        }

      for (int j = 0; j < at->num_bot_right_buttons;
           j++)
        {
          cb =
            at->bot_right_buttons[j];

          cb->x =
            width -
              (BUTTON_SIZE + BUTTON_PADDING) *
              (at->num_bot_right_buttons - j);
          cb->y =
            (top + at->height) -
              (BUTTON_PADDING_FROM_EDGE +
               BUTTON_SIZE);

          CustomButtonWidgetState state =
            CUSTOM_BUTTON_WIDGET_STATE_NORMAL;

          if (cb == self->clicked_button)
            {
              /* currently clicked button */
              state =
                CUSTOM_BUTTON_WIDGET_STATE_ACTIVE;
            }
          else if (hovered_cb == cb)
            {
              state =
                CUSTOM_BUTTON_WIDGET_STATE_HOVERED;
            }

          if (state != cb->last_state)
            {
              /* add another cycle to draw
               * transition */
              self->redraw =
                CUSTOM_BUTTON_WIDGET_MAX_TRANSITION_FRAMES;
              track_widget_force_redraw (self);
            }
          custom_button_widget_draw (
            cb, cr, cb->x, cb->y, state);
        }
    }
}

static int
track_draw_cb (
  GtkWidget *   widget,
  cairo_t *     cr,
  TrackWidget * self)
{
  g_message ("redrawing track (%d)", self->redraw);
  if (self->redraw)
    {
      GtkStyleContext *context =
        gtk_widget_get_style_context (widget);

      int width =
        gtk_widget_get_allocated_width (widget);
      int height =
        gtk_widget_get_allocated_height (widget);

      z_cairo_reset_caches (
        &self->cached_cr,
        &self->cached_surface, width,
        height, cr);

      gtk_render_background (
        context, self->cached_cr, 0, 0,
        width, height);

      if (self->bg_hovered)
        {
          cairo_set_source_rgba (
            self->cached_cr, 1, 1, 1, 0.1);
          cairo_rectangle (
            self->cached_cr, 0, 0, width, height);
          cairo_fill (self->cached_cr);
        }
      else if (track_is_selected (self->track))
        {
          cairo_set_source_rgba (
            self->cached_cr, 1, 1, 1, 0.07);
          cairo_rectangle (
            self->cached_cr, 0, 0, width, height);
          cairo_fill (self->cached_cr);
        }

      draw_color_area (
        self, self->cached_cr, width, height);

      draw_name (self, self->cached_cr);

      draw_buttons (
        self, self->cached_cr, 1, width);

      /* only show bot buttons if enough space */
      if (BOT_BUTTONS_SHOULD_BE_VISIBLE (
            self->track->main_height))
        {
          draw_buttons (
            self, self->cached_cr, 0, width);
        }

      draw_lanes (self, self->cached_cr, width);

      draw_automation (
        self, self->cached_cr, width);

      self->redraw--;

      /* finish redrawing the sequence */
      if (self->redraw)
        gtk_widget_queue_draw (widget);
    }

  cairo_set_source_surface (
    cr, self->cached_surface, 0, 0);
  cairo_paint (cr);

  return FALSE;
}

static gboolean
on_motion (
  GtkWidget *      widget,
  GdkEventMotion * event,
  TrackWidget *    self)
{
  int height =
    gtk_widget_get_allocated_height (widget);

  /* show resize cursor or not */
  if (self->bg_hovered)
    {
      CustomButtonWidget * cb =
        get_hovered_button (
          self, (int) event->x, (int) event->y);
      if ((!cb && height - event->y < 12) ||
          self->resizing)
        {
          self->resize = 1;
          ui_set_cursor_from_name (
            widget, "s-resize");
        }
      else
        {
          self->resize = 0;
          ui_set_pointer_cursor (widget);
        }
    }

  if (event->type == GDK_ENTER_NOTIFY)
    {
      g_message ("enter");
      self->bg_hovered = 1;
      self->resize = 0;
    }
  else if (event->type == GDK_LEAVE_NOTIFY)
    {
      g_message ("leave");
      ui_set_pointer_cursor (widget);
      if (!self->resizing)
        {
          self->bg_hovered = 0;
          self->resize = 0;
          self->button_pressed = 0;
        }
    }
  else
    {
      self->bg_hovered = 1;
    }
  track_widget_force_redraw (self);

  self->last_x = event->x;
  self->last_y = event->y;

  return FALSE;
}

/**
 * Wrapper.
 */
void
track_widget_force_redraw (
  TrackWidget * self)
{
  g_return_if_fail (self);
  self->redraw =
    MIN (self->redraw + 1, 10);
  gtk_widget_queue_draw (
    (GtkWidget *) self->drawing_area);
}

/**
 * Returns if cursor is in top half of the track.
 *
 * Used by timeline to determine if it will select
 * objects or range.
 */
int
track_widget_is_cursor_in_top_half (
  TrackWidget * self,
  double        y)
{
  gint wx, wy;
  gtk_widget_translate_coordinates (
    GTK_WIDGET (MW_TIMELINE),
    GTK_WIDGET (self),
    0,
    (int) y,
    &wx,
    &wy);

  /* if bot half */
  if (wy >= self->track->main_height / 2 &&
      wy <= self->track->main_height)
    {
      return 0;
    }
  else /* if top half */
    {
      return 1;
    }
}

static TrackLane *
get_lane_at_y (
  TrackWidget * self,
  double        y)
{
  Track * track = self->track;

  if (!track->lanes_visible)
    return NULL;

  TrackLane * lane = NULL;
  for (int i = 0; i < track->num_lanes; i++)
    {
      lane = track->lanes[i];

      g_message ("checking %d", i);
      if (lane->widget &&
          ui_is_child_hit (
            GTK_WIDGET (self),
            GTK_WIDGET (lane->widget),
            0, 1, 0, y, 0, 0))
        {
          return lane;
        }
    }

  return NULL;
}

/**
 * Info to pass when selecting a MIDI channel from
 * the context menu.
 */
typedef struct MidiChSelectionInfo
{
  /** Either one of these should be set. */
  Track *     track;
  TrackLane * lane;

  /** MIDI channel (1-16), or 0 for lane to
   * inherit. */
  midi_byte_t ch;
} MidiChSelectionInfo;

static void
on_midi_ch_selected (
  GtkMenuItem *         menu_item,
  MidiChSelectionInfo * info)
{
  if (info->lane)
    {
      info->lane->midi_ch = info->ch;
    }
  if (info->track)
    {
      info->track->midi_ch = info->ch;
    }
  free (info);
}

static void
show_context_menu (
  TrackWidget * self,
  double        y)
{
  GtkWidget *menu;
  GtkMenuItem *menuitem;
  menu = gtk_menu_new();
  Track * track = self->track;
  TrackLane * lane =
    get_lane_at_y (self, y);

#define APPEND(mi) \
  gtk_menu_shell_append ( \
    GTK_MENU_SHELL (menu), \
    GTK_WIDGET (menuitem));

  int num_selected =
    TRACKLIST_SELECTIONS->num_tracks;

  if (num_selected > 0)
    {
      char * str;

      if (track->type != TRACK_TYPE_MASTER &&
          track->type != TRACK_TYPE_CHORD &&
          track->type != TRACK_TYPE_MARKER)
        {
          /* delete track */
          if (num_selected == 1)
            str =
              g_strdup (_("_Delete Track"));
          else
            str =
              g_strdup (_("_Delete Tracks"));
          menuitem =
            z_gtk_create_menu_item (
              str,
              "z-delete",
              0,
              NULL,
              0,
              "win.delete-selected-tracks");
          g_free (str);
          APPEND (menuitem);

          /* duplicate track */
          if (num_selected == 1)
            str =
              g_strdup (_("_Duplicate Track"));
          else
            str =
              g_strdup (_("_Duplicate Tracks"));
          menuitem =
            z_gtk_create_menu_item (
              str,
              "z-edit-duplicate",
              0,
              NULL,
              0,
              "win.duplicate-selected-tracks");
          g_free (str);
          APPEND (menuitem);
        }

      /* add regions */
      if (track->type == TRACK_TYPE_INSTRUMENT)
        {
          menuitem =
            z_gtk_create_menu_item (
              _("Add Region"),
              "z-gtk-add",
              0,
              NULL,
              0,
              "win.duplicate-selected-tracks");
          APPEND (menuitem);
        }

      menuitem =
        z_gtk_create_menu_item (
          num_selected == 1 ?
            _("Hide Track") :
            _("Hide Tracks"),
          "z-gnumeric-column-hide",
          0,
          NULL,
          0,
          "win.hide-selected-tracks");
      APPEND (menuitem);

      menuitem =
        z_gtk_create_menu_item (
          num_selected == 1 ?
            _("Pin/Unpin Track") :
            _("Pin/Unpin Tracks"),
          "z-window-pin",
          0,
          NULL,
          0,
          "win.pin-selected-tracks");
      APPEND (menuitem);
    }

  /* add midi channel selectors */
  if (track_has_piano_roll (track))
    {
      menuitem =
        GTK_MENU_ITEM (
          gtk_menu_item_new_with_label (
            _("Track MIDI Ch")));

      GtkMenu * submenu =
        GTK_MENU (gtk_menu_new ());
      gtk_widget_set_visible (
        GTK_WIDGET (submenu), 1);
      GtkMenuItem * submenu_item;
      for (int i = 1; i <= 16; i++)
        {
          char * lbl =
            g_strdup_printf (
              _("%sMIDI Channel %d"),
              i == track->midi_ch ? "* " : "",
              i);
          submenu_item =
            GTK_MENU_ITEM (
              gtk_menu_item_new_with_label (lbl));
          g_free (lbl);

          MidiChSelectionInfo * info =
            calloc (
              1, sizeof (MidiChSelectionInfo));
          info->track = track;
          info->ch = (midi_byte_t) i;
          g_signal_connect (
            G_OBJECT (submenu_item), "activate",
            G_CALLBACK (on_midi_ch_selected),
            info);

          gtk_menu_shell_append (
            GTK_MENU_SHELL (submenu),
            GTK_WIDGET (submenu_item));
          gtk_widget_set_visible (
            GTK_WIDGET (submenu_item), 1);
        }

      gtk_menu_item_set_submenu (
        menuitem, GTK_WIDGET (submenu));
      gtk_widget_set_visible (
        GTK_WIDGET (menuitem), 1);

      APPEND (menuitem);

      if (lane)
        {
          char * lbl =
            g_strdup_printf (
              _("Lane %d MIDI Ch"),
              lane->pos);
          menuitem =
            GTK_MENU_ITEM (
              gtk_menu_item_new_with_label (
                lbl));
          g_free (lbl);

          submenu =
            GTK_MENU (gtk_menu_new ());
          gtk_widget_set_visible (
            GTK_WIDGET (submenu), 1);
          for (int i = 0; i <= 16; i++)
            {
              if (i == 0)
                lbl =
                  g_strdup_printf (
                    _("%sInherit"),
                    lane->midi_ch == i ? "* " : "");
              else
                lbl =
                  g_strdup_printf (
                    _("%sMIDI Channel %d"),
                    lane->midi_ch == i ? "* " : "",
                    i);
              submenu_item =
                GTK_MENU_ITEM (
                  gtk_menu_item_new_with_label (
                    lbl));
              g_free (lbl);

              MidiChSelectionInfo * info =
                calloc (
                  1, sizeof (MidiChSelectionInfo));
              info->lane = lane;
              info->ch = (midi_byte_t) i;
              g_signal_connect (
                G_OBJECT (submenu_item), "activate",
                G_CALLBACK (on_midi_ch_selected),
                info);

              gtk_menu_shell_append (
                GTK_MENU_SHELL (submenu),
                GTK_WIDGET (submenu_item));
              gtk_widget_set_visible (
                GTK_WIDGET (submenu_item), 1);
            }

          gtk_menu_item_set_submenu (
            menuitem, GTK_WIDGET (submenu));
          gtk_widget_set_visible (
            GTK_WIDGET (menuitem), 1);

          APPEND (menuitem);
        }
    }


#undef APPEND

  gtk_menu_attach_to_widget (
    GTK_MENU (menu),
    GTK_WIDGET (self), NULL);
  gtk_menu_popup_at_pointer (GTK_MENU (menu), NULL);
}

static void
on_right_click (
  GtkGestureMultiPress *gesture,
  gint                  n_press,
  gdouble               x,
  gdouble               y,
  TrackWidget *         self)
{

  GdkModifierType state_mask =
    ui_get_state_mask (GTK_GESTURE (gesture));

  Track * track = self->track;
  if (!track_is_selected (track))
    {
      if (state_mask & GDK_SHIFT_MASK ||
          state_mask & GDK_CONTROL_MASK)
        {
          track_select (
            track, F_SELECT, 0, 1);
        }
      else
        {
          track_select (
            track, F_SELECT, 1, 1);
        }
    }
  if (n_press == 1)
    {
      show_context_menu (self, y);
    }
}

static void
multipress_pressed (
  GtkGestureMultiPress *gesture,
  gint                  n_press,
  gdouble               x,
  gdouble               y,
  TrackWidget *         self)
{
  /* FIXME should do this via focus on click
   * property */
  /*if (!gtk_widget_has_focus (GTK_WIDGET (self)))*/
    /*gtk_widget_grab_focus (GTK_WIDGET (self));*/

  GdkModifierType state_mask =
    ui_get_state_mask (GTK_GESTURE (gesture));

  CustomButtonWidget * cb =
    get_hovered_button (self, (int) x, (int) y);
  if (cb)
    {
      self->button_pressed = 1;
      self->clicked_button = cb;
    }
  else
    {
      Track * track = self->track;

      PROJECT->last_selection =
        SELECTION_TYPE_TRACK;

      track_select (
        track,
        track_is_selected (track) &&
        state_mask & GDK_CONTROL_MASK ?
          F_NO_SELECT: F_SELECT,
        (state_mask & GDK_SHIFT_MASK ||
          state_mask & GDK_CONTROL_MASK) ?
          0 : 1,
        1);
    }

  track_widget_force_redraw (self);
}

static void
multipress_released (
  GtkGestureMultiPress *gesture,
  gint                  n_press,
  gdouble               x,
  gdouble               y,
  TrackWidget *         self)
{
  if (self->clicked_button)
    {
      CustomButtonWidget * cb =
        self->clicked_button;
      g_message ("owner %p ",
                 cb->owner);
      if ((Track *) cb->owner == self->track)
        {
          if (string_is_equal (
                cb->icon_name, ICON_NAME_RECORD, 1))
            {
              track_widget_on_record_toggled (self);
            }
          if (string_is_equal (
                cb->icon_name, ICON_NAME_SOLO, 1))
            {
              track_widget_on_solo_toggled (self);
            }
          else if (string_is_equal (
                cb->icon_name, ICON_NAME_MUTE, 1))
            {
              track_widget_on_mute_toggled (self);
            }
          else if (string_is_equal (
                cb->icon_name,
                ICON_NAME_SHOW_TRACK_LANES, 1))
            {
              track_widget_on_show_lanes_toggled (self);
            }
          else if (string_is_equal (
                cb->icon_name,
                ICON_NAME_SHOW_AUTOMATION_LANES, 1))
            {
              track_widget_on_show_automation_toggled (
                self);
            }
        }
      else if (cb->owner_type ==
                 CUSTOM_BUTTON_WIDGET_OWNER_LANE)
        {
          /*TrackLane * lane =*/
            /*(TrackLane *) cb->owner;*/

          /* TODO */
        }
      else if (cb->owner_type ==
                 CUSTOM_BUTTON_WIDGET_OWNER_AT)
        {
          AutomationTrack * at =
            (AutomationTrack *) cb->owner;
          g_return_if_fail (at);

          if (string_is_equal (
                cb->icon_name, ICON_NAME_PLUS, 1))
            {
              AutomationTracklist * atl =
                track_get_automation_tracklist (
                  at->track);
              g_return_if_fail (atl);
              AutomationTrack * new_at =
                automation_tracklist_get_first_invisible_at (
                  atl);

              /* if any invisible at exists, show
               * it */
              if (new_at)
                {
                  if (!new_at->created)
                    new_at->created = 1;
                  new_at->visible = 1;

                  EVENTS_PUSH (
                    ET_AUTOMATION_TRACK_ADDED,
                    new_at);
                }
            }
        }
    }

  self->button_pressed = 0;
  self->clicked_button = NULL;
  track_widget_force_redraw (self);
}

static void
on_drag_begin (GtkGestureDrag *gesture,
               gdouble         start_x,
               gdouble         start_y,
               TrackWidget * self)
{
  self->selected_in_dnd = 0;
  self->dragged = 0;

  if (self->resize)
    {
      /* start resizing */
      self->resizing = 1;
    }
  else if (self->button_pressed)
    {
      gtk_event_controller_reset (
        GTK_EVENT_CONTROLLER (gesture));
      /* if one of the buttons is pressed, ignore */
    }
  else
    {
      Track * track = self->track;
      self->selected_in_dnd = 1;
      MW_MIXER->start_drag_track = track;

      if (self->n_press == 1)
        {
          int ctrl = 0, selected = 0;

          ctrl = self->ctrl_held_at_start;

          if (tracklist_selections_contains_track (
                TRACKLIST_SELECTIONS,
                track))
            selected = 1;

          /* no control & not selected */
          if (!ctrl && !selected)
            {
              tracklist_selections_select_single (
                TRACKLIST_SELECTIONS,
                track);
            }
          else if (!ctrl && selected)
            { }
          else if (ctrl && !selected)
            tracklist_selections_add_track (
              TRACKLIST_SELECTIONS,
              track);
        }
    }

  self->start_x = start_x;
  self->start_y = start_y;
}

static void
on_drag_update (
  GtkGestureDrag * gesture,
  gdouble         offset_x,
  gdouble         offset_y,
  TrackWidget * self)
{
  g_message ("drag_update");

  self->dragged = 1;

  if (self->resizing)
    {
      /* resize */
      Track * track = self->track;
      int prev_height =
        track->main_height;
      /*int new_full_height =*/
        /*(int) (offset_y + self->start_y);*/
      int diff =
        (int) (offset_y - self->last_offset_y);
      track->main_height =
        MAX (
          TRACK_MIN_HEIGHT,
          prev_height + diff);
      g_message ("resizing %f %f",
                 offset_y, self->start_y);
      track_widget_update_size (self);
    }
  else
    {
      /* start dnd */
      char * entry_track =
        g_strdup (TARGET_ENTRY_TRACK);
      GtkTargetEntry entries[] = {
        {
          entry_track, GTK_TARGET_SAME_APP,
          symap_map (ZSYMAP, TARGET_ENTRY_TRACK),
        },
      };
      GtkTargetList * target_list =
        gtk_target_list_new (
          entries, G_N_ELEMENTS (entries));

      gtk_drag_begin_with_coordinates (
        (GtkWidget *) self, target_list,
        GDK_ACTION_MOVE | GDK_ACTION_COPY,
        (int) gtk_gesture_single_get_button (
          GTK_GESTURE_SINGLE (gesture)),
        NULL,
        (int) (self->start_x + offset_x),
        (int) (self->start_y + offset_y));

      g_free (entry_track);
    }

  self->last_offset_y = offset_y;
}

static void
on_drag_end (
  GtkGestureDrag *gesture,
  gdouble         offset_x,
  gdouble         offset_y,
  TrackWidget * self)
{
  self->resizing = 0;
  self->last_offset_y = 0;
}

static void
on_drag_data_get (
  GtkWidget        *widget,
  GdkDragContext   *context,
  GtkSelectionData *data,
  guint             info,
  guint             time,
  TrackWidget * self)
{
  /* Not really needed since the selections are
   * used. just send master */
  gtk_selection_data_set (
    data,
    gdk_atom_intern_static_string (
      TARGET_ENTRY_TRACK),
    32,
    (const guchar *) &P_MASTER_TRACK,
    sizeof (P_MASTER_TRACK));
}

/**
 * Highlights/unhighlights the Tracks
 * appropriately.
 *
 * @param highlight 1 to highlight top or bottom,
 *   0 to unhighlight all.
 */
void
track_widget_do_highlight (
  TrackWidget * self,
  gint          x,
  gint          y,
  const int     highlight)
{
  if (highlight)
    {
      /* if we are closer to the start of selection
       * than the end */
      int h =
        gtk_widget_get_allocated_height (
          GTK_WIDGET (self));
      if (y < h / 2)
        {
          /* highlight top */
          gtk_drag_highlight (
            GTK_WIDGET (
              self->highlight_top_box));
          gtk_widget_set_size_request (
            GTK_WIDGET (
              self->highlight_top_box),
            -1, 2);

          /* unhilight bot */
          gtk_drag_unhighlight (
            GTK_WIDGET (
              self->highlight_bot_box));
          gtk_widget_set_size_request (
            GTK_WIDGET (
              self->highlight_bot_box),
            -1, -1);
        }
      else
        {
          /* highlight bot */
          gtk_drag_highlight (
            GTK_WIDGET (
              self->highlight_bot_box));
          gtk_widget_set_size_request (
            GTK_WIDGET (
              self->highlight_bot_box),
            -1, 2);

          /* unhilight top */
          gtk_drag_unhighlight (
            GTK_WIDGET (
              self->highlight_top_box));
          gtk_widget_set_size_request (
            GTK_WIDGET (
              self->highlight_top_box),
            -1, -1);
        }
    }
  else
    {
      gtk_drag_unhighlight (
        GTK_WIDGET (
          self->highlight_top_box));
      gtk_widget_set_size_request (
        GTK_WIDGET (self->highlight_top_box),
        -1, -1);
      gtk_drag_unhighlight (
        GTK_WIDGET (
          self->highlight_bot_box));
      gtk_widget_set_size_request (
        GTK_WIDGET (self->highlight_bot_box),
        -1, -1);
    }
}

/**
 * Callback when lanes button is toggled.
 */
void
track_widget_on_show_lanes_toggled (
  TrackWidget * self)
{
  Track * track = self->track;

  /* set visibility flag */
  track_set_lanes_visible (
    track, !track->lanes_visible);
}

void
track_widget_on_show_automation_toggled (
  TrackWidget * self)
{
  Track * track = self->track;

  /* set visibility flag */
  track_set_automation_visible (
    track, !track->automation_visible);
}

void
track_widget_on_solo_toggled (
  TrackWidget * self)
{
  track_set_soloed (
    self->track, self->track->solo, 1);
}

/**
 * General handler for tracks that have mute
 * buttons.
 */
void
track_widget_on_mute_toggled (
  TrackWidget * self)
{
  track_set_muted (
    self->track, !self->track->mute, 1);
}

void
track_widget_on_record_toggled (
  TrackWidget * self)
{
  Track * track = self->track;
  ChannelTrack * ct = (ChannelTrack *) track;
  Channel * chan = ct->channel;

  /* toggle record flag */
  track_set_recording (track, !track->recording);
  chan->record_set_automatically = 0;
  g_message ("recording %d, %s",
             track->recording,
             track->name);

  EVENTS_PUSH (ET_TRACK_STATE_CHANGED,
               track);
}

static void
recreate_pango_layouts (
  TrackWidget * self,
  GdkRectangle * allocation)
{
  if (PANGO_IS_LAYOUT (self->layout))
    g_object_unref (self->layout);

  GtkWidget * widget =
    GTK_WIDGET (self->drawing_area);

  PangoFontDescription *desc;
  self->layout =
    gtk_widget_create_pango_layout (
      widget, NULL);
  desc =
    pango_font_description_from_string (
      NAME_FONT);
  pango_layout_set_font_description (
    self->layout, desc);
  pango_font_description_free (desc);
  pango_layout_set_ellipsize (
    self->layout, PANGO_ELLIPSIZE_END);
  if (allocation)
    {
      pango_layout_set_width (
        self->layout,
        pango_units_from_double (
          allocation->width - 20));
    }
}

static void
on_size_allocate (
  GtkWidget *    widget,
  GdkRectangle * allocation,
  TrackWidget * self)
{
  recreate_pango_layouts (self, allocation);
  track_widget_force_redraw (self);
}

static void
on_screen_changed (
  GtkWidget *    widget,
  GdkScreen *    previous_screen,
  TrackWidget * self)
{
  recreate_pango_layouts (self, NULL);
}

/**
 * Add a button.
 *
 * @param top 1 for top, 0 for bottom.
 */
static CustomButtonWidget *
add_button (
  TrackWidget * self,
  int           top,
  const char *  icon_name)
{
  CustomButtonWidget * cb =
    custom_button_widget_new (
      icon_name, BUTTON_SIZE);
  if (top)
    {
      self->top_buttons[
        self->num_top_buttons++] = cb;
    }
  else
    {
      self->bot_buttons[
        self->num_bot_buttons++] = cb;
    }

  cb->owner_type = CUSTOM_BUTTON_WIDGET_OWNER_TRACK;
  cb->owner = self->track;

  return cb;
}

static CustomButtonWidget *
add_solo_button (
  TrackWidget * self,
  int           top)
{
  CustomButtonWidget * cb =
    add_button (self, top, ICON_NAME_SOLO);
  gdk_rgba_parse (
    &cb->toggled_color,
    UI_COLOR_SOLO_CHECKED);
  gdk_rgba_parse (
    &cb->held_color, UI_COLOR_SOLO_ACTIVE);

  return cb;
}

static CustomButtonWidget *
add_record_button (
  TrackWidget * self,
  int           top)
{
  CustomButtonWidget * cb =
    add_button (self, top, ICON_NAME_RECORD);
  gdk_rgba_parse (
    &cb->toggled_color,
    UI_COLOR_RECORD_CHECKED);
  gdk_rgba_parse (
    &cb->held_color, UI_COLOR_RECORD_ACTIVE);

  return cb;
}

/**
 * Updates the full track size and redraws the
 * track.
 */
void
track_widget_update_size (
  TrackWidget * self)
{
  g_return_if_fail (self->track);
  int height =
    track_get_full_visible_height (self->track);
  int w;
  gtk_widget_get_size_request (
    (GtkWidget *) self, &w, NULL);
  g_return_if_fail (height > 1);
  gtk_widget_set_size_request (
    GTK_WIDGET (self), w, height);
  track_widget_force_redraw (self);
}

/**
 * Wrapper for child track widget.
 *
 * Sets color, draw callback, etc.
 */
TrackWidget *
track_widget_new (Track * track)
{
  g_return_val_if_fail (track, NULL);

  TrackWidget * self =
    g_object_new (
      TRACK_WIDGET_TYPE, NULL);

  self->track = track;

  switch (track->type)
    {
    case TRACK_TYPE_INSTRUMENT:
      strcpy (self->icon_name, "z-audio-midi");
      add_record_button (self, 1);
      add_solo_button (self, 1);
      add_button (
        self, 1, ICON_NAME_MUTE);
      add_button (
        self, 1, ICON_NAME_SHOW_UI);
      add_button (
        self, 0, ICON_NAME_LOCK);
      add_button (
        self, 0, ICON_NAME_FREEZE);
      add_button (
        self, 0, ICON_NAME_SHOW_TRACK_LANES);
      add_button (
        self, 0, ICON_NAME_SHOW_AUTOMATION_LANES);
      break;
    case TRACK_TYPE_MIDI:
      strcpy (self->icon_name, "z-audio-midi");
      add_record_button (self, 1);
      add_solo_button (self, 1);
      add_button (
        self, 1, ICON_NAME_MUTE);
      add_button (
        self, 0, ICON_NAME_LOCK);
      add_button (
        self, 0, ICON_NAME_SHOW_TRACK_LANES);
      add_button (
        self, 0, ICON_NAME_SHOW_AUTOMATION_LANES);
      break;
    case TRACK_TYPE_MASTER:
      strcpy (self->icon_name, "bus");
      add_solo_button (self, 1);
      add_button (
        self, 1, ICON_NAME_MUTE);
      add_button (
        self, 0, ICON_NAME_SHOW_AUTOMATION_LANES);
      break;
    case TRACK_TYPE_CHORD:
      strcpy (self->icon_name, "z-minuet-chords");
      add_record_button (self, 1);
      add_solo_button (self, 1);
      add_button (
        self, 1, ICON_NAME_MUTE);
      break;
    case TRACK_TYPE_MARKER:
      strcpy (
        self->icon_name,
        "z-kdenlive-show-markers");
      break;
    case TRACK_TYPE_AUDIO_BUS:
      strcpy (self->icon_name, "bus");
      add_solo_button (self, 1);
      add_button (
        self, 1, ICON_NAME_MUTE);
      add_button (
        self, 0, ICON_NAME_SHOW_AUTOMATION_LANES);
      break;
    case TRACK_TYPE_MIDI_BUS:
      strcpy (self->icon_name, "bus");
      add_solo_button (self, 1);
      add_button (
        self, 1, ICON_NAME_MUTE);
      add_button (
        self, 0, ICON_NAME_SHOW_AUTOMATION_LANES);
      break;
    case TRACK_TYPE_AUDIO_GROUP:
      strcpy (self->icon_name, "bus");
      add_solo_button (self, 1);
      add_button (
        self, 1, ICON_NAME_MUTE);
      add_button (
        self, 0, ICON_NAME_SHOW_AUTOMATION_LANES);
      break;
    case TRACK_TYPE_MIDI_GROUP:
      strcpy (self->icon_name, "bus");
      add_solo_button (self, 1);
      add_button (
        self, 1, ICON_NAME_MUTE);
      add_button (
        self, 0, ICON_NAME_SHOW_AUTOMATION_LANES);
      break;
    default:
      break;
    }

  track_widget_update_size (self);

  return self;
}

static void
on_destroy (
  TrackWidget * self)
{

  Track * track = self->track;

  CustomButtonWidget * cb = NULL;
  for (int i = 0; i < self->num_top_buttons; i++)
    {
      cb = self->top_buttons[i];
      custom_button_widget_free (cb);
    }
  for (int i = 0; i < self->num_bot_buttons; i++)
    {
      cb = self->bot_buttons[i];
      custom_button_widget_free (cb);
    }

  g_object_unref (self);

  track->widget = NULL;
}

static void
track_widget_init (TrackWidget * self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_set_vexpand_set (
    (GtkWidget *) self, 1);

  /* set font sizes */
  /*gtk_label_set_max_width_chars (*/
    /*self->top_grid->name->label, 14);*/
  /*gtk_label_set_width_chars (*/
    /*self->top_grid->name->label, 14);*/
  /*gtk_label_set_ellipsize (*/
    /*self->top_grid->name->label, PANGO_ELLIPSIZE_END);*/
  /*gtk_label_set_xalign (*/
    /*self->top_grid->name->label, 0);*/

  self->drag =
    GTK_GESTURE_DRAG (
      gtk_gesture_drag_new (
        GTK_WIDGET (self->drawing_area)));

  self->multipress =
    GTK_GESTURE_MULTI_PRESS (
      gtk_gesture_multi_press_new (
        GTK_WIDGET (self->drawing_area)));
  self->right_mouse_mp =
    GTK_GESTURE_MULTI_PRESS (
      gtk_gesture_multi_press_new (
        GTK_WIDGET (self->drawing_area)));
  gtk_gesture_single_set_button (
    GTK_GESTURE_SINGLE (self->right_mouse_mp),
    GDK_BUTTON_SECONDARY);

  /* make widget able to notify */
  gtk_widget_add_events (
    GTK_WIDGET (self->drawing_area),
    GDK_ALL_EVENTS_MASK);

  g_signal_connect (
    G_OBJECT (self->multipress), "pressed",
    G_CALLBACK (multipress_pressed), self);
  g_signal_connect (
    G_OBJECT (self->multipress), "released",
    G_CALLBACK (multipress_released), self);
  g_signal_connect (
    G_OBJECT (self->right_mouse_mp), "pressed",
    G_CALLBACK (on_right_click), self);
  g_signal_connect (
    G_OBJECT (self->drawing_area),
    "enter-notify-event",
    G_CALLBACK (on_motion),  self);
  g_signal_connect (
    G_OBJECT (self->drawing_area),
    "leave-notify-event",
    G_CALLBACK (on_motion),  self);
  g_signal_connect (
    G_OBJECT (self->drawing_area),
    "motion-notify-event",
    G_CALLBACK (on_motion),  self);
  g_signal_connect (
    G_OBJECT (self->drawing_area), "draw",
    G_CALLBACK (track_draw_cb),  self);
  g_signal_connect (
    G_OBJECT (self->drag), "drag-begin",
    G_CALLBACK (on_drag_begin), self);
  g_signal_connect (
    G_OBJECT (self->drag), "drag-update",
    G_CALLBACK (on_drag_update), self);
  g_signal_connect (
    G_OBJECT (self->drag), "drag-end",
    G_CALLBACK (on_drag_end), self);
  g_signal_connect (
    GTK_WIDGET (self),
    "drag-data-get",
    G_CALLBACK (on_drag_data_get), self);
  g_signal_connect (
    G_OBJECT(self), "destroy",
    G_CALLBACK (on_destroy),  NULL);
  g_signal_connect (
    G_OBJECT (self->drawing_area), "screen-changed",
    G_CALLBACK (on_screen_changed),  self);
  g_signal_connect (
    G_OBJECT (self->drawing_area), "size-allocate",
    G_CALLBACK (on_size_allocate),  self);

  g_object_ref (self);

  self->redraw = 1;
}

static void
track_widget_class_init (TrackWidgetClass * _klass)
{
  GtkWidgetClass * klass =
    GTK_WIDGET_CLASS (_klass);
  resources_set_class_template (
    klass, "track.ui");

#define BIND_CHILD(x) \
  gtk_widget_class_bind_template_child ( \
    klass, TrackWidget, x)

  BIND_CHILD (drawing_area);
  /*BIND_CHILD (color);*/
  /*BIND_CHILD (paned);*/
  /*BIND_CHILD (top_grid);*/
  /*BIND_CHILD (event_box);*/
  BIND_CHILD (highlight_top_box);
  BIND_CHILD (highlight_bot_box);
}
