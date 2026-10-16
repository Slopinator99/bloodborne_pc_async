/* bb-update.h: what bb-launcher needs to update itself from a GitHub release, header-only
 * (WinHTTP, tar.exe, plain Win32). Included by bb-launcher.c and tests/test_launcher_update.c.
 *
 * The flow, as in the Python launcher: ask the releases API for the newest release, download its
 * zip, unpack it into a temporary folder and run the new bb-launcher.exe from there with
 * --install-update <install folder> <pid>; that copy waits for the old launcher to close,
 * copies the new files over the install (never the user's files) and starts the new launcher. */
#ifndef BB_UPDATE_H
#define BB_UPDATE_H

#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bb-settings.h" /* json_decode, json_skip_string */

typedef struct {
    char version[32];  /* digits joined by '.', from the tag */
    char zip_url[1024];
    char page_url[512];
} ReleaseInfo;

/* Numbers in a text as a tuple: "windows-v1.7.5" -> 1, 7, 5. */
static int version_parts(const char* text, int* out, int max) {
    int n = 0;
    while (*text && n < max) {
        if (*text >= '0' && *text <= '9') {
            out[n++] = atoi(text);
            while (*text >= '0' && *text <= '9') ++text;
        } else {
            ++text;
        }
    }
    return n;
}

/* <0, 0, >0 like strcmp, comparing the number tuples; a missing part counts as 0. */
static int version_cmp(const char* a, const char* b) {
    int x[8] = {0}, y[8] = {0};
    version_parts(a, x, 8);
    version_parts(b, y, 8);
    for (int i = 0; i < 8; ++i)
        if (x[i] != y[i]) return x[i] < y[i] ? -1 : 1;
    return 0;
}

/* The string value of the first "key": "..." in a JSON text, decoded; 0 when there is none. */
static int json_find_string(const char* text, const char* key, char* out, size_t n) {
    char needle[64];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char* p = strstr(text, needle);
    while (p) {
        const char* q = json_ws(p + strlen(needle));
        if (*q == ':') {
            q = json_ws(q + 1);
            if (*q == '"') {
                json_decode(q, out, n);
                return 1;
            }
        }
        p = strstr(p + 1, needle);
    }
    return 0;
}

static int ends_with_nocase(const char* s, const char* suffix) {
    size_t a = strlen(s), b = strlen(suffix);
    return a >= b && !_stricmp(s + a - b, suffix);
}

/* GitHub's releases/latest JSON: the version from tag_name, the release page (html_url, the
 * first one is the release's own) and the first asset whose download URL ends in .zip.
 * Returns 1 when there is a tag. */
static int release_parse(const char* json, ReleaseInfo* out) {
    char tag[64];
    memset(out, 0, sizeof(*out));
    if (!json_find_string(json, "tag_name", tag, sizeof(tag))) return 0;
    int parts[8], n = version_parts(tag, parts, 8), used = 0;
    for (int i = 0; i < n; ++i)
        used += snprintf(out->version + used, sizeof(out->version) - (size_t)used, "%s%d", i ? "." : "", parts[i]);
    json_find_string(json, "html_url", out->page_url, sizeof(out->page_url));
    const char* p = json;
    while ((p = strstr(p, "\"browser_download_url\"")) != NULL) {
        char url[1024];
        if (json_find_string(p, "browser_download_url", url, sizeof(url)) && ends_with_nocase(url, ".zip")) {
            snprintf(out->zip_url, sizeof(out->zip_url), "%s", url);
            break;
        }
        p += 10;
    }
    return 1;
}

/* --- WinHTTP --- */
typedef void (*ProgressFn)(int percent, void* context);

static wchar_t* utf8_to_wide(const char* s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wchar_t* w = n > 0 ? malloc((size_t)n * sizeof(wchar_t)) : NULL;
    if (w) MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}

/* Opens a GET request for url (redirects followed); returns the request handle with the response
 * received and its HTTP status in *status, or NULL. The caller closes it and the two handles
 * stored in *session and *connect. */
static HINTERNET http_open(const char* url, int* status, HINTERNET* session, HINTERNET* connect) {
    *session = *connect = NULL;
    *status = 0;
    wchar_t* wurl = utf8_to_wide(url);
    if (!wurl) return NULL;
    URL_COMPONENTS uc = {.dwStructSize = sizeof(uc)};
    wchar_t host[256], path[2048];
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 2048;
    uc.dwSchemeLength = (DWORD)-1;
    uc.dwExtraInfoLength = (DWORD)-1;
    HINTERNET req = NULL;
    if (WinHttpCrackUrl(wurl, 0, 0, &uc)) {
        /* The query part (?...) is in ExtraInfo: keep it with the path. */
        wchar_t full[3072];
        swprintf(full, 3072, L"%ls%ls", path, uc.lpszExtraInfo ? uc.lpszExtraInfo : L"");
        *session = WinHttpOpen(L"bbport-launcher", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, NULL, NULL, 0);
        if (*session) {
            WinHttpSetTimeouts(*session, 15000, 15000, 30000, 30000);
            *connect = WinHttpConnect(*session, host, uc.nPort, 0);
        }
        if (*connect) {
            req = WinHttpOpenRequest(*connect, L"GET", full, NULL, WINHTTP_NO_REFERER,
                                     WINHTTP_DEFAULT_ACCEPT_TYPES,
                                     uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
        }
        if (req) {
            WinHttpAddRequestHeaders(req, L"Accept: application/vnd.github+json", (DWORD)-1,
                                     WINHTTP_ADDREQ_FLAG_ADD);
            DWORD len = sizeof(DWORD), code = 0;
            if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                WinHttpReceiveResponse(req, NULL) &&
                WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                    WINHTTP_HEADER_NAME_BY_INDEX, &code, &len, WINHTTP_NO_HEADER_INDEX)) {
                *status = (int)code;
            } else {
                WinHttpCloseHandle(req);
                req = NULL;
            }
        }
    }
    free(wurl);
    return req;
}

static void http_close(HINTERNET req, HINTERNET session, HINTERNET connect) {
    if (req) WinHttpCloseHandle(req);
    if (connect) WinHttpCloseHandle(connect);
    if (session) WinHttpCloseHandle(session);
}

/* GET url into out (NUL-terminated, truncated to n-1). Returns the HTTP status, 0 on a network
 * error. */
static int http_get(const char* url, char* out, size_t n) {
    HINTERNET session, connect;
    int status;
    out[0] = 0;
    HINTERNET req = http_open(url, &status, &session, &connect);
    if (!req) {
        http_close(NULL, session, connect);
        return 0;
    }
    size_t used = 0;
    DWORD got;
    while (used < n - 1 && WinHttpReadData(req, out + used, (DWORD)(n - 1 - used), &got) && got) used += got;
    out[used] = 0;
    http_close(req, session, connect);
    return status;
}

/* GET url into the file path, reporting percent done (when the size is known). Returns 1 on a
 * complete 200 response written to disk. */
static int http_download(const char* url, const char* path, ProgressFn progress, void* context) {
    HINTERNET session, connect;
    int status;
    HINTERNET req = http_open(url, &status, &session, &connect);
    int ok = 0;
    if (req && status == 200) {
        DWORD len = sizeof(DWORD), total = 0;
        if (!WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                                 WINHTTP_HEADER_NAME_BY_INDEX, &total, &len, WINHTTP_NO_HEADER_INDEX))
            total = 0;
        FILE* f = fopen(path, "wb");
        if (f) {
            static char buf[1 << 16];
            unsigned long long done = 0;
            int shown = -1, clean = 1;
            DWORD got = 0;
            for (;;) {
                if (!WinHttpReadData(req, buf, sizeof(buf), &got)) {
                    clean = 0;
                    break;
                }
                if (!got) break;
                if (fwrite(buf, 1, got, f) != got) {
                    clean = 0;
                    break;
                }
                done += got;
                int pct = total ? (int)(done * 100 / total) : 0;
                if (progress && pct != shown) {
                    shown = pct;
                    progress(pct, context);
                }
            }
            ok = fclose(f) == 0 && clean && (!total || done == total);
        }
    }
    http_close(req, session, connect);
    if (!ok) DeleteFileA(path);
    return ok;
}

typedef void (*CountFn)(int done, int total, void* context);

/* Downloads base/<name> into folder/<name> for each name, through a .part file so an interrupted
 * download never leaves a half file under the real name. Stops at the first failure: returns 0
 * with *failed the index of that name (earlier files stay). progress(done, total) is called
 * before the first file and after each one. Returns 1 when every file arrived. */
static int download_files(const char* base, char* const* names, int n, const char* folder,
                          CountFn progress, void* context, int* failed) {
    CreateDirectoryA(folder, NULL);
    if (failed) *failed = -1;
    if (progress) progress(0, n, context);
    for (int i = 0; i < n; ++i) {
        char url[1536], part[MAX_PATH + 8], dest[MAX_PATH];
        snprintf(url, sizeof(url), "%s/%s", base, names[i]);
        snprintf(dest, sizeof(dest), "%s\\%s", folder, names[i]);
        snprintf(part, sizeof(part), "%s.part", dest);
        if (!http_download(url, part, NULL, NULL) ||
            !MoveFileExA(part, dest, MOVEFILE_REPLACE_EXISTING)) {
            DeleteFileA(part);
            if (failed) *failed = i;
            return 0;
        }
        if (progress) progress(i + 1, n, context);
    }
    return 1;
}

/* --- files --- */

/* Deletes a folder tree (best effort). */
static void rmtree(const char* dir) {
    char pattern[MAX_PATH + 4];
    snprintf(pattern, sizeof(pattern), "%s\\*", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
            char child[MAX_PATH * 2];
            snprintf(child, sizeof(child), "%s\\%s", dir, fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) rmtree(child);
            else {
                SetFileAttributesA(child, FILE_ATTRIBUTE_NORMAL);
                DeleteFileA(child);
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryA(dir);
}

/* The first file called name under root (depth first); 1 when found. */
static int find_file(const char* root, const char* name, char* out, size_t n) {
    char pattern[MAX_PATH + 4];
    snprintf(pattern, sizeof(pattern), "%s\\*", root);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    int found = 0;
    do {
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
        char child[MAX_PATH * 2];
        snprintf(child, sizeof(child), "%s\\%s", root, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            found = find_file(child, name, out, n);
        } else if (!_stricmp(fd.cFileName, name)) {
            snprintf(out, n, "%s", child);
            found = 1;
        }
    } while (!found && FindNextFileA(h, &fd));
    FindClose(h);
    return found;
}

static int in_list(const char* name, const char* const* list) {
    for (; list && *list; ++list)
        if (!_stricmp(name, *list)) return 1;
    return 0;
}

/* Copies src over dst (made as needed). Names in skip (a NULL-terminated list) are left alone at
 * the top level only. Returns the number of files that could not be copied. */
static int copy_tree(const char* src, const char* dst, const char* const* skip, int top) {
    CreateDirectoryA(dst, NULL);
    char pattern[MAX_PATH + 4];
    snprintf(pattern, sizeof(pattern), "%s\\*", src);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return 1;
    int failures = 0;
    do {
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
        if (top && in_list(fd.cFileName, skip)) continue;
        char from[MAX_PATH * 2], to[MAX_PATH * 2];
        snprintf(from, sizeof(from), "%s\\%s", src, fd.cFileName);
        snprintf(to, sizeof(to), "%s\\%s", dst, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) failures += copy_tree(from, to, skip, 0);
        else if (!CopyFileA(from, to, FALSE)) ++failures;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return failures;
}

/* The installation step: copy src over dst, retrying for up to tries seconds while files are
 * still in use (the old launcher is closing). Returns 1 on success. */
static int install_copy(const char* src, const char* dst, const char* const* skip, int tries) {
    for (int i = 0; i < tries; ++i) {
        if (copy_tree(src, dst, skip, 1) == 0) return 1;
        Sleep(1000);
    }
    return 0;
}

/* Unpacks a zip with the tar.exe that comes with Windows 10 (bsdtar reads zip). 1 on success. */
static int extract_zip(const char* zip, const char* dir) {
    char sys[MAX_PATH], cmd[MAX_PATH * 3];
    GetSystemDirectoryA(sys, sizeof(sys));
    CreateDirectoryA(dir, NULL);
    snprintf(cmd, sizeof(cmd), "\"%s\\tar.exe\" -xf \"%s\" -C \"%s\"", sys, zip, dir);
    STARTUPINFOA si = {.cb = sizeof(si)};
    PROCESS_INFORMATION pi;
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) return 0;
    WaitForSingleObject(pi.hProcess, 300000);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code == 0;
}

#endif
