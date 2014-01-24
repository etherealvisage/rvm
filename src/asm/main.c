#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "common/inst.h"

static void parse(FILE *in, FILE *out);
static void parse_line(char *line, rvm_inst *result, uint32_t *following,
    int *num_following);
static void skip_whitespace(char **p);
static char *get_token(char **p);

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
    while(fgets(line, sizeof(line), in)) {
        line[strlen(line)-1] = 0;
        lineno++;

        parse_line(line, &inst, encoded + 1, &num_following);

        if(rvm_inst_check_valid(&inst)) {
            printf("Invalid instruction on line %i\n", lineno);
            exit(1);
        }

        encoded[0] = rvm_inst_from_struct(&inst);

        fwrite(encoded, sizeof(uint32_t), num_following+1, out);
    }
}

static void parse_line(char *line, rvm_inst *result, uint32_t *following,
    int *num_following) {

    // clear result to keep things clean.
    memset(result, 0, sizeof(*result));
    
    *num_following = 0;

    char *op = get_token(&line);
    // empty line?
    if(*op == 0) return;
    // comment?
    if(*op == '#') return;
    // label?
    if(*op == ':') {
        // TODO: add label to global symbol table
        result->type = RVM_INST_ENTRY;
        *num_following = 0;
        return;
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
        // label reference
        if(opstr[i][0] == ':') {
            // TODO: look up stuff...
        }
        // register
        else if(opstr[i][0] == 'r') {
            if(opstr[i][2] != 0 || !isdigit(opstr[i][1])) {
                printf("Unknown register specification '%s'\n", opstr[i]);
                exit(1);
            }
            result->optype[i] = RVM_OP_REG;
            result->opval[i] = opstr[i][1] - '0';
        }
        // stack memory
        else if(opstr[i][0] == '!') {
            uint32_t parsed;
            if(sscanf(opstr[i] + 1, "%u", &parsed) == 0) {
                printf("Unknown stack offset specification '%s'\n", opstr[i]);
                exit(1);
            }
            result->optype[i] = RVM_OP_STACK;
            if(parsed > 0xff) {
                printf("Stack offset too large: %u; can be at most 255\n",
                    parsed);
                exit(1);
            }
            result->opval[i] = parsed;
        }
        // heap memory
        else if(opstr[i][0] == '@') {
            uint32_t parsed;
            if(sscanf(opstr[i] + 1, "%u", &parsed) == 0) {
                printf("Unknown heap offset specification '%s'\n", opstr[i]);
                exit(1);
            }
            result->optype[i] = RVM_OP_HEAP;
            (*num_following) ++;
            *(following++) = parsed;
        }
        // constant
        else {
            uint32_t parsed;
            if(sscanf(opstr[i] + 1, "%u", &parsed) == 0) {
                printf("Unknown constant specification '%s'\n", opstr[i]);
                exit(1);
            }
            if((int32_t)((int8_t)parsed) == (int32_t)parsed) {
                result->optype[i] = RVM_OP_SCONST;
                result->opval[i] = (uint8_t)parsed;
            }
            else {
                result->optype[i] = RVM_OP_LCONST;
                (*num_following) ++;
                *(following++) = parsed;
            }
        }
    }

    while(opcount > 0) free(opstr[--opcount]);
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
