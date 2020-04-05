/*
 * Copyright (C) 2020 Alexandros Theodotou <alex at zrythm dot org>
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#if !defined(SCM_MAGIC_SNARF_DOCS) && \
  !defined(SCM_MAGIC_SNARFER)
#include "audio/position.h"
#include "guile/audio/position.h"
#endif

#include <libguile.h>

static SCM position_type;

static void
init_position_type (void)
{
  SCM name = scm_from_utf8_symbol ("position");
  SCM slots =
    scm_list_1 (
      scm_from_utf8_symbol ("data"));
  scm_t_struct_finalize finalizer = NULL;

  position_type =
    scm_make_foreign_object_type (
      name, slots, finalizer);
}

SCM_DEFINE (
  make_position, "position-new", 5, 0, 0,
  (SCM bars, SCM beats, SCM sixteenths,
   SCM ticks, SCM sub_tick),
  "Return a newly-created position as @var{bars}.\
@var{beats}.@var{sixteenths}.@var{ticks}.\
@var{sub_tick}.")
#define FUNC_NAME s_
{
  /* Allocate the Position.  Because we
     use scm_gc_malloc, this memory block will
     be automatically reclaimed when it becomes
     inaccessible, and its members will be traced
     by the garbage collector.  */
  Position * pos =
    (Position *)
    scm_gc_malloc (sizeof (Position), "position");

  pos->bars = scm_to_int (bars);
  pos->beats = scm_to_int (beats);
  pos->sixteenths = scm_to_int (sixteenths);
  pos->ticks = scm_to_int (ticks);
  pos->sub_tick = scm_to_double (sub_tick);
  position_update_ticks_and_frames (pos);

  /* wrap the Position * in a new foreign object
   * and return that object */
  return
    scm_make_foreign_object_1 (
      position_type, pos);
}
#undef FUNC_NAME

SCM_DEFINE (
  print_position, "position-print", 1, 0, 0,
  (SCM pos),
  "Prints the position as `bars.beats.sixteenths.\
ticks.sub_tick`.")
#define FUNC_NAME s_
{
  scm_assert_foreign_object_type (
    position_type, pos);
  Position * refpos =
    scm_foreign_object_ref (pos, 0);

  char buf[120];
  position_stringize (refpos, buf);

  SCM out_port = scm_current_output_port ();
  scm_display (
    scm_from_utf8_string (buf),
    out_port);

  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME

static void
init_module (void * data)
{
  init_position_type ();

#ifndef SCM_MAGIC_SNARFER
#include "audio_position.x"
#endif
  scm_c_export (
    "position-new", "position-print", NULL);
}

void
guile_audio_position_define_module (void)
{
  scm_c_define_module (
    "audio position", init_module, NULL);
}
