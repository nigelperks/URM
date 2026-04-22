// URM front end

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>

static void fatal(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  exit(EXIT_FAILURE);
}

enum token_type {
  TOK_INVALID,
  TOK_EOL,
  TOK_VAR,
  TOK_NUM,
  // keywords
  TOK_ZERO,
};

static const struct keyword {
  char* name;
  int token;
} keywords[] = {
  { "zero", TOK_ZERO },
  { NULL, TOK_INVALID },
};

struct codegen {
  unsigned nreg;
  unsigned nline;
};

static void parse_statement(struct codegen *, unsigned lineno, char*);

int main(void) {
  char buf[128];
  unsigned lineno = 0;

  struct codegen gen;
  gen.nline = 0;
  gen.nreg = 0;

  while (fgets(buf, sizeof buf, stdin)) {
    lineno++;
    parse_statement(&gen, lineno, buf);
  }
  return 0;
}

static void error(unsigned lineno, const char* token, const char* msg) {
  fatal("line %u: %s: %s\n", lineno, token, msg);
}

struct token {
  int type;
  char* text;
};

static struct token get_token(char* start, char* *next);

static char* zero_statement(struct codegen * gen, unsigned lineno, char* args);

static void parse_statement(struct codegen * gen, unsigned lineno, char* buf) {
  char* start = buf;
  char* next;
  struct token tok = get_token(start, &next);
  if (tok.type != TOK_EOL) {
    switch (tok.type) {
      case TOK_ZERO: next = zero_statement(gen, lineno, next); break;
      default: error(lineno, tok.text, "statement expected");
    }
    tok = get_token(next, &next);
    if (tok.type != TOK_EOL)
      error(lineno, tok.text, "end of line expected");
  }
}

struct symbol {
  char* name;
  unsigned reg;
};

#define MAX_SYMBOLS (32)

static struct symbol symtab[MAX_SYMBOLS];
static unsigned nsym = 0;

static struct symbol * lookup(const char* name) {
  for (unsigned i = 0; i < nsym; i++) {
    if (strcmp(symtab[i].name, name) == 0)
      return &symtab[i];
  }
  return NULL;
}

static struct symbol * insert(const char* name, unsigned reg, unsigned lineno) {
  symtab[nsym].name = _strdup(name); // TODO: check for null
  symtab[nsym].reg = reg;
  return &symtab[nsym++];
}

static void emit(struct codegen * gen, const char* fmt, ...) {
  gen->nline++;
  printf("%u ", gen->nline);

  va_list ap;
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);

  putchar('\n');
}

static char* zero_statement(struct codegen * gen, unsigned lineno, char* args) {
  char* next;
  struct token tok = get_token(args, &next);
  if (tok.type != TOK_VAR)
    error(lineno, tok.text, "variable expected");
  struct symbol * sym = lookup(tok.text);
  unsigned reg;
  if (sym)
    reg = sym->reg;
  else {
    gen->nreg++;
    reg = gen->nreg;
    insert(tok.text, reg, lineno);
  }
  emit(gen, "Z(%u)", reg);
  return next;
}

static char* demarcate_token(char* start, char* *next);

static struct token get_token(char* start, char* *next) {
  struct token tok;

  tok.text = demarcate_token(start, next);
  if (*tok.text == '\0' || *tok.text == '\n' || *tok.text == ';') {
    tok.type = TOK_EOL;
    return tok;
  }

  for (const struct keyword * k = keywords; k->name; k++) {
    if (strcmp(k->name, start) == 0) {
      tok.type = k->token;
      return tok;
    }
  }

  bool alpha = true;
  bool numeric = true;
  for (const char* p = start; *p; p++) {
    if (!isalpha(*p))
      alpha = false;
    if (!isdigit(*p))
      numeric = false;
  }
  if (alpha) {
    tok.type = TOK_VAR;
    return tok;
  }
  if (numeric) {
    tok.type = TOK_NUM;
    return tok;
  }
  tok.type = TOK_INVALID;
  return tok;
}

static char* demarcate_token(char* start, char* *next) {
  while (isspace(*start))
    start++;
  char *end = start;
  while (*end != '\0' && !isspace(*end))
    end++;
  if (*end == '\0')
    *next = end;
  else {
    *end = '\0';
    *next = end + 1;
  }
  return start;
}
