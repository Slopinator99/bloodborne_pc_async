/* bb-shell.h: a Windows shortcut (.lnk) written through the shell's own COM interface (no
 * PowerShell, no script). Header-only; included by bb-launcher.c and tests/test_launcher_shell.c.
 * COM must be initialised by the caller (CoInitialize). */
#ifndef BB_SHELL_H
#define BB_SHELL_H

#include <windows.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <stdio.h>

static wchar_t* shell_wide(const char* utf8) {
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    wchar_t* w = n > 0 ? malloc((size_t)n * sizeof(wchar_t)) : NULL;
    if (w) MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w, n);
    return w;
}

/* Writes the shortcut file lnk_path starting target in work_dir, with the target's first icon.
 * Returns 1 on success. */
static int make_shortcut(const char* lnk_path, const char* target, const char* work_dir, const char* description) {
    IShellLinkW* link = NULL;
    IPersistFile* file = NULL;
    int ok = 0;
    wchar_t *wl = shell_wide(lnk_path), *wt = shell_wide(target), *ww = shell_wide(work_dir),
            *wd = shell_wide(description);
    if (wl && wt && ww && wd &&
        SUCCEEDED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void**)&link))) {
        link->lpVtbl->SetPath(link, wt);
        link->lpVtbl->SetWorkingDirectory(link, ww);
        link->lpVtbl->SetIconLocation(link, wt, 0);
        link->lpVtbl->SetDescription(link, wd);
        if (SUCCEEDED(link->lpVtbl->QueryInterface(link, &IID_IPersistFile, (void**)&file))) {
            ok = SUCCEEDED(file->lpVtbl->Save(file, wl, TRUE));
            file->lpVtbl->Release(file);
        }
        link->lpVtbl->Release(link);
    }
    free(wl);
    free(wt);
    free(ww);
    free(wd);
    return ok;
}

#ifdef BB_SHELL_WITH_READ /* tests */
/* The target and working folder stored in a shortcut file (UTF-8), for tests. */
static int read_shortcut(const char* lnk_path, char* target, size_t tn, char* work_dir, size_t wn) {
    IShellLinkW* link = NULL;
    IPersistFile* file = NULL;
    int ok = 0;
    wchar_t* wl = shell_wide(lnk_path);
    if (wl && SUCCEEDED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void**)&link))) {
        if (SUCCEEDED(link->lpVtbl->QueryInterface(link, &IID_IPersistFile, (void**)&file))) {
            wchar_t wt[MAX_PATH], ww[MAX_PATH];
            if (SUCCEEDED(file->lpVtbl->Load(file, wl, STGM_READ)) &&
                SUCCEEDED(link->lpVtbl->GetPath(link, wt, MAX_PATH, NULL, 0)) &&
                SUCCEEDED(link->lpVtbl->GetWorkingDirectory(link, ww, MAX_PATH))) {
                WideCharToMultiByte(CP_UTF8, 0, wt, -1, target, (int)tn, NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, ww, -1, work_dir, (int)wn, NULL, NULL);
                ok = 1;
            }
            file->lpVtbl->Release(file);
        }
        link->lpVtbl->Release(link);
    }
    free(wl);
    return ok;
}

#endif

/* The path of the current user's desktop (UTF-8); 1 on success. */
static int desktop_folder(char* out, size_t n) {
    wchar_t* path = NULL;
    if (FAILED(SHGetKnownFolderPath(&FOLDERID_Desktop, 0, NULL, &path))) return 0;
    WideCharToMultiByte(CP_UTF8, 0, path, -1, out, (int)n, NULL, NULL);
    CoTaskMemFree(path);
    return 1;
}

#endif
