#include "personal_best.h"
#include "board.h"
#include <fxcg/file.h>
#include <limits.h>

#define PERSONAL_BEST_MAGIC 0x4d535042
#define PERSONAL_BEST_VERSION 1
#define PERSONAL_BEST_MAX_RECORDS 128

typedef struct {
    unsigned char width;
    unsigned char height;
    unsigned char mines;
    unsigned char reserved;
    unsigned int deciseconds;
} PersonalBestRecord;

typedef struct {
    unsigned int magic;
    unsigned int version;
    PersonalBestRecord records[PERSONAL_BEST_MAX_RECORDS];
} PersonalBestData;

static PersonalBestData data;
static bool loaded = false;
static bool fileWritable = false;
static int pendingIndex = -1;
static PersonalBestRecord previousRecord;

static void PersonalBest_clear(void) {
    data.magic = PERSONAL_BEST_MAGIC;
    data.version = PERSONAL_BEST_VERSION;
    for (int i = 0; i < PERSONAL_BEST_MAX_RECORDS; i++) {
        data.records[i].width = 0;
        data.records[i].height = 0;
        data.records[i].mines = 0;
        data.records[i].reserved = 0;
        data.records[i].deciseconds = 0;
    }
}

static void PersonalBest_path(unsigned short* path) {
    Bfile_StrToName_ncpy(path, "\\\\fls0\\MINESPB.DAT", 32);
}

void PersonalBest_load(void) {
    if (loaded) return;
    loaded = true;
    PersonalBest_clear();

    unsigned short path[32];
    PersonalBest_path(path);
    int handle = Bfile_OpenFile_OS(path, READ, 0);
    if (handle < 0) return;

    int size = Bfile_GetFileSize_OS(handle);
    int result = -1;
    if (size == (int)sizeof(PersonalBestData)) {
        result = Bfile_ReadFile_OS(handle, &data, sizeof(PersonalBestData), 0);
    }
    int closeResult = Bfile_CloseFile_OS(handle);

    if (result != (int)sizeof(PersonalBestData) || closeResult < 0 ||
        data.magic != PERSONAL_BEST_MAGIC || data.version != PERSONAL_BEST_VERSION) {
        PersonalBest_clear();
        return;
    }

    fileWritable = true;
    for (int i = 0; i < PERSONAL_BEST_MAX_RECORDS; i++) {
        PersonalBestRecord* record = &data.records[i];
        if (record->width < 2 || record->width > BOARD_MAX_WIDTH ||
            record->height < 2 || record->height > BOARD_MAX_HEIGHT ||
            record->mines == 0 || record->mines >= record->width * record->height ||
            record->reserved != 0 || record->deciseconds > INT_MAX) {
            *record = (PersonalBestRecord){0};
        }
    }
}

static bool PersonalBest_save(void) {
    unsigned short path[32];
    PersonalBest_path(path);

    // Creation fails if an unread or damaged file already exists, preserving it.
    if (!fileWritable) {
        size_t size = sizeof(PersonalBestData);
        if (Bfile_CreateEntry_OS(path, CREATEMODE_FILE, &size) < 0) return false;
        fileWritable = true;
    }

    int handle = Bfile_OpenFile_OS(path, READWRITE, 0);
    if (handle < 0) return false;
    bool saved = Bfile_GetFileSize_OS(handle) == (int)sizeof(PersonalBestData) &&
        Bfile_SeekFile_OS(handle, 0) >= 0 &&
        Bfile_WriteFile_OS(handle, &data, sizeof(PersonalBestData)) == (int)sizeof(PersonalBestData);
    if (Bfile_CloseFile_OS(handle) < 0) saved = false;
    return saved;
}

bool PersonalBest_flush(void) {
    if (pendingIndex < 0) return true;
    bool saved = PersonalBest_save();
    if (!saved) data.records[pendingIndex] = previousRecord;
    // Never retry failed writes every frame.
    pendingIndex = -1;
    return saved;
}

int PersonalBest_get(int width, int height, int mines) {
    PersonalBest_load();
    for (int i = 0; i < PERSONAL_BEST_MAX_RECORDS; i++) {
        PersonalBestRecord* record = &data.records[i];
        if (record->width == width && record->height == height && record->mines == mines) {
            return record->deciseconds;
        }
    }
    return -1;
}

bool PersonalBest_record(int width, int height, int mines, int deciseconds) {
    PersonalBest_load();
    if (width < 2 || width > BOARD_MAX_WIDTH || height < 2 || height > BOARD_MAX_HEIGHT ||
        mines <= 0 || mines >= width * height || deciseconds < 0 || pendingIndex >= 0) return false;

    int index = -1;
    int empty = -1;
    int slowest = 0;
    for (int i = 0; i < PERSONAL_BEST_MAX_RECORDS; i++) {
        PersonalBestRecord* record = &data.records[i];
        if (record->width == width && record->height == height && record->mines == mines) {
            if (record->deciseconds <= (unsigned int)deciseconds) return false;
            index = i;
            break;
        }
        if (empty < 0 && record->width == 0) empty = i;
        if (record->deciseconds > data.records[slowest].deciseconds) slowest = i;
    }

    if (index < 0) index = empty >= 0 ? empty : slowest;
    previousRecord = data.records[index];
    pendingIndex = index;
    data.records[index].width = width;
    data.records[index].height = height;
    data.records[index].mines = mines;
    data.records[index].reserved = 0;
    data.records[index].deciseconds = deciseconds;
    return true;
}
