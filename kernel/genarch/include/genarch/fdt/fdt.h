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

#define BUFFER_STRING_SIZE (256)
#define MAX_DEPTH (32)
#define TOKEN_SIZE (4)

#define BE2LE(value) (uint32_t_byteorder_swap(value))

// All of the fields are Big Endian
typedef struct {
    uint32_t len;
    uint32_t nameoff;
    uint8_t data[];
} fdt_prop_t;

// All of the fields are Big Endian
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
} fdt_header_t;

extern fdt_header_t* fdt_pointer;

/**
 * @brief Initialize the FDT parser and map the device tree blob into kernel memory.
 *
 * Validates the FDT magic number and remaps FDT structure based on its totalsize field.
 *
 * @return EOK on success.
 * @return EINVAL if fdt_pointer is NULL or the FDT header magic is invalid.
 */
errno_t fdt_init(void);

/**
 * @brief Dump the entire FDT to the console in a readable format.
 *
 * @return EOK on success.
 * @return EINVAL if the FDT is not initialized, malformed, or exceeds MAX_DEPTH.
 */
errno_t fdt_dump(void);

/**
 * @brief Find node by a provided path
 * 
 * @param path Example paths: "/" (root), "/soc/gpio@7e200000/spi0-gpio35"
 * @param out_node Copies the pointer to
 * @return errno_t 
 * EINVAL if fdt_pointer is uninitialized or tree is invalid
 * EOVERFLOW if the path is too long (than BUFFER_STRING_SIZE)
 * EOK if successful
 * ENOENT if there is no such node
 */
errno_t fdt_find_node(const char* path, uintptr_t* out_node);

/**
 * @brief Get pointer on property section
 * 
 * @param node Pointer to the node structure
 * @param prop_name Property name to find
 * @return const fdt_prop_t* property
 */
const fdt_prop_t* fdt_get_prop(uintptr_t node, const char* prop_name);

/**
 * @brief Get the name of the property
 * 
 * For printing it is necessary to copy that string 
 * using std_cpy to some buffer.
 *
 * @param property Property
 * @return const char* In most cases misaligned pointer
 */
const char* get_name_prop(const fdt_prop_t* property);

#endif

/** @}
 */
