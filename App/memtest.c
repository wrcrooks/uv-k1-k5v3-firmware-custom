/* Minimal, standalone diagnostic — see memtest.h. */

#include "memtest.h"

#include <string.h>

#include "driver/backlight.h"
#include "driver/py25q16.h"
#include "driver/st7565.h"
#include "driver/system.h"
#include "external/printf/printf.h"
#include "ui/helper.h"

// Small font is ~6-7px wide; keep every line under ~17 chars so nothing
// clips or wraps onto the next line at this display width.
void MEMTEST_Run(void)
{
    BACKLIGHT_TurnOn();

    char line[20];

    // --- Stage 1: JEDEC ID (polled, single-byte SPI, no DMA) ---
    uint8_t id[3] = {0, 0, 0};
    PY25Q16_ReadJedecID(id);

    UI_DisplayClear();
    UI_PrintStringSmallNormal("MEMTEST", 2, 127, 0);

    sprintf(line, "ID:%02X %02X %02X", id[0], id[1], id[2]);
    UI_PrintStringSmallNormal(line, 2, 127, 1);

    if (id[0] == 0x00 || id[0] == 0xFF)
    {
        UI_PrintStringSmallNormal("No response", 2, 127, 2);
    }
    else if (id[2] >= 16 && id[2] <= 26)
    {
        // Standard JEDEC convention: capacity = 2^(last ID byte) bytes.
        uint32_t capacity_kb = (1UL << id[2]) / 1024UL;
        sprintf(line, "Cap:%lu KB", (unsigned long)capacity_kb);
        UI_PrintStringSmallNormal(line, 2, 127, 2);
    }
    else
    {
        UI_PrintStringSmallNormal("Non-std ID", 2, 127, 2);
    }

    // --- Stage 2: bulk DMA-based read (PY25Q16_ReadBuffer with Size >= 16
    // routes through SPI_ReadBuf, a completely different code path than the
    // polled JEDEC ID read above). If the firmware hangs here, the screen
    // freezes on "DMA read..." and goes no further - that by itself tells
    // us the DMA transfer-complete wait never returns. ---
    UI_PrintStringSmallNormal("DMA read...", 2, 127, 3);
    ST7565_BlitFullScreen();
    SYSTEM_DelayMs(1500);   // let stage 1's result stay visible for a beat

    uint8_t buf[32];
    memset(buf, 0, sizeof(buf));
    PY25Q16_ReadBuffer(0x010000, buf, sizeof(buf));  // calibration region, read-only

    // Full redraw (not just overwriting line 3) so there's no leftover
    // pixels from "DMA read..." showing through the new text.
    UI_DisplayClear();
    UI_PrintStringSmallNormal("MEMTEST", 2, 127, 0);
    sprintf(line, "ID:%02X %02X %02X", id[0], id[1], id[2]);
    UI_PrintStringSmallNormal(line, 2, 127, 1);
    UI_PrintStringSmallNormal("DMA read: OK", 2, 127, 3);
    sprintf(line, "Bytes:%02X %02X %02X", buf[0], buf[1], buf[2]);
    UI_PrintStringSmallNormal(line, 2, 127, 4);
    UI_PrintStringSmallNormal("Safe to retry", 2, 127, 6);

    ST7565_BlitFullScreen();

    for (;;)
        SYSTEM_DelayMs(1000);
}
