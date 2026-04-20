// // Copyright 2026 EPFL
// // Solderpad Hardware License, Version 2.1, see LICENSE.md for details.
// // SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
// // Description: Example application to test the Serial Link FIFO interrupt.
// //              When data arrives in the Serial Link FIFO, an interrupt fires
// //              and directly triggers a DMA transfer from the FIFO to RAM.
// //              The CPU is free to do other work while waiting for data.

// #include <stdio.h>
// #include <stdlib.h>
// #include "csr.h"
// #include "hart.h"
// #include "handler.h"
// #include "core_v_mini_mcu.h"
// #include "dma.h"
// #include "serial_link_single_channel_regs.h"
// #include "serial_link_regs.h"
// #include "serial_link.h"
// #include "serial_link_xheep_wrapper_driver.h"
// #include "pad_control.h"
// #include "pad_control_regs.h"
// #include "rv_plic.h"

// // 1 = receiver board, 0 = sender board 
// #define FPGA_RECEIVE 1

// #define PRINTF_IN_FPGA  1
// #define PRINTF_IN_SIM   1

// #if TARGET_SIM && PRINTF_IN_SIM
//     #define PRINTF(fmt, ...)    printf(fmt, ## __VA_ARGS__)
// #elif PRINTF_IN_FPGA && !TARGET_SIM
//     #define PRINTF(fmt, ...)    printf(fmt, ## __VA_ARGS__)
// #else
//     #define PRINTF(...)
// #endif

// #define DIRECT_WRITE_TARGET_ADDR    0x0000F800
// #define SYNC_ADDR                   0x00007F00
// #define READY                       0x00000001

// #define NUM_WORDS 4
// const int32_t test_data[NUM_WORDS] = {0x11111111, 0x22222222, 0x33333333, 0x44444444};

// // DMA destination buffer
// static uint32_t dma_buffer[NUM_WORDS] __attribute__((aligned(4))) = {0};

// // Simulation only
// #if TARGET_SIM
//     #define EXT_SLAVE_LENGTH            0x400
//     #define SL_EXTERNAL_WRITE           (volatile int32_t *)(EXT_SLAVE_START_ADDRESS + EXT_SLAVE_LENGTH)
//     #define SL_EXTERNAL_CTRL_REG_ADDR   (int32_t *)(EXT_PERIPHERAL_START_ADDRESS + 0x06000 + SERIAL_LINK_SINGLE_CHANNEL_CTRL_REG_OFFSET)
//     #define SL_EXTERNAL_DIRECT_WRITE    (int32_t *)(EXT_SLAVE_START_ADDRESS + EXT_SLAVE_LENGTH + DIRECT_WRITE_TARGET_ADDR)
// #endif

// void handler_irq_sl_direct_write(uint32_t id) {
//     sl_wrapper_direct_write_intr_flag = 1;
//     plic_irq_set_enabled(SERIAL_LINK_DIRECT_WRITE_ID, kPlicToggleDisabled);
// }

// int main(int argc, char *argv[]) {

//     // PAD MUX
//     pad_control_t pad_control;
//     pad_control.base_addr = mmio_region_from_addr((uintptr_t)PAD_CONTROL_START_ADDRESS);
//     pad_control_set_mux(&pad_control, (ptrdiff_t)(PAD_CONTROL_PAD_MUX_GPIO_1_REG_OFFSET), 1);
//     pad_control_set_mux(&pad_control, (ptrdiff_t)(PAD_CONTROL_PAD_MUX_GPIO_2_REG_OFFSET), 1);
//     pad_control_set_mux(&pad_control, (ptrdiff_t)(PAD_CONTROL_PAD_MUX_GPIO_3_REG_OFFSET), 1);
//     pad_control_set_mux(&pad_control, (ptrdiff_t)(PAD_CONTROL_PAD_MUX_GPIO_6_REG_OFFSET), 1);
//     pad_control_set_mux(&pad_control, (ptrdiff_t)(PAD_CONTROL_PAD_MUX_GPIO_7_REG_OFFSET), 1);
//     pad_control_set_mux(&pad_control, (ptrdiff_t)(PAD_CONTROL_PAD_MUX_GPIO_8_REG_OFFSET), 1);
//     pad_control_set_mux(&pad_control, (ptrdiff_t)(PAD_CONTROL_PAD_MUX_GPIO_9_REG_OFFSET), 1);
//     pad_control_set_mux(&pad_control, (ptrdiff_t)(PAD_CONTROL_PAD_MUX_GPIO_10_REG_OFFSET), 1);

//     sl_init((volatile uint32_t *)CTRL_REG_ADDR, (int32_t *)CTRL_REG_ADDR);
// #if TARGET_SIM
//     // =========================================================================
//     // SIMULATION: single board loopback test
//     // =========================================================================
//     PRINTF("=== Serial Link FIFO Interrupt Test (SIM) ===\n");

//     sl_init((volatile uint32_t *)SL_EXTERNAL_CTRL_REG_ADDR,
//             (int32_t *)SL_EXTERNAL_CTRL_REG_ADDR);

//     sl_wrapper_set_rx_mode(SL_WRAPPER_RX_MODE_FIFO);
//     dma_config_flags_t res = sl_wrapper_dma_read_launch(dma_buffer, NUM_WORDS);
//     if (res != DMA_CONFIG_OK) {
//         PRINTF("DMA launch failed: %d\n", res);
//         return EXIT_FAILURE;
//     }

//     for (int i = 0; i < NUM_WORDS; i++) {
//         *SL_EXTERNAL_WRITE = test_data[i];
//     }

//     PRINTF("Waiting for DMA completion...\n");
//     while (!sl_wrapper_dma_intr_flag) {
//         wait_for_interrupt();
//     }
//     sl_wrapper_dma_intr_flag = 0;

//     PRINTF("DMA complete! Verifying...\n");
//     int errors = 0;
//     for (int i = 0; i < NUM_WORDS; i++) {
//         if (dma_buffer[i] != (uint32_t)test_data[i]) {
//             PRINTF("ERROR [%d]: got 0x%08x expected 0x%08x\n",
//                    i, dma_buffer[i], (uint32_t)test_data[i]);
//             errors++;
//         } else {
//             PRINTF("OK [%d]: 0x%08x\n", i, dma_buffer[i]);
//         }
//     }

//      PRINTF("--- Test 2: Direct write interrupt ---\n");

//     for (int i = 0; i < NUM_WORDS; i++)
//         ((volatile uint32_t *)DIRECT_WRITE_TARGET_ADDR)[i] = 0;

//     sl_wrapper_set_rx_mode(SL_WRAPPER_RX_MODE_DIRECT_WRITE);

//     sl_wrapper_direct_write_arm(NUM_WORDS);

//     for (int i = 0; i < NUM_WORDS; i++) {
//         *((volatile int32_t *)SL_EXTERNAL_DIRECT_WRITE + i) = test_data[i];
//     }

//     while (!sl_wrapper_direct_write_intr_flag) {
//         wait_for_interrupt();
//     }
//     sl_wrapper_direct_write_intr_flag = 0;

//     for (int i = 0; i < NUM_WORDS; i++) {
//         int32_t rcv = ((volatile int32_t *)DIRECT_WRITE_TARGET_ADDR)[i];
//         if (rcv != test_data[i]) {
//             PRINTF("DIRECT WRITE ERROR [%d]: got 0x%08x expected 0x%08x\n",
//                    i, rcv, test_data[i]);
//             errors++;
//         } else {
//             PRINTF("DIRECT WRITE OK [%d]: 0x%08x\n", i, rcv);
//         }
//     }

//     if (errors == 0) {
//         PRINTF("DONE - All tests passed\n");
//         return EXIT_SUCCESS;
//     } else {
//         PRINTF("FAILED - %d errors\n", errors);
//         return EXIT_FAILURE;
//     }

// #elif FPGA_RECEIVE
//     // =========================================================================
//     // FPGA RECEIVER
//     // =========================================================================

//     PRINTF("=== Serial Link FIFO Interrupt Test (RECEIVE) ===\n");
//     int32_t rcv_data;

//     sl_wrapper_set_rx_mode(SL_WRAPPER_RX_MODE_FIFO);

//     dma_config_flags_t res = sl_wrapper_dma_read_launch(dma_buffer, NUM_WORDS); 
//     if (res != DMA_CONFIG_OK) {
//         PRINTF("DMA launch failed: %d\n", res);
//         return EXIT_FAILURE;
//     }

//     while (!sl_wrapper_dma_intr_flag) {
//         wait_for_interrupt();
//     }
//     sl_wrapper_dma_intr_flag = 0;

//     PRINTF("DMA complete! Verifying data...\n");

//     int errors = 0;
//     for (int i = 0; i < NUM_WORDS; i++) {
//         if (dma_buffer[i] != (uint32_t)test_data[i]) {
//             PRINTF("ERROR [%d]: got 0x%08x expected 0x%08x\n",
//                    i, dma_buffer[i], test_data[i]);
//             errors++;
//         } else {
//             PRINTF("OK [%d]: 0x%08x\n", i, dma_buffer[i]);
//         }
//     }

//     PRINTF("--- Test 2: Direct write mode ---\n");
//     sl_wrapper_set_rx_mode(SL_WRAPPER_RX_MODE_DIRECT_WRITE);

//     for (int i = 0; i < NUM_WORDS; i++)
//         ((volatile uint32_t *)DIRECT_WRITE_TARGET_ADDR)[i] = 0;

//     sl_wrapper_direct_write_arm(NUM_WORDS);

//     sl_wrapper_direct_write(SYNC_ADDR, READY);

//     while (!sl_wrapper_direct_write_intr_flag) {
//         wait_for_interrupt();
//     }
//     sl_wrapper_direct_write_intr_flag = 0;

//     for (int i = 0; i < NUM_WORDS; i++) {
//         rcv_data = ((volatile int32_t *)DIRECT_WRITE_TARGET_ADDR)[i];
//         if (rcv_data != test_data[i]) {
//             PRINTF("DIRECT WRITE ERROR [%d]: got 0x%08x expected 0x%08x\n",
//                 i, rcv_data, test_data[i]);
//             errors++;
//         } else {
//             PRINTF("DIRECT WRITE OK [%d]: 0x%08x\n", i, rcv_data);
//         }
//     }

//     if (errors == 0) {
//         PRINTF("DONE - All tests passed\n");
//         return EXIT_SUCCESS;
//     } else {
//         PRINTF("FAILED - %d errors\n", errors);
//         return EXIT_FAILURE;
//     }

// #else
//     // =========================================================================
//     // FPGA SENDER
//     // =========================================================================
//     sl_wrapper_set_rx_mode(SL_WRAPPER_RX_MODE_DIRECT_WRITE);
//     volatile uint32_t *ready = (volatile uint32_t *)SYNC_ADDR;
    
//     PRINTF("=== Serial Link FIFO Interrupt Test (SEND) ===\n");

//     for (int i = 0; i < NUM_WORDS; i++) {
//         *SL_WRITE = test_data[i];
//         PRINTF("Sent [%d]: 0x%08x\n", i, test_data[i]);
//     }

//     while(*ready != READY);
//     *ready = 0; 
   
//     for (int i = 0; i < NUM_WORDS; i++) {
//         sl_wrapper_direct_write(DIRECT_WRITE_TARGET_ADDR + i * 4, (uint32_t)test_data[i]);
//         PRINTF("Direct write sent [%d]: 0x%08x\n", i, test_data[i]);
//     }

//     PRINTF("DONE\n");
//     return EXIT_SUCCESS;
// #endif
// }

// Copyright 2026 EPFL
// Solderpad Hardware License, Version 2.1, see LICENSE.md for details.
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1

#include <stdio.h>
#include <stdlib.h>
#include "csr.h"
#include "hart.h"
#include "handler.h"
#include "core_v_mini_mcu.h"
#include "dma.h"
#include "serial_link_single_channel_regs.h"
#include "serial_link_regs.h"
#include "serial_link.h"
#include "serial_link_xheep_wrapper_driver.h"
#include "pad_control.h"
#include "pad_control_regs.h"
#include "rv_plic.h"

#define FPGA_RECEIVE 1

#define PRINTF_IN_FPGA  1
#define PRINTF_IN_SIM   1

#if TARGET_SIM && PRINTF_IN_SIM
    #define PRINTF(fmt, ...)    printf(fmt, ## __VA_ARGS__)
#elif PRINTF_IN_FPGA && !TARGET_SIM
    #define PRINTF(fmt, ...)    printf(fmt, ## __VA_ARGS__)
#else
    #define PRINTF(...)
#endif

#define DIRECT_WRITE_TARGET_ADDR    0x0000F800
#define SYNC_ADDR                   0x00007F00
#define READY                       0x00000001

#define NUM_WORDS       4
#define NUM_WORDS_LARGE 8

// Test data patterns
const int32_t test_data[NUM_WORDS] = {
    0x11111111, 0x22222222, 0x33333333, 0x44444444
};
const int32_t test_data_large[NUM_WORDS_LARGE] = {
    0xDEADBEEF, 0xCAFEBABE, 0x12345678, 0xABCDEF01,
    0x55AA55AA, 0xFF00FF00, 0x0F0F0F0F, 0xA5A5A5A5
};
const int32_t test_data_alt[NUM_WORDS] = {
    0xFFFFFFFF, 0x00000000, 0xAAAAAAAA, 0x55555555
};

// DMA destination buffers — placed well within ram0
static uint32_t dma_buffer[NUM_WORDS_LARGE] __attribute__((aligned(4))) = {0};

#if TARGET_SIM
    #define EXT_SLAVE_LENGTH            0x400
    #define SL_EXTERNAL_WRITE           (volatile int32_t *)(EXT_SLAVE_START_ADDRESS + EXT_SLAVE_LENGTH)
    #define SL_EXTERNAL_CTRL_REG_ADDR   (int32_t *)(EXT_PERIPHERAL_START_ADDRESS + 0x06000 + SERIAL_LINK_SINGLE_CHANNEL_CTRL_REG_OFFSET)
    #define SL_EXTERNAL_DIRECT_WRITE    (int32_t *)(EXT_SLAVE_START_ADDRESS + EXT_SLAVE_LENGTH + DIRECT_WRITE_TARGET_ADDR)
#endif

void handler_irq_sl_direct_write(uint32_t id) {
    sl_wrapper_direct_write_intr_flag = 1;
    plic_irq_set_enabled(SERIAL_LINK_DIRECT_WRITE_ID, kPlicToggleDisabled);
}

int main(int argc, char *argv[]) {

    pad_control_t pad_control;
    pad_control.base_addr = mmio_region_from_addr((uintptr_t)PAD_CONTROL_START_ADDRESS);
    pad_control_set_mux(&pad_control, (ptrdiff_t)(PAD_CONTROL_PAD_MUX_GPIO_1_REG_OFFSET),  1);
    pad_control_set_mux(&pad_control, (ptrdiff_t)(PAD_CONTROL_PAD_MUX_GPIO_2_REG_OFFSET),  1);
    pad_control_set_mux(&pad_control, (ptrdiff_t)(PAD_CONTROL_PAD_MUX_GPIO_3_REG_OFFSET),  1);
    pad_control_set_mux(&pad_control, (ptrdiff_t)(PAD_CONTROL_PAD_MUX_GPIO_6_REG_OFFSET),  1);
    pad_control_set_mux(&pad_control, (ptrdiff_t)(PAD_CONTROL_PAD_MUX_GPIO_7_REG_OFFSET),  1);
    pad_control_set_mux(&pad_control, (ptrdiff_t)(PAD_CONTROL_PAD_MUX_GPIO_8_REG_OFFSET),  1);
    pad_control_set_mux(&pad_control, (ptrdiff_t)(PAD_CONTROL_PAD_MUX_GPIO_9_REG_OFFSET),  1);
    pad_control_set_mux(&pad_control, (ptrdiff_t)(PAD_CONTROL_PAD_MUX_GPIO_10_REG_OFFSET), 1);

    sl_init((volatile uint32_t *)CTRL_REG_ADDR, (int32_t *)CTRL_REG_ADDR);

    int errors = 0;

#if TARGET_SIM
    // =========================================================================
    // SIMULATION
    // =========================================================================
    PRINTF("=== Serial Link Full Test Suite (SIM) ===\n");

    sl_init((volatile uint32_t *)SL_EXTERNAL_CTRL_REG_ADDR,
            (int32_t *)SL_EXTERNAL_CTRL_REG_ADDR);

    plic_Init();

    // -------------------------------------------------------------------------
    // T1: FIFO HW-DMA — basic transfer
    // -------------------------------------------------------------------------
    PRINTF("--- T1: FIFO HW-DMA basic (%d words) ---\n", NUM_WORDS);
    for (int i = 0; i < NUM_WORDS; i++) dma_buffer[i] = 0;
    sl_wrapper_set_rx_mode(SL_WRAPPER_RX_MODE_FIFO);
    dma_config_flags_t res = sl_wrapper_dma_read_launch(dma_buffer, NUM_WORDS);
    if (res != DMA_CONFIG_OK) { PRINTF("T1 DMA launch failed: %d\n", res); return EXIT_FAILURE; }
    for (int i = 0; i < NUM_WORDS; i++) *SL_EXTERNAL_WRITE = test_data[i];
    while (!sl_wrapper_dma_intr_flag) wait_for_interrupt();
    sl_wrapper_dma_intr_flag = 0;
    for (int i = 0; i < NUM_WORDS; i++) {
        if (dma_buffer[i] != (uint32_t)test_data[i]) {
            PRINTF("T1 ERROR [%d]: got 0x%08x expected 0x%08x\n", i, dma_buffer[i], test_data[i]);
            errors++;
        }
    }
    PRINTF("T1: %s\n", errors == 0 ? "PASS" : "FAIL");

    // -------------------------------------------------------------------------
    // T2: FIFO HW-DMA — larger transfer, different data pattern
    // -------------------------------------------------------------------------
    PRINTF("--- T2: FIFO HW-DMA large (%d words) ---\n", NUM_WORDS_LARGE);
    for (int i = 0; i < NUM_WORDS_LARGE; i++) dma_buffer[i] = 0;
    sl_wrapper_set_rx_mode(SL_WRAPPER_RX_MODE_FIFO);
    res = sl_wrapper_dma_read_launch(dma_buffer, NUM_WORDS_LARGE);
    if (res != DMA_CONFIG_OK) { PRINTF("T2 DMA launch failed: %d\n", res); return EXIT_FAILURE; }
    for (int i = 0; i < NUM_WORDS_LARGE; i++) *SL_EXTERNAL_WRITE = test_data_large[i];
    while (!sl_wrapper_dma_intr_flag) wait_for_interrupt();
    sl_wrapper_dma_intr_flag = 0;
    int t2_errors = 0;
    for (int i = 0; i < NUM_WORDS_LARGE; i++) {
        if (dma_buffer[i] != (uint32_t)test_data_large[i]) {
            PRINTF("T2 ERROR [%d]: got 0x%08x expected 0x%08x\n", i, dma_buffer[i], test_data_large[i]);
            t2_errors++; errors++;
        }
    }
    PRINTF("T2: %s\n", t2_errors == 0 ? "PASS" : "FAIL");

    // -------------------------------------------------------------------------
    // T3: FIFO HW-DMA — back-to-back, verify counter resets
    // -------------------------------------------------------------------------
    PRINTF("--- T3: FIFO HW-DMA back-to-back ---\n");
    int t3_errors = 0;
    for (int pass = 0; pass < 2; pass++) {
        const int32_t *data = (pass == 0) ? test_data : test_data_alt;
        for (int i = 0; i < NUM_WORDS; i++) dma_buffer[i] = 0;
        sl_wrapper_set_rx_mode(SL_WRAPPER_RX_MODE_FIFO);
        res = sl_wrapper_dma_read_launch(dma_buffer, NUM_WORDS);
        if (res != DMA_CONFIG_OK) { PRINTF("T3 pass%d DMA launch failed\n", pass); return EXIT_FAILURE; }
        for (int i = 0; i < NUM_WORDS; i++) *SL_EXTERNAL_WRITE = data[i];
        while (!sl_wrapper_dma_intr_flag) wait_for_interrupt();
        sl_wrapper_dma_intr_flag = 0;
        for (int i = 0; i < NUM_WORDS; i++) {
            if (dma_buffer[i] != (uint32_t)data[i]) {
                PRINTF("T3 pass%d ERROR [%d]: got 0x%08x expected 0x%08x\n", pass, i, dma_buffer[i], data[i]);
                t3_errors++; errors++;
            }
        }
    }
    PRINTF("T3: %s\n", t3_errors == 0 ? "PASS" : "FAIL");

    // -------------------------------------------------------------------------
    // T4: Direct write — basic transfer
    // -------------------------------------------------------------------------
    PRINTF("--- T4: Direct write interrupt basic (%d words) ---\n", NUM_WORDS);
    for (int i = 0; i < NUM_WORDS; i++) ((volatile uint32_t *)DIRECT_WRITE_TARGET_ADDR)[i] = 0;
    sl_wrapper_set_rx_mode(SL_WRAPPER_RX_MODE_DIRECT_WRITE);
    sl_wrapper_direct_write_arm(NUM_WORDS);
    for (int i = 0; i < NUM_WORDS; i++) *((volatile int32_t *)SL_EXTERNAL_DIRECT_WRITE + i) = test_data[i];
    while (!sl_wrapper_direct_write_intr_flag) wait_for_interrupt();
    sl_wrapper_direct_write_intr_flag = 0;
    int t4_errors = 0;
    for (int i = 0; i < NUM_WORDS; i++) {
        int32_t rcv = ((volatile int32_t *)DIRECT_WRITE_TARGET_ADDR)[i];
        if (rcv != test_data[i]) {
            PRINTF("T4 ERROR [%d]: got 0x%08x expected 0x%08x\n", i, rcv, test_data[i]);
            t4_errors++; errors++;
        }
    }
    PRINTF("T4: %s\n", t4_errors == 0 ? "PASS" : "FAIL");

    // -------------------------------------------------------------------------
    // T5: Direct write — back-to-back, verify counter resets correctly
    // -------------------------------------------------------------------------
    PRINTF("--- T5: Direct write back-to-back ---\n");
    int t5_errors = 0;
    for (int pass = 0; pass < 2; pass++) {
        const int32_t *data = (pass == 0) ? test_data_alt : test_data;
        for (int i = 0; i < NUM_WORDS; i++) ((volatile uint32_t *)DIRECT_WRITE_TARGET_ADDR)[i] = 0;
        sl_wrapper_set_rx_mode(SL_WRAPPER_RX_MODE_DIRECT_WRITE);
        sl_wrapper_direct_write_arm(NUM_WORDS);
        for (int i = 0; i < NUM_WORDS; i++) *((volatile int32_t *)SL_EXTERNAL_DIRECT_WRITE + i) = data[i];
        while (!sl_wrapper_direct_write_intr_flag) wait_for_interrupt();
        sl_wrapper_direct_write_intr_flag = 0;
        for (int i = 0; i < NUM_WORDS; i++) {
            int32_t rcv = ((volatile int32_t *)DIRECT_WRITE_TARGET_ADDR)[i];
            if (rcv != data[i]) {
                PRINTF("T5 pass%d ERROR [%d]: got 0x%08x expected 0x%08x\n", pass, i, rcv, data[i]);
                t5_errors++; errors++;
            }
        }
    }
    PRINTF("T5: %s\n", t5_errors == 0 ? "PASS" : "FAIL");

    // -------------------------------------------------------------------------
    // T6: Mode switch — FIFO → direct write → FIFO, verify rx_mode resets
    // -------------------------------------------------------------------------
    PRINTF("--- T6: Mode switch FIFO->DW->FIFO ---\n");
    int t6_errors = 0;

    // FIFO pass
    for (int i = 0; i < NUM_WORDS; i++) dma_buffer[i] = 0;
    sl_wrapper_set_rx_mode(SL_WRAPPER_RX_MODE_FIFO);
    res = sl_wrapper_dma_read_launch(dma_buffer, NUM_WORDS);
    if (res != DMA_CONFIG_OK) { PRINTF("T6 FIFO DMA launch failed\n"); return EXIT_FAILURE; }
    for (int i = 0; i < NUM_WORDS; i++) *SL_EXTERNAL_WRITE = test_data[i];
    while (!sl_wrapper_dma_intr_flag) wait_for_interrupt();
    sl_wrapper_dma_intr_flag = 0;
    for (int i = 0; i < NUM_WORDS; i++) {
        if (dma_buffer[i] != (uint32_t)test_data[i]) {
            PRINTF("T6 FIFO ERROR [%d]: got 0x%08x expected 0x%08x\n", i, dma_buffer[i], test_data[i]);
            t6_errors++; errors++;
        }
    }

    // Direct write pass
    for (int i = 0; i < NUM_WORDS; i++) ((volatile uint32_t *)DIRECT_WRITE_TARGET_ADDR)[i] = 0;
    sl_wrapper_set_rx_mode(SL_WRAPPER_RX_MODE_DIRECT_WRITE);
    sl_wrapper_direct_write_arm(NUM_WORDS);
    for (int i = 0; i < NUM_WORDS; i++) *((volatile int32_t *)SL_EXTERNAL_DIRECT_WRITE + i) = test_data_alt[i];
    while (!sl_wrapper_direct_write_intr_flag) wait_for_interrupt();
    sl_wrapper_direct_write_intr_flag = 0;
    for (int i = 0; i < NUM_WORDS; i++) {
        int32_t rcv = ((volatile int32_t *)DIRECT_WRITE_TARGET_ADDR)[i];
        if (rcv != test_data_alt[i]) {
            PRINTF("T6 DW ERROR [%d]: got 0x%08x expected 0x%08x\n", i, rcv, test_data_alt[i]);
            t6_errors++; errors++;
        }
    }

    // FIFO again — verify mode switched back correctly
    for (int i = 0; i < NUM_WORDS; i++) dma_buffer[i] = 0;
    sl_wrapper_set_rx_mode(SL_WRAPPER_RX_MODE_FIFO);
    res = sl_wrapper_dma_read_launch(dma_buffer, NUM_WORDS);
    if (res != DMA_CONFIG_OK) { PRINTF("T6 FIFO2 DMA launch failed\n"); return EXIT_FAILURE; }
    for (int i = 0; i < NUM_WORDS; i++) *SL_EXTERNAL_WRITE = test_data_large[i];
    while (!sl_wrapper_dma_intr_flag) wait_for_interrupt();
    sl_wrapper_dma_intr_flag = 0;
    for (int i = 0; i < NUM_WORDS; i++) {
        if (dma_buffer[i] != (uint32_t)test_data_large[i]) {
            PRINTF("T6 FIFO2 ERROR [%d]: got 0x%08x expected 0x%08x\n", i, dma_buffer[i], test_data_large[i]);
            t6_errors++; errors++;
        }
    }
    PRINTF("T6: %s\n", t6_errors == 0 ? "PASS" : "FAIL");

    // -------------------------------------------------------------------------
    // Result
    // -------------------------------------------------------------------------
    PRINTF("\n=== Results: %d error(s) ===\n", errors);
    if (errors == 0) { PRINTF("DONE - All tests passed\n"); return EXIT_SUCCESS; }
    else             { PRINTF("FAILED\n");                  return EXIT_FAILURE;  }

#elif FPGA_RECEIVE
    // =========================================================================
    // FPGA RECEIVER
    // =========================================================================
    PRINTF("=== Serial Link Full Test Suite (FPGA RECEIVE) ===\n");

    // -------------------------------------------------------------------------
    // T1: FIFO HW-DMA basic
    // -------------------------------------------------------------------------
    PRINTF("--- T1: FIFO HW-DMA basic (%d words) ---\n", NUM_WORDS);
    for (int i = 0; i < NUM_WORDS; i++) dma_buffer[i] = 0;
    sl_wrapper_set_rx_mode(SL_WRAPPER_RX_MODE_FIFO);
    dma_config_flags_t res = sl_wrapper_dma_read_launch(dma_buffer, NUM_WORDS);
    if (res != DMA_CONFIG_OK) { PRINTF("T1 DMA launch failed: %d\n", res); return EXIT_FAILURE; }
    while (!sl_wrapper_dma_intr_flag) wait_for_interrupt();
    sl_wrapper_dma_intr_flag = 0;
    int t1_errors = 0;
    for (int i = 0; i < NUM_WORDS; i++) {
        if (dma_buffer[i] != (uint32_t)test_data[i]) {
            PRINTF("T1 ERROR [%d]: got 0x%08x expected 0x%08x\n", i, dma_buffer[i], test_data[i]);
            t1_errors++; errors++;
        } else { PRINTF("T1 OK [%d]: 0x%08x\n", i, dma_buffer[i]); }
    }
    PRINTF("T1: %s\n", t1_errors == 0 ? "PASS" : "FAIL");

    // -------------------------------------------------------------------------
    // T2: FIFO HW-DMA large
    // -------------------------------------------------------------------------
    PRINTF("--- T2: FIFO HW-DMA large (%d words) ---\n", NUM_WORDS_LARGE);
    for (int i = 0; i < NUM_WORDS_LARGE; i++) dma_buffer[i] = 0;
    sl_wrapper_set_rx_mode(SL_WRAPPER_RX_MODE_FIFO);
    res = sl_wrapper_dma_read_launch(dma_buffer, NUM_WORDS_LARGE);
    if (res != DMA_CONFIG_OK) { PRINTF("T2 DMA launch failed: %d\n", res); return EXIT_FAILURE; }
    sl_wrapper_direct_write(SYNC_ADDR, READY);
    while (!sl_wrapper_dma_intr_flag) wait_for_interrupt();
    sl_wrapper_dma_intr_flag = 0;
    int t2_errors = 0;
    for (int i = 0; i < NUM_WORDS_LARGE; i++) {
        if (dma_buffer[i] != (uint32_t)test_data_large[i]) {
            PRINTF("T2 ERROR [%d]: got 0x%08x expected 0x%08x\n", i, dma_buffer[i], test_data_large[i]);
            t2_errors++; errors++;
        } else { PRINTF("T2 OK [%d]: 0x%08x\n", i, dma_buffer[i]); }
    }
    PRINTF("T2: %s\n", t2_errors == 0 ? "PASS" : "FAIL");

    // -------------------------------------------------------------------------
    // T3: FIFO HW-DMA back-to-back
    // -------------------------------------------------------------------------
    PRINTF("--- T3: FIFO HW-DMA back-to-back ---\n");
    int t3_errors = 0;
    for (int pass = 0; pass < 2; pass++) {
        const int32_t *data = (pass == 0) ? test_data : test_data_alt;
        for (int i = 0; i < NUM_WORDS; i++) dma_buffer[i] = 0;
        sl_wrapper_set_rx_mode(SL_WRAPPER_RX_MODE_FIFO);
        res = sl_wrapper_dma_read_launch(dma_buffer, NUM_WORDS);
        if (res != DMA_CONFIG_OK) { PRINTF("T3 pass%d launch failed\n", pass); return EXIT_FAILURE; }
        sl_wrapper_direct_write(SYNC_ADDR, READY);
        while (!sl_wrapper_dma_intr_flag) wait_for_interrupt();
        sl_wrapper_dma_intr_flag = 0;
        for (int i = 0; i < NUM_WORDS; i++) {
            if (dma_buffer[i] != (uint32_t)data[i]) {
                PRINTF("T3 pass%d ERROR [%d]: got 0x%08x expected 0x%08x\n", pass, i, dma_buffer[i], data[i]);
                t3_errors++; errors++;
            } else { PRINTF("T3 pass%d OK [%d]: 0x%08x\n", pass, i, dma_buffer[i]); }
        }
    }
    PRINTF("T3: %s\n", t3_errors == 0 ? "PASS" : "FAIL");

    // -------------------------------------------------------------------------
    // T4: Direct write basic
    // -------------------------------------------------------------------------
    PRINTF("--- T4: Direct write interrupt basic (%d words) ---\n", NUM_WORDS);
    for (int i = 0; i < NUM_WORDS; i++) ((volatile uint32_t *)DIRECT_WRITE_TARGET_ADDR)[i] = 0;
    sl_wrapper_set_rx_mode(SL_WRAPPER_RX_MODE_DIRECT_WRITE);
    sl_wrapper_direct_write_arm(NUM_WORDS);
    sl_wrapper_direct_write(SYNC_ADDR, READY);      // signal sender test 4
    while (!sl_wrapper_direct_write_intr_flag) wait_for_interrupt();
    sl_wrapper_direct_write_intr_flag = 0;
    int t4_errors = 0;
    for (int i = 0; i < NUM_WORDS; i++) {
        int32_t rcv = ((volatile int32_t *)DIRECT_WRITE_TARGET_ADDR)[i];
        if (rcv != test_data[i]) {
            PRINTF("T4 ERROR [%d]: got 0x%08x expected 0x%08x\n", i, rcv, test_data[i]);
            t4_errors++; errors++;
        } else { PRINTF("T4 OK [%d]: 0x%08x\n", i, rcv); }
    }
    PRINTF("T4: %s\n", t4_errors == 0 ? "PASS" : "FAIL");

    // -------------------------------------------------------------------------
    // T5: Direct write back-to-back
    // -------------------------------------------------------------------------
    PRINTF("--- T5: Direct write back-to-back ---\n");
    int t5_errors = 0;
    for (int pass = 0; pass < 2; pass++) {
        const int32_t *expected = (pass == 0) ? test_data_alt : test_data;
        for (int i = 0; i < NUM_WORDS; i++) ((volatile uint32_t *)DIRECT_WRITE_TARGET_ADDR)[i] = 0;
        sl_wrapper_set_rx_mode(SL_WRAPPER_RX_MODE_DIRECT_WRITE);
        sl_wrapper_direct_write_arm(NUM_WORDS);
        sl_wrapper_direct_write(SYNC_ADDR, READY);  // signal sender each pass
        while (!sl_wrapper_direct_write_intr_flag) wait_for_interrupt();
        sl_wrapper_direct_write_intr_flag = 0;
        for (int i = 0; i < NUM_WORDS; i++) {
            int32_t rcv = ((volatile int32_t *)DIRECT_WRITE_TARGET_ADDR)[i];
            if (rcv != expected[i]) {
                PRINTF("T5 pass%d ERROR [%d]: got 0x%08x expected 0x%08x\n", pass, i, rcv, expected[i]);
                t5_errors++; errors++;
            } else { PRINTF("T5 pass%d OK [%d]: 0x%08x\n", pass, i, rcv); }
        }
    }
    PRINTF("T5: %s\n", t5_errors == 0 ? "PASS" : "FAIL");

    // -------------------------------------------------------------------------
    // T6: Mode switch FIFO→DW→FIFO
    // -------------------------------------------------------------------------
    PRINTF("--- T6: Mode switch FIFO->DW->FIFO ---\n");
    int t6_errors = 0;

    // FIFO
    for (int i = 0; i < NUM_WORDS; i++) dma_buffer[i] = 0;
    sl_wrapper_set_rx_mode(SL_WRAPPER_RX_MODE_FIFO);
    res = sl_wrapper_dma_read_launch(dma_buffer, NUM_WORDS);
    if (res != DMA_CONFIG_OK) { PRINTF("T6 FIFO launch failed\n"); return EXIT_FAILURE; }
    sl_wrapper_direct_write(SYNC_ADDR, READY);
    while (!sl_wrapper_dma_intr_flag) wait_for_interrupt();
    sl_wrapper_dma_intr_flag = 0;
    for (int i = 0; i < NUM_WORDS; i++) {
        if (dma_buffer[i] != (uint32_t)test_data[i]) {
            PRINTF("T6 FIFO ERROR [%d]: 0x%08x\n", i, dma_buffer[i]);
            t6_errors++; errors++;
        }
    }

    // Direct write
    for (int i = 0; i < NUM_WORDS; i++) ((volatile uint32_t *)DIRECT_WRITE_TARGET_ADDR)[i] = 0;
    sl_wrapper_set_rx_mode(SL_WRAPPER_RX_MODE_DIRECT_WRITE);
    sl_wrapper_direct_write_arm(NUM_WORDS);
    sl_wrapper_direct_write(SYNC_ADDR, READY);
    while (!sl_wrapper_direct_write_intr_flag) wait_for_interrupt();
    sl_wrapper_direct_write_intr_flag = 0;
    for (int i = 0; i < NUM_WORDS; i++) {
        int32_t rcv = ((volatile int32_t *)DIRECT_WRITE_TARGET_ADDR)[i];
        if (rcv != test_data_alt[i]) {
            PRINTF("T6 DW ERROR [%d]: 0x%08x\n", i, rcv);
            t6_errors++; errors++;
        }
    }

    // FIFO again
    for (int i = 0; i < NUM_WORDS; i++) dma_buffer[i] = 0;
    sl_wrapper_set_rx_mode(SL_WRAPPER_RX_MODE_FIFO);
    res = sl_wrapper_dma_read_launch(dma_buffer, NUM_WORDS);
    if (res != DMA_CONFIG_OK) { PRINTF("T6 FIFO2 launch failed\n"); return EXIT_FAILURE; }
    sl_wrapper_direct_write(SYNC_ADDR, READY);
    while (!sl_wrapper_dma_intr_flag) wait_for_interrupt();
    sl_wrapper_dma_intr_flag = 0;
    for (int i = 0; i < NUM_WORDS; i++) {
        if (dma_buffer[i] != (uint32_t)test_data_large[i]) {
            PRINTF("T6 FIFO2 ERROR [%d]: 0x%08x\n", i, dma_buffer[i]);
            t6_errors++; errors++;
        }
    }
    PRINTF("T6: %s\n", t6_errors == 0 ? "PASS" : "FAIL");

    // -------------------------------------------------------------------------
    // Result
    // -------------------------------------------------------------------------
    PRINTF("\n=== Results: %d error(s) ===\n", errors);
    if (errors == 0) { PRINTF("DONE - All tests passed\n"); return EXIT_SUCCESS; }
    else             { PRINTF("FAILED\n");                  return EXIT_FAILURE;  }

#else
    // =========================================================================
    // FPGA SENDER
    // =========================================================================
    PRINTF("=== Serial Link Full Test Suite (FPGA SEND) ===\n");

    sl_wrapper_set_rx_mode(SL_WRAPPER_RX_MODE_DIRECT_WRITE);
    volatile uint32_t *ready = (volatile uint32_t *)SYNC_ADDR;

    // T1: FIFO basic
    PRINTF("--- T1: sending %d words via FIFO ---\n", NUM_WORDS);
    for (int i = 0; i < NUM_WORDS; i++) *SL_WRITE = test_data[i];

    // T2: FIFO large
    PRINTF("--- T2: sending %d words via FIFO ---\n", NUM_WORDS_LARGE);
    while (*ready != READY); *ready = 0;
    for (int i = 0; i < NUM_WORDS_LARGE; i++) *SL_WRITE = test_data_large[i];

    // T3: FIFO back-to-back (2 passes)
    PRINTF("--- T3: FIFO back-to-back ---\n");
    while (*ready != READY); *ready = 0;
    for (int i = 0; i < NUM_WORDS; i++) *SL_WRITE = test_data[i];
    while (*ready != READY); *ready = 0;
    for (int i = 0; i < NUM_WORDS; i++) *SL_WRITE = test_data_alt[i];

    // T4: Direct write basic — wait for receiver ready
    PRINTF("--- T4: Direct write basic ---\n");
    while (*ready != READY); *ready = 0;
    for (int i = 0; i < NUM_WORDS; i++)
        sl_wrapper_direct_write(DIRECT_WRITE_TARGET_ADDR + i * 4, (uint32_t)test_data[i]);

    // T5: Direct write back-to-back (2 passes)
    PRINTF("--- T5: Direct write back-to-back ---\n");
    while (*ready != READY); *ready = 0;
    for (int i = 0; i < NUM_WORDS; i++)
        sl_wrapper_direct_write(DIRECT_WRITE_TARGET_ADDR + i * 4, (uint32_t)test_data_alt[i]);
    while (*ready != READY); *ready = 0;
    for (int i = 0; i < NUM_WORDS; i++)
        sl_wrapper_direct_write(DIRECT_WRITE_TARGET_ADDR + i * 4, (uint32_t)test_data[i]);

    // T6: Mode switch — FIFO, then direct write, then FIFO again
    PRINTF("--- T6: Mode switch ---\n");
     while (*ready != READY); *ready = 0;
    for (int i = 0; i < NUM_WORDS; i++) *SL_WRITE = test_data[i];
    while (*ready != READY); *ready = 0;
    for (int i = 0; i < NUM_WORDS; i++)
        sl_wrapper_direct_write(DIRECT_WRITE_TARGET_ADDR + i * 4, (uint32_t)test_data_alt[i]);
    while (*ready != READY); *ready = 0;
    for (int i = 0; i < NUM_WORDS; i++) *SL_WRITE = test_data_large[i];

    PRINTF("DONE\n");
    return EXIT_SUCCESS;
#endif
}