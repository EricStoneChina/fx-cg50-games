#include "board.h"
#include "personal_best.h"
#include "utils.h"
#include <fxcg/heap.h>
#include <fxcg/keyboard.h>
#include <fxcg/display.h>
#include <fxcg/rtc.h>
#include <fxcg/misc.h>
#include <fxcg/system.h>

void _drawHighlight(Board* board, int row, int col, void* data);

static void Board_updateElapsedTime(Board* board) {
    if (board->firstReveal || board->won || board->lost || board->timerStartTicks < 0) return;

    // RTC ticks run at 128 Hz. Unsigned subtraction also handles tick wraparound.
    unsigned int elapsedTicks = (unsigned int)RTC_GetTicks() - (unsigned int)board->timerStartTicks;
    board->centiseconds = (int)((elapsedTicks * 10u) / 128u);
}

void _incrementMineCount(Board* board, int row, int col, void* mineCount) {
    if (*Board_getCell(board, row, col) == (kTileTypeMine | COVER_TILE_BIT)) {
        (*(char*)mineCount)++;
    }
}

static void Board_createInternal(Board* board, int width, int height, int mines, bool fake, int safeRow, int safeCol) {
    if (width > BOARD_MAX_WIDTH) width = BOARD_MAX_WIDTH;
    if (height > BOARD_MAX_HEIGHT) height = BOARD_MAX_HEIGHT;

    board->width = width;
    board->height = height;
    board->mines = mines;

    board->row = board->height / 2;
    board->col = board->width / 2;

    board->offsetX = (LCD_WIDTH_PX / 2) - (width * 24 / 2);
    board->offsetY = ((LCD_HEIGHT_PX - 24) / 2) - (height * 24 / 2);

    board->shakeX = 0;
    board->shakeY = 0;

    board->won = false;
    board->lost = false;
    board->newPersonalBest = false;

    board->firstReveal = true;


    board->highlightRow = -1;
    board->highlightCol = -1;
    board->highlightTicks = -1;

    board->timerStartTicks = -1;
    board->centiseconds = 0;

    board->konamiCodeIndex = 0;

    // calloc reimplementation (sys_calloc crashes???)
    board->data = sys_malloc(width * height);
    for (int i = 0; i < width * height; i++) {
        board->data[i] = 0;
    }

    // Build and shuffle a finite list of valid mine positions. This always
    // terminates, including on tiny and very dense boards.
    int safeArea = 0;
    if (safeRow >= 0 && safeCol >= 0) {
        int top = safeRow > 0 ? safeRow - 1 : safeRow;
        int bottom = safeRow + 1 < height ? safeRow + 1 : safeRow;
        int left = safeCol > 0 ? safeCol - 1 : safeCol;
        int right = safeCol + 1 < width ? safeCol + 1 : safeCol;
        safeArea = (bottom - top + 1) * (right - left + 1);
    }
    bool clearNeighbours = safeRow >= 0 && width * height - safeArea >= mines;

    int candidates[BOARD_MAX_WIDTH * BOARD_MAX_HEIGHT];
    int candidateCount = 0;
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            if (row == safeRow && col == safeCol) continue;
            if (clearNeighbours && row >= safeRow - 1 && row <= safeRow + 1 && col >= safeCol - 1 && col <= safeCol + 1) continue;
            candidates[candidateCount++] = row * width + col;
        }
    }

    if (mines > candidateCount) mines = candidateCount;
    board->mines = mines;
    for (int i = candidateCount - 1; i > 0; i--) {
        int swap = Utils_randrange(0, i + 1);
        int temp = candidates[i];
        candidates[i] = candidates[swap];
        candidates[swap] = temp;
    }
    for (int i = 0; i < mines; i++) {
        board->data[candidates[i]] = kTileTypeMine | COVER_TILE_BIT;
    }

    // and place other tiles around the mines
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            char* cell = Board_getCell(board, row, col);

            if (*cell == (kTileTypeMine | COVER_TILE_BIT)) continue;

            char mineCount = 0;
            Board_runForSurroundingCells(board, row, col, _incrementMineCount, &mineCount);

            *cell = mineCount | COVER_TILE_BIT;
        }
    }

}

void Board_create(Board* board, int width, int height, int mines, bool fake) {
    Board_createInternal(board, width, height, mines, fake, -1, -1);
}

void Board_createSafe(Board* board, int width, int height, int mines, bool fake, int safeRow, int safeCol) {
    Board_createInternal(board, width, height, mines, fake, safeRow, safeCol);
}

void Board_free(Board* board) {
    sys_free(board->data);
    sys_free(board);
}

void Board_draw(Board* board) {
    // draw board
    for (int row = 0; row < board->height; row++) {
        for (int col = 0; col < board->width; col++) {
            int x = 24*col + board->offsetX + board->shakeX;
            int y = 24*row + board->offsetY + board->shakeY;

            if (x < -24 || y < -24 || x > LCD_WIDTH_PX || y > LCD_HEIGHT_PX) continue;

            Utils_drawSpriteAtPos(*Board_getCell(board, row, col), x, y);
        }
    }

    // A failed chord attempt briefly marks the covered, unflagged neighbours
    // so the player can see which cells may still contain mines.
    if (board->highlightTicks >= 0) {
        if (RTC_Elapsed_ms(board->highlightTicks, 900)) {
            board->highlightTicks = -1;
        } else {
            Board_runForSurroundingCells(board, board->highlightRow, board->highlightCol, _drawHighlight, 0x0);
        }
    }

    // draw cursor (maybe make this a sprite or look better in general?)
    int x = 24 * board->col + board->offsetX + board->shakeX;
    int y = 24 * board->row + board->offsetY + board->shakeY;
    Bdisp_Rectangle(
        x, y,
        x + 24, y + 24,
        TEXT_COLOR_BLACK
    );

    Bdisp_Rectangle(
        x - 1, y - 1,
        x + 25, y + 25,
        TEXT_COLOR_BLACK
    );

}

void Board_drawStatusArea(Board* board) {
    Board_updateElapsedTime(board);
    int x = 24;
    int y = 2;

    unsigned char buf[12];
    bool complete = board->won || board->lost;

    if (complete) {
        PrintMini(&x, &y, board->won ? "Won " : "Lost ", 1 << 6, 0xffffffff, 0, 0, COLOR_BLACK, COLOR_WHITE, true, 0);
    } else {
        int flagCount = 0;
        for (int i = 0; i < board->width * board->height; i++) {
            if (board->data[i] & FLAG_TILE_BIT) flagCount++;
        }
        PrintMini(&x, &y, "Flags ", 1 << 6, 0xffffffff, 0, 0, COLOR_BLACK, COLOR_WHITE, true, 0);
        Utils_clearAndFillBuffer(buf, flagCount);
        PrintMini(&x, &y, (const char*)buf, 1 << 6, 0xffffffff, 0, 0, COLOR_NAVY, COLOR_WHITE, true, 0);
        PrintMini(&x, &y, "/", 1 << 6, 0xffffffff, 0, 0, COLOR_BLACK, COLOR_WHITE, true, 0);
        Utils_clearAndFillBuffer(buf, board->mines);
        PrintMini(&x, &y, (const char*)buf, 1 << 6, 0xffffffff, 0, 0, COLOR_NAVY, COLOR_WHITE, true, 0);
        PrintMini(&x, &y, "  ", 1 << 6, 0xffffffff, 0, 0, COLOR_BLACK, COLOR_WHITE, true, 0);
    }

    Utils_clearAndFillBuffer(buf, board->centiseconds / 10);
    PrintMini(&x, &y, (const char*)buf, 1 << 6, 0xffffffff, 0, 0, COLOR_NAVY, COLOR_WHITE, true, 0);
    PrintMini(&x, &y, ".", 1 << 6, 0xffffffff, 0, 0, COLOR_BLACK, COLOR_WHITE, true, 0);
    Utils_clearAndFillBuffer(buf, board->centiseconds % 10);
    PrintMini(&x, &y, (const char*)buf, 1 << 6, 0xffffffff, 0, 0, COLOR_NAVY, COLOR_WHITE, true, 0);
    PrintMini(&x, &y, "s", 1 << 6, 0xffffffff, 0, 0, COLOR_BLACK, COLOR_WHITE, true, 0);
    if (board->newPersonalBest) {
        PrintMini(&x, &y, " NEW PB!", 1 << 6, 0xffffffff, 0, 0, COLOR_RED, COLOR_WHITE, true, 0);
    }
    if (complete) {
        PrintMini(&x, &y, " EXE/EXIT", 1 << 6, 0xffffffff, 0, 0, COLOR_BLACK, COLOR_WHITE, true, 0);
    }
}

int code[7] = {
    KEY_PRGM_8,
    KEY_PRGM_0,
    KEY_PRGM_0,
    KEY_PRGM_8,
    KEY_PRGM_1,
    KEY_PRGM_3,
    KEY_PRGM_5
};

// returns true if the game should be exited back to the setup screen
bool Board_handleKeypress(Board* board, int key) {
    if (board->won || board->lost) {
        if (key == KEY_PRGM_RETURN || key == KEY_PRGM_EXIT) return true;
        // Inspect the final board without changing cells or dismissing the result.
        if (key != KEY_PRGM_UP && key != KEY_PRGM_DOWN &&
            key != KEY_PRGM_LEFT && key != KEY_PRGM_RIGHT) return false;
    }

    if (key) board->highlightTicks = -1;

    if (!board->won && !board->lost && key == code[board->konamiCodeIndex]) {
        board->konamiCodeIndex++;

        if (board->konamiCodeIndex == 7) {
            Board_onGameComplete(board, true);
            return false;
        }
    }

    switch (key) {
        case KEY_PRGM_UP: {
            board->row--;
            if (board->row < 0) board->row = board->height - 1;
            Board_updateOffset(board);
        } break;

        case KEY_PRGM_DOWN: {
            board->row++;
            if (board->row > board->height - 1) board->row = 0;
            Board_updateOffset(board);
        } break;

        case KEY_PRGM_LEFT: {
            board->col--;
            if (board->col < 0) board->col = board->width - 1;
            Board_updateOffset(board);
        } break;

        case KEY_PRGM_RIGHT: {
            board->col++;
            if (board->col > board->width - 1) board->col = 0;
            Board_updateOffset(board);
        } break;

        case KEY_PRGM_F1:
        case KEY_PRGM_ALPHA: {
            Board_flag(board, board->row, board->col);
        } break;

        case KEY_PRGM_F6:
        case KEY_PRGM_SHIFT: {
            Board_revealSingleCell(board, board->row, board->col);
        } break;

        case KEY_PRGM_EXIT: {
            return true;
        }

        case KEY_PRGM_ACON: {
            return true;
        }
    }

    return false;
}

void Board_updateOffset(Board* board) {
    // Only scroll when the board is actually larger than the screen.
    // The old threshold (width*24+24 > LCD_*) wrongly treated a 9x9 board
    // (216 px tall == screen height) as overflowing, which made the whole
    // board drift on every arrow key press. Now the cursor tile is always
    // kept fully visible and scrolling moves in whole tiles, so a board
    // that fits on screen never scrolls at all.
    const int tile = 24;

    if (board->width * tile > LCD_WIDTH_PX) {
        int left = tile * board->col + board->offsetX + board->shakeX;
        int right = left + tile;

        while (left < 0) {
            board->offsetX += tile;
            left = tile * board->col + board->offsetX + board->shakeX;
        }
        while (right > LCD_WIDTH_PX) {
            board->offsetX -= tile;
            right = tile * board->col + board->offsetX + board->shakeX + tile;
        }
    }

    if (board->height * tile > LCD_HEIGHT_PX) {
        int top = tile * board->row + board->offsetY + board->shakeY;
        int bottom = top + tile;

        while (top < 0) {
            board->offsetY += tile;
            top = tile * board->row + board->offsetY + board->shakeY;
        }
        while (bottom > LCD_HEIGHT_PX) {
            board->offsetY -= tile;
            bottom = tile * board->row + board->offsetY + board->shakeY + tile;
        }
    }
}

void Board_flag(Board* board, int row, int col) {
    char* cell = Board_getCell(board, row, col);

    if (!Board_cellIsCovered(cell)) {
        // you shouldnt be able to flag revealed cells
        return;
    }

    *cell ^= FLAG_TILE_BIT;
}

void _revealCellIfNotFlagged(Board* board, int row, int col, void* _unused) {
    if (board->won || board->lost) return;
    char* cell = Board_getCell(board, row, col);
    if (!Board_cellIsFlagged(cell) && Board_cellIsCovered(cell)) {
        Board_revealSingleCell(board, row, col);
    }
}

void _incrementFlagCount(Board* board, int row, int col, void* flagCount) {
    if (*Board_getCell(board, row, col) & FLAG_TILE_BIT) {
        (*(char*)flagCount)++;
    }
}

void _drawHighlight(Board* board, int row, int col, void* _unused) {
    char* cell = Board_getCell(board, row, col);
    if (!Board_cellIsCovered(cell) || Board_cellIsFlagged(cell)) return;

    int x = 24 * col + board->offsetX + board->shakeX;
    int y = 24 * row + board->offsetY + board->shakeY;
    if (x < -24 || y < -24 || x > LCD_WIDTH_PX || y > LCD_HEIGHT_PX) return;

    Bdisp_Rectangle(x + 1, y + 1, x + 23, y + 23, TEXT_COLOR_YELLOW);
}

void _highlightSurroundingCells(Board* board, int row, int col) {
    board->highlightRow = row;
    board->highlightCol = col;
    board->highlightTicks = RTC_GetTicks();
}

void Board_revealSingleCell(Board* board, int row, int col) {
    if (board->won || board->lost) return;

    board->highlightTicks = -1;
    char* cell = Board_getCell(board, row, col);

    if (Board_cellIsFlagged(cell)) {
        *cell ^= FLAG_TILE_BIT;
        return;
    }

    if (board->firstReveal) {
        // oooh exciting exciting
        board->firstReveal = false;

        board->timerStartTicks = RTC_GetTicks();

        // Generate once with a guaranteed safe first cell. If the board has
        // room, its eight neighbours are safe too, making the first cell zero.
        Board* newBoard = sys_malloc(sizeof(Board));
        Board_createSafe(newBoard, board->width, board->height, board->mines, /* fake */ true, row, col);
        for (int i = 0; i < board->width * board->height; i++) {
            newBoard->data[i] |= board->data[i] & FLAG_TILE_BIT;
        }
        sys_free(board->data);
        board->data = newBoard->data;
        board->mines = newBoard->mines;
        sys_free(newBoard); // keep its data, now owned by board
        cell = Board_getCell(board, row, col);
    }

    // If it's not covered, use the standard Minesweeper chord action: when the
    // flag count matches the number, reveal the remaining covered neighbours.
    // Otherwise show those possible mine cells briefly instead of doing
    // nothing, so a mistaken or incomplete flag count is visible to the player.
    if (!Board_cellIsCovered(cell)) {
        char flagCount = 0;
        Board_runForSurroundingCells(board, row, col, _incrementFlagCount, &flagCount);

        if (*cell == flagCount) {
            Board_runForSurroundingCells(board, row, col, _revealCellIfNotFlagged, 0x0);
        } else {
            _highlightSurroundingCells(board, row, col);
        }

        return;
    }

    *cell &= ~COVER_TILE_BIT; // unset covered bit
    *cell &= ~FLAG_TILE_BIT;

    if (*cell == kTileTypeMine) {
        // uh oh!
        Board_kablooey(board, row, col);
        return;
    }

    Board_checkWinCondition(board);

    // Win may have just fired mid-cascade; do not keep flooding after settle.
    if (board->won || board->lost) return;

    if (*cell == kTileTypeZero) {
        // reveal surrounding cells
        Board_revealSurroundingCells(board, row, col);
        return;
    }
}

void Board_kablooey(Board* board, int row, int col) {
    if (board->won || board->lost) return;

    Board_onGameComplete(board, false);

    // set certain tiles to special ones
    for (int i = 0; i < board->width * board->height; i++) {
        char* cell = &board->data[i];

        // mark all incorrect flags
        if (Board_cellIsFlagged(cell) && *cell != (kTileTypeMine | COVER_TILE_BIT | FLAG_TILE_BIT)) {
            *cell = kTileTypeIncorrectFlag;
            continue;
        }

        // and uncover all mines
        if (*cell == (kTileTypeMine | COVER_TILE_BIT)) {
            *cell &= ~COVER_TILE_BIT;
            continue;
        }
    }

    (*Board_getCell(board, row, col)) = kTileTypeHitMine;
}

void Board_onGameComplete(Board* board, bool won) {
    // Only enter settle once. Flood/chord can keep calling after the last cell.
    if (board->won || board->lost) return;

    Board_updateElapsedTime(board);
    if (won) {
        board->won = true;
        // Memory-only PB update. File write is PersonalBest_flush() from the
        // main loop, never from inside reveal recursion (stack + Bfile TLB).
        // The hidden completion sequence is not eligible for personal bests.
        if (board->konamiCodeIndex != 7) {
            board->newPersonalBest = PersonalBest_record(board->width, board->height, board->mines, board->centiseconds);
        }
    }
    else     board->lost = true;
}

void Board_revealSurroundingCells(Board* board, int row, int col) {
    if (board->won || board->lost) return;
    Board_runForSurroundingCells(board, row, col, _revealCellIfNotFlagged, 0x0);
}

void Board_runForSurroundingCells(Board* board, int row, int col, void(*callback)(Board*, int, int, void*), void* data) {
    int width = board->width;
    int height = board->height;

    if (row != 0) {
        if (col != 0) { callback(board, row - 1, col - 1, data); }
        callback(board, row - 1, col, data);
        if (col != width - 1) { callback(board, row - 1, col + 1, data); }
    }

    if (col != 0) { callback(board, row, col - 1, data); }
    if (col != width - 1) { callback(board, row, col + 1, data); }

    if (row != height - 1) {
        if (col != 0) { callback(board, row + 1, col - 1, data); }
        callback(board, row + 1, col, data);
        if (col != width - 1) { callback(board, row + 1, col + 1, data); }
    }
}

bool Board_cellIsFlagged(char* cell) {
    return *cell & FLAG_TILE_BIT;
}

bool Board_cellIsCovered(char* cell) {
    return *cell & COVER_TILE_BIT;
}

void Board_checkWinCondition(Board* board) {
    if (board->won || board->lost) return;

    // if any cells are covered still and aren't mines, return
    for (int i = 0; i < board->width * board->height; i++) {
        if (
            Board_cellIsCovered(&board->data[i])
        &&  board->data[i] != (kTileTypeMine | COVER_TILE_BIT)
        &&  board->data[i] != (kTileTypeMine | COVER_TILE_BIT | FLAG_TILE_BIT)
        ) return;
    }

    // else you win
    Board_onGameComplete(board, true);
}

char* Board_getCell(Board* board, int row, int col) {
    return &board->data[row*board->width + col];
}
