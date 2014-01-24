#ifndef RVM_COMMON_INST_H
#define RVM_COMMON_INST_H

#include <stdint.h>

typedef enum rvm_inst_type {
    RVM_INST_HLT,
    RVM_INST_ADD,
    RVM_INST_SUB,
    RVM_INST_MUL,
    RVM_INST_DIV,
    RVM_INST_OR,
    RVM_INST_AND,
    RVM_INST_NOT,
    RVM_INST_XOR,
    RVM_INST_SHL,
    RVM_INST_SHR,
    RVM_INST_CMP,
    RVM_INST_ENTRY,
    RVM_INST_JMP,
    RVM_INST_JE,
    RVM_INST_JL,
    RVM_INST_JLE,
    RVM_INST_JNE,
    RVM_INST_JNL,
    RVM_INST_JNLE,
    RVM_INST_CALL,
    RVM_INST_RET,
    RVM_INST_PUSH,
    RVM_INST_POP,
    RVM_INST_SWAP,
    RVM_INST_ALLOC,
    RVM_INST_FREE,
    RVM_INST_EXP0,
    RVM_INST_EXP1,
    RVM_INST_EXP2,
    RVM_INST_EXP3,
    RVM_INST_EXP4,
    RVM_INST_COUNT
} rvm_inst_type;

extern const char *rvm_inst_type_strings[];

typedef enum rvm_op_type {
    RVM_OP_ABSENT = 0,
    RVM_OP_SCONST,
    RVM_OP_REG,
    RVM_OP_STACK,
    RVM_OP_HEAP,
    RVM_OP_LCONST
} rvm_op_type;

typedef struct rvm_inst {
    rvm_inst_type type;
    rvm_op_type optype[3];
    uint8_t opval[3];
} rvm_inst;

// instruction-specific information for easy validation
extern const int rvm_inst_opcount_min[];
extern const int rvm_inst_opcount_max[];

uint32_t rvm_inst_from_struct(rvm_inst *inst);
void rvm_inst_to_struct(uint32_t encoded, rvm_inst *inst);

int rvm_inst_check_valid(rvm_inst *inst);

#endif
