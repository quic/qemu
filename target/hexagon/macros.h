/*
 *  Copyright(c) 2019-2020 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HEXAGON_MACROS_H
#define HEXAGON_MACROS_H

#include "cpu.h"
#include "hex_regs.h"
#include "reg_fields.h"
#include "op_helper.h"

#define MARK_LATE_PRED_WRITE(RNUM) /* Not modelled in qemu */

#ifdef QEMU_GENERATE
#define READ_REG(dest, NUM)              gen_read_reg(dest, NUM)
#define READ_PREG(dest, NUM)             gen_read_preg(dest, (NUM))
#else /* !QEMU_GENERATE */
#define READ_REG(NUM)                    (env->gpr[(NUM)])
#define READ_PREG(NUM)                   (env->pred[NUM])

#define READ_SREG(NUM) ARCH_GET_SYSTEM_REG(env, NUM)
#define READ_SGP0()    ARCH_GET_SYSTEM_REG(env, HEX_SREG_SGP0)
#define READ_SGP1()    ARCH_GET_SYSTEM_REG(env, HEX_SREG_SGP1)
#define READ_SGP10()   ((uint64_t)ARCH_GET_SYSTEM_REG(env, HEX_SREG_SGP0) | \
    ((uint64_t)ARCH_GET_SYSTEM_REG(env, HEX_SREG_SGP1) << 32))

#define WRITE_SREG(NUM, VAL)      log_sreg_write(env, NUM, VAL, slot)
#define WRITE_SGP0(VAL)           log_sreg_write(env, HEX_SREG_SGP0, VAL, slot)
#define WRITE_SGP1(VAL)           log_sreg_write(env, HEX_SREG_SGP1, VAL, slot)
#define WRITE_SGP10(VAL) \
    do { \
        log_sreg_write(env, HEX_SREG_SGP0, (VAL) & 0xFFFFFFFF, slot); \
        log_sreg_write(env, HEX_SREG_SGP1, (VAL) >> 32, slot); \
    } while (0)

#define WRITE_PREG(NUM, VAL)             log_pred_write(env, NUM, VAL)
#endif /* QEMU_GENERATE */

#define PCALIGN 4
#define PCALIGN_MASK (PCALIGN - 1)

#ifdef QEMU_GENERATE
#define GET_FIELD(RES, FIELD, REGIN) \
    tcg_gen_extract_tl(RES, REGIN, reg_field_info[FIELD].offset, \
                                   reg_field_info[FIELD].width)
#define GET_SSR_FIELD(RES, FIELD) \
    GET_FIELD(RES, FIELD, hex_t_sreg[HEX_SREG_SSR])
#else
#define GET_FIELD(FIELD,REGIN) \
    fEXTRACTU_BITS(REGIN, reg_field_info[FIELD].width, \
                   reg_field_info[FIELD].offset)
#define GET_SSR_FIELD(FIELD,REGIN) \
    (uint32_t)GET_FIELD(FIELD, REGIN)
#define GET_SYSCFG_FIELD(FIELD,REGIN) \
    (uint32_t)GET_FIELD(FIELD, REGIN)
#endif
#define SET_SYSTEM_FIELD(ENV,REG,FIELD,VAL) do {\
        uint32_t regval = ARCH_GET_SYSTEM_REG(ENV, REG); \
        fINSERT_BITS(regval,reg_field_info[FIELD].width, \
                     reg_field_info[FIELD].offset,(VAL)); \
        ARCH_SET_SYSTEM_REG(ENV, REG, regval); \
    } while (0)
#define SET_SSR_FIELD(ENV,FIELD,VAL) \
    SET_SYSTEM_FIELD(ENV,HEX_SREG_SSR,FIELD,VAL)
#define SET_SYSCFG_FIELD(ENV,FIELD,VAL) \
    SET_SYSTEM_FIELD(ENV,HEX_SREG_SYSCFG,FIELD,VAL)

#ifdef QEMU_GENERATE
#define GET_USR_FIELD(FIELD, DST) \
    tcg_gen_extract_tl(DST, hex_gpr[HEX_REG_USR], \
                       reg_field_info[FIELD].offset, \
                       reg_field_info[FIELD].width)

#define TYPE_INT(X)          __builtin_types_compatible_p(typeof(X), int)
#define TYPE_TCGV(X)         __builtin_types_compatible_p(typeof(X), TCGv)
#define TYPE_TCGV_I64(X)     __builtin_types_compatible_p(typeof(X), TCGv_i64)

#define SET_USR_FIELD_FUNC(X) \
    __builtin_choose_expr(TYPE_INT(X), \
        gen_set_usr_fieldi, \
        __builtin_choose_expr(TYPE_TCGV(X), \
            gen_set_usr_field, (void)0))
#define SET_USR_FIELD(FIELD, VAL) \
    SET_USR_FIELD_FUNC(VAL)(FIELD, VAL)
#else
#define GET_USR_FIELD(FIELD) \
    fEXTRACTU_BITS(env->gpr[HEX_REG_USR], reg_field_info[FIELD].width, \
                   reg_field_info[FIELD].offset)

#define SET_USR_FIELD(FIELD, VAL) \
    fINSERT_BITS(env->new_value[HEX_REG_USR], reg_field_info[FIELD].width, \
                 reg_field_info[FIELD].offset, (VAL))
#endif

#ifdef QEMU_GENERATE
/*
 * Section 5.5 of the Hexagon V67 Programmer's Reference Manual
 *
 * Slot 1 store with slot 0 load
 * A slot 1 store operation with a slot 0 load operation can appear in a packet.
 * The packet attribute :mem_noshuf inhibits the instruction reordering that
 * would otherwise be done by the assembler. For example:
 *     {
 *         memw(R5) = R2 // slot 1 store
 *         R3 = memh(R6) // slot 0 load
 *     }:mem_noshuf
 * Unlike most packetized operations, these memory operations are not executed
 * in parallel (Section 3.3.1). Instead, the store instruction in Slot 1
 * effectively executes first, followed by the load instruction in Slot 0. If
 * the addresses of the two operations are overlapping, the load will receive
 * the newly stored data. This feature is supported in processor versions
 * V65 or greater.
 *
 *
 * For qemu, we look for a load in slot 0 when there is  a store in slot 1
 * in the same packet.  When we see this, we call a helper that merges the
 * bytes from the store buffer with the value loaded from memory.
 */
#define CHECK_NOSHUF(DST, VA, SZ, SIGN) \
    do { \
        if (insn->slot == 0 && pkt->pkt_has_scalar_store_s1) { \
            process_store(ctx, pkt, 1); \
        } \
    } while (0)

#define MEM_LOAD1s(DST, VA) \
    do { \
        CHECK_NOSHUF(DST, VA, 1, s); \
        tcg_gen_qemu_ld8s(DST, VA, ctx->mem_idx); \
    } while (0)
#define MEM_LOAD1u(DST, VA) \
    do { \
        CHECK_NOSHUF(DST, VA, 1, u); \
        tcg_gen_qemu_ld8u(DST, VA, ctx->mem_idx); \
    } while (0)
#define MEM_LOAD2s(DST, VA) \
    do { \
        CHECK_NOSHUF(DST, VA, 2, s); \
        tcg_gen_qemu_ld16s(DST, VA, ctx->mem_idx); \
    } while (0)
#define MEM_LOAD2u(DST, VA) \
    do { \
        CHECK_NOSHUF(DST, VA, 2, u); \
        tcg_gen_qemu_ld16u(DST, VA, ctx->mem_idx); \
    } while (0)
#define MEM_LOAD4s(DST, VA) \
    do { \
        CHECK_NOSHUF(DST, VA, 4, s); \
        tcg_gen_qemu_ld32s(DST, VA, ctx->mem_idx); \
    } while (0)
#define MEM_LOAD4u(DST, VA) \
    do { \
        CHECK_NOSHUF(DST, VA, 4, u); \
        tcg_gen_qemu_ld32s(DST, VA, ctx->mem_idx); \
    } while (0)
#define MEM_LOAD8u(DST, VA) \
    do { \
        CHECK_NOSHUF(DST, VA, 8, u); \
        tcg_gen_qemu_ld64(DST, VA, ctx->mem_idx); \
    } while (0)

#define MEM_STORE1_FUNC(X) \
    _Generic((X), int : gen_store1i, TCGv_i32 : gen_store1)
#define MEM_STORE1(VA, DATA, SLOT) \
    MEM_STORE1_FUNC(DATA)(cpu_env, VA, DATA, ctx, SLOT)

#define MEM_STORE2_FUNC(X) \
    _Generic((X), int : gen_store2i, TCGv_i32 : gen_store2)
#define MEM_STORE2(VA, DATA, SLOT) \
    MEM_STORE2_FUNC(DATA)(cpu_env, VA, DATA, ctx, SLOT)

#define MEM_STORE4_FUNC(X) \
    _Generic((X), int : gen_store4i, TCGv_i32 : gen_store4)
#define MEM_STORE4(VA, DATA, SLOT) \
    MEM_STORE4_FUNC(DATA)(cpu_env, VA, DATA, ctx, SLOT)

#define MEM_STORE8_FUNC(X) \
    _Generic((X), int64_t : gen_store8i, TCGv_i64 : gen_store8)
#define MEM_STORE8(VA, DATA, SLOT) \
    MEM_STORE8_FUNC(DATA)(cpu_env, VA, DATA, ctx, SLOT)
#endif

#ifdef QEMU_GENERATE
static inline void gen_cancel(TCGv slot)
{
    TCGv one = tcg_const_tl(1);
    TCGv mask = tcg_temp_new();
    tcg_gen_shl_tl(mask, one, slot);
    tcg_gen_or_tl(hex_slot_cancelled, hex_slot_cancelled, mask);
    tcg_temp_free(one);
    tcg_temp_free(mask);
}

#define CANCEL gen_cancel(slot);
#else
#define CANCEL cancel_slot(env, slot)
#endif

#define STORE_ZERO
#define LOAD_CANCEL(EA) do { CANCEL; } while (0)

#ifdef QEMU_GENERATE
static inline void gen_pred_cancel(TCGv pred, int slot_num)
 {
    TCGv slot_mask = tcg_const_tl(1 << slot_num);
    TCGv tmp = tcg_temp_new();
    TCGv zero = tcg_const_tl(0);
    TCGv one = tcg_const_tl(1);
    tcg_gen_or_tl(slot_mask, hex_slot_cancelled, slot_mask);
    tcg_gen_andi_tl(tmp, pred, 1);
    tcg_gen_movcond_tl(TCG_COND_EQ, hex_slot_cancelled, tmp, zero,
                       slot_mask, hex_slot_cancelled);
    tcg_temp_free(slot_mask);
    tcg_temp_free(tmp);
    tcg_temp_free(zero);
    tcg_temp_free(one);
}
#define PRED_LOAD_CANCEL(PRED, EA) \
    gen_pred_cancel(PRED, insn->is_endloop ? 4 : insn->slot)

#define PRED_STORE_CANCEL(PRED, EA) \
    gen_pred_cancel(PRED, insn->is_endloop ? 4 : insn->slot)
#else
#define STORE_CANCEL(EA) env->slot_cancelled |= (1 << slot)
#endif

#define IS_CANCELLED(SLOT)
#define fMAX(A, B) (((A) > (B)) ? (A) : (B))

#ifdef QEMU_GENERATE
#define fMIN(DST, A, B) tcg_gen_movcond_i32(TCG_COND_LT, DST, A, B, A, B)
#else
#define fMIN(A, B) (((A) < (B)) ? (A) : (B))
#endif

#define fABS(A) (((A) < 0) ? (-(A)) : (A))
#define fINSERT_BITS(REG, WIDTH, OFFSET, INVAL) \
    do { \
        REG = ((REG) & ~(((fCONSTLL(1) << (WIDTH)) - 1) << (OFFSET))) | \
           (((INVAL) & ((fCONSTLL(1) << (WIDTH)) - 1)) << (OFFSET)); \
    } while (0)
#define fEXTRACTU_BITS(INREG, WIDTH, OFFSET) \
    (fZXTN(WIDTH, 32, (INREG >> OFFSET)))
#define fEXTRACTU_BIDIR(INREG, WIDTH, OFFSET) \
    (fZXTN(WIDTH, 32, fBIDIR_LSHIFTR((INREG), (OFFSET), 4_8)))
#define fEXTRACTU_RANGE(INREG, HIBIT, LOWBIT) \
    (fZXTN((HIBIT - LOWBIT + 1), 32, (INREG >> LOWBIT)))
#define fINSERT_RANGE(INREG, HIBIT, LOWBIT, INVAL) \
    do { \
        int offset = LOWBIT; \
        int width = HIBIT - LOWBIT + 1; \
        INREG &= ~(((fCONSTLL(1) << width) - 1) << offset); \
        INREG |= ((INVAL & ((fCONSTLL(1) << width) - 1)) << offset); \
    } while (0)

#ifdef QEMU_GENERATE
#define f8BITSOF(RES, VAL) gen_8bitsof(RES, VAL)
#define fLSBOLD(VAL) tcg_gen_andi_tl(LSB, (VAL), 1)
#else
#define f8BITSOF(VAL) ((VAL) ? 0xff : 0x00)
#define fLSBOLD(VAL)  ((VAL) & 1)
#endif

#ifdef QEMU_GENERATE
#define fLSBNEW(PVAL)   tcg_gen_andi_tl(LSB, (PVAL), 1)
#define fLSBNEW0        tcg_gen_andi_tl(LSB, hex_new_pred_value[0], 1)
#define fLSBNEW1        tcg_gen_andi_tl(LSB, hex_new_pred_value[1], 1)
#else
#define fLSBNEW(PVAL)   ((PVAL) & 1)
#define fLSBNEW0        (env->new_pred_value[0] & 1)
#define fLSBNEW1        (env->new_pred_value[1] & 1)
#endif

#ifdef QEMU_GENERATE
static inline void gen_logical_not(TCGv dest, TCGv src)
{
    TCGv one = tcg_const_tl(1);
    TCGv zero = tcg_const_tl(0);

    tcg_gen_movcond_tl(TCG_COND_NE, dest, src, zero, zero, one);

    tcg_temp_free(one);
    tcg_temp_free(zero);
}
#define fLSBOLDNOT(VAL) \
    do { \
        tcg_gen_andi_tl(LSB, (VAL), 1); \
        tcg_gen_xori_tl(LSB, LSB, 1); \
    } while (0)
#define fLSBNEWNOT(PNUM) \
    gen_logical_not(LSB, (PNUM))
#define fLSBNEW0NOT \
    do { \
        tcg_gen_mov_tl(LSB, hex_new_pred_value[0]); \
        gen_logical_not(LSB, LSB); \
    } while (0)
#define fLSBNEW1NOT \
    do { \
        tcg_gen_mov_tl(LSB, hex_new_pred_value[1]); \
        gen_logical_not(LSB, LSB); \
    } while (0)
#else
#define fLSBNEWNOT(PNUM) (!fLSBNEW(PNUM))
#define fLSBOLDNOT(VAL) (!fLSBOLD(VAL))
#define fLSBNEW0NOT (!fLSBNEW0)
#define fLSBNEW1NOT (!fLSBNEW1)
#endif

#define fNEWREG(VAL) ((int32_t)(VAL))

#define fNEWREG_ST(VAL) (VAL)

#define fMEMZNEW(RNUM) (RNUM == 0)
#define fMEMNZNEW(RNUM) (RNUM != 0)
#define fVSATUVALN(N, VAL) \
    ({ \
        (((int)(VAL)) < 0) ? 0 : ((1LL << (N)) - 1); \
    })
#define fSATUVALN(N, VAL) \
    ({ \
        fSET_OVERFLOW(); \
        ((VAL) < 0) ? 0 : ((1LL << (N)) - 1); \
    })
#define fSATVALN(N, VAL) \
    ({ \
        fSET_OVERFLOW(); \
        ((VAL) < 0) ? (-(1LL << ((N) - 1))) : ((1LL << ((N) - 1)) - 1); \
    })
#define fVSATVALN(N, VAL) \
    ({ \
        ((VAL) < 0) ? (-(1LL << ((N) - 1))) : ((1LL << ((N) - 1)) - 1); \
    })
#define fZXTN(N, M, VAL) ((VAL) & ((1LL << (N)) - 1))
#define fSXTN(N, M, VAL) \
    ((fZXTN(N, M, VAL) ^ (1LL << ((N) - 1))) - (1LL << ((N) - 1)))
#define fSATN(N, VAL) \
    ((fSXTN(N, 64, VAL) == (VAL)) ? (VAL) : fSATVALN(N, VAL))
#define fVSATN(N, VAL) \
    ((fSXTN(N, 64, VAL) == (VAL)) ? (VAL) : fVSATVALN(N, VAL))
#define fADDSAT64(DST, A, B) \
    do { \
        size8u_t __a = fCAST8u(A); \
        size8u_t __b = fCAST8u(B); \
        size8u_t __sum = __a + __b; \
        size8u_t __xor = __a ^ __b; \
        const size8u_t __mask = 0x8000000000000000ULL; \
        if (__xor & __mask) { \
            DST = __sum; \
        } \
        else if ((__a ^ __sum) & __mask) { \
            if (__sum & __mask) { \
                DST = 0x7FFFFFFFFFFFFFFFLL; \
                fSET_OVERFLOW(); \
            } else { \
                DST = 0x8000000000000000LL; \
                fSET_OVERFLOW(); \
            } \
        } else { \
            DST = __sum; \
        } \
    } while (0)
#define fVSATUN(N, VAL) \
    ((fZXTN(N, 64, VAL) == (VAL)) ? (VAL) : fVSATUVALN(N, VAL))
#define fSATUN(N, VAL) \
    ((fZXTN(N, 64, VAL) == (VAL)) ? (VAL) : fSATUVALN(N, VAL))
#define fSATH(VAL) (fSATN(16, VAL))
#define fSATUH(VAL) (fSATUN(16, VAL))
#define fVSATH(VAL) (fVSATN(16, VAL))
#define fVSATUH(VAL) (fVSATUN(16, VAL))
#define fSATUB(VAL) (fSATUN(8, VAL))
#define fSATB(VAL) (fSATN(8, VAL))
#define fVSATUB(VAL) (fVSATUN(8, VAL))
#define fVSATB(VAL) (fVSATN(8, VAL))
#define fIMMEXT(IMM) (IMM = IMM)
#define fMUST_IMMEXT(IMM) fIMMEXT(IMM)

#define fPCALIGN(IMM) IMM = (IMM & ~PCALIGN_MASK)

#define fGET_EXTENSION (insn->extension)

#ifdef QEMU_GENERATE
static inline TCGv gen_read_ireg(TCGv result, TCGv val, int shift)
{
    /*
     * Section 2.2.4 of the Hexagon V67 Programmer's Reference Manual
     *
     *  The "I" value from a modifier register is divided into two pieces
     *      LSB         bits 23:17
     *      MSB         bits 31:28
     * The value is signed
     *
     * At the end we shift the result according to the shift argument
     */
    TCGv msb = tcg_temp_new();
    TCGv lsb = tcg_temp_new();

    tcg_gen_extract_tl(lsb, val, 17, 7);
    tcg_gen_sari_tl(msb, val, 21);
    tcg_gen_deposit_tl(result, msb, lsb, 0, 7);

    tcg_gen_shli_tl(result, result, shift);

    tcg_temp_free(msb);
    tcg_temp_free(lsb);

    return result;
}
#define fREAD_IREG(VAL, SHIFT) gen_read_ireg(ireg, (VAL), (SHIFT))
#define fREAD_LR() (READ_REG(tmp, HEX_REG_LR))
#else
#define fREAD_LR() (READ_REG(HEX_REG_LR))
#define fREAD_ELR() (READ_SREG(HEX_SREG_ELR))
#endif

#define fWRITE_LR(A)       log_reg_write(env, HEX_REG_LR, A, slot)

#ifdef QEMU_GENERATE
#define fREAD_SP() (READ_REG(tmp, HEX_REG_SP))
#define fREAD_GOSP() (READ_REG(tmp, HEX_REG_GOSP))
#define fREAD_GELR() (READ_REG(tmp, HEX_REG_GELR))
#define fREAD_FP() (READ_REG(tmp, HEX_REG_FP))
#define fREAD_PC() (READ_REG(tmp, HEX_REG_PC))
#else
#define fREAD_SP() (READ_REG(HEX_REG_SP))
#define fREAD_GOSP() (READ_REG(HEX_REG_GOSP))
#define fREAD_GELR() (READ_REG(HEX_REG_GELR))
#define fREAD_LC0 (READ_REG(HEX_REG_LC0))
#define fREAD_LC1 (READ_REG(HEX_REG_LC1))
#define fREAD_SA0 (READ_REG(HEX_REG_SA0))
#define fREAD_SA1 (READ_REG(HEX_REG_SA1))
#define fREAD_FP() (READ_REG(HEX_REG_FP))
#define fREAD_PC() (READ_REG(HEX_REG_PC))
#endif

#define fREAD_NPC() (env->next_PC & (0xfffffffe))

#ifdef QEMU_GENERATE
#define fREAD_P0() (READ_PREG(tmp, 0))
#define fREAD_P3() (READ_PREG(tmp, 3))
#define fNOATTRIB_READ_P3() (READ_PREG(tmp, 3))
#else
#define fREAD_P0() (READ_PREG(0))
#define fREAD_P3() (READ_PREG(3))
#define fNOATTRIB_READ_P3() (READ_PREG(3))
#endif

#define fCHECK_PCALIGN(A)                                   \
    do {                                                    \
        if (((A)&PCALIGN_MASK) != 0) {                      \
            env->cause_code = HEX_CAUSE_PC_NOT_ALIGNED;     \
            helper_raise_exception(env, HEX_EVENT_PRECISE); \
        }                                                   \
    } while (0);

#define fCUREXT() GET_SSR_FIELD(SSR_XA)
#define fCUREXT_WRAP(EXT_NO)

#ifdef QEMU_GENERATE
#define fWRITE_NPC(A) gen_write_new_pc(A)
#else
#define fWRITE_NPC(A) write_new_pc(env, A)
#endif

#define fLOOPSTATS(A)
#define CALLBACK(...)
#define fCOF_CALLBACK(LOC, TYPE)
#define fBRANCH(LOC, TYPE) \
    do { \
        fWRITE_NPC(LOC); \
        fCOF_CALLBACK(LOC, TYPE); \
    } while (0)
#define fJUMPR(REGNO, TARGET, TYPE) fBRANCH(TARGET, COF_TYPE_JUMPR)
#define fHINTJR(TARGET) { }
#define fBP_RAS_CALL(A)
#define fCALL(A) \
    do { \
        fWRITE_LR(fREAD_NPC()); \
        fBRANCH(A, COF_TYPE_CALL); \
    } while (0)
#define fCALLR(A) \
    do { \
        fWRITE_LR(fREAD_NPC()); \
        fBRANCH(A, COF_TYPE_CALLR); \
    } while (0)
#define fWRITE_LOOP_REGS0(START, COUNT) \
    do { \
        log_reg_write(env, HEX_REG_LC0, COUNT, slot);  \
        log_reg_write(env, HEX_REG_SA0, START, slot); \
    } while (0)
#define fWRITE_LOOP_REGS1(START, COUNT) \
    do { \
        log_reg_write(env, HEX_REG_LC1, COUNT, slot);  \
        log_reg_write(env, HEX_REG_SA1, START, slot);\
    } while (0)
#define fWRITE_LC0(VAL) log_reg_write(env, HEX_REG_LC0, VAL, slot)
#define fWRITE_LC1(VAL) log_reg_write(env, HEX_REG_LC1, VAL, slot)

#ifdef QEMU_GENERATE
#define fCARRY_FROM_ADD(RES, A, B, C) gen_carry_from_add64(RES, A, B, C)
#else
#define fCARRY_FROM_ADD(A, B, C) carry_from_add64(A, B, C)
#endif

#define fSET_OVERFLOW() SET_USR_FIELD(USR_OVF, 1)
#define fSET_LPCFG(VAL) SET_USR_FIELD(USR_LPCFG, (VAL))
#define fGET_LPCFG (GET_USR_FIELD(USR_LPCFG))
#define fWRITE_P0(VAL) WRITE_PREG(0, VAL)
#define fWRITE_P1(VAL) WRITE_PREG(1, VAL)
#define fWRITE_P2(VAL) WRITE_PREG(2, VAL)
#define fWRITE_P3(VAL) WRITE_PREG(3, VAL)
#define fWRITE_P3_LATE(VAL) WRITE_PREG(3, VAL)
#define fPART1(WORK) if (part1) { WORK; return; }
#define fCAST4u(A) ((size4u_t)(A))
#define fCAST4s(A) ((size4s_t)(A))
#define fCAST8u(A) ((size8u_t)(A))
#define fCAST8s(A) ((size8s_t)(A))
#define fCAST2_2s(A) ((size2s_t)(A))
#define fCAST2_2u(A) ((size2u_t)(A))
#define fCAST4_4s(A) ((size4s_t)(A))
#define fCAST4_4u(A) ((size4u_t)(A))
#define fCAST4_8s(A) ((size8s_t)((size4s_t)(A)))
#define fCAST4_8u(A) ((size8u_t)((size4u_t)(A)))
#define fCAST8_8s(A) ((size8s_t)(A))
#define fCAST8_8u(A) ((size8u_t)(A))
#define fCAST2_8s(A) ((size8s_t)((size2s_t)(A)))
#define fCAST2_8u(A) ((size8u_t)((size2u_t)(A)))
#define fZE8_16(A) ((size2s_t)((size1u_t)(A)))
#define fSE8_16(A) ((size2s_t)((size1s_t)(A)))
#define fSE16_32(A) ((size4s_t)((size2s_t)(A)))
#define fZE16_32(A) ((size4u_t)((size2u_t)(A)))
#define fSE32_64(A) ((size8s_t)((size4s_t)(A)))
#define fZE32_64(A) ((size8u_t)((size4u_t)(A)))
#define fSE8_32(A) ((size4s_t)((size1s_t)(A)))
#define fZE8_32(A) ((size4s_t)((size1u_t)(A)))
#define fMPY8UU(A, B) (int)(fZE8_16(A) * fZE8_16(B))
#define fMPY8US(A, B) (int)(fZE8_16(A) * fSE8_16(B))
#define fMPY8SU(A, B) (int)(fSE8_16(A) * fZE8_16(B))
#define fMPY8SS(A, B) (int)((short)(A) * (short)(B))
#define fMPY16SS(A, B) fSE32_64(fSE16_32(A) * fSE16_32(B))
#define fMPY16UU(A, B) fZE32_64(fZE16_32(A) * fZE16_32(B))
#define fMPY16SU(A, B) fSE32_64(fSE16_32(A) * fZE16_32(B))
#define fMPY16US(A, B) fMPY16SU(B, A)
#define fMPY32SS(A, B) (fSE32_64(A) * fSE32_64(B))
#define fMPY32UU(A, B) (fZE32_64(A) * fZE32_64(B))
#define fMPY32SU(A, B) (fSE32_64(A) * fZE32_64(B))
#define fMPY3216SS(A, B) (fSE32_64(A) * fSXTN(16, 64, B))
#define fMPY3216SU(A, B) (fSE32_64(A) * fZXTN(16, 64, B))
#define fROUND(A) (A + 0x8000)
#define fCLIP(DST, SRC, U) \
    do { \
        size4s_t maxv = (1 << U) - 1; \
        size4s_t minv = -(1 << U); \
        DST = fMIN(maxv, fMAX(SRC, minv)); \
    } while (0)
#define fCRND(A) ((((A) & 0x3) == 0x3) ? ((A) + 1) : ((A)))
#define fRNDN(A, N) ((((N) == 0) ? (A) : (((fSE32_64(A)) + (1 << ((N) - 1))))))
#define fCRNDN(A, N) (conv_round(A, N))
#define fCRNDN64(A, N) (conv_round64(A, N))
#define fADD128(A, B) (add128(A, B))
#define fSUB128(A, B) (sub128(A, B))
#define fSHIFTR128(A, B) (shiftr128(A, B))
#define fSHIFTL128(A, B) (shiftl128(A, B))
#define fAND128(A, B) (and128(A, B))
#define fCAST8S_16S(A) (cast8s_to_16s(A))
#define fCAST16S_8S(A) (cast16s_to_8s(A))
#define fCAST16S_4S(A) (cast16s_to_4s(A))

#ifdef QEMU_GENERATE
#define fEA_RI(REG, IMM) tcg_gen_addi_tl(EA, REG, IMM)
#define fEA_RRs(REG, REG2, SCALE) \
    do { \
        TCGv tmp = tcg_temp_new(); \
        tcg_gen_shli_tl(tmp, REG2, SCALE); \
        tcg_gen_add_tl(EA, REG, tmp); \
        tcg_temp_free(tmp); \
    } while (0)
#define fEA_IRs(IMM, REG, SCALE) \
    do { \
        tcg_gen_shli_tl(EA, REG, SCALE); \
        tcg_gen_addi_tl(EA, EA, IMM); \
    } while (0)
#else
#define fEA_RI(REG, IMM) \
    do { \
        EA = REG + IMM; \
        fDOCHKPAGECROSS(REG, EA); \
    } while (0)
#define fEA_RRs(REG, REG2, SCALE) \
    do { \
        EA = REG + (REG2 << SCALE); \
        fDOCHKPAGECROSS(REG, EA); \
    } while (0)
#define fEA_IRs(IMM, REG, SCALE) \
    do { \
        EA = IMM + (REG << SCALE); \
        fDOCHKPAGECROSS(IMM, EA); \
    } while (0)
#endif

#ifdef QEMU_GENERATE
#define fEA_IMM(IMM) tcg_gen_movi_tl(EA, IMM)
#define fEA_REG(REG) do {\
    tcg_gen_mov_tl(EA, REG); \
    } while(0)
static inline void gen_fbrev(TCGv result, TCGv src)
{
    TCGv result_hi = tcg_temp_new();
    TCGv result_lo = tcg_temp_new();
    TCGv tmp1 = tcg_temp_new();
    TCGv tmp2 = tcg_temp_new();
    int i;

    /*
     *  Bit reverse the low 16 bits of the address
     */
    tcg_gen_andi_tl(result_hi, src, 0xffff0000);
    tcg_gen_movi_tl(result_lo, 0);
    tcg_gen_mov_tl(tmp1, src);
    for (i = 0; i < 16; i++) {
        /*
         * result_lo = (result_lo << 1) | (tmp1 & 1);
         * tmp1 >>= 1;
         */
        tcg_gen_shli_tl(result_lo, result_lo, 1);
        tcg_gen_andi_tl(tmp2, tmp1, 1);
        tcg_gen_or_tl(result_lo, result_lo, tmp2);
        tcg_gen_sari_tl(tmp1, tmp1, 1);
    }
    tcg_gen_or_tl(result, result_hi, result_lo);

    tcg_temp_free(result_hi);
    tcg_temp_free(result_lo);
    tcg_temp_free(tmp1);
    tcg_temp_free(tmp2);
}

#define fEA_BREVR(REG)      gen_helper_fbrev(EA, REG)
#define fEA_GPI(IMM) \
    do { \
        if (insn->extension_valid) { \
            tcg_gen_movi_tl(EA, IMM); \
        } else { \
            tcg_gen_addi_tl(EA, hex_gpr[HEX_REG_GP], IMM); \
        } \
    } while (0)
#define fPM_I(REG, IMM)     tcg_gen_addi_tl(REG, REG, IMM)
#define fPM_M(REG, MVAL)    tcg_gen_add_tl(REG, REG, MVAL)
#define fPM_M_BREV(REG, MVAL)    tcg_gen_add_tl(REG, REG, MVAL)
#else
#define fEA_IMM(IMM) EA = IMM
#define fEA_REG(REG) EA = REG
#define fEA_GPI(IMM) \
    do { \
        EA = fREAD_GP() + IMM; \
        fGP_DOCHKPAGECROSS(fREAD_GP(), EA); \
    } while (0)
#define fPM_I(REG, IMM) \
    do { \
        REG = REG + IMM; \
    } while (0)
#define fPM_M(REG, MVAL) \
    do { \
        REG = REG + MVAL; \
    } while (0)
#define fPM_M_BREV(REG, MVAL) \
    do { \
        REG = REG + MVAL; \
    } while (0)
#define fEA_BREVR(REG) \
    do { \
        EA = fbrev(REG); \
    } while (0)
#endif

#define fPM_CIRI(REG, IMM, MVAL) \
    do { \
        TCGv tcgv_siV = tcg_const_tl(siV); \
        gen_helper_fcircadd(REG, REG, tcgv_siV, MuV, \
                            hex_gpr[HEX_REG_CS0 + MuN]); \
        tcg_temp_free(tcgv_siV); \
    } while (0)

#define fPM_CIRR(REG, VAL, MVAL) \
    do { \
        fcirc_add(REG, VAL, MuV); \
    } while (0)
#define fMODCIRCU(N, P) ((N) & ((1 << (P)) - 1))
#define fSCALE(N, A) (((size8s_t)(A)) << N)
#define fVSATW(A) fVSATN(32, ((long long)A))
#define fSATW(A) fSATN(32, ((long long)A))
#define fVSAT(A) fVSATN(32, (A))
#define fSAT(A) fSATN(32, (A))
#define fSAT_ORIG_SHL(A, ORIG_REG) \
    ((((size4s_t)((fSAT(A)) ^ ((size4s_t)(ORIG_REG)))) < 0) \
        ? fSATVALN(32, ((size4s_t)(ORIG_REG))) \
        : ((((ORIG_REG) > 0) && ((A) == 0)) ? fSATVALN(32, (ORIG_REG)) \
                                            : fSAT(A)))
#define fPASS(A) A
#define fRND(A) (((A) + 1) >> 1)
#define fBIDIR_SHIFTL(SRC, SHAMT, REGSTYPE) \
    (((SHAMT) < 0) ? ((fCAST##REGSTYPE(SRC) >> ((-(SHAMT)) - 1)) >> 1) \
                   : (fCAST##REGSTYPE(SRC) << (SHAMT)))
#define fBIDIR_ASHIFTL(SRC, SHAMT, REGSTYPE) \
    fBIDIR_SHIFTL(SRC, SHAMT, REGSTYPE##s)
#define fBIDIR_LSHIFTL(SRC, SHAMT, REGSTYPE) \
    fBIDIR_SHIFTL(SRC, SHAMT, REGSTYPE##u)
#define fBIDIR_ASHIFTL_SAT(SRC, SHAMT, REGSTYPE) \
    (((SHAMT) < 0) ? ((fCAST##REGSTYPE##s(SRC) >> ((-(SHAMT)) - 1)) >> 1) \
                   : fSAT_ORIG_SHL(fCAST##REGSTYPE##s(SRC) << (SHAMT), (SRC)))
#define fBIDIR_SHIFTR(SRC, SHAMT, REGSTYPE) \
    (((SHAMT) < 0) ? ((fCAST##REGSTYPE(SRC) << ((-(SHAMT)) - 1)) << 1) \
                   : (fCAST##REGSTYPE(SRC) >> (SHAMT)))
#define fBIDIR_ASHIFTR(SRC, SHAMT, REGSTYPE) \
    fBIDIR_SHIFTR(SRC, SHAMT, REGSTYPE##s)
#define fBIDIR_LSHIFTR(SRC, SHAMT, REGSTYPE) \
    fBIDIR_SHIFTR(SRC, SHAMT, REGSTYPE##u)
#define fBIDIR_ASHIFTR_SAT(SRC, SHAMT, REGSTYPE) \
    (((SHAMT) < 0) ? fSAT_ORIG_SHL((fCAST##REGSTYPE##s(SRC) \
                        << ((-(SHAMT)) - 1)) << 1, (SRC)) \
                   : (fCAST##REGSTYPE##s(SRC) >> (SHAMT)))
#ifdef QEMU_GENERATE
#define fASHIFTR(DST, SRC, SHAMT, REGSTYPE) \
    gen_ashiftr_##REGSTYPE##s(DST, SRC, SHAMT)
#define fLSHIFTR(DST, SRC, SHAMT, REGSTYPE) \
    gen_lshiftr_##REGSTYPE##u(DST, SRC, SHAMT)
#else
#define fASHIFTR(SRC, SHAMT, REGSTYPE) (fCAST##REGSTYPE##s(SRC) >> (SHAMT))
#define fLSHIFTR(SRC, SHAMT, REGSTYPE) \
    (((SHAMT) >= 64) ? 0 : (fCAST##REGSTYPE##u(SRC) >> (SHAMT)))
#endif
#define fROTL(SRC, SHAMT, REGSTYPE) \
    (((SHAMT) == 0) ? (SRC) : ((fCAST##REGSTYPE##u(SRC) << (SHAMT)) | \
                              ((fCAST##REGSTYPE##u(SRC) >> \
                                 ((sizeof(SRC) * 8) - (SHAMT))))))
#define fROTR(SRC, SHAMT, REGSTYPE) \
    (((SHAMT) == 0) ? (SRC) : ((fCAST##REGSTYPE##u(SRC) >> (SHAMT)) | \
                              ((fCAST##REGSTYPE##u(SRC) << \
                                 ((sizeof(SRC) * 8) - (SHAMT))))))
#ifdef QEMU_GENERATE
#define fASHIFTL(DST, SRC, SHAMT, REGSTYPE) \
    gen_ashiftl_##REGSTYPE##s(DST, SRC, SHAMT)
#else
#define fASHIFTL(SRC, SHAMT, REGSTYPE) \
    (((SHAMT) >= 64) ? 0 : (fCAST##REGSTYPE##s(SRC) << (SHAMT)))
#endif
#define fFLOAT(A) \
    ({ union { float f; size4u_t i; } _fipun; _fipun.i = (A); _fipun.f; })
#define fUNFLOAT(A) \
    ({ union { float f; size4u_t i; } _fipun; \
     _fipun.f = (A); isnan(_fipun.f) ? 0xFFFFFFFFU : _fipun.i; })
#define fSFNANVAL() 0xffffffff
#define fSFINFVAL(A) (((A) & 0x80000000) | 0x7f800000)
#define fSFONEVAL(A) (((A) & 0x80000000) | fUNFLOAT(1.0))
#define fCHECKSFNAN(DST, A) \
    do { \
        if (isnan(fFLOAT(A))) { \
            if ((fGETBIT(22, A)) == 0) { \
                fRAISEFLAGS(FE_INVALID); \
            } \
            DST = fSFNANVAL(); \
        } \
    } while (0)
#define fCHECKSFNAN3(DST, A, B, C) \
    do { \
        fCHECKSFNAN(DST, A); \
        fCHECKSFNAN(DST, B); \
        fCHECKSFNAN(DST, C); \
    } while (0)
#define fSF_BIAS() 127
#define fSF_MANTBITS() 23
#define fSF_RECIP_LOOKUP(IDX) arch_recip_lookup(IDX)
#define fSF_INVSQRT_LOOKUP(IDX) arch_invsqrt_lookup(IDX)
#define fSF_MUL_POW2(A, B) \
    (fUNFLOAT(fFLOAT(A) * fFLOAT((fSF_BIAS() + (B)) << fSF_MANTBITS())))
#define fSF_GETEXP(A) (((A) >> fSF_MANTBITS()) & 0xff)
#define fSF_MAXEXP() (254)
#define fSF_RECIP_COMMON(N, D, O, A, S) arch_sf_recip_common((float32 *)&N, (float32 *)&D, (float32 *)&O, &A, S)
#define fSF_INVSQRT_COMMON(N, O, A, S) arch_sf_invsqrt_common((float32 *)&N, (float32 *)&O, &A, S)
#define fFMAFX(A, B, C, ADJ) internal_fmafx(A, B, C, fSXTN(8, 64, ADJ))
#define fFMAF(A, B, C) internal_fmafx(A, B, C, 0)
#define fSFMPY(A, B) internal_mpyf(A, B)
#define fMAKESF(SIGN, EXP, MANT) \
    ((((SIGN) & 1) << 31) | \
     (((EXP) & 0xff) << fSF_MANTBITS()) | \
     ((MANT) & ((1 << fSF_MANTBITS()) - 1)))
#define fDOUBLE(A) \
    ({ union { double f; size8u_t i; } _fipun; _fipun.i = (A); _fipun.f; })
#define fUNDOUBLE(A) \
    ({ union { double f; size8u_t i; } _fipun; \
     _fipun.f = (A); \
     isnan(_fipun.f) ? 0xFFFFFFFFFFFFFFFFULL : _fipun.i; })
#define fDFNANVAL() 0xffffffffffffffffULL
#define fDFINFVAL(A) (((A) & 0x8000000000000000ULL) | 0x7ff0000000000000ULL)
#define fDFONEVAL(A) (((A) & 0x8000000000000000ULL) | fUNDOUBLE(1.0))
#define fCHECKDFNAN(DST, A) \
    do { \
        if (isnan(fDOUBLE(A))) { \
            if ((fGETBIT(51, A)) == 0) { \
                fRAISEFLAGS(FE_INVALID); \
            } \
            DST = fDFNANVAL(); \
        } \
    } while (0)
#define fCHECKDFNAN3(DST, A, B, C) \
    do { \
        fCHECKDFNAN(DST, A); \
        fCHECKDFNAN(DST, B); \
        fCHECKDFNAN(DST, C); \
    } while (0)
#define fDF_BIAS() 1023
#define fDF_ISNORMAL(X) (fpclassify(fDOUBLE(X)) == FP_NORMAL)
#define fDF_ISDENORM(X) (fpclassify(fDOUBLE(X)) == FP_SUBNORMAL)
#define fDF_ISBIG(X) (fDF_GETEXP(X) >= 512)
#define fDF_MANTBITS() 52
#define fDF_RECIP_LOOKUP(IDX) (size8u_t)(arch_recip_lookup(IDX))
#define fDF_INVSQRT_LOOKUP(IDX) (size8u_t)(arch_invsqrt_lookup(IDX))
#define fDF_MUL_POW2(A, B) \
    (fUNDOUBLE(fDOUBLE(A) * fDOUBLE((0ULL + fDF_BIAS() + (B)) << \
     fDF_MANTBITS())))
#define fDF_GETEXP(A) (((A) >> fDF_MANTBITS()) & 0x7ff)
#define fDF_MAXEXP() (2046)
#define fDF_RECIP_COMMON(N, D, O, A) arch_df_recip_common(&N, &D, &O, &A)
#define fDF_INVSQRT_COMMON(N, O, A) arch_df_invsqrt_common(&N, &O, &A)
#define fFMA(A, B, C) internal_fma(A, B, C)
#define fDFMPY(A, B) internal_mpy(A, B)
#define fDF_MPY_HH(A, B, ACC) internal_mpyhh(A, B, ACC)
#define fFMAX(A, B, C, ADJ) internal_fmax(A, B, C, fSXTN(8, 64, ADJ) * 2)
#define fMAKEDF(SIGN, EXP, MANT) \
    ((((SIGN) & 1ULL) << 63) | \
     (((EXP) & 0x7ffULL) << fDF_MANTBITS()) | \
     ((MANT) & ((1ULL << fDF_MANTBITS()) - 1)))

#ifdef QEMU_GENERATE
/* These will be needed if we write any FP instructions with TCG */
#define fFPOP_START()      /* nothing */
#define fFPOP_END()        /* nothing */
#else
#define fFPOP_START() arch_fpop_start(env)
#define fFPOP_END() arch_fpop_end(env)
#endif

#define fFPSETROUND_NEAREST() fesetround(FE_TONEAREST)
#define fFPSETROUND_CHOP() fesetround(FE_TOWARDZERO)
#define fFPCANCELFLAGS() feclearexcept(FE_ALL_EXCEPT)
#define fISINFPROD(A, B) \
    ((isinf(A) && isinf(B)) || \
     (isinf(A) && isfinite(B) && ((B) != 0.0)) || \
     (isinf(B) && isfinite(A) && ((A) != 0.0)))
#define fISZEROPROD(A, B) \
    ((((A) == 0.0) && isfinite(B)) || (((B) == 0.0) && isfinite(A)))
#define fRAISEFLAGS(A) arch_raise_fpflag(A)
#define fDF_MAX(A, B) \
    (((A) == (B)) ? fDOUBLE(fUNDOUBLE(A) & fUNDOUBLE(B)) : fmax(A, B))
#define fDF_MIN(A, B) \
    (((A) == (B)) ? fDOUBLE(fUNDOUBLE(A) | fUNDOUBLE(B)) : fmin(A, B))
#define fSF_MAX(A, B) \
    (((A) == (B)) ? fFLOAT(fUNFLOAT(A) & fUNFLOAT(B)) : fmaxf(A, B))
#define fSF_MIN(A, B) \
    (((A) == (B)) ? fFLOAT(fUNFLOAT(A) | fUNFLOAT(B)) : fminf(A, B))
#define fMMU(ADDR) ADDR

#ifdef QEMU_GENERATE
#define fcirc_add(REG, INCR, MV) \
    gen_helper_fcircadd(REG, REG, INCR, MV, fREAD_CSREG(MuN))
#endif

#define fbrev(REG) (fbrevaddr(REG))

#ifdef QEMU_GENERATE
#define fLOAD(NUM, SIZE, SIGN, EA, DST) MEM_LOAD##SIZE##SIGN(DST, EA)
#else
#define fLOAD(NUM, SIZE, SIGN, EA, DST) \
    DST = (size##SIZE##SIGN##_t)MEM_LOAD##SIZE##SIGN(EA);
#endif

#define fMEMOP(NUM, SIZE, SIGN, EA, FNTYPE, VALUE)

#ifdef QEMU_GENERATE
#define fGET_FRAMEKEY() READ_REG(tmp, HEX_REG_FRAMEKEY)
static inline TCGv_i64 gen_frame_scramble(TCGv_i64 result)
{
    /* ((LR << 32) | FP) ^ (FRAMEKEY << 32)) */
    TCGv_i64 LR_i64 = tcg_temp_new_i64();
    TCGv_i64 FP_i64 = tcg_temp_new_i64();
    TCGv_i64 FRAMEKEY_i64 = tcg_temp_new_i64();

    tcg_gen_extu_i32_i64(LR_i64, hex_gpr[HEX_REG_LR]);
    tcg_gen_extu_i32_i64(FP_i64, hex_gpr[HEX_REG_FP]);
    tcg_gen_extu_i32_i64(FRAMEKEY_i64, hex_gpr[HEX_REG_FRAMEKEY]);

    tcg_gen_shli_i64(LR_i64, LR_i64, 32);
    tcg_gen_shli_i64(FRAMEKEY_i64, FRAMEKEY_i64, 32);
    tcg_gen_or_i64(result, LR_i64, FP_i64);
    tcg_gen_xor_i64(result, result, FRAMEKEY_i64);

    tcg_temp_free_i64(LR_i64);
    tcg_temp_free_i64(FP_i64);
    tcg_temp_free_i64(FRAMEKEY_i64);
    return result;
}
#define fFRAME_SCRAMBLE(VAL) gen_frame_scramble(scramble_tmp)
static inline TCGv_i64 gen_frame_unscramble(TCGv_i64 frame)
{
    TCGv_i64 FRAMEKEY_i64 = tcg_temp_new_i64();
    tcg_gen_extu_i32_i64(FRAMEKEY_i64, hex_gpr[HEX_REG_FRAMEKEY]);
    tcg_gen_shli_i64(FRAMEKEY_i64, FRAMEKEY_i64, 32);
    tcg_gen_xor_i64(frame, frame, FRAMEKEY_i64);
    tcg_temp_free_i64(FRAMEKEY_i64);
    return frame;
}
#define fFRAME_UNSCRAMBLE(VAL) gen_frame_unscramble(VAL)
#else
#define fGET_FRAMEKEY()         g_assert_not_reached() //READ_REG(HEX_REG_FRAMEKEY)
#define fFRAME_SCRAMBLE(VAL)    g_assert_not_reached()
#define fFRAME_UNSCRAMBLE(VAL)  g_assert_not_reached()
#endif

#ifdef CONFIG_USER_ONLY
#define fFRAMECHECK(ADDR, EA) do { } while (0) /* Not modelled in linux-user */
#else
#ifdef QEMU_GENERATE
#if 0
#define fFRAMECHECK(ADDR, EA) \
    do { \
        TCGLabel *ok = gen_new_label(); \
        tcg_gen_brcond_tl(TCG_COND_GEU, ADDR, hex_gpr[HEX_REG_FRAMELIMIT], \
                          ok); \
        TCGv_i32 slot = tcg_const_i32(insn->slot); \
        gen_helper_raise_stack_overflow(cpu_env, slot, EA); \
        gen_set_label(ok); \
    } while (0)
#else
#define fFRAMECHECK(ADDR, EA)
#endif
#endif
#endif

#ifdef QEMU_GENERATE
#define fLOAD_LOCKED(NUM, SIZE, SIGN, EA, DST) \
    gen_load_locked##SIZE##SIGN(DST, EA, ctx->mem_idx);
#else
#define fLOAD_LOCKED(NUM, SIZE, SIGN, EA, DST) \
    g_assert_not_reached();
#endif

#ifdef CONFIG_USER_ONLY
#define fLOAD_PHYS(NUM, SIZE, SIGN, SRC1, SRC2, DST)
#else
#define fLOAD_PHYS(NUM, SIZE, SIGN, SRC1, SRC2, DST) { \
  const uintptr_t rs = ((unsigned long)(unsigned)(SRC1)) & 0x7ff; \
  const uintptr_t rt = ((unsigned long)(unsigned)(SRC2)) << 11; \
  const uintptr_t addr = rs + rt;         \
  cpu_physical_memory_read(addr, &DST, sizeof(uint32_t)); \
}
#endif

#ifdef QEMU_GENERATE
#define fSTORE(NUM, SIZE, EA, SRC) MEM_STORE##SIZE(EA, SRC, insn->slot)
#else
#define fSTORE(NUM, SIZE, EA, SRC) MEM_STORE##SIZE(EA, SRC, slot);
#endif

#ifdef QEMU_GENERATE
#define fSTORE_LOCKED(NUM, SIZE, EA, SRC, PRED) \
    gen_store_conditional##SIZE(ctx, PRED, EA, SRC);
#else
#define fSTORE_LOCKED(NUM, SIZE, EA, SRC, PRED) \
    g_assert_not_reached();
#endif

#define fVTCM_MEMCPY(DST, SRC, SIZE)
#define fPERMUTEH(SRC0, SRC1, CTRL) fpermuteh((SRC0), (SRC1), CTRL)
#define fPERMUTEB(SRC0, SRC1, CTRL) fpermuteb((SRC0), (SRC1), CTRL)

#ifdef QEMU_GENERATE
#define GETBYTE_FUNC(X) \
    __builtin_choose_expr(TYPE_TCGV(X), \
        gen_get_byte, \
        __builtin_choose_expr(TYPE_TCGV_I64(X), \
            gen_get_byte_i64, (void)0))
#define fGETBYTE(N, SRC) GETBYTE_FUNC(SRC)(BYTE, N, SRC, true)
#define fGETUBYTE(N, SRC) GETBYTE_FUNC(SRC)(BYTE, N, SRC, false)
#else
#define fGETBYTE(N, SRC) ((size1s_t)((SRC >> ((N) * 8)) & 0xff))
#define fGETUBYTE(N, SRC) ((size1u_t)((SRC >> ((N) * 8)) & 0xff))
#endif

#ifdef QEMU_GENERATE
#define SETBYTE_FUNC(X) \
    _Generic((X), TCGv_i32 : gen_set_byte, TCGv_i64 : gen_set_byte_i64)
#define fSETBYTE(N, DST, VAL) SETBYTE_FUNC(DST)(N, DST, VAL)

#define fGETHALF(N, SRC)  gen_get_half(HALF, N, SRC, true)
#define fGETUHALF(N, SRC) gen_get_half(HALF, N, SRC, false)

#define SETHALF_FUNC(X) \
    _Generic((X), TCGv_i32 : gen_set_half, TCGv_i64 : gen_set_half_i64)
#define fSETHALF(N, DST, VAL) SETHALF_FUNC(DST)(N, DST, VAL)
#define fSETHALFw(N, DST, VAL) gen_set_half(N, DST, VAL)
#define fSETHALFd(N, DST, VAL) gen_set_half_i64(N, DST, VAL)
#else
#define fSETBYTE(N, DST, VAL) \
    do { \
        DST = (DST & ~(0x0ffLL << ((N) * 8))) | \
        (((size8u_t)((VAL) & 0x0ffLL)) << ((N) * 8)); \
    } while (0)
#define fGETHALF(N, SRC) ((size2s_t)((SRC >> ((N) * 16)) & 0xffff))
#define fGETUHALF(N, SRC) ((size2u_t)((SRC >> ((N) * 16)) & 0xffff))
#define fSETHALF(N, DST, VAL) \
    do { \
        DST = (DST & ~(0x0ffffLL << ((N) * 16))) | \
        (((size8u_t)((VAL) & 0x0ffff)) << ((N) * 16)); \
    } while (0)
#define fSETHALFw fSETHALF
#define fSETHALFd fSETHALF
#endif

#ifdef QEMU_GENERATE
#define GETWORD_FUNC(X) \
    _Generic((X), TCGv_i32 : gen_get_word, TCGv_i64 : gen_get_word_i64)
#define fGETWORD(N, SRC)  GETWORD_FUNC(WORD)(WORD, N, SRC, true)
#define fGETUWORD(N, SRC) GETWORD_FUNC(WORD)(WORD, N, SRC, false)
#else
#define fGETWORD(N, SRC) \
    ((size8s_t)((size4s_t)((SRC >> ((N) * 32)) & 0x0ffffffffLL)))
#define fGETUWORD(N, SRC) \
    ((size8u_t)((size4u_t)((SRC >> ((N) * 32)) & 0x0ffffffffLL)))
#endif

#define fSETWORD(N, DST, VAL) \
    do { \
        DST = (DST & ~(0x0ffffffffLL << ((N) * 32))) | \
              (((VAL) & 0x0ffffffffLL) << ((N) * 32)); \
    } while (0)
#define fACC()
#define fEXTENSION_AUDIO(A) A

#ifdef QEMU_GENERATE
#define fSETBIT(N, DST, VAL) gen_set_bit((N), (DST), (VAL));
#else
#define fSETBIT(N, DST, VAL) \
    do { \
        DST = (DST & ~(1ULL << (N))) | (((size8u_t)(VAL)) << (N)); \
    } while (0)
#endif

#define fGETBIT(N, SRC) (((SRC) >> N) & 1)
#define fSETBITS(HI, LO, DST, VAL) \
    do { \
        int j; \
        for (j = LO; j <= HI; j++) { \
            fSETBIT(j, DST, VAL); \
        } \
    } while (0)
#define fCOUNTONES_2(VAL) ctpop16(VAL)
#define fCOUNTONES_4(VAL) ctpop32(VAL)
#define fCOUNTONES_8(VAL) ctpop64(VAL)
#define fBREV_8(VAL) revbit64(VAL)
#define fBREV_4(VAL) revbit32(VAL)
#define fBREV_2(VAL) revbit16(VAL)
#define fBREV_1(VAL) revbit8(VAL)
#define fCL1_8(VAL) clo64(VAL)
#define fCL1_4(VAL) clo32(VAL)
#define fCL1_2(VAL) clo16(VAL)
#define fCL1_1(VAL) clo8(VAL)
#define fINTERLEAVE(ODD, EVEN) interleave(ODD, EVEN)
#define fDEINTERLEAVE(MIXED) deinterleave(MIXED)
#define fNORM16(VAL) \
    ((VAL == 0) ? (31) : (fMAX(fCL1_2(VAL), fCL1_2(~VAL)) - 1))
#define fHIDE(A) A
#define fCONSTLL(A) A##LL
#define fCONSTULL(A) A##ULL
#define fECHO(A) (A)

#define fDO_TRACE(SREG)
#define fBREAK()
#define fGP_DOCHKPAGECROSS(BASE, SUM)
#define fDOCHKPAGECROSS(BASE, SUM)
#define fUNPAUSE()

#ifdef CONFIG_USER_ONLY
#define fTRAP(TRAPTYPE, IMM) helper_raise_exception(env, HEX_EVENT_TRAP0)
#define fCHECKFORPRIV()                                 \
    do {                                                \
        env->cause_code = HEX_CAUSE_PRIV_USER_NO_SINSN; \
        helper_raise_exception(env, HEX_EVENT_PRECISE); \
    } while (0);
#define fCHECKFORGUEST()                                 \
    do {                                                 \
        env->cause_code = HEX_CAUSE_GUEST_USER_NO_GINSN; \
        helper_raise_exception(env, HEX_EVENT_PRECISE);  \
    } while (0);
#else
#define fTRAP(TRAPTYPE, IMM) { \
  register_trap_exception(env, fREAD_NPC(), TRAPTYPE, IMM); \
}
#ifdef QEMU_GENERATE
#define fCHECKFORPRIV() gen_helper_checkforpriv(cpu_env)
#define fCHECKFORGUEST() gen_helper_checkforguest(cpu_env)
#endif
#endif

#define fALIGN_REG_FIELD_VALUE(FIELD, VAL) \
    ((VAL) << reg_field_info[FIELD].offset)
#define fGET_REG_FIELD_MASK(FIELD) \
    (((1 << reg_field_info[FIELD].width) - 1) << reg_field_info[FIELD].offset)
#define fLOG_REG_FIELD(REG, FIELD, VAL)
#define fWRITE_GLOBAL_REG_FIELD(REG, FIELD, VAL)
#define fLOG_GLOBAL_REG_FIELD(REG, FIELD, VAL)
#define fREAD_REG_FIELD(REG, FIELD) \
    fEXTRACTU_BITS(env->gpr[HEX_REG_##REG], \
                   reg_field_info[FIELD].width, \
                   reg_field_info[FIELD].offset)
#define fREAD_GLOBAL_REG_FIELD(REG, FIELD)
#define fGET_FIELD(VAL, FIELD) \
    fEXTRACTU_BITS(VAL, \
                   reg_field_info[FIELD].width, \
                   reg_field_info[FIELD].offset)
#define fSET_FIELD(VAL, FIELD, NEWVAL) \
    fINSERT_BITS(VAL, \
                 reg_field_info[FIELD].width, \
                 reg_field_info[FIELD].offset, \
                 (NEWVAL))
#define fPOW2_HELP_ROUNDUP(VAL) \
    ((VAL) | \
     ((VAL) >> 1) | \
     ((VAL) >> 2) | \
     ((VAL) >> 4) | \
     ((VAL) >> 8) | \
     ((VAL) >> 16))
#define fPOW2_ROUNDUP(VAL) (fPOW2_HELP_ROUNDUP((VAL) - 1) + 1)
#define fBARRIER()
#define fSYNCH()
#define fISYNC()
#define fICFETCH(REG)
#define fDCFETCH(REG) do { REG = REG; } while (0) /* Nothing to do in qemu */
#define fICINVIDX(REG)
#define fICINVA(REG) do { REG = REG; } while (0) /* Nothing to do in qemu */
#define fICKILL()
#define fDCKILL()
#define fL2KILL()
#define fL2UNLOCK()
#define fL2CLEAN()
#define fL2CLEANINV()
#define fL2CLEANPA(REG)
#define fL2CLEANINVPA(REG)
#define fL2CLEANINVIDX(REG)
#define fL2CLEANIDX(REG)
#define fL2INVIDX(REG)
#define fL2FETCH(ADDR, HEIGHT, WIDTH, STRIDE, FLAGS)
#define fL2TAGR(INDEX, DST, DSTREG)
#define fL2UNLOCKA(VA)
#define fL2TAGW(INDEX, PART2)
#define fDCCLEANIDX(REG)
#define fDCCLEANA(REG) do { REG = REG; } while (0) /* Nothing to do in qemu */
#define fDCCLEANINVIDX(REG)
#define fDCCLEANINVA(REG) \
    do { REG = REG; } while (0) /* Nothing to do in qemu */

#ifdef QEMU_GENERATE
#define fL2LOCKA(VA, DST, PREGDST) \
    tcg_gen_movi_tl((DST), 0x0ff) // FIXME untested
#define fDCZEROA(REG) tcg_gen_mov_tl(hex_dczero_addr, (REG))
#else
/* Always succeed: */
#define fL2LOCKA(EA,PDV,PDN) (PDV = 0xFF)
#define fDCZEROA(REG) do { REG = REG; g_assert_not_reached(); } while (0)
#define fCLEAR_RTE_EX() \
    do { \
        uint32_t tmp = 0; \
        tmp = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR); \
        fINSERT_BITS(tmp,reg_field_info[SSR_EX].width, \
                     reg_field_info[SSR_EX].offset,0); \
        log_sreg_write(env, HEX_SREG_SSR, tmp, slot); \
    } while (0)
#endif

#define fDCINVIDX(REG)
#define fDCINVA(REG) do { REG = REG; } while (0) /* Nothing to do in qemu */
#define fBRANCH_SPECULATED_RIGHT(JC, SD, DOTNEWVAL) \
    (((JC) ^ (SD) ^ (DOTNEWVAL & 1)) & 0x1)
#define fBRANCH_SPECULATE_STALL(DOTNEWVAL, JUMP_COND, SPEC_DIR, HINTBITNUM, \
                                STRBITNUM) /* Nothing */

#define IV1DEAD()
#define fVIRTINSN_SPSWAP(IMM, REG)
#define fVIRTINSN_GETIE(IMM, REG) { REG = 0xdeafbeef; }
#define fVIRTINSN_SETIE(IMM, REG)
#define fVIRTINSN_RTE(IMM, REG)
#ifdef CONFIG_USER_ONLY
#define fTRAP1_VIRTINSN(IMM) \
    (((IMM) == 1) || ((IMM) == 3) || ((IMM) == 4) || ((IMM) == 6))
#else
#define fGRE_ENABLED() GET_FIELD(CCR_GRE, READ_SREG(HEX_SREG_CCR))
#define fTRAP1_VIRTINSN(IMM) \
    (fGRE_ENABLED() && (((IMM) == 1) || ((IMM) == 3) || ((IMM) == 4) || ((IMM) == 6)))
#endif
#define fNOP_EXECUTED
#define fPREDUSE_TIMING()
#define fSET_TLB_LOCK()       hex_tlb_lock(env);
#define fCLEAR_TLB_LOCK()     hex_tlb_unlock(env);

#define fSET_K0_LOCK()        hex_k0_lock(env);
#define fCLEAR_K0_LOCK()      hex_k0_unlock(env);

#define fGET_TNUM()               thread->threadId
#define fSTART(REG)               helper_fstart(env, REG)
#define fRESUME(REG)              helper_fresume(env, REG)
#define fCLEAR_RUN_MODE(x)        helper_clear_run_mode(env,(x))
#define READ_IMASK(TID)           helper_getimask(env, TID)
#define WRITE_IMASK(PRED,MASK)    helper_setimask(env, PRED, MASK)
#define WRITE_PRIO(TH,PRIO)       helper_setprio(env, TH, PRIO)

#ifndef CONFIG_USER_ONLY
#define fTLB_IDXMASK(INDEX) \
    ((INDEX) & (fPOW2_ROUNDUP(fCAST4u(NUM_TLB_ENTRIES)) - 1))

#define fTLB_NONPOW2WRAP(INDEX) \
    (((INDEX) >= NUM_TLB_ENTRIES) ? ((INDEX) - NUM_TLB_ENTRIES) : (INDEX))

#define fTLBW(INDEX, VALUE) \
    hex_tlbw(env, (INDEX), (VALUE))
#define fTLBW_EXTENDED(INDEX, VALUE) \
    hex_tlbw(env, (INDEX), (VALUE))
#define fTLB_ENTRY_OVERLAP(VALUE,INDEX) \
    (hex_tlb_check_overlap(env, VALUE, INDEX) != -2)
#define fTLB_ENTRY_OVERLAP_IDX(VALUE,INDEX) \
    hex_tlb_check_overlap(env, VALUE, INDEX)
#define fTLBR(INDEX) \
    (env->hex_tlb->entries[fTLB_NONPOW2WRAP(fTLB_IDXMASK(INDEX))])
#define fTLBR_EXTENDED(INDEX) \
    (env->hex_tlb->entries[fTLB_NONPOW2WRAP(fTLB_IDXMASK(INDEX))])
#define fTLBP(TLBHI) \
    hex_tlb_lookup(env, ((TLBHI) >> 12), ((TLBHI) << 12))
#endif

/* FIXME - Update these when properly implementing stop instruction */

#define fSET_WAIT_MODE(TNUM)      helper_fwait(env, TNUM)

#define fIN_DEBUG_MODE(TNUM) \
    0    /* FIXME */
#define fIN_DEBUG_MODE_NO_ISDB(TNUM) \
    0    /* FIXME */
#define fIN_DEBUG_MODE_WARN(TNUM)

#ifdef QEMU_GENERATE

#define DO_IASSIGNR(RS, RD)                   \
    do {                                      \
        gen_helper_iassignr(RD, cpu_env, RS); \
    } while (0)
#define DO_IASSIGNW(RS)                   \
    do {                                  \
        gen_helper_iassignw(cpu_env, RS); \
    } while (0)

#define DO_SIAD(RS) \
    do { \
        TCGv tmp = tcg_temp_new(); \
        gen_get_sreg_field(HEX_SREG_IPENDAD, IPENDAD_IAD, tmp); \
        tcg_gen_ori_tl(tmp, tmp, not_rs); \
        gen_set_sreg_field(HEX_SREG_IPENDAD, IPENDAD_IAD, tmp);  \
    } while (0)

#define DO_CSWI(RS) \
    do { \
        TCGv tmp = tcg_temp_new(); \
        TCGv not_rs = tcg_temp_new(); \
        tcg_gen_not_i32(not_rs, (RS)); \
        gen_get_sreg_field(HEX_SREG_IPENDAD, IPENDAD_IPEND, tmp); \
        tcg_gen_andi_tl(tmp, tmp, not_rs); \
        gen_set_sreg_field(HEX_SREG_IPENDAD, IPENDAD_IPEND, tmp);  \
    } while (0)

#else

#define DO_IASSIGNR(RS, RD)            \
    do {                               \
        RD = helper_iassignr(env, RS); \
    } while (0)
#define DO_IASSIGNW(RS)           \
    do {                          \
        helper_iassignw(env, RS); \
    } while (0)

#define DO_CIAD(RS)                   \
    do {                                  \
        helper_ciad(env, RS); \
    } while (0)

#define DO_SIAD(RS) \
    do { \
        uint32_t tmp = READ_SREG(HEX_SREG_IPENDAD); \
        uint32_t iad = fGET_FIELD(tmp, IPENDAD_IAD); \
        fSET_FIELD(tmp, IPENDAD_IAD, iad | RS); \
        WRITE_SREG(HEX_SREG_IPENDAD, tmp);  \
    } while (0)

#define DO_CSWI(RS) \
    do { \
        uint32_t tmp = READ_SREG(HEX_SREG_IPENDAD); \
        uint32_t ipend = fGET_FIELD(tmp, IPENDAD_IPEND); \
        fSET_FIELD(tmp, IPENDAD_IPEND, tmp & ~ipend); \
        WRITE_SREG(HEX_SREG_IPENDAD, tmp);  \
    } while (0)
#endif

#define DO_SWI(RS)  helper_swi(env, RS)

#endif


#ifdef CONFIG_USER_ONLY
#define fDO_NMI(RS) /* FIXME make this abort */
#else
#define fDO_NMI(RS) helper_nmi(env, RS);
#endif

#ifdef QEMU_GENERATE

/* Read tags back as zero for now: */
#define fICTAGR(RS, RD, RD2) \
    do { \
        /* tag value in RD[31:10] for 32k, RD[31:9] for 16k */ \
        TCGv zero = tcg_const_tl(0); \
        RD = zero; \
        tcg_temp_free(zero); \
    } while (0);
#define fICTAGW(RS, RD)
#define fICDATAR(RS, RD) \
    do { \
        TCGv zero = tcg_const_tl(0); \
        RD = zero; \
        tcg_temp_free(zero); \
    } while (0);
#define fICDATAW(RS, RD)

#define fDCTAGW(RS, RT)
#define fDCTAGR(INDEX, DST, DST_REG_NUM) \
    do { \
        /* tag: RD[23:0], state: RD[30:29] */ \
        TCGv zero = tcg_const_tl(0); \
        DST = zero; \
        tcg_temp_free(zero); \
    } while (0);
#else

/* Read tags back as zero for now: */
#define fICTAGR(RS, RD, RD2) \
    do { \
        /* tag value in RD[31:10] for 32k, RD[31:9] for 16k */ \
        RD = 0x00; \
    } while (0);
#define fICTAGW(RS, RD)
#define fICDATAR(RS, RD) \
    do { \
        RD = 0x00; \
    } while (0);
#define fICDATAW(RS, RD)

#define fDCTAGW(RS, RT)
#define fDCTAGR(INDEX, DST, DST_REG_NUM) \
    do { \
        /* tag: RD[23:0], state: RD[30:29] */ \
        DST = HEX_DC_STATE_INVALID | 0x00; \
    } while (0);

#define fSTORE_DMA(NUM,SIZE,EA,SRC) { mem_dmalink_store(thread, EA, SIZE, SRC, 0); }
#define fDMSTART(NEWPTR) \
    do { \
        dma_adapter_cmd(thread, DMA_CMD_START, NEWPTR, 0); \
        arch_dma_tick_until_stop(thread->processor_ptr, thread->threadId); \
    } while (0)
#define fDMLINK(CURPTR, NEWPTR) \
    do { \
        dma_adapter_cmd(thread, DMA_CMD_LINK, CURPTR, NEWPTR); \
        arch_dma_tick_until_stop(thread->processor_ptr, thread->threadId); \
    } while (0)
#define fDMPOLL(DST) DST=dma_adapter_cmd(thread,DMA_CMD_POLL,0,0)
#define fDMWAIT(DST) DST=dma_adapter_cmd(thread,DMA_CMD_WAIT,0,0)
#define fDMSYNCHT(DST) DST=dma_adapter_cmd(thread,DMA_CMD_SYNCHT,0,0)
#define fDMTLBSYNCH(DST) DST=dma_adapter_cmd(thread,DMA_CMD_TLBSYNCH,0,0)
#define fDMPAUSE(DST) DST=dma_adapter_cmd(thread,DMA_CMD_PAUSE,0,0)
#define fDMRESUME(PTR) dma_adapter_cmd(thread,DMA_CMD_RESUME,PTR,0)
#define fDMWAITDESCRIPTOR(SRC,DST) DST=dma_adapter_cmd(thread,DMA_CMD_WAITDESCRIPTOR,SRC,0)
#define fDMCFGRD(DMANUM,DST) DST=dma_adapter_cmd(thread,DMA_CMD_CFGRD,DMANUM,0)
#define fDMCFGWR(DMANUM,DATA) dma_adapter_cmd(thread,DMA_CMD_CFGWR,DMANUM,DATA)
#define GET_DMA_LDST_ERROR_BADVA(EXTENDED_VA, VA) ((EXTENDED_VA) ? (size4u_t) (( (size8u_t) VA>>32) | (VA & ~0xFFF)) : (size4u_t)VA)

#endif
