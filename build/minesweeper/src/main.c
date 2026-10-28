#include "menu.h"
#include "utils.h"
#include "keyboard.h"
#include "personal_best.h"
#include <fxcg/display.h>
#include <fxcg/keyboard.h>
#include <fxcg/heap.h>
#include <fxcg/system.h>
#include <fxcg/rtc.h>
#include "../../common/main_menu.h"

int main() {
    Bdisp_AllClr_VRAM();

    Bdisp_EnableColor(true);

    char col = TEXT_COLOR_WHITE;
    DefineStatusAreaFlags(3, SAF_BATTERY, &col, &col);

    unsigned int _ = 0;
    unsigned int millisecond = 0;
    RTC_GetTime(&_, &_, &_, &millisecond);
    Utils_srand(millisecond);

    Utils_initKeyToNumberMap();


    while (true) {
        Menu* menu = sys_malloc(sizeof(Menu));
        Menu_create(menu);

        bool redraw = true;
        unsigned int lastStatusTicks = (unsigned int)RTC_GetTicks();
        while (true) {
            Key_update();
            int key = Key_pressed();
            if (key) {
                Menu_handleKeypress(menu, key);
                redraw = true;
            }

            // File I/O stays outside reveal recursion and precedes the success label.
            if (!PersonalBest_flush()) {
                if (menu->board) menu->board->newPersonalBest = false;
                menu->notification = "Best not saved!";
                redraw = true;
            }

            unsigned int now = (unsigned int)RTC_GetTicks();
            Board* board = menu->board;
            if (board && board->highlightTicks >= 0 && RTC_Elapsed_ms(board->highlightTicks, 900)) redraw = true;
            if (redraw) {
                Bdisp_AllClr_VRAM();
                Menu_draw(menu);
            }

            unsigned int statusInterval = board && !board->firstReveal && !board->won && !board->lost ? 13u : 128u;
            if (redraw || now - lastStatusTicks >= statusInterval) {
                DisplayStatusArea();
                Menu_drawStatusArea(menu);
                Bdisp_PutDisp_DD();
                lastStatusTicks = now;
                redraw = false;
            }

            if (key == KEY_PRGM_MENU) break;
            OS_InnerWait_ms(10);
        }

        Menu_free(menu);
        // Keep the OS add-in context resumable when this icon is opened again.
        openMainMenu();
    }

    return 0;
}
