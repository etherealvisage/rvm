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

static int is_const(uint16_t optype);

uint32_t rvm_inst_from_struct(rvm_inst *inst) {
    uint32_t result = 0;
    result |= (uint32_t)(inst->type) << (32-5);

    uint16_t optypes = 0;
    optypes += inst->optype[2];
    optypes *= 10;
    optypes += inst->optype[1];
    optypes *= 10;
    optypes += inst->optype[0];

    result |= (uint32_t)(optypes) << (32-5-10);

    uint8_t reqbits = 0, sconsts = 0;
    for(int i = 0; i < 3; i ++) {
        if(inst->optype[i] % 3 == 0 && inst->optype[i] != RVM_OP_ABSENT) {
            sconsts ++;
        }
        if(inst->optype[i] % 3 == 2) {
            reqbits += 3;
        }
    }
    uint8_t pers = 0;
    if(sconsts > 0) pers = (17 - reqbits) / sconsts;

    uint8_t offset = 0;
    for(int i = 0; i < 3; i ++) {
        if(inst->optype[i] == RVM_OP_ABSENT) continue;
        switch(inst->optype[i] % 3) {
        case 0: // sconst
            result |= ((uint32_t)(inst->opval[i])) << offset;
            offset += pers;
            break;
        case 1: // lconst
            break;
        case 2: // reg
            result |= ((uint32_t)(inst->opval[i]) << offset);
            offset += 3;
            break;
        default:
            break;
        }
    }

    return result;
}

void rvm_inst_to_struct(uint32_t encoded, rvm_inst *inst) {
    uint8_t type = encoded >> (32-5);

    inst->type = (rvm_inst_type)type;
    uint16_t optypes = (encoded >> (32-5-10)) & 0x3ff;
    inst->optype[0] = (rvm_op_type)(optypes % 10);
    inst->optype[1] = (rvm_op_type)((optypes / 10) % 10);
    inst->optype[2] = (rvm_op_type)((optypes / 10 / 10) % 10);

    uint8_t reqbits = 0, sconsts = 0;
    for(int i = 0; i < 3; i ++) {
        if(inst->optype[i] % 3 == 0 && inst->optype[i] != RVM_OP_ABSENT) {
            sconsts ++;
        }
        if(inst->optype[i] % 3 == 2) {
            reqbits += 3;
        }
    }
    uint8_t pers = 0;
    if(sconsts > 0) pers = (17 - reqbits) / sconsts;

    uint8_t offset = 0;
    for(int i = 0; i < 3; i ++) {
        if(inst->optype[i] == RVM_OP_ABSENT) continue;

        switch(inst->optype[i] % 3) {
        case 0: // sconst
            inst->opval[i] = (encoded >> offset) & ((1<<pers)-1);
            offset += pers;
        case 1: // lconst
            break;
        case 2: // reg
            inst->opval[i] = (encoded >> offset) & 0x7;
            offset += 3;
            break;
        default:
            break;
        }
    }
}

static int is_present(uint16_t optype) {
    return optype == RVM_OP_ABSENT?0:1;
}

static int is_const(uint16_t optype) {
    if(optype == RVM_OP_VALUE_SCONST || optype == RVM_OP_VALUE_LCONST)
        return 1;
    else
        return 0;
}

int rvm_inst_check_valid(rvm_inst *inst) {
    const uint16_t op1 = inst->optype[0];
    const uint16_t op2 = inst->optype[1];
    const uint16_t op3 = inst->optype[2];

    switch(inst->type) {
    // no operands for these
    case RVM_INST_HLT:
    case RVM_INST_ENTRY:
    case RVM_INST_RET:
        if(is_present(op1) || is_present(op2) || is_present(op3)) return 1;
        return 0;
    // either two or three operands; when two operands, first is non-constant,
    // and with three, third is non-constant
    case RVM_INST_ADD:
    case RVM_INST_SUB:
    case RVM_INST_MUL:
    case RVM_INST_DIV:
    case RVM_INST_OR:
    case RVM_INST_AND:
    case RVM_INST_XOR:
    case RVM_INST_SHL:
    case RVM_INST_SHR:
        if(!is_present(op1)) return 1;
        if(!is_present(op2)) return 1;
        if(!is_present(op3) && is_const(op1)) return 1;
        if(is_present(op3) && is_const(op3)) return 1;
        return 0;
    // either one or two operands; when two operands, second is non-constant,
    // and with one, first is non-constant
    case RVM_INST_NOT:
        if(!is_present(op1)) return 1;
        if(is_present(op3)) return 1;
        if(is_present(op2) && is_const(op2)) return 1;
        if(!is_present(op2) && is_const(op1)) return 1;
        return 0;
    // exactly one operand; can be any type.
    case RVM_INST_JMP:
    case RVM_INST_JE:
    case RVM_INST_JL:
    case RVM_INST_JLE:
    case RVM_INST_JNE:
    case RVM_INST_JNL:
    case RVM_INST_JNLE:
    case RVM_INST_CALL:
    case RVM_INST_PUSH:
        if(!is_present(op1) || is_present(op2) || is_present(op3)) return 1;
        return 0;
    // two operands; first can be any type, and second is non-constant.
    case RVM_INST_ALLOC:
    case RVM_INST_FREE:
        if(!is_present(op1) || !is_present(op2) || is_present(op3)) return 1;
        if(is_const(op2)) return 1;
        return 0;
    // exactly one non-constant operand.
    case RVM_INST_POP:
        if(!is_present(op1) || is_present(op2) || is_present(op3)) return 1;
        if(is_const(op1)) return 1;
        return 0;
    // exactly two non-constant operands.
    case RVM_INST_SWAP:
        if(!is_present(op1) || !is_present(op2) || is_present(op3)) return 1;
        if(is_const(op1) || is_const(op2)) return 1;
        return 0;
    default: // expansions are all rendered invalid by this
        return 0;
    }
}
