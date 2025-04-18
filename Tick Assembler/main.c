#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "../Tick VM/tickvm.h"

#define MAX_LABELS    512
#define MAX_INSNS     1024
#define MAX_LINE_LEN  256

typedef struct {
    char* name;
    int     addr;
} Label;

typedef struct {
    char* mnemonic;
    char* op1;
    char* op2;
    int     lineno;
} RawInsn;

static Label     labels[MAX_LABELS];
static int       label_count = 0;

static RawInsn   raw[MAX_INSNS];
static int       raw_count = 0;

/* strdup isnt ANSIC, so we roll our own */
static char* my_strdup(const char* s) {
    char* p = (char*)malloc(strlen(s) + 1);
    if (p) strcpy(p, s);
    return p;
}

/* Trim leading/trailing whitespace in place */
static char* trim(char* s) {
    char* end;
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) {
        *end-- = '\0';
    }
    return s;
}

/* Look up a label by name; returns addr or 1 if not found */
static int find_label(const char* name) {
    int i;
    for (i = 0; i < label_count; i++) {
        if (strcmp(labels[i].name, name) == 0) {
            return labels[i].addr;
        }
    }
    return -1;
}

/* Add a new label; exits on duplicate or overflow */
static void add_label(const char* name, int addr, int lineno) {
    int i;
    if (label_count >= MAX_LABELS) {
        fprintf(stderr, "Line %d: too many labels\n", lineno);
        exit(1);
    }
    for (i = 0; i < label_count; i++) {
        if (strcmp(labels[i].name, name) == 0) {
            fprintf(stderr, "Line %d: duplicate label '%s'\n", lineno, name);
            exit(1);
        }
    }
    labels[label_count].name = my_strdup(name);
    labels[label_count].addr = addr;
    label_count++;
}

/* Parse one line: strip comments, extract optional label, then mnemonic+ops */
static void parse_line(char* line, int lineno) {
    char* p, * colon, * rest, * tok;

    /* strip comments */
    p = strchr(line, ';');
    if (p) *p = '\0';
    p = trim(line);
    if (*p == '\0') return;

    /* label? */
    colon = strchr(p, ':');
    if (colon) {
        *colon = '\0';
        add_label(trim(p), raw_count, lineno);
        p = trim(colon + 1);
        if (*p == '\0') return;
    }

    if (raw_count >= MAX_INSNS) {
        fprintf(stderr, "Line %d: too many instructions\n", lineno);
        exit(1);
    }

    /* record raw instruction */
    raw[raw_count].lineno = lineno;
    raw[raw_count].mnemonic = NULL;
    raw[raw_count].op1 = NULL;
    raw[raw_count].op2 = NULL;

    /* split mnemonic */
    tok = strtok(p, " \t");
    if (tok) {
        raw[raw_count].mnemonic = my_strdup(tok);
        rest = strtok(NULL, "");
        if (rest) {
            rest = trim(rest);
            /* split operands by comma */
            tok = strchr(rest, ',');
            if (tok) {
                *tok = '\0';
                raw[raw_count].op1 = my_strdup(trim(rest));
                raw[raw_count].op2 = my_strdup(trim(tok + 1));
            }
            else {
                raw[raw_count].op1 = my_strdup(rest);
            }
        }
        raw_count++;
    }
}

/* Parses tok  value; sets *is_reg or *is_label; exits on undefined label */
static uint32_t parse_operand(const char* tok,
    int* is_reg,
    int* is_label,
    int  lineno)
{
    char* endptr;
    long  v;

    if (tok == NULL) {
        *is_reg = 0;
        *is_label = 0;
        return 0;
    }

    /* register? R0R255 */
    if ((tok[0] == 'R' || tok[0] == 'r') && isdigit((unsigned char)tok[1])) {
        *is_reg = 1;
        *is_label = 0;
        return (uint32_t)atoi(tok + 1);
    }

    /* immediate integer (decimal or 0x...) */
    v = strtol(tok, &endptr, 0);
    if (*endptr == '\0') {
        *is_reg = 0;
        *is_label = 0;
        return (uint32_t)v;
    }

    /* otherwise, label */
    *is_reg = 0;
    *is_label = 1;
    v = find_label(tok);
    if ((int)v < 0) {
        fprintf(stderr, "Line %d: undefined label '%s'\n", lineno, tok);
        exit(1);
    }
    return (uint32_t)v;
}

int main(int argc, char** argv) {
    FILE* infile, * outfile;
    char           linebuf[MAX_LINE_LEN];
    int            lineno;
    int            i, pc;
    tick_instr_t   program[MAX_INSNS];

    if (argc != 3) {
        fprintf(stderr, "Usage: %s infile.asm outfile.bin\n", argv[0]);
        return 1;
    }

    infile = fopen(argv[1], "r");
    if (!infile) {
        perror("fopen");
        return 1;
    }

    /* Pass 1: read lines, record labels & raw instructions */
    lineno = 0;
    while (fgets(linebuf, sizeof(linebuf), infile)) {
        lineno++;
        parse_line(linebuf, lineno);
    }
    fclose(infile);

    /* Pass 2: assemble each RawInsn  tick_instr_t */
    pc = 0;
    for (i = 0; i < raw_count; i++) {
        RawInsn* ri = &raw[i];
        tick_instr_t* out = &program[pc];
        int         d_is_reg, d_is_label;
        int         s_is_reg, s_is_label;
        uint32_t    dval, sval;

        out->opcode = 0;
        out->arginfo = 0;
        out->destination = 0;
        out->source = 0;

        /* map mnemonic  opcode */
        if (strcmp(ri->mnemonic, "HLT") == 0) out->opcode = OP_HLT;
        else if (strcmp(ri->mnemonic, "MOV") == 0) out->opcode = OP_MOV;
        else if (strcmp(ri->mnemonic, "ADD") == 0) out->opcode = OP_ADD;
        else if (strcmp(ri->mnemonic, "SUB") == 0) out->opcode = OP_SUB;
        else if (strcmp(ri->mnemonic, "MUL") == 0) out->opcode = OP_MUL;
        else if (strcmp(ri->mnemonic, "DIV") == 0) out->opcode = OP_DIV;
        else if (strcmp(ri->mnemonic, "JMP") == 0) out->opcode = OP_JMP;
        else if (strcmp(ri->mnemonic, "JIZ") == 0) out->opcode = OP_JIZ;
        else if (strcmp(ri->mnemonic, "JNZ") == 0) out->opcode = OP_JNZ;
        else if (strcmp(ri->mnemonic, "EQL") == 0) out->opcode = OP_EQL;
        else if (strcmp(ri->mnemonic, "LDM") == 0) out->opcode = OP_LDM;
        else if (strcmp(ri->mnemonic, "STM") == 0) out->opcode = OP_STM;
        else if (strcmp(ri->mnemonic, "OUT") == 0) out->opcode = OP_OUT;
        else if (strcmp(ri->mnemonic, "PUT") == 0) out->opcode = OP_PUT;
        else {
            fprintf(stderr, "Line %d: unknown mnemonic '%s'\n",
                ri->lineno, ri->mnemonic);
            return 1;
        }

        /* destination operand (op1) */
        dval = parse_operand(ri->op1, &d_is_reg, &d_is_label, ri->lineno);
        if (ri->op1) {
            out->destination = dval;
            out->arginfo |= AR_DL;
        }

        /* source operand (op2) */
        sval = parse_operand(ri->op2, &s_is_reg, &s_is_label, ri->lineno);
        if (ri->op2) {
            out->source = sval;
            if (!s_is_reg)    /* immediate or label  literal */
                out->arginfo |= AR_SL;
        }

        pc++;
    }

    /* write binary image */
    outfile = fopen(argv[2], "wb");
    if (!outfile) {
        perror("fopen");
        return 1;
    }
    fwrite(program, sizeof(tick_instr_t), pc, outfile);
    fclose(outfile);

    printf("Assembled %d instructions  %s\n", pc, argv[2]);
    return 0;
}
