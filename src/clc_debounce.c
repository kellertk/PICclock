/**
 * CLC Hardware Debounce for PIC16F18344
 *
 * Adapted from Microchip's pic18f16q40-clc-switch-debouncing example
 * (3 CLC solution). Translated for PIC16F18344 register differences:
 *
 *   - Direct register names (CLC1SEL0) instead of indexed (CLCSELECT + CLCnSEL0)
 *   - Different CLC data input selection values (Table 21-1, DS40001800E):
 *       CLCIN0PPS  = 0x00   (Q40: 0x00)
 *       CLC1_OUT   = 0x04   (Q40: 0x22)
 *       CLC2_OUT   = 0x05   (Q40: 0x23)
 *       CLC3_OUT   = 0x06   (Q40: 0x24)
 *       TMR2 match = 0x1A   (Q40: 0x10)
 *   - Basic TMR2 clocked from Fosc/4 = 6 MHz (no LFINTOSC option)
 *     Prescaler 1:64, PR2 = 140 → period = (141 × 64) / 6 MHz ≈ 1.5 ms
 */

#include <xc.h>
#include "clc_debounce.h"

/* CLC data input source values for PIC16F18344 (Table 21-1, DS40001800E) */
#define CLC_IN_CLCIN0PPS    0x00
#define CLC_IN_CLC1_OUT     0x04
#define CLC_IN_CLC2_OUT     0x05
#define CLC_IN_CLC3_OUT     0x06
#define CLC_IN_TMR2_MATCH   0x1A

void clc_debounce_init(void) {
    /*
     * Route step button (RC4) to CLCIN0 via PPS.
     * PPS input value: Port C base = 0x10, pin 4 → 0x14.
     */
    CLCIN0PPS = 0x14;

    /*
     * TMR2: ~1.5 ms sampling clock
     *
     * Clock source = Fosc/4 = 6 MHz (only option on PIC16F18344 TMR2)
     * Prescaler = 1:64  (T2CKPS = 0b11)
     * PR2 = 140
     * Period = (140 + 1) × 64 / 6 MHz = 1.504 ms
     *
     * T2CON bit layout:
     *   [7]   unused
     *   [6:3] T2OUTPS<3:0> = 0000 (postscaler 1:1)
     *   [2]   TMR2ON = 1
     *   [1:0] T2CKPS<1:0> = 11 (prescaler 1:64)
     *
     * T2CON = 0b00000111 = 0x07
     */
    T2CON = 0x00;       /* Stop TMR2 while configuring */
    PR2   = 140;         /* Period register */
    TMR2  = 0x00;        /* Clear counter */
    T2CON = 0x07;        /* TMR2 ON, prescaler 1:64 */

    /*
     * CLC3: 2-input D flip-flop with R (mode 0b101)
     * Samples the raw switch input on each TMR2 tick.
     *
     *   Data1 (→ Gate1 = CLK) = TMR2/PR2 match
     *   Data2 (→ Gate2 = D)   = CLCIN0 (raw switch on RC4)
     *   Gate3 (R) = no inputs  → reset inactive
     *   Gate4     = unused
     *
     * This captures the "previous" raw sample.
     */
    CLC3CON  = 0x00;               /* Disable during setup */
    CLC3POL  = 0x00;               /* No polarity inversions */
    CLC3SEL0 = CLC_IN_TMR2_MATCH;  /* Data1 = TMR2 match → CLK */
    CLC3SEL1 = CLC_IN_CLCIN0PPS;   /* Data2 = raw switch → D */
    CLC3SEL2 = CLC_IN_CLCIN0PPS;   /* Data3 = unused */
    CLC3SEL3 = CLC_IN_CLCIN0PPS;   /* Data4 = unused */
    CLC3GLS0 = 0x02;               /* Gate1(CLK): D1 true */
    CLC3GLS1 = 0x08;               /* Gate2(D):   D2 true */
    CLC3GLS2 = 0x00;               /* Gate3(R):   none (no reset) */
    CLC3GLS3 = 0x00;               /* Gate4:      unused */
    CLC3CON  = 0x85;               /* Enable, mode = 2-input D-FF w/ R */

    /*
     * CLC2: 4-input AND (mode 0b010)
     * Implements majority vote: MAJ(raw_switch, CLC3_out, CLC1_out)
     *
     * The majority function is decomposed as:
     *   MAJ(A,B,C) = AB + AC + BC
     *   Using inverted-input NAND gates:
     *     Gate1: no inputs → 0, inverted by G1POL → 1 (constant true)
     *     Gate2: ¬D2 | ¬D3 = NAND(switch, CLC3_out)
     *     Gate3: ¬D2 | ¬D4 = NAND(switch, CLC1_out)
     *     Gate4: ¬D3 | ¬D4 = NAND(CLC3_out, CLC1_out)
     *
     *   4-input AND output = 1·NAND·NAND·NAND
     *   With output polarity inverted → NOT(NAND·NAND·NAND)
     *     = OR(AND, AND, AND) = AB + AC + BC = MAJ(A,B,C)
     *
     *   Data1 = CLCIN0 (unused by gates, but must be valid)
     *   Data2 = CLCIN0 (raw switch) → used inverted in Gates 2, 3
     *   Data3 = CLC3_OUT (previous sample) → used inverted in Gates 2, 4
     *   Data4 = CLC1_OUT (debounced output) → used inverted in Gates 3, 4
     */
    CLC2CON  = 0x00;               /* Disable during setup */
    CLC2POL  = 0x81;               /* Output inverted + Gate1 inverted */
    CLC2SEL0 = CLC_IN_CLCIN0PPS;   /* Data1 = CLCIN0 (filler) */
    CLC2SEL1 = CLC_IN_CLCIN0PPS;   /* Data2 = raw switch */
    CLC2SEL2 = CLC_IN_CLC3_OUT;    /* Data3 = CLC3 output (prev sample) */
    CLC2SEL3 = CLC_IN_CLC1_OUT;    /* Data4 = CLC1 output (debounced) */
    CLC2GLS0 = 0x00;               /* Gate1: no inputs (→0, G1POL→1) */
    CLC2GLS1 = 0x14;               /* Gate2: D2_inv + D3_inv */
    CLC2GLS2 = 0x44;               /* Gate3: D2_inv + D4_inv */
    CLC2GLS3 = 0x50;               /* Gate4: D3_inv + D4_inv */
    CLC2CON  = 0x82;               /* Enable, mode = 4-input AND */

    /*
     * CLC1: 2-input D flip-flop with R (mode 0b101)
     * Samples the majority-vote output on each TMR2 tick.
     * This is the final debounced output.
     *
     *   Data1 (→ Gate1 = CLK) = TMR2/PR2 match
     *   Data2 (→ Gate2 = D)   = CLC2 output (majority vote)
     *   Gate3 (R) = no inputs  → reset inactive
     *   Gate4     = unused
     */
    CLC1CON  = 0x00;               /* Disable during setup */
    CLC1POL  = 0x00;               /* No polarity inversions */
    CLC1SEL0 = CLC_IN_TMR2_MATCH;  /* Data1 = TMR2 match → CLK */
    CLC1SEL1 = CLC_IN_CLC2_OUT;    /* Data2 = majority result → D */
    CLC1SEL2 = CLC_IN_CLCIN0PPS;   /* Data3 = unused */
    CLC1SEL3 = CLC_IN_CLCIN0PPS;   /* Data4 = unused */
    CLC1GLS0 = 0x02;               /* Gate1(CLK): D1 true */
    CLC1GLS1 = 0x08;               /* Gate2(D):   D2 true */
    CLC1GLS2 = 0x00;               /* Gate3(R):   none (no reset) */
    CLC1GLS3 = 0x00;               /* Gate4:      unused */
    CLC1CON  = 0x85;               /* Enable, mode = 2-input D-FF w/ R */
}
