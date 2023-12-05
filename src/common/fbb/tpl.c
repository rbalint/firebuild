{# ------------------------------------------------------------------ #}
{# Copyright (c) 2022 Firebuild Inc.                                  #}
{# All rights reserved.                                               #}
{# Free for personal use and commercial trial.                        #}
{# Non-trial commercial use requires licenses available from          #}
{# https://firebuild.com.                                             #}
{# Modification and redistribution are permitted, but commercial use  #}
{# of derivative works is subject to the same requirements of this    #}
{# license.                                                           #}
{# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,    #}
{# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF #}
{# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND              #}
{# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT        #}
{# HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,       #}
{# WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, #}
{# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER      #}
{# DEALINGS IN THE SOFTWARE.                                          #}
{# ------------------------------------------------------------------ #}
{# Template to generate {{ ns }}.c.                                   #}
{# ------------------------------------------------------------------ #}

/* Auto-generated by generate_fbb, do not edit */  {# Well, not here, #}
{#                         this is the manually edited template file, #}
{#                                placing this message in the output. #}

{% set NS = ns|upper %}

#include "{{ ns }}.h"

#include <stdio.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"

/* Round up the offset number to the nearest multiple of 8.
 * Updates the given offset variable to the new value.
 * The old offset value must be nonnegative. */
#define ADD_PADDING_LEN(offset) do { \
    offset = (offset + 7) & ~0x07; \
  } while (0);

/* Pad with zeros to the next offset that's a multiple of 8.
 * Zeroes out 0 to 7 memory cells beginning at p + offset.
 * Updates the given offset variable accordingly to the new value.
 * The old offset value must be nonnegative. */
#define PAD(p, offset) do { \
    int len = 7 - ((offset + 7) & 0x07); \
    memset(p + offset, 0, len); \
    offset += len; \
  } while (0);

/* Beginning of extra_c */
{{ extra_c }}
/* End of extra_c */

static void {{ ns }}_debug_string(FILE *f, const char *str) {
  fputc('"', f);
  while (*str) {
    size_t quick_run = strcspn(str, "\\\"\b\f\n\r\t");
    fwrite(str, 1, quick_run, f);
    str += quick_run;
    if (!*str) break;
    switch (*str) {
      case '\\': fputs("\\\\", f); break;
      case '"':  fputs("\\\"", f); break;
      case '\b': fputs("\\b", f); break;
      case '\f': fputs("\\f", f); break;
      case '\n': fputs("\\n", f); break;
      case '\r': fputs("\\r", f); break;
      case '\t': fputs("\\t", f); break;
      default:   assert(0);
    }
    str++;
  }
  fputc('"', f);
}

static void {{ ns }}_builder_debug_indent(FILE *f, const {{ NS }}_Builder *msg, int indent);
static void {{ ns }}_serialized_debug_indent(FILE *f, const {{ NS }}_Serialized *msg, int indent);

### for (msg, fields) in msgs
/******************************************************************************
 *  {{ msg }}
 ******************************************************************************/

###   set jinjans = namespace(has_relptr=False)
###   for (quant, type, var, dbgfn) in fields
###     if quant == ARRAY or type in [STRING, FBB]
###       set jinjans.has_relptr = True
###     endif
###   endfor

### for variant in ['builder', 'serialized']
/*
 * {{ variant|capitalize }} - Debug a '{{ msg }}' message
 */
static void {{ ns }}_{{ variant }}_{{ msg }}_debug_indent(FILE *f, const {{ NS }}_{{ variant|capitalize }} *variant, int indent) {
  const {{ NS }}_{{ variant|capitalize }}_{{ msg }} *msg = (const {{ NS }}_{{ variant|capitalize }}_{{ msg }}*)variant;
  const char *sep;
  const int indent_step = 4;
  indent += indent_step;
  fprintf(f, "{\n%*s\"[{{ NS }}_TAG]\": \"%s\"", indent, "", "{{ msg }}");
###     for (quant, type, var, dbgfn) in fields
###       if quant in [REQUIRED, OPTIONAL]
###         if quant == OPTIONAL
  /* Optional {{ type }} '{{ var }}' */
  if ({{ ns }}_{{ variant }}_{{ msg }}_has_{{ var }}(msg)) {
###         else
  /* Required {{ type }} '{{ var }}' */
  if (1) {
###         endif
    fprintf(f, ",\n%*s\"{{ var }}\": ", indent, "");
###         if dbgfn
    {{ dbgfn }}(f, {{ ns }}_{{ variant }}_{{ msg }}_get_{{ var }}(msg),
              {% if variant == 'builder' %}false{% else %}true {% endif %}, msg);
###         elif var in varnames_with_custom_debugger
    {{ ns }}_debug_{{ var }}(f, {{ ns }}_{{ variant }}_{{ msg }}_get_{{ var }}(msg));
###         elif type == STRING
    {{ ns }}_debug_string(f, {{ ns }}_{{ variant }}_{{ msg }}_get_{{ var }}(msg));
###         elif type == FBB
    {{ ns }}_{{ variant }}_debug_indent(f, {{ ns }}_{{ variant }}_{{ msg }}_get_{{ var }}(msg), indent);
###         elif type in types_with_custom_debugger
    {{ ns }}_debug_{{ type|replace(" ","_")|replace(":","_") }}(f, {{ ns }}_{{ variant }}_{{ msg }}_get_{{ var }}(msg));
###         elif type == "bool"
    fprintf(f, "%s", {{ ns }}_{{ variant }}_{{ msg }}_get_{{ var }}(msg) ? "true" : "false");
###         else
    fprintf(f, "%lld", (long long) {{ ns }}_{{ variant }}_{{ msg }}_get_{{ var }}(msg));
###         endif
###         if quant == OPTIONAL
  } else {
    fprintf(f, ",\n%*s\"// {{ var }}\": null", indent, "");
###         endif
  }
###       else
  /* {{ type }} array '{{ var }}' */
  fprintf(f, ",\n%*s\"{{ var }}\": [", indent, "");
  indent += indent_step;
  sep = "";
  for (fbb_size_t idx = 0; idx < {{ ns }}_{{ variant }}_{{ msg }}_get_{{ var }}_count(msg); idx++) {
    fprintf(f, "%s\n%*s", sep, indent, "");
###         if dbgfn
    {{ dbgfn }}(f, {{ ns }}_{{ variant }}_{{ msg }}_get_{{ var }}_at(msg, idx)
              {% if variant == 'builder' %}false{% else %}true {% endif %}, msg);
###         elif var in varnames_with_custom_debugger
    {{ ns }}_debug_{{ var }}(f, {{ ns }}_{{ variant }}_{{ msg }}_get_{{ var }}_at(msg, idx));
###         elif type == STRING
    {{ ns }}_debug_string(f, {{ ns }}_{{ variant }}_{{ msg }}_get_{{ var }}_at(msg, idx));
###         elif type == FBB
    {{ ns }}_{{ variant }}_debug_indent(f, {{ ns }}_{{ variant }}_{{ msg }}_get_{{ var }}_at(msg, idx), indent);
###         elif type in types_with_custom_debugger
    {{ ns }}_debug_{{ type|replace(" ","_")|replace(":","_") }}(f, {{ ns }}_{{ variant }}_{{ msg }}_get_{{ var }}_at(msg, idx));
###         elif type == "bool"
    fprintf(f, "%s", {{ ns }}_{{ variant }}_{{ msg }}_get_{{ var }}_at(msg, idx) ? "true" : "false");
###         else
    fprintf(f, "%lld", (long long) {{ ns }}_{{ variant }}_{{ msg }}_get_{{ var }}_at(msg, idx));
###         endif
    sep = ",";
  }
  indent -= indent_step;
  if ({{ ns }}_{{ variant }}_{{ msg }}_get_{{ var }}_count(msg) > 0) fprintf(f, "\n%*s", indent, "");
  fprintf(f, "]");
###       endif
###     endfor
  indent -= indent_step;
  fprintf(f, "\n%*s}", indent, "");
}
###   endfor

/*
 * Builder - Measure a '{{ msg }}' message
 *
 * Has to be kept in sync (wrt. paddings and such) with {{ ns }}_builder_{{ msg }}_serialize()
 * to make sure that they return the same length.
 */
static fbb_size_t {{ ns }}_builder_{{ msg }}_measure(const {{ NS }}_Builder *bldr) {
  /* The base structure */
  const {{ NS }}_Builder_{{ msg }} *msgbldr = ({{ NS }}_Builder_{{ msg }}*)bldr;
  fbb_size_t len = sizeof({{ NS }}_Serialized_{{ msg }});

###   if jinjans.has_relptr
  /* Relptrs */
  len += sizeof({{ NS }}_Relptrs_{{ msg }});
###   endif

  /* Padding */
  ADD_PADDING_LEN(len);

  /* Sizes of scalar arrays */
###   for (quant, type, var, dbgfn) in fields
###     if quant == ARRAY and type not in [STRING, FBB]
  len += msgbldr->wire.{{ var }}_count_ * sizeof(*msgbldr->{{ var }}_);
  ADD_PADDING_LEN(len);
###     endif
###   endfor

  /* Sizes of required and optional strings */
###   for (quant, type, var, dbgfn) in fields
###     if quant in [REQUIRED, OPTIONAL] and type == STRING
  if (msgbldr->{{ var }}_ != NULL) {
    len += msgbldr->wire.{{ var }}_len_ + 1;
    ADD_PADDING_LEN(len);
  }
###     endif
###   endfor

  /* Recurse into required and optional FBBs */
###   for (quant, type, var, dbgfn) in fields
###     if quant in [REQUIRED, OPTIONAL] and type == FBB
  if (msgbldr->{{ var }}_ != NULL) {
    len += {{ ns }}_builder_measure(msgbldr->{{ var }}_);  /* already includes padding */
  }
###     endif
###   endfor

    /* The second hop for arrays of strings */
###   for (quant, type, var, dbgfn) in fields
###     if quant == ARRAY and type == STRING
  len += 2 * msgbldr->wire.{{ var }}_count_ * sizeof(fbb_size_t);  /* we'll build an alternating list of offsets and lengths */
  ADD_PADDING_LEN(len);
  for (fbb_size_t idx = 0; idx < msgbldr->wire.{{ var }}_count_; idx++) {
    len += {{ ns }}_builder_{{ msg }}_get_{{ var }}_len_at(msgbldr, idx) + 1;
    ADD_PADDING_LEN(len);
  }
###     endif
###   endfor

    /* The second hop for arrays of FBBs, including recursion */
###   for (quant, type, var, dbgfn) in fields
###     if quant == ARRAY and type == FBB
  len += msgbldr->wire.{{ var }}_count_ * sizeof(fbb_size_t);
  ADD_PADDING_LEN(len);
  for (fbb_size_t idx = 0; idx < msgbldr->wire.{{ var }}_count_; idx++) {
    len +=  {{ ns }}_builder_measure({{ ns }}_builder_{{ msg }}_get_{{ var }}_at(msgbldr, idx));  /* already includes padding */
  }
###     endif
###   endfor

  ADD_PADDING_LEN(len);
  return len;
}

/*
 * Builder - Serialize a '{{ msg }}' message to memory
 *
 * Has to be kept in sync (wrt. paddings and such) with {{ ns }}_builder_{{ msg }}_measure()
 * to make sure that they return the same length.
 */
static fbb_size_t {{ ns }}_builder_{{ msg }}_serialize(const {{ NS }}_Builder *bldr, char *dst) {
  const {{ NS }}_Builder_{{ msg }} *msgbldr = ({{ NS }}_Builder_{{ msg }}*)bldr;
#ifdef FB_EXTRA_DEBUG
  /* Verify that the required fields were set */
###   for (quant, type, var, dbgfn) in fields
###     if quant == REQUIRED
###       if type in [STRING, FBB]
  assert(msgbldr->{{ var }}_ != NULL);
###       else
  assert(msgbldr->has_{{ var }}_);
###       endif
###     endif
###   endfor
#endif

  fbb_size_t offset = 0;  /* relative to the beginning of this (sub)message */

  /* The wire structure */
  memcpy(dst, &msgbldr->wire, sizeof(msgbldr->wire));
  offset = sizeof(msgbldr->wire);

  /* No padding here (other than the one contained within msgbldr->wire).
   * If more padding is added here then the offset of the Relptr structure
   * will need to be adjusted in the serialized getters. */

###   if jinjans.has_relptr
  /* First hop relptrs. Zero out, just in case there's a padding at the end of this structure that we
   * won't initialize (or we could go with attribute packed, too). Will set the actual values soon. */
  {{ NS }}_Relptrs_{{ msg }} *relptrs = ({{ NS }}_Relptrs_{{ msg }} *) (dst + offset);
  memset(dst + offset, 0, sizeof({{ NS }}_Relptrs_{{ msg }}));
  offset += sizeof({{ NS }}_Relptrs_{{ msg }});
###   endif

  /* Padding */
  PAD(dst, offset);

  /* Arrays of scalars */
###   for (quant, type, var, dbgfn) in fields
###     if quant == ARRAY and type not in [STRING, FBB]
  if (msgbldr->wire.{{ var }}_count_ > 0) {
    relptrs->{{ var }}_relptr_ = offset;
    fbb_size_t size = msgbldr->wire.{{ var }}_count_ * sizeof(msgbldr->{{ var }}_[0]);
    memcpy(dst + offset, msgbldr->{{ var }}_, size);
    offset += size;
    PAD(dst, offset);
  } else {
    relptrs->{{ var }}_relptr_ = 0;
  }
###     endif
###   endfor

  /* Required and optional strings */
###   for (quant, type, var, dbgfn) in fields
###     if quant in [REQUIRED, OPTIONAL] and type == STRING
  if (msgbldr->{{ var }}_ != NULL) {
    relptrs->{{ var }}_relptr_ = offset;
    fbb_size_t size = msgbldr->wire.{{ var }}_len_ + 1;
    memcpy(dst + offset, msgbldr->{{ var }}_, size);
    offset += size;
    PAD(dst, offset);
  } else {
    relptrs->{{ var }}_relptr_ = 0;
  }
###     endif
###   endfor

  /* Required and optional FBBs */
###   for (quant, type, var, dbgfn) in fields
###     if quant in [REQUIRED, OPTIONAL] and type == FBB
  if (msgbldr->{{ var }}_ != NULL) {
    relptrs->{{ var }}_relptr_ = offset;
    fbb_size_t size = {{ ns }}_builder_serialize(msgbldr->{{ var }}_, dst + offset);
    offset += size;
    /* FBB is serialized with a final padding, so no more padding is added here */
  } else {
    relptrs->{{ var }}_relptr_ = 0;
  }
###     endif
###   endfor

  /* Arrays of strings */
###   for (quant, type, var, dbgfn) in fields
###     if quant == ARRAY and type == STRING
  if (msgbldr->wire.{{ var }}_count_ > 0) {
    relptrs->{{ var }}_relptr_ = offset;
    fbb_size_t *hops = (fbb_size_t *) (dst + offset);
    fbb_size_t size = 2 * msgbldr->wire.{{ var }}_count_ * sizeof(fbb_size_t);  /* room for alternating list of offsets and lengths */
    offset += size;
    PAD(dst, offset);
    for (fbb_size_t idx = 0; idx < msgbldr->wire.{{ var }}_count_; idx++) {
      fbb_size_t len = 0;
      const char *str = {{ ns }}_builder_{{ msg }}_get_{{ var }}_with_len_at(msgbldr, idx, &len);
      size = len + 1;
      *hops++ = offset;  /* build up an alternating list of offsets and lengths */
      *hops++ = len;
      memcpy(dst + offset, str, size);
      offset += size;
      PAD(dst, offset);
    }
  } else {
    relptrs->{{ var }}_relptr_ = 0;
  }
###     endif
###   endfor

  /* Arrays of FBBs */
###   for (quant, type, var, dbgfn) in fields
###     if quant == ARRAY and type == FBB
  if (msgbldr->wire.{{ var }}_count_ > 0) {
    relptrs->{{ var }}_relptr_ = offset;
    fbb_size_t *hops = (fbb_size_t *) (dst + offset);
    fbb_size_t size = msgbldr->wire.{{ var }}_count_ * sizeof(fbb_size_t);
    offset += size;
    PAD(dst, offset);
    for (fbb_size_t idx = 0; idx < msgbldr->wire.{{ var }}_count_; idx++) {
      *hops++ = offset;
      const {{ NS }}_Builder *fbb = {{ ns }}_builder_{{ msg }}_get_{{ var }}_at(msgbldr, idx);
      offset += {{ ns }}_builder_serialize(fbb, dst + offset);  /* recurse */
      /* FBB is serialized with a final padding, so no more padding is added here */
    }
  } else {
    relptrs->{{ var }}_relptr_ = 0;
  }
###     endif
###   endfor

  PAD(dst, offset);
  return offset;
}

### endfor

/******************************************************************************
 *  Global
 ******************************************************************************/

/*
 * Lookup array for the message name of a particular message tag
 */
static const char *{{ ns }}_tag_to_string_array[] = {
  NULL,
### for (msg, _) in msgs
  "{{ msg }}",
### endfor
};

/*
 * Get the tag as string
 */
const char *{{ ns }}_tag_to_string(int tag) {
  assert(tag >= 1 && tag < {{ NS }}_TAG_NEXT);
  return {{ ns }}_tag_to_string_array[tag];
}

/*
 * Builder - Lookup array for the debugger function of a particular message tag
 */
static void (*{{ ns }}_builder_debuggers_array[])(FILE *, const {{ NS }}_Builder *, int) = {
  NULL,
### for (msg, _) in msgs
  (void (*) (FILE *, const {{ NS }}_Builder *, int)) {{ ns }}_builder_{{ msg }}_debug_indent,
### endfor
};

/*
 * Builder - Debug any message with custom indentation and no final newline and no flushing
 */
static void {{ ns }}_builder_debug_indent(FILE *f, const {{ NS }}_Builder *msg, int indent) {
  int tag = * ((int *) msg);
  assert(tag >= 1 && tag < {{ NS }}_TAG_NEXT);
  (*{{ ns }}_builder_debuggers_array[tag])(f, msg, indent);
}
/*
 * Builder - Debug any message
 */
void {{ ns }}_builder_debug(FILE *f, const {{ NS }}_Builder *msg) {
  {{ ns }}_builder_debug_indent(f, msg, 0);
  fprintf(f, "\n");
  fflush(f);
}

/*
 * Serialized - Lookup array for the debugger function of a particular message tag
 */
static void (*{{ ns }}_serialized_debuggers_array[])(FILE *, const {{ NS }}_Serialized *, int) = {
  NULL,
### for (msg, _) in msgs
  (void (*) (FILE *, const {{ NS }}_Serialized *, int)) {{ ns }}_serialized_{{ msg }}_debug_indent,
### endfor
};

/*
 * Serialized - Debug any message with custom indentation and no final newline and no flushing
 */
static void {{ ns }}_serialized_debug_indent(FILE *f, const {{ NS }}_Serialized *msg, int indent) {
  int tag = * ((int *) msg);
  assert(tag >= 1 && tag < {{ NS }}_TAG_NEXT);
  (*{{ ns }}_serialized_debuggers_array[tag])(f, msg, indent);
}
/*
 * Serialized - Debug any message
 */
void {{ ns }}_serialized_debug(FILE *f, const {{ NS }}_Serialized *msg) {
  {{ ns }}_serialized_debug_indent(f, msg, 0);
  fprintf(f, "\n");
  fflush(f);
}

/*
 * Builder - Lookup array for the measurer function of a particular message tag
 */
static fbb_size_t (*{{ ns }}_builder_measures_array[])(const {{ NS }}_Builder *) = {
  NULL,
### for (msg, _) in msgs
  (fbb_size_t (*) (const {{ NS }}_Builder *)) {{ ns }}_builder_{{ msg }}_measure,
### endfor
};

/*
 * Builder - Measure any message
 *
 * See the documentation in tpl.h.
 */
fbb_size_t {{ ns }}_builder_measure(const {{ NS }}_Builder *msgbldr) {
  /* Invoke the particular measurer for this message type */
  int tag = * ((int *) msgbldr);
  assert(tag >= 1 && tag < {{ NS }}_TAG_NEXT);
  return (*{{ ns }}_builder_measures_array[tag])(msgbldr);
}

/*
 * Builder - Lookup array for the serializer function of a particular message tag
 */
static fbb_size_t (*{{ ns }}_builder_serializers_array[])(const {{ NS }}_Builder *, char *) = {
  NULL,
### for (msg, _) in msgs
  (fbb_size_t (*) (const {{ NS }}_Builder *, char *)) {{ ns }}_builder_{{ msg }}_serialize,
### endfor
};

/*
 * Builder - Serialize any message to memory
 *
 * See the documentation in tpl.h.
 */
fbb_size_t {{ ns }}_builder_serialize(const {{ NS }}_Builder *msgbldr, char *dst) {
#ifdef FB_EXTRA_DEBUG
  /* If the measured value is incorrect then a nasty buffer overrun can occur.
   * In order to guarantee FBB's correct behavior, let's do the debug assertion internally here in
   * FBB, rather than the caller having to do it. Which means we need to run measure() again (the
   * caller has already run it but we don't know that value). */
  fbb_size_t len_measured = {{ ns }}_builder_measure(msgbldr);
#endif

  /* Invoke the particular serializer for this message type */
  int tag = * ((int *) msgbldr);
  assert(tag >= 1 && tag < {{ NS }}_TAG_NEXT);
  fbb_size_t len = (*{{ ns }}_builder_serializers_array[tag])(msgbldr, dst);

#ifdef FB_EXTRA_DEBUG
  assert(len == len_measured);
#endif
  return len;
}

#pragma GCC diagnostic pop
