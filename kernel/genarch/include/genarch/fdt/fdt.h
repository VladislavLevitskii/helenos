/*
 * Copyright (c) 2026 Vladislav Levitskii
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** @addtogroup kernel_genarch
 * @{
 */
/**
 * @file
 */

#ifndef KERN_FDT_H_
#define KERN_FDT_H_

#include <stdint.h>
#include <byteorder.h>
#include <stdint.h>
#include <str.h>

#define FDT_HEADER_MAGIC (0xd00dfeed)

#define BUFFER_STRING_SIZE (64)

#define TOKEN_SIZE (4)

#define BE2LE(value) (uint32_t_byteorder_swap(value))


typedef enum {
    FDT_BEGIN_NODE = 0x00000001,
    FDT_END_NODE = 0x00000002,
    FDT_PROP = 0x00000003,
    FDT_NOP = 0x00000004,
    FDT_END = 0x00000009
} TOKEN_TYPE;

typedef struct {
    uint32_t len;
    uint32_t nameoff;
} fdt_property;

// All the fields are Big Endian
typedef struct {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
} fdt_header;

extern fdt_header* fdt_pointer;

errno_t dump_fdt(void);
errno_t process_fdt(void);

#endif

/** @}
 */
