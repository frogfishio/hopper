/* Internal shared definitions for zing codegen. */
#ifndef ZING_CODEGEN_INTERNAL_H
#define ZING_CODEGEN_INTERNAL_H

#include <stdio.h>
#include <stdint.h>
#include <zing_ast.h>
#include "../expr_pratt.h"
#include <limits.h>

#if defined(__GNUC__) || defined(__clang__)
#define ZING_UNUSED __attribute__((unused))
#else
#define ZING_UNUSED
#endif

/* Forward decls for AST types we reference. */
typedef struct Expr Expr;
typedef struct ZingName ZingName;
typedef struct ZingMethod ZingMethod;
typedef struct ZingExtern ZingExtern;
typedef struct ZingUse ZingUse;
typedef struct ZingConst ZingConst;
typedef struct ZingEnum ZingEnum;
typedef struct ZingEnumCase ZingEnumCase;
typedef struct ZingLayout ZingLayout;
typedef struct ZingLayoutField ZingLayoutField;
typedef struct ZingStruct ZingStruct;
typedef struct SymVal SymVal;
typedef struct ZingTest ZingTest;

typedef struct StrLit StrLit;
typedef struct Var Var;
typedef struct IntType IntType;
typedef struct BlockDef BlockDef;

struct StrLit {
  char *text;
  size_t len;
  char *label;
  StrLit *next;
};

struct Var {
  char *name;
  char *label;
  int width;
  int is_unsigned;
  int emit_data;
  Var *next;
};

struct IntType {
  int width;
  int is_unsigned;
};

struct BlockDef {
  Expr *expr;
  int id;
  char *label;
  char *method_label;
  const char *module_id;
  const char *method_name;
  const ZingName *method_params;
  BlockDef *next;
};

typedef enum {
  REC_LABEL = 0,
  REC_INSTR = 1,
  REC_DIR = 2
} RecKind;

typedef enum {
  OP_SYM = 0,
  OP_NUM = 1,
  OP_STR = 2,
  OP_MEM = 3
} OpKind;

typedef struct {
  OpKind kind;
  char *sym;
  int64_t num;
  uint64_t unum;
  int is_unsigned;
} RecOp;

typedef struct {
  RecKind kind;
  char *name;
  char *mnem;
  char *dir;
  RecOp *ops;
  size_t op_count;
} ZasmRec;

typedef struct {
  FILE *out;
  StrLit *strings;
  Var *vars;
  BlockDef *blocks;
  int block_count;
  size_t label_id;
  size_t temp_depth;
  size_t temp_max;
  int needs_itoa;
  int in_block;
  const char *method_name;
  const ZingName *method_params;
  const char *method_label;
  const char *module_id;
  ZingMethod *methods;
  ZingExtern *externs;
  ZingUse *uses;
  ZingConst *consts;
  ZingEnum *enums;
  ZingTest *tests;
  ZasmRec *recs;
  size_t rec_count;
  size_t rec_cap;
  int emit_mode;
  int emit_tests;
} Codegen;

/* Layout usage kinds shared across emitters/hopper. */
enum {
  LAYOUT_USAGE_DISPLAY = 0,
  LAYOUT_USAGE_COMP3 = 1,
  LAYOUT_USAGE_COMP = 2
};

typedef enum {
  FMT_RRR = 0,
  FMT_RRI12 = 1,
  FMT_R = 2,
  FMT_MEM = 3,
  FMT_STORE = 4,
  FMT_J = 5,
  FMT_IMM = 6,
  FMT_DIR = 7
} OpFormat;

typedef struct {
  const char *mnem;
  uint8_t op;
  OpFormat fmt;
} OpInfo;

const OpInfo *find_op_info(const char *mnem);
int reg_index(const char *sym);
int cond_code(const char *sym);
int instr_required_ext(const ZasmRec *rec, SymVal *code_words,
                       size_t code_words_count, SymVal *data_syms,
                       size_t data_syms_count, SymVal *equ_syms,
                       size_t equ_syms_count, size_t word_index);
int require_reg(const char *sym, const char *mnem);
int fits_i12(int64_t v);
int fits_i32(int64_t v);
const char *strip_symbol_hash(const char *sym);
ZingLayout *find_layout_local(Codegen *ctx, const char *name);
ZingLayoutField *find_layout_field_global(const char *name,
                                          ZingLayout **out_layout);
int layout_field_exists(const char *name);
ZingStruct *find_struct(Codegen *ctx, const char *module_id,
                        const char *name);
size_t name_list_count(const ZingName *list);
size_t expr_arg_count(const Expr *expr);
int name_in_list(ZingName *list, const char *name);
int resolve_result_enum(Codegen *ctx, ZingEnum **enum_out, int *ok_idx,
                        int *fail_idx);
int enum_max_fields(const ZingEnum *def);
int enum_case_field_count(const ZingEnumCase *c);
int layout_index(const ZingLayout *layout);
int resolve_err_enum(Codegen *ctx, ZingEnum **enum_out, int *err_idx);
char *enum_pool_label(const char *module_id, const char *name,
                      const char *suffix);
int enum_instance_size(const ZingEnum *def);
const char *module_id_or_default(const char *module_id);
const char *resolve_module_alias(Codegen *ctx, const char *alias);
char *sanitize_label(const char *label);
char *strip_kw_dup(const char *kw);
const char *ensure_var(Codegen *ctx, const char *name);
const char *ensure_var_in_method(Codegen *ctx, const char *method,
                                 const char *name);
int expr_is_unit(const Expr *expr);
IntType int_type_from_expr(Codegen *ctx, const Expr *expr);
IntType int_type_combine(IntType a, IntType b);
void set_var_type(Codegen *ctx, const char *name, IntType t);
void set_var_type_key(Codegen *ctx, const char *key, IntType type);
void set_var_type_for_scope(Codegen *ctx, const char *scope,
                            const char *name, IntType type);
const char *ensure_global(Codegen *ctx, const char *name);
IntType int_type_default(void);
void method_ret_key(const char *module_id, const char *name,
                    const ZingName *params, char *out, size_t cap);
IntType method_ret_type(Codegen *ctx, const char *module_id,
                        const char *name, const ZingName *params);
BlockDef *register_block(Codegen *ctx, Expr *expr);
void analyze_method_types(Codegen *ctx, ZingMethod *method);
int is_builtin_extern_name(const char *name);
const char *method_symbol(Codegen *ctx, const char *module_id,
                          const char *name, const ZingName *params);
ZingMethod *find_method_for_call(Codegen *ctx, const char *module_id,
                                 const char *name, const Expr *expr);
ZingMethod *find_method_by_arity(Codegen *ctx, const char *module_id,
                                 const char *name, size_t arity);
const char *resolve_open_import_call(Codegen *ctx, const char *name);
const char *resolve_open_import_struct(Codegen *ctx, const char *name);
ZingConst *find_const(Codegen *ctx, const char *module_id, const char *name);
int has_var(Codegen *ctx, const char *name);
Var *find_var_node(Codegen *ctx, const char *name);
Var *find_var_node_by_key(Codegen *ctx, const char *key);
const char *method_ret_label(Codegen *ctx, const char *module_id,
                             const char *name, const ZingName *params);
void build_method_sig(const char *name, const ZingName *params, char *out,
                      size_t cap);
int emit_main_auto_dump(Codegen *ctx, const Expr *expr, int strict);
ZingExtern *find_extern(Codegen *ctx, const char *module_id,
                        const char *name);
Expr *record_field_value(ExprField *fields, const char *name);
int emit_string_byte(Codegen *ctx, Expr *expr);
void emit_enum_construct(Codegen *ctx, ZingEnum *def,
                         const ZingEnumCase *ecase, int case_index,
                         ExprField *fields);
ExprField *expr_field_new(const char *name, Expr *value);
void emit_block(Codegen *ctx, Expr *block);
void emit_vec_at_slots(Codegen *ctx, size_t base_slot, size_t idx_slot);
int enum_field_exists(const ZingEnumCase *c, const char *name);
/* Hopper helpers used by emitters */
void emit_hopper_record(Codegen *ctx, Expr *layout_expr);
void emit_hopper_read_u8_at(Codegen *ctx, Expr *recv, Expr *offset);
void emit_hopper_read_u16_at(Codegen *ctx, Expr *recv, Expr *offset);
void emit_hopper_read_u32_at(Codegen *ctx, Expr *recv, Expr *offset);
void emit_hopper_write_u8_at(Codegen *ctx, Expr *recv, Expr *offset,
                             Expr *value);
void emit_hopper_write_u16_at(Codegen *ctx, Expr *recv, Expr *offset,
                              Expr *value);
void emit_hopper_write_u32_at(Codegen *ctx, Expr *recv, Expr *offset,
                              Expr *value);
void emit_hopper_field_get_comp3(Codegen *ctx, size_t ref_slot,
                                 const ZingLayoutField *field);
void emit_hopper_field_set_comp3(Codegen *ctx, size_t ref_slot,
                                 const ZingLayoutField *field, Expr *value);
void emit_hopper_field_get_comp(Codegen *ctx, size_t ref_slot,
                                const ZingLayoutField *field);
void emit_hopper_field_set_comp(Codegen *ctx, size_t ref_slot,
                                const ZingLayoutField *field, Expr *value);
void emit_hopper_field_get_numeric(Codegen *ctx, size_t ref_slot,
                                   const ZingLayoutField *field);
void emit_hopper_field_get_bytes(Codegen *ctx, size_t ref_slot,
                                 const ZingLayoutField *field);
void emit_hopper_field_set_bytes(Codegen *ctx, size_t ref_slot,
                                 const ZingLayoutField *field, Expr *value);
void emit_hopper_field_set_raw_bytes(Codegen *ctx, size_t ref_slot,
                                     const ZingLayoutField *field, Expr *value);
void emit_hopper_field_set_numeric(Codegen *ctx, size_t ref_slot,
                                   const ZingLayoutField *field, Expr *value);
void emit_hopper_field_format_display(Codegen *ctx, size_t ref_slot,
                                      const ZingLayoutField *field);

enum {
  HOPPER_ARENA_SIZE = 20000,
  HOPPER_REF_COUNT = 64,
  HOPPER_REF_SIZE = 12,
  BUFFER_CAP = 4096,
  STR_HEADER = 8,
  VEC_HEADER = 16,
  MAP_HEADER = 16,
  BUFFER_HEADER = 12,
  VEC_CAP = 256,
  MAP_CAP = 64,
  VEC_SIZE = VEC_HEADER + (VEC_CAP * 4),
  MAP_SIZE = MAP_HEADER + (MAP_CAP * 8),
  BUFFER_SIZE = BUFFER_HEADER + BUFFER_CAP,
  VEC_POOL_COUNT = 16,
  MAP_POOL_COUNT = 16,
  BUFFER_POOL_COUNT = 4,
  ENUM_POOL_COUNT = 16,
  TAG_VEC = 1,
  TAG_MAP = 2,
  TAG_STR = 3
};

int pic_pow10_i32(int scale, int *out);
char *make_string_literal(const char *raw, size_t len);

/* Rec helpers shared across compilation units. */
ZasmRec *rec_new(RecKind kind);
void rec_append(Codegen *ctx, ZasmRec *rec);
void rec_add_op(ZasmRec *rec, OpKind kind, const char *sym,
                int64_t num, uint64_t unum, int is_unsigned);
void rec_free(ZasmRec *rec);
void emit_label(Codegen *ctx, const char *name);
void emit_instr0(Codegen *ctx, const char *mnem);
void emit_instr_sym(Codegen *ctx, const char *mnem, const char *sym);
void emit_instr_sym_sym(Codegen *ctx, const char *mnem, const char *lhs,
                        const char *rhs);
void emit_instr_sym_num(Codegen *ctx, const char *mnem, const char *lhs,
                        int64_t rhs);
void emit_instr_sym_num_u64(Codegen *ctx, const char *mnem,
                            const char *lhs, uint64_t rhs);
void emit_instr_sym_mem(Codegen *ctx, const char *mnem, const char *lhs,
                        const char *base);
void emit_instr_mem_sym(Codegen *ctx, const char *mnem, const char *base,
                        const char *rhs);
void emit_jr(Codegen *ctx, const char *label);
void emit_jr_cond(Codegen *ctx, const char *cond, const char *label);

/* String helpers */
const char *get_symbol_label(Codegen *ctx, const char *literal,
                             size_t *out_len);
const char *get_string_label(Codegen *ctx, const char *literal,
                             const char *tag, size_t *out_len);
void emit_string_data(Codegen *ctx);
int parse_string_literal_tagged(const char *literal, const char *tag,
                                char **out_str, size_t *out_len);

/* Cross-file emitter helpers used by the front-end */
ZingEnumCase *find_enum_case_global(Codegen *ctx, const char *module_id,
                                    const char *name, ZingEnum **out_enum,
                                    int *out_index);
void emit_load_label(Codegen *ctx, const char *label);
void emit_store_label(Codegen *ctx, const char *label);
void emit_store_temp(Codegen *ctx, size_t slot);
void emit_load_ptr_offset(Codegen *ctx, size_t base_slot, int offset);
void emit_print_str(Codegen *ctx, Expr *expr);
void emit_stream_copy_to(Codegen *ctx, Expr *src, Expr *dst);
void emit_expr(Codegen *ctx, Expr *expr);

#endif /* ZING_CODEGEN_INTERNAL_H */
