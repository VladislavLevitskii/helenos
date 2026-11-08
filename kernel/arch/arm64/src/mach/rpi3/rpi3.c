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
#include <stdio.h>

#define BIT(n) (1U << (n))

#define AUX_MU_BASE_REG   0x7E215000 
#define AUX_MU_IO_OFFSET  0x40 
#define AUX_MU_LSR_OFFSET 0x54

#define VIDEO_ADR2CPU_ADR(n) ((n) - 0x3F000000)

static uintptr_t rpi3_miniuart_virt_base = 0;
static uintptr_t rpi3_acpi_rsdp = 0;

// static struct {
// 	pl011_uart_t	uart;
// } raspi;

/** Initialize the RPi 3 platform. */
static void rpi3_init(void)
{	
	const unsigned int uart_flags = PAGE_NOT_CACHEABLE | PAGE_READ | PAGE_WRITE | PAGE_KERNEL;
    rpi3_miniuart_virt_base = km_map(VIDEO_ADR2CPU_ADR(AUX_MU_BASE_REG), PAGE_SIZE, KM_NATURAL_ALIGNMENT, uart_flags);

	const unsigned int acpi_rsdp_flags = PAGE_READ | PAGE_KERNEL;
	rpi3_acpi_rsdp = km_map((uintptr_t)acpi_rsdp, PAGE_SIZE, KM_NATURAL_ALIGNMENT, acpi_rsdp_flags);

	printf("rpi3_acpi_rsdp: %p\n", (void*)rpi3_acpi_rsdp);
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

	while ((*uart_status & BIT(5)) == 0) {
	}

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
