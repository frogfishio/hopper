// SPDX-FileCopyrightText: 2026 Frogfish
// SPDX-License-Identifier: Apache-2.0
// Author: Alexander Croft <alex@frogfish.io>

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "hopper.h"
#include "pic.h"

typedef struct hopper_ref_entry_s {
  uint32_t offset;
  uint32_t size;
  uint32_t layout_id;
  uint8_t in_use;
} hopper_ref_entry_t;

struct hopper_s {
  uint8_t *arena;
  uint32_t arena_bytes;
  uint32_t cursor;

  hopper_ref_entry_t *refs;
  uint32_t ref_count;

  const hopper_catalog_t *catalog;
};

static int is_layout_valid(const hopper_catalog_t *catalog, uint32_t layout_id, const hopper_layout_t **out) {
  if (!catalog || catalog->abi_version != HOPPER_ABI_VERSION) {
    return 0;
  }
  for (uint32_t i = 0; i < catalog->layout_count; i++) {
    const hopper_layout_t *layout = &catalog->layouts[i];
    if (layout->layout_id == layout_id) {
      if (out) {
        *out = layout;
      }
      return 1;
    }
  }
  return 0;
}

static int ref_is_valid(hopper_t *h, hopper_ref_t ref, hopper_ref_entry_t **out_entry) {
  if (!h || ref < 0 || (uint32_t)ref >= h->ref_count) {
    return 0;
  }
  hopper_ref_entry_t *entry = &h->refs[(uint32_t)ref];
  if (!entry->in_use) {
    return 0;
  }
  if (out_entry) {
    *out_entry = entry;
  }
  return 1;
}

size_t hopper_sizeof(void) {
  return sizeof(hopper_t);
}

size_t hopper_ref_entry_sizeof(void) {
  return sizeof(hopper_ref_entry_t);
}

uint32_t hopper_version(void) {
  return HOPPER_ABI_VERSION;
}

hopper_err_t hopper_init(void *hopper_storage, const hopper_config_t *cfg, hopper_t **out) {
  if (!hopper_storage || !cfg || !out) {
    return HOPPER_E_BAD_FIELD;
  }
  if (cfg->abi_version != HOPPER_ABI_VERSION) {
    return HOPPER_E_BAD_FIELD;
  }
  if (!cfg->arena_mem || cfg->arena_bytes == 0 || !cfg->ref_mem || cfg->ref_count == 0) {
    return HOPPER_E_BAD_FIELD;
  }

  hopper_t *h = (hopper_t *)hopper_storage;
  h->arena = (uint8_t *)cfg->arena_mem;
  h->arena_bytes = cfg->arena_bytes;
  h->cursor = 0;
  h->refs = (hopper_ref_entry_t *)cfg->ref_mem;
  h->ref_count = cfg->ref_count;
  h->catalog = cfg->catalog;

  memset(h->arena, 0, h->arena_bytes);
  memset(h->refs, 0, sizeof(hopper_ref_entry_t) * h->ref_count);

  *out = h;
  return HOPPER_OK;
}

hopper_err_t hopper_reset(hopper_t *h, int32_t wipe_arena) {
  if (!h) {
    return HOPPER_E_BAD_FIELD;
  }
  h->cursor = 0;
  memset(h->refs, 0, sizeof(hopper_ref_entry_t) * h->ref_count);
  if (wipe_arena) {
    memset(h->arena, 0, h->arena_bytes);
  }
  return HOPPER_OK;
}

static hopper_err_t ensure_layout_and_ref(hopper_t *h, hopper_ref_t ref, const hopper_layout_t **out_layout, hopper_ref_entry_t **out_entry) {
  hopper_ref_entry_t *entry = NULL;
  if (!ref_is_valid(h, ref, &entry)) {
    return HOPPER_E_BAD_REF;
  }
  const hopper_layout_t *layout = NULL;
  if (!is_layout_valid(h->catalog, entry->layout_id, &layout)) {
    return HOPPER_E_BAD_LAYOUT;
  }
  if (out_layout) {
    *out_layout = layout;
  }
  if (out_entry) {
    *out_entry = entry;
  }
  return HOPPER_OK;
}

hopper_result_ref_t hopper_record(hopper_t *h, uint32_t layout_id) {
  hopper_result_ref_t res = {0, HOPPER_OK, 0};
  const hopper_layout_t *layout = NULL;
  if (!h) {
    res.err = HOPPER_E_BAD_FIELD;
    return res;
  }
  if (!is_layout_valid(h->catalog, layout_id, &layout)) {
    res.err = HOPPER_E_BAD_LAYOUT;
    return res;
  }
  if (layout->record_bytes == 0) {
    res.err = HOPPER_E_BAD_LAYOUT;
    return res;
  }
  if ((uint64_t)h->cursor + (uint64_t)layout->record_bytes > (uint64_t)h->arena_bytes) {
    res.err = HOPPER_E_OOM_ARENA;
    return res;
  }

  uint32_t free_idx = h->ref_count;
  for (uint32_t i = 0; i < h->ref_count; i++) {
    if (!h->refs[i].in_use) {
      free_idx = i;
      break;
    }
  }
  if (free_idx == h->ref_count) {
    res.err = HOPPER_E_OOM_REFS;
    return res;
  }

  hopper_ref_entry_t *entry = &h->refs[free_idx];
  entry->offset = h->cursor;
  entry->size = layout->record_bytes;
  entry->layout_id = layout->layout_id;
  entry->in_use = 1;

  memset(h->arena + entry->offset, 0, entry->size);
  h->cursor += entry->size;

  res.ok = 1;
  res.ref = (hopper_ref_t)free_idx;
  return res;
}

int32_t hopper_ref_info(hopper_t *h, hopper_ref_t ref, hopper_ref_info_t *out) {
  hopper_ref_entry_t *entry = NULL;
  if (!ref_is_valid(h, ref, &entry)) {
    return 0;
  }
  if (out) {
    out->offset = entry->offset;
    out->size = entry->size;
    out->layout_id = entry->layout_id;
  }
  return 1;
}

static hopper_err_t bounds_ok(const hopper_ref_entry_t *entry, uint32_t offset, uint32_t width) {
  uint64_t end = (uint64_t)offset + (uint64_t)width;
  if (end > entry->size) {
    return HOPPER_E_BOUNDS;
  }
  return HOPPER_OK;
}

hopper_result_u32_t hopper_read_u8(hopper_t *h, hopper_ref_t ref, uint32_t off) {
  hopper_result_u32_t res = {0, HOPPER_OK, 0};
  hopper_ref_entry_t *entry = NULL;
  if (!ref_is_valid(h, ref, &entry)) {
    res.err = HOPPER_E_BAD_REF;
    return res;
  }
  hopper_err_t err = bounds_ok(entry, off, 1);
  if (err != HOPPER_OK) {
    res.err = err;
    return res;
  }
  res.ok = 1;
  res.v = h->arena[entry->offset + off];
  return res;
}

hopper_result_u32_t hopper_read_u16le(hopper_t *h, hopper_ref_t ref, uint32_t off) {
  hopper_result_u32_t res = {0, HOPPER_OK, 0};
  hopper_ref_entry_t *entry = NULL;
  if (!ref_is_valid(h, ref, &entry)) {
    res.err = HOPPER_E_BAD_REF;
    return res;
  }
  hopper_err_t err = bounds_ok(entry, off, 2);
  if (err != HOPPER_OK) {
    res.err = err;
    return res;
  }
  uint32_t base = entry->offset + off;
  res.ok = 1;
  res.v = (uint32_t)h->arena[base] | ((uint32_t)h->arena[base + 1] << 8);
  return res;
}

hopper_result_u32_t hopper_read_u32le(hopper_t *h, hopper_ref_t ref, uint32_t off) {
  hopper_result_u32_t res = {0, HOPPER_OK, 0};
  hopper_ref_entry_t *entry = NULL;
  if (!ref_is_valid(h, ref, &entry)) {
    res.err = HOPPER_E_BAD_REF;
    return res;
  }
  hopper_err_t err = bounds_ok(entry, off, 4);
  if (err != HOPPER_OK) {
    res.err = err;
    return res;
  }
  uint32_t base = entry->offset + off;
  res.ok = 1;
  res.v = (uint32_t)h->arena[base] |
          ((uint32_t)h->arena[base + 1] << 8) |
          ((uint32_t)h->arena[base + 2] << 16) |
          ((uint32_t)h->arena[base + 3] << 24);
  return res;
}

hopper_err_t hopper_write_u8(hopper_t *h, hopper_ref_t ref, uint32_t off, uint8_t v) {
  hopper_ref_entry_t *entry = NULL;
  if (!ref_is_valid(h, ref, &entry)) {
    return HOPPER_E_BAD_REF;
  }
  hopper_err_t err = bounds_ok(entry, off, 1);
  if (err != HOPPER_OK) {
    return err;
  }
  h->arena[entry->offset + off] = v;
  return HOPPER_OK;
}

hopper_err_t hopper_write_u16le(hopper_t *h, hopper_ref_t ref, uint32_t off, uint16_t v) {
  hopper_ref_entry_t *entry = NULL;
  if (!ref_is_valid(h, ref, &entry)) {
    return HOPPER_E_BAD_REF;
  }
  hopper_err_t err = bounds_ok(entry, off, 2);
  if (err != HOPPER_OK) {
    return err;
  }
  uint32_t base = entry->offset + off;
  h->arena[base] = (uint8_t)(v & 0xFFu);
  h->arena[base + 1] = (uint8_t)((v >> 8) & 0xFFu);
  return HOPPER_OK;
}

hopper_err_t hopper_write_u32le(hopper_t *h, hopper_ref_t ref, uint32_t off, uint32_t v) {
  hopper_ref_entry_t *entry = NULL;
  if (!ref_is_valid(h, ref, &entry)) {
    return HOPPER_E_BAD_REF;
  }
  hopper_err_t err = bounds_ok(entry, off, 4);
  if (err != HOPPER_OK) {
    return err;
  }
  uint32_t base = entry->offset + off;
  h->arena[base] = (uint8_t)(v & 0xFFu);
  h->arena[base + 1] = (uint8_t)((v >> 8) & 0xFFu);
  h->arena[base + 2] = (uint8_t)((v >> 16) & 0xFFu);
  h->arena[base + 3] = (uint8_t)((v >> 24) & 0xFFu);
  return HOPPER_OK;
}

static hopper_err_t validate_field_ref(hopper_t *h, hopper_ref_t ref, uint32_t field_index, const hopper_layout_t **out_layout, const hopper_field_t **out_field, uint8_t **out_base) {
  const hopper_layout_t *layout = NULL;
  hopper_ref_entry_t *entry = NULL;
  hopper_err_t err = ensure_layout_and_ref(h, ref, &layout, &entry);
  if (err != HOPPER_OK) {
    return err;
  }
  if (field_index >= layout->field_count) {
    return HOPPER_E_BAD_FIELD;
  }
  const hopper_field_t *field = &layout->fields[field_index];
  uint64_t end = (uint64_t)field->offset + (uint64_t)field->size;
  if (end > entry->size) {
    return HOPPER_E_BOUNDS;
  }
  if (out_layout) {
    *out_layout = layout;
  }
  if (out_field) {
    *out_field = field;
  }
  if (out_base) {
    *out_base = h->arena + entry->offset + field->offset;
  }
  return HOPPER_OK;
}

hopper_err_t hopper_field_set_bytes(hopper_t *h, hopper_ref_t ref, uint32_t field_index, hopper_bytes_t bytes) {
  const hopper_field_t *field = NULL;
  uint8_t *base = NULL;
  hopper_err_t err = validate_field_ref(h, ref, field_index, NULL, &field, &base);
  if (err != HOPPER_OK) {
    return err;
  }
  if (field->kind != HOPPER_FIELD_BYTES) {
    return HOPPER_E_BAD_FIELD;
  }
  if (bytes.len > field->size) {
    return HOPPER_E_PIC_INVALID;
  }
  uint32_t copy_len = bytes.len;
  if (copy_len > 0) {
    memcpy(base, bytes.ptr, copy_len);
  }
  if (field->size > copy_len) {
    memset(base + copy_len, field->pad_byte, field->size - copy_len);
  }
  return HOPPER_OK;
}

hopper_err_t hopper_field_get_bytes(hopper_t *h, hopper_ref_t ref, uint32_t field_index, hopper_bytes_mut_t out) {
  const hopper_field_t *field = NULL;
  uint8_t *base = NULL;
  hopper_err_t err = validate_field_ref(h, ref, field_index, NULL, &field, &base);
  if (err != HOPPER_OK) {
    return err;
  }
  if (field->kind != HOPPER_FIELD_BYTES) {
    return HOPPER_E_BAD_FIELD;
  }
  if (out.len < field->size) {
    return HOPPER_E_DST_TOO_SMALL;
  }
  memcpy(out.ptr, base, field->size);
  return HOPPER_OK;
}

hopper_result_i32_t hopper_field_get_i32(hopper_t *h, hopper_ref_t ref, uint32_t field_index) {
  hopper_result_i32_t res = {0, HOPPER_OK, 0};
  const hopper_field_t *field = NULL;
  uint8_t *base = NULL;
  hopper_err_t err = validate_field_ref(h, ref, field_index, NULL, &field, &base);
  if (err != HOPPER_OK) {
    res.err = err;
    return res;
  }
  if (field->kind != HOPPER_FIELD_NUM_I32) {
    res.err = HOPPER_E_BAD_FIELD;
    return res;
  }
  int32_t v = 0;
  err = hopper_pic_decode_i32(field, base, field->size, &v);
  if (err != HOPPER_OK) {
    res.err = err;
    return res;
  }
  res.ok = 1;
  res.v = v;
  return res;
}

hopper_err_t hopper_field_set_i32(hopper_t *h, hopper_ref_t ref, uint32_t field_index, int32_t v) {
  const hopper_field_t *field = NULL;
  uint8_t *base = NULL;
  hopper_err_t err = validate_field_ref(h, ref, field_index, NULL, &field, &base);
  if (err != HOPPER_OK) {
    return err;
  }
  if (field->kind != HOPPER_FIELD_NUM_I32) {
    return HOPPER_E_BAD_FIELD;
  }
  return hopper_pic_encode_i32(field, v, base, field->size);
}

static hopper_err_t format_display_mask(const hopper_field_t *field, int32_t value, hopper_bytes_mut_t out) {
  if (!field->pic.mask_ascii || field->pic.mask_len == 0) {
    return HOPPER_E_BAD_FIELD;
  }
  if (out.len < field->pic.mask_len) {
    return HOPPER_E_DST_TOO_SMALL;
  }

  uint16_t digits = field->pic.digits;
  if (digits == 0 || digits > 64) {
    return HOPPER_E_BAD_FIELD;
  }

  char digit_buf[64];
  uint64_t abs_v = value < 0 ? -(int64_t)value : (uint64_t)value;
  for (int i = digits - 1; i >= 0; i--) {
    digit_buf[i] = (char)('0' + (abs_v % 10u));
    abs_v /= 10u;
  }

  const char *mask = field->pic.mask_ascii;
  uint16_t mask_len = field->pic.mask_len;
  uint16_t placeholder_count = 0;
  int32_t last_digit_pos = -1;
  int has_plus = 0;
  int has_minus = 0;

  for (int i = 0; i < mask_len; i++) {
    char c = mask[i];
    if (c == '9' || c == 'Z') {
      placeholder_count++;
      last_digit_pos = i;
    } else if (c == '+') {
      has_plus = 1;
    } else if (c == '-') {
      has_minus = 1;
    } else if (c == ' ' || c == ',' || c == '.') {
      continue;
    } else {
      return HOPPER_E_PIC_INVALID;
    }
  }

  if (placeholder_count != digits) {
    return HOPPER_E_PIC_INVALID;
  }
  if (field->pic.is_signed && !(has_plus || has_minus)) {
    return HOPPER_E_PIC_INVALID;
  }

  uint16_t digit_idx = 0;
  int shown_digit = 0;
  int any_non_zero = 0;
  for (int i = 0; i < mask_len; i++) {
    char c = mask[i];
    switch (c) {
    case '9': {
      char d = digit_buf[digit_idx++];
      if (d != '0') {
        shown_digit = 1;
        any_non_zero = 1;
      } else {
        shown_digit = 1; // 9 always emits a digit (including 0)
      }
      out.ptr[i] = (uint8_t)d;
      break;
    }
    case 'Z': {
      char d = digit_buf[digit_idx++];
      if (d != '0' || shown_digit) {
        out.ptr[i] = (uint8_t)d;
        shown_digit = 1;
        if (d != '0') {
          any_non_zero = 1;
        }
      } else {
        out.ptr[i] = (uint8_t)' ';
      }
      break;
    }
    case ',':
      out.ptr[i] = shown_digit ? (uint8_t)',' : (uint8_t)' ';
      break;
    case '.':
      out.ptr[i] = (uint8_t)'.';
      break;
    case '+':
      out.ptr[i] = (value < 0) ? (uint8_t)'-' : (uint8_t)'+';
      break;
    case '-':
      out.ptr[i] = (value < 0) ? (uint8_t)'-' : (uint8_t)' ';
      break;
    case ' ':
      out.ptr[i] = (uint8_t)' ';
      break;
    default:
      return HOPPER_E_PIC_INVALID;
    }
  }

  if (digit_idx != digits) {
    return HOPPER_E_PIC_INVALID;
  }

  if (!any_non_zero && last_digit_pos >= 0) {
    out.ptr[last_digit_pos] = (uint8_t)'0';
  }
  return HOPPER_OK;
}

hopper_result_i32_t hopper_field_format_display(hopper_t *h, hopper_ref_t ref, uint32_t field_index, hopper_bytes_mut_t out) {
  hopper_result_i32_t res = {0, HOPPER_OK, 0};
  const hopper_field_t *field = NULL;
  uint8_t *base = NULL;
  hopper_err_t err = validate_field_ref(h, ref, field_index, NULL, &field, &base);
  if (err != HOPPER_OK) {
    res.err = err;
    return res;
  }
  if (field->kind != HOPPER_FIELD_NUM_I32) {
    res.err = HOPPER_E_BAD_FIELD;
    return res;
  }
  if (field->pic.usage != HOPPER_USAGE_DISPLAY) {
    res.err = HOPPER_E_UNSUPPORTED;
    return res;
  }
  int32_t value = 0;
  err = hopper_pic_decode_i32(field, base, field->size, &value);
  if (err != HOPPER_OK) {
    res.err = err;
    return res;
  }
  err = format_display_mask(field, value, out);
  if (err != HOPPER_OK) {
    res.err = err;
    return res;
  }
  res.ok = 1;
  res.v = (int32_t)field->pic.mask_len;
  return res;
}
