/* Exercises packaging/windows/bb-settings.h against a settings.json written by the Python
 * launcher. Usage: test_launcher_settings <dir>; the dir holds fixture.json and corrupt.json. */
#include "../packaging/windows/bb-settings.h"

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
    char v[256];

    snprintf(g_cfg, sizeof(g_cfg), "%s\\fixture.json", argv[1]);
    cfg_load();
    CHECK(!g_cfg_bad);
    CHECK(cfg_get_str("game_dir", v, sizeof(v), NULL) && !strcmp(v, "F:\\games\\CUSA03173"));
    CHECK(cfg_get_str("player_name", v, sizeof(v), NULL) && !strcmp(v, "Chasseur \xC3\xA9t\xC3\xA9 \xF0\x9F\x8C\x99"));
    CHECK(cfg_get_str("mods_enabled", v, sizeof(v), NULL) && !strcmp(v, "1"));
    CHECK(cfg_get_str("frame_stats", v, sizeof(v), NULL) && !strcmp(v, "0"));
    CHECK(cfg_get_str("language", v, sizeof(v), NULL) && !strcmp(v, "1"));
    CHECK(!cfg_get_str("missing", v, sizeof(v), "fallback") && !strcmp(v, "fallback"));
    CHECK(!cfg_get_str("nothing", v, sizeof(v), "dflt") && !strcmp(v, "dflt")); /* JSON null */

    /* Edit known keys, add a new one, save, reload. */
    cfg_set_str("game_dir", "E:\\My \"Games\"\\CUSA03173");
    cfg_set_bool("frame_stats", 1);
    cfg_set_str("frame_cap", "120");
    CHECK(cfg_save());
    cfg_load();
    CHECK(!g_cfg_bad);
    CHECK(cfg_get_str("game_dir", v, sizeof(v), NULL) && !strcmp(v, "E:\\My \"Games\"\\CUSA03173"));
    CHECK(cfg_get_str("frame_stats", v, sizeof(v), NULL) && !strcmp(v, "1"));
    CHECK(cfg_get_str("frame_cap", v, sizeof(v), NULL) && !strcmp(v, "120"));
    /* Untouched keys, nested values and secrets come back unchanged. */
    CHECK(cfg_get_str("online_password", v, sizeof(v), NULL) && !strcmp(v, "p\"w\\d"));
    CHECK(cfg_find("remap") >= 0 && g_kv[cfg_find("remap")].raw[0] == '{' &&
          strstr(g_kv[cfg_find("remap")].raw, "\"}\""));
    CHECK(cfg_find("ratio") >= 0 && !strcmp(g_kv[cfg_find("ratio")].raw, "0.5"));
    CHECK(cfg_get_str("player_name", v, sizeof(v), NULL) && !strcmp(v, "Chasseur \xC3\xA9t\xC3\xA9 \xF0\x9F\x8C\x99"));

    /* A file that does not parse to the end is backed up before the first save. */
    snprintf(g_cfg, sizeof(g_cfg), "%s\\corrupt.json", argv[1]);
    cfg_load();
    CHECK(g_cfg_bad);
    cfg_set_bool("frame_stats", 1);
    CHECK(cfg_save());
    char bak[MAX_PATH + 8];
    snprintf(bak, sizeof(bak), "%s.bak", g_cfg);
    CHECK(GetFileAttributesA(bak) != INVALID_FILE_ATTRIBUTES);

    /* A missing file is an empty store. */
    snprintf(g_cfg, sizeof(g_cfg), "%s\\absent.json", argv[1]);
    cfg_load();
    CHECK(g_kvn == 0 && !g_cfg_bad);

    printf(failures ? "FAILED\n" : "OK\n");
    return failures ? 1 : 0;
}
