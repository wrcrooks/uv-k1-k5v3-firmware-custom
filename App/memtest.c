/* Minimal, standalone diagnostic — see memtest.h. */

#include "memtest.h"

#include <string.h>

#include "driver/backlight.h"
#include "driver/py25q16.h"
#include "driver/st7565.h"
#include "driver/system.h"
#include "external/printf/printf.h"
#include "ui/helper.h"

static bool all_ff(const uint8_t *buf, unsigned n)
{
    for (unsigned i = 0; i < n; i++)
        if (buf[i] != 0xFF)
            return false;
    return true;
}

// Small font is ~6-7px wide; keep every line under ~17 chars so nothing
// clips or wraps onto the next line at this display width.
void MEMTEST_Run(void)
{
    BACKLIGHT_TurnOn();

    char line[20];

    // --- Stage 1: JEDEC ID (polled, single-byte SPI, no DMA) ---
    uint8_t id[3] = {0, 0, 0};
    PY25Q16_ReadJedecID(id);

    // --- Stage 2: read the exact calibration sub-fields
    // SETTINGS_LoadCalibration() reads (settings.c), to check whether
    // calibration data is genuinely blank/erased on this unit or whether an
    // earlier single-address probe just landed on an unused gap. Read-only;
    // matches addresses already read unconditionally on every normal boot. */
    uint8_t rssi1[8], rssi2[8], batt[12], misc[8];
    PY25Q16_ReadBuffer(0x010000 + 0xc0, rssi1, sizeof(rssi1));
    PY25Q16_ReadBuffer(0x010000 + 0xc8, rssi2, sizeof(rssi2));
    PY25Q16_ReadBuffer(0x010000 + 0x140, batt, sizeof(batt));
    PY25Q16_ReadBuffer(0x010000 + 0x188, misc, sizeof(misc));

    UI_DisplayClear();
    UI_PrintStringSmallNormal("MEMTEST", 2, 127, 0);

    sprintf(line, "ID:%02X %02X %02X", id[0], id[1], id[2]);
    UI_PrintStringSmallNormal(line, 2, 127, 1);

    sprintf(line, "RSSI1:%02X%s", rssi1[0], all_ff(rssi1, sizeof(rssi1)) ? " BLANK" : " OK");
    UI_PrintStringSmallNormal(line, 2, 127, 2);

    sprintf(line, "RSSI2:%02X%s", rssi2[0], all_ff(rssi2, sizeof(rssi2)) ? " BLANK" : " OK");
    UI_PrintStringSmallNormal(line, 2, 127, 3);

    sprintf(line, "BATT:%02X%s", batt[0], all_ff(batt, sizeof(batt)) ? " BLANK" : " OK");
    UI_PrintStringSmallNormal(line, 2, 127, 4);

    sprintf(line, "MISC:%02X%s", misc[0], all_ff(misc, sizeof(misc)) ? " BLANK" : " OK");
    UI_PrintStringSmallNormal(line, 2, 127, 5);

    // --- Stage 3: a real flash WRITE, not just reads. All calibration
    // fields came back blank, meaning the very first normal boot has to
    // write default channel attributes (2 bytes each, at
    // FLASH_CHANNEL_ATTR_BASE = 0x8000) for all ~1031 channels in a tight
    // loop. That write path goes through WaitWIP(), which polls the flash
    // status register via the same low-level polled SPI primitive used by
    // stage 1's JEDEC read - but stage 1 never issued a WRITE/erase command,
    // only reads, so this is a genuinely untested code path. This writes to
    // channel 0's own attribute slot: the exact address, and the exact
    // operation, the real firmware performs on every boot regardless -
    // no additional risk beyond what already happens today. If this hangs,
    // the screen freezes on "Write test..." below and never updates. */
    UI_PrintStringSmallNormal("Write test...", 2, 127, 6);
    ST7565_BlitFullScreen();
    SYSTEM_DelayMs(1500);   // let stage 1/2 results stay visible for a beat

    uint8_t wr[2] = {0x00, 0x07};   // mirrors att->__val=0, att->band=7
    PY25Q16_WriteBuffer(0x8000, wr, sizeof(wr), false);

    uint8_t rb[2] = {0, 0};
    PY25Q16_ReadBuffer(0x8000, rb, sizeof(rb));

    sprintf(line, "Write OK: %02X %02X", rb[0], rb[1]);
    UI_PrintStringSmallNormal(line, 2, 127, 6);

    ST7565_BlitFullScreen();

    for (;;)
        SYSTEM_DelayMs(1000);
}
