#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <setjmp.h>
#include <stdint.h>

#define main cg2048_main
#ifndef GAME_SOURCE
#define GAME_SOURCE "../build/2048/src/main.c"
#endif
#include GAME_SOURCE
#undef main

static void check_failed(const char *condition, int line) {
    fprintf(stderr, "FAIL line %d: %s\n", line, condition);
    exit(EXIT_FAILURE);
}
#undef assert
#define assert(condition) ((condition) ? (void)0 : check_failed(#condition, __LINE__))

static color_t frame[LCD_WIDTH_PX * LCD_HEIGHT_PX];
static color_t reference_frame[LCD_WIDTH_PX * LCD_HEIGHT_PX];
static SaveState disk;
static int disk_exists, write_calls, fail_writes, key_mode, key_calls;
static int write_limit, close_error, wait_calls;
static int read_size = sizeof(SaveState);
static unsigned int ticks;
static int startup_run;
static jmp_buf startup_stop;

void EnableStatusArea(int n) { (void)n; }
void Bdisp_EnableColor(int n) { (void)n; }
void Bdisp_PutDisp_DD(void) {}
void OS_InnerWait_ms(int ms) { assert(ms == 10); wait_calls++; }
int RTC_GetTicks(void) {
    if (startup_run && (write_calls || ticks > 700)) longjmp(startup_stop, 1);
    return (int)++ticks;
}
int PRGM_GetKey(void) {
    key_calls++;
    if (key_mode == 1 && key_calls == 1) return KEY_PRGM_RETURN;
    if (key_mode == 2 && key_calls == 1) return KEY_PRGM_SHIFT;
    if (key_mode == 2 && key_calls == 2) return KEY_PRGM_EXIT;
    return 0;
}
void Bfile_StrToName_ncpy(unsigned short *dest, const char *src, size_t n) {
    size_t i = 0;
    for (; i + 1 < n && src[i]; i++) dest[i] = (unsigned char)src[i];
    dest[i] = 0;
}
int Bfile_OpenFile_OS(const unsigned short *name, int mode, int zero) {
    (void)name; (void)mode; (void)zero;
    return disk_exists ? 1 : -1;
}
int Bfile_CreateEntry_OS(const unsigned short *name, int mode, size_t *size) {
    (void)name; (void)mode;
    assert(*size == sizeof disk);
    disk_exists = 1;
    return 0;
}
int Bfile_CloseFile_OS(int handle) { (void)handle; return close_error ? -1 : 0; }
int Bfile_SeekFile_OS(int handle, int pos) { (void)handle; return pos; }
int Bfile_ReadFile_OS(int handle, void *buf, int size, int pos) {
    (void)handle; (void)pos;
    assert(size == sizeof disk);
    memcpy(buf, &disk, sizeof disk);
    return read_size;
}
int Bfile_WriteFile_OS(int handle, const void *buf, int size) {
    (void)handle;
    write_calls++;
    if (fail_writes) return -1;
    assert(size == sizeof disk);
    int count = write_limit >= 0 ? write_limit : size;
    memcpy(&disk, buf, (size_t)count);
    return count;
}

static void reset_state(void) {
    memset(board, 0, sizeof board);
    memset(undoBoard, 0, sizeof undoBoard);
    memset(&disk, 0, sizeof disk);
    memset(frame, 0, sizeof frame);
    VRAM_ADDR = frame;
    score = best = undoScore = 0;
    won = keepPlaying = over = undoWon = undoKeepPlaying = false;
    dirty = saveBroken = undoAvailable = toastActive = false;
    settingsOpen = false;
    movesSinceSave = 0;
    options = OPT_DEFAULT;
    disk_exists = write_calls = fail_writes = key_mode = key_calls = 0;
    startup_run = 0;
    ticks = 0;
    read_size = sizeof(SaveState);
    write_limit = -1;
    close_error = wait_calls = 0;
    srand(7);
}

static void prepare_disk(void) {
    disk_exists = 1;
    disk.magic = SAVE_MAGIC;
    disk.version = SAVE_VERSION;
    disk.best = 777777;
    disk.score = 100;
    disk.settings = OPT_VIVID | OPT_AUTOSAVE_EXIT | OPT_CONFIRM_NEW;
    disk.board[0] = disk.board[1] = 2;
}

static int frame_difference(void) {
    int n = 0;
    for (int i = 0; i < LCD_WIDTH_PX * LCD_HEIGHT_PX; i++)
        if (frame[i] != reference_frame[i]) n++;
    return n;
}

static void dump_frame(const char *name) {
    FILE *f = fopen(name, "wb");
    assert(f);
    fprintf(f, "P6\n%d %d\n255\n", LCD_WIDTH_PX, LCD_HEIGHT_PX);
    for (int i = 0; i < LCD_WIDTH_PX * LCD_HEIGHT_PX; i++) {
        unsigned int p = frame[i];
        unsigned char rgb[3] = {((p >> 11) & 31) * 255 / 31,
                               ((p >> 5) & 63) * 255 / 63, (p & 31) * 255 / 31};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

static void run_startup(bool skip) {
    key_mode = skip ? 1 : 0;
    startup_run = 1;
    if (!setjmp(startup_stop)) cg2048_main();
    startup_run = 0;
}

static void assert_fresh_board(void) {
    int count = 0;
    for (int i = 0; i < 16; i++) {
        int value = board[i / 4][i % 4];
        assert(value == 0 || value == 2 || value == 4);
        if (value) count++;
    }
    assert(count == 2 && score == 0 && !won && !keepPlaying && !over);
}

int main(void) {
    for (int encoded = 0; encoded < 2401; encoded++) {
        reset_state();
        int n = encoded, input[4], packed[4], expected[4] = {0};
        int count = 0, output = 0, expected_score = 0;
        for (int i = 0; i < 4; i++) {
            int digit = n % 7; n /= 7;
            input[i] = digit ? 1 << digit : 0;
            if (input[i]) packed[count++] = input[i];
        }
        for (int i = 0; i < count; i++) {
            int value = packed[i];
            if (i + 1 < count && value == packed[i + 1]) {
                value *= 2; i++; expected_score += value;
            }
            expected[output++] = value;
        }
        bool changed = memcmp(input, expected, sizeof input) != 0;
        assert(slideLine(input) == changed);
        assert(!memcmp(input, expected, sizeof input) && score == expected_score);
    }
    puts("PASS independent merge oracle: 2401 line states");

    for (int failure = 0; failure < 3; failure++) {
        reset_state(); prepare_disk();
        board[0][0] = 2; dirty = true;
        if (failure == 0) write_limit = sizeof disk - 4;
        if (failure == 1) close_error = 1;
        if (failure == 2) fail_writes = 1;
        drawStaticFrame(); drawBoard(); drawScore(); present();
        memcpy(reference_frame, frame, sizeof frame);
        persist();
        assert(dirty && saveBroken && write_calls == 1 && frame_difference() > 0);
        dump_frame("save_failure.ppm");
        present();
        assert(frame_difference() > 0 && write_calls == 1);
        write_limit = -1; close_error = fail_writes = 0;
        persist();
        assert(!dirty && !saveBroken && write_calls == 2 && frame_difference() == 0);
    }
    reset_state();
    assert(readKey() == 0 && wait_calls == 1);
    puts("PASS short-write/close/write failures stay dirty, show warning and recover; input yields");

    reset_state(); prepare_disk();
    run_startup(false);
    assert(best == 777777 && options == 13 && score == 100 && write_calls == 0);
    assert(board[0][0] == 2 && board[0][1] == 2);

    reset_state(); prepare_disk();
    run_startup(true);
    assert_fresh_board();
    assert(best == 777777 && options == 13 && dirty && write_calls == 0);
    persist();
    assert(disk.best == 777777 && disk.settings == 13 && disk.score == 0);
    puts("PASS startup: resume and new-game choice preserve best/options");

    reset_state();
    board[0][0] = board[0][1] = 2;
    movesSinceSave = 49;
    fail_writes = disk_exists = 1;
    gameKeyEvent(KEY_PRGM_LEFT);
    assert(saveBroken && write_calls == 1);
    gameKeyEvent(KEY_PRGM_RIGHT);
    gameKeyEvent(KEY_PRGM_LEFT);
    assert(write_calls == 1 && movesSinceSave == 52 && dirty);
    board[0][0] = board[0][1] = 1024;
    board[0][2] = board[0][3] = 0;
    gameKeyEvent(KEY_PRGM_LEFT);
    assert(won && write_calls == 1);
    persist(); /* An explicit save may retry after an automatic failure. */
    assert(write_calls == 2);
    puts("PASS save failure: automatic retries stop; explicit save can retry");

    reset_state();
    board[0][0] = board[0][1] = 2;
    movesSinceSave = 49;
    options |= OPT_AUTOSAVE_EXIT;
    disk_exists = 1;
    gameKeyEvent(KEY_PRGM_LEFT);
    assert(write_calls == 0 && !over && !won && dirty);

    reset_state();
    board[0][0] = board[0][1] = 2;
    movesSinceSave = 49;
    disk_exists = 1;
    gameKeyEvent(KEY_PRGM_LEFT);
    assert(write_calls == 1 && !dirty && movesSinceSave == 0);

    reset_state();
    board[0][0] = board[0][1] = 1024;
    options |= OPT_AUTOSAVE_EXIT;
    disk_exists = 1;
    gameKeyEvent(KEY_PRGM_LEFT);
    assert(won && write_calls == 1 && disk.won == 1);

    reset_state();
    const int losing_board[4][4] = {
        {2, 4, 8, 8}, {4, 8, 32, 64},
        {8, 16, 64, 128}, {16, 32, 128, 256}
    };
    memcpy(board, losing_board, sizeof board);
    options |= OPT_AUTOSAVE_EXIT;
    disk_exists = 1;
    gameKeyEvent(KEY_PRGM_LEFT);
    assert(over && write_calls == 1 && disk.score == 0 && disk.best == 16);
    for (int i = 0; i < 16; i++) assert(disk.board[i] == 0);
    puts("PASS save mode: exit-only skips checkpoints; win/game-over still save");

    for (unsigned int setting = 0; setting < 16; setting++) {
        reset_state(); prepare_disk();
        disk.settings = setting;
        run_startup(false);
        assert(options == setting && score == 100 && best == 777777);
    }
    reset_state(); prepare_disk();
    disk.version = SAVE_VERSION_LEGACY;
    disk.settings = 0;
    run_startup(false);
    assert(options == OPT_DEFAULT && best == 777777 && score == 100);

    reset_state(); prepare_disk();
    memset(disk.board, 0, sizeof disk.board);
    disk.score = 0;
    run_startup(false);
    assert_fresh_board();
    assert(options == 13 && best == 777777);

    for (int continued = 0; continued < 2; continued++) {
        reset_state(); prepare_disk();
        disk.board[0] = 131072;
        disk.won = 1;
        disk.keepPlaying = continued;
        run_startup(false);
        assert(board[0][0] == 131072 && won && keepPlaying == continued);
    }
    puts("PASS valid saves: all options, legacy, cleared board, win and continuation");

    for (int fixture = 0; fixture < 16; fixture++) {
        reset_state(); prepare_disk();
        switch (fixture) {
            case 0: disk.board[0] = 3; break;
            case 1: disk.board[0] = 0xffffffffu; break;
            case 2: disk.board[0] = 1; break;
            case 3: disk.board[0] = 262144; break;
            case 4: disk.score = 0x80000000u; break;
            case 5: disk.best = 0xffffffffu; break;
            case 6: disk.best = 99; break;
            case 7: disk.won = 2; break;
            case 8: disk.keepPlaying = 1; break;
            case 9: disk.settings = 16; break;
            case 10: disk.magic = 0; break;
            case 11: disk.version = 3; break;
            case 12: read_size = sizeof disk - 1; break;
            case 13: disk.won = 1; break;
            case 14: disk.board[0] = 2048; break;
            case 15: disk.score = disk.best = INT_MAX; break;
        }
        run_startup(false);
        assert_fresh_board();
        assert(best == 0 && options == OPT_DEFAULT);
    }
    puts("PASS malformed saves: 16 invalid records rejected before importing state");

    reset_state();
    for (int i = 0; i < 16; i++) board[i / 4][i % 4] = 2 << (i % 6);
    drawStaticFrame(); drawBoard(); drawScore();
    memcpy(reference_frame, frame, sizeof frame);
    key_mode = 2;
    gameKeyEvent(KEY_PRGM_SHIFT);
    assert(frame_difference() == 0);
    dump_frame("cancel_dialog_fixed.ppm");

    undoAvailable = false;
    gameKeyEvent(KEY_PRGM_F1);
    drawBoard(); drawScore();
    assert(frame_difference() == 0);
    drawOverlay(true);
    drawBoard(); drawScore();
    assert(frame_difference() == 0);
    puts("PASS redraw: cancel, toast and win overlay leave zero residual pixels");

    for (int value = 2; value <= 131072; value *= 2) {
        reset_state();
        drawTileNumber(cellX(0), cellY(0), value);
        int pixels = 0;
        for (int y = 0; y < LCD_HEIGHT_PX; y++)
            for (int x = 0; x < LCD_WIDTH_PX; x++)
                if (frame[y * LCD_WIDTH_PX + x]) {
                    assert(x >= cellX(0) && x < cellX(0) + CELL_SIZE);
                    assert(y >= cellY(0) && y < cellY(0) + CELL_SIZE);
                    pixels++;
                }
        assert(pixels > 0);
    }
    reset_state();
    board[0][0] = 8192; board[0][1] = 16384;
    board[0][2] = 65536; board[0][3] = 131072;
    drawStaticFrame(); drawBoard(); drawScore();
    dump_frame("large_tiles_fixed.ppm");
    puts("PASS tile rendering: all 17 supported values fit inside their cells");

    const int displayed_scores[] = {0, 1024, MAX_SCORE_VALUE, INT_MAX};
    for (int fixture = 0; fixture < 4; fixture++) {
        reset_state();
        drawStaticFrame(); drawBoard();
        memcpy(reference_frame, frame, sizeof frame);
        score = fixture < 3 ? displayed_scores[fixture] : displayed_scores[2];
        best = displayed_scores[fixture];
        drawScore();
        int score_pixels = 0, best_pixels = 0;
        for (int y = 0; y < LCD_HEIGHT_PX; y++) {
            for (int x = 0; x < LCD_WIDTH_PX; x++) {
                if (frame[y * LCD_WIDTH_PX + x] == reference_frame[y * LCD_WIDTH_PX + x]) continue;
                assert(y >= SCOREBOX_Y + 20 && y < SCOREBOX_Y + SCOREBOX_H - 2);
                if (x > SCOREBOX_X && x < SCOREBOX_X + SCOREBOX_W - 1) score_pixels++;
                else {
                    assert(x > BESTBOX_X && x < BESTBOX_X + SCOREBOX_W - 1);
                    best_pixels++;
                }
            }
        }
        assert(score_pixels > 0 && best_pixels > 0);
        score = best = 0;
        drawScore();
        memcpy(reference_frame, frame, sizeof frame);
        drawStaticFrame(); drawBoard(); drawScore();
        assert(frame_difference() == 0);
    }
    puts("PASS score rendering: full score range stays in its cards; shrinking clears old digits");
    puts("2048 repair regression passed");
    return 0;
}
