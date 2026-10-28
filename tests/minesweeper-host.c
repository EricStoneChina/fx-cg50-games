#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <limits.h>
#include <setjmp.h>
#define CG50_GAMES_MAIN_MENU_H
static void openMainMenu(void);
#define itoa calculator_itoa

/* Compile the active production files, with only calculator syscalls replaced. */
#include "../build/minesweeper/src/board.c"
#include "../build/minesweeper/src/menu.c"
#include "../build/minesweeper/src/personal_best.c"
#include "../build/minesweeper/src/keyboard.c"
#include "../build/minesweeper/src/utils.c"
#include "../build/minesweeper/src/sprites.c"
#define main calculator_main
#include "../build/minesweeper/src/main.c"
#undef main

static jmp_buf menu_stop;
static int menu_returns;
static void openMainMenu(void) { menu_returns++; longjmp(menu_stop, 1); }
static int run_until_menu(void) {
    if (!setjmp(menu_stop)) calculator_main();
    return 0;
}

static unsigned int ticks = 100;
static PersonalBestData disk;
static int disk_exists, read_error, read_limit = -1, write_error, close_error;
static int writes, next_key, write_limit = -1, open_write_error, seek_error, create_error;
static int file_size = sizeof(disk), create_calls, write_opens, deletes;
static int allocations;
static Menu *running_menu;
static int main_step, main_active, rendered_pb, rendered_error;
static Board *from_mask(int w, int h, unsigned int mask);
static char hud[256];
static int notice_draws, row_frame_y;
static int idle_probe, idle_scans, clear_calls, present_calls, wait_calls;

static void check_failed(const char *condition, int line) {
    fprintf(stderr, "FAIL line %d: %s\n", line, condition);
    exit(EXIT_FAILURE);
}
#undef assert
#define assert(condition) ((condition) ? (void)0 : check_failed(#condition, __LINE__))

void *sys_malloc(int size) { void *p = malloc((size_t)size); assert(p); allocations++; if (main_active && !running_menu) { assert(size == sizeof(Menu)); running_menu = p; } return p; }
void sys_free(void *p) { if (p) allocations--; free(p); }
int RTC_GetTicks(void) { return (int)ticks; }
int RTC_Elapsed_ms(int start, int ms) { return (ticks - (unsigned int)start) * 1000u >= (unsigned int)ms * 128u; }
void RTC_GetTime(unsigned int *h, unsigned int *m, unsigned int *s, unsigned int *ms) { *h = *m = *s = 0; *ms = 1; }
int PRGM_GetKey(void) {
    if (idle_probe) return ++idle_scans > 512 ? KEY_PRGM_MENU : 0;
    if (!main_active) return next_key;
    switch (main_step++) {
        case 0: return KEY_PRGM_F1;
        case 1:
            assert(running_menu->board);
            Board_free(running_menu->board);
            running_menu->board = from_mask(3, 3, 1);
            running_menu->board->row = 0;
            running_menu->board->col = 2;
            ticks = 1380;
            return KEY_PRGM_SHIFT;
        case 2:
            assert(running_menu->board->won && rendered_pb == !write_error);
            assert(rendered_error == !!write_error);
            assert(PersonalBest_get(3, 3, 1) == (write_error ? 200 : 100));
            return KEY_PRGM_MENU;
        default: assert(0); return 0;
    }
}
void itoa(int value, unsigned char *out) { sprintf((char *)out, "%d", value); }
void OS_InnerWait_ms(int ms) { assert(ms == 10); wait_calls++; }
void Bdisp_AllClr_VRAM(void) { clear_calls++; }
void Bdisp_EnableColor(int n) { (void)n; }
void Bdisp_PutDisp_DD(void) {
    present_calls++;
    if (main_active && running_menu->board && running_menu->board->won) {
        assert(running_menu->board->newPersonalBest == !write_error);
        assert(writes == 1);
    }
}
void DisplayStatusArea(void) {}
int DefineStatusAreaFlags(int mode, int flags, char *a, char *b) { (void)mode; (void)flags; (void)a; (void)b; return 0; }
void Bdisp_Rectangle(int x, int y, int x2, int y2, char color) {
    if (x == 18 && x2 == 218 && color == TEXT_COLOR_BLUE) {
        assert(y >= 46 && y2 == y + 24 && y2 + 24 < LCD_HEIGHT_PX);
        row_frame_y = y;
    }
}
void Bdisp_FilledRectangle(int x, int y, int x2, int y2, char color) {
    assert(x > 0 && x2 < LCD_WIDTH_PX && y >= 24 && y2 + 24 < LCD_HEIGHT_PX);
    assert(color == TEXT_COLOR_WHITE);
    notice_draws++;
}
void VRAM_CopySprite(const color_t *p, int x, int y, int w, int h) { (void)p; (void)x; (void)y; (void)w; (void)h; }
void PrintCXY(int x, int y, const char *s, int mode, int n, int c, int bg, int a, int b) { (void)x; (void)y; (void)s; (void)mode; (void)n; (void)c; (void)bg; (void)a; (void)b; }
void PrintMini(int *x, int *y, const char *s, int mode, unsigned int limit, int a, int b, int color, int bg, int draw, int c) { if (draw && strlen(hud) + strlen(s) < sizeof(hud)) strcat(hud, s); if (main_active && draw) { if (!strcmp(s, " NEW PB!")) rendered_pb++; if (!strcmp(s, "Best not saved!")) rendered_error++; } *x += (int)strlen(s) * 8; (void)y; (void)mode; (void)limit; (void)a; (void)b; (void)color; (void)bg; (void)draw; (void)c; }
void PrintMiniMini(int *x, int *y, const char *s, int mode, char color, int measure) { *x += (int)strlen(s) * 4; (void)y; (void)mode; (void)color; (void)measure; }
void Bfile_StrToName_ncpy(unsigned short *dest, const char *source, size_t n) { size_t i = 0; for (; i + 1 < n && source[i]; i++) dest[i] = (unsigned char)source[i]; dest[i] = 0; }
int Bfile_OpenFile_OS(const unsigned short *path, int mode, int zero) {
    (void)path; (void)zero;
    if (mode == READWRITE) write_opens++;
    return disk_exists && !(mode == READ && read_error) && !(mode == READWRITE && open_write_error) ? 1 : -1;
}
int Bfile_CloseFile_OS(int handle) { (void)handle; return close_error ? -1 : 0; }
int Bfile_GetFileSize_OS(int handle) { (void)handle; return file_size; }
int Bfile_ReadFile_OS(int handle, void *out, int size, int pos) {
    (void)handle; assert(pos == 0 && size == sizeof(disk));
    if (read_error) return -1;
    int n = read_limit >= 0 ? read_limit : size;
    memcpy(out, &disk, (size_t)n);
    return n;
}
int Bfile_WriteFile_OS(int handle, const void *in, int size) {
    (void)handle; assert(size == sizeof(disk)); writes++;
    if (write_error) return -1;
    int n = write_limit >= 0 ? write_limit : size;
    memcpy(&disk, in, (size_t)n);
    return n;
}
int Bfile_SeekFile_OS(int handle, int pos) { (void)handle; assert(pos == 0); return seek_error ? -1 : 4096; }
int Bfile_CreateEntry_OS(const unsigned short *path, int mode, size_t *size) {
    (void)path; assert(mode == CREATEMODE_FILE && *size == sizeof(disk)); create_calls++;
    if (disk_exists || create_error) return -13;
    disk_exists = 1;
    file_size = (int)*size;
    memset(&disk, 0, sizeof(disk));
    return 0;
}
int Bfile_DeleteEntry(const unsigned short *path) { (void)path; deletes++; disk_exists = 0; return 0; }

static void reset_pb(void) {
    loaded = fileWritable = false;
    pendingIndex = -1;
    disk_exists = read_error = write_error = close_error = writes = 0;
    open_write_error = seek_error = create_error = create_calls = write_opens = deletes = 0;
    read_limit = write_limit = -1;
    file_size = sizeof(disk);
    memset(&disk, 0, sizeof(disk));
    PersonalBest_clear();
}

static void seed_disk(void) {
    reset_pb();
    disk.magic = PERSONAL_BEST_MAGIC;
    disk.version = PERSONAL_BEST_VERSION;
    disk.records[0] = (PersonalBestRecord){16, 8, 30, 0, 200};
    disk.records[1] = (PersonalBestRecord){8, 8, 10, 0, 350};
    disk_exists = 1;
}

static int clue(const unsigned char *mines, int w, int h, int i) {
    int n = 0;
    for (int r = i / w - 1; r <= i / w + 1; r++)
        for (int c = i % w - 1; c <= i % w + 1; c++)
            if (r >= 0 && r < h && c >= 0 && c < w && r * w + c != i) n += mines[r * w + c];
    return n;
}

static Board *from_mask(int w, int h, unsigned int mask) {
    Board *b = sys_malloc(sizeof(*b));
    Board_create(b, w, h, 1, false);
    unsigned char mines[128] = {0};
    b->mines = 0;
    for (int i = 0; i < w * h; i++) { mines[i] = (mask >> i) & 1u; b->mines += mines[i]; }
    for (int i = 0; i < w * h; i++) b->data[i] = COVER_TILE_BIT | (mines[i] ? kTileTypeMine : clue(mines, w, h, i));
    b->firstReveal = false;
    b->timerStartTicks = 100;
    b->row = b->col = 1;
    ticks = 228;
    return b;
}

/* Independent queue oracle: numbered cells stop expansion; flags stay covered. */
static void reference_reveal(const unsigned char *mines, int w, int h, int start, const unsigned char *flags, unsigned char *open) {
    int queue[128], first = 0, count = 0;
    if (mines[start] || flags[start] || open[start]) return;
    open[start] = 1;
    queue[count++] = start;
    while (first < count) {
        int i = queue[first++];
        if (clue(mines, w, h, i)) continue;
        for (int r = i / w - 1; r <= i / w + 1; r++)
            for (int c = i % w - 1; c <= i % w + 1; c++) {
                if (r < 0 || r >= h || c < 0 || c >= w) continue;
                int j = r * w + c;
                if (!mines[j] && !flags[j] && !open[j]) { open[j] = 1; queue[count++] = j; }
            }
    }
}

static void core_checks(void) {
    int boards = 0, reveals = 0, chords = 0;
    for (int w = 2; w <= 16; w++) for (int h = 2; h <= 8; h++) {
        int counts[3] = {1, w * h / 4, w * h * 9 / 10};
        for (int k = 0; k < 3; k++) for (int start = 0; start < w * h; start++) {
            Board b;
            Board_createSafe(&b, w, h, counts[k], true, start / w, start % w);
            unsigned char mines[128] = {0};
            int n = 0;
            for (int i = 0; i < w * h; i++) { mines[i] = (b.data[i] & 15) == kTileTypeMine; n += mines[i]; }
            assert(n == counts[k] && !mines[start]);
            for (int i = 0; i < w * h; i++) if (!mines[i]) assert((b.data[i] & 15) == clue(mines, w, h, i));
            int protected = 0;
            for (int r = start / w - 1; r <= start / w + 1; r++) for (int c = start % w - 1; c <= start % w + 1; c++)
                if (r >= 0 && r < h && c >= 0 && c < w) protected++;
            if (w * h - protected >= counts[k]) assert((b.data[start] & 15) == 0);
            sys_free(b.data);
            boards++;
        }
    }
    for (unsigned int mask = 1; mask < 511; mask++) for (int start = 0; start < 9; start++) {
        Board *b = from_mask(3, 3, mask);
        unsigned char mines[128] = {0}, flags[128] = {0}, open[128] = {0};
        for (int i = 0; i < 9; i++) mines[i] = (mask >> i) & 1u;
        b->row = start / 3; b->col = start % 3;
        Board_revealSingleCell(b, b->row, b->col);
        if (mines[start]) assert(b->lost && !b->won);
        else {
            reference_reveal(mines, 3, 3, start, flags, open);
            int remaining = 0;
            for (int i = 0; i < 9; i++) { assert((!Board_cellIsCovered(&b->data[i])) == open[i]); if (!mines[i] && !open[i]) remaining++; }
            assert(!b->lost && b->won == (remaining == 0));
        }
        Board_free(b);
        reveals++;
    }
    for (unsigned int mask = 1; mask < 511; mask++) {
        if (mask & (1u << 4)) continue;
        if (mask == (511u ^ (1u << 4))) continue; /* Already won before chording. */
        unsigned char mines[128] = {0};
        for (int i = 0; i < 9; i++) mines[i] = (mask >> i) & 1u;
        for (unsigned int flag_mask = 0; flag_mask < 512; flag_mask++) {
            if (flag_mask & (1u << 4)) continue;
            Board *b = from_mask(3, 3, mask);
            b->data[4] &= ~COVER_TILE_BIT;
            int flags = 0;
            for (int i = 0; i < 9; i++) if (flag_mask & (1u << i)) { b->data[i] |= FLAG_TILE_BIT; flags++; }
            int should_lose = flags == clue(mines, 3, 3, 4) && (mask & ~flag_mask) != 0;
            Board_revealSingleCell(b, 1, 1);
            assert(b->lost == should_lose && !(b->lost && b->won));
            if (should_lose) {
                int hits = 0;
                for (int i = 0; i < 9; i++) if (b->data[i] == kTileTypeHitMine) {
                    assert(mines[i] && !(flag_mask & (1u << i)));
                    hits++;
                }
                assert(hits == 1 && b->data[4] == clue(mines, 3, 3, 4));
            } else {
                unsigned char marked[128] = {0}, open[128] = {0};
                open[4] = 1;
                for (int i = 0; i < 9; i++) marked[i] = (flag_mask >> i) & 1u;
                if (flags == clue(mines, 3, 3, 4))
                    for (int i = 0; i < 9; i++) reference_reveal(mines, 3, 3, i, marked, open);
                int remaining = 0;
                for (int i = 0; i < 9; i++) {
                    assert(!!Board_cellIsCovered(&b->data[i]) == !open[i]);
                    assert(!!Board_cellIsFlagged(&b->data[i]) == marked[i]);
                    if (!mines[i] && !open[i]) remaining++;
                }
                assert(b->won == (remaining == 0));
            }
            Board_free(b);
            chords++;
        }
    }
    assert(writes == 0);
    printf("PASS safe_generation=%d ordinary_reveals=%d chord_oracles=%d\n", boards, reveals, chords);
}

static void flag_and_hidden_checks(void) {
    reset_pb();
    Board *b = sys_malloc(sizeof(*b));
    Board_create(b, 16, 8, 10, false);
    Board_flag(b, 0, 0);
    Board_flag(b, 7, 15);
    char original[128];
    memcpy(original, b->data, sizeof(original));
    Board_revealSingleCell(b, 0, 0);
    original[0] ^= FLAG_TILE_BIT;
    assert(b->firstReveal && b->timerStartTicks == -1);
    assert(!memcmp(original, b->data, sizeof(original)));
    Board_flag(b, 0, 0);
    Board_revealSingleCell(b, 4, 8);
    assert(!b->firstReveal && !b->lost);
    assert(Board_cellIsFlagged(&b->data[0]) && Board_cellIsCovered(&b->data[0]));
    assert(Board_cellIsFlagged(&b->data[127]) && Board_cellIsCovered(&b->data[127]));
    Board_free(b);

    b = from_mask(4, 3, 1u << 11);
    Board_flag(b, 0, 1);
    Board_revealSingleCell(b, 0, 0);
    assert(Board_cellIsFlagged(&b->data[1]) && Board_cellIsCovered(&b->data[1]));
    assert(!b->won && !b->lost);
    Board_revealSingleCell(b, 0, 1);
    assert(!Board_cellIsFlagged(&b->data[1]) && Board_cellIsCovered(&b->data[1]));
    Board_revealSingleCell(b, 0, 1);
    assert(b->won && !b->lost);
    Board_free(b);

    seed_disk();
    b = sys_malloc(sizeof(*b));
    Board_create(b, 16, 8, 30, false);
    Board_revealSingleCell(b, 4, 8);
    ticks += 128;
    for (int i = 0; i < 7; i++) Board_handleKeypress(b, code[i]);
    assert(b->won && !b->newPersonalBest && PersonalBest_get(16, 8, 30) == 200);
    assert(pendingIndex == -1 && PersonalBest_flush() && writes == 0);
    Board_free(b);
    puts("PASS first_reveal_flags flagged_first_cell zero_flood_flags hidden_completion_excludes_PB");
}

static void unread_file_checks(void) {
    for (int failure = 0; failure < 6; failure++) {
        seed_disk();
        switch (failure) {
            case 0: read_error = 1; break;
            case 1: read_limit = 16; break;
            case 2: file_size = 16; break;
            case 3: disk.magic = 0; break;
            case 4: disk.version++; break;
            case 5: close_error = 1; break;
        }
        PersonalBestData before = disk;
        PersonalBest_load();
        assert(!fileWritable && PersonalBest_get(16, 8, 30) == -1);
        read_error = close_error = 0;
        read_limit = -1;
        file_size = sizeof(disk);
        assert(PersonalBest_record(4, 4, 3, 50));
        assert(!PersonalBest_flush());
        assert(!memcmp(&disk, &before, sizeof(disk)));
        assert(PersonalBest_get(4, 4, 3) == -1 && writes == 0 && write_opens == 0 && deletes == 0);
        assert(PersonalBest_flush() && create_calls == 1);
    }

    seed_disk();
    disk.records[0].deciseconds = UINT_MAX;
    disk.records[2] = (PersonalBestRecord){1, 8, 3, 0, 100};
    disk.records[3] = (PersonalBestRecord){8, 9, 3, 0, 100};
    disk.records[4] = (PersonalBestRecord){8, 8, 0, 0, 100};
    disk.records[5] = (PersonalBestRecord){8, 8, 64, 0, 100};
    disk.records[6] = (PersonalBestRecord){8, 8, 3, 1, 100};
    assert(PersonalBest_get(16, 8, 30) == -1 && PersonalBest_get(8, 8, 10) == 350);
    for (int i = 0; i < 7; i++) if (i != 1) assert(data.records[i].width == 0);
    assert(PersonalBest_record(16, 8, 30, 150) && PersonalBest_flush());
    loaded = false;
    assert(PersonalBest_get(16, 8, 30) == 150 && PersonalBest_get(8, 8, 10) == 350);
    puts("PASS unread_file_preserved=6 invalid_records_rejected=6 valid_records_retained");
}

static void save_failure_checks(void) {
    for (int failure = 0; failure < 6; failure++) {
        seed_disk();
        PersonalBest_load();
        assert(PersonalBest_record(16, 8, 30, 100));
        switch (failure) {
            case 0: open_write_error = 1; break;
            case 1: file_size = 16; break;
            case 2: seek_error = 1; break;
            case 3: write_error = 1; break;
            case 4: write_limit = 8; break;
            case 5: close_error = 1; break;
        }
        assert(!PersonalBest_flush());
        assert(PersonalBest_get(16, 8, 30) == 200 && PersonalBest_get(8, 8, 10) == 350);
        int before = write_opens;
        assert(PersonalBest_flush() && PersonalBest_flush() && write_opens == before);
        open_write_error = seek_error = write_error = close_error = 0;
        write_limit = -1;
        file_size = sizeof(disk);
        assert(PersonalBest_record(16, 8, 30, 150) && PersonalBest_flush());
        loaded = false;
        assert(PersonalBest_get(16, 8, 30) == 150 && PersonalBest_get(8, 8, 10) == 350);
        assert(deletes == 0);
    }

    for (int failure = 0; failure < 2; failure++) {
        reset_pb();
        create_error = failure == 0;
        write_error = failure == 1;
        assert(PersonalBest_record(3, 3, 1, 100) && !PersonalBest_flush());
        assert(PersonalBest_get(3, 3, 1) == -1);
        create_error = write_error = 0;
        assert(PersonalBest_record(3, 3, 1, 150) && PersonalBest_flush());
        assert(disk.records[0].deciseconds == 150 && deletes == 0);
    }

    seed_disk();
    for (int i = 0; i < PERSONAL_BEST_MAX_RECORDS; i++)
        disk.records[i] = (PersonalBestRecord){16, 8, i + 1, 0, 100 + i};
    disk.records[127].mines = 1;
    PersonalBest_load();
    PersonalBestRecord evicted = data.records[127];
    assert(PersonalBest_record(3, 3, 1, 50));
    write_error = 1;
    assert(!PersonalBest_flush());
    assert(!memcmp(&data.records[127], &evicted, sizeof(evicted)));
    puts("PASS save_failures_rollback=6 creation_recovery=2 full_table_rollback no_frame_retries");
}

static void lifecycle_checks(void) {
    reset_pb();
    Menu m;
    Menu_create(&m);
    Utils_initKeyToNumberMap();
    Menu_handleKeypress(&m, KEY_PRGM_F1);
    assert(m.board);
    Menu_handleKeypress(&m, KEY_PRGM_EXIT);
    assert(!m.board);
    Menu_handleKeypress(&m, KEY_PRGM_F1);
    Menu_handleKeypress(&m, KEY_PRGM_ACON);
    assert(!m.board);
    int action = 0;
    lastKey = currentKey = 0;
    next_key = KEY_PRGM_SHIFT; Key_update(); action += Key_pressed() != 0;
    Key_update(); action += Key_pressed() != 0;
    next_key = 0; Key_update();
    next_key = KEY_PRGM_SHIFT; Key_update(); action += Key_pressed() != 0;
    assert(action == 2);
    seed_disk();
    assert(PersonalBest_get(16, 8, 30) == 200);
    assert(!PersonalBest_record(16, 8, 30, 250));
    assert(!PersonalBest_record(1, 8, 3, 100) && !PersonalBest_record(8, 8, 64, 100));
    assert(!PersonalBest_record(8, 8, 3, -1));
    assert(PersonalBest_record(16, 8, 30, 150) && PersonalBest_flush());
    loaded = false;
    assert(PersonalBest_get(16, 8, 30) == 150 && PersonalBest_get(8, 8, 10) == 350);
    puts("PASS setup_EXIT_AC key_edges PB_normal_save_and_reload invalid_record_inputs");
}

static void result_ui_checks(void) {
    for (int won = 0; won < 2; won++) {
        Board *b = from_mask(3, 3, 1);
        b->lost = !won;
        b->won = won;
        b->centiseconds = 123;
        b->konamiCodeIndex = 7;
        char original[9];
        memcpy(original, b->data, sizeof(original));
        int row = b->row, col = b->col;
        assert(!Board_handleKeypress(b, KEY_PRGM_RIGHT));
        assert(b->col == (col + 1) % 3 && b->row == row);
        assert(!Board_handleKeypress(b, KEY_PRGM_UP));
        assert(b->row == (row + 2) % 3);
        assert(!Board_handleKeypress(b, KEY_PRGM_SHIFT));
        assert(!Board_handleKeypress(b, KEY_PRGM_ALPHA));
        assert(!Board_handleKeypress(b, KEY_PRGM_ACON));
        assert(!memcmp(original, b->data, sizeof(original)));
        hud[0] = 0;
        Board_drawStatusArea(b);
        assert(strstr(hud, won ? "Won 12.3s" : "Lost 12.3s"));
        assert(strstr(hud, "EXE/EXIT") && b->centiseconds == 123);
        assert(Board_handleKeypress(b, KEY_PRGM_RETURN));
        assert(Board_handleKeypress(b, KEY_PRGM_EXIT));
        Board_free(b);
    }
    Board *b = from_mask(3, 3, 1);
    ticks = 1380;
    b->timerStartTicks = 100;
    hud[0] = 0;
    Board_drawStatusArea(b);
    assert(strstr(hud, "Flags 0/1  10.0s"));
    ticks += 128;
    hud[0] = 0;
    Board_drawStatusArea(b);
    assert(strstr(hud, "11.0s"));
    Board_free(b);
    puts("PASS result_inspection_no_cell_changes explicit_return frozen_result_timer live_HUD");
}

static void notice_and_editor_checks(void) {
    reset_pb();
    Menu m;
    Menu_create(&m);
    m.width = 1;
    Menu_handleKeypress(&m, KEY_PRGM_F1);
    assert(m.notification && !m.board);
    const char *message = m.notification;
    ticks += 1280;
    notice_draws = 0;
    hud[0] = 0;
    Menu_drawStatusArea(&m);
    assert(m.notification == message && notice_draws == 1);
    assert(strstr(hud, "Width cannot be <2!") && strstr(hud, "EXE/EXIT: OK"));
    Menu_handleKeypress(&m, KEY_PRGM_DOWN);
    assert(m.notification == message && m.settingRow == 1);
    Menu_handleKeypress(&m, KEY_PRGM_RETURN);
    assert(!m.notification && m.settingRow == 1 && !m.board);
    m.settingRow = 0;
    Menu_handleKeypress(&m, KEY_PRGM_F1);
    Menu_handleKeypress(&m, KEY_PRGM_LEFT);
    assert(m.notification);
    Menu_handleKeypress(&m, KEY_PRGM_2);
    assert(!m.notification && m.width == BOARD_MAX_WIDTH);
    row_frame_y = -1;
    Menu_draw(&m);
    assert(row_frame_y == 46);
    Menu_handleKeypress(&m, KEY_PRGM_DOWN);
    Menu_draw(&m);
    assert(row_frame_y == 70);
    Menu_handleKeypress(&m, KEY_PRGM_DEL);
    assert(m.height == 0);
    Menu_handleKeypress(&m, KEY_PRGM_F1);
    assert(m.notification && !m.board);
    Menu_handleKeypress(&m, KEY_PRGM_8);
    assert(!m.notification && m.height == 8);

    m.board = from_mask(3, 3, 1);
    m.board->won = true;
    m.board->newPersonalBest = false;
    m.notification = "Best not saved!";
    char original[9];
    memcpy(original, m.board->data, sizeof(original));
    int col = m.board->col;
    ticks += 1280;
    hud[0] = 0;
    Menu_drawStatusArea(&m);
    assert(strstr(hud, "Won ") && strstr(hud, "Best not saved!"));
    assert(m.notification && !m.board->newPersonalBest);
    Menu_handleKeypress(&m, KEY_PRGM_RIGHT);
    assert(m.board->col == col && !memcmp(original, m.board->data, sizeof(original)));
    Menu_handleKeypress(&m, KEY_PRGM_EXIT);
    assert(!m.notification && m.board && m.board->won);
    Menu_handleKeypress(&m, KEY_PRGM_RIGHT);
    assert(m.board->col == (col + 1) % 3);
    Menu_handleKeypress(&m, KEY_PRGM_RETURN);
    assert(!m.board);
    puts("PASS persistent_separate_notice consume_ack edit_dismiss row_focus editing_preserved");
}

static void main_loop_checks(void) {
    for (int failure = 0; failure < 2; failure++) {
        seed_disk();
        disk.records[0] = (PersonalBestRecord){3, 3, 1, 0, 200};
        write_error = failure;
        lastKey = currentKey = 0;
        main_active = 1;
        main_step = rendered_pb = rendered_error = 0;
        running_menu = NULL;
        assert(run_until_menu() == 0 && writes == 1);
        assert(disk.records[0].deciseconds == (failure ? 200u : 100u));
        main_active = 0;
        running_menu = NULL;
        assert(allocations == 0);
    }
    assert(menu_returns == 2);
    puts("PASS main_loop_save_before_first_PB_frame success_and_failure returns_via_resumable_menu allocation_balance=0");
}

static void zero_time_and_idle_checks(void) {
    reset_pb();
    Board *b = sys_malloc(sizeof(*b));
    Board_create(b, 2, 2, 3, false);
    Board_revealSingleCell(b, 0, 0);
    assert(b->won && b->centiseconds == 0 && b->newPersonalBest && writes == 0);
    assert(PersonalBest_flush() && writes == 1);
    loaded = false;
    assert(PersonalBest_get(2, 2, 3) == 0);
    assert(!PersonalBest_record(2, 2, 3, 0) && !PersonalBest_record(2, 2, 3, 1));
    Board_free(b);
    Menu menu;
    Menu_create(&menu);
    menu.width = menu.height = 2; menu.mines = 3;
    hud[0] = 0;
    Menu_draw(&menu);
    assert(strstr(hud, "Best: 0.0s"));

    reset_pb();
    lastKey = currentKey = 0;
    idle_probe = 1;
    idle_scans = clear_calls = present_calls = wait_calls = 0;
    assert(run_until_menu() == 0);
    assert(clear_calls == 3 && present_calls == 2 && wait_calls == 512);
    assert(allocations == 0);
    idle_probe = 0;

    uint32_t expected = 17;
    Utils_srand(17);
    for (int i = 0; i < 1000; i++) {
        expected = (uint32_t)((uint64_t)expected * 1664525u + 1013904223u);
        assert(Utils_rand() == (int)((expected >> 16) & 0x7fff));
    }
    puts("PASS zero_time_PB_saved_reloaded_rendered idle_512_scans_only_2_frames RNG_1000_wraps");
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    assert(sizeof(PersonalBestData) == 1032 && sizeof(unsigned int) == 4);
    reset_pb();
    Utils_srand(17);
    core_checks();
    flag_and_hidden_checks();
    unread_file_checks();
    save_failure_checks();
    lifecycle_checks();
    result_ui_checks();
    notice_and_editor_checks();
    main_loop_checks();
    zero_time_and_idle_checks();
    assert(allocations == 0);
    puts("All Minesweeper host regressions passed.");
    return 0;
}
