# PICclock

Variable-frequency clock generator using PIC16F18344 hardware NCO.

See [src/README.md](src/README.md) and [hardware/README.md](hardware/README.md) for details.

## Features

- 1 Hz to 1 MHz output with 50% duty cycle
- Hardware NCO for 12+ Hz (zero CPU overhead)
- Software timing for 1-11 Hz
- Logarithmic frequency sweep via potentiometer
- Step mode for single pulses

## Building

1. Run the configure script:
   - Windows: `.\configure.ps1`
   - Linux/macOS: `./configure`

2. Build: `make`

3. Flash: `make flash`

## Requirements

- Microchip XC8 compiler
- MPLAB X IDE (for IPE and DFP)
- PICkit 5 (or compatible programmer)
- PIC16F1xxxx_DFP device family pack

## License

This project is intentionally available under a restrictive license: CC BY-NC-SA.
Commercial use of this work is not permitted without prior agreement. Please contact
me if you need alternate licensing terms. See [LICENSE.txt](LICENSE.txt) for details.