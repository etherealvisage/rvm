#include "inst.h"

const char *rvm_inst_type_strings[] = {
    "hlt",
    "add",
    "sub",
    "mul",
    "div",
    "or",
    "and",
    "not",
    "xor",
    "shl",
    "shr",
    "cmp",
    "entry",
    "jmp",
    "je",
    "jl",
    "jle",
    "jne",
    "jnl",
    "jnle",
    "call",
    "ret",
    "push",
    "pop",
    "swap",
    "alloc",
    "free",
    "exp0",
    "exp1",
    "exp2",
    "exp3",
    "exp4",
};

const int rvm_inst_opcount_min[] = {
    0, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 2,
    1, 2, 0, 0, 0, 0, 0
};

const int rvm_inst_opcount_max[] = {
    0, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 2, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 2,
    1, 2, 0, 0, 0, 0, 0
};

uint32_t rvm_inst_from_struct(rvm_inst *inst) {
    uint32_t result = 0;
    result |= (uint32_t)(inst->type) << (32-5);

    uint8_t optypes = 0;
    optypes += inst->optype[2];
    optypes *= 5;
    optypes += inst->optype[1];
    optypes *= 5;
    optypes += inst->optype[0];

    result |= (uint32_t)(optypes) << (32-5-8);

    uint8_t offset = 0;
    for(int i = 0; i < 3; i ++) {
        switch(inst->optype[i]) {
        case RVM_OP_REG:
            result |= ((uint32_t)(inst->opval[i]) << offset);
            offset += 3;
            break;
        case RVM_OP_ABSENT:
        case RVM_OP_LCONST:
            break;
        default: // SCONST/STACK
            result |= ((uint32_t)(inst->opval[i])) << offset;
            offset += 8;
            break;
        }
    }

    return result;
}

void rvm_inst_to_struct(uint32_t encoded, rvm_inst *inst) {
    uint8_t type = encoded >> (32-5);

    inst->type = (rvm_inst_type)type;
    uint8_t optypes = (encoded >> (32-5-8)) & 0xff;
    inst->optype[0] = (rvm_op_type)(optypes % 5);
    inst->optype[1] = (rvm_op_type)((optypes / 5) % 5);
    inst->optype[2] = (rvm_op_type)((optypes / 5 / 5) % 5);

    uint8_t offset = 0;
    for(int i = 0; i < 3; i ++) {
        switch(inst->optype[i]) {
        case RVM_OP_REG:
            inst->opval[i] = (encoded >> offset) & 0x7;
            offset += 3;
            break;
        case RVM_OP_ABSENT:
        case RVM_OP_LCONST:
            break;
        default:
            inst->opval[i] = (encoded >> offset) & 0xff;
            offset += 8;
            break;
        }
    }
}

int rvm_inst_check_valid(rvm_inst *inst) {
    switch(inst->type) {
    case RVM_INST_HLT:
    case RVM_INST_ENTRY:
    case RVM_INST_RET:
        if(inst->optype[0] != RVM_OP_ABSENT) return 1;
        if(inst->optype[1] != RVM_OP_ABSENT) return 1;
        if(inst->optype[2] != RVM_OP_ABSENT) return 1;
        return 0;
    case RVM_INST_ADD:
    case RVM_INST_SUB:
    case RVM_INST_MUL:
    case RVM_INST_DIV:
    case RVM_INST_OR:
    case RVM_INST_AND:
    case RVM_INST_XOR:
    case RVM_INST_SHL:
    case RVM_INST_SHR:
        if(inst->optype[0] == RVM_OP_ABSENT) return 1;
        if(inst->optype[1] == RVM_OP_ABSENT) return 1;
        if(inst->optype[2] == RVM_OP_ABSENT
            && (inst->optype[0] == RVM_OP_LCONST
                || inst->optype[0] == RVM_OP_SCONST)) return 1;
        return 0;
    case RVM_INST_NOT:
        if(inst->optype[0] == RVM_OP_ABSENT) return 1;
        if(inst->optype[1] == RVM_OP_ABSENT
            && (inst->optype[0] == RVM_OP_LCONST
                || inst->optype[0] == RVM_OP_SCONST)) return 1;
        if(inst->optype[2] != RVM_OP_ABSENT) return 1;
        return 0;
    case RVM_INST_JMP:
    case RVM_INST_JE:
    case RVM_INST_JL:
    case RVM_INST_JLE:
    case RVM_INST_JNE:
    case RVM_INST_JNL:
    case RVM_INST_JNLE:
    case RVM_INST_CALL:
        if(inst->optype[0] == RVM_OP_ABSENT) return 1;
        if(inst->optype[1] != RVM_OP_ABSENT) return 1;
        if(inst->optype[2] != RVM_OP_ABSENT) return 1;
        return 0;
    case RVM_INST_PUSH:
    case RVM_INST_ALLOC:
        if(inst->optype[0] == RVM_OP_ABSENT) return 1;
        if(inst->optype[1] != RVM_OP_ABSENT) return 1;
        if(inst->optype[2] != RVM_OP_ABSENT) return 1;
        return 0;
    case RVM_INST_POP:
        // can't have constant target for pop
        if(inst->optype[0] == RVM_OP_ABSENT || inst->optype[0] == RVM_OP_SCONST
            || inst->optype[0] == RVM_OP_LCONST) return 1;
        if(inst->optype[1] != RVM_OP_ABSENT) return 1;
        if(inst->optype[2] != RVM_OP_ABSENT) return 1;
        return 0;
    case RVM_INST_SWAP:
        // can't have constant target
        if(inst->optype[0] == RVM_OP_ABSENT || inst->optype[0] == RVM_OP_SCONST
            || inst->optype[0] == RVM_OP_LCONST) return 1;
        // can't have constant target
        if(inst->optype[1] == RVM_OP_ABSENT || inst->optype[1] == RVM_OP_SCONST
            || inst->optype[1] == RVM_OP_LCONST) return 1;
        if(inst->optype[2] != RVM_OP_ABSENT) return 1;
        return 0;
    case RVM_INST_FREE:
        if(inst->optype[0] == RVM_OP_ABSENT) return 1;
        if(inst->optype[1] == RVM_OP_ABSENT) return 1;
        if(inst->optype[2] != RVM_OP_ABSENT) return 1;
        return 0;
    default: // expansions are all rendered invalid by this
        return 0;
    }
}
