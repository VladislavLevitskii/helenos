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
 * @brief Parsing Flattened Device Tree
 */

#include <genarch/fdt/fdt.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <align.h>
#include <mm/km.h>

// this pointer is set twice,
// first in main after reading the bootinfo,
// second in mapping to virtual address
fdt_header* fdt_pointer = NULL;


static inline bool is_printable_ascii(char c) {
	if (c < 0x20 || c > 0x7E) {
		return false;
	}

	return true;
}

/**
 * @brief Checks if the given data is a printable ASCII string.
 *
 * In case len == 0, the string is not valid.
 *
 * Allow \0 so we can print <string list> to
 *
 * @param string String to check.
 * @param len Length of the potential string in bytes.
 * @return true If the string has the length of 'len', every byte from index 0 
 * to len - 2 is a valid printable ASCII character (from 0x20 to 0x7E and 0x00), 
 * and the byte at index len - 1 is '\0'.
 * @return false Otherwise.
 */
static bool is_printable_ascii_string(const char* string, size_t len) {
	if (len == 0) {
		return false;
	}

	for (size_t i = 0; i < len - 1; ++i) {
        const char c = string[i];
		if (!is_printable_ascii(c) && c != '\0') {
			return false;
		}
	}

	if (string[len - 1] != '\0') {
		return false;
	}

	return true;
}


static void print_property_value(uintptr_t address, int len) {
    // flag
    if (len == 0) {
        printf("= (flag);\n");
        return;
    }
    
    // string or string list
    if (is_printable_ascii_string((char*)address, len)) {
        printf("= \"");
        
        for (int i = 0; i < len - 1; ++i) {
            char c = ((char*)address)[i];
            
            if (c == '\0') {
                printf("\", \"");
            } else {
                printf("%c", c);
            }
        }
        
        printf("\";\n");
        return;
    }

    // raw data
    printf("= < ");
    uint8_t* raw_data = (uint8_t*)address;
    for (int i = 0; i < len; ++i) {
        printf("%02X ", raw_data[i]);
    }
    printf(">;\n");

    return;
}

static void print_path(const char** path, int depth) {
    for (int i = 0; i <= depth; ++i) {
        if (path[i][0] != '\0') { 
			// here we need to create safe string because pointer 
			// in path array can point to misaligned memory
            char safe_string[BUFFER_STRING_SIZE];
			str_cpy(safe_string, BUFFER_STRING_SIZE, path[i]);
            printf("/%s", safe_string);
        } else if (depth == 0) {
			printf("/");
		}
    }
}

/**
 * @brief Initialize fdt by mapping it to a virtual address 
 * and checking magic number.
 * 
 * @return errno_t
 */
errno_t process_fdt(void) {
	const unsigned int fdt_flags = PAGE_READ | PAGE_KERNEL;

    if (fdt_pointer == NULL) {
        printf("Invalid FDT pointer from bootinfo!\n");
        return EINVAL;
    }

    // the first mapping
	fdt_header* fdt_header_pointer = (fdt_header*)km_map((uintptr_t)fdt_pointer, sizeof(fdt_header), KM_NATURAL_ALIGNMENT, fdt_flags);
	const uint32_t magic = BE2LE(fdt_header_pointer->magic);

	if (magic != FDT_HEADER_MAGIC) {
		printf("Invalid magic of the header: %x\n", magic);
		return EINVAL;
	}

	const uint32_t fdt_size = BE2LE(fdt_header_pointer->totalsize);

    // here is the second mapping
	fdt_pointer = (fdt_header*)km_map((uintptr_t)fdt_pointer, fdt_size, KM_NATURAL_ALIGNMENT, fdt_flags);
	return EOK;
}

/**
 * @brief Dumps whole FDT
 * 
 * @return errno_t 
 */
errno_t dump_fdt(void) {
	printf("Starting dumping...\n");

	const char* path[32];

	uintptr_t current_address = (uintptr_t)fdt_pointer + BE2LE(fdt_pointer->off_dt_struct);

	int depth = 0;

	while (true) {
		TOKEN_TYPE token = BE2LE(*(uint32_t*)current_address);
		switch (token) {
			case FDT_BEGIN_NODE:
                {
					// we shouldn't create save buffer for string because it should be aligned
					const char* raw_unit_name = (char*)(current_address + TOKEN_SIZE);
					path[depth] = raw_unit_name;
					
					for (int i = 0; i < depth; ++i) { printf("\t"); }
					print_path(path, depth);
					printf(" {\n");
					
					current_address += TOKEN_SIZE;
					current_address += str_size(raw_unit_name) + 1;
					current_address = ALIGN_UP(current_address, TOKEN_SIZE);

					depth++;
					if (depth >= 32) {
                        printf("FDT too deep!\n");
                        return EINVAL;
                    }

					break;
				}
			case FDT_END_NODE:
				{
					current_address += TOKEN_SIZE;
					depth--;
					path[depth] = NULL;
					for (int i = 0; i < depth; ++i) { printf("\t"); }
					printf("};\n");
					break;
				}
			case FDT_PROP:
				{	
					current_address += TOKEN_SIZE;
					fdt_property* current_property = (fdt_property*)current_address;

					uintptr_t strings_addr = (uintptr_t)fdt_pointer + BE2LE(fdt_pointer->off_dt_strings);
					uint32_t name_offset = BE2LE(current_property->nameoff);

					char* string_name = (char*)(strings_addr + name_offset);
					char safe_name[BUFFER_STRING_SIZE];
					str_cpy(safe_name, BUFFER_STRING_SIZE, string_name);
                    
					// we will print the name
					for (int j = 0; j < depth; ++j) printf("\t");
					printf("'%s'\n", safe_name);

					uint32_t len = BE2LE(current_property->len);

					// skip header
					current_address += sizeof(fdt_property);

                    for (int j = 0; j < depth + 1; ++j) printf("\t");
                    print_property_value(current_address, len);

                    // skip value
					current_address += len;
					current_address = ALIGN_UP(current_address, TOKEN_SIZE);
				}
				break;
			case FDT_NOP:
				{
					current_address += TOKEN_SIZE;
                    continue;
				}
			case FDT_END: 
                {
					printf("Found FDT_END. End of tree.\n");
                	return EOK; 
				}
            default:
				{
					printf("Unknown token: %x\n", token);
					return EINVAL;
				}
		}

	}

	printf("FDT_END wasn't found.");
	return EINVAL;
}

/** @}
 */
