/* Minimal, standalone diagnostic: probe the external SPI flash chip's JEDEC
 * ID (a read-only command every SPI NOR flash chip supports) and show it on
 * screen, without running any of the normal boot sequence (EEPROM/settings,
 * calibration, radio register setup) that might be the actual crash cause
 * on a given unit. Never touches (reads or writes) any user data. */
#ifndef MEMTEST_H
#define MEMTEST_H

void MEMTEST_Run(void);

#endif
