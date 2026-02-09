/**
 * CLC Hardware Debounce for PIC16F18344
 *
 * Implements the "Three CLC" switch debounce solution from Microchip
 * application example pic18f16q40-clc-switch-debouncing, adapted for
 * the PIC16F18344.
 *
 * The circuit uses TMR2 as a ~1.5 ms sampling clock and three CLCs:
 *   CLC3: D flip-flop samples raw switch (CLCIN0 = RC4) on TMR2 clock
 *   CLC2: 4-input AND implements majority vote of (raw, prev, debounced)
 *   CLC1: D flip-flop samples majority output on TMR2 → debounced result
 *
 * The debounced output is readable from CLCDATA bit CLC1OUT with zero
 * CPU overhead — the CLC+TMR2 hardware runs autonomously.
 *
 * Reference: https://github.com/microchip-pic-avr-examples/
 *            pic18f16q40-clc-switch-debouncing
 */

#ifndef CLC_DEBOUNCE_H
#define CLC_DEBOUNCE_H

#include <xc.h>

/**
 * Initialize the 3-CLC hardware debounce circuit for the step button (RC4).
 * Must be called after I/O port configuration and before the main loop.
 */
void clc_debounce_init(void);

#endif /* CLC_DEBOUNCE_H */
