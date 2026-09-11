/* Minimal, standalone diagnostic — see memtest.h.
 *
 * Stages 1-3 (JEDEC ID, calibration sub-fields, a single channel-attribute
 * write+readback) all passed on the test unit this was built for: the
 * external SPI flash chip is correctly sized (2048 KB) and responds
 * correctly to both polled reads, DMA bulk reads, and a real write. All
 * calibration data came back blank (0xFF), meaning the chip has likely
 * never been initialized by this firmware family before.
 *
 * Stage 4 runs the REAL boot sequence from App/main.c's Main() - the exact
 * same functions, in the exact same order - one at a time, updating the
 * screen before and after each. Wherever the radio actually freezes
 * pinpoints the exact function call responsible, rather than guessing at
 * mechanisms one hypothesis at a time. */

#include "memtest.h"

#include <string.h>

#include "board.h"
#include "driver/backlight.h"
#include "driver/bk4819.h"
#include "driver/py25q16.h"
#include "driver/st7565.h"
#include "driver/system.h"
#include "external/printf/printf.h"
#include "helper/battery.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
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
static void show_step(const char *step, uint8_t n, uint8_t total)
{
    char line[20];
    UI_DisplayClear();
    UI_PrintStringSmallNormal("MEMTEST BOOT TRACE", 2, 127, 0);
    sprintf(line, "Step %u/%u:", n, total);
    UI_PrintStringSmallNormal(line, 2, 127, 2);
    UI_PrintStringSmallNormal(step, 2, 127, 3);
    UI_PrintStringSmallNormal("(frozen here = culprit)", 2, 127, 5);
    ST7565_BlitFullScreen();
}

void MEMTEST_Run(void)
{
    BACKLIGHT_TurnOn();

    char line[20];

    // --- Stages 1-3: already-verified low-level flash checks (kept brief
    // since they passed before; see git history for the detailed version) ---
    uint8_t id[3] = {0, 0, 0};
    PY25Q16_ReadJedecID(id);

    uint8_t rssi1[8];
    PY25Q16_ReadBuffer(0x010000 + 0xc0, rssi1, sizeof(rssi1));

    uint8_t wr[2] = {0x00, 0x07};
    PY25Q16_WriteBuffer(0x8000, wr, sizeof(wr), false);
    uint8_t rb[2] = {0, 0};
    PY25Q16_ReadBuffer(0x8000, rb, sizeof(rb));

    UI_DisplayClear();
    UI_PrintStringSmallNormal("MEMTEST", 2, 127, 0);
    sprintf(line, "ID:%02X %02X %02X", id[0], id[1], id[2]);
    UI_PrintStringSmallNormal(line, 2, 127, 1);
    sprintf(line, "Calib:%s Wr:%02X%02X", all_ff(rssi1, sizeof(rssi1)) ? "BLANK" : "OK", rb[0], rb[1]);
    UI_PrintStringSmallNormal(line, 2, 127, 2);
    UI_PrintStringSmallNormal("Starting boot trace", 2, 127, 4);
    ST7565_BlitFullScreen();
    SYSTEM_DelayMs(2000);

    // --- Stage 4: the REAL boot sequence, one real function at a time ---
    const uint8_t TOTAL = 8;

    show_step("BK4819_Init", 1, TOTAL);
    BK4819_Init();
    SYSTEM_DelayMs(400);

    show_step("ADC_GetBatteryInfo", 2, TOTAL);
    BOARD_ADC_GetBatteryInfo(&gBatteryCurrentVoltage, &gBatteryCurrent);
    SYSTEM_DelayMs(400);

    show_step("InitEEPROM (slow)", 3, TOTAL);
    SETTINGS_InitEEPROM();
    SYSTEM_DelayMs(400);

    show_step("LoadCalibration", 4, TOTAL);
    SETTINGS_LoadCalibration();
    SYSTEM_DelayMs(400);

    show_step("ConfigureChannel 0", 5, TOTAL);
    RADIO_ConfigureChannel(0, VFO_CONFIGURE_RELOAD);
    SYSTEM_DelayMs(400);

    show_step("ConfigureChannel 1", 6, TOTAL);
    RADIO_ConfigureChannel(1, VFO_CONFIGURE_RELOAD);
    SYSTEM_DelayMs(400);

    show_step("SelectVfos", 7, TOTAL);
    RADIO_SelectVfos();
    SYSTEM_DelayMs(400);

    show_step("SetupRegisters", 8, TOTAL);
    RADIO_SetupRegisters(true);
    SYSTEM_DelayMs(400);

    UI_DisplayClear();
    UI_PrintStringSmallNormal("MEMTEST", 2, 127, 0);
    UI_PrintStringSmallNormal("ALL 8 STEPS OK", 2, 127, 2);
    UI_PrintStringSmallNormal("Boot path completes", 2, 127, 4);
    ST7565_BlitFullScreen();

    for (;;)
        SYSTEM_DelayMs(1000);
}
