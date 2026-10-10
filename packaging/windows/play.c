/* Play Bloodborne.exe: starts the game with the settings saved by the launcher, without opening
 * the launcher (it runs `bb-runtime.exe --play` from its own folder and waits for it, so Steam
 * and other front ends see the game running). Extra arguments are passed on. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <wchar.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR arguments, int show)
{
    (void)instance, (void)previous, (void)show;
    static wchar_t dir[32768], exe[32768], parameters[32768], line[32768], text[1024];
    DWORD length = GetModuleFileNameW(NULL, dir, 32768);
    if (length == 0 || length >= 32768)
        return 1;
    wchar_t *slash = wcsrchr(dir, L'\\');
    if (slash)
        *slash = 0;
    /* The frozen Python runtime: bb-runtime.exe, Bloodborne.exe in older packages. */
    static const wchar_t* names[] = {L"bb-runtime.exe", L"Bloodborne.exe"};
    const wchar_t* name = names[0];
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        swprintf(exe, 32768, L"%ls\\%ls", dir, names[i]);
        DWORD attributes = GetFileAttributesW(exe);
        if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
            name = names[i];
            break;
        }
    }
    swprintf(exe, 32768, L"%ls\\%ls", dir, name);
    swprintf(parameters, 32768, L"--play%ls%ls", arguments && *arguments ? L" " : L"",
             arguments ? arguments : L"");
    swprintf(line, 32768, L"\"%ls\" %ls", exe, parameters);

    HANDLE game = NULL;
    STARTUPINFOW startup = {.cb = sizeof(startup)};
    PROCESS_INFORMATION process;
    DWORD error = 0;
    if (CreateProcessW(exe, line, NULL, NULL, FALSE, 0, NULL, dir, &startup, &process)) {
        CloseHandle(process.hThread);
        game = process.hProcess;
    } else {
        error = GetLastError();
        if (error == ERROR_ELEVATION_REQUIRED) {
            /* The runtime is set to "Run this program as an administrator" (its Compatibility
             * properties): CreateProcess refuses that, only the shell can ask for it. */
            SHELLEXECUTEINFOW shell = {.cbSize = sizeof(shell), .fMask = SEE_MASK_NOCLOSEPROCESS,
                                       .lpFile = exe, .lpParameters = parameters,
                                       .lpDirectory = dir, .nShow = SW_SHOWNORMAL};
            if (ShellExecuteExW(&shell) && shell.hProcess)
                game = shell.hProcess;
            else
                error = GetLastError();
        }
    }
    if (!game) {
        /* "Not found" only when it really is missing: every failure used to say so. */
        if (GetFileAttributesW(exe) == INVALID_FILE_ATTRIBUTES) {
            swprintf(text, 1024, L"%ls was not found next to this file:\n%ls", name, dir);
        } else {
            wchar_t reason[512] = L"";
            FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error, 0,
                           reason, 512, NULL);
            swprintf(text, 1024, L"Could not start %ls (error %lu): %ls", name, (unsigned long)error,
                     reason);
        }
        MessageBoxW(NULL, text, L"Bloodborne", MB_OK | MB_ICONERROR);
        return 1;
    }
    WaitForSingleObject(game, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(game, &code);
    CloseHandle(game);
    return (int)code;
}
