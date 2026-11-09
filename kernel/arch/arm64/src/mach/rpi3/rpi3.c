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

/** @addtogroup kernel_arm64_rpi364a
 * @{
 */
/** @file
 * @brief Raspberry Pi platform driver.
 */

#include "genarch/acpi/acpi.h"
#include <arch/mach/rpi3/rpi3.h>
#include <genarch/drivers/pl011/pl011.h>
#include <genarch/drivers/bcm2835/irc.h>
#include <genarch/drivers/bcm2835/timer.h>
#include <genarch/drivers/bcm2835/mbox.h>
#include <mm/km.h>
#include <stdint.h>
#include <stdio.h>

#define BIT(n) (1U << (n))

#define AUX_MU_BASE_REG   0x7E215000 
#define AUX_MU_IO_OFFSET  0x40 
#define AUX_MU_LSR_OFFSET 0x54

#define VIDEO_ADR2CPU_ADR(n) ((n) - 0x3F000000)

static uintptr_t rpi3_miniuart_virt_base = 0;

// static struct {
// 	pl011_uart_t	uart;
// } raspi;

// we need this function because ARM doesn't support misaligned access via 64b pointer from xsdt
static uint64_t read_unaligned_64(const void *ptr) {
    const volatile uint8_t *b = (const volatile uint8_t *)ptr;
    
    uint64_t val = 0;
    val |= ((uint64_t)b[0]);
    val |= ((uint64_t)b[1] << 8);
    val |= ((uint64_t)b[2] << 16);
    val |= ((uint64_t)b[3] << 24);
    val |= ((uint64_t)b[4] << 32);
    val |= ((uint64_t)b[5] << 40);
    val |= ((uint64_t)b[6] << 48);
    val |= ((uint64_t)b[7] << 56);
    
    return val;
}

static bool header_check(void* ptr, int length_bytes)
{
    uint8_t sum = 0;
    uint8_t *bytes = (uint8_t *) ptr;

    for (int i = 0; i < length_bytes; i++) {
        sum += bytes[i];
    }

    return sum == 0;
}

static void initialize_acpi_tables(void) {
    const unsigned int acpi_flags = PAGE_NOT_CACHEABLE | PAGE_READ | PAGE_KERNEL;

    // mapping xsdp
    uintptr_t acpi_xsdp_addr = km_map((uintptr_t)acpi_rsdp, sizeof(struct acpi_rsdp), KM_NATURAL_ALIGNMENT, acpi_flags);
    struct acpi_rsdp* acpi_xsdp = (struct acpi_rsdp*)acpi_xsdp_addr;

    if (acpi_xsdp->revision < 2) {
        // since we use UEFI that provides us 64b version this check is optional 
        printf("RSDP is not XSDP (not 64b)\n");
        return;
    }

    bool rsdp_valid = header_check(acpi_xsdp, sizeof(struct acpi_rsdp));
    if (!rsdp_valid) {
        printf("RSDP is not valid!\n");
        return;
    }

    // now we map only header of the xsdt since we don't know how big it is
    // later we will remap it
    uintptr_t acpi_xsdt_addr = km_map((uintptr_t)acpi_xsdp->xsdt_address, sizeof(struct acpi_sdt_header), KM_NATURAL_ALIGNMENT, acpi_flags);
    struct acpi_xsdt* acpi_xsdt = (struct acpi_xsdt*)acpi_xsdt_addr;

    int table_length = acpi_xsdt->header.length;
    
    acpi_xsdt_addr = km_map((uintptr_t)acpi_xsdp->xsdt_address, table_length, KM_NATURAL_ALIGNMENT, acpi_flags);
    acpi_xsdt = (struct acpi_xsdt*)acpi_xsdt_addr;
    
    bool xsdt_valid = header_check((void*)acpi_xsdt_addr, acpi_xsdt->header.length);
    if (!xsdt_valid) {
        printf("XSDT is not valid!\n"); // Přidáno \n
        return;
    }
    
    // xsdt is valid and now we can iterate through the entries
    int entries_number = (acpi_xsdt->header.length - sizeof(struct acpi_sdt_header)) / sizeof(uint64_t);

    for (int i = 0; i < entries_number; ++i) {
        uint64_t phys_addr = read_unaligned_64((const void*)acpi_xsdt->entry[i]);

        uintptr_t entry_header_addr = km_map((uintptr_t)phys_addr, sizeof(struct acpi_sdt_header), KM_NATURAL_ALIGNMENT, acpi_flags);
        struct acpi_sdt_header* entry_header = (struct acpi_sdt_header*)entry_header_addr;

        printf("Entry signature %u: ", i);
        for (int j = 0; j < 4; ++j) {
            printf("%c", (char)entry_header->signature[j]);
        }
        printf("\n");
    }
    
}

/**
* Initialize the RPi 3 platform.
*
* Unfortunately mappings of the ACPI table should be done here due to
* compilation errors in generic acpi.c
*
*/
static void rpi3_init(void)
{	
	const unsigned int uart_flags = PAGE_NOT_CACHEABLE | PAGE_READ | PAGE_WRITE | PAGE_KERNEL;

	// printf works only after setting the miniUART virtual base
    rpi3_miniuart_virt_base = km_map(VIDEO_ADR2CPU_ADR(AUX_MU_BASE_REG), PAGE_SIZE, KM_NATURAL_ALIGNMENT, uart_flags);
	initialize_acpi_tables();
}

/** Handle interrupts for the RPi3 platform. */
static void rpi3_irq_exception(unsigned int exc_no, istate_t *istate)
{
	
}

/** Initialize the output device for the RPi 3 platform. */
static void rpi3_output_init(void)
{
	// if (pl011_uart_init(&raspi.uart, 57,
	//     0x7E20100))
	// 	stdout_wire(&raspi.uart.outdev);
}

/** Initialize the input device for the RPi 3 platform. */
static void rpi3_input_init(void)
{
	
}

/** Enable the virtual timer interrupt for the RPi 3 platform. */
static inr_t rpi3_enable_vtimer_irq(void)
{
	return 27;
}

/** Get the number of interrupts for the RPi 3 platform. */
static size_t rpi3_get_irq_count(void)
{
	return BCM2835_IRQ_COUNT;
}

/** Get the platform name for the RPi 3 platform. */
static const char* rpi3_get_platform_name(void)
{
	return "rpi3";
}

/** Output a character to the early UART for the RPi 3 platform. */
static void rpi3_early_uart_output(char32_t c)
{
	if (!rpi3_miniuart_virt_base) {
		// UART doesn't work and we have no way how to tell a user
        return;
    }

	volatile uint32_t *uart_data = (volatile uint32_t *)(rpi3_miniuart_virt_base + AUX_MU_IO_OFFSET);
	volatile uint32_t *uart_status = (volatile uint32_t *)(rpi3_miniuart_virt_base + AUX_MU_LSR_OFFSET);

	while ((*uart_status & BIT(5)) == 0) {}

	*uart_data = c;
}

struct arm_machine_ops rpi3_machine_ops = {
	rpi3_init,
	rpi3_irq_exception,
	rpi3_output_init,
	rpi3_input_init,
	rpi3_enable_vtimer_irq,
	rpi3_get_irq_count,
	rpi3_get_platform_name,
	rpi3_early_uart_output,
};

/** @}
 */
