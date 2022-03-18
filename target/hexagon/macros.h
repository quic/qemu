/*
 *  Copyright(c) 2019-2022 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#define PCALIGN 4
#define PCALIGN_MASK (PCALIGN - 1)

#define GET_FIELD(FIELD, REGIN) \
    fEXTRACTU_BITS(REGIN, reg_field_info[FIELD].width, \
                   reg_field_info[FIELD].offset)

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
#define CHECK_NOSHUF \
    do { \
        if (insn->slot == 0 && pkt->pkt_has_store_s1) { \
            process_store(ctx, pkt, 1); \
        } \
    } while (0)

#define MEM_LOAD1s(DST, VA) \
    do { \
        CHECK_NOSHUF; \
        tcg_gen_qemu_ld8s(DST, VA, ctx->mem_idx); \
    } while (0)
#define MEM_LOAD1u(DST, VA) \
    do { \
        CHECK_NOSHUF; \
        tcg_gen_qemu_ld8u(DST, VA, ctx->mem_idx); \
    } while (0)
#define MEM_LOAD2s(DST, VA) \
    do { \
        CHECK_NOSHUF; \
        tcg_gen_qemu_ld16s(DST, VA, ctx->mem_idx); \
    } while (0)
#define MEM_LOAD2u(DST, VA) \
    do { \
        CHECK_NOSHUF; \
        tcg_gen_qemu_ld16u(DST, VA, ctx->mem_idx); \
    } while (0)
#define MEM_LOAD4s(DST, VA) \
    do { \
        CHECK_NOSHUF; \
        tcg_gen_qemu_ld32s(DST, VA, ctx->mem_idx); \
    } while (0)
#define MEM_LOAD4u(DST, VA) \
    do { \
        CHECK_NOSHUF; \
        tcg_gen_qemu_ld32s(DST, VA, ctx->mem_idx); \
    } while (0)
#define MEM_LOAD8u(DST, VA) \
    do { \
        CHECK_NOSHUF; \
        tcg_gen_qemu_ld64(DST, VA, ctx->mem_idx); \
    } while (0)

#define MEM_STORE1_FUNC(X) \
    __builtin_choose_expr(TYPE_INT(X), \
        gen_store1i, \
        __builtin_choose_expr(TYPE_TCGV(X), \
            gen_store1, (void)0))
#define MEM_STORE1(VA, DATA, SLOT) \
    MEM_STORE1_FUNC(DATA)(cpu_env, VA, DATA, SLOT)

#define MEM_STORE2_FUNC(X) \
    __builtin_choose_expr(TYPE_INT(X), \
        gen_store2i, \
        __builtin_choose_expr(TYPE_TCGV(X), \
            gen_store2, (void)0))
#define MEM_STORE2(VA, DATA, SLOT) \
    MEM_STORE2_FUNC(DATA)(cpu_env, VA, DATA, SLOT)

#define MEM_STORE4_FUNC(X) \
    __builtin_choose_expr(TYPE_INT(X), \
        gen_store4i, \
        __builtin_choose_expr(TYPE_TCGV(X), \
            gen_store4, (void)0))
#define MEM_STORE4(VA, DATA, SLOT) \
    MEM_STORE4_FUNC(DATA)(cpu_env, VA, DATA, SLOT)

#define MEM_STORE8_FUNC(X) \
    __builtin_choose_expr(TYPE_INT(X), \
        gen_store8i, \
        __builtin_choose_expr(TYPE_TCGV_I64(X), \
            gen_store8, (void)0))
#define MEM_STORE8(VA, DATA, SLOT) \
    MEM_STORE8_FUNC(DATA)(cpu_env, VA, DATA, SLOT)
#endif

#define CANCEL do { } while (0)

#define LOAD_CANCEL(EA) do { CANCEL; } while (0)

#define fMAX(A, B) (((A) > (B)) ? (A) : (B))

#define fMIN(A, B) (((A) < (B)) ? (A) : (B))

#define fABS(A) (((A) < 0) ? (-(A)) : (A))
#define fINSERT_BITS(REG, WIDTH, OFFSET, INVAL) \
    REG = ((WIDTH) ? deposit64(REG, (OFFSET), (WIDTH), (INVAL)) : REG)
#define fEXTRACTU_BITS(INREG, WIDTH, OFFSET) \
    ((WIDTH) ? extract64((INREG), (OFFSET), (WIDTH)) : 0LL)
#define fEXTRACTU_BIDIR(INREG, WIDTH, OFFSET) \
    (fZXTN(WIDTH, 32, fBIDIR_LSHIFTR((INREG), (OFFSET), 4_8)))
#define fEXTRACTU_RANGE(INREG, HIBIT, LOWBIT) \
    (((HIBIT) - (LOWBIT) + 1) ? \
        extract64((INREG), (LOWBIT), ((HIBIT) - (LOWBIT) + 1)) : \
        0LL)
#define fINSERT_RANGE(INREG, HIBIT, LOWBIT, INVAL) \
    do { \
        int width = ((HIBIT) - (LOWBIT) + 1); \
        INREG = (width >= 0 ? \
            deposit64((INREG), (LOWBIT), width, (INVAL)) : \
            INREG); \
    } while (0)

#define f8BITSOF(VAL) ((VAL) ? 0xff : 0x00)

#ifdef QEMU_GENERATE
#define fLSBOLD(VAL) tcg_gen_andi_tl(LSB, (VAL), 1)
#else
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
#define fLSBOLDNOT(VAL) \
    do { \
        tcg_gen_andi_tl(LSB, (VAL), 1); \
        tcg_gen_xori_tl(LSB, LSB, 1); \
    } while (0)
#define fLSBNEWNOT(PNUM) \
    do { \
        tcg_gen_andi_tl(LSB, (PNUM), 1); \
        tcg_gen_xori_tl(LSB, LSB, 1); \
    } while (0)
#define fLSBNEW0NOT \
    do { \
        tcg_gen_andi_tl(LSB, hex_new_pred_value[0], 1); \
        tcg_gen_xori_tl(LSB, LSB, 1); \
    } while (0)
#else
#define fLSBNEWNOT(PNUM) (!fLSBNEW(PNUM))
#define fLSBOLDNOT(VAL) (!fLSBOLD(VAL))
#define fLSBNEW0NOT (!fLSBNEW0)
#define fLSBNEW1NOT (!fLSBNEW1)
#endif

#define fNEWREG(VAL) ((int32_t)(VAL))

#define fNEWREG_ST(VAL) (VAL)

#define fVSATUVALN(N, VAL) \
    ({ \
        (((int64_t)(VAL)) < 0) ? 0 : ((1LL << (N)) - 1); \
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
#define fZXTN(N, M, VAL) (((N) != 0) ? extract64((VAL), 0, (N)) : 0LL)
#define fSXTN(N, M, VAL) (((N) != 0) ? sextract64((VAL), 0, (N)) : 0LL)
#define fSATN(N, VAL) \
    ((fSXTN(N, 64, VAL) == (VAL)) ? (VAL) : fSATVALN(N, VAL))
#define fVSATN(N, VAL) \
    ((fSXTN(N, 64, VAL) == (VAL)) ? (VAL) : fVSATVALN(N, VAL))
#define fADDSAT64(DST, A, B) \
    do { \
        uint64_t __a = fCAST8u(A); \
        uint64_t __b = fCAST8u(B); \
        uint64_t __sum = __a + __b; \
        uint64_t __xor = __a ^ __b; \
        const uint64_t __mask = 0x8000000000000000ULL; \
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
#endif
#define fREAD_LR() (env->gpr[HEX_REG_LR])

#define fREAD_PC() (PC)

#define fREAD_P0() (env->pred[0])
#define fCHECK_PCALIGN(A)

#define fWRITE_NPC(A) write_new_pc(env, pkt_has_multi_cof != 0, A)

#define fBRANCH(LOC, TYPE)          fWRITE_NPC(LOC)
#define fJUMPR(REGNO, TARGET, TYPE) fBRANCH(TARGET, COF_TYPE_JUMPR)
#define fHINTJR(TARGET) { /* Not modelled in qemu */}
#define fWRITE_LOOP_REGS0(START, COUNT) \
    do { \
        log_reg_write(env, HEX_REG_LC0, COUNT);  \
        log_reg_write(env, HEX_REG_SA0, START); \
    } while (0)

#define fSET_OVERFLOW() SET_USR_FIELD(USR_OVF, 1)
#define fSET_LPCFG(VAL) SET_USR_FIELD(USR_LPCFG, (VAL))
#define fGET_LPCFG (GET_USR_FIELD(USR_LPCFG))
#define fWRITE_P0(VAL) log_pred_write(env, 0, VAL)
#define fWRITE_P1(VAL) log_pred_write(env, 1, VAL)
#define fWRITE_P2(VAL) log_pred_write(env, 2, VAL)
#define fWRITE_P3(VAL) log_pred_write(env, 3, VAL)
#define fPART1(WORK) if (part1) { WORK; return; }
#define fCAST4u(A) ((uint32_t)(A))
#define fCAST4s(A) ((int32_t)(A))
#define fCAST8u(A) ((uint64_t)(A))
#define fCAST8s(A) ((int64_t)(A))
#define fCAST2_2s(A) ((int16_t)(A))
#define fCAST2_2u(A) ((uint16_t)(A))
#define fCAST4_4s(A) ((int32_t)(A))
#define fCAST4_4u(A) ((uint32_t)(A))
#define fCAST4_8s(A) ((int64_t)((int32_t)(A)))
#define fCAST4_8u(A) ((uint64_t)((uint32_t)(A)))
#define fCAST8_8s(A) ((int64_t)(A))
#define fCAST8_8u(A) ((uint64_t)(A))
#define fCAST2_8s(A) ((int64_t)((int16_t)(A)))
#define fCAST2_8u(A) ((uint64_t)((uint16_t)(A)))
#define fZE8_16(A) ((int16_t)((uint8_t)(A)))
#define fSE8_16(A) ((int16_t)((int8_t)(A)))
#define fSE16_32(A) ((int32_t)((int16_t)(A)))
#define fZE16_32(A) ((uint32_t)((uint16_t)(A)))
#define fSE32_64(A) ((int64_t)((int32_t)(A)))
#define fZE32_64(A) ((uint64_t)((uint32_t)(A)))
#define fSE8_32(A) ((int32_t)((int8_t)(A)))
#define fZE8_32(A) ((int32_t)((uint8_t)(A)))
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
        int32_t maxv = (1 << U) - 1; \
        int32_t minv = -(1 << U); \
        DST = fMIN(maxv, fMAX(SRC, minv)); \
    } while (0)
#define fCRND(A) ((((A) & 0x3) == 0x3) ? ((A) + 1) : ((A)))
#define fRNDN(A, N) ((((N) == 0) ? (A) : (((fSE32_64(A)) + (1 << ((N) - 1))))))
#define fCRNDN(A, N) (conv_round(A, N))
#define fADD128(A, B) (int128_add(A, B))
#define fSUB128(A, B) (int128_sub(A, B))
#define fSHIFTR128(A, B) (int128_rshift(A, B))
#define fSHIFTL128(A, B) (int128_lshift(A, B))
#define fAND128(A, B) (int128_and(A, B))
#define fCAST8S_16S(A) (int128_exts64(A))
#define fCAST16S_8S(A) (int128_getlo(A))

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
    } while (0)
#define fEA_RRs(REG, REG2, SCALE) \
    do { \
        EA = REG + (REG2 << SCALE); \
    } while (0)
#define fEA_IRs(IMM, REG, SCALE) \
    do { \
        EA = IMM + (REG << SCALE); \
    } while (0)
#endif

#ifdef QEMU_GENERATE
#define fEA_IMM(IMM) tcg_gen_movi_tl(EA, IMM)
#define fEA_REG(REG) tcg_gen_mov_tl(EA, REG)
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
#define fPM_CIRI(REG, IMM, MVAL) \
    do { \
        TCGv tcgv_siV = tcg_const_tl(siV); \
        gen_helper_fcircadd(REG, REG, tcgv_siV, MuV, \
                            hex_gpr[HEX_REG_CS0 + MuN]); \
        tcg_temp_free(tcgv_siV); \
    } while (0)
#else
#define fEA_IMM(IMM)        do { EA = (IMM); } while (0)
#define fEA_REG(REG)        do { EA = (REG); } while (0)
#define fPM_I(REG, IMM)     do { REG = REG + (IMM); } while (0)
#define fPM_M(REG, MVAL)    do { REG = REG + (MVAL); } while (0)
#endif
#define fSCALE(N, A) (((int64_t)(A)) << N)
#define fVSATW(A) fVSATN(32, ((long long)A))
#define fSATW(A) fSATN(32, ((long long)A))
#define fVSAT(A) fVSATN(32, (A))
#define fSAT(A) fSATN(32, (A))
#define fSAT_ORIG_SHL(A, ORIG_REG) \
    ((((int32_t)((fSAT(A)) ^ ((int32_t)(ORIG_REG)))) < 0) \
        ? fSATVALN(32, ((int32_t)(ORIG_REG))) \
        : ((((ORIG_REG) > 0) && ((A) == 0)) ? fSATVALN(32, (ORIG_REG)) \
                                            : fSAT(A)))
#define fPASS(A) A
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
    (((SHAMT) >= (sizeof(SRC) * 8)) ? 0 : (fCAST##REGSTYPE##u(SRC) >> (SHAMT)))
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
    (((SHAMT) >= (sizeof(SRC) * 8)) ? 0 : (fCAST##REGSTYPE##s(SRC) << (SHAMT)))
#endif

#ifdef QEMU_GENERATE
#define fLOAD(NUM, SIZE, SIGN, EA, DST) MEM_LOAD##SIZE##SIGN(DST, EA)
#else
#define fLOAD(NUM, SIZE, SIGN, EA, DST) \
    DST = (size##SIZE##SIGN##_t)MEM_LOAD##SIZE##SIGN(EA)
#endif

#define fMEMOP(NUM, SIZE, SIGN, EA, FNTYPE, VALUE)

#ifdef QEMU_GENERATE
#define fGET_FRAMEKEY() gen_read_reg(tmp, HEX_REG_FRAMEKEY)
static inline TCGv_i64 gen_frame_scramble(TCGv_i64 result)
{
    /* ((LR << 32) | FP) ^ (FRAMEKEY << 32)) */
    TCGv_i64 FRAMEKEY_i64 = tcg_temp_new_i64();

    tcg_gen_extu_i32_i64(FRAMEKEY_i64, hex_gpr[HEX_REG_FRAMEKEY]);
    tcg_gen_shli_i64(FRAMEKEY_i64, FRAMEKEY_i64, 32);

    tcg_gen_concat_i32_i64(result, hex_gpr[HEX_REG_FP], hex_gpr[HEX_REG_LR]);
    tcg_gen_xor_i64(result, result, FRAMEKEY_i64);

    tcg_temp_free_i64(FRAMEKEY_i64);
    return result;
}
static inline TCGv_i64 gen_frame_unscramble(TCGv_i64 frame)
{
    TCGv_i64 FRAMEKEY_i64 = tcg_temp_new_i64();
    tcg_gen_extu_i32_i64(FRAMEKEY_i64, hex_gpr[HEX_REG_FRAMEKEY]);
    tcg_gen_shli_i64(FRAMEKEY_i64, FRAMEKEY_i64, 32);
    tcg_gen_xor_i64(frame, frame, FRAMEKEY_i64);
    tcg_temp_free_i64(FRAMEKEY_i64);
    return frame;
}
#endif

#ifdef CONFIG_USER_ONLY
#define fFRAMECHECK(ADDR, EA) do { } while (0) /* Not modelled in linux-user */
#else
/* System mode not implemented yet */
#define fFRAMECHECK(ADDR, EA)  g_assert_not_reached();
#endif

#ifdef QEMU_GENERATE
#define fLOAD_LOCKED(NUM, SIZE, SIGN, EA, DST) \
    gen_load_locked##SIZE##SIGN(DST, EA, ctx->mem_idx);
#endif

#ifdef QEMU_GENERATE
#define fSTORE(NUM, SIZE, EA, SRC) MEM_STORE##SIZE(EA, SRC, insn->slot)
#define fSTORE1(EA, SRC) MEM_STORE1(EA, SRC, insn->slot)
#define fSTORE2(EA, SRC) MEM_STORE2(EA, SRC, insn->slot)
#define fSTORE4(EA, SRC) MEM_STORE4(EA, SRC, insn->slot)
#define fSTORE8(EA, SRC) MEM_STORE8(EA, SRC, insn->slot)
#else
#define fSTORE(NUM, SIZE, EA, SRC) MEM_STORE##SIZE(EA, SRC, slot)
#define fSTORE1(EA, SRC) MEM_STORE1(EA, SRC, slot)
#define fSTORE2(EA, SRC) MEM_STORE2(EA, SRC, slot)
#define fSTORE4(EA, SRC) MEM_STORE4(EA, SRC, slot)
#define fSTORE8(EA, SRC) MEM_STORE8(EA, SRC, slot)
#endif

#ifdef QEMU_GENERATE
#define fSTORE_LOCKED(NUM, SIZE, EA, SRC, PRED) \
    gen_store_conditional##SIZE(ctx, PRED, EA, SRC);
#endif

#ifdef QEMU_GENERATE
#define GETBYTE_FUNC(X) \
    __builtin_choose_expr(TYPE_TCGV(X), \
        gen_get_byte, \
        __builtin_choose_expr(TYPE_TCGV_I64(X), \
            gen_get_byte_i64, (void)0))
#define fGETBYTE(N, SRC) GETBYTE_FUNC(SRC)(BYTE, N, SRC, true)
#define fGETUBYTE(N, SRC) GETBYTE_FUNC(SRC)(BYTE, N, SRC, false)
#else
#define fGETBYTE(N, SRC) ((int8_t)((SRC >> ((N) * 8)) & 0xff))
#define fGETUBYTE(N, SRC) ((uint8_t)((SRC >> ((N) * 8)) & 0xff))
#endif

#define fSETBYTE(N, DST, VAL) \
    do { \
        DST = (DST & ~(0x0ffLL << ((N) * 8))) | \
        (((uint64_t)((VAL) & 0x0ffLL)) << ((N) * 8)); \
    } while (0)

#ifdef QEMU_GENERATE
#define fGETHALF(N, SRC)  gen_get_half(HALF, N, SRC, true)
#define fGETUHALF(N, SRC) gen_get_half(HALF, N, SRC, false)
#else
#define fGETHALF(N, SRC) ((int16_t)((SRC >> ((N) * 16)) & 0xffff))
#define fGETUHALF(N, SRC) ((uint16_t)((SRC >> ((N) * 16)) & 0xffff))
#endif
#define fSETHALF(N, DST, VAL) \
    do { \
        DST = (DST & ~(0x0ffffLL << ((N) * 16))) | \
        (((uint64_t)((VAL) & 0x0ffff)) << ((N) * 16)); \
    } while (0)
#define fSETHALFw fSETHALF
#define fSETHALFd fSETHALF

#ifdef QEMU_GENERATE
#define GETWORD_FUNC(X) \
    __builtin_choose_expr(TYPE_TCGV(X), \
        gen_get_word, \
        __builtin_choose_expr(TYPE_TCGV_I64(X), \
            gen_get_word_i64, (void)0))
#define fGETWORD(N, SRC)  GETWORD_FUNC(WORD)(WORD, N, SRC, true)
#define fGETUWORD(N, SRC) GETWORD_FUNC(WORD)(WORD, N, SRC, false)
#else
#define fGETWORD(N, SRC) \
    ((int64_t)((int32_t)((SRC >> ((N) * 32)) & 0x0ffffffffLL)))
#define fGETUWORD(N, SRC) \
    ((uint64_t)((uint32_t)((SRC >> ((N) * 32)) & 0x0ffffffffLL)))
#endif

#define fSETWORD(N, DST, VAL) \
    do { \
        DST = (DST & ~(0x0ffffffffLL << ((N) * 32))) | \
              (((VAL) & 0x0ffffffffLL) << ((N) * 32)); \
    } while (0)

#define fSETBIT(N, DST, VAL) \
    do { \
        DST = (DST & ~(1ULL << (N))) | (((uint64_t)(VAL)) << (N)); \
    } while (0)

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
#define fCL1_8(VAL) clo64(VAL)
#define fCL1_4(VAL) clo32(VAL)
#define fCL1_2(VAL) (clz32(~(uint16_t)(VAL) & 0xffff) - 16)
#define fINTERLEAVE(ODD, EVEN) interleave(ODD, EVEN)
#define fDEINTERLEAVE(MIXED) deinterleave(MIXED)
#define fHIDE(A) A
#define fCONSTLL(A) A##LL
#define fECHO(A) (A)

#define fDO_TRACE(SREG)
#define fBREAK()
#define fTRAP(TRAPTYPE, IMM) helper_raise_exception(env, HEX_EXCP_TRAP0)
#define fPAUSE(IMM)

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
#define fDCFETCH(REG) \
    do { (void)REG; } while (0) /* Nothing to do in qemu */
#define fICINVA(REG) \
    do { (void)REG; } while (0) /* Nothing to do in qemu */
#define fL2FETCH(ADDR, HEIGHT, WIDTH, STRIDE, FLAGS)
#define fDCCLEANA(REG) \
    do { (void)REG; } while (0) /* Nothing to do in qemu */
#define fDCCLEANINVA(REG) \
    do { (void)REG; } while (0) /* Nothing to do in qemu */

#ifdef QEMU_GENERATE
#define fDCZEROA(REG) tcg_gen_mov_tl(hex_dczero_addr, (REG))
#else
#define fDCZEROA(REG) do { env->dczero_addr = (REG); } while (0)
#endif

#define fBRANCH_SPECULATE_STALL(DOTNEWVAL, JUMP_COND, SPEC_DIR, HINTBITNUM, \
                                STRBITNUM) /* Nothing */

#define fVIRTINSN_SPSWAP(IMM, REG)
#define fVIRTINSN_GETIE(IMM, REG) { REG = 0xdeafbeef; }
#define fVIRTINSN_SETIE(IMM, REG)
#define fVIRTINSN_RTE(IMM, REG)
#define fTRAP1_VIRTINSN(IMM) \
    (((IMM) == 1) || ((IMM) == 3) || ((IMM) == 4) || ((IMM) == 6))

#endif
