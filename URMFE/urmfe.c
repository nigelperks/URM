// URM frontend
// Nigel Perks, 2026

// A frontend language for URM.
// Avoid working with instruction numbers and register numbers.
// Allocate instruction numbers to labels, register numbers to variables.

// fred:
//
// zero foo                    --> Z(n)
// succ foo                    --> S(n)
// inc foo                     --> S(n)
// copy foo to bar             --> C(n,m)
// goto fred                   --> J(1,1,p)
// if foo = bar then goto fred --> J(n,m,p)
//
// foo := 0                    --> Z(n)
// foo := foo + 1              --> S(n)
// bar := foo                  --> C(n,m)
//
// foo := n                    --> Z(n), S(n), S(n), ...
// inc foo, n                  --> S(n), S(n), ...
// foo := foo + n              --> S(n), S(n), ...
//
// declare n, m ; allocate next registers to variables n, m
//
// if x != y                   --> J(x,y,out_label)
// ...
// end if
//
// while x != y                --> J(x,y,out_label)
// ...
// end while                   --> J(1,1,test_label)
//
// for i := k to n             --> Z(i), S(i), S(i), ..., J(i,n,out_label)
// ...
// end for                     --> S(i), J(1,1,test_label)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <assert.h>

static void fatal(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  exit(EXIT_FAILURE);
}

static void error(unsigned lineno, const char* token, const char* fmt, ...) {
  fprintf(stderr, "line %u: ", lineno);
  if (token)
    fprintf(stderr, "%s: ", token);

  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  putc('\n', stderr);

  exit(EXIT_FAILURE);
}

enum token_type {
  TOK_INVALID = 256,
  TOK_EOL,
  TOK_ID,
  TOK_NUM,
  TOK_ASSIGN,
  TOK_UNEQUAL,
  // keywords
  TOK_COPY,
  TOK_DECLARE,
  TOK_END,
  TOK_FOR,
  TOK_GOTO,
  TOK_IF,
  TOK_INC,
  TOK_THEN,
  TOK_TO,
  TOK_WHILE,
  TOK_ZERO,
};

static const struct keyword {
  char* name;
  int token;
} keywords[] = {
  { "copy", TOK_COPY },
  { "declare", TOK_DECLARE },
  { "end", TOK_END },
  { "for", TOK_FOR },
  { "goto", TOK_GOTO },
  { "if", TOK_IF },
  { "inc", TOK_INC },
  { "succ", TOK_INC },
  { "then", TOK_THEN },
  { "to", TOK_TO },
  { "while", TOK_WHILE },
  { "zero", TOK_ZERO },
  { NULL, TOK_INVALID },
};

enum inst_type { URM_NOOP, URM_ZERO, URM_SUCC, URM_COPY, URM_JUMP };

struct inst {
  unsigned type;
  unsigned reg1;
  unsigned reg2;
  struct label * label;
};

#define MAX_INST (128)

struct codegen {
  unsigned nreg;
  struct inst inst[MAX_INST];
  unsigned ninst;
};

static void init_codegen(struct codegen * gen) {
  gen->nreg = 0;
  gen->inst[0].type = URM_NOOP;
  gen->ninst = 1;
}

#define MAX_CTRL (4)

struct ctrl_struct {
  int type; // type of control structure: TOK_IF, TOK_WHILE
  unsigned lineno;
  struct label * test;
  struct label * out;
  unsigned reg;
};

struct parser {
  struct ctrl_struct ctrl[MAX_CTRL];
  unsigned nctrl;
};

static void init_parser(struct parser * p) {
  p->nctrl = 0;
}

static void parse_statement(struct parser *, struct codegen *, unsigned lineno, char*);
static void check_control_structure(struct parser *);

static void check_undefined_labels(struct codegen *);
static void output_program(struct codegen *);

int main(int argc, char* argvp[]) {
  if (argc > 1)
    fatal("usage: URMFE < input.ufe > ouput.urm\n");

  char buf[128];
  unsigned lineno = 0;

  struct codegen gen;
  init_codegen(&gen);

  struct parser parser;
  init_parser(&parser);

  while (fgets(buf, sizeof buf, stdin)) {
    lineno++;
    parse_statement(&parser, &gen, lineno, buf);
  }

  check_control_structure(&parser);
  check_undefined_labels(&gen);
  output_program(&gen);

  return 0;
}

static void check_control_structure(struct parser * parser) {
  if (parser->nctrl) {
    struct ctrl_struct * cs = &parser->ctrl[parser->nctrl - 1];
    const char* name = NULL;
    switch (cs->type) {
      case TOK_IF: name = "IF"; break;
      case TOK_WHILE: name = "WHILE"; break;
    }
    error(cs->lineno, name, "unterminated control structure");
  }
}

#define MAX_TEXT (32)

struct lex {
  unsigned lineno;
  const char* next;
  char text[MAX_TEXT];
};

static void init_lex(struct lex * lex, unsigned lineno, const char* buf) {
  lex->lineno = lineno;
  lex->next = buf;
  lex->text[0] = '\0';
}

static int get_token(struct lex *);

static void copy_statement(struct codegen * gen, struct lex *);
static void declare_statement(struct codegen * gen, struct lex *);
static void end_statement(struct parser *, struct codegen * gen, struct lex *);
static void for_statement(struct parser *, struct codegen * gen, struct lex *);
static void goto_statement(struct codegen * gen, struct lex *);
static void if_statement(struct parser * parser, struct codegen * gen, struct lex *);
static void inc_statement(struct codegen * gen, struct lex *);
static void while_statement(struct parser *, struct codegen * gen, struct lex *);
static void zero_statement(struct codegen * gen, struct lex *);
static void identifier_statement(struct codegen * gen, struct lex *);

static void parse_statement(struct parser * parser, struct codegen * gen, unsigned lineno, char* buf) {
  struct lex lex;
  init_lex(&lex, lineno, buf);
  int tok = get_token(&lex);
  if (tok != TOK_EOL) {
    switch (tok) {
      case TOK_COPY: copy_statement(gen, &lex); break;
      case TOK_DECLARE: declare_statement(gen, &lex); break;
      case TOK_END: end_statement(parser, gen, &lex); break;
      case TOK_FOR: for_statement(parser, gen, &lex); break;
      case TOK_GOTO: goto_statement(gen, &lex); break;
      case TOK_IF: if_statement(parser, gen, &lex); break;
      case TOK_INC: inc_statement(gen, &lex); break;
      case TOK_WHILE: while_statement(parser, gen, &lex); break;
      case TOK_ZERO: zero_statement(gen, &lex); break;
      case TOK_ID: identifier_statement(gen, &lex); break;
      default: error(lineno, lex.text, "statement expected");
    }
    tok = get_token(&lex);
    if (tok != TOK_EOL)
      error(lineno, lex.text, "end of line expected");
  }
}

struct symbol {
  char* name;
  unsigned reg;
};

#define MAX_SYMBOLS (32)

static struct symbol symtab[MAX_SYMBOLS];
static unsigned nsym = 0;

static struct symbol * lookup_symbol(const char* name) {
  for (unsigned i = 0; i < nsym; i++) {
    if (strcmp(symtab[i].name, name) == 0)
      return &symtab[i];
  }
  return NULL;
}

static struct symbol * insert_symbol(const char* name, unsigned reg, unsigned lineno) {
  if (nsym >= MAX_SYMBOLS)
    error(lineno, name, "too many variables");
  symtab[nsym].name = _strdup(name); // TODO: check for null
  symtab[nsym].reg = reg;
  return &symtab[nsym++];
}

struct label {
  char* name;
  unsigned inst;
};

#define MAX_LABELS (32)

static struct label labels[MAX_LABELS];
static unsigned nlabel = 0;

static struct label * lookup_label(const char* name) {
  for (unsigned i = 0; i < nlabel; i++) {
    if (strcmp(labels[i].name, name) == 0)
      return &labels[i];
  }
  return NULL;
}

static struct label * insert_label(const char* name, unsigned inst, unsigned lineno) {
  if (nlabel >= MAX_LABELS)
    error(lineno, name, "too many labels");
  labels[nlabel].name = _strdup(name); // TODO: check for null
  labels[nlabel].inst = inst;
  return &labels[nlabel++];
}

static struct label * gen_label(unsigned inst, unsigned lineno) {
  char name[16];
  static unsigned count = 0;
  sprintf(name, "_%u", count++);
  return insert_label(name, inst, lineno);
}

static unsigned variable_register(struct codegen * gen, const char* var, unsigned lineno) {
  struct symbol * sym = lookup_symbol(var);
  unsigned reg;
  if (sym)
    reg = sym->reg;
  else {
    gen->nreg++;
    reg = gen->nreg;
    insert_symbol(var, reg, lineno);
  }
  return reg;
}

static void declare_variable(struct codegen * gen, const char* var, unsigned lineno) {
  struct symbol * sym = lookup_symbol(var);
  unsigned reg;
  if (sym)
    error(lineno, var, "variable already used");
  gen->nreg++;
  reg = gen->nreg;
  insert_symbol(var, reg, lineno);
}

static struct inst * new_inst(struct codegen * gen, unsigned lineno) {
  if (gen->ninst >= MAX_INST)
    error(lineno, NULL, "program at maximum size");
  return &gen->inst[gen->ninst++];
}

static void emit_zero(struct codegen * gen, unsigned reg, unsigned lineno) {
  struct inst * i = new_inst(gen, lineno);
  i->type = URM_ZERO;
  i->reg1 = reg;
}

static void emit_succ(struct codegen * gen, unsigned reg, unsigned lineno) {
  struct inst * i = new_inst(gen, lineno);
  i->type = URM_SUCC;
  i->reg1 = reg;
}

static void emit_copy(struct codegen * gen, unsigned src, unsigned dst, unsigned lineno) {
  struct inst * i = new_inst(gen, lineno);
  i->type = URM_COPY;
  i->reg1 = src;
  i->reg2 = dst;
}

static void emit_jump(struct codegen * gen, unsigned reg1, unsigned reg2, struct label * label, unsigned lineno) {
  struct inst * i = new_inst(gen, lineno);
  i->type = URM_JUMP;
  i->reg1 = reg1;
  i->reg2 = reg2;
  i->label = label;
}

static void check_undefined_labels(struct codegen * gen) {
  unsigned err = 0;
  for (unsigned i = 1; i < gen->ninst; i++) {
    if (gen->inst[i].type == URM_JUMP && gen->inst[i].label->inst == 0) {
      fprintf(stderr, "undefined label: %s\n", gen->inst[i].label->name);
      err++;
    }
  }
  if (err)
    fatal("%u undefined labels\n", err);
}

static void output_program(struct codegen * gen) {
  for (unsigned i = 1; i < gen->ninst; i++) {
    printf("%u ", i);
    struct inst * inst = &gen->inst[i];
    switch (inst->type) {
      case URM_ZERO: printf("Z(%u)", inst->reg1); break;
      case URM_SUCC: printf("S(%u)", inst->reg1); break;
      case URM_COPY: printf("C(%u,%u)", inst->reg1, inst->reg2); break;
      case URM_JUMP: printf("J(%u,%u,%u)", inst->reg1, inst->reg2, inst->label->inst); break;
      default: assert(0 && "unexpected URM instruction"); break;
    }
    putchar('\n');
  }
}

static void get_variable(struct lex *);
static unsigned long get_number(struct lex *);
static void match(int expected, const char* descrip, struct lex *);

static void zero_statement(struct codegen * gen, struct lex * lex) {
  get_variable(lex);
  unsigned reg = variable_register(gen, lex->text, lex->lineno);
  emit_zero(gen, reg, lex->lineno);
}

static void inc_statement(struct codegen * gen, struct lex * lex) {
  get_variable(lex);
  unsigned reg = variable_register(gen, lex->text, lex->lineno);
  unsigned long n = 1;
  int tok = get_token(lex);
  if (tok == ',')
    n = get_number(lex);
  while (n--)
    emit_succ(gen, reg, lex->lineno);
}

static void copy_statement(struct codegen * gen, struct lex * lex) {
  get_variable(lex);
  unsigned src = variable_register(gen, lex->text, lex->lineno);
  match(TOK_TO, "TO", lex);
  get_variable(lex);
  unsigned dst = variable_register(gen, lex->text, lex->lineno);
  emit_copy(gen, src, dst, lex->lineno);
}

static void goto_statement(struct codegen * gen, struct lex * lex) {
  match(TOK_ID, "label", lex);
  struct label * label = lookup_label(lex->text);
  if (label == NULL)
    label = insert_label(lex->text, 0, lex->lineno);
  emit_jump(gen, 1, 1, label, lex->lineno);
}

static struct ctrl_struct * control_structure(struct parser * parser, const char* token, unsigned lineno) {
  if (parser->nctrl >= MAX_CTRL)
    error(lineno, token, "maximum control structure nesting exceeded");
  return &parser->ctrl[parser->nctrl++];
}

static void if_statement(struct parser * parser, struct codegen * gen, struct lex * lex) {
  get_variable(lex);
  unsigned reg1 = variable_register(gen, lex->text, lex->lineno);

  int tok = get_token(lex);

  if (tok == '=') {
    // IF a = b THEN GOTO k
    get_variable(lex);
    unsigned reg2 = variable_register(gen, lex->text, lex->lineno);
    match(TOK_THEN, "THEN", lex);
    match(TOK_GOTO, "GOTO", lex);
    match(TOK_ID, "label", lex);
    struct label * label = lookup_label(lex->text);
    if (label == NULL)
      label = insert_label(lex->text, 0, lex->lineno);
    emit_jump(gen, reg1, reg2, label, lex->lineno);
    return;
  }

  if (tok == TOK_UNEQUAL) {
    // IF a != b THEN ... END IF
    get_variable(lex);
    unsigned reg2 = variable_register(gen, lex->text, lex->lineno);
    match(TOK_THEN, "THEN", lex);
    match(TOK_EOL, "end of line", lex);
    struct ctrl_struct * cs = control_structure(parser, "IF", lex->lineno);
    cs->lineno = lex->lineno;
    cs->type = TOK_IF;
    cs->test = NULL;
    cs->out = gen_label(0, lex->lineno);
    cs->reg = 0;
    emit_jump(gen, reg1, reg2, cs->out, lex->lineno);
    return;
  }

  error(lex->lineno, lex->text, "equality or inequality test expected");
}

static void while_statement(struct parser * parser, struct codegen * gen, struct lex * lex) {
  get_variable(lex);
  unsigned reg1 = variable_register(gen, lex->text, lex->lineno);
  match(TOK_UNEQUAL, "inequality test", lex);
  get_variable(lex);
  unsigned reg2 = variable_register(gen, lex->text, lex->lineno);
  struct ctrl_struct * cs = control_structure(parser, "WHILE", lex->lineno);
  cs->lineno = lex->lineno;
  cs->type = TOK_WHILE;
  cs->test = gen_label(gen->ninst, lex->lineno);
  cs->out = gen_label(0, lex->lineno);
  cs->reg = 0;
  emit_jump(gen, reg1, reg2, cs->out, lex->lineno);
}

static void for_statement(struct parser * parser, struct codegen * gen, struct lex * lex) {
  get_variable(lex);
  unsigned index = variable_register(gen, lex->text, lex->lineno);
  match(TOK_ASSIGN, ":=", lex);
  unsigned long n = get_number(lex);
  match(TOK_TO, "TO", lex);
  get_variable(lex);
  unsigned limit = variable_register(gen, lex->text, lex->lineno);
  struct ctrl_struct * cs = control_structure(parser, "FOR", lex->lineno);
  cs->lineno = lex->lineno;
  cs->type = TOK_FOR;
  emit_zero(gen, index, lex->lineno);
  while (n--)
    emit_succ(gen, index, lex->lineno);
  cs->test = gen_label(gen->ninst, lex->lineno);
  cs->out = gen_label(0, lex->lineno);
  cs->reg = index;
  emit_jump(gen, index, limit, cs->out, lex->lineno);
}

static void end_statement(struct parser * parser, struct codegen * gen, struct lex * lex) {
  int tok = get_token(lex);
  if (tok != TOK_FOR && tok != TOK_IF && tok != TOK_WHILE)
    error(lex->lineno, lex->text, "FOR, IF or WHILE expected");
  if (parser->nctrl == 0)
    error(lex->lineno, "END", "no control structure is in effect");
  parser->nctrl--;
  struct ctrl_struct * cs = &parser->ctrl[parser->nctrl];
  if (tok != cs->type)
    error(lex->lineno, lex->text, "mismatched control structures");
  switch (cs->type) {
    case TOK_FOR:
      emit_succ(gen, cs->reg, lex->lineno);
      emit_jump(gen, 1, 1, cs->test, lex->lineno);
      break;
    case TOK_WHILE:
      emit_jump(gen, 1, 1, cs->test, lex->lineno);
      break;
  }
  cs->out->inst = gen->ninst;
}

static void define_label(struct codegen *, const char* id, unsigned lineno);
static void assignment(struct codegen *, struct lex *, const char* id);

static void identifier_statement(struct codegen * gen, struct lex * lex) {
  char id[MAX_TEXT];
  strcpy(id, lex->text);
  int tok = get_token(lex);
  switch (tok) {
    case ':': define_label(gen, id, lex->lineno); break;
    case TOK_ASSIGN: assignment(gen, lex, id); break;
    default: error(lex->lineno, id, "label definition or variable assignment expected"); break;
  }
}

static void assign_number(struct codegen *, struct lex *, unsigned dst);
static void assign_variable(struct codegen *, struct lex *, unsigned dst);

// a := 0
// a := a + 1
// a := b
static void assignment(struct codegen * gen, struct lex * lex, const char* id) {
  unsigned dst = variable_register(gen, id, lex->lineno);
  switch (get_token(lex)) {
    case TOK_NUM: assign_number(gen, lex, dst); break;
    case TOK_ID: assign_variable(gen, lex, dst); break;
    default: error(lex->lineno, lex->text, "zero, copy, or increment expected");
  }
}

// a := 0
static void assign_number(struct codegen * gen, struct lex * lex, unsigned dst) {
  emit_zero(gen, dst, lex->lineno);
  unsigned long n = strtoul(lex->text, NULL, 10);
  while (n--)
    emit_succ(gen, dst, lex->lineno);
}

// a := a + 1
// a := b
static void assign_variable(struct codegen * gen, struct lex * lex, unsigned dst) {
   unsigned src = variable_register(gen, lex->text, lex->lineno);
   int tok = get_token(lex);
   if (tok == TOK_EOL) {
     emit_copy(gen, src, dst, lex->lineno);
     return;
   }
   if (tok != '+')
     error(lex->lineno, lex->text, "increment expected");
   unsigned inc = get_number(lex);
   if (src != dst)
     error(lex->lineno, lex->text, "malformed copy or increment");
   while (inc--)
     emit_succ(gen, dst, lex->lineno);
}

static void define_label(struct codegen * gen, const char* id, unsigned lineno) {
  struct label * label = lookup_label(id);
  if (label) {
    if (label->inst)
      error(lineno, id, "label already defined");
    label->inst = gen->ninst;
  }
  else
    insert_label(id, gen->ninst, lineno);
}

static void declare_statement(struct codegen * gen, struct lex * lex) {
  get_variable(lex);
  declare_variable(gen, lex->text, lex->lineno);
  int tok;
  while ((tok = get_token(lex)) == ',') {
    get_variable(lex);
    declare_variable(gen, lex->text, lex->lineno);
  }
  if (tok != TOK_EOL)
    error(lex->lineno, lex->text, "comma or end of line expected");
}

static void match(int expected, const char* descrip, struct lex * lex) {
  int tok = get_token(lex);
  if (tok != expected)
    error(lex->lineno, lex->text, "%s expected", descrip);
}

static void get_variable(struct lex * lex) {
  match(TOK_ID, "variable", lex);
}

static unsigned long get_number(struct lex * lex) {
  int tok = get_token(lex);
  if (tok != TOK_NUM)
    error(lex->lineno, lex->text, "number expected");
  return strtoul(lex->text, NULL, 10);
}

static int get_token(struct lex * lex) {
  while (isspace(*lex->next))
    lex->next++;

  const char* start = lex->next;
  int tok;

  if (isalpha(*lex->next) || *lex->next == '_') {
    do {
      lex->next++;
    } while (isalnum(*lex->next) || *lex->next == '_');
    tok = TOK_ID;
  }
  else if (isdigit(*lex->next)) {
    do {
      lex->next++;
    } while (isdigit(*lex->next));
    tok = TOK_NUM;
  }
  else if (*lex->next == '\0' || *lex->next == '\n' || *lex->next == ';')
    tok = TOK_EOL;
  else if (*lex->next == ':') {
    lex->next++;
    if (*lex->next == '=') {
      tok = TOK_ASSIGN;
      lex->next++;
    }
    else
      tok = ':';
  }
  else if (*lex->next == '!') {
    lex->next++;
    if (*lex->next == '=') {
      tok = TOK_UNEQUAL;
      lex->next++;
    }
    else
      tok = '!';
  }
  else {
    tok = *lex->next;
    lex->next++;
  }

  strncpy(lex->text, start, lex->next - start);
  lex->text[lex->next - start] = '\0';

  if (tok == TOK_ID) {
    for (const struct keyword * k = keywords; k->name; k++) {
      if (strcmp(k->name, lex->text) == 0) {
        tok = k->token;
        break;
      }
    }
  }

  return tok;
}
