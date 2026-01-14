#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "hopper.h"

static void check(hopper_err_t err, hopper_err_t expect) {
  if (err != expect) {
    fprintf(stderr, "Expected err=%d got=%d\n", expect, err);
  }
  assert(err == expect);
}

static const hopper_field_t g_fields[] = {
    {
        .name_ascii = "raw",
        .name_len = 3,
        .offset = 0,
        .size = 4,
        .kind = HOPPER_FIELD_BYTES,
        .pad_byte = ' ',
        .pic = {0},
        .redefines_index = -1,
    },
    {
        .name_ascii = "num",
        .name_len = 3,
        .offset = 4,
        .size = 3, // 3 digits, unsigned DISPLAY
        .kind = HOPPER_FIELD_NUM_I32,
        .pad_byte = 0,
        .pic =
            {
                .digits = 3,
                .scale = 0,
                .is_signed = 0,
                .usage = HOPPER_USAGE_DISPLAY,
                .mask_ascii = NULL,
                .mask_len = 0,
            },
        .redefines_index = -1,
    },
    {
        .name_ascii = "snum",
        .name_len = 4,
        .offset = 7,
        .size = 4, // sign + 3 digits
        .kind = HOPPER_FIELD_NUM_I32,
        .pad_byte = 0,
        .pic =
            {
                .digits = 3,
                .scale = 0,
                .is_signed = 1,
                .usage = HOPPER_USAGE_DISPLAY,
                .mask_ascii = "+ZZ9",
                .mask_len = 4,
            },
        .redefines_index = -1,
    },
    {
        .name_ascii = "comp",
        .name_len = 4,
        .offset = 11,
        .size = 2,
        .kind = HOPPER_FIELD_NUM_I32,
        .pad_byte = 0,
        .pic =
            {
                .digits = 4,
                .scale = 0,
                .is_signed = 1,
                .usage = HOPPER_USAGE_COMP,
                .mask_ascii = NULL,
                .mask_len = 0,
            },
        .redefines_index = -1,
    },
    {
        .name_ascii = "comp3",
        .name_len = 5,
        .offset = 13,
        .size = 3, // digits=5 -> 3 bytes packed
        .kind = HOPPER_FIELD_NUM_I32,
        .pad_byte = 0,
        .pic =
            {
                .digits = 5,
                .scale = 0,
                .is_signed = 1,
        .usage = HOPPER_USAGE_COMP3,
        .mask_ascii = NULL,
        .mask_len = 0,
    },
    .redefines_index = -1,
    },
    {
        .name_ascii = "scaled",
        .name_len = 6,
        .offset = 16,
        .size = 6, // sign + 5 digits (S9(5) with scale 2)
        .kind = HOPPER_FIELD_NUM_I32,
        .pad_byte = 0,
        .pic =
            {
                .digits = 5,
                .scale = 2,
                .is_signed = 1,
                .usage = HOPPER_USAGE_DISPLAY,
                .mask_ascii = "+Z,ZZ.99",
                .mask_len = 8,
            },
        .redefines_index = -1,
    },
};

static const hopper_field_t g_overlay_fields[] = {
    {
        .name_ascii = "bytes4",
        .name_len = 6,
        .offset = 0,
        .size = 4,
        .kind = HOPPER_FIELD_BYTES,
        .pad_byte = 0,
        .pic = {0},
        .redefines_index = -1,
    },
    {
        .name_ascii = "num_overlay",
        .name_len = 11,
        .offset = 0, // overlay same region
        .size = 4,
        .kind = HOPPER_FIELD_NUM_I32,
        .pad_byte = 0,
        .pic =
            {
                .digits = 4,
                .scale = 0,
                .is_signed = 1,
                .usage = HOPPER_USAGE_COMP,
                .mask_ascii = NULL,
                .mask_len = 0,
            },
        .redefines_index = 0,
    },
};

static const hopper_field_t g_bad_fields[] = {
    {
        .name_ascii = "too_big",
        .name_len = 7,
        .offset = 6,
        .size = 4, // offset+size = 10 > record_bytes (8)
        .kind = HOPPER_FIELD_BYTES,
        .pad_byte = 0,
        .pic = {0},
        .redefines_index = -1,
    },
};

static const hopper_field_t g_overlap_fields[] = {
    {
        .name_ascii = "base6",
        .name_len = 5,
        .offset = 0,
        .size = 6,
        .kind = HOPPER_FIELD_BYTES,
        .pad_byte = 0,
        .pic = {0},
        .redefines_index = -1,
    },
    {
        .name_ascii = "num_overlap",
        .name_len = 11,
        .offset = 2, // overlaps bytes[2..5]
        .size = 4,
        .kind = HOPPER_FIELD_NUM_I32,
        .pad_byte = 0,
        .pic =
            {
                .digits = 10,
                .scale = 0,
                .is_signed = 1,
                .usage = HOPPER_USAGE_COMP,
                .mask_ascii = NULL,
                .mask_len = 0,
            },
        .redefines_index = 0,
    },
};

static const hopper_field_t g_scaled_comp_fields[] = {
    {
        .name_ascii = "comp_s1",
        .name_len = 7,
        .offset = 0,
        .size = 2,
        .kind = HOPPER_FIELD_NUM_I32,
        .pad_byte = 0,
        .pic =
            {
                .digits = 4,
                .scale = 1,
                .is_signed = 1,
                .usage = HOPPER_USAGE_COMP,
                .mask_ascii = NULL,
                .mask_len = 0,
            },
        .redefines_index = -1,
    },
    {
        .name_ascii = "comp3_s1",
        .name_len = 8,
        .offset = 2,
        .size = 3,
        .kind = HOPPER_FIELD_NUM_I32,
        .pad_byte = 0,
        .pic =
            {
                .digits = 5,
                .scale = 1,
                .is_signed = 1,
                .usage = HOPPER_USAGE_COMP3,
                .mask_ascii = NULL,
                .mask_len = 0,
            },
        .redefines_index = -1,
    },
};

static const hopper_field_t g_overlay_separate_fields[] = {
    {
        .name_ascii = "raw_a",
        .name_len = 5,
        .offset = 0,
        .size = 4,
        .kind = HOPPER_FIELD_NUM_I32,
        .pad_byte = 0,
        .pic =
            {
                .digits = 4,
                .scale = 0,
                .is_signed = 1,
                .usage = HOPPER_USAGE_COMP,
                .mask_ascii = NULL,
                .mask_len = 0,
            },
        .redefines_index = -1,
    },
    {
        .name_ascii = "raw_b",
        .name_len = 5,
        .offset = 4,
        .size = 4,
        .kind = HOPPER_FIELD_BYTES,
        .pad_byte = 0,
        .pic = {0},
        .redefines_index = -1,
    },
};

static const hopper_layout_t g_layouts[] = {
    {
        .name_ascii = "Sample",
        .name_len = 6,
        .record_bytes = 24,
        .layout_id = 1,
        .fields = g_fields,
        .field_count = sizeof(g_fields) / sizeof(g_fields[0]),
    },
    {
        .name_ascii = "Overlay",
        .name_len = 7,
        .record_bytes = 8,
        .layout_id = 2,
        .fields = g_overlay_fields,
        .field_count = sizeof(g_overlay_fields) / sizeof(g_overlay_fields[0]),
    },
    {
        .name_ascii = "BadBounds",
        .name_len = 9,
        .record_bytes = 8,
        .layout_id = 3,
        .fields = g_bad_fields,
        .field_count = sizeof(g_bad_fields) / sizeof(g_bad_fields[0]),
    },
    {
        .name_ascii = "OverlapPartial",
        .name_len = 14,
        .record_bytes = 10,
        .layout_id = 4,
        .fields = g_overlap_fields,
        .field_count = sizeof(g_overlap_fields) / sizeof(g_overlap_fields[0]),
    },
    {
        .name_ascii = "ScaledComp",
        .name_len = 10,
        .record_bytes = 8,
        .layout_id = 5,
        .fields = g_scaled_comp_fields,
        .field_count = sizeof(g_scaled_comp_fields) / sizeof(g_scaled_comp_fields[0]),
    },
    {
        .name_ascii = "OverlaySeparate",
        .name_len = 15,
        .record_bytes = 8,
        .layout_id = 6,
        .fields = g_overlay_separate_fields,
        .field_count = sizeof(g_overlay_separate_fields) / sizeof(g_overlay_separate_fields[0]),
    },
};

static const hopper_catalog_t g_catalog = {
    .abi_version = HOPPER_ABI_VERSION,
    .layouts = g_layouts,
    .layout_count = sizeof(g_layouts) / sizeof(g_layouts[0]),
};

static void test_all(void) {
  uint8_t arena[128];
  uint8_t ref_mem[256];
  size_t ctx_size = hopper_sizeof();
  uint8_t hopper_mem[ctx_size];

  hopper_config_t cfg = {
      .abi_version = HOPPER_ABI_VERSION,
      .arena_mem = arena,
      .arena_bytes = sizeof(arena),
      .ref_mem = ref_mem,
      .ref_count = 8,
      .catalog = &g_catalog,
  };

  hopper_t *h = NULL;
  assert(hopper_init(hopper_mem, &cfg, &h) == HOPPER_OK);

  hopper_result_ref_t rec = hopper_record(h, 1);
  assert(rec.ok == 1);

  hopper_ref_info_t info = {0};
  assert(hopper_ref_info(h, rec.ref, &info) == 1);
  assert(info.size == 24);
  assert(info.layout_id == 1);

  // Raw byte access bounds.
  assert(hopper_write_u8(h, rec.ref, 23, 0xAA) == HOPPER_OK);
  assert(hopper_write_u8(h, rec.ref, 24, 0xAA) == HOPPER_E_BOUNDS);
  hopper_result_u32_t rbyte = hopper_read_u8(h, rec.ref, 23);
  assert(rbyte.ok && rbyte.v == 0xAA);

  // Bytes field padding.
  const char *txt = "hi";
  hopper_bytes_t bsrc = {(const uint8_t *)txt, 2};
  assert(hopper_field_set_bytes(h, rec.ref, 0, bsrc) == HOPPER_OK);
  char bout[4] = {0};
  hopper_bytes_mut_t bdst = {(uint8_t *)bout, sizeof(bout)};
  assert(hopper_field_get_bytes(h, rec.ref, 0, bdst) == HOPPER_OK);
  assert(memcmp(bout, "hi  ", 4) == 0);

  // DISPLAY unsigned numeric.
  assert(hopper_field_set_i32(h, rec.ref, 1, 123) == HOPPER_OK);
  hopper_result_i32_t n = hopper_field_get_i32(h, rec.ref, 1);
  assert(n.ok && n.v == 123);
  // should reject negative for unsigned field
  assert(hopper_field_set_i32(h, rec.ref, 1, -1) == HOPPER_E_PIC_INVALID);

  // DISPLAY signed numeric + edit mask.
  assert(hopper_field_set_i32(h, rec.ref, 2, 45) == HOPPER_OK);
  hopper_result_i32_t sn = hopper_field_get_i32(h, rec.ref, 2);
  assert(sn.ok && sn.v == 45);
  char mask_out[4] = {0};
  hopper_bytes_mut_t mask_buf = {(uint8_t *)mask_out, sizeof(mask_out)};
  hopper_result_i32_t fmt = hopper_field_format_display(h, rec.ref, 2, mask_buf);
  assert(fmt.ok && fmt.v == 4);
  assert(memcmp(mask_out, "+ 45", 4) == 0);

  // COMP numeric.
  assert(hopper_field_set_i32(h, rec.ref, 3, 1234) == HOPPER_OK);
  hopper_result_i32_t comp_val = hopper_field_get_i32(h, rec.ref, 3);
  assert(comp_val.ok && comp_val.v == 1234);
  hopper_result_u32_t raw16 = hopper_read_u16le(h, rec.ref, 11);
  assert(raw16.ok && raw16.v == 1234);

  // COMP-3 numeric with negative sign.
  assert(hopper_field_set_i32(h, rec.ref, 4, -12345) == HOPPER_OK);
  hopper_result_i32_t comp3_val = hopper_field_get_i32(h, rec.ref, 4);
  assert(comp3_val.ok && comp3_val.v == -12345);
  // sign nibble should be D.
  uint8_t raw_comp3[3] = {0};
  for (int i = 0; i < 3; i++) {
    hopper_result_u32_t b = hopper_read_u8(h, rec.ref, 13u + (uint32_t)i);
    assert(b.ok);
    raw_comp3[i] = (uint8_t)b.v;
  }
  assert((raw_comp3[2] & 0x0F) == 0xD);

  // Scaled DISPLAY numeric.
  check(hopper_field_set_i32(h, rec.ref, 5, 12345), HOPPER_OK); // stored 123.45
  hopper_result_i32_t scaled = hopper_field_get_i32(h, rec.ref, 5);
  assert(scaled.ok && scaled.v == 12345);
  uint8_t buf[16] = {0};
  hopper_bytes_mut_t fmtbuf = {buf, sizeof(buf)};
  hopper_result_i32_t fmt_scaled = hopper_field_format_display(h, rec.ref, 5, fmtbuf);
  if (!fmt_scaled.ok) {
    fprintf(stderr, "scaled format err=%d\n", fmt_scaled.err);
  }
  assert(fmt_scaled.ok && fmt_scaled.v == 8);
  assert(memcmp(buf, "+1,23.45", 8) == 0);
}

static void test_oom_and_reset(void) {
  uint8_t arena[64];
  uint8_t ref_mem[64]; // ample space for small ref_count
  uint8_t hopper_mem[hopper_sizeof()];

  hopper_config_t cfg_refs = {
      .abi_version = HOPPER_ABI_VERSION,
      .arena_mem = arena,
      .arena_bytes = sizeof(arena),
      .ref_mem = ref_mem,
      .ref_count = 1, // only one ref allowed
      .catalog = &g_catalog,
  };
  hopper_t *h = NULL;
  assert(hopper_init(hopper_mem, &cfg_refs, &h) == HOPPER_OK);

  hopper_result_ref_t r1 = hopper_record(h, 1);
  assert(r1.ok);
  hopper_result_ref_t r2 = hopper_record(h, 1);
  assert(!r2.ok && r2.err == HOPPER_E_OOM_REFS);

  // Reset without wiping retains data; with wipe clears.
  check(hopper_write_u8(h, r1.ref, 0, 0x11), HOPPER_OK);
  hopper_reset(h, 0);
  hopper_result_u32_t b = hopper_read_u8(h, r1.ref, 0);
  assert(!b.ok && b.err == HOPPER_E_BAD_REF); // refs cleared

  hopper_reset(h, 1);
  hopper_result_u32_t zero = hopper_read_u8(h, r1.ref, 0);
  assert(!zero.ok && zero.err == HOPPER_E_BAD_REF);

  // Arena OOM path: tiny arena but more ref slots.
  uint8_t small_arena[24];
  uint8_t more_refs[64];
  hopper_config_t cfg_arena = {
      .abi_version = HOPPER_ABI_VERSION,
      .arena_mem = small_arena,
      .arena_bytes = sizeof(small_arena),
      .ref_mem = more_refs,
      .ref_count = 4,
      .catalog = &g_catalog,
  };
  assert(hopper_init(hopper_mem, &cfg_arena, &h) == HOPPER_OK);
  hopper_result_ref_t a1 = hopper_record(h, 1);
  assert(a1.ok);
  hopper_result_ref_t a2 = hopper_record(h, 1);
  assert(!a2.ok && a2.err == HOPPER_E_OOM_ARENA);
}

static void test_bounds_and_invalid(void) {
  uint8_t arena[32];
  uint8_t ref_mem[128];
  uint8_t hopper_mem[hopper_sizeof()];

  hopper_config_t cfg = {
      .abi_version = HOPPER_ABI_VERSION,
      .arena_mem = arena,
      .arena_bytes = sizeof(arena),
      .ref_mem = ref_mem,
      .ref_count = 4,
      .catalog = &g_catalog,
  };
  hopper_t *h = NULL;
  assert(hopper_init(hopper_mem, &cfg, &h) == HOPPER_OK);
  hopper_result_ref_t r = hopper_record(h, 1);
  assert(r.ok);

  hopper_result_u32_t out = hopper_read_u8(h, r.ref, 32);
  assert(!out.ok && out.err == HOPPER_E_BOUNDS);
  check(hopper_write_u32le(h, r.ref, 21, 0xFFFF), HOPPER_E_BOUNDS);

  hopper_result_u32_t bad = hopper_read_u8(h, 99, 0);
  assert(!bad.ok && bad.err == HOPPER_E_BAD_REF);

  // mask output buffer too small -> PIC_INVALID
  check(hopper_field_set_i32(h, r.ref, 2, 12), HOPPER_OK);
  uint8_t tiny[2];
  hopper_bytes_mut_t tiny_buf = {tiny, sizeof(tiny)};
  hopper_result_i32_t fmt = hopper_field_format_display(h, r.ref, 2, tiny_buf);
  assert(!fmt.ok && fmt.err == HOPPER_E_DST_TOO_SMALL);

  // bytes dest too small
  uint8_t small[2];
  hopper_bytes_mut_t small_buf = {small, sizeof(small)};
  check(hopper_field_get_bytes(h, r.ref, 0, small_buf), HOPPER_E_DST_TOO_SMALL);
}

static void test_comp3_bad_sign(void) {
  uint8_t arena[64];
  uint8_t ref_mem[128];
  uint8_t hopper_mem[hopper_sizeof()];

  hopper_config_t cfg = {
      .abi_version = HOPPER_ABI_VERSION,
      .arena_mem = arena,
      .arena_bytes = sizeof(arena),
      .ref_mem = ref_mem,
      .ref_count = 2,
      .catalog = &g_catalog,
  };
  hopper_t *h = NULL;
  assert(hopper_init(hopper_mem, &cfg, &h) == HOPPER_OK);
  hopper_result_ref_t r = hopper_record(h, 1);
  assert(r.ok);
  check(hopper_field_set_i32(h, r.ref, 4, 12345), HOPPER_OK);
  // Corrupt sign nibble to 0xE (invalid)
  hopper_result_u32_t last = hopper_read_u8(h, r.ref, 15);
  assert(last.ok);
  uint8_t corrupted = (uint8_t)((last.v & 0xF0u) | 0x0E);
  check(hopper_write_u8(h, r.ref, 15, corrupted), HOPPER_OK);
  hopper_result_i32_t res = hopper_field_get_i32(h, r.ref, 4);
  assert(!res.ok && res.err == HOPPER_E_PIC_INVALID);
}

static void test_overlay_bounds(void) {
  uint8_t arena[64];
  uint8_t ref_mem[128];
  uint8_t hopper_mem[hopper_sizeof()];

  hopper_config_t cfg = {
      .abi_version = HOPPER_ABI_VERSION,
      .arena_mem = arena,
      .arena_bytes = sizeof(arena),
      .ref_mem = ref_mem,
      .ref_count = 4,
      .catalog = &g_catalog,
  };
  hopper_t *h = NULL;
  assert(hopper_init(hopper_mem, &cfg, &h) == HOPPER_OK);
  hopper_result_ref_t r = hopper_record(h, 2);
  assert(r.ok);

  // Set bytes field and read numeric overlay (COMP, LE).
  uint8_t data[4] = {0xD2, 0x04, 0x00, 0x00}; // 1234 LE signed
  check(hopper_field_set_bytes(h, r.ref, 0, (hopper_bytes_t){data, 4}), HOPPER_OK);
  hopper_result_i32_t num = hopper_field_get_i32(h, r.ref, 1);
  assert(num.ok && num.v == 1234);

  // Numeric write updates same bytes region.
  check(hopper_field_set_i32(h, r.ref, 1, -77), HOPPER_OK);
  hopper_bytes_mut_t out = {data, sizeof(data)};
  check(hopper_field_get_bytes(h, r.ref, 0, out), HOPPER_OK);
  // -77 encoded in LE
  int32_t roundtrip = (int32_t)((uint32_t)data[0] | ((uint32_t)data[1] << 8) |
                                ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24));
  assert(roundtrip == -77);

  // Partial overlap bounds: writing past size should still be checked via field size.
  hopper_field_t short_field = g_overlay_fields[0];
  short_field.size = 2;
  uint8_t scratch[4] = {0};
  hopper_bytes_mut_t small_dst = {scratch, sizeof(scratch)};
  // Getting bytes with smaller field should fail bounds if offset+size > record.
  // Here offset=0 size=2 fits; ensure write respects size.
  check(hopper_field_set_bytes(h, r.ref, 0, (hopper_bytes_t){(const uint8_t *)"AB", 2}), HOPPER_OK);
  check(hopper_field_get_bytes(h, r.ref, 0, small_dst), HOPPER_OK);
}

static void test_bad_field_bounds(void) {
  uint8_t arena[32];
  uint8_t ref_mem[128];
  uint8_t hopper_mem[hopper_sizeof()];
  hopper_config_t cfg = {
      .abi_version = HOPPER_ABI_VERSION,
      .arena_mem = arena,
      .arena_bytes = sizeof(arena),
      .ref_mem = ref_mem,
      .ref_count = 2,
      .catalog = &g_catalog,
  };
  hopper_t *h = NULL;
  assert(hopper_init(hopper_mem, &cfg, &h) == HOPPER_OK);
  hopper_result_ref_t r = hopper_record(h, 3);
  assert(r.ok);
  uint8_t buf[4];
  hopper_bytes_mut_t dst = {buf, sizeof(buf)};
  hopper_err_t err = hopper_field_get_bytes(h, r.ref, 0, dst);
  assert(err == HOPPER_E_BOUNDS);
}

static void test_mask_zero_suppression(void) {
  uint8_t arena[128];
  uint8_t ref_mem[128];
  uint8_t hopper_mem[hopper_sizeof()];
  hopper_config_t cfg = {
      .abi_version = HOPPER_ABI_VERSION,
      .arena_mem = arena,
      .arena_bytes = sizeof(arena),
      .ref_mem = ref_mem,
      .ref_count = 4,
      .catalog = &g_catalog,
  };
  hopper_t *h = NULL;
  assert(hopper_init(hopper_mem, &cfg, &h) == HOPPER_OK);
  hopper_result_ref_t r = hopper_record(h, 1);
  assert(r.ok);

  // Zero value with suppression: expect "+   .00" for mask "+ZZZ.99"
  check(hopper_field_set_i32(h, r.ref, 5, 0), HOPPER_OK);
  uint8_t out[16] = {0};
  hopper_bytes_mut_t buf = {out, sizeof(out)};
  hopper_result_i32_t fmt = hopper_field_format_display(h, r.ref, 5, buf);
  assert(fmt.ok && fmt.v == 8);
  const char expected_zero[] = "+    .00";
  assert(memcmp(out, expected_zero, fmt.v) == 0);

  // Negative small value with suppression: -0.45 => "-   .45" under "+Z,ZZ.99"
  check(hopper_field_set_i32(h, r.ref, 5, -45), HOPPER_OK);
  memset(out, 0, sizeof(out));
  fmt = hopper_field_format_display(h, r.ref, 5, buf);
  assert(fmt.ok && fmt.v == 8);
  const char expected_neg[] = "-    .45";
  assert(memcmp(out, expected_neg, fmt.v) == 0);

  // Mask with comma and larger digits: reuse field 5 mask (+Z,ZZ.99) with value 99999 => "+9,99.99"
  check(hopper_field_set_i32(h, r.ref, 5, 99999), HOPPER_OK);
  memset(out, 0, sizeof(out));
  fmt = hopper_field_format_display(h, r.ref, 5, buf);
  assert(fmt.ok && fmt.v == 8);
  const char expected_max[] = "+9,99.99";
  assert(memcmp(out, expected_max, fmt.v) == 0);
}

static void test_partial_overlap(void) {
  uint8_t arena[64];
  uint8_t ref_mem[128];
  uint8_t hopper_mem[hopper_sizeof()];
  hopper_config_t cfg = {
      .abi_version = HOPPER_ABI_VERSION,
      .arena_mem = arena,
      .arena_bytes = sizeof(arena),
      .ref_mem = ref_mem,
      .ref_count = 4,
      .catalog = &g_catalog,
  };
  hopper_t *h = NULL;
  assert(hopper_init(hopper_mem, &cfg, &h) == HOPPER_OK);
  hopper_result_ref_t r = hopper_record(h, 4);
  assert(r.ok);

  uint8_t pattern[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
  check(hopper_field_set_bytes(h, r.ref, 0, (hopper_bytes_t){pattern, sizeof(pattern)}), HOPPER_OK);

  hopper_result_i32_t val = hopper_field_get_i32(h, r.ref, 1);
  int32_t expected = (int32_t)((uint32_t)0x66 << 24 | (uint32_t)0x55 << 16 | (uint32_t)0x44 << 8 | (uint32_t)0x33);
  assert(val.ok && val.v == expected);

  // Write numeric and ensure prefix bytes untouched.
  check(hopper_field_set_i32(h, r.ref, 1, -1), HOPPER_OK);
  uint8_t out[6] = {0};
  hopper_bytes_mut_t b = {out, sizeof(out)};
  check(hopper_field_get_bytes(h, r.ref, 0, b), HOPPER_OK);
  assert(out[0] == 0x11 && out[1] == 0x22);
  // Overlapped region should be 0xFF for -1 LE 32-bit.
  assert(out[2] == 0xFF && out[3] == 0xFF && out[4] == 0xFF && out[5] == 0xFF);
}

static void test_scaled_comp_fields(void) {
  uint8_t arena[64];
  uint8_t ref_mem[128];
  uint8_t hopper_mem[hopper_sizeof()];
  hopper_config_t cfg = {
      .abi_version = HOPPER_ABI_VERSION,
      .arena_mem = arena,
      .arena_bytes = sizeof(arena),
      .ref_mem = ref_mem,
      .ref_count = 4,
      .catalog = &g_catalog,
  };
  hopper_t *h = NULL;
  assert(hopper_init(hopper_mem, &cfg, &h) == HOPPER_OK);
  hopper_result_ref_t r = hopper_record(h, 5);
  assert(r.ok);

  // COMP with scale: digits=4, scale=1. Value 1234 (->123.4) fits.
  check(hopper_field_set_i32(h, r.ref, 0, 1234), HOPPER_OK);
  hopper_result_i32_t cv = hopper_field_get_i32(h, r.ref, 0);
  assert(cv.ok && cv.v == 1234);
  // Overflow at 10000.
  assert(hopper_field_set_i32(h, r.ref, 0, 10000) == HOPPER_E_OVERFLOW);

  // COMP-3 with scale: digits=5, scale=1. 54321 fits; 100000 overflows.
  check(hopper_field_set_i32(h, r.ref, 1, 54321), HOPPER_OK);
  hopper_result_i32_t pv = hopper_field_get_i32(h, r.ref, 1);
  assert(pv.ok && pv.v == 54321);
  assert(hopper_field_set_i32(h, r.ref, 1, 100000) == HOPPER_E_OVERFLOW);
}

static void test_overlay_separate(void) {
  uint8_t arena[64];
  uint8_t ref_mem[128];
  uint8_t hopper_mem[hopper_sizeof()];
  hopper_config_t cfg = {
      .abi_version = HOPPER_ABI_VERSION,
      .arena_mem = arena,
      .arena_bytes = sizeof(arena),
      .ref_mem = ref_mem,
      .ref_count = 4,
      .catalog = &g_catalog,
  };
  hopper_t *h = NULL;
  assert(hopper_init(hopper_mem, &cfg, &h) == HOPPER_OK);
  hopper_result_ref_t r = hopper_record(h, 6);
  assert(r.ok);

  check(hopper_field_set_i32(h, r.ref, 0, 1234), HOPPER_OK);
  const uint8_t payload[4] = {'A', 'B', 'C', 'D'};
  check(hopper_field_set_bytes(h, r.ref, 1, (hopper_bytes_t){payload, sizeof(payload)}), HOPPER_OK);

  hopper_result_i32_t num = hopper_field_get_i32(h, r.ref, 0);
  assert(num.ok && num.v == 1234);
  uint8_t out[4] = {0};
  hopper_bytes_mut_t buf = {out, sizeof(out)};
  check(hopper_field_get_bytes(h, r.ref, 1, buf), HOPPER_OK);
  assert(memcmp(out, "ABCD", 4) == 0);
}

static void test_scale_overflow(void) {
  uint8_t arena[64];
  uint8_t ref_mem[128];
  uint8_t hopper_mem[hopper_sizeof()];

  hopper_config_t cfg = {
      .abi_version = HOPPER_ABI_VERSION,
      .arena_mem = arena,
      .arena_bytes = sizeof(arena),
      .ref_mem = ref_mem,
      .ref_count = 4,
      .catalog = &g_catalog,
  };
  hopper_t *h = NULL;
  assert(hopper_init(hopper_mem, &cfg, &h) == HOPPER_OK);
  hopper_result_ref_t r = hopper_record(h, 1);
  assert(r.ok);

  // Scaled field digits=5, scale=2 => max magnitude 99999 -> 999.99; try overflow.
  check(hopper_field_set_i32(h, r.ref, 5, 99999), HOPPER_OK);
  hopper_err_t ov = hopper_field_set_i32(h, r.ref, 5, 100000); // one too big
  assert(ov == HOPPER_E_OVERFLOW);

  // COMP field digits=4: overflow when value requires >4 digits.
  assert(hopper_field_set_i32(h, r.ref, 3, 9999) == HOPPER_OK);
  assert(hopper_field_set_i32(h, r.ref, 3, 10000) == HOPPER_E_OVERFLOW);

  // COMP-3 field digits=5: overflow at 100000
  assert(hopper_field_set_i32(h, r.ref, 4, 99999) == HOPPER_OK);
  assert(hopper_field_set_i32(h, r.ref, 4, 100000) == HOPPER_E_OVERFLOW);
}

int main(void) {
  test_all();
  test_oom_and_reset();
  test_bounds_and_invalid();
  test_comp3_bad_sign();
  test_overlay_bounds();
  test_bad_field_bounds();
  test_partial_overlap();
  test_scaled_comp_fields();
  test_overlay_separate();
  test_mask_zero_suppression();
  test_scale_overflow();
  printf("All Hopper tests passed.\n");
  return 0;
}
