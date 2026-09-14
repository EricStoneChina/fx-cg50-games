#include "personal_best.h"
#include <fxcg/file.h>

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
    Bfile_CloseFile_OS(handle);

    if (result < 0 || data.magic != PERSONAL_BEST_MAGIC || data.version != PERSONAL_BEST_VERSION) {
        PersonalBest_clear();
    }
}

static bool PersonalBest_equal(const PersonalBestData* left, const PersonalBestData* right) {
    const unsigned char* a = (const unsigned char*)left;
    const unsigned char* b = (const unsigned char*)right;
    for (unsigned int i = 0; i < sizeof(PersonalBestData); i++) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

static bool PersonalBest_save(void) {
    unsigned short path[32];
    PersonalBest_path(path);

    int handle = Bfile_OpenFile_OS(path, READWRITE, 0);
    if (handle >= 0 && Bfile_GetFileSize_OS(handle) != (int)sizeof(PersonalBestData)) {
        Bfile_CloseFile_OS(handle);
        Bfile_DeleteEntry(path);
        handle = -1;
    }

    if (handle < 0) {
        size_t size = sizeof(PersonalBestData);
        if (Bfile_CreateEntry_OS(path, CREATEMODE_FILE, &size) < 0) return false;
        handle = Bfile_OpenFile_OS(path, READWRITE, 0);
    }

    if (handle < 0) return false;
    if (Bfile_SeekFile_OS(handle, 0) < 0 ||
        Bfile_WriteFile_OS(handle, &data, sizeof(PersonalBestData)) < 0) {
        Bfile_CloseFile_OS(handle);
        return false;
    }
    Bfile_CloseFile_OS(handle);

    // A successful syscall is not enough: reopen and verify the complete file
    // before reporting NEW PB to the player.
    PersonalBestData verify;
    handle = Bfile_OpenFile_OS(path, READ, 0);
    if (handle < 0) return false;
    int result = Bfile_ReadFile_OS(handle, &verify, sizeof(PersonalBestData), 0);
    Bfile_CloseFile_OS(handle);
    return result >= 0 && PersonalBest_equal(&data, &verify);
}

int PersonalBest_get(int width, int height, int mines) {
    PersonalBest_load();
    for (int i = 0; i < PERSONAL_BEST_MAX_RECORDS; i++) {
        PersonalBestRecord* record = &data.records[i];
        if (record->width == width && record->height == height && record->mines == mines) {
            return record->deciseconds;
        }
    }
    return 0;
}

bool PersonalBest_record(int width, int height, int mines, int deciseconds) {
    PersonalBest_load();
    if (width <= 0 || height <= 0 || mines <= 0 || deciseconds <= 0) return false;

    int empty = -1;
    int slowest = 0;
    for (int i = 0; i < PERSONAL_BEST_MAX_RECORDS; i++) {
        PersonalBestRecord* record = &data.records[i];
        if (record->width == width && record->height == height && record->mines == mines) {
            if (record->deciseconds != 0 && record->deciseconds <= (unsigned int)deciseconds) return false;
            unsigned int previous = record->deciseconds;
            record->deciseconds = deciseconds;
            if (PersonalBest_save()) return true;
            record->deciseconds = previous;
            return false;
        }
        if (empty < 0 && record->width == 0) empty = i;
        if (record->deciseconds > data.records[slowest].deciseconds) slowest = i;
    }

    int index = empty >= 0 ? empty : slowest;
    PersonalBestRecord previous = data.records[index];
    data.records[index].width = width;
    data.records[index].height = height;
    data.records[index].mines = mines;
    data.records[index].reserved = 0;
    data.records[index].deciseconds = deciseconds;
    if (PersonalBest_save()) return true;
    data.records[index] = previous;
    return false;
}
