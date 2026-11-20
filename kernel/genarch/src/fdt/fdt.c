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

#include <str.h>
#include <genarch/fdt/fdt.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <align.h>
#include <mm/km.h>

typedef enum {
    FDT_BEGIN_NODE = 0x00000001,
    FDT_END_NODE = 0x00000002,
    FDT_PROP = 0x00000003,
    FDT_NOP = 0x00000004,
    FDT_END = 0x00000009
} TOKEN_TYPE;

// this pointer is set twice,
// first in main after reading the bootinfo,
// second in mapping to virtual address
fdt_header_t* fdt_pointer = NULL;

/**
 * @brief Prints misaligned string byte by byte
 * 
 * Copies string to buffer with size: BUFFER_STRING_SIZE.
 * If the size of the string is bigger then str_cpy 
 * cuts it and terminates with \0
 * 
 * @param string String to print
 */
static void print_misaligned_string(const char* string) {
    char safe_name[BUFFER_STRING_SIZE];
    str_cpy(safe_name, BUFFER_STRING_SIZE, string);
    printf("%s", safe_name);
}

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
 * Allow \0 so we can print <string list> too
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

/**
 * @brief Prints value of the property
 *
 * Heurestically guesses if the value if flag, string (string list) or just raw data.
 * 
 * @param address Pointer to the value of the property
 * @param len Length of the property value
 */
static void print_property_value(const uint8_t* data, int len) {
    // flag
    if (len == 0) {
        printf("= (flag);\n");
        return;
    }
    
    // string or string list
    if (is_printable_ascii_string((char*)data, len)) {
        printf("= \"");
        
        // here we can't use print_misaligned_string because we need to transform '\0' to ','
        for (int i = 0; i < len - 1; ++i) {
            char c = data[i];
            
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
    printf("= RAW DATA: ");
    for (int i = 0; i < len; ++i) {
        printf("%02X ", data[i]);
    }
    printf(";\n");

    return;
}

/**
 * @brief Creates printable string and copies it to the buffer.
 * 
 * @param path Array of the folders
 * @param depth Depth of the folders
 * @param out_buffer Buffer to copy
 * @param buffer_size Buffer size
 * @return errno_t 
 * EOK if copying was successful, 
 * EOVERFLOW if copying was successful but buffer was to small to copy everything (still printable string)
 * EINVAL if the function was called with invalid parameters (practically unreachable)
 */
static errno_t copy_path(const char** path, int depth, char* out_buffer, int buffer_size) {
	// check if we have place for '/\0'
	if (!out_buffer || !path || buffer_size < 2) {
		return EINVAL;
	}

	// for checking if we copied everything we could
	int totalsize = 0;

	int buffer_size_local = buffer_size;

	// root directory
	*out_buffer++ = '/';
	totalsize++;
	buffer_size_local--;

	for (int node_idx = 0; node_idx <= depth; ++node_idx) {
		const char* current_folder = path[node_idx];

		totalsize += str_size(current_folder);

		str_cpy(out_buffer, buffer_size_local, current_folder);
		
		size_t copied_len = str_size(out_buffer);
		// we don't add '\0' as a byte because we will overwrite it with '/'
		out_buffer += copied_len;
		buffer_size_local -= copied_len;

		// if it is not last folder and it is not first one (because it always has '')
		if (node_idx < depth && current_folder[0] != '\0') {
			totalsize++;
			if (buffer_size_local - 1 > 0) {
				*out_buffer++ = '/';
				buffer_size_local--;
			}
		}
	}

	*(out_buffer) = '\0';
	totalsize++;

	if (totalsize > buffer_size) {
		return EOVERFLOW;
	}

	return EOK;
}

const char* get_name_prop(const fdt_prop_t* property) {
	if (!fdt_pointer || !property) {
		return NULL;
	}

	uintptr_t strings_addr = (uintptr_t)fdt_pointer + BE2LE(fdt_pointer->off_dt_strings);
	uint32_t name_offset = BE2LE(property->nameoff);

	return (const char*)(strings_addr + name_offset);
}

errno_t fdt_init(void) {
	if (!fdt_pointer) {
		return EINVAL;
	}
	
	const unsigned int fdt_flags = PAGE_READ | PAGE_KERNEL;

    // the first mapping
	fdt_header_t* fdt_header_pointer = (fdt_header_t*)km_map((uintptr_t)fdt_pointer, sizeof(fdt_header_t), KM_NATURAL_ALIGNMENT, fdt_flags);
	const uint32_t magic = BE2LE(fdt_header_pointer->magic);

	if (magic != FDT_HEADER_MAGIC) {
		return EINVAL;
	}

	const uint32_t fdt_size = BE2LE(fdt_header_pointer->totalsize);

    // here is the second mapping
	fdt_pointer = (fdt_header_t*)km_map((uintptr_t)fdt_pointer, fdt_size, KM_NATURAL_ALIGNMENT, fdt_flags);
	return EOK;
}

errno_t fdt_dump(void) {
	if (!fdt_pointer) {
		printf("FDT pointer is not initialized!\n");
		return EINVAL;
	}

	printf("Starting dumping...\n");

	const char* path[MAX_DEPTH];

	uintptr_t current_address = (uintptr_t)fdt_pointer + BE2LE(fdt_pointer->off_dt_struct);

	int depth = 0;

	while (true) {
		TOKEN_TYPE token = BE2LE(*(uint32_t*)current_address);
		switch (token) {
			case FDT_BEGIN_NODE:
                {
					if (depth >= MAX_DEPTH) {
                        printf("FDT too deep!\n");
                        return EINVAL;
                    }

					const char* raw_unit_name = (char*)(current_address + TOKEN_SIZE);
					path[depth] = raw_unit_name;
					
					for (int i = 0; i < depth; ++i) { printf("\t"); }
					char path_name[BUFFER_STRING_SIZE];
					errno_t err = copy_path(path, depth, path_name, BUFFER_STRING_SIZE);
					if (err == EINVAL) {
						// should be unreachable
						printf("Some problems with parameters while copying the path!\n");
						return EINVAL;
					}
					
					printf("%s {\n", path_name);
					
					current_address += TOKEN_SIZE;
					current_address += str_size(raw_unit_name) + 1;
					current_address = ALIGN_UP(current_address, TOKEN_SIZE);

					depth++;

					break;
				}
			case FDT_END_NODE:
				{
					if (depth <= 0) {
						return EINVAL;
					}

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
					fdt_prop_t* current_property = (fdt_prop_t*)current_address;

					const char* string_name = get_name_prop(current_property);

					for (int j = 0; j < depth; ++j) printf("\t");
                    print_misaligned_string(string_name);
					printf(":\n");

					uint32_t len = BE2LE(current_property->len);

                    for (int j = 0; j < depth + 1; ++j) printf("\t");
                    print_property_value(current_property->data, len);
					
					current_address += sizeof(fdt_prop_t);
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

errno_t fdt_find_node(const char* path, uintptr_t* out_node) {
	if (!path || !out_node || !fdt_pointer) {
		return EINVAL;
	}

	if (str_size(path) >= BUFFER_STRING_SIZE) {
		return EOVERFLOW;
	}

	const char* current_path[MAX_DEPTH];
	char current_path_buffer[BUFFER_STRING_SIZE];

	uintptr_t current_address = (uintptr_t)fdt_pointer + BE2LE(fdt_pointer->off_dt_struct);

	int depth = 0;

	while (true) {
		TOKEN_TYPE token = BE2LE(*(uint32_t*)current_address);
		switch (token) {
			case FDT_BEGIN_NODE:
                {
					if (depth >= MAX_DEPTH) {
                        return EINVAL;
                    }

					const char* raw_unit_name = (char*)(current_address + TOKEN_SIZE);
					current_path[depth] = raw_unit_name;

					errno_t err = copy_path(current_path, depth, current_path_buffer, BUFFER_STRING_SIZE);

					if (err == EOK) {
						if (!str_cmp(path, current_path_buffer)) {
							*out_node = current_address;
							return EOK;
						}
					} else if (err != EOVERFLOW) {
						// the idea behind that is we might step across
						// long path that we are not trying to find
						// so we don't want to return early
						return err;
					}

					current_address += TOKEN_SIZE;
					current_address += str_size(raw_unit_name) + 1;
					current_address = ALIGN_UP(current_address, TOKEN_SIZE);

					depth++;

					break;
				}
			case FDT_END_NODE:
				{
					if (depth <= 0) {
						return EINVAL;
					}

					current_address += TOKEN_SIZE;
					depth--;
					current_path[depth] = NULL;
					break;
				}
			case FDT_PROP:
				{
					current_address += TOKEN_SIZE;
					fdt_prop_t* current_property = (fdt_prop_t*)current_address;
					uint32_t len = BE2LE(current_property->len);
					
					current_address += sizeof(fdt_prop_t);
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
                	return ENOENT; 
				}
            default:
				{
					// should be unreachable if device tree is valid
					return EINVAL;
				}
		}

	}

	// should be unreachable if device tree is valid
	return EINVAL;
}

const fdt_prop_t* fdt_get_prop(uintptr_t node, const char* prop_name) {
	if (!node || !prop_name || !fdt_pointer) {
		return NULL;
	}

	if (BE2LE(*(uint32_t*)node) != FDT_BEGIN_NODE) {
		return NULL;
	}

	node += TOKEN_SIZE;
	node += str_size((char*)node) + 1;
	node = ALIGN_UP(node, TOKEN_SIZE);

	while (true) {
		TOKEN_TYPE token = BE2LE(*(uint32_t*)node);
		switch (token) {
			case FDT_NOP:
				{
					node += TOKEN_SIZE;
					break;
				}
			case FDT_PROP:
				{
					node += TOKEN_SIZE;
					const fdt_prop_t* current_property = (fdt_prop_t*)node;

					const char* string_name = get_name_prop(current_property);

					if (!str_cmp(string_name, prop_name)) {
						return current_property;
					}

					uint32_t len = BE2LE(current_property->len);
					node += sizeof(fdt_prop_t);
					node += len;
					node = ALIGN_UP(node, TOKEN_SIZE);
					break;
				}
			case FDT_END:
			case FDT_BEGIN_NODE:
			case FDT_END_NODE:
			default:
				return NULL;
		}
	}
}

/** @}
 */
