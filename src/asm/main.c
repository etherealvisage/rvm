#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_SYMBOLS 1024
#define MAX_SYMBOL_REFS 1024

#include "common/inst.h"

static char *symbol_names[MAX_SYMBOLS];
static uint32_t symbol_values[MAX_SYMBOLS];
static uint8_t symbol_has_value[MAX_SYMBOLS];
static uint32_t symbol_count;

static uint32_t symbol_ref_address[MAX_SYMBOL_REFS];
static uint32_t symbol_ref_index[MAX_SYMBOL_REFS];
static uint32_t symbol_ref_adjust[MAX_SYMBOL_REFS];
static uint32_t symbol_ref_count;

static void parse(FILE *in, FILE *out);
static int parse_line(char *line, rvm_inst *result, uint32_t *following,
    int *num_following, uint32_t address);
static void skip_whitespace(char **p);
static char *get_token(char **p);
static uint32_t symbol_index(const char *name);


int main(int argc, char *argv[]) {
    if(argc != 3) {
        printf("usage: %s input-assembly-filename output-object-filename\n", 
            argv[0]);
        return 1;
    }

    FILE *in = fopen(argv[1], "rt");
    FILE *out = fopen(argv[2], "wb");

    if(in == NULL) {
        printf("Couldn't open input file!\n");
        return 1;
    }
    if(out == NULL) {
        printf("Couldn't open output file!\n");
        return 1;
    }

    parse(in, out);

    fclose(in);
    fclose(out);

    return 0;
}

static void parse(FILE *in, FILE *out) {
    rvm_inst inst;
    uint32_t encoded[4];
    int num_following;

    // assuming all lines < 1024 chars long
    char line[1024];
    int lineno = 0;
    uint32_t address = 0;
    while(fgets(line, sizeof(line), in)) {
        line[strlen(line)-1] = 0;
        lineno++;

        if(parse_line(line, &inst, encoded + 1, &num_following, address))
            continue;

        if(rvm_inst_check_valid(&inst)) {
            printf("Invalid instruction on line %i\n", lineno);
            exit(1);
        }

        encoded[0] = rvm_inst_from_struct(&inst);

        fwrite(encoded, sizeof(uint32_t), num_following+1, out);
        address += (num_following+1)*4;
    }

    for(uint32_t i = 0; i < symbol_ref_count; i ++) {
        fseek(out, symbol_ref_address[i], SEEK_SET);
        uint32_t value = symbol_values[symbol_ref_index[i]];
        printf("symbol name: %s\n", symbol_names[symbol_ref_index[i]]);
        printf("symbol value: %x\n", value);
        printf("symbol adjust: %x\n", symbol_ref_adjust[i]);
        printf("symbol has value: %x\n", symbol_has_value[symbol_ref_index[i]]);
        value += symbol_ref_adjust[i];
        fwrite(&value, sizeof(uint32_t), 1, out);
    }
}

static int parse_line(char *line, rvm_inst *result, uint32_t *following,
    int *num_following, uint32_t address) {

    // clear result to keep things clean.
    memset(result, 0, sizeof(*result));
    
    *num_following = 0;

    char *op = get_token(&line);
    // empty line?
    if(*op == 0) return 1;
    // comment?
    if(*op == '#') return 1;
    // label?
    if(*op == ':') {
        symbol_names[symbol_count] = malloc(line-op+1);
        strncpy(symbol_names[symbol_count], op+1, line-op-1);
        symbol_names[symbol_count][line-op-1] = 0;
        uint32_t in = symbol_index(symbol_names[symbol_count]);
        if(in == symbol_count) symbol_count ++;
        else free(symbol_names[symbol_count]);
        symbol_values[in] = address/4;
        symbol_has_value[in] = 1;

        result->type = RVM_INST_ENTRY;

        for(int i = 0; i < 3; i ++) result->optype[i] = RVM_OP_ABSENT;
        return 0;
    }
    // label forward-decl?
    if(*op == ';') {
        symbol_names[symbol_count] = malloc(line-op+1);
        strncpy(symbol_names[symbol_count], op+1, line-op-1);
        symbol_names[symbol_count][line-op-1] = 0;
        uint32_t in = symbol_index(symbol_names[symbol_count]);
        if(in == symbol_count) symbol_count ++, symbol_has_value[in] = 0;
        else free(symbol_names[symbol_count]);

        return 1;
    }

    int type = -1;
    for(int i = 0; i < RVM_INST_COUNT; i ++) {
        if(!strncmp(op, rvm_inst_type_strings[i], line-op)) {
            type = i;
            break;
        }
    }
    if(type == -1) {
        printf("Syntax error: unknown instruction \"%s\"\n", op);
        exit(1);
    }

    result->type = type;

    // absent by default
    result->optype[0] = RVM_OP_ABSENT;
    result->optype[1] = RVM_OP_ABSENT;
    result->optype[2] = RVM_OP_ABSENT;

    char *opstr[3] = {NULL, NULL, NULL};
    int opcount;
    for(opcount = 0; opcount < 3; opcount ++) {
        opstr[opcount] = get_token(&line);
        if(opstr[opcount] == line) break;
        char *t = malloc(line-opstr[opcount] + 1);
        strncpy(t, opstr[opcount], line-opstr[opcount]);
        t[line-opstr[opcount]] = 0;
        opstr[opcount] = t;
    }

    for(int i = 0; i < opcount; i ++) {
        // relative label reference
        if(opstr[i][0] == ':') {
            uint32_t in = symbol_index(opstr[i]+1);
            if(in == symbol_count) {
                printf("Unknown symbol reference '%s'.\n", opstr[i]);
                exit(1);
            }

            printf("Found symbol '%s' at index %i\n", opstr[i]+1, in);
            (*num_following) ++;
            symbol_ref_address[symbol_ref_count] = address + (*num_following * 4);
            printf("\taddress: %i (adjust %x)\n", address, -(address/4));
            symbol_ref_adjust[symbol_ref_count] = -(address/4);
            symbol_ref_index[symbol_ref_count++] = in;
            result->optype[i] = RVM_OP_VALUE_LCONST;
            continue;
        }

        // value/stack/heap.
        uint16_t type = 0;
        const char *ops = opstr[i];
        // stack?
        if(opstr[i][0] == '!') {
            type = RVM_OP_STACK_SCONST;
            ops ++;
        }
        else if(opstr[i][0] == '@') {
            type = RVM_OP_HEAP_SCONST;
            ops ++;
        }

        // register
        if(ops[0] == 'r') {
            if(ops[2] != 0 || !isdigit(ops[1])) {
                printf("Unknown register specification '%s'\n", ops);
                exit(1);
            }
            type += 2; // reg
            result->opval[i] = opstr[i][1] - '0';
        }
        // constant
        else {
            uint32_t parsed = 0;
            if(sscanf(ops, "%u", &parsed) == 0) {
                printf("Unknown constant specification '%s'\n", opstr[i]);
                exit(1);
            }

            if((int32_t)((int8_t)parsed) == (int32_t)parsed) {
                result->opval[i] = (uint8_t)parsed;
            }
            else {
                type += 1; // lconst
                (*num_following) ++;
                *(following++) = parsed;
            }
        }
        result->optype[i] = type;
    }

    while(opcount > 0) free(opstr[--opcount]);

    return 0;
}

static void skip_whitespace(char **p) {
    while(**p == ' ' || **p == '\t') (*p) ++;
}

static char *get_token(char **p) {
    skip_whitespace(p);
    char *word = *p;
    while(**p != ' ' && **p != '\t' && **p != 0) {
        (*p) ++;
    }
    return word;
}

static uint32_t symbol_index(const char *name) {
    uint32_t j;
    for(j = 0; j < symbol_count; j ++) {
        if(!strcmp(name, symbol_names[j])) break;
    }
    return j;
}
