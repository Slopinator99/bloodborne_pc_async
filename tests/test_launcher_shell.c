/* bb-shell.h: write a shortcut into a folder and read it back. Usage: test_launcher_shell <dir> */
#define BB_SHELL_WITH_READ
#include "../packaging/windows/bb-shell.h"

static int failures;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL line %d: %s\n", __LINE__, #cond);                   \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    CoInitialize(NULL);
    char lnk[MAX_PATH], target[MAX_PATH], work[MAX_PATH];
    snprintf(lnk, sizeof(lnk), "%s\\Bloodborne.lnk", argv[1]);
    char exe[MAX_PATH];
    GetModuleFileNameA(NULL, exe, sizeof(exe)); /* any file that exists will do as a target */
    char dir[MAX_PATH];
    snprintf(dir, sizeof(dir), "%s", argv[1]);

    CHECK(make_shortcut(lnk, exe, dir, "Bloodborne"));
    CHECK(GetFileAttributesA(lnk) != INVALID_FILE_ATTRIBUTES);
    CHECK(read_shortcut(lnk, target, sizeof(target), work, sizeof(work)));
    CHECK(!_stricmp(target, exe));
    CHECK(!_stricmp(work, dir));

    /* writing it again replaces it */
    CHECK(make_shortcut(lnk, exe, dir, "Bloodborne again"));
    /* a folder that does not exist is a failure, not a crash */
    CHECK(!make_shortcut("C:\\no\\such\\folder\\x.lnk", exe, dir, "x"));

    char desktop[MAX_PATH];
    CHECK(desktop_folder(desktop, sizeof(desktop)) && GetFileAttributesA(desktop) != INVALID_FILE_ATTRIBUTES);

    CoUninitialize();
    printf(failures ? "FAILED\n" : "OK\n");
    return failures ? 1 : 0;
}
