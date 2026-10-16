/* Exercises packaging/windows/bb-update.h against a local fake release server.
 * Usage: test_launcher_update <port> <work dir>; the dir holds package/ (the unpacked release)
 * and install/ (the installation to update, with user files that must survive). */
#include "../packaging/windows/bb-update.h"

static int failures;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL line %d: %s\n", __LINE__, #cond);                   \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static int last_pct = -1, calls, regressions;
static void on_progress(int pct, void* context) {
    (void)context;
    if (pct < last_pct) ++regressions;
    last_pct = pct;
    ++calls;
}

static void count_progress(int done, int total, void* context) {
    (void)total;
    *(int*)context = done;
}

static int file_has(const char* path, const char* text) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    char buf[256] = "";
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = 0;
    fclose(f);
    return strstr(buf, text) != NULL;
}

int main(int argc, char** argv) {
    if (argc < 3) return 2;
    int port = atoi(argv[1]);
    const char* dir = argv[2];
    char url[256], path[MAX_PATH], body[8192];

    /* versions */
    CHECK(version_cmp("1.7.5", "1.7.5") == 0);
    CHECK(version_cmp("1.7.5", "1.8.0") < 0);
    CHECK(version_cmp("1.10.0", "1.9.9") > 0);   /* numbers, not text */
    CHECK(version_cmp("windows-v1.7", "1.7.0") == 0);
    CHECK(version_cmp("2", "1.99.99") > 0);

    /* the releases JSON: tag, the release's own page, the first .zip asset */
    static const char* json =
        "{\"url\":\"https://api.github.com/x\",\"html_url\":\"https://github.com/o/r/releases/tag/windows-v1.8.0\","
        "\"tag_name\":\"windows-v1.8.0\",\"author\":{\"html_url\":\"https://github.com/someone\"},"
        "\"assets\":[{\"name\":\"a.7z\",\"browser_download_url\":\"https://h/a.7z\"},"
        "{\"name\":\"bbport.ZIP\",\"browser_download_url\":\"https://h/dl/bbport-windows.ZIP\"},"
        "{\"name\":\"b.zip\",\"browser_download_url\":\"https://h/b.zip\"}]}";
    ReleaseInfo info;
    CHECK(release_parse(json, &info));
    CHECK(!strcmp(info.version, "1.8.0"));
    CHECK(!strcmp(info.page_url, "https://github.com/o/r/releases/tag/windows-v1.8.0"));
    CHECK(!strcmp(info.zip_url, "https://h/dl/bbport-windows.ZIP"));
    CHECK(!release_parse("{\"message\":\"Not Found\"}", &info));
    CHECK(release_parse("{\"tag_name\":\"v2.0\",\"assets\":[]}", &info) && !info.zip_url[0]);

    /* HTTP */
    snprintf(url, sizeof(url), "http://127.0.0.1:%d/releases/latest", port);
    CHECK(http_get(url, body, sizeof(body)) == 200);
    CHECK(release_parse(body, &info) && !strcmp(info.version, "1.8.0") && strstr(info.zip_url, "/package.zip"));
    snprintf(url, sizeof(url), "http://127.0.0.1:%d/missing", port);
    CHECK(http_get(url, body, sizeof(body)) == 404);
    CHECK(http_get("http://127.0.0.1:1/nothing", body, sizeof(body)) == 0);   /* refused */

    snprintf(path, sizeof(path), "%s\\update.zip", dir);
    CHECK(http_download(info.zip_url, path, on_progress, NULL));
    CHECK(calls > 0 && last_pct == 100 && regressions == 0);
    snprintf(url, sizeof(url), "http://127.0.0.1:%d/missing.zip", port);
    char bad[MAX_PATH];
    snprintf(bad, sizeof(bad), "%s\\bad.zip", dir);
    CHECK(!http_download(url, bad, NULL, NULL));
    CHECK(GetFileAttributesA(bad) == INVALID_FILE_ATTRIBUTES);   /* no partial file left */

    /* unpack, find the new launcher */
    char unpacked[MAX_PATH], exe[MAX_PATH];
    snprintf(unpacked, sizeof(unpacked), "%s\\new", dir);
    CHECK(extract_zip(path, unpacked));
    CHECK(find_file(unpacked, "bb-launcher.exe", exe, sizeof(exe)));
    CHECK(!find_file(unpacked, "nothing.exe", exe, sizeof(exe)));
    CHECK(!extract_zip(bad, unpacked));   /* not a zip */

    /* install over the old files, leaving the user's alone */
    char root[MAX_PATH], install[MAX_PATH], probe[MAX_PATH];
    snprintf(root, sizeof(root), "%s\\new\\bbport-windows", dir);
    snprintf(install, sizeof(install), "%s\\install", dir);
    static const char* skip[] = {"user", "out", "mods", "bbport.ini", "input.ini", "mods.json",
                                 "patches.json", "last_run.log", NULL};
    CHECK(install_copy(root, install, skip, 2));
    snprintf(probe, sizeof(probe), "%s\\bb-launcher.exe", install);
    CHECK(file_has(probe, "NEW LAUNCHER"));
    snprintf(probe, sizeof(probe), "%s\\data\\new.txt", install);
    CHECK(file_has(probe, "fresh"));
    snprintf(probe, sizeof(probe), "%s\\user\\save.dat", install);
    CHECK(file_has(probe, "MY SAVE"));                 /* the package carries a user\ too */
    snprintf(probe, sizeof(probe), "%s\\bbport.ini", install);
    CHECK(file_has(probe, "my settings"));
    snprintf(probe, sizeof(probe), "%s\\mods.json", install);
    CHECK(file_has(probe, "my mod order"));
    snprintf(probe, sizeof(probe), "%s\\mods\\mine\\file.txt", install);
    CHECK(file_has(probe, "my mod"));
    snprintf(probe, sizeof(probe), "%s\\scripts\\deep\\er.py", install);
    CHECK(file_has(probe, "nested"));                  /* subfolders are created */

    /* a missing source is a failure, not success */
    CHECK(!install_copy("C:\\no\\such\\folder", install, skip, 1));

    /* a list of files: all arrive, a missing one stops the run with its index */
    char fsr[MAX_PATH], fsr_base[256];
    snprintf(fsr, sizeof(fsr), "%s\\fsr", dir);
    snprintf(fsr_base, sizeof(fsr_base), "http://127.0.0.1:%d/fsr", port);
    char* good[] = {"rcas.spv", "a_pass1.spv", "b_pre.spv"};
    last_pct = -1;
    int done_seen = 0, total_seen = 0;
    CHECK(download_files(fsr_base, good, 3, fsr, count_progress, &done_seen, NULL));
    (void)total_seen;
    CHECK(done_seen == 3);
    snprintf(probe, sizeof(probe), "%s\\a_pass1.spv", fsr);
    CHECK(file_has(probe, "a_pass1.spv"));              /* the server answers with the name */
    snprintf(probe, sizeof(probe), "%s\\rcas.spv.part", fsr);
    CHECK(GetFileAttributesA(probe) == INVALID_FILE_ATTRIBUTES);
    char* mixed[] = {"c_one.spv", "missing_two.spv", "d_three.spv"};
    int failed = -1;
    CHECK(!download_files(fsr_base, mixed, 3, fsr, NULL, NULL, &failed));
    CHECK(failed == 1);
    snprintf(probe, sizeof(probe), "%s\\c_one.spv", fsr);
    CHECK(file_has(probe, "c_one.spv"));                /* before the failure: kept */
    snprintf(probe, sizeof(probe), "%s\\missing_two.spv", fsr);
    CHECK(GetFileAttributesA(probe) == INVALID_FILE_ATTRIBUTES);
    snprintf(probe, sizeof(probe), "%s\\missing_two.spv.part", fsr);
    CHECK(GetFileAttributesA(probe) == INVALID_FILE_ATTRIBUTES);
    snprintf(probe, sizeof(probe), "%s\\d_three.spv", fsr);
    CHECK(GetFileAttributesA(probe) == INVALID_FILE_ATTRIBUTES);   /* after it: not tried */

    /* cleanup */
    rmtree(unpacked);
    CHECK(GetFileAttributesA(unpacked) == INVALID_FILE_ATTRIBUTES);

    printf(failures ? "FAILED\n" : "OK\n");
    return failures ? 1 : 0;
}
