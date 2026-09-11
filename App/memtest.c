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

#include "app/app.h"
#include "board.h"
#include "driver/backlight.h"
#include "driver/bk4819.h"
#include "driver/py25q16.h"
#include "driver/st7565.h"
#include "driver/system.h"
#include "external/printf/printf.h"
#include "functions.h"
#include "helper/battery.h"
#include "helper/boot.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/helper.h"
#include "ui/menu.h"
#include "ui/welcome.h"

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
    const uint8_t TOTAL = 16;

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

    // --- Stage 5: the rest of Main(), up to (not including) the permanent
    // while(true) scheduler loop. Mirrors App/main.c's logic; not a
    // byte-identical copy (several ENABLE_* branches in the real Main() are
    // skipped here for simplicity), but every function call below is the
    // real one, in the real order, with the real battery-level branch. ---
    show_step("Battery avg loop", 9, TOTAL);
    for (unsigned int i = 0; i < ARRAY_SIZE(gBatteryVoltages); i++)
        BOARD_ADC_GetBatteryInfo(&gBatteryVoltages[i], &gBatteryCurrent);
    BATTERY_GetReadings(false);
    SYSTEM_DelayMs(400);

    show_step("BOOT_GetMode", 10, TOTAL);
    BOOT_Mode_t BootMode = BOOT_GetMode();
    SYSTEM_DelayMs(400);

    show_step("UI_MENU_BuildView", 11, TOTAL);
    UI_MENU_BuildView();
    SYSTEM_DelayMs(400);

    if (!gChargingWithTypeC && gBatteryDisplayLevel == 0)
    {
        show_step("FUNCTION_Select(PS)", 12, TOTAL);
        FUNCTION_Select(FUNCTION_POWER_SAVE);
    }
    else
    {
        show_step("UI_DisplayWelcome", 12, TOTAL);
        UI_DisplayWelcome();
        SYSTEM_DelayMs(400);

        // Real Main() sets this to 250 (2.5s) right at the top; our trace
        // never has, so without setting it here this loop would silently
        // never run even though it does on real hardware. Blank EEPROM
        // decodes POWER_ON_DISPLAY_MODE to VOLTAGE (not NONE/SOUND), so
        // this loop *does* run on this unit - and it has no timeout of its
        // own: it only exits when boot_counter_10ms (decremented once per
        // SysTick tick) reaches 0, or a key is pressed. Untested until now.
        boot_counter_10ms = 250;
        show_step("Boot beep loop", 13, TOTAL);
        while (boot_counter_10ms > 0)
        {
            if (KEYBOARD_Poll() != KEY_INVALID)
            {
                boot_counter_10ms = 0;
                break;
            }
        }
        RADIO_SetupRegisters(true);
    }
    SYSTEM_DelayMs(400);

    show_step("BOOT_ProcessMode", 14, TOTAL);
    BOOT_ProcessMode(BootMode);
    SYSTEM_DelayMs(400);

    show_step("APP_Update x1", 15, TOTAL);
    APP_Update();
    SYSTEM_DelayMs(400);

    show_step("TimeSlice10/500ms", 16, TOTAL);
    APP_TimeSlice10ms();
    APP_TimeSlice500ms();
    SYSTEM_DelayMs(400);

    // --- Stage 6: the REAL, unmodified while(true) scheduler loop from the
    // end of Main() - not a single synthetic pass, but actually run forever,
    // exactly like the real firmware does. A single pass through
    // APP_Update/APP_TimeSlice10ms/APP_TimeSlice500ms above already
    // succeeded; if the real failure only shows up after several seconds of
    // real operation (matching "flashes for a bit, then blank"), it needs
    // to run for real, not just once, to reproduce it. A counter incremented
    // once per APP_TimeSlice500ms() call (~2x/sec) is drawn on screen so a
    // freeze is visible as "the number stopped counting", and roughly how
    // many seconds of real operation it took to get there. ---
    UI_DisplayClear();
    UI_PrintStringSmallNormal("MEMTEST", 2, 127, 0);
    UI_PrintStringSmallNormal("Running real loop", 2, 127, 2);
    UI_PrintStringSmallNormal("(freeze = culprit)", 2, 127, 4);
    ST7565_BlitFullScreen();
    SYSTEM_DelayMs(1500);

    uint32_t heartbeat = 0;
    while (true)
    {
        APP_Update();

        if (gNextTimeslice)
        {
            APP_TimeSlice10ms();

            if (gNextTimeslice_500ms)
            {
                APP_TimeSlice500ms();

                heartbeat++;
                UI_DisplayClear();
                UI_PrintStringSmallNormal("MEMTEST", 2, 127, 0);
                sprintf(line, "Alive: %lu (~%lus)", (unsigned long)heartbeat, (unsigned long)(heartbeat / 2));
                UI_PrintStringSmallNormal(line, 2, 127, 2);
                UI_PrintStringSmallNormal("(freeze = culprit)", 2, 127, 4);
                ST7565_BlitFullScreen();
            }
        }
    }
}
