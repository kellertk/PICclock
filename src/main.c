/**
 * PICclock - Hybrid NCO/Software Clock Generator
 * Target: PIC16F18344 @ 24 MHz (external crystal)
 * 
 * Generates variable-frequency clock output using:
 *   - Hardware NCO for 12 Hz to 1 MHz (zero CPU overhead)
 *   - Software timing for 1 Hz to 11 Hz (trivial overhead at slow speeds)
 * 
 * Output on RB6 drives EL7232CNZ line driver.
 * 
 * Frequency range: 1 Hz to 1 MHz with 50% duty cycle
 * Timing accuracy: Inherits from 24 MHz crystal (typically ±50 ppm)
 * 
 * NCO FDC mode: F_out = (24 MHz × NCO_INC) / 2^21
 */

#include <xc.h>
#include <stdint.h>
#include "freq_table.h"

// Configuration bits for PIC16F18344
#pragma config FEXTOSC = HS    // External oscillator: HS (24 MHz crystal)
#pragma config RSTOSC = EXT1X  // Power-up oscillator: EXTOSC (no 4x PLL)
#pragma config CLKOUTEN = OFF  // Clock out disabled
#pragma config CSWEN = OFF     // Clock switch disabled
#pragma config FCMEN = OFF     // Fail-safe clock monitor disabled
#pragma config MCLRE = ON      // MCLR pin enabled (RA3)
#pragma config PWRTE = OFF     // Power-up timer disabled
#pragma config WDTE = OFF      // Watchdog disabled
#pragma config LPBOREN = OFF   // Low-power BOR disabled
#pragma config BOREN = ON      // Brown-out reset enabled
#pragma config BORV = LOW      // Brown-out voltage low trip point
#pragma config PPS1WAY = ON    // PPS one-way control
#pragma config STVREN = ON     // Stack overflow reset enabled
#pragma config DEBUG = OFF     // Background debugger disabled
#pragma config LVP = OFF       // Low-voltage programming disabled
#pragma config CP = OFF        // Code protection off

#define _XTAL_FREQ 24000000UL  // 24 MHz external crystal

// Pin 2:  RA5/OSC1 = Crystal
// Pin 3:  RA4/OSC2 = Crystal
// Pin 4:  RA3/MCLR = Reset
// Pin 5:  RC5 = Debug LED output
// Pin 6:  RC4 = Step button (SW3, active low)
// Pin 7:  RC6 = Halt select (SW2, active low)
// Pin 8:  RC3 = Step mode select (SW1, active low)
// Pin 11: RB6 = NCO1 output (drives EL7232CNZ)
// Pin 19: RA0 = ADC input (pot)

#define DEBUG_LED   LATCbits.LATC5   // Debug LED (active high)
#define HALT_SEL    PORTCbits.RC6    // Halt select (SW2)
#define STEP_BTN    PORTCbits.RC4    // Step button (SW3)
#define STEP_SEL    PORTCbits.RC3    // Step mode select (SW1)

static void adc_init(void) {
    ADCON0 = 0x01;         // AN0 selected, ADC enabled
    ADCON1 = 0x60;         // Left-justified, Fosc/64, Vref = VDD/VSS
    ANSELAbits.ANSA0 = 1;  // RA0 is analog
}

static uint8_t adc_read(void) {
    ADCON0bits.GO_nDONE = 1;
    while (ADCON0bits.GO_nDONE);
    return ADRESH;         // Return upper 8 bits (left-justified)
}

/**
 * Initialize the NCO (Numerically Controlled Oscillator)
 * 
 * FDC mode output frequency = (Fosc × NCO_INC) / 2^21
 */
static void nco_init(void) {
    // Configure RB6 as NCO1 output
    TRISBbits.TRISB6 = 0;       // Output
    ANSELBbits.ANSB6 = 0;       // Digital
    
    // Route NCO1 to RB6 via PPS (Peripheral Pin Select)
    RB6PPS = 0x1D;              // NCO1 output (0x1D per datasheet Table 13-3)
    
    NCO1CON = 0x00;             // Disable NCO while configuring
    NCO1CLK = 0x01;             // Clock source = FOSC (24 MHz external crystal)

    // Set initial value
    NCO1INCU = 0x00;            // Upper bits
    NCO1INCH = 0x00;            // High byte
    NCO1INCL = 0x01;            // Low byte - minimum frequency initially
    
    // Clear accumulator
    NCO1ACCU = 0x00;
    NCO1ACCH = 0x00;
    NCO1ACCL = 0x00;
    
    // Enable NCO in Fixed Duty Cycle (FDC) mode - 50% duty cycle output
    NCO1CON = 0x80;             // N1EN=1, N1PFM=0 (FDC mode), N1POL=0
}

/**
 * Update NCO frequency from 20-bit increment value
 */
static void nco_set_increment(uint32_t inc) {
    // Temporarily disable NCO to update increment atomically
    NCO1CON = 0x00;
    
    NCO1INCL = (uint8_t)(inc & 0xFF);
    NCO1INCH = (uint8_t)((inc >> 8) & 0xFF);
    NCO1INCU = (uint8_t)((inc >> 16) & 0x0F);  // Only 4 bits in upper
    
    NCO1CON = 0x80;  // Enable NCO, FDC mode
}

/**
 * Stop NCO output (for step mode or halt)
 */
static void nco_stop(void) {
    NCO1CON = 0x00;
    LATBbits.LATB6 = 0;         // Ensure output is low
}

/**
 * Disconnect NCO from pin (for software mode)
 * Sets RB6 as regular GPIO output
 */
static void nco_disconnect(void) {
    NCO1CON = 0x00;
    RB6PPS = 0x00;              // Disconnect NCO from RB6, use LATB6
    LATBbits.LATB6 = 0;
}

/**
 * Reconnect NCO to pin
 */
static void nco_connect(void) {
    RB6PPS = 0x1D;              // Route NCO1 to RB6
    NCO1CON = 0x80;             // Enable NCO, FDC mode
}

/**
 * Output a single step pulse (for step mode)
 */
static void output_step_pulse(void) {
    LATBbits.LATB6 = 1;
    __delay_ms(10);
    LATBbits.LATB6 = 0;
}

/**
 * Simple button debounce
 */
static uint8_t button_pressed(void) {
    if (STEP_BTN == 0) {
        __delay_ms(20);
        if (STEP_BTN == 0) {
            return 1;
        }
    }
    return 0;
}

static void wait_button_release(void) {
    while (STEP_BTN == 0);
    __delay_ms(20);
}

void main(void) {
    // Configure I/O
    // PORTA: RA0=analog in, RA4/RA5=crystal
    TRISA = 0b00110001;         // RA0,RA4,RA5 input (crystal pins), rest output
    ANSELA = 0b00000001;        // Only RA0 is analog
    LATA = 0x00;
    
    // PORTB: RB6=NCO out
    TRISB = 0b00000000;         // All outputs
    ANSELB = 0x00;              // All digital
    LATB = 0x00;
    
    // PORTC: RC3,RC4,RC6 = switch inputs, RC5 = LED out
    TRISC = 0b01011000;         // RC3,RC4,RC6 inputs, rest outputs
    ANSELC = 0x00;              // All digital
    LATC = 0x00;
    
    // Enable weak pull-up on step button
    WPUC = 0b00010000;
    
    // Startup LED blink
    DEBUG_LED = 1;
    __delay_ms(100);
    DEBUG_LED = 0;
    __delay_ms(100);
    DEBUG_LED = 1;

    adc_init();
    nco_init();

    // Read initial ADC value
    uint8_t last_adc = adc_read();
    uint32_t freq_entry = freq_table[last_adc];
    uint8_t software_mode = IS_SOFTWARE_MODE(freq_entry) ? 1 : 0;
    uint32_t half_period = 0;  // For software mode (in cycles)
    
    if (software_mode) {
        nco_disconnect();
        half_period = GET_FREQ_VALUE(freq_entry);
    } else {
        nco_set_increment(GET_FREQ_VALUE(freq_entry));
    }
    
    uint8_t running = 1;
    uint8_t halted = 0;

    // Main loop
    while (1) {
        // Check mode switches
        uint8_t mode = (HALT_SEL ? 0 : 2) | (STEP_SEL ? 0 : 1);  // Active low switches
        
        // Halt mode: stop output, LED off
        if (mode & 2) {
            if (!halted) {
                if (software_mode) {
                    LATBbits.LATB6 = 0;
                } else {
                    nco_stop();
                }
                DEBUG_LED = 0;
                halted = 1;
                running = 0;
            }
            __delay_ms(50);
            continue;
        }
        
        // Step mode: output stopped, manual pulses
        if (mode & 1) {
            if (running || !halted) {
                if (!software_mode) {
                    nco_disconnect();
                }
                LATBbits.LATB6 = 0;
                running = 0;
                halted = 1;
            }
            DEBUG_LED = 1;
            
            if (button_pressed()) {
                output_step_pulse();
                wait_button_release();
            }
            __delay_ms(10);
            continue;
        }
        
        // --- Variable frequency mode ---
        
        // Resume from halt/step mode
        if (halted) {
            // Re-read ADC and reconfigure
            last_adc = adc_read();
            freq_entry = freq_table[last_adc];
            software_mode = IS_SOFTWARE_MODE(freq_entry) ? 1 : 0;
            
            if (software_mode) {
                half_period = GET_FREQ_VALUE(freq_entry);
                // RB6 already disconnected from NCO
            } else {
                nco_connect();
                nco_set_increment(GET_FREQ_VALUE(freq_entry));
            }
            DEBUG_LED = 1;  // LED on when running
            running = 1;
            halted = 0;
        }
        
        // Software mode: generate clock with delays
        if (software_mode) {
            // High phase
            LATBbits.LATB6 = 1;
            
            // Delay for half period (use 10us chunks for long delays)
            // At 24 MHz, 10us = 240 cycles
            // half_period is in cycles, convert to 10us units
            uint32_t delay_10us = half_period / 240;
            for (uint32_t i = 0; i < delay_10us; i++) {
                __delay_us(10);
                
                // Check for mode change every ~10ms
                if ((i & 0x3FF) == 0) {
                    uint8_t m = (HALT_SEL ? 0 : 2) | (STEP_SEL ? 0 : 1);
                    if (m != 0) {
                        LATBbits.LATB6 = 0;
                        break;  // Exit for loop, main while will catch mode change
                    }
                }
            }
            
            // Low phase
            LATBbits.LATB6 = 0;
            
            for (uint32_t i = 0; i < delay_10us; i++) {
                __delay_us(10);
                
                // Check for mode change every ~10ms
                if ((i & 0x3FF) == 0) {
                    uint8_t m = (HALT_SEL ? 0 : 2) | (STEP_SEL ? 0 : 1);
                    if (m != 0) break;  // Exit for loop, main while will catch mode change
                    
                    // Also check ADC during low phase
                    uint8_t adc_val = adc_read();
                    if (adc_val != last_adc) {
                        if (adc_val > last_adc + 1 || adc_val < last_adc - 1) {
                            last_adc = adc_val;
                            freq_entry = freq_table[adc_val];
                            
                            // Check if switching to NCO mode
                            if (!IS_SOFTWARE_MODE(freq_entry)) {
                                software_mode = 0;
                                nco_connect();
                                nco_set_increment(GET_FREQ_VALUE(freq_entry));
                                break;
                            } else {
                                half_period = GET_FREQ_VALUE(freq_entry);
                                delay_10us = half_period / 240;
                            }
                        }
                    }
                }
            }
        } else {
            // NCO mode: just poll ADC occasionally
            uint8_t adc_val = adc_read();
            if (adc_val != last_adc) {
                if (adc_val > last_adc + 1 || adc_val < last_adc - 1) {
                    last_adc = adc_val;
                    freq_entry = freq_table[adc_val];
                    
                    // Check if switching to software mode
                    if (IS_SOFTWARE_MODE(freq_entry)) {
                        software_mode = 1;
                        nco_disconnect();
                        half_period = GET_FREQ_VALUE(freq_entry);
                    } else {
                        nco_set_increment(GET_FREQ_VALUE(freq_entry));
                    }
                }
            }
            
            __delay_ms(20);  // NCO runs independently, just poll UI
        }
    }
}