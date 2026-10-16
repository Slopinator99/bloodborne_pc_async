/* bb-launcher: classic native launcher for bbport (Windows).
 * Plain Win32, no frameworks: game folder, upscaler/preset/output/FPS, checkboxes,
 * Run + Advanced. Advanced shows the full settings in tabs (effects, gameplay,
 * mods, performance). Writes bbport.ini and starts run.py.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shobjidl.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bb-settings.h"

#define ID_GAME_EDIT 101
#define ID_BROWSE 102
#define ID_UPSCALER 103
#define ID_PRESET 104
#define ID_OUTPUT 105
#define ID_FPS 106
#define ID_FULLSCREEN 107
#define ID_SKIP_INTRO 108
#define ID_ASYNC 109
#define ID_STATS 110
#define ID_FPS_SHOW 111
#define ID_RUN 112
#define ID_ADVANCED 113
#define ID_USER_EDIT 114
#define ID_ENV_EDIT 115
#define ID_STATUS 116
#define ID_TABS 117
#define ID_USER_LBL 118
#define ID_ENV_LBL 119
/* Tab page controls start here (created per page, ids must stay unique). */
#define ID_FX_BASE 200
#define ID_GP_BASE 230
#define ID_MODS_ON 260
#define ID_MODS_EDIT 261
#define ID_PATCH_EDIT 262
#define ID_PRESENT 263
#define ID_DRAWPIPE 264
#define ID_READBACKS 265
#define ID_AHEAD 266
#define ID_CAP 267
#define ID_USER_BROWSE 268
#define ID_MODS_BROWSE 269
#define ID_PATCH_BROWSE 270

static const char* kUpscalers[] = {"taa", "fsr3", "fsr4", "fsr411", "off"};
static const char* kUpscalerLabels[] = {"TAA (native)", "FSR 3.1", "FSR 4", "FSR 4.1.1", "Off"};
static const char* kPresets[] = {"Native AA (x1.0)", "Quality (x1.5)", "Balanced (x1.7)",
                                 "Performance (x2.0)", "Ultra Performance (x3.0)"};
static const char* kOutputs[] = {"1280x720", "1920x1080", "2560x1440", "3840x2160"};
static const char* kFps[] = {"uncap", "60", "90", "30"};
static const char* kFpsLabels[] = {"Unlocked", "60 FPS", "90 FPS", "30 FPS (PS4)"};
static const char* kPresent[] = {"Mailbox", "Fifo", "FifoRelaxed", "Immediate"};
static const char* kPipe[] = {"", "1", "0"};
static const char* kPipeLabels[] = {"Auto (8+ threads)", "On", "Off (stable)"};
static const char* kRb[] = {"", "0", "2"};
static const char* kRbLabels[] = {"Relaxed (default)", "Off", "Precise"};
static const char* kAhead[] = {"", "2", "0"};
static const char* kAheadLabels[] = {"1 (default)", "2", "Unbounded"};
static const char* kCaps[] = {"", "60", "90", "120", "144", "0"};
static const char* kCapLabels[] = {"Auto (max 120)", "60", "90", "120", "144", "No limit"};

/* Effects tab (real bbport.ini keys, defaults = game behaviour). */
static const struct { const char* key; const char* label; int dflt; } kFx[] = {
    {"effect_chromatic_aberration", "Chromatic aberration", 1},
    {"effect_dof", "Depth of field (DoF)", 1},
    {"effect_motion_blur", "Motion blur", 1},
    {"effect_ssao", "Ambient occlusion (SSAO)", 1},
    {"effect_dynamic_shadows", "Dynamic light shadows", 1},
    {"effect_game_aa", "The game's own AA (off w/ TAA)", 0},
    {"effect_ssr", "Screen-space reflections", 0},
};
/* Gameplay tab. */
static const struct { const char* key; const char* label; int dflt; } kGp[] = {
    {"debug_camera", "Free camera (Cross + L3)", 0},
    {"debug_menu", "Game debug menu (needs fonts)", 0},
    {"cheat_no_death", "Never die (1 HP)", 0},
    {"cheat_stealth", "Enemies blind (unless hit)", 0},
    {"cheat_silent", "Enemies deaf", 0},
    {"cheat_rally_no_decay", "Rally never fades", 0},
    {"cheat_enemy_control", "Control enemy (R3)", 0},
    {"tweak_no_rally", "No rally HP return", 0},
    {"tweak_camera_distance", "Camera farther", 0},
    {"tweak_no_camera_rotation", "No camera auto-turn", 0},
    {"tweak_easy_run", "Easier run (stick)", 0},
    {"tweak_ragdoll", "Souls-like ragdolls", 0},
};

static HWND g_main, g_status, g_tabs;
static HWND g_pages[4];
static char g_root[MAX_PATH];
static char g_ini[MAX_PATH];
static char g_runpy[MAX_PATH];
static int g_advanced, g_running;
static HANDLE g_child;
/* DPI scaling: every layout pixel goes through S(). The window declares
 * PerMonitorV2, so on 125/150% displays fixed pixels would overlap. */
static int g_dpi = 96;
static HFONT g_font;
static int S(int v) {
    return (v * g_dpi + 48) / 96;
}

/* --- small widget helpers (parent = any container) --- */
static HWND w_label(HWND p, int id, const char* t, int x, int y, int w) {
    HWND h = CreateWindowA("STATIC", t, WS_CHILD | WS_VISIBLE, S(x), S(y), S(w), S(16), p,
                           (HMENU)(INT_PTR)id, NULL, NULL);
    SendMessageA(h, WM_SETFONT, (WPARAM)g_font, 1);
    return h;
}

static HWND w_edit(HWND p, int id, int x, int y, int w) {
    HWND h = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                             ES_AUTOHSCROLL, S(x), S(y), S(w), S(22), p, (HMENU)(INT_PTR)id,
                             NULL, NULL);
    SendMessageA(h, WM_SETFONT, (WPARAM)g_font, 1);
    return h;
}

static HWND w_combo(HWND p, int id, int x, int y, int w, const char** items, int n) {
    HWND h = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                           S(x), S(y), S(w), S(160), p, (HMENU)(INT_PTR)id, NULL, NULL);
    SendMessageA(h, WM_SETFONT, (WPARAM)g_font, 1);
    for (int i = 0; i < n; ++i) SendMessageA(h, CB_ADDSTRING, 0, (LPARAM)items[i]);
    return h;
}

static HWND w_check(HWND p, int id, const char* t, int x, int y, int w) {
    HWND h = CreateWindowA("BUTTON", t, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                           S(x), S(y), S(w), S(20), p, (HMENU)(INT_PTR)id, NULL, NULL);
    SendMessageA(h, WM_SETFONT, (WPARAM)g_font, 1);
    return h;
}

static HWND w_button(HWND p, int id, const char* t, int x, int y, int wd, int ht) {
    /* Standard button + visual-styles manifest (bb-launcher.manifest): native
     * hover fade and press animations, no custom painting to go wrong. */
    HWND h = CreateWindowA("BUTTON", t, WS_CHILD | WS_VISIBLE | WS_TABSTOP, S(x), S(y),
                           S(wd), S(ht), p, (HMENU)(INT_PTR)id, NULL, NULL);
    SendMessageA(h, WM_SETFONT, (WPARAM)g_font, 1);
    return h;
}

static void set_text(int id, const char* s) {
    SetWindowTextA(GetDlgItem(g_main, id), s);
}

static void get_text(int id, char* out, size_t n) {
    GetWindowTextA(GetDlgItem(g_main, id), out, (int)n);
}

static void page_get(HWND page, int id, char* out, size_t n) {
    GetWindowTextA(GetDlgItem(page, id), out, (int)n);
}

static void set_combo(int id, int i) {
    SendMessageA(GetDlgItem(g_main, id), CB_SETCURSEL, (WPARAM)(i < 0 ? 0 : i), 0);
}

static int get_combo(int id) {
    return (int)SendMessageA(GetDlgItem(g_main, id), CB_GETCURSEL, 0, 0);
}

static void set_check(int id, int on) {
    SendMessageA(GetDlgItem(g_main, id), BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
}

static int get_check(int id) {
    return SendMessageA(GetDlgItem(g_main, id), BM_GETCHECK, 0, 0) == BST_CHECKED;
}

static void status(const char* s) {
    SetWindowTextA(g_status, s);
}

/* --- bbport.ini: keep unknown lines, update managed keys --- */
static void ini_get(const char* key, char* out, size_t n, const char* dflt) {
    snprintf(out, n, "%s", dflt);
    FILE* f = fopen(g_ini, "r");
    if (!f) return;
    char line[512];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (!strncmp(line, key, klen) && line[klen] == '=') {
            char* v = line + klen + 1;
            v[strcspn(v, "\r\n")] = 0;
            snprintf(out, n, "%s", v);
            break;
        }
    }
    fclose(f);
}

/* A launcher option: settings.json (shared with bb-runtime) first, then the launcher_* key older
 * versions of this launcher kept in bbport.ini, then the default. Booleans read as "1"/"0". */
static void app_get(const char* key, const char* legacy, char* out, size_t n, const char* dflt) {
    if (cfg_get_str(key, out, n, NULL)) return;
    if (legacy && legacy[0]) ini_get(legacy, out, n, dflt);
    else snprintf(out, n, "%s", dflt);
}

static int combo_index(const char** vals, int n, const char* v, int dflt) {
    for (int i = 0; i < n; ++i)
        if (!strcmp(v, vals[i])) return i;
    return dflt;
}

static void load_settings(void) {
    char v[512], game[MAX_PATH];
    /* Remembered folder first; fall back to a CUSA03173 next to the checkout. */
    app_get("game_dir", "launcher_game_dir", game, sizeof(game), "");
    DWORD attrs = GetFileAttributesA(game);
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        snprintf(game, sizeof(game), "%s\\..\\CUSA03173", g_root);
        char norm[MAX_PATH];
        GetFullPathNameA(game, sizeof(norm), norm, NULL);
        attrs = GetFileAttributesA(norm);
        if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY))
            snprintf(norm, sizeof(norm), "%s", g_root);
        snprintf(game, sizeof(game), "%s", norm);
    }
    set_text(ID_GAME_EDIT, game);

    ini_get("upscaler", v, sizeof(v), "taa");
    set_combo(ID_UPSCALER, combo_index(kUpscalers, 5, v, 0));
    ini_get("preset", v, sizeof(v), "0");
    int p = atoi(v);
    set_combo(ID_PRESET, (p < 0 || p > 4) ? 0 : p);
    ini_get("output_res", v, sizeof(v), "1920x1080");
    set_combo(ID_OUTPUT, combo_index(kOutputs, 4, v, 1));
    app_get("fps_mode", "launcher_fps", v, sizeof(v), "60");
    set_combo(ID_FPS, combo_index(kFps, 4, v, 1));
    ini_get("skip_intro", v, sizeof(v), "1");
    set_check(ID_SKIP_INTRO, atoi(v) != 0);
    ini_get("show_fps", v, sizeof(v), "1");
    set_check(ID_FPS_SHOW, atoi(v) != 0);
    app_get("fullscreen", "launcher_fullscreen", v, sizeof(v), "0");
    set_check(ID_FULLSCREEN, atoi(v) != 0);
    app_get("user_dir", "launcher_user_dir", v, sizeof(v), "");
    set_text(ID_USER_EDIT, v);
    app_get("async_pipelines", "launcher_async", v, sizeof(v), "0");
    set_check(ID_ASYNC, atoi(v) != 0);
    app_get("frame_stats", "launcher_stats", v, sizeof(v), "1");
    set_check(ID_STATS, atoi(v) != 0);
    app_get("extra_env", "", v, sizeof(v), "");
    set_text(ID_ENV_EDIT, v);
    /* Tabs. */
    for (int i = 0; i < 7; ++i) {
        char d[4];
        snprintf(d, sizeof(d), "%d", kFx[i].dflt);
        ini_get(kFx[i].key, v, sizeof(v), d);
        SendMessageA(GetDlgItem(g_pages[0], ID_FX_BASE + i), BM_SETCHECK,
                     atoi(v) ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    for (int i = 0; i < 12; ++i) {
        ini_get(kGp[i].key, v, sizeof(v), "0");
        SendMessageA(GetDlgItem(g_pages[1], ID_GP_BASE + i), BM_SETCHECK,
                     atoi(v) ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    app_get("mods_enabled", "launcher_mods_on", v, sizeof(v), "1");
    SendMessageA(GetDlgItem(g_pages[2], ID_MODS_ON), BM_SETCHECK,
                 atoi(v) ? BST_CHECKED : BST_UNCHECKED, 0);
    app_get("mods_dir", "launcher_mods_dir", v, sizeof(v), "");
    SetWindowTextA(GetDlgItem(g_pages[2], ID_MODS_EDIT), v);
    app_get("patches_dir", "launcher_patches_dir", v, sizeof(v), "");
    SetWindowTextA(GetDlgItem(g_pages[2], ID_PATCH_EDIT), v);
    app_get("present_mode", "launcher_present", v, sizeof(v), "Mailbox");
    SendMessageA(GetDlgItem(g_pages[3], ID_PRESENT), CB_SETCURSEL,
                 (WPARAM)combo_index(kPresent, 4, v, 0), 0);
    app_get("draw_pipe", "launcher_draw_pipe", v, sizeof(v), "");
    SendMessageA(GetDlgItem(g_pages[3], ID_DRAWPIPE), CB_SETCURSEL,
                 (WPARAM)combo_index(kPipe, 3, v, 0), 0);
    app_get("readbacks", "launcher_readbacks", v, sizeof(v), "");
    SendMessageA(GetDlgItem(g_pages[3], ID_READBACKS), CB_SETCURSEL,
                 (WPARAM)combo_index(kRb, 3, v, 0), 0);
    app_get("frames_ahead", "launcher_ahead", v, sizeof(v), "");
    SendMessageA(GetDlgItem(g_pages[3], ID_AHEAD), CB_SETCURSEL,
                 (WPARAM)combo_index(kAhead, 3, v, 0), 0);
    app_get("frame_cap", "launcher_cap", v, sizeof(v), "");
    SendMessageA(GetDlgItem(g_pages[3], ID_CAP), CB_SETCURSEL,
                 (WPARAM)combo_index(kCaps, 6, v, 0), 0);
}

static int is_managed(const char* line) {
    static const char* keys[] = {"upscaler", "preset", "output_res", "effect_game_aa",
                                 "skip_intro", "show_fps", "launcher_game_dir",
                                 "launcher_user_dir", "launcher_fps", "launcher_fullscreen",
                                 "launcher_mods_on", "launcher_mods_dir", "launcher_patches_dir",
                                 "launcher_present", "launcher_draw_pipe",
                                 "launcher_readbacks", "launcher_ahead", "launcher_cap",
                                 "launcher_async", "launcher_stats"};
    for (int i = 0; i < 7; ++i) {
        size_t k = strlen(kFx[i].key);
        if (!strncmp(line, kFx[i].key, k) && line[k] == '=') return 1;
    }
    for (int i = 0; i < 12; ++i) {
        size_t k = strlen(kGp[i].key);
        if (!strncmp(line, kGp[i].key, k) && line[k] == '=') return 1;
    }
    for (unsigned i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        size_t k = strlen(keys[i]);
        if (!strncmp(line, keys[i], k) && line[k] == '=') return 1;
    }
    return 0;
}

static int combo_val(HWND page, int id, const char** vals, int n) {
    int i = (int)SendMessageA(GetDlgItem(page, id), CB_GETCURSEL, 0, 0);
    (void)n;
    return i;
}

/* Options only bb-runtime has pages for so far: applied from the shared settings.json so a game
 * started here behaves as it does from there (language, voice, name, DLC, HDR, validation). */
static void shared_env(void) {
    char v[MAX_PATH], path[MAX_PATH];
    if (cfg_get_str("dlc_dir", v, sizeof(v), NULL) && v[0]) SetEnvironmentVariableA("BB_DLC_DIR", v);
    snprintf(path, sizeof(path), "%s\\mods.json", g_root);
    SetEnvironmentVariableA("BB_MODS_CONFIG", path);
    snprintf(path, sizeof(path), "%s\\patches.json", g_root);
    SetEnvironmentVariableA("BB_PATCHES_CONFIG", path);
    snprintf(path, sizeof(path), "%s\\mods", g_root);
    SetEnvironmentVariableA("BB_MODS_DIR", path);
    snprintf(path, sizeof(path), "%s\\patches", g_root);
    SetEnvironmentVariableA("BB_PATCHES_DIR", path);
    cfg_get_str("language", v, sizeof(v), "1");
    SetEnvironmentVariableA("BB_LANGUAGE", v);
    cfg_get_str("voice_language", v, sizeof(v), "auto");
    SetEnvironmentVariableA("BB_VOICE_LANGUAGE", v);
    if (cfg_get_str("player_name", v, sizeof(v), NULL)) {
        char* b = v;
        while (*b == ' ') ++b;
        size_t len = strlen(b);
        while (len && b[len - 1] == ' ') b[--len] = 0;
        if (*b) SetEnvironmentVariableA("BB_USER_NAME", b);
    }
    if (cfg_get_str("hdr", v, sizeof(v), "0") && !strcmp(v, "1")) SetEnvironmentVariableA("BB_HDR", "1");
    if (cfg_get_str("vk_validation", v, sizeof(v), "0") && !strcmp(v, "1"))
        SetEnvironmentVariableA("BB_VK_VALIDATION", "1");
}

static void save_settings(void) {
    char game[MAX_PATH], user[MAX_PATH], env[1024], mods[512], patches[512];
    get_text(ID_GAME_EDIT, game, sizeof(game));
    get_text(ID_USER_EDIT, user, sizeof(user));
    get_text(ID_ENV_EDIT, env, sizeof(env));
    page_get(g_pages[2], ID_MODS_EDIT, mods, sizeof(mods));
    page_get(g_pages[2], ID_PATCH_EDIT, patches, sizeof(patches));

    int u = get_combo(ID_UPSCALER);
    int p = get_combo(ID_PRESET);
    int o = get_combo(ID_OUTPUT);
    if (u < 0) u = 0;
    if (p < 0) p = 0;
    if (o < 0) o = 1;

    char rest[16384] = "";
    FILE* f = fopen(g_ini, "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            if (!is_managed(line) && strlen(rest) + strlen(line) < sizeof(rest) - 1)
                strcat(rest, line);
        }
        fclose(f);
    }
    f = fopen(g_ini, "w");
    if (!f) {
        status("Cannot write bbport.ini");
        return;
    }
    fprintf(f, "# written by bb-launcher (classic)\nupscaler=%s\npreset=%d\noutput_res=%s\n",
            kUpscalers[u], p, kOutputs[o]);
    fprintf(f, "effect_game_aa=0\nskip_intro=%d\nshow_fps=%d\n", get_check(ID_SKIP_INTRO),
            get_check(ID_FPS_SHOW));
    for (int i = 0; i < 7; ++i)
        fprintf(f, "%s=%d\n", kFx[i].key,
                SendMessageA(GetDlgItem(g_pages[0], ID_FX_BASE + i), BM_GETCHECK, 0, 0) ==
                        BST_CHECKED);
    for (int i = 0; i < 12; ++i)
        fprintf(f, "%s=%d\n", kGp[i].key,
                SendMessageA(GetDlgItem(g_pages[1], ID_GP_BASE + i), BM_GETCHECK, 0, 0) ==
                        BST_CHECKED);
    int mods_on = SendMessageA(GetDlgItem(g_pages[2], ID_MODS_ON), BM_GETCHECK, 0, 0) ==
                  BST_CHECKED;
    int pr = combo_val(g_pages[3], ID_PRESENT, kPresent, 4);
    int dp = combo_val(g_pages[3], ID_DRAWPIPE, kPipe, 3);
    int rb = combo_val(g_pages[3], ID_READBACKS, kRb, 3);
    int ah = combo_val(g_pages[3], ID_AHEAD, kAhead, 3);
    int cp = combo_val(g_pages[3], ID_CAP, kCaps, 6);
    int fi = get_combo(ID_FPS);
    if (fi < 0) fi = 1;
    fputs(rest, f);
    fclose(f);

    /* The launcher options go to settings.json, where bb-runtime reads them too. */
    cfg_set_str("game_dir", game);
    cfg_set_str("user_dir", user);
    cfg_set_str("fps_mode", kFps[fi]);
    cfg_set_bool("fullscreen", get_check(ID_FULLSCREEN));
    cfg_set_bool("async_pipelines", get_check(ID_ASYNC));
    cfg_set_bool("frame_stats", get_check(ID_STATS));
    cfg_set_bool("gpu_profile", get_check(ID_STATS));
    cfg_set_bool("mods_enabled", mods_on);
    cfg_set_str("mods_dir", mods);
    cfg_set_str("patches_dir", patches);
    cfg_set_str("present_mode", pr >= 0 ? kPresent[pr] : "Mailbox");
    cfg_set_str("draw_pipe", dp > 0 ? kPipe[dp] : "");
    cfg_set_str("readbacks", rb > 0 ? kRb[rb] : "");
    cfg_set_str("frames_ahead", ah > 0 ? kAhead[ah] : "");
    cfg_set_str("frame_cap", cp > 0 ? kCaps[cp] : "");
    cfg_set_str("extra_env", env);
    if (!cfg_save()) status("Cannot write settings.json");
    shared_env();

    SetEnvironmentVariableA("BB_GAME_DIR", game);
    SetEnvironmentVariableA("BB_FPS", kFps[fi]);
    /* Unbuffered child output: the log file stays live while playing. */
    SetEnvironmentVariableA("PYTHONUNBUFFERED", "1");
    SetEnvironmentVariableA("PYTHONIOENCODING", "utf-8");
    /* Match run.py: uncap runs free, 90 gets a 90 Hz vblank, else 60. */
    SetEnvironmentVariableA("BB_VBLANK_HZ", !strcmp(kFps[fi], "uncap") ? "0" :
                                             !strcmp(kFps[fi], "90") ? "90" : "60");
    SetEnvironmentVariableA("BB_UPSCALER", kUpscalers[u]);
    SetEnvironmentVariableA("BB_PRESENT_MODE", pr >= 0 ? kPresent[pr] : "Mailbox");
    if (get_check(ID_FULLSCREEN)) SetEnvironmentVariableA("BB_FULLSCREEN", "1");
    if (get_check(ID_ASYNC)) SetEnvironmentVariableA("BB_ASYNC_PIPELINES", "1");
    if (get_check(ID_STATS)) {
        SetEnvironmentVariableA("BB_FRAME_STATS", "1");
        SetEnvironmentVariableA("BB_GPU_PROFILE", "1");
    }
    if (user[0]) SetEnvironmentVariableA("BB_USER_DIR", user);
    SetEnvironmentVariableA("BB_MODS_ENABLED", mods_on ? "1" : "0");
    if (mods[0]) SetEnvironmentVariableA("BB_MODS_DIR", mods);
    if (patches[0]) SetEnvironmentVariableA("BB_PATCHES_DIR", patches);
    if (dp > 0) SetEnvironmentVariableA("BB_DRAW_PIPE", kPipe[dp]);
    if (rb > 0) SetEnvironmentVariableA("BB_READBACKS", kRb[rb]);
    if (ah > 0) SetEnvironmentVariableA("BB_FRAMES_AHEAD", kAhead[ah]);
    if (cp > 0) SetEnvironmentVariableA("BB_FPS_LIMIT", kCaps[cp]);
    for (char* tok = strtok(env, " "); tok; tok = strtok(NULL, " ")) {
        char* eq = strchr(tok, '=');
        if (eq) {
            *eq = 0;
            SetEnvironmentVariableA(tok, eq + 1);
        }
    }
}

/* --- run / browse / watch --- */
static int find_python(char* out, size_t n) {
    DWORD a = GetFileAttributesA("C:\\msys64\\clang64\\bin\\python.exe");
    if (a != INVALID_FILE_ATTRIBUTES) {
        snprintf(out, n, "\"C:\\msys64\\clang64\\bin\\python.exe\"");
        return 1;
    }
    snprintf(out, n, "python");
    return 1;
}

/* The frozen Python runtime next to the game: bb-runtime.exe in current packages,
 * Bloodborne.exe in older ones (accepted while players mix old and new files). */
static void frozen_runtime(const char* dir, char* out, size_t n) {
    static const char* names[] = {"bb-runtime.exe", "Bloodborne.exe"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        snprintf(out, n, "%s\\%s", dir, names[i]);
        if (GetFileAttributesA(out) != INVALID_FILE_ATTRIBUTES) {
            return;
        }
    }
    snprintf(out, n, "%s\\bb-runtime.exe", dir);
}

static DWORD WINAPI watch_thread(LPVOID unused) {
    (void)unused;
    WaitForSingleObject(g_child, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(g_child, &code);
    CloseHandle(g_child);
    g_child = NULL;
    g_running = 0;
    char msg[128];
    snprintf(msg, sizeof(msg), "Game exited (%lu). %s", (unsigned long)code,
             code ? "See launcher-run.log." : "Ready.");
    status(msg);
    EnableWindow(GetDlgItem(g_main, ID_RUN), TRUE);
    if (code) {
        /* Surface the log tail: bb-probe's own stdout may predate the crash. */
        char path[MAX_PATH], tail[2048] = "";
        snprintf(path, sizeof(path), "%s\\launcher-run.log", g_root);
        FILE* f = fopen(path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            long start = size > 1500 ? size - 1500 : 0;
            fseek(f, start, SEEK_SET);
            size_t n = fread(tail, 1, sizeof(tail) - 1, f);
            tail[n] = 0;
            fclose(f);
            char* first = strchr(tail, '\n');
            snprintf(msg, sizeof(msg), "The game exited with code %lu.\n\nLog tail:\n%s",
                     (unsigned long)code, first ? first + 1 : tail);
            MessageBoxA(g_main, msg, "Bloodborne", MB_OK | MB_ICONWARNING);
        }
    }
    return 0;
}

static void on_run(void) {
    if (g_running) return;
    char game[MAX_PATH];
    get_text(ID_GAME_EDIT, game, sizeof(game));
    char eboot[MAX_PATH];
    snprintf(eboot, sizeof(eboot), "%s\\eboot.bin", game);
    if (GetFileAttributesA(eboot) == INVALID_FILE_ATTRIBUTES) {
        status("No eboot.bin in the game folder.");
        MessageBoxA(g_main, "Pick the decrypted CUSA03173 folder (eboot.bin inside).",
                    "Bloodborne", MB_OK | MB_ICONWARNING);
        return;
    }
    save_settings();
    /* Packaged: the frozen runtime next to us runs run.py itself (--run),
     * so players need no Python. Dev tree: python run.py. */
    char frozen[MAX_PATH], cmd[2048];
    frozen_runtime(g_root, frozen, sizeof(frozen));
    /* g_root is the repo root in dev (exe lives in out/); the package root is
     * where the exe lives. Check both. */
    char exedir[MAX_PATH];
    GetModuleFileNameA(NULL, exedir, sizeof(exedir));
    *strrchr(exedir, '\\') = 0;
    char frozen_here[MAX_PATH];
    frozen_runtime(exedir, frozen_here, sizeof(frozen_here));
    const char* runner = NULL;
    if (GetFileAttributesA(frozen_here) != INVALID_FILE_ATTRIBUTES) {
        snprintf(cmd, sizeof(cmd), "\"%s\" --run", frozen_here);
        runner = frozen_here;
    } else if (GetFileAttributesA(frozen) != INVALID_FILE_ATTRIBUTES &&
               strcmp(exedir, g_root)) {
        snprintf(cmd, sizeof(cmd), "\"%s\" --run", frozen);
        runner = frozen;
    } else if (GetFileAttributesA(g_runpy) != INVALID_FILE_ATTRIBUTES) {
        char python[512];
        find_python(python, sizeof(python));
        /* Fail loud instead of running a Store stub that prints 'py help'. */
        if (!strcmp(python, "python")) {
            char where[512] = "";
            DWORD found = SearchPathA(NULL, "python", ".exe", sizeof(where), where, NULL);
            if (!found || strstr(where, "WindowsApps")) {
                MessageBoxA(g_main,
                            "No usable Python found and no packaged runtime.\n\n"
                            "Developers: install Python 3.10+ or MSYS2 CLANG64 python.\n"
                            "Players: use the packaged build (bb-runtime.exe next to this file).",
                            "Bloodborne", MB_OK | MB_ICONWARNING);
                status("No Python and no packaged build.");
                return;
            }
        }
        snprintf(cmd, sizeof(cmd), "%s \"%s\"", python, g_runpy);
        runner = python;
    } else {
        status("run.py not found (packaged builds use bb-runtime.exe).");
        MessageBoxA(g_main, "run.py was not found. Packaged players: use bb-runtime.exe.",
                    "Bloodborne", MB_OK | MB_ICONWARNING);
        return;
    }
    (void)runner;
    /* Silent: no console window; the game's output goes to launcher-run.log. */
    char logpath[MAX_PATH];
    snprintf(logpath, sizeof(logpath), "%s\\launcher-run.log", g_root);
    SECURITY_ATTRIBUTES sa = {.nLength = sizeof(sa), .lpSecurityDescriptor = NULL,
                                .bInheritHandle = TRUE};
    HANDLE login = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                               OPEN_EXISTING, 0, NULL);
    HANDLE logout = CreateFileA(logpath, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL, NULL);
    STARTUPINFOA si = {.cb = sizeof(si)};
    DWORD flags = CREATE_NO_WINDOW;
    if (login != INVALID_HANDLE_VALUE && logout != INVALID_HANDLE_VALUE) {
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = login;
        si.hStdOutput = logout;
        si.hStdError = logout;
    }
    PROCESS_INFORMATION pi;
    BOOL ok = CreateProcessA(NULL, cmd, NULL, NULL, TRUE, flags, NULL, g_root, &si, &pi);
    if (login != INVALID_HANDLE_VALUE) CloseHandle(login);
    if (logout != INVALID_HANDLE_VALUE) CloseHandle(logout);
    if (!ok) {
        status("Failed to start run.py.");
        return;
    }
    CloseHandle(pi.hThread);
    g_child = pi.hProcess;
    g_running = 1;
    EnableWindow(GetDlgItem(g_main, ID_RUN), FALSE);
    status("Game starting silently (launcher-run.log)...");
    CloseHandle(CreateThread(NULL, 0, watch_thread, NULL, 0, NULL));
}

    /* Modern folder picker: works on every drive. target lives on page/parent. */
static void pick_folder_into(HWND parent, int edit_id, const wchar_t* title) {
    IFileOpenDialog* dlg = NULL;
    if (FAILED(CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                                &IID_IFileOpenDialog, (void**)&dlg)))
        return;
    DWORD opts = 0;
    dlg->lpVtbl->GetOptions(dlg, &opts);
    dlg->lpVtbl->SetOptions(dlg, opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    dlg->lpVtbl->SetTitle(dlg, title);
    if (SUCCEEDED(dlg->lpVtbl->Show(dlg, g_main))) {
        IShellItem* item = NULL;
        if (SUCCEEDED(dlg->lpVtbl->GetResult(dlg, &item))) {
            wchar_t* path = NULL;
            if (SUCCEEDED(item->lpVtbl->GetDisplayName(item, SIGDN_FILESYSPATH, &path))) {
                char mb[MAX_PATH];
                WideCharToMultiByte(CP_UTF8, 0, path, -1, mb, sizeof(mb), NULL, NULL);
                SetWindowTextA(GetDlgItem(parent, edit_id), mb);
                CoTaskMemFree(path);
            }
            item->lpVtbl->Release(item);
        }
    }
    dlg->lpVtbl->Release(dlg);
}

static void on_browse(void) {
    pick_folder_into(g_main, ID_GAME_EDIT, L"Pick the CUSA03173 game folder");
}

/* --- tab pages --- */
static HWND make_page(void) {
    /* Same wndproc as main so page buttons (browse) deliver WM_COMMAND; only the
     * main window owns Close/quit. */
    HWND h = CreateWindowA("BbPage", "", WS_CHILD, 0, 0, 100, 100, g_main, NULL, NULL, NULL);
    return h;
}

static void place_pages(void) {
    RECT rc;
    GetWindowRect(g_tabs, &rc);
    MapWindowPoints(NULL, g_main, (POINT*)&rc, 2);
    TabCtrl_AdjustRect(g_tabs, FALSE, &rc);
    for (int i = 0; i < 4; ++i)
        SetWindowPos(g_pages[i], NULL, rc.left, rc.top, rc.right - rc.left,
                     rc.bottom - rc.top, SWP_NOZORDER);
}

static void build_pages(void) {
    /* Effects (7 rows fit the tab display area). */
    g_pages[0] = make_page();
    for (int i = 0; i < 7; ++i)
        w_check(g_pages[0], ID_FX_BASE + i, kFx[i].label, 12, 6 + i * 22, 270);
    /* Gameplay: two columns of six. WS_VISIBLE is required (manual creation). */
    g_pages[1] = make_page();
    for (int i = 0; i < 12; ++i) {
        int col = i / 6, row = i % 6;
        HWND h = CreateWindowA("BUTTON", kGp[i].label, WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                               BS_AUTOCHECKBOX, S(12 + col * 150), S(8 + row * 24), S(145),
                               S(20), g_pages[1], (HMENU)(INT_PTR)(ID_GP_BASE + i), NULL, NULL);
        SendMessageA(h, WM_SETFONT, (WPARAM)g_font, 1);
    }
    /* Mods. */
    g_pages[2] = make_page();
    w_check(g_pages[2], ID_MODS_ON, "Enable loose-file mods", 12, 8, 270);
    w_label(g_pages[2], 0, "Mods folder:", 12, 38, 90);
    w_edit(g_pages[2], ID_MODS_EDIT, 12, 56, 220);
    w_button(g_pages[2], ID_MODS_BROWSE, "...", 236, 54, 42, 26);
    w_label(g_pages[2], 0, "Extra patches folder:", 12, 86, 150);
    w_edit(g_pages[2], ID_PATCH_EDIT, 12, 104, 220);
    w_button(g_pages[2], ID_PATCH_BROWSE, "...", 236, 102, 42, 26);
    /* Performance. */
    g_pages[3] = make_page();
    const struct {
        int id;
        const char* lab;
        const char** items;
        int n;
    } rows[] = {{ID_PRESENT, "Presentation:", kPresent, 4},
                {ID_DRAWPIPE, "GPU draw pipe:", kPipeLabels, 3},
                {ID_READBACKS, "GPU readbacks:", kRbLabels, 3},
                {ID_AHEAD, "Frames ahead:", kAheadLabels, 3},
                {ID_CAP, "Frame cap:", kCapLabels, 6}};
    for (int i = 0; i < 5; ++i) {
        w_label(g_pages[3], 0, rows[i].lab, 12, 10 + i * 30, 110);
        w_combo(g_pages[3], rows[i].id, 124, 8 + i * 30, 158, rows[i].items, rows[i].n);
    }
    for (int i = 0; i < 4; ++i) ShowWindow(g_pages[i], SW_HIDE);
}

static void layout(int advanced) {
    int show = advanced ? SW_SHOW : SW_HIDE;
    ShowWindow(g_tabs, show);
    for (int i = 0; i < 4; ++i)
        ShowWindow(g_pages[i], (show && i == TabCtrl_GetCurSel(g_tabs)) ? SW_SHOW : SW_HIDE);
    /* Advanced rows (user dir + extra env) live above the tabs. */
    int au = advanced ? SW_SHOW : SW_HIDE;
    ShowWindow(GetDlgItem(g_main, ID_USER_LBL), au);
    ShowWindow(GetDlgItem(g_main, ID_USER_EDIT), au);
    ShowWindow(GetDlgItem(g_main, ID_USER_BROWSE), au);
    ShowWindow(GetDlgItem(g_main, ID_ENV_LBL), au);
    ShowWindow(GetDlgItem(g_main, ID_ENV_EDIT), au);
    /* Content ends at 318 basic (last checkbox) / 576 advanced (tabs). */
    int by = advanced ? 588 : 330;
    SetWindowPos(GetDlgItem(g_main, ID_RUN), NULL, S(16), S(by), S(150), S(30), SWP_NOZORDER);
    SetWindowPos(GetDlgItem(g_main, ID_ADVANCED), NULL, S(174), S(by), S(150), S(30),
                 SWP_NOZORDER);
    SetWindowPos(g_status, NULL, S(16), S(by + 36), S(308), S(16), SWP_NOZORDER);
    RECT rc = {0, 0, S(340), S(advanced ? 660 : 392)};
    AdjustWindowRect(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    SetWindowPos(g_main, NULL, 0, 0, rc.right - rc.left, rc.bottom - rc.top,
                 SWP_NOMOVE | SWP_NOZORDER);
    if (advanced) place_pages();
}

/* --- main window --- */
static LRESULT CALLBACK wndproc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_BROWSE: on_browse(); break;
        case ID_USER_BROWSE:
            pick_folder_into(g_main, ID_USER_EDIT, L"Pick the saves folder");
            break;
        case ID_MODS_BROWSE:
            pick_folder_into(g_pages[2], ID_MODS_EDIT, L"Pick the mods folder");
            break;
        case ID_PATCH_BROWSE:
            pick_folder_into(g_pages[2], ID_PATCH_EDIT, L"Pick the extra patches folder");
            break;
        case ID_RUN: on_run(); break;
        case ID_ADVANCED:
            g_advanced = !g_advanced;
            SetWindowTextA(GetDlgItem(g_main, ID_ADVANCED), g_advanced ? "Basic" : "Advanced");
            layout(g_advanced);
            break;
        }
        break;
    case WM_NOTIFY: {
        NMHDR* n = (NMHDR*)lp;
        if (n->idFrom == ID_TABS && n->code == TCN_SELCHANGE) {
            int sel = TabCtrl_GetCurSel(g_tabs);
            for (int i = 0; i < 4; ++i) ShowWindow(g_pages[i], i == sel ? SW_SHOW : SW_HIDE);
        }
        break;
    }
    case WM_CLOSE:
        if (w == g_main) DestroyWindow(w);
        break;
    case WM_DESTROY:
        if (w == g_main) PostQuitMessage(0);
        break;
    case WM_DPICHANGED: {
        /* Best effort for monitor moves: new scale + font, same layout pass. */
        g_dpi = HIWORD(wp);
        if (g_dpi < 96) g_dpi = 96;
        LOGFONTA lf;
        GetObjectA(GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), &lf);
        lf.lfHeight = lf.lfHeight * g_dpi / 96;
        lf.lfWidth = 0;
        HFONT fresh = CreateFontIndirectA(&lf);
        if (fresh) {
            DeleteObject(g_font);
            g_font = fresh;
        }
        RECT* suggested = (RECT*)lp;
        SetWindowPos(w, NULL, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        layout(g_advanced);
        break;
    }
    }
    return DefWindowProcA(w, m, wp, lp);
}

static void build_ui(void) {
    int y = S(12);
    HFONT big;
    {
        HWND t = CreateWindowA("STATIC", "Bloodborne bbport", WS_CHILD | WS_VISIBLE, S(16), y,
                               S(308), S(22), g_main, NULL, NULL, NULL);
        big = CreateFontA(S(18), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                          DEFAULT_PITCH, "MS Shell Dlg");
        SendMessageA(t, WM_SETFONT, (WPARAM)big, 1);
    }
    y += S(24);
    w_label(g_main, 0, "native port launcher: settings, then Run.", 16, y, 308);
    y += S(22);
    w_label(g_main, 0, "Game folder:", 16, y + S(4), 90);
    w_edit(g_main, ID_GAME_EDIT, 100, y, 170);
    w_button(g_main, ID_BROWSE, "...", 276, y - S(2), 48, 26);
    y += S(30);
    w_label(g_main, 0, "Upscaler:", 16, y + S(4), 90);
    w_combo(g_main, ID_UPSCALER, 100, y, 224, kUpscalerLabels, 5);
    y += S(30);
    w_label(g_main, 0, "Preset:", 16, y + S(4), 90);
    w_combo(g_main, ID_PRESET, 100, y, 224, kPresets, 5);
    y += S(30);
    w_label(g_main, 0, "Output:", 16, y + S(4), 90);
    w_combo(g_main, ID_OUTPUT, 100, y, 224, kOutputs, 4);
    y += S(30);
    w_label(g_main, 0, "Frame rate:", 16, y + S(4), 90);
    w_combo(g_main, ID_FPS, 100, y, 224, kFpsLabels, 4);
    y += S(32);
    w_check(g_main, ID_FULLSCREEN, "Run in full screen", 16, y, 300);
    y += S(22);
    w_check(g_main, ID_SKIP_INTRO, "Skip the intro logos and movie", 16, y, 300);
    y += S(22);
    w_check(g_main, ID_ASYNC, "Async pipeline compilation (experimental)", 16, y, 300);
    y += S(22);
    w_check(g_main, ID_STATS, "Frame statistics + GPU profile in the log", 16, y, 300);
    y += S(22);
    w_check(g_main, ID_FPS_SHOW, "Show FPS in the game", 16, y, 300);
    y += S(28);
    /* Advanced rows live here (hidden until Advanced). */
    w_label(g_main, ID_USER_LBL, "User dir (saves):", 16, y + S(4), 100);
    w_edit(g_main, ID_USER_EDIT, 118, y, 160);
    w_button(g_main, ID_USER_BROWSE, "...", 282, y - S(2), 42, 26);
    y += S(30);
    w_label(g_main, ID_ENV_LBL, "Extra env:", 16, y + S(4), 100);
    w_edit(g_main, ID_ENV_EDIT, 118, y, 206);
    y += S(30);
    /* Tabs below the rows; buttons + status below the tabs (no overlaps). */
    INITCOMMONCONTROLSEX icc = {.dwSize = sizeof(icc), .dwICC = ICC_TAB_CLASSES};
    InitCommonControlsEx(&icc);
    g_tabs = CreateWindowA(WC_TABCONTROLA, "", WS_CHILD | WS_TABSTOP | TCS_TABS, S(16), S(y),
                           S(308), S(190), g_main, (HMENU)(INT_PTR)ID_TABS, NULL, NULL);
    SendMessageA(g_tabs, WM_SETFONT, (WPARAM)g_font, 1);
    const char* tabs[] = {"Effects", "Gameplay", "Mods", "Perf"};
    for (int i = 0; i < 4; ++i) {
        TCITEMA ti = {.mask = TCIF_TEXT, .pszText = (char*)tabs[i]};
        TabCtrl_InsertItem(g_tabs, i, &ti);
    }
    build_pages();
    w_button(g_main, ID_RUN, "Run", 16, 330, 150, 30);
    w_button(g_main, ID_ADVANCED, "Advanced", 174, 330, 150, 30);
    g_status = CreateWindowA("STATIC", "Ready.", WS_CHILD | WS_VISIBLE, S(16), S(366), S(308),
                             S(16), g_main, (HMENU)(INT_PTR)ID_STATUS, NULL, NULL);
    SendMessageA(g_status, WM_SETFONT, (WPARAM)g_font, 1);
    (void)big;
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show) {
    (void)prev;
    (void)cmd;
    CoInitialize(NULL);

    char exe[MAX_PATH];
    GetModuleFileNameA(NULL, exe, sizeof(exe));
    char* slash = strrchr(exe, '\\');
    char dir[MAX_PATH];
    snprintf(dir, sizeof(dir), "%.*s", (int)(slash - exe), exe);
    char probe[MAX_PATH];
    snprintf(probe, sizeof(probe), "%s\\run.py", dir);
    if (GetFileAttributesA(probe) != INVALID_FILE_ATTRIBUTES) {
        snprintf(g_root, sizeof(g_root), "%s", dir);
    } else {
        snprintf(g_root, sizeof(g_root), "%s\\..", dir);
    }
    char norm[MAX_PATH];
    /* NOTE: source and destination must differ; in-place calls fail. */
    GetFullPathNameA(g_root, sizeof(norm), norm, NULL);
    snprintf(g_root, sizeof(g_root), "%s", norm);
    snprintf(g_ini, sizeof(g_ini), "%s\\bbport.ini", g_root);
    snprintf(g_runpy, sizeof(g_runpy), "%s\\run.py", g_root);
    {
        const char* appdata = getenv("APPDATA");
        snprintf(g_cfg, sizeof(g_cfg), "%s\\bbport-launcher\\settings.json",
                 appdata && *appdata ? appdata : g_root);
        cfg_load();
    }

    WNDCLASSA wc = {.lpfnWndProc = wndproc,
                    .hInstance = inst,
                    .hCursor = LoadCursor(NULL, IDC_ARROW),
                    .hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1),
                    .lpszClassName = "BbLauncher"};
    RegisterClassA(&wc);
    WNDCLASSA pg = {.lpfnWndProc = wndproc,
                    .hInstance = inst,
                    .hCursor = LoadCursor(NULL, IDC_ARROW),
                    .hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1),
                    .lpszClassName = "BbPage"};
    RegisterClassA(&pg);
    /* DPI before any pixels: system DPI first, refined per-window below. */
    HMODULE user32 = GetModuleHandleA("user32.dll");
    UINT(WINAPI* get_system_dpi)(void) =
        (UINT(WINAPI*)(void))GetProcAddress(user32, "GetDpiForSystem");
    g_dpi = get_system_dpi ? (int)get_system_dpi() : 96;
    if (g_dpi < 96) g_dpi = 96;
    {
        LOGFONTA lf;
        GetObjectA(GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), &lf);
        lf.lfHeight = lf.lfHeight * g_dpi / 96;
        lf.lfWidth = 0;
        g_font = CreateFontIndirectA(&lf);
    }
    RECT rc = {0, 0, S(340), S(392)};
    AdjustWindowRect(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    g_main = CreateWindowA("BbLauncher", "Bloodborne", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
                           WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left,
                           rc.bottom - rc.top, NULL, NULL, inst, NULL);
    build_ui();
    load_settings();
    layout(0);
    ShowWindow(g_main, show);
    UpdateWindow(g_main);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        /* Tab navigation across controls. */
        if (!IsDialogMessageA(g_main, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
    CoUninitialize();
    return 0;
}
