/*
 * Copyright (C) 2018-2019 Alexandros Theodotou <alex at zrythm dot org>
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
 * Chord related code.
 * See https://www.scales-chords.com
 * https://www.basicmusictheory.com/c-harmonic-minor-triad-chords
 */

#ifndef __AUDIO_CHORD_H__
#define __AUDIO_CHORD_H__

#include <stdint.h>

#include "audio/position.h"
#include "gui/backend/arranger_object_info.h"
#include "utils/yaml.h"

/**
 * @addtogroup audio
 *
 * @{
 */

/** Gets the main counterpart of the ZChord. */
#define chord_get_main_chord(r) \
  ((ZChord *) r->obj_info.main)

/** Gets the transient counterpart of the ZChord. */
#define chord_get_main_trans_chord(r) \
  ((ZChord *) r->obj_info.main_trans)

typedef enum MusicalNote
{
  NOTE_C,
  NOTE_CS,
  NOTE_D,
  NOTE_DS,
  NOTE_E,
  NOTE_F,
  NOTE_FS,
  NOTE_G,
  NOTE_GS,
  NOTE_A,
  NOTE_AS,
  NOTE_B
} MusicalNote;

#define NOTE_LABELS \
static const char * note_labels[12] = { \
  "C", \
  "D\u266D", \
  "D", \
  "E\u266D", \
  "E", \
  "F", \
  "F\u266F", \
  "G", \
  "A\u266D", \
  "A", \
  "B\u266D", \
  "B" }

/**
 * Chord type.
 */
typedef enum ChordType
{
  CHORD_TYPE_MAJ,
  CHORD_TYPE_MIN,
  CHORD_TYPE_7,
  CHORD_TYPE_MIN_7,
  CHORD_TYPE_MAJ_7,
  CHORD_TYPE_6,
  CHORD_TYPE_MIN_6,
  CHORD_TYPE_6_9,
  CHORD_TYPE_5,
  CHORD_TYPE_9,
  CHORD_TYPE_MIN_9,
  CHORD_TYPE_MAJ_9,
  CHORD_TYPE_11,
  CHORD_TYPE_MIN_11,
  CHORD_TYPE_13,
  CHORD_TYPE_ADD_2,
  CHORD_TYPE_ADD_9,
  CHORD_TYPE_7_MINUS_5,
  CHORD_TYPE_7_PLUS_5,
  CHORD_TYPE_SUS2,
  CHORD_TYPE_SUS4,
  CHORD_TYPE_DIM,
  CHORD_TYPE_AUG
} ChordType;

typedef struct _ChordWidget ChordWidget;

/**
 * Chords are to be generated on demand.
 */
typedef struct ZChord
{
  /** ZChord object position (if used in chord
   * Track). */
  Position       pos;

  /** Cache, used in runtime operations. */
  Position       cache_pos;

  /** Has bass note or not. */
  int            has_bass;

  /** Root note. */
  MusicalNote    root_note;

  /** Bass note 1 octave below. */
  MusicalNote    bass_note;

  ChordType      type;

  /** Position of Track this ZChord is in. */
  int            track_pos;

  /** Cache. */
  Track *        track;

  /**
   * 3 octaves, 1st octave is for bass note.
   *
   * Starts at C always.
   */
  int            notes[36];

  /**
   * 0 no inversion,
   * less than 0 highest note(s) drop an octave,
   * greater than 0 lwest note(s) receive an octave.
   */
  int                   inversion;

  /** Selected in TimelineArrangerWidget or not. */
  int                   selected;
  int                   visible;
  ChordWidget *         widget;

  ArrangerObjectInfo    obj_info;
} ZChord;

static const cyaml_strval_t musical_note_strings[] = {
	{ "C",          NOTE_C    },
	{ "C#",         NOTE_CS   },
	{ "D",          NOTE_D   },
	{ "D#",         NOTE_DS   },
	{ "E",          NOTE_E   },
	{ "F",          NOTE_F   },
	{ "F#",         NOTE_FS   },
	{ "G",          NOTE_G   },
	{ "G#",         NOTE_GS   },
	{ "A",          NOTE_A   },
	{ "A#",         NOTE_AS   },
	{ "B",          NOTE_B   },
};

static const cyaml_schema_field_t
  chord_fields_schema[] =
{
  CYAML_FIELD_MAPPING (
    "pos", CYAML_FLAG_DEFAULT,
    ZChord, pos, position_fields_schema),
	CYAML_FIELD_INT (
    "has_bass", CYAML_FLAG_DEFAULT,
    ZChord, has_bass),
  CYAML_FIELD_ENUM (
    "root_note", CYAML_FLAG_DEFAULT,
    ZChord, root_note, musical_note_strings,
    CYAML_ARRAY_LEN (musical_note_strings)),
  CYAML_FIELD_ENUM (
    "bass_note", CYAML_FLAG_DEFAULT,
    ZChord, bass_note, musical_note_strings,
    CYAML_ARRAY_LEN (musical_note_strings)),
  CYAML_FIELD_SEQUENCE_FIXED (
    "notes", CYAML_FLAG_OPTIONAL,
    ZChord, notes, &int_schema, 36),
	CYAML_FIELD_INT (
    "inversion", CYAML_FLAG_DEFAULT,
    ZChord, inversion),

	CYAML_FIELD_END
};

static const cyaml_schema_value_t
chord_schema = {
	CYAML_VALUE_MAPPING(CYAML_FLAG_POINTER,
			ZChord, chord_fields_schema),
};

void
chord_init_loaded (ZChord * self);

/**
 * Creates a chord.
 */
ZChord *
chord_new (MusicalNote            root,
           uint8_t                has_bass,
           MusicalNote            bass,
           ChordType              type,
           int                    inversion);

static inline int
_notes_equal (
  int * notes_a,
  int * notes_b)
{
  /* 36 notes in Chord */
  for (int i = 0; i < 36; i++)
    {
      if (notes_a[i] != notes_b[i])
        return 0;
    }
  return 1;
}

static inline int
chord_is_equal (
  ZChord * a,
  ZChord * b)
{
  return !position_compare(&a->pos, &b->pos) &&
    a->has_bass == b->has_bass &&
    a->root_note == b->root_note &&
    a->bass_note == b->bass_note &&
    a->type == b->type &&
    _notes_equal (a->notes, b->notes) &&
    a->inversion == b->inversion &&
    a->visible == b->visible;
}

/**
 * Returns the Track this ZChord is in.
 */
Track *
chord_get_track (
  ZChord * self);

/**
 * Finds the chord in the project corresponding to the
 * given one.
 */
ZChord *
chord_find (
  ZChord * clone);

/**
 * Returns the musical note as a string (eg. "C3").
 */
const char *
chord_note_to_string (MusicalNote note);

/**
 * Returns the chord in human readable string.
 *
 * MUST be free'd by caller.
 */
char *
chord_as_string (ZChord * chord);

void
chord_set_pos (ZChord *    self,
               Position * pos);

/**
 * Moves the Region by the given amount of ticks.
 *
 * @param use_cached_pos Add the ticks to the cached
 *   Position instead of its current Position.
 * @param trans_only Only move transients.
 * @return Whether moved or not.
 */
int
chord_move (
  ZChord * chord,
  long     ticks,
  int      use_cached_pos,
  int      trans_only);

void
chord_free (ZChord * self);

/**
 * @}
 */

#endif
