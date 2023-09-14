/*
 *  Copyright(c) 2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
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
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <hexagon_standalone.h>
#include "vtcm_common.h"
#include "cfgtable.h"

#define INIT_VAL 0x11111111

static int err;
static uint32_t l2line_size;

uint32_t round_up(uint32_t num, uint32_t alignment)
{
    uint32_t remainder = num % alignment;
    return remainder ? num + alignment - remainder : num;
}

static void test_memcpy(uint32_t *dst, uint32_t *src, uint32_t src_bytes,
                        uint32_t cp_bytes)
{
    uint32_t real_cp_bytes = round_up(cp_bytes + 1, l2line_size);
    fprintf(stderr, "Testing size %"PRIu32"\n", cp_bytes);

    assert(src_bytes % sizeof(*src) == 0);
    assert(real_cp_bytes % sizeof(*src) == 0);
    uint32_t src_size = src_bytes / sizeof(*src);
    uint32_t real_cp_size = real_cp_bytes / sizeof(*src);

    for (int i = 0; i < src_size; i++) {
        src[i] = i;
        dst[i] = INIT_VAL;
    }

    asm volatile(
        "m0 = %2\n"
        "memcpy(%0, %1, m0)\n"
        : : "r"(dst), "r"(src), "r"(cp_bytes)
        : "m0", "memory"
    );

    for (int i = 0; i < src_size; i++) {
        uint32_t expect = i < real_cp_size ? src[i] : INIT_VAL;
        if (dst[i] != expect) {
            fprintf(stderr, "ERROR: at %d got %"PRIu32", exp %"PRIu32"\n",
                    i, dst[i], expect);
            err |= 1;
        }
    }
}

int main()
{
    uint32_t src_bytes;
    uint32_t *src, *dst = setup_default_vtcm();

    l2line_size = read_cfgtable_field(0x50); /* L2LINE_SZ */
    src_bytes = l2line_size * 3;
    if (posix_memalign((void **)&src, l2line_size, src_bytes)) {
        fprintf(stderr, "posix_memalign failed\n");
        return 1;
    }

    /*
     * Test some key sizes. See vtcm_memcpy implementation note at
     * target/hexagon/genptr.c for more details.
     */
    test_memcpy(dst, src, src_bytes, 0);
    test_memcpy(dst, src, src_bytes, l2line_size - 1);
    test_memcpy(dst, src, src_bytes, l2line_size);
    test_memcpy(dst, src, src_bytes, 2 * l2line_size - 1);
    test_memcpy(dst, src, src_bytes, 2 * l2line_size);

    free(src);
    puts(err ? "FAIL\n" : "PASS\n");
    return err;
}
