/*!
 * Copyright (c) Alibaba, Inc. and its affiliates.
 * @file    gemm_microkernel_macro_m2.h
 */

#pragma once

#ifdef ENABLE_ARM_V84_V9
#include <arm_sve.h>

#include "activation_macro.h"

// clang-format off
/***********************/

#define ASM_BLOCK_PREFETCH_PART_0                              \
      "prfw    pldl2keep, p0, [%[scale_p],    #0, MUL VL]  \n" \
      "prfw    pldl2keep, p0, [%[scaleXzp_p], #0, MUL VL]  \n" \
                                                               \
      "prfw    pldl2keep, p0, [%[a_bf16_ptr], #0, MUL VL]  \n" \
                                                               \
      "prfw    pldl2keep, p0, [%[b_u8_ptr1], #0, MUL VL]   \n" \
      "prfw    pldl2keep, p0, [%[b_u8_ptr2], #0, MUL VL]   \n"

#define ASM_BLOCK_PREFETCH_PART_1                      \
      "add     x7,    x7,    #1                    \n" \
      "cmp     x7,    #2                           \n" \
      "b.any   " LABEL_SKIP_PRF_1 "f               \n" \
      "add     x8,    %[a_bf16_ptr],   #64         \n" \
      "prfw    pldl2keep, p0, [x8,     #0, MUL VL] \n" \
      "add     x8,    %[b_u8_ptr1],    #96         \n" \
      "prfw    pldl2keep, p0, [x8,     #0, MUL VL] \n" \
      "add     x8,    %[b_u8_ptr2],    #96         \n" \
      "prfw    pldl2keep, p0, [x8,     #0, MUL VL] \n" \
      "mov     x7,    #0                           \n" \
      " " LABEL_SKIP_PRF_1 ":                      \n"

/***********************/

#define ASM_BLOCK_ADD_BIAS                               \
    asm volatile(                                        \
        "ld1w    z4.s, p1/z, [%[bias_p], #0, MUL VL] \n" \
        "fadd    z0.s, z0.s, z4.s             \n"        \
        "fadd    z1.s, z1.s, z4.s             \n"        \
        : /* empty OutputOperands */                     \
        : [bias_p] "r"(bias_ptr)                         \
        : "p1", "z0", "z1", "z4",                        \
          "cc", "memory");

/***********************/

#define ASM_BLOCK_C_STORE                                                \
    asm volatile(                                                        \
        "mov     x9,   %[c_fp32_ptr]                \n"                  \
        "st1w    z0.s, p1,   [x9, #0, MUL VL]       \n"                  \
                                                                         \
        "add     x9,   x9,   %[next_line_offset]    \n"                  \
        "st1w    z1.s, p1,   [x9, #0, MUL VL]       \n"                  \
        : /* empty OutputOperands */                                     \
        : [c_fp32_ptr] "r"(c_fp32_ptr),                                  \
          [next_line_offset] "r"(next_line_offset),                      \
          [M] "r"(M)                                                     \
        : "p1", "x9", "z0", "z1",                                        \
          "cc", "memory");

#define ASM_BLOCK_C_ACCUMULATE                                           \
    asm volatile(                                                        \
        "mov     x9,    %[c_fp32_ptr]                \n"                 \
        "ld1w    z10.s, p1/z, [x9, #0, MUL VL]       \n"                 \
        "fadd    z0.s,  z0.s, z10.s                  \n"                 \
                                                                         \
        "add     x9,    x9,   %[next_line_offset]    \n"                 \
        "ld1w    z11.s, p1/z, [x9, #0, MUL VL]       \n"                 \
        "fadd    z1.s,  z1.s, z11.s                  \n"                 \
        : /* empty OutputOperands */                                     \
        : [c_fp32_ptr] "r"(c_fp32_ptr),                                  \
          [next_line_offset] "r"(next_line_offset),                      \
          [M] "r"(M)                                                     \
        : "p1", "x9", "z0", "z1", "z10", "z11",                          \
          "cc", "memory");

#define ASM_BLOCK_C_RES_STORE                                            \
    asm volatile(                                                        \
        "mov     x9,   %[c_fp32_ptr]                \n"                  \
        "st1w    z0.s, p1,   [x9, #0, MUL VL]       \n"                  \
                                                                         \
        /* if (m + 1) > M, go to label (skip store) */                   \
        "add     x5,   x2, #1                       \n"                  \
        "cmp     x5,   %[M]                         \n"                  \
        "b.tcont " LABEL_SKIP_STORE "f              \n"                  \
        "add     x9,   x9,   %[next_line_offset]    \n"                  \
        "st1w    z1.s, p1,   [x9, #0, MUL VL]       \n"                  \
                                                                         \
        " " LABEL_SKIP_STORE ":\n"                                       \
        : /* empty OutputOperands */                                     \
        : [c_fp32_ptr] "r"(c_fp32_ptr),                                  \
          [next_line_offset] "r"(next_line_offset),                      \
          [M] "r"(M)                                                     \
        : "p1", "x2", "x5", "x9", "z0", "z1",                            \
          "cc", "memory");

#define ASM_BLOCK_C_RES_ACCUMULATE                                       \
    asm volatile(                                                        \
        "mov     x9,    %[c_fp32_ptr]                \n"                 \
        "ld1w    z10.s, p1/z, [x9, #0, MUL VL]       \n"                 \
        "fadd    z0.s,  z0.s, z10.s                  \n"                 \
                                                                         \
        /* if (m + 1) > M, go to label (skip store) */                   \
        "add     x5,    x2, #1                       \n"                 \
        "cmp     x5,    %[M]                         \n"                 \
        "b.tcont " LABEL_SKIP_ACCUMULATE "f          \n"                 \
        "add     x9,    x9,   %[next_line_offset]    \n"                 \
        "ld1w    z11.s, p1/z, [x9, #0, MUL VL]       \n"                 \
        "fadd    z1.s,  z1.s, z11.s                  \n"                 \
                                                                         \
        " " LABEL_SKIP_ACCUMULATE ":\n"                                  \
        : /* empty OutputOperands */                                     \
        : [c_fp32_ptr] "r"(c_fp32_ptr),                                  \
          [next_line_offset] "r"(next_line_offset),                      \
          [M] "r"(M)                                                     \
        : "p1", "x2", "x5", "x9", "z0", "z1", "z10", "z11",              \
          "cc", "memory");

/***********************/

#define ASM_BLOCK_ACTIVE_RELU              \
    asm volatile(                          \
      "fmax    z0.s, p0/m, z0.s, #0.0 \n"  \
      "fmax    z1.s, p0/m, z1.s, #0.0 \n"  \
      : /* empty OutputOperands */         \
      : /* empty InputOperands */          \
      : "p0", "z0", "z1",                  \
        "cc", "memory");

#define ASM_BLOCK_ACTIVE_SILU                              \
    asm volatile(                                          \
      "ld1w    z5.s,  p0/z, [%[exp_const], #0, MUL VL] \n" \
      "ld1w    z6.s,  p0/z, [%[exp_const], #1, MUL VL] \n" \
      "ld1w    z7.s,  p0/z, [%[exp_const], #2, MUL VL] \n" \
                                                           \
      ASM_BLOCK_SILU_MICRO(z0, z0, z5, z6, z7,             \
                           z2, z3, z4, z8, z9,             \
                           p0, p2)                         \
                                                           \
      ASM_BLOCK_SILU_MICRO(z1, z1, z5, z6, z7,             \
                           z2, z3, z4, z8, z9,             \
                           p0, p2)                         \
                                                           \
      : /* empty OutputOperands */                         \
      : [exp_const] "r"(constant.exp_const)                \
      : "p0", "p2",                                        \
        "z0", "z1", "z2", "z3", "z4", "z5", "z6", "z7",    \
        "z8", "z9",                                        \
        "cc", "memory");

#define ASM_BLOCK_ACTIVE_TANH                              \
    asm volatile(                                          \
      "ld1w    z5.s,  p0/z, [%[exp_const], #0, MUL VL] \n" \
      "ld1w    z6.s,  p0/z, [%[exp_const], #1, MUL VL] \n" \
      "ld1w    z7.s,  p0/z, [%[exp_const], #2, MUL VL] \n" \
                                                           \
      "mov     z8.s, p0/m, z0.s                        \n" \
      ASM_BLOCK_TANH(z8, z0, z5, z6, z7,                   \
                     z2, z3, z4, p0, p2)                   \
                                                           \
      "mov     z8.s, p0/m, z1.s                        \n" \
      ASM_BLOCK_TANH(z8, z1, z5, z6, z7,                   \
                     z2, z3, z4, p0, p2)                   \
                                                           \
      : /* empty OutputOperands */                         \
      : [exp_const] "r"(constant.exp_const)                \
      : "p0", "p2",                                        \
        "z0", "z1", "z2", "z3", "z4", "z5", "z6", "z7", "z8", \
        "cc", "memory");

#define ASM_BLOCK_ACTIVE_GELU_ERF                              \
    asm volatile(                                              \
      "ld1w    z4.s,  p0/z, [%[exp_const], #0, MUL VL] \n"     \
      "ld1w    z5.s,  p0/z, [%[exp_const], #1, MUL VL] \n"     \
      "ld1w    z6.s,  p0/z, [%[exp_const], #2, MUL VL] \n"     \
                                                               \
      "ld1w    z7.s,  p0/z, [%[erf_const], #0, MUL VL] \n"     \
      "ld1w    z8.s,  p0/z, [%[erf_const], #1, MUL VL] \n"     \
                                                               \
      ASM_BLOCK_GELU_ERF_MICRO(z0, z0,                         \
                               z4, z5, z6, z7, z8,             \
                               z2, z3, z9, z10, z11, z12, z13, \
                               p0, p2)                         \
                                                               \
      ASM_BLOCK_GELU_ERF_MICRO(z1, z1,                         \
                               z4, z5, z6, z7, z8,             \
                               z2, z3, z9, z10, z11, z12, z13, \
                               p0, p2)                         \
                                                               \
      : /* empty OutputOperands */                             \
      : [exp_const] "r"(constant.exp_const),                   \
        [erf_const] "r"(constant.erf_const),                   \
        [inv_sqrt] "r"(constant.inv_sqrt)                      \
      : "p0", "p2", "p3",                                      \
        "z0", "z1", "z2", "z3", "z4", "z5", "z6", "z7",        \
        "z8", "z9", "z10", "z11", "z12", "z13",                \
        "cc", "memory");

#define ASM_BLOCK_ACTIVE_GELU_TANH                         \
    asm volatile(                                          \
      "ld1w    z5.s,  p0/z, [%[exp_const], #0, MUL VL] \n" \
      "ld1w    z6.s,  p0/z, [%[exp_const], #1, MUL VL] \n" \
      "ld1w    z7.s,  p0/z, [%[exp_const], #2, MUL VL] \n" \
                                                           \
      ASM_BLOCK_GELU_TANH_MICRO(z0, z0,                    \
                                z5, z6, z7,                \
                                z2, z3, z4, z8, z9,        \
                                p0, p2)                    \
                                                           \
      ASM_BLOCK_GELU_TANH_MICRO(z1, z1,                    \
                                z5, z6, z7,                \
                                z2, z3, z4, z8, z9,        \
                                p0, p2)                    \
                                                           \
      : /* empty OutputOperands */                         \
      : [exp_const] "r"(constant.exp_const),               \
        [erf_const] "r"(constant.erf_const),               \
        [const1] "r"(constant.gelu_tanh_const[0]),         \
        [const2] "r"(constant.gelu_tanh_const[1])          \
      : "p0", "p2",                                                 \
        "z0", "z1", "z2", "z3", "z4", "z5", "z6", "z7", "z8", "z9", \
        "cc", "memory");
// clang-format on
#endif
