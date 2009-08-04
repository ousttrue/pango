/*
 * Copyright (C) 2007,2008,2009  Red Hat, Inc.
 *
 *  This is part of HarfBuzz, an OpenType Layout engine library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Red Hat Author(s): Behdad Esfahbod
 */

#ifndef HB_OT_LAYOUT_GSUBGPOS_PRIVATE_HH
#define HB_OT_LAYOUT_GSUBGPOS_PRIVATE_HH

#include "hb-buffer-private.h"
#include "hb-ot-layout-gdef-private.hh"


#define APPLY_ARG_DEF \
	hb_ot_layout_context_t *context, \
	hb_buffer_t    *buffer, \
	unsigned int    context_length HB_GNUC_UNUSED, \
	unsigned int    nesting_level_left HB_GNUC_UNUSED, \
	unsigned int    lookup_flag, \
	unsigned int    property HB_GNUC_UNUSED /* propety of first glyph */
#define APPLY_ARG \
	context, \
	buffer, \
	context_length, \
	nesting_level_left, \
	lookup_flag, \
	property


typedef bool (*match_func_t) (hb_codepoint_t glyph_id, const USHORT &value, char *data);
typedef bool (*apply_lookup_func_t) (APPLY_ARG_DEF, unsigned int lookup_index);

struct ContextFuncs
{
  match_func_t match;
  apply_lookup_func_t apply;
};


static inline bool match_glyph (hb_codepoint_t glyph_id, const USHORT &value, char *data)
{
  return glyph_id == value;
}

static inline bool match_class (hb_codepoint_t glyph_id, const USHORT &value, char *data)
{
  const ClassDef &class_def = *(const ClassDef *)data;
  return class_def.get_class (glyph_id) == value;
}

static inline bool match_coverage (hb_codepoint_t glyph_id, const USHORT &value, char *data)
{
  const OffsetTo<Coverage> &coverage = (const OffsetTo<Coverage>&)value;
  return (data+coverage) (glyph_id) != NOT_COVERED;
}


static inline bool match_input (APPLY_ARG_DEF,
				unsigned int count, /* Including the first glyph (not matched) */
				const USHORT input[], /* Array of input values--start with second glyph */
				match_func_t match_func,
				char *match_data,
				unsigned int *context_length_out)
{
  unsigned int i, j;
  unsigned int end = MIN (buffer->in_length, buffer->in_pos + context_length);
  if (HB_UNLIKELY (buffer->in_pos + count > end))
    return false;

  for (i = 1, j = buffer->in_pos + 1; i < count; i++, j++)
  {
    while (_hb_ot_layout_skip_mark (context->face, IN_INFO (j), lookup_flag, NULL))
    {
      if (HB_UNLIKELY (j + count - i == end))
	return false;
      j++;
    }

    if (HB_LIKELY (!match_func (IN_GLYPH (j), input[i - 1], match_data)))
      return false;
  }

  *context_length_out = j - buffer->in_pos;

  return true;
}

static inline bool match_backtrack (APPLY_ARG_DEF,
				    unsigned int count,
				    const USHORT backtrack[],
				    match_func_t match_func,
				    char *match_data)
{
  if (HB_UNLIKELY (buffer->out_pos < count))
    return false;

  for (unsigned int i = 0, j = buffer->out_pos - 1; i < count; i++, j--)
  {
    while (_hb_ot_layout_skip_mark (context->face, OUT_INFO (j), lookup_flag, NULL))
    {
      if (HB_UNLIKELY (j + 1 == count - i))
	return false;
      j--;
    }

    if (HB_LIKELY (!match_func (OUT_GLYPH (j), backtrack[i], match_data)))
      return false;
  }

  return true;
}

static inline bool match_lookahead (APPLY_ARG_DEF,
				    unsigned int count,
				    const USHORT lookahead[],
				    match_func_t match_func,
				    char *match_data,
				    unsigned int offset)
{
  unsigned int i, j;
  unsigned int end = MIN (buffer->in_length, buffer->in_pos + context_length);
  if (HB_UNLIKELY (buffer->in_pos + offset + count > end))
    return false;

  for (i = 0, j = buffer->in_pos + offset; i < count; i++, j++)
  {
    while (_hb_ot_layout_skip_mark (context->face, OUT_INFO (j), lookup_flag, NULL))
    {
      if (HB_UNLIKELY (j + count - i == end))
	return false;
      j++;
    }

    if (HB_LIKELY (!match_func (IN_GLYPH (j), lookahead[i], match_data)))
      return false;
  }

  return true;
}


struct LookupRecord
{
  public:
  inline bool sanitize (SANITIZE_ARG_DEF) {
    return SANITIZE_SELF ();
  }

  USHORT	sequenceIndex;		/* Index into current glyph
					 * sequence--first glyph = 0 */
  USHORT	lookupListIndex;	/* Lookup to apply to that
					 * position--zero--based */
};
ASSERT_SIZE (LookupRecord, 4);

static inline bool apply_lookup (APPLY_ARG_DEF,
				 unsigned int count, /* Including the first glyph */
				 unsigned int lookupCount,
				 const LookupRecord lookupRecord[], /* Array of LookupRecords--in design order */
				 apply_lookup_func_t apply_func)
{
  unsigned int end = MIN (buffer->in_length, buffer->in_pos + context_length);
  if (HB_UNLIKELY (buffer->in_pos + count > end))
    return false;

  /* TODO We don't support lookupRecord arrays that are not increasing:
   *      Should be easy for in_place ones at least. */
  for (unsigned int i = 0; i < count; i++)
  {
    while (_hb_ot_layout_skip_mark (context->face, IN_CURINFO (), lookup_flag, NULL))
    {
      if (HB_UNLIKELY (buffer->in_pos == end))
	return true;
      /* No lookup applied for this index */
      _hb_buffer_next_glyph (buffer);
    }

    if (lookupCount && i == lookupRecord->sequenceIndex)
    {
      unsigned int old_pos = buffer->in_pos;

      /* Apply a lookup */
      bool done = apply_func (APPLY_ARG, lookupRecord->lookupListIndex);

      lookupRecord++;
      lookupCount--;
      i += buffer->in_pos - old_pos;
      if (HB_UNLIKELY (buffer->in_pos == end))
	return true;

      if (!done)
	goto not_applied;
    }
    else
    {
    not_applied:
      /* No lookup applied for this index */
      _hb_buffer_next_glyph (buffer);
      i++;
    }
  }

  return true;
}


/* Contextual lookups */

struct ContextLookupContext
{
  ContextFuncs funcs;
  char *match_data;
};

static inline bool context_lookup (APPLY_ARG_DEF,
				   unsigned int inputCount, /* Including the first glyph (not matched) */
				   const USHORT input[], /* Array of input values--start with second glyph */
				   unsigned int lookupCount,
				   const LookupRecord lookupRecord[],
				   ContextLookupContext &lookup_context)
{
  return match_input (APPLY_ARG,
		      inputCount, input,
		      lookup_context.funcs.match, lookup_context.match_data,
		      &context_length) &&
	 apply_lookup (APPLY_ARG,
		       inputCount,
		       lookupCount, lookupRecord,
		       lookup_context.funcs.apply);
}

struct Rule
{
  friend struct RuleSet;

  private:
  inline bool apply (APPLY_ARG_DEF, ContextLookupContext &lookup_context) const
  {
    const LookupRecord *lookupRecord = (const LookupRecord *)
				       (CONST_CHARP(input) +
					sizeof (input[0]) * (inputCount ? inputCount - 1 : 0));
    return context_lookup (APPLY_ARG,
			   inputCount, input,
			   lookupCount, lookupRecord,
			   lookup_context);
  }

  public:
  inline bool sanitize (SANITIZE_ARG_DEF) {
    if (!SANITIZE_SELF ()) return false;
    return SANITIZE_MEM (input,
			 sizeof (input[0]) * inputCount +
			 sizeof (lookupRecordX[0]) * lookupCount);
  }

  private:
  USHORT	inputCount;		/* Total number of glyphs in input
					 * glyph sequence--includes the  first
					 * glyph */
  USHORT	lookupCount;		/* Number of LookupRecords */
  USHORT	input[];		/* Array of match inputs--start with
					 * second glyph */
  LookupRecord	lookupRecordX[];	/* Array of LookupRecords--in
					 * design order */
};
ASSERT_SIZE (Rule, 4);

struct RuleSet
{
  inline bool apply (APPLY_ARG_DEF, ContextLookupContext &lookup_context) const
  {
    unsigned int num_rules = rule.len;
    for (unsigned int i = 0; i < num_rules; i++)
    {
      if ((this+rule[i]).apply (APPLY_ARG, lookup_context))
        return true;
    }

    return false;
  }

  inline bool sanitize (SANITIZE_ARG_DEF) {
    return SANITIZE_THIS (rule);
  }

  private:
  OffsetArrayOf<Rule>
		rule;			/* Array of Rule tables
					 * ordered by preference */
};


struct ContextFormat1
{
  friend struct Context;

  private:
  inline bool apply (APPLY_ARG_DEF, apply_lookup_func_t apply_func) const
  {
    unsigned int index = (this+coverage) (IN_CURGLYPH ());
    if (HB_LIKELY (index == NOT_COVERED))
      return false;

    const RuleSet &rule_set = this+ruleSet[index];
    struct ContextLookupContext lookup_context = {
      {match_glyph, apply_func},
      NULL
    };
    return rule_set.apply (APPLY_ARG, lookup_context);
  }

  inline bool sanitize (SANITIZE_ARG_DEF) {
    return SANITIZE_THIS2 (coverage, ruleSet);
  }

  private:
  USHORT	format;			/* Format identifier--format = 1 */
  OffsetTo<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of table */
  OffsetArrayOf<RuleSet>
		ruleSet;		/* Array of RuleSet tables
					 * ordered by Coverage Index */
};
ASSERT_SIZE (ContextFormat1, 6);


struct ContextFormat2
{
  friend struct Context;

  private:
  inline bool apply (APPLY_ARG_DEF, apply_lookup_func_t apply_func) const
  {
    unsigned int index = (this+coverage) (IN_CURGLYPH ());
    if (HB_LIKELY (index == NOT_COVERED))
      return false;

    const ClassDef &class_def = this+classDef;
    index = class_def (IN_CURGLYPH ());
    const RuleSet &rule_set = this+ruleSet[index];
    /* LONGTERMTODO: Old code fetches glyph classes at most once and caches
     * them across subrule lookups.  Not sure it's worth it.
     */
    struct ContextLookupContext lookup_context = {
     {match_class, apply_func},
      CHARP(&class_def)
    };
    return rule_set.apply (APPLY_ARG, lookup_context);
  }

  inline bool sanitize (SANITIZE_ARG_DEF) {
    return SANITIZE_THIS3 (coverage, classDef, ruleSet);
  }

  private:
  USHORT	format;			/* Format identifier--format = 2 */
  OffsetTo<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of table */
  OffsetTo<ClassDef>
		classDef;		/* Offset to glyph ClassDef table--from
					 * beginning of table */
  OffsetArrayOf<RuleSet>
		ruleSet;		/* Array of RuleSet tables
					 * ordered by class */
};
ASSERT_SIZE (ContextFormat2, 8);


struct ContextFormat3
{
  friend struct Context;

  private:
  inline bool apply (APPLY_ARG_DEF, apply_lookup_func_t apply_func) const
  {
    unsigned int index = (this+coverage[0]) (IN_CURGLYPH ());
    if (HB_LIKELY (index == NOT_COVERED))
      return false;

    const LookupRecord *lookupRecord = (const LookupRecord *)
				       (CONST_CHARP(coverage) +
					sizeof (coverage[0]) * glyphCount);
    struct ContextLookupContext lookup_context = {
      {match_coverage, apply_func},
      CHARP(this)
    };
    return context_lookup (APPLY_ARG,
			   glyphCount, (const USHORT *) (coverage + 1),
			   lookupCount, lookupRecord,
			   lookup_context);
  }

  inline bool sanitize (SANITIZE_ARG_DEF) {
    if (!SANITIZE_SELF ()) return false;
    unsigned int count = glyphCount;
    for (unsigned int i = 0; i < count; i++)
      if (!SANITIZE_THIS (coverage[i])) return false;
    LookupRecord *lookupRecord = (LookupRecord *)
				 (CHARP(coverage) +
				  sizeof (coverage[0]) * glyphCount);
    return SANITIZE_MEM (lookupRecord, sizeof (lookupRecord[0]) * lookupCount);
  }

  private:
  USHORT	format;			/* Format identifier--format = 3 */
  USHORT	glyphCount;		/* Number of glyphs in the input glyph
					 * sequence */
  USHORT	lookupCount;		/* Number of LookupRecords */
  OffsetTo<Coverage>
		coverage[];		/* Array of offsets to Coverage
					 * table in glyph sequence order */
  LookupRecord	lookupRecordX[];	/* Array of LookupRecords--in
					 * design order */
};
ASSERT_SIZE (ContextFormat3, 6);

struct Context
{
  protected:
  bool apply (APPLY_ARG_DEF, apply_lookup_func_t apply_func) const
  {
    switch (u.format) {
    case 1: return u.format1->apply (APPLY_ARG, apply_func);
    case 2: return u.format2->apply (APPLY_ARG, apply_func);
    case 3: return u.format3->apply (APPLY_ARG, apply_func);
    default:return false;
    }
  }

  inline bool sanitize (SANITIZE_ARG_DEF) {
    if (!SANITIZE (u.format)) return false;
    switch (u.format) {
    case 1: return u.format1->sanitize (SANITIZE_ARG);
    case 2: return u.format2->sanitize (SANITIZE_ARG);
    case 3: return u.format3->sanitize (SANITIZE_ARG);
    default:return true;
    }
  }

  private:
  union {
  USHORT		format;		/* Format identifier */
  ContextFormat1	format1[];
  ContextFormat2	format2[];
  ContextFormat3	format3[];
  } u;
};
ASSERT_SIZE (Context, 2);


/* Chaining Contextual lookups */

struct ChainContextLookupContext
{
  ContextFuncs funcs;
  char *match_data[3];
};

static inline bool chain_context_lookup (APPLY_ARG_DEF,
					 unsigned int backtrackCount,
					 const USHORT backtrack[],
					 unsigned int inputCount, /* Including the first glyph (not matched) */
					 const USHORT input[], /* Array of input values--start with second glyph */
					 unsigned int lookaheadCount,
					 const USHORT lookahead[],
					 unsigned int lookupCount,
					 const LookupRecord lookupRecord[],
					 ChainContextLookupContext &lookup_context)
{
  /* First guess */
  if (HB_UNLIKELY (buffer->out_pos < backtrackCount ||
		   buffer->in_pos + inputCount + lookaheadCount > buffer->in_length ||
		   inputCount + lookaheadCount > context_length))
    return false;

  unsigned int offset;
  return match_backtrack (APPLY_ARG,
			  backtrackCount, backtrack,
			  lookup_context.funcs.match, lookup_context.match_data[0]) &&
	 match_input (APPLY_ARG,
		      inputCount, input,
		      lookup_context.funcs.match, lookup_context.match_data[1],
		      &offset) &&
	 match_lookahead (APPLY_ARG,
			  lookaheadCount, lookahead,
			  lookup_context.funcs.match, lookup_context.match_data[2],
			  offset) &&
	 (context_length = offset, true) &&
	 apply_lookup (APPLY_ARG,
		       inputCount,
		       lookupCount, lookupRecord,
		       lookup_context.funcs.apply);
}

struct ChainRule
{
  friend struct ChainRuleSet;

  private:
  inline bool apply (APPLY_ARG_DEF, ChainContextLookupContext &lookup_context) const
  {
    const HeadlessArrayOf<USHORT> &input = *(const HeadlessArrayOf<USHORT>*)
					    (CONST_CHARP(&backtrack) + backtrack.get_size ());
    const ArrayOf<USHORT> &lookahead = *(const ArrayOf<USHORT>*)
				        (CONST_CHARP(&input) + input.get_size ());
    const ArrayOf<LookupRecord> &lookup = *(const ArrayOf<LookupRecord>*)
					   (CONST_CHARP(&lookahead) + lookahead.get_size ());
    return chain_context_lookup (APPLY_ARG,
				 backtrack.len, backtrack.array,
				 input.len, input.array + 1,
				 lookahead.len, lookahead.array,
				 lookup.len, lookup.array,
				 lookup_context);
    return false;
  }

  public:
  inline bool sanitize (SANITIZE_ARG_DEF) {
    if (!SANITIZE (backtrack)) return false;
    HeadlessArrayOf<USHORT> &input = *(HeadlessArrayOf<USHORT>*)
				      (CHARP(&backtrack) + backtrack.get_size ());
    if (!SANITIZE (input)) return false;
    ArrayOf<USHORT> &lookahead = *(ArrayOf<USHORT>*)
				  (CHARP(&input) + input.get_size ());
    if (!SANITIZE (lookahead)) return false;
    ArrayOf<LookupRecord> &lookup = *(ArrayOf<LookupRecord>*)
				     (CHARP(&lookahead) + lookahead.get_size ());
    return SANITIZE (lookup);
  }

  private:
  ArrayOf<USHORT>
		backtrack;		/* Array of backtracking values
					 * (to be matched before the input
					 * sequence) */
  HeadlessArrayOf<USHORT>
		inputX;			/* Array of input values (start with
					 * second glyph) */
  ArrayOf<USHORT>
		lookaheadX;		/* Array of lookahead values's (to be
					 * matched after the input sequence) */
  ArrayOf<LookupRecord>
		lookupX;		/* Array of LookupRecords--in
					 * design order) */
};
ASSERT_SIZE (ChainRule, 8);

struct ChainRuleSet
{
  inline bool apply (APPLY_ARG_DEF, ChainContextLookupContext &lookup_context) const
  {
    unsigned int num_rules = rule.len;
    for (unsigned int i = 0; i < num_rules; i++)
    {
      if ((this+rule[i]).apply (APPLY_ARG, lookup_context))
        return true;
    }

    return false;
  }

  inline bool sanitize (SANITIZE_ARG_DEF) {
    return SANITIZE_THIS (rule);
  }

  private:
  OffsetArrayOf<ChainRule>
		rule;			/* Array of ChainRule tables
					 * ordered by preference */
};
ASSERT_SIZE (ChainRuleSet, 2);

struct ChainContextFormat1
{
  friend struct ChainContext;

  private:
  inline bool apply (APPLY_ARG_DEF, apply_lookup_func_t apply_func) const
  {
    unsigned int index = (this+coverage) (IN_CURGLYPH ());
    if (HB_LIKELY (index == NOT_COVERED))
      return false;

    const ChainRuleSet &rule_set = this+ruleSet[index];
    struct ChainContextLookupContext lookup_context = {
      {match_glyph, apply_func},
      {NULL, NULL, NULL}
    };
    return rule_set.apply (APPLY_ARG, lookup_context);
  }

  inline bool sanitize (SANITIZE_ARG_DEF) {
    return SANITIZE_THIS2 (coverage, ruleSet);
  }

  private:
  USHORT	format;			/* Format identifier--format = 1 */
  OffsetTo<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of table */
  OffsetArrayOf<ChainRuleSet>
		ruleSet;		/* Array of ChainRuleSet tables
					 * ordered by Coverage Index */
};
ASSERT_SIZE (ChainContextFormat1, 6);

struct ChainContextFormat2
{
  friend struct ChainContext;

  private:
  inline bool apply (APPLY_ARG_DEF, apply_lookup_func_t apply_func) const
  {
    unsigned int index = (this+coverage) (IN_CURGLYPH ());
    if (HB_LIKELY (index == NOT_COVERED))
      return false;

    const ClassDef &backtrack_class_def = this+backtrackClassDef;
    const ClassDef &input_class_def = this+inputClassDef;
    const ClassDef &lookahead_class_def = this+lookaheadClassDef;

    index = input_class_def (IN_CURGLYPH ());
    const ChainRuleSet &rule_set = this+ruleSet[index];
    /* LONGTERMTODO: Old code fetches glyph classes at most once and caches
     * them across subrule lookups.  Not sure it's worth it.
     */
    struct ChainContextLookupContext lookup_context = {
     {match_class, apply_func},
     {CHARP(&backtrack_class_def),
      CHARP(&input_class_def),
      CHARP(&lookahead_class_def)}
    };
    return rule_set.apply (APPLY_ARG, lookup_context);
  }

  inline bool sanitize (SANITIZE_ARG_DEF) {
    return SANITIZE_THIS2 (coverage, backtrackClassDef) &&
	   SANITIZE_THIS2 (inputClassDef, lookaheadClassDef) &&
	   SANITIZE_THIS (ruleSet);
  }

  private:
  USHORT	format;			/* Format identifier--format = 2 */
  OffsetTo<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of table */
  OffsetTo<ClassDef>
		backtrackClassDef;	/* Offset to glyph ClassDef table
					 * containing backtrack sequence
					 * data--from beginning of table */
  OffsetTo<ClassDef>
		inputClassDef;		/* Offset to glyph ClassDef
					 * table containing input sequence
					 * data--from beginning of table */
  OffsetTo<ClassDef>
		lookaheadClassDef;	/* Offset to glyph ClassDef table
					 * containing lookahead sequence
					 * data--from beginning of table */
  OffsetArrayOf<ChainRuleSet>
		ruleSet;		/* Array of ChainRuleSet tables
					 * ordered by class */
};
ASSERT_SIZE (ChainContextFormat2, 12);

struct ChainContextFormat3
{
  friend struct ChainContext;

  private:

  inline bool apply (APPLY_ARG_DEF, apply_lookup_func_t apply_func) const
  {
    const OffsetArrayOf<Coverage> &input = *(const OffsetArrayOf<Coverage>*)
					    (CONST_CHARP(&backtrack) + backtrack.get_size ());

    unsigned int index = (this+input[0]) (IN_CURGLYPH ());
    if (HB_LIKELY (index == NOT_COVERED))
      return false;

    const OffsetArrayOf<Coverage> &lookahead = *(const OffsetArrayOf<Coverage>*)
					        (CONST_CHARP(&input) + input.get_size ());
    const ArrayOf<LookupRecord> &lookup = *(const ArrayOf<LookupRecord>*)
					   (CONST_CHARP(&lookahead) + lookahead.get_size ());
    struct ChainContextLookupContext lookup_context = {
      {match_coverage, apply_func},
      {CHARP(this), CHARP(this), CHARP(this)}
    };
    return chain_context_lookup (APPLY_ARG,
				 backtrack.len, (USHORT *) backtrack.array,
				 input.len, (USHORT *) input.array,
				 lookahead.len, (USHORT *) lookahead.array,
				 lookup.len, lookup.array,
				 lookup_context);
    return false;
  }

  inline bool sanitize (SANITIZE_ARG_DEF) {
    if (!SANITIZE_THIS (backtrack)) return false;
    OffsetArrayOf<Coverage> &input = *(OffsetArrayOf<Coverage>*)
				      (CHARP(&backtrack) + backtrack.get_size ());
    if (!SANITIZE_THIS (input)) return false;
    OffsetArrayOf<Coverage> &lookahead = *(OffsetArrayOf<Coverage>*)
					  (CHARP(&input) + input.get_size ());
    if (!SANITIZE_THIS (lookahead)) return false;
    ArrayOf<LookupRecord> &lookup = *(ArrayOf<LookupRecord>*)
				     (CHARP(&lookahead) + lookahead.get_size ());
    return SANITIZE (lookup);
  }

  private:
  USHORT	format;			/* Format identifier--format = 3 */
  OffsetArrayOf<Coverage>
		backtrack;		/* Array of coverage tables
					 * in backtracking sequence, in  glyph
					 * sequence order */
  OffsetArrayOf<Coverage>
		inputX		;	/* Array of coverage
					 * tables in input sequence, in glyph
					 * sequence order */
  OffsetArrayOf<Coverage>
		lookaheadX;		/* Array of coverage tables
					 * in lookahead sequence, in glyph
					 * sequence order */
  ArrayOf<LookupRecord>
		lookupX;		/* Array of LookupRecords--in
					 * design order) */
};
ASSERT_SIZE (ChainContextFormat3, 10);

struct ChainContext
{
  protected:
  bool apply (APPLY_ARG_DEF, apply_lookup_func_t apply_func) const
  {
    switch (u.format) {
    case 1: return u.format1->apply (APPLY_ARG, apply_func);
    case 2: return u.format2->apply (APPLY_ARG, apply_func);
    case 3: return u.format3->apply (APPLY_ARG, apply_func);
    default:return false;
    }
  }

  inline bool sanitize (SANITIZE_ARG_DEF) {
    if (!SANITIZE (u.format)) return false;
    switch (u.format) {
    case 1: return u.format1->sanitize (SANITIZE_ARG);
    case 2: return u.format2->sanitize (SANITIZE_ARG);
    case 3: return u.format3->sanitize (SANITIZE_ARG);
    default:return true;
    }
  }

  private:
  union {
  USHORT		format;	/* Format identifier */
  ChainContextFormat1	format1[];
  ChainContextFormat2	format2[];
  ChainContextFormat3	format3[];
  } u;
};
ASSERT_SIZE (ChainContext, 2);


struct ExtensionFormat1
{
  friend struct Extension;

  private:
  inline unsigned int get_type (void) const { return extensionLookupType; }
  inline unsigned int get_offset (void) const { return (extensionOffset[0] << 16) + extensionOffset[1]; }
  inline const LookupSubTable& get_subtable (void) const
  {
    unsigned int offset = get_offset ();
    if (HB_UNLIKELY (!offset)) return Null(LookupSubTable);
    return *(LookupSubTable*)(CHARP(this) + offset);
  }

  inline bool sanitize (SANITIZE_ARG_DEF) {
    return SANITIZE_SELF ();
  }

  private:
  USHORT	format;			/* Format identifier. Set to 1. */
  USHORT	extensionLookupType;	/* Lookup type of subtable referenced
					 * by ExtensionOffset (i.e. the
					 * extension subtable). */
  USHORT	extensionOffset[2];	/* Offset to the extension subtable,
					 * of lookup type subtable.
					 * Defined as two shorts to avoid
					 * alignment requirements. */
};
ASSERT_SIZE (ExtensionFormat1, 8);

struct Extension
{
  inline unsigned int get_type (void) const
  {
    switch (u.format) {
    case 1: return u.format1->get_type ();
    default:return 0;
    }
  }
  inline const LookupSubTable& get_subtable (void) const
  {
    switch (u.format) {
    case 1: return u.format1->get_subtable ();
    default:return Null(LookupSubTable);
    }
  }

  inline bool sanitize (SANITIZE_ARG_DEF) {
    if (!SANITIZE (u.format)) return false;
    switch (u.format) {
    case 1: return u.format1->sanitize (SANITIZE_ARG);
    default:return true;
    }
  }

  private:
  union {
  USHORT		format;		/* Format identifier */
  ExtensionFormat1	format1[];
  } u;
};
ASSERT_SIZE (Extension, 2);


/*
 * GSUB/GPOS Common
 */

struct GSUBGPOS
{
  static const hb_tag_t GSUBTag		= HB_TAG ('G','S','U','B');
  static const hb_tag_t GPOSTag		= HB_TAG ('G','P','O','S');

  STATIC_DEFINE_GET_FOR_DATA_CHECK_MAJOR_VERSION (GSUBGPOS, 1, 1);

  DEFINE_TAG_LIST_INTERFACE (Script,  script );	/* get_script_count (), get_script (i), get_script_tag (i) */
  DEFINE_TAG_LIST_INTERFACE (Feature, feature);	/* get_feature_count(), get_feature(i), get_feature_tag(i) */
  DEFINE_LIST_INTERFACE     (Lookup,  lookup );	/* get_lookup_count (), get_lookup (i) */

  // LONGTERMTODO bsearch
  DEFINE_TAG_FIND_INTERFACE (Script,  script );	/* find_script_index (), get_script_by_tag (tag) */
  DEFINE_TAG_FIND_INTERFACE (Feature, feature);	/* find_feature_index(), get_feature_by_tag(tag) */

  inline bool sanitize (SANITIZE_ARG_DEF) {
    if (!SANITIZE (version)) return false;
    if (version.major != 1) return true;
    return SANITIZE_THIS3 (scriptList, featureList, lookupList);
  }

  protected:
  FixedVersion	version;	/* Version of the GSUB/GPOS table--initially set
				 * to 0x00010000 */
  OffsetTo<ScriptList>
		scriptList;  	/* ScriptList table */
  OffsetTo<FeatureList>
		featureList; 	/* FeatureList table */
  OffsetTo<LookupList>
		lookupList; 	/* LookupList table */
};
ASSERT_SIZE (GSUBGPOS, 10);


#endif /* HB_OT_LAYOUT_GSUBGPOS_PRIVATE_HH */