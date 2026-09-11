/* See boot_trace.h. */

#include "boot_trace.h"

#ifdef ENABLE_BOOT_TRACE

#include "driver/backlight.h"
#include "driver/st7565.h"
#include "driver/system.h"
#include "external/printf/printf.h"
#include "ui/helper.h"

void BOOT_TRACE_Step(const char *step)
{
    BACKLIGHT_TurnOn();
    UI_DisplayClear();
    UI_PrintStringSmallNormal("BOOTTRACE", 2, 127, 0);
    UI_PrintStringSmallNormal(step, 2, 127, 3);
    ST7565_BlitFullScreen();
    SYSTEM_DelayMs(250);
}

void BOOT_TRACE_Heartbeat(void)
{
    static uint32_t count = 0;
    char line[20];

    count++;
    UI_DisplayClear();
    UI_PrintStringSmallNormal("BOOTTRACE", 2, 127, 0);
    sprintf(line, "Alive: %lu", (unsigned long)count);
    UI_PrintStringSmallNormal(line, 2, 127, 3);
    ST7565_BlitFullScreen();
}

#else

void BOOT_TRACE_Step(const char *step)
{
    (void)step;
}

void BOOT_TRACE_Heartbeat(void)
{
}

#endif
