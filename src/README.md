# PICclock Firmware Design

## Overview

Generates a variable-frequency clock output using a hybrid NCO/software approach.  
Output range: 1 Hz to 1 MHz on RB6 with 50% duty cycle.

**Timing Methods:**
- **NCO mode (12 Hz - 1 MHz):** Hardware NCO generates clock with zero CPU overhead
- **Software mode (1 Hz - 11 Hz):** Delay loops for sub-NCO frequencies

Frequency accuracy inherits from the PIC's 24 MHz crystal (typically ±50 ppm).

## Hardware Interface

| Function        | Pin  | Description                    |
|-----------------|------|--------------------------------|
| Crystal         | RA4/5| 24 MHz external crystal        |
| ADC Input       | RA0  | Potentiometer (0-5V)           |
| Clock Output    | RB6  | NCO1 output, active low (drives EL7232CNZ) |
| Debug LED       | RC5  | Status indicator               |
| Halt Select     | RC6  | High-speed mode (SW2, active low) |
| Step Button     | RC4  | Step pulse trigger (SW3)       |
| Step Select     | RC3  | Step mode select (SW1, active low) |
| ICSP            | RA0/1| Programming interface          |

## Timing Analysis

PIC16F18344 @ 24 MHz:
- NCO clock = Fosc = 24 MHz
- NCO resolution = 20 bits (2^20 = 1,048,576)

**NCO Frequency Formula:**
```
F_out = (Fosc × NCO_INC) / 2^21
F_out = (24,000,000 × NCO_INC) / 2,097,152
```

**NCO Increment Range:**

| NCO_INC | Frequency |
|---------|-----------|
| 1       | 11.4 Hz   |
| 87,381  | 1 MHz     |

**Software Mode Formula:**
```
F_out = Fosc / (2 × half_period_cycles)
F_out = 24,000,000 / (2 × half_period)
```

## Modules

### main.c

Application entry point with hybrid clock generation.

| Function           | Purpose                              |
|--------------------|--------------------------------------|
| `adc_init`         | Configure AN0 with Fosc/64 clock     |
| `adc_read`         | Return 8-bit ADC result              |
| `nco_init`         | Configure NCO1 for FDC mode output   |
| `nco_set_increment`| Update NCO frequency                 |
| `nco_connect`      | Route NCO1 to RB6 via PPS            |
| `nco_disconnect`   | Disconnect NCO, use GPIO             |
| `nco_stop`         | Disable NCO output                   |
| `output_step_pulse`| Generate single step pulse           |
| `button_pressed`   | Debounced button read                |
| `main`             | Mode detection and clock generation  |

### freq_table.h

Combined lookup table mapping ADC values (0-255) to frequency settings.

| Data             | Purpose                                    |
|------------------|--------------------------------------------|
| `freq_table[]`   | 256 entries: software half-periods or NCO increments |

Bit 31 indicates mode:
- Bit 31 = 1: Software mode, bits 0-23 = half-period in cycles
- Bit 31 = 0: NCO mode, bits 0-19 = NCO increment value

Logarithmically spaced (1 Hz to 1 MHz) for perceptually uniform control feel.

## Main Loop

1. Check mode switches (active low):
   - RC6 (Halt): Hold output low, LED off
   - RC3 (Step): Wait for button, output single pulse
2. If resuming from halt/step:
   - Re-read ADC and reconfigure mode
3. If software mode (freq < 12 Hz):
   - Generate clock with delay loops
   - Check for mode/frequency changes during delays
4. If NCO mode (freq ≥12 Hz):
   - NCO runs autonomously
   - Poll ADC for frequency changes

## Configuration Bits

| Setting | Value | Reason                          |
|---------|-------|---------------------------------|
| FOSC    | HS    | 24 MHz external crystal         |
| WDTE    | OFF   | No watchdog needed              |
| PWRTE   | OFF   | Power-up timer disabled         |
| BOREN   | ON    | Brown-out protection            |
| CP      | OFF   | No code protection              |
