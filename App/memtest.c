/* Minimal, standalone diagnostic — see memtest.h. */

#include "memtest.h"

#include "driver/backlight.h"
#include "driver/py25q16.h"
#include "driver/st7565.h"
#include "driver/system.h"
#include "external/printf/printf.h"
#include "ui/helper.h"

void MEMTEST_Run(void)
{
    BACKLIGHT_TurnOn();

    uint8_t id[3] = {0, 0, 0};
    PY25Q16_ReadJedecID(id);

    char line[22];

    UI_DisplayClear();

    UI_PrintStringSmallNormal("MEMTEST - SPI FLASH", 2, 127, 0);

    sprintf(line, "MFG:%02X TYPE:%02X CAP:%02X", id[0], id[1], id[2]);
    UI_PrintStringSmallNormal(line, 2, 127, 2);

    if (id[0] == 0x00 || id[0] == 0xFF)
    {
        UI_PrintStringSmallNormal("No response (0x00/0xFF)", 2, 127, 4);
        UI_PrintStringSmallNormal("Chip not detected", 2, 127, 5);
    }
    else
    {
        // Standard JEDEC convention: capacity = 2^(last ID byte) bytes.
        // Only meaningful if the byte is in a sane range for a flash chip
        // (roughly 64KB to 64MB); anything else means this chip doesn't
        // follow the convention and the raw ID above is what to go by.
        if (id[2] >= 16 && id[2] <= 26)
        {
            uint32_t capacity_kb = (1UL << id[2]) / 1024UL;
            sprintf(line, "Capacity: ~%lu KB", (unsigned long)capacity_kb);
            UI_PrintStringSmallNormal(line, 2, 127, 4);
            sprintf(line, "(%lu Mbit, JEDEC guess)", (unsigned long)(capacity_kb / 128UL));
            UI_PrintStringSmallNormal(line, 2, 127, 5);
        }
        else
        {
            UI_PrintStringSmallNormal("Non-standard ID byte", 2, 127, 4);
            UI_PrintStringSmallNormal("(see MFG/TYPE/CAP above)", 2, 127, 5);
        }
    }

    UI_PrintStringSmallNormal("Read-only - safe to retry", 2, 127, 6);

    ST7565_BlitFullScreen();

    for (;;)
        SYSTEM_DelayMs(1000);
}
