/* bb-launcher: classic native launcher for bbport (Windows).
 * Plain Win32, no frameworks: one tabbed window with every setting, Run and Save.
 * Pages are described by one field table (g_fields); creating, loading, saving and the
 * environment handed to the game all walk that table.
 *
 * Two stores, both shared with bb-runtime (the Python launcher):
 *   bbport.ini                                  game settings (the in-game menu edits them too)
 *   %APPDATA%\bbport-launcher\settings.json     launcher options (bb-settings.h)
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bb-settings.h"

#define ID_SAVE 113
#define ID_CLOSE 114
#define ID_STATUS 116
#define ID_TABS 117
#define ID_GAME_NOTE 120
#define ID_GAME_INFO 121
#define ID_BANNER 130
#define ID_BANNER_TITLE 131
#define ID_BANNER_SUB 132
#define ID_BANNER_ICON 133
#define ID_PLAY 134
#define ID_OPEN_BASE 4000 /* Play page shortcuts */
#define ID_MOD_UP 5001
#define ID_MOD_DOWN 5002
#define ID_LISTS_REFRESH 5003
#define ID_FIELD_BASE 1000  /* field i: control 1000+i */
#define ID_BROWSE_BASE 2000 /* folder field i: browse button 2000+i */
#define ID_VALUE_BASE 3000  /* slider field i: value text 3000+i */

/* Layout, in unscaled pixels (S() applies the DPI). Compact and classic: a property-sheet style
 * window, 8 pt dialog font, group boxes, no visual styles (see bb-launcher.manifest). */
#define WIN_W 420
#define WIN_H 392
#define TABS_X 6
#define TABS_Y 6
#define TABS_W 408
#define TABS_H 356
#define PAGE_W 396
#define LABEL_X 10
#define LABEL_W 118
#define CTRL_X 132
#define CTRL_W 252
#define ROW_H 25
#define CHECK_H 18

/* --- option lists: stored value + shown text, the same values bb-runtime stores --- */
static const char* kUpscalerV[] = {"dlss", "fsr4", "fsr411", "fsr3", "taa", "off"};
static const char* kUpscalerL[] = {"DLSS (NVIDIA GeForce RTX)", "FSR 4 (best quality)",
                                   "FSR 4.1.1 (needs fsr4_411 assets)", "FSR 3.1 (every GPU)",
                                   "TAA (native resolution anti-aliasing)", "Off"};
static const char* kPresetV[] = {"0", "1", "2", "3", "4"};
static const char* kPresetL[] = {"Native AA (x1.0)", "Quality (x1.5)", "Balanced (x1.7)",
                                 "Performance (x2)", "Ultra Performance (x3)"};
static const char* kOutputV[] = {"1280x720",  "1920x1080", "2560x1440", "3840x2160", "2560x1080",
                                 "3440x1440", "3840x1600", "5120x2160", "3840x1080", "5120x1440"};
static const char* kOutputL[] = {"1280 x 720 (Steam Deck)", "1920 x 1080",        "2560 x 1440",
                                 "3840 x 2160 (4K)",         "2560 x 1080 (21:9)", "3440 x 1440 (21:9)",
                                 "3840 x 1600 (21:9)",       "5120 x 2160 (21:9)", "3840 x 1080 (32:9)",
                                 "5120 x 1440 (32:9)"};
static const char* kHudV[] = {"100", "75", "50", "25", "0"};
static const char* kHudL[] = {"Screen edges", "75%", "Halfway", "25%", "Centred (16:9)"};
static const char* kLiveV[] = {"auto", "0", "1"};
static const char* kLiveL[] = {"Auto (by graphics card)", "Off (faster)", "On (change without restarting)"};
static const char* kLodV[] = {"0", "-2", "1", "2"};
static const char* kLodL[] = {"As in the game", "Highest (-2)", "Lower (1)", "Lowest (2)"};
static const char* kFpsV[] = {"uncap", "60", "90", "30"};
static const char* kFpsL[] = {"Unlocked (frame-time patch)", "60 FPS", "90 FPS", "30 FPS (as on PS4)"};
static const char* kCapV[] = {"", "60", "90", "120", "144", "165", "240", "0"};
static const char* kCapL[] = {"Auto: display refresh, max 120 (recommended)", "60", "90", "120",
                              "144 (risky)", "165 (risky)", "240 (risky)", "No limit (risky)"};
static const char* kAheadV[] = {"", "2", "0"};
static const char* kAheadL[] = {"1 (default)", "2", "Unbounded"};
static const char* kPresentV[] = {"Mailbox", "Fifo", "FifoRelaxed", "Immediate"};
static const char* kPresentL[] = {"Mailbox (low latency, no tearing)", "FIFO (VSync)", "FIFO Relaxed",
                                  "Immediate (tearing)"};
static const char* kLangV[] = {"1", "8", "0", "2", "3", "4", "5", "17"};
static const char* kLangL[] = {"English", "Russian", "Japanese", "French", "Spanish", "German",
                               "Italian", "Portugu\xc3\xaas (Brasil)"};
static const char* kVoiceV[] = {"auto", "eng", "ptb", "ded", "esa", "ese", "frf", "iti", "jaj"};
static const char* kVoiceL[] = {"As in the game", "English", "Portugu\xc3\xaas (Brasil)", "Deutsch",
                                "Espa\xc3\xb1ol (Am\xc3\xa9rica)", "Espa\xc3\xb1ol (Espa\xc3\xb1" "a)",
                                "Fran\xc3\xa7" "ais", "Italiano", "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e"};
static const char* kDrawPipeV[] = {"", "1", "0"};
static const char* kDrawPipeL[] = {"Auto (8+ threads)", "On", "Off (more stable)"};
static const char* kReadbackV[] = {"", "0", "2"};
static const char* kReadbackL[] = {"Relaxed (default)", "Off", "Precise"};

/* --- the field table --- */
enum { K_HEAD, K_CHECK, K_COMBO, K_TEXT, K_FOLDER, K_SLIDER, K_LIST };
enum { ST_INI, ST_APP };
enum { PG_PLAY, PG_GRAPHICS, PG_DISPLAY, PG_GAME, PG_CHEATS, PG_MODS, PG_ADV, PG_COUNT };
static const char* kPageNames[PG_COUNT] = {"Play", "Graphics", "Display", "Game",
                                           "Cheats", "Mods", "Advanced"};

typedef struct {
    int page, kind, store;
    const char* key;
    const char* legacy; /* launcher_* key older versions kept in bbport.ini (ST_APP fallback) */
    const char* label;
    const char* const* values;
    const char* const* labels;
    int n;
    const char* dflt;
    int id;
    HWND hwnd;
} Field;

/* Headings and lists carry no value of their own (n is a list's height in pixels). */
#define HAS_VALUE(f) ((f)->kind != K_HEAD && (f)->kind != K_LIST)
#define FLD_LIST(pg, key, height) {pg, K_LIST, ST_APP, key, "", "", NULL, NULL, height, "", 0, NULL}
#define FLD_HEAD(pg, label) {pg, K_HEAD, 0, "", "", label, NULL, NULL, 0, "", 0, NULL}
#define FLD_CHECK(pg, st, key, legacy, label, dflt) \
    {pg, K_CHECK, st, key, legacy, label, NULL, NULL, 0, dflt, 0, NULL}
#define FLD_COMBO(pg, st, key, legacy, label, V, L, dflt) \
    {pg, K_COMBO, st, key, legacy, label, V, L, (int)(sizeof(V) / sizeof((V)[0])), dflt, 0, NULL}
#define FLD_TEXT(pg, st, key, label, dflt) {pg, K_TEXT, st, key, "", label, NULL, NULL, 0, dflt, 0, NULL}
#define FLD_FOLDER(pg, st, key, legacy, label) \
    {pg, K_FOLDER, st, key, legacy, label, NULL, NULL, 0, "", 0, NULL}
#define FLD_SLIDER(pg, st, key, label, dflt) {pg, K_SLIDER, st, key, "", label, NULL, NULL, 0, dflt, 0, NULL}

static Field g_fields[] = {
    /* Play */
    FLD_HEAD(PG_PLAY, "Game"),
    FLD_FOLDER(PG_PLAY, ST_APP, "game_dir", "launcher_game_dir", "Game folder:"),
    FLD_CHECK(PG_PLAY, ST_INI, "skip_intro", "", "Skip the intro logos and movie", "1"),
    FLD_CHECK(PG_PLAY, ST_INI, "show_fps", "", "Show the FPS counter in the game", "1"),
    /* Graphics */
    FLD_COMBO(PG_GRAPHICS, ST_INI, "upscaler", "", "Upscaler:", kUpscalerV, kUpscalerL, "taa"),
    FLD_COMBO(PG_GRAPHICS, ST_INI, "preset", "", "Quality preset:", kPresetV, kPresetL, "0"),
    FLD_COMBO(PG_GRAPHICS, ST_INI, "output_res", "", "Output resolution:", kOutputV, kOutputL,
              "1920x1080"),
    FLD_COMBO(PG_GRAPHICS, ST_INI, "hud_position", "", "HUD position (21:9):", kHudV, kHudL, "100"),
    FLD_COMBO(PG_GRAPHICS, ST_INI, "live_resolution", "", "Live resolution:", kLiveV, kLiveL, "auto"),
    FLD_CHECK(PG_GRAPHICS, ST_INI, "sharpen", "", "Sharpening (RCAS)", "1"),
    FLD_SLIDER(PG_GRAPHICS, ST_INI, "sharpness", "Sharpness:", "0.50"),
    FLD_CHECK(PG_GRAPHICS, ST_INI, "object_motion", "", "Object motion vectors (about 10% FPS)", "1"),
    FLD_COMBO(PG_GRAPHICS, ST_INI, "model_lod", "", "Model detail (LOD):", kLodV, kLodL, "0"),
    /* Display */
    FLD_COMBO(PG_DISPLAY, ST_APP, "fps_mode", "launcher_fps", "Frame rate:", kFpsV, kFpsL, "60"),
    FLD_COMBO(PG_DISPLAY, ST_APP, "frame_cap", "launcher_cap", "Frame cap (unlocked):", kCapV, kCapL,
              ""),
    FLD_COMBO(PG_DISPLAY, ST_APP, "frames_ahead", "launcher_ahead", "Frames ahead of the GPU:",
              kAheadV, kAheadL, ""),
    FLD_CHECK(PG_DISPLAY, ST_APP, "fullscreen", "launcher_fullscreen", "Run in full screen", "0"),
    FLD_COMBO(PG_DISPLAY, ST_APP, "present_mode", "launcher_present", "Presentation:", kPresentV,
              kPresentL, "Mailbox"),
    FLD_CHECK(PG_DISPLAY, ST_APP, "hdr", "", "Allow HDR output", "0"),
    /* Game */
    FLD_FOLDER(PG_GAME, ST_APP, "user_dir", "launcher_user_dir", "Saves folder:"),
    FLD_FOLDER(PG_GAME, ST_APP, "dlc_dir", "", "DLC folder:"),
    FLD_COMBO(PG_GAME, ST_APP, "language", "", "Text language:", kLangV, kLangL, "1"),
    FLD_COMBO(PG_GAME, ST_APP, "voice_language", "", "Voice language:", kVoiceV, kVoiceL, "auto"),
    FLD_TEXT(PG_GAME, ST_APP, "player_name", "Player name:", ""),
    FLD_HEAD(PG_GAME, "Effects"),
    FLD_CHECK(PG_GAME, ST_INI, "effect_chromatic_aberration", "", "Chromatic aberration", "1"),
    FLD_CHECK(PG_GAME, ST_INI, "effect_dof", "", "Depth of field", "1"),
    FLD_CHECK(PG_GAME, ST_INI, "effect_motion_blur", "", "Motion blur", "1"),
    FLD_CHECK(PG_GAME, ST_INI, "effect_ssao", "", "Ambient occlusion (SSAO)", "1"),
    FLD_CHECK(PG_GAME, ST_INI, "effect_game_aa", "", "The game's own anti-aliasing", "0"),
    FLD_CHECK(PG_GAME, ST_INI, "effect_dynamic_shadows", "", "Shadows of dynamic lights", "1"),
    FLD_CHECK(PG_GAME, ST_INI, "effect_ssr", "", "Screen-space reflections (not in the original)", "0"),
    /* Cheats */
    FLD_HEAD(PG_CHEATS, "Extras"),
    FLD_CHECK(PG_CHEATS, ST_INI, "debug_camera", "", "Free camera (hold Cross + L3; keyboard Space + Z)", "0"),
    FLD_CHECK(PG_CHEATS, ST_INI, "debug_menu", "", "Game debug menu (needs the debug fonts)", "0"),
    FLD_HEAD(PG_CHEATS, "Cheats (game version 1.09)"),
    FLD_CHECK(PG_CHEATS, ST_INI, "cheat_no_death", "", "Never die (health stops at 1 HP)", "0"),
    FLD_CHECK(PG_CHEATS, ST_INI, "cheat_stealth", "", "Enemies do not see you (unless attacked)", "0"),
    FLD_CHECK(PG_CHEATS, ST_INI, "cheat_silent", "", "Enemies do not hear you", "0"),
    FLD_CHECK(PG_CHEATS, ST_INI, "cheat_rally_no_decay", "", "Rally never fades", "0"),
    FLD_CHECK(PG_CHEATS, ST_INI, "cheat_enemy_control", "", "Control the targeted enemy (R3)", "0"),
    FLD_HEAD(PG_CHEATS, "Gameplay tweaks"),
    FLD_CHECK(PG_CHEATS, ST_INI, "tweak_no_rally", "", "No Rally (hits do not give health back)", "0"),
    FLD_CHECK(PG_CHEATS, ST_INI, "tweak_camera_distance", "", "Camera further from the character", "0"),
    FLD_CHECK(PG_CHEATS, ST_INI, "tweak_no_camera_rotation", "", "No camera auto-rotation while moving", "0"),
    FLD_CHECK(PG_CHEATS, ST_INI, "tweak_easy_run", "", "Run with less stick tilt (about 70%)", "0"),
    FLD_CHECK(PG_CHEATS, ST_INI, "tweak_ragdoll", "", "Dark Souls-style ragdoll physics", "0"),
    FLD_CHECK(PG_CHEATS, ST_INI, "tweak_circle_confirm", "", "Japanese buttons: Circle confirms, Cross dodges", "0"),
    FLD_CHECK(PG_CHEATS, ST_INI, "quit_to_desktop", "", "Quit to Desktop in the title and System menus", "1"),
    /* Mods */
    FLD_HEAD(PG_MODS, "Mods"),
    FLD_CHECK(PG_MODS, ST_APP, "mods_enabled", "launcher_mods_on",
              "Load mods (one folder each; lower in the list wins)", "1"),
    FLD_FOLDER(PG_MODS, ST_APP, "mods_dir", "launcher_mods_dir", "Mods folder:"),
    FLD_LIST(PG_MODS, "mods_list", 84),
    FLD_HEAD(PG_MODS, "Third-party patches (game version 1.09)"),
    FLD_FOLDER(PG_MODS, ST_APP, "patches_dir", "launcher_patches_dir", "Patches folder:"),
    FLD_LIST(PG_MODS, "patches_list", 64),
    /* Advanced */
    FLD_COMBO(PG_ADV, ST_APP, "draw_pipe", "launcher_draw_pipe", "GPU draw pipe:", kDrawPipeV, kDrawPipeL, ""),
    FLD_COMBO(PG_ADV, ST_APP, "readbacks", "launcher_readbacks", "GPU readbacks:", kReadbackV, kReadbackL, ""),
    FLD_CHECK(PG_ADV, ST_APP, "async_pipelines", "launcher_async", "Async pipeline compilation (experimental)", "0"),
    FLD_CHECK(PG_ADV, ST_APP, "frame_stats", "launcher_stats", "Frame statistics + GPU profile in the log", "1"),
    FLD_CHECK(PG_ADV, ST_APP, "vk_validation", "", "Vulkan validation layers (slow)", "0"),
    FLD_TEXT(PG_ADV, ST_APP, "extra_env", "Extra environment:", ""),
};
#define NFIELDS ((int)(sizeof(g_fields) / sizeof(g_fields[0])))

/* Older versions of this launcher kept their options as launcher_* keys in bbport.ini. They are
 * read as a fallback (the legacy column above) and dropped from the file on the next save. */
static const char* kLegacyKeys[] = {
    "launcher_game_dir", "launcher_user_dir",  "launcher_fps",       "launcher_fullscreen",
    "launcher_mods_on",  "launcher_mods_dir",  "launcher_patches_dir", "launcher_present",
    "launcher_draw_pipe", "launcher_readbacks", "launcher_ahead",     "launcher_cap",
    "launcher_async",    "launcher_stats"};

static HWND g_main, g_status, g_tabs;
static HWND g_pages[PG_COUNT];
static char g_root[MAX_PATH];
static char g_ini[MAX_PATH];
static char g_runpy[MAX_PATH];
static int g_running;
static HANDLE g_child;
/* DPI scaling: every layout pixel goes through S(). The window declares
 * PerMonitorV2, so on 125/150% displays fixed pixels would overlap. */
static int g_dpi = 96;
static HFONT g_font, g_title_font, g_play_font;
static HBRUSH g_black;
static int S(int v) {
    return (v * g_dpi + 48) / 96;
}

/* --- small widget helpers (parent = any container) --- */
static HWND w_label(HWND p, int id, const char* t, int x, int y, int w) {
    HWND h = CreateWindowA("STATIC", t, WS_CHILD | WS_VISIBLE, S(x), S(y), S(w), S(14), p,
                           (HMENU)(INT_PTR)id, NULL, NULL);
    SendMessageA(h, WM_SETFONT, (WPARAM)g_font, 1);
    return h;
}

static HWND w_edit(HWND p, int id, int x, int y, int w) {
    HWND h = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                             ES_AUTOHSCROLL, S(x), S(y), S(w), S(21), p, (HMENU)(INT_PTR)id,
                             NULL, NULL);
    SendMessageA(h, WM_SETFONT, (WPARAM)g_font, 1);
    return h;
}

static HWND w_combo(HWND p, int id, int x, int y, int w, const char* const* items, int n) {
    HWND h = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST |
                           WS_VSCROLL, S(x), S(y), S(w), S(240), p, (HMENU)(INT_PTR)id, NULL, NULL);
    SendMessageA(h, WM_SETFONT, (WPARAM)g_font, 1);
    for (int i = 0; i < n; ++i) SendMessageA(h, CB_ADDSTRING, 0, (LPARAM)items[i]);
    return h;
}

static HWND w_check(HWND p, int id, const char* t, int x, int y, int w) {
    HWND h = CreateWindowA("BUTTON", t, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                           S(x), S(y), S(w), S(16), p, (HMENU)(INT_PTR)id, NULL, NULL);
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

static int is_managed(const char* line) {
    for (int i = 0; i < NFIELDS; ++i) {
        if (g_fields[i].store != ST_INI || !HAS_VALUE(&g_fields[i])) continue;
        size_t k = strlen(g_fields[i].key);
        if (!strncmp(line, g_fields[i].key, k) && line[k] == '=') return 1;
    }
    for (unsigned i = 0; i < sizeof(kLegacyKeys) / sizeof(kLegacyKeys[0]); ++i) {
        size_t k = strlen(kLegacyKeys[i]);
        if (!strncmp(line, kLegacyKeys[i], k) && line[k] == '=') return 1;
    }
    return 0;
}

/* --- fields: the control of a field, its value as text, load and save --- */
static int option_index(const Field* f, const char* v) {
    for (int i = 0; i < f->n; ++i)
        if (!strcmp(v, f->values[i])) return i;
    for (int i = 0; i < f->n; ++i)
        if (!strcmp(f->dflt, f->values[i])) return i;
    return 0;
}

static Field* field_by_key(const char* key) {
    for (int i = 0; i < NFIELDS; ++i)
        if (g_fields[i].kind != K_HEAD && !strcmp(g_fields[i].key, key)) return &g_fields[i];
    return NULL;
}

static void slider_text(const Field* f, char* out, size_t n) {
    int pos = (int)SendMessageA(f->hwnd, TBM_GETPOS, 0, 0);
    snprintf(out, n, "%d.%02d", pos / 20, (pos % 20) * 5);
}

/* The field's current value as the text stored for it ("1"/"0" for check boxes). */
static void field_value(const Field* f, char* out, size_t n) {
    switch (f->kind) {
    case K_CHECK:
        snprintf(out, n, "%d", SendMessageA(f->hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED);
        break;
    case K_COMBO: {
        int i = (int)SendMessageA(f->hwnd, CB_GETCURSEL, 0, 0);
        snprintf(out, n, "%s", (i >= 0 && i < f->n) ? f->values[i] : f->dflt);
        break;
    }
    case K_SLIDER: slider_text(f, out, n); break;
    default:
        GetWindowTextA(f->hwnd, out, (int)n);
        break;
    }
}

static void game_dir_fallback(char* game, size_t n) {
    DWORD attrs = GetFileAttributesA(game);
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) return;
    /* Remembered folder gone: a CUSA03173 next to the checkout, else the checkout itself. */
    char cand[MAX_PATH], norm[MAX_PATH];
    snprintf(cand, sizeof(cand), "%s\\..\\CUSA03173", g_root);
    GetFullPathNameA(cand, sizeof(norm), norm, NULL);
    attrs = GetFileAttributesA(norm);
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY))
        snprintf(norm, sizeof(norm), "%s", g_root);
    snprintf(game, n, "%s", norm);
}

/* TITLE and APP_VER from a PS4 param.sfo (little-endian key/value table). */
static int read_sfo(const char* path, char* title, size_t tn, char* ver, size_t vn) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    unsigned char buf[8192];
    size_t got = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    if (got < 20 || memcmp(buf, "\0PSF", 4) != 0) return 0;
    unsigned keys = buf[8] | buf[9] << 8 | buf[10] << 16 | (unsigned)buf[11] << 24;
    unsigned data = buf[12] | buf[13] << 8 | buf[14] << 16 | (unsigned)buf[15] << 24;
    unsigned count = buf[16] | buf[17] << 8 | buf[18] << 16 | (unsigned)buf[19] << 24;
    title[0] = ver[0] = 0;
    for (unsigned i = 0; i < count && 20 + (i + 1) * 16 <= got; ++i) {
        const unsigned char* e = buf + 20 + i * 16;
        unsigned ko = e[0] | e[1] << 8, dlen = e[4] | e[5] << 8, doff = e[12] | e[13] << 8;
        if (keys + ko >= got || data + doff + dlen > got) continue;
        const char* key = (const char*)buf + keys + ko;
        const char* val = (const char*)buf + data + doff;
        if (!strcmp(key, "TITLE")) snprintf(title, tn, "%s", val);
        else if (!strcmp(key, "APP_VER")) snprintf(ver, vn, "%s", val);
    }
    return 1;
}

static void refresh_game_note(void) {
    Field* f = field_by_key("game_dir");
    char game[MAX_PATH], path[MAX_PATH + 24], title[128], ver[32], info[128];
    GetWindowTextA(f->hwnd, game, sizeof(game));
    snprintf(path, sizeof(path), "%s\\eboot.bin", game);
    int found = GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
    SetWindowTextA(GetDlgItem(g_pages[PG_PLAY], ID_GAME_NOTE),
                   found ? "eboot.bin found."
                         : "No eboot.bin in this folder: pick the decrypted CUSA03173 dump.");
    info[0] = 0;
    snprintf(path, sizeof(path), "%s\\sce_sys\\param.sfo", game);
    if (found && read_sfo(path, title, sizeof(title), ver, sizeof(ver)) && ver[0]) {
        const char* trim = ver[0] == '0' ? ver + 1 : ver;
        if (!strcmp(ver, "01.09"))
            snprintf(info, sizeof(info), "Version %s: the community patches apply.", trim);
        else
            snprintf(info, sizeof(info), "Version %s: patches need 1.09 (30 FPS).", trim);
    }
    SetWindowTextA(GetDlgItem(g_pages[PG_PLAY], ID_GAME_INFO), info);
}

static void load_fields(void) {
    char v[1024];
    for (int i = 0; i < NFIELDS; ++i) {
        Field* f = &g_fields[i];
        if (!HAS_VALUE(f)) continue;
        if (f->store == ST_INI) ini_get(f->key, v, sizeof(v), f->dflt);
        else app_get(f->key, f->legacy, v, sizeof(v), f->dflt);
        switch (f->kind) {
        case K_CHECK:
            SendMessageA(f->hwnd, BM_SETCHECK, atoi(v) ? BST_CHECKED : BST_UNCHECKED, 0);
            break;
        case K_COMBO:
            SendMessageA(f->hwnd, CB_SETCURSEL, (WPARAM)option_index(f, v), 0);
            break;
        case K_SLIDER: {
            double d = atof(v);
            int pos = (int)(d * 20 + 0.5);
            if (pos < 0) pos = 0;
            if (pos > 40) pos = 40;
            SendMessageA(f->hwnd, TBM_SETPOS, TRUE, pos);
            slider_text(f, v, sizeof(v));
            SetWindowTextA(GetDlgItem(g_pages[f->page], ID_VALUE_BASE + i), v);
            break;
        }
        default:
            if (!strcmp(f->key, "game_dir")) game_dir_fallback(v, sizeof(v));
            SetWindowTextA(f->hwnd, v);
            break;
        }
    }
    refresh_game_note();
}

/* Writes bbport.ini (unknown lines kept) and settings.json (unknown keys kept). */
/* What every field holds right now, to tell whether anything changed since the last load or
 * save: with no Save button on most tabs, closing asks before dropping changes. */
static void lists_state(char* out, size_t n);
static void lists_save(void);

static char g_snapshot[32768];

static void snapshot(char* out, size_t n) {
    out[0] = 0;
    char v[1024];
    for (int i = 0; i < NFIELDS; ++i) {
        if (!HAS_VALUE(&g_fields[i])) continue;
        field_value(&g_fields[i], v, sizeof(v));
        size_t used = strlen(out);
        if (used + strlen(v) + 4 >= n) break;
        sprintf(out + used, "%s\n", v);
    }
    size_t used = strlen(out);
    if (used + 8 < n) lists_state(out + used, n - used);
}

static int save_fields(void) {
    char rest[16384] = "", v[1024];
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
        return 0;
    }
    fputs("# written by bb-launcher (classic)\n", f);
    for (int i = 0; i < NFIELDS; ++i) {
        const Field* fl = &g_fields[i];
        if (!HAS_VALUE(fl) || fl->store != ST_INI) continue;
        field_value(fl, v, sizeof(v));
        fprintf(f, "%s=%s\n", fl->key, v);
    }
    fputs(rest, f);
    fclose(f);

    for (int i = 0; i < NFIELDS; ++i) {
        const Field* fl = &g_fields[i];
        if (!HAS_VALUE(fl) || fl->store != ST_APP) continue;
        field_value(fl, v, sizeof(v));
        if (fl->kind == K_CHECK) cfg_set_bool(fl->key, atoi(v));
        else cfg_set_str(fl->key, v);
    }
    /* One check box covers both logs. */
    field_value(field_by_key("frame_stats"), v, sizeof(v));
    cfg_set_bool("gpu_profile", atoi(v));
    if (!cfg_save()) {
        status("Cannot write settings.json");
        return 0;
    }
    lists_save();
    snapshot(g_snapshot, sizeof(g_snapshot));
    return 1;
}

/* --- the environment handed to the game, from the saved settings --- */
static void set_env(const char* name, const char* v) {
    SetEnvironmentVariableA(name, v);
}

static void apply_env(void) {
    char v[1024], path[MAX_PATH + 16];
    cfg_get_str("game_dir", v, sizeof(v), "");
    set_env("BB_GAME_DIR", v);
    if (cfg_get_str("user_dir", v, sizeof(v), "") && v[0]) set_env("BB_USER_DIR", v);
    if (cfg_get_str("dlc_dir", v, sizeof(v), "") && v[0]) set_env("BB_DLC_DIR", v);

    snprintf(path, sizeof(path), "%s\\mods.json", g_root);
    set_env("BB_MODS_CONFIG", path);
    snprintf(path, sizeof(path), "%s\\patches.json", g_root);
    set_env("BB_PATCHES_CONFIG", path);
    cfg_get_str("mods_enabled", v, sizeof(v), "1");
    set_env("BB_MODS_ENABLED", v);
    if (!(cfg_get_str("mods_dir", v, sizeof(v), "") && v[0]))
        snprintf(v, sizeof(v), "%s\\mods", g_root);
    set_env("BB_MODS_DIR", v);
    if (!(cfg_get_str("patches_dir", v, sizeof(v), "") && v[0]))
        snprintf(v, sizeof(v), "%s\\patches", g_root);
    set_env("BB_PATCHES_DIR", v);

    cfg_get_str("language", v, sizeof(v), "1");
    set_env("BB_LANGUAGE", v);
    cfg_get_str("voice_language", v, sizeof(v), "auto");
    set_env("BB_VOICE_LANGUAGE", v);
    if (cfg_get_str("player_name", v, sizeof(v), "")) {
        char* b = v;
        while (*b == ' ') ++b;
        size_t len = strlen(b);
        while (len && b[len - 1] == ' ') b[--len] = 0;
        if (*b) set_env("BB_USER_NAME", b);
    }

    cfg_get_str("fullscreen", v, sizeof(v), "0");
    set_env("BB_FULLSCREEN", v);
    cfg_get_str("present_mode", v, sizeof(v), "Mailbox");
    set_env("BB_PRESENT_MODE", v);
    if (cfg_get_str("hdr", v, sizeof(v), "0") && !strcmp(v, "1")) set_env("BB_HDR", "1");
    cfg_get_str("fps_mode", v, sizeof(v), "60");
    set_env("BB_FPS", v);
    /* Match run.py: uncap runs free, 90 gets a 90 Hz vblank, else 60. */
    set_env("BB_VBLANK_HZ", !strcmp(v, "uncap") ? "0" : !strcmp(v, "90") ? "90" : "60");
    if (cfg_get_str("frame_cap", v, sizeof(v), "") && v[0]) set_env("BB_FPS_LIMIT", v);
    if (cfg_get_str("draw_pipe", v, sizeof(v), "") && v[0]) set_env("BB_DRAW_PIPE", v);
    if (cfg_get_str("readbacks", v, sizeof(v), "") && v[0]) set_env("BB_READBACKS", v);
    if (cfg_get_str("frames_ahead", v, sizeof(v), "") && v[0]) set_env("BB_FRAMES_AHEAD", v);

    if (cfg_get_str("frame_stats", v, sizeof(v), "0") && !strcmp(v, "1")) {
        set_env("BB_FRAME_STATS", "1");
        set_env("BB_GPU_PROFILE", "1");
    }
    if (cfg_get_str("async_pipelines", v, sizeof(v), "0") && !strcmp(v, "1"))
        set_env("BB_ASYNC_PIPELINES", "1");
    if (cfg_get_str("vk_validation", v, sizeof(v), "0") && !strcmp(v, "1"))
        set_env("BB_VK_VALIDATION", "1");

    /* Unbuffered child output: the log file stays live while playing. */
    set_env("PYTHONUNBUFFERED", "1");
    set_env("PYTHONIOENCODING", "utf-8");

    if (cfg_get_str("extra_env", v, sizeof(v), "")) {
        for (char* tok = strtok(v, " "); tok; tok = strtok(NULL, " ")) {
            char* eq = strchr(tok, '=');
            if (eq) {
                *eq = 0;
                set_env(tok, eq + 1);
            }
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

static void set_run_enabled(BOOL on) {
    EnableWindow(GetDlgItem(g_pages[PG_PLAY], ID_PLAY), on);
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
    set_run_enabled(TRUE);
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
    GetWindowTextA(field_by_key("game_dir")->hwnd, game, sizeof(game));
    char eboot[MAX_PATH + 16];
    snprintf(eboot, sizeof(eboot), "%s\\eboot.bin", game);
    if (GetFileAttributesA(eboot) == INVALID_FILE_ATTRIBUTES) {
        status("No eboot.bin in the game folder.");
        MessageBoxA(g_main, "Pick the decrypted CUSA03173 folder (eboot.bin inside).",
                    "Bloodborne", MB_OK | MB_ICONWARNING);
        return;
    }
    if (!save_fields()) return;
    /* Online play is set up in bb-runtime; this launcher cannot start it yet. */
    char online[16];
    if (cfg_get_str("online", online, sizeof(online), "0") && !strcmp(online, "1") &&
        MessageBoxA(g_main,
                    "Online play is turned on in your settings, but this launcher cannot start it "
                    "yet (use bb-runtime for that).\n\nStart the game offline?",
                    "Bloodborne", MB_YESNO | MB_ICONQUESTION) != IDYES) {
        return;
    }
    apply_env();
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
    set_run_enabled(FALSE);
    status("Game starting silently (launcher-run.log)...");
    CloseHandle(CreateThread(NULL, 0, watch_thread, NULL, 0, NULL));
}

/* Modern folder picker: works on every drive. The chosen path goes into the edit control
 * edit_id of parent. */
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

/* --- Mods tab: the mod and patch lists ---
 * What counts as a mod or a patch comes from scripts/launcher_lists.py (the Python launcher's
 * own rules); the order and the on/off choices are kept in mods.json and patches.json next to
 * the port, the files the Python launcher and run.py already use. */
#define LIST_MAX 256
typedef struct {
    char* v[LIST_MAX];
    int n;
} StrList;

static char* g_patch_keys[LIST_MAX];
static int g_patch_n;
static int g_lists_ok; /* the helper answered: only then are the json files written */

static void strlist_free(StrList* l) {
    for (int i = 0; i < l->n; ++i) free(l->v[i]);
    l->n = 0;
}

static int strlist_index(const StrList* l, const char* s) {
    for (int i = 0; i < l->n; ++i)
        if (!strcmp(l->v[i], s)) return i;
    return -1;
}

static void strlist_add(StrList* l, const char* s) {
    if (l->n < LIST_MAX) l->v[l->n++] = _strdup(s);
}

static char* read_text_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = size >= 0 ? malloc((size_t)size + 1) : NULL;
    if (buf) {
        size_t got = fread(buf, 1, (size_t)size, f);
        buf[got] = 0;
    }
    fclose(f);
    return buf;
}

/* The strings of "key": [ "a", "b" ] in a JSON text. */
static void json_string_array(const char* text, const char* key, StrList* out) {
    out->n = 0;
    char needle[64];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char* p = text ? strstr(text, needle) : NULL;
    if (!p) return;
    p = json_ws(p + strlen(needle));
    if (*p != ':') return;
    p = json_ws(p + 1);
    if (*p != '[') return;
    for (++p;;) {
        p = json_ws(p);
        if (*p != '"') break;
        const char* e = json_skip_string(p);
        if (!e) break;
        size_t len = (size_t)(e - p) + 1;
        char* buf = malloc(len);
        if (!buf) break;
        json_decode(p, buf, len);
        if (out->n < LIST_MAX) out->v[out->n++] = buf;
        else free(buf);
        p = json_ws(e);
        if (*p == ',') ++p;
    }
}

/* Runs cmd with its output captured, up to 20 s. Returns 1 when it exited with 0. */
static int run_capture(char* cmd, char* out, size_t n) {
    SECURITY_ATTRIBUTES sa = {.nLength = sizeof(sa), .bInheritHandle = TRUE};
    HANDLE rd, wr;
    out[0] = 0;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return 0;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOA si = {.cb = sizeof(si), .dwFlags = STARTF_USESTDHANDLES, .hStdOutput = wr,
                       .hStdError = wr, .hStdInput = GetStdHandle(STD_INPUT_HANDLE)};
    PROCESS_INFORMATION pi;
    BOOL ok = CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, g_root, &si, &pi);
    CloseHandle(wr);
    if (!ok) {
        CloseHandle(rd);
        return 0;
    }
    size_t used = 0;
    DWORD start = GetTickCount();
    for (;;) {
        DWORD avail = 0, got = 0;
        if (PeekNamedPipe(rd, NULL, 0, NULL, &avail, NULL) && avail) {
            size_t room = n - 1 - used;
            if (!room) break;
            ReadFile(rd, out + used, avail < room ? avail : (DWORD)room, &got, NULL);
            used += got;
            continue;
        }
        if (WaitForSingleObject(pi.hProcess, 25) == WAIT_OBJECT_0) {
            while (PeekNamedPipe(rd, NULL, 0, NULL, &avail, NULL) && avail && used < n - 1) {
                ReadFile(rd, out + used, avail < n - 1 - used ? avail : (DWORD)(n - 1 - used), &got, NULL);
                used += got;
            }
            break;
        }
        if (GetTickCount() - start > 20000) {
            TerminateProcess(pi.hProcess, 1);
            break;
        }
    }
    out[used] = 0;
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(rd);
    return code == 0;
}

/* The command running scripts/launcher_lists.py: through the frozen runtime when there is one
 * (--script), else through Python in a source tree. */
static void helper_command(const char* args, char* cmd, size_t n) {
    char exedir[MAX_PATH], frozen[MAX_PATH], script[MAX_PATH + 32];
    GetModuleFileNameA(NULL, exedir, sizeof(exedir));
    *strrchr(exedir, '\\') = 0;
    snprintf(script, sizeof(script), "%s\\scripts\\launcher_lists.py", g_root);
    frozen_runtime(exedir, frozen, sizeof(frozen));
    if (GetFileAttributesA(frozen) == INVALID_FILE_ATTRIBUTES) frozen_runtime(g_root, frozen, sizeof(frozen));
    if (GetFileAttributesA(frozen) != INVALID_FILE_ATTRIBUTES) {
        snprintf(cmd, n, "\"%s\" --script \"%s\" %s", frozen, script, args);
    } else {
        char python[512];
        find_python(python, sizeof(python));
        snprintf(cmd, n, "%s \"%s\" %s", python, script, args);
    }
}

static void lv_add(HWND lv, int index, const char* text, int checked, LPARAM param) {
    LVITEMA it = {.mask = LVIF_TEXT | LVIF_PARAM, .iItem = index, .pszText = (char*)text, .lParam = param};
    SendMessageA(lv, LVM_INSERTITEMA, 0, (LPARAM)&it);
    ListView_SetCheckState(lv, index, checked);
}

static int lv_state(HWND lv, int index) {
    return ListView_GetCheckState(lv, index) ? 1 : 0;
}

static void trim_slashes(char* path) {
    size_t len = strlen(path);
    while (len > 3 && (path[len - 1] == '\\' || path[len - 1] == '/')) path[--len] = 0;
}

/* Rebuilds both lists from the folders in the edit boxes, keeping the choices already made on
 * screen and falling back to mods.json / patches.json. */
static void lists_refresh(void) {
    HWND mods_lv = field_by_key("mods_list")->hwnd, patch_lv = field_by_key("patches_list")->hwnd;
    char mods[MAX_PATH], patches[MAX_PATH], args[3 * MAX_PATH], cmd[4 * MAX_PATH], path[MAX_PATH + 16];
    static char out[65536];
    GetWindowTextA(field_by_key("mods_dir")->hwnd, mods, sizeof(mods));
    GetWindowTextA(field_by_key("patches_dir")->hwnd, patches, sizeof(patches));
    if (!mods[0]) snprintf(mods, sizeof(mods), "%s\\mods", g_root);
    if (!patches[0]) snprintf(patches, sizeof(patches), "%s\\patches", g_root);
    trim_slashes(mods);
    trim_slashes(patches);
    snprintf(args, sizeof(args), "\"%s\" \"%s\" 01.09", mods, patches);
    helper_command(args, cmd, sizeof(cmd));
    status("Reading mods and patches...");
    if (!run_capture(cmd, out, sizeof(out))) {
        status("Could not list mods and patches (no runtime or Python found?).");
        g_lists_ok = 0;
        return;
    }

    /* Choices already on screen, by name. */
    StrList old_mods = {0}, old_on = {0}, old_patch = {0}, old_patch_on = {0};
    char text[512];
    for (int i = 0, n = ListView_GetItemCount(mods_lv); i < n; ++i) {
        ListView_GetItemText(mods_lv, i, 0, text, (int)sizeof(text));
        strlist_add(&old_mods, text);
        strlist_add(&old_on, lv_state(mods_lv, i) ? "1" : "0");
    }
    for (int i = 0; i < g_patch_n; ++i) {
        strlist_add(&old_patch, g_patch_keys[i]);
        strlist_add(&old_patch_on, lv_state(patch_lv, i) ? "1" : "0");
    }

    StrList order = {0}, disabled = {0}, enabled = {0}, off = {0}, avail = {0};
    snprintf(path, sizeof(path), "%s\\mods.json", g_root);
    char* profile = read_text_file(path);
    json_string_array(profile, "order", &order);
    json_string_array(profile, "disabled", &disabled);
    free(profile);
    snprintf(path, sizeof(path), "%s\\patches.json", g_root);
    char* chosen = read_text_file(path);
    json_string_array(chosen, "enabled", &enabled);
    json_string_array(chosen, "disabled", &off);
    free(chosen);

    ListView_DeleteAllItems(mods_lv);
    ListView_DeleteAllItems(patch_lv);
    for (int i = 0; i < g_patch_n; ++i) free(g_patch_keys[i]);
    g_patch_n = 0;

    /* The output is one entry per line; split it in place. */
    StrList patch_lines = {0};
    for (char* line = strtok(out, "\n"); line; line = strtok(NULL, "\n")) {
        line[strcspn(line, "\r")] = 0;
        if (!strncmp(line, "mod\t", 4)) strlist_add(&avail, line + 4);
        else if (!strncmp(line, "patch\t", 6)) strlist_add(&patch_lines, line + 6);
    }
    int count = 0;
    for (int i = 0; i < order.n; ++i) {   /* known order first, then the rest */
        if (strlist_index(&avail, order.v[i]) < 0) continue;
        int was = strlist_index(&old_mods, order.v[i]);
        lv_add(mods_lv, count++, order.v[i],
               was >= 0 ? old_on.v[was][0] == '1' : strlist_index(&disabled, order.v[i]) < 0, 0);
    }
    for (int i = 0; i < avail.n; ++i) {
        int listed = 0;
        for (int j = 0; j < order.n; ++j) listed |= !strcmp(order.v[j], avail.v[i]);
        if (listed) continue;
        int was = strlist_index(&old_mods, avail.v[i]);
        lv_add(mods_lv, count++, avail.v[i],
               was >= 0 ? old_on.v[was][0] == '1' : strlist_index(&disabled, avail.v[i]) < 0, 0);
    }
    for (int i = 0; i < patch_lines.n; ++i) {
        char* key = patch_lines.v[i];
        char* author = strchr(key, '\t');
        char* dflt = "0";
        if (author) {
            *author++ = 0;
            char* tab = strchr(author, '\t');
            if (tab) {
                *tab++ = 0;
                dflt = tab;
            }
        }
        int was = strlist_index(&old_patch, key);
        int on = was >= 0 ? old_patch_on.v[was][0] == '1'
                          : (strlist_index(&enabled, key) >= 0 ||
                             (strlist_index(&off, key) < 0 && dflt[0] == '1'));
        char label[768];
        snprintf(label, sizeof(label), "%s%s%s", key, author && author[0] ? "  -  " : "", author ? author : "");
        if (g_patch_n < LIST_MAX) {
            g_patch_keys[g_patch_n] = _strdup(key);
            lv_add(patch_lv, g_patch_n, label, on, g_patch_n);
            ++g_patch_n;
        }
    }
    char msg[160];
    snprintf(msg, sizeof(msg), "%d mod(s) and %d patch(es) found.", count, g_patch_n);
    status(msg);
    g_lists_ok = 1;
    StrList* all[] = {&old_mods, &old_on, &old_patch, &old_patch_on, &order, &disabled, &enabled, &off, &avail, &patch_lines};
    for (unsigned i = 0; i < sizeof(all) / sizeof(all[0]); ++i) strlist_free(all[i]);
}

/* A string of what the lists hold, for the unsaved-changes check. */
static void lists_state(char* out, size_t n) {
    HWND mods_lv = field_by_key("mods_list")->hwnd, patch_lv = field_by_key("patches_list")->hwnd;
    char text[512];
    out[0] = 0;
    for (int i = 0, c = ListView_GetItemCount(mods_lv); i < c; ++i) {
        ListView_GetItemText(mods_lv, i, 0, text, (int)sizeof(text));
        if (strlen(out) + strlen(text) + 4 < n) sprintf(out + strlen(out), "%s:%d;", text, lv_state(mods_lv, i));
    }
    for (int i = 0; i < g_patch_n; ++i)
        if (strlen(out) + strlen(g_patch_keys[i]) + 4 < n)
            sprintf(out + strlen(out), "%s:%d;", g_patch_keys[i], lv_state(patch_lv, i));
}

static void write_json_array(FILE* f, const char* name, const StrList* l, int last) {
    fprintf(f, "  \"%s\": [", name);
    for (int i = 0; i < l->n; ++i) {
        char* q = json_quote(l->v[i]);
        fprintf(f, "%s\n    %s", i ? "," : "", q ? q : "\"\"");
        free(q);
    }
    fprintf(f, "%s]%s\n", l->n ? "\n  " : "", last ? "" : ",");
}

/* mods.json {"order", "disabled"} and patches.json {"enabled", "disabled"}, as the Python
 * launcher writes them; patches that are not listed here keep the choice already stored. */
static void lists_save(void) {
    if (!g_lists_ok) return;
    HWND mods_lv = field_by_key("mods_list")->hwnd, patch_lv = field_by_key("patches_list")->hwnd;
    char path[MAX_PATH + 16], text[512];
    int count = ListView_GetItemCount(mods_lv);
    if (count > 0) {
        StrList order = {0}, disabled = {0};
        for (int i = 0; i < count; ++i) {
            ListView_GetItemText(mods_lv, i, 0, text, (int)sizeof(text));
            strlist_add(&order, text);
            if (!lv_state(mods_lv, i)) strlist_add(&disabled, text);
        }
        snprintf(path, sizeof(path), "%s\\mods.json", g_root);
        FILE* f = fopen(path, "wb");
        if (f) {
            fputs("{\n", f);
            write_json_array(f, "order", &order, 0);
            write_json_array(f, "disabled", &disabled, 1);
            fputs("}\n", f);
            fclose(f);
        }
        strlist_free(&order);
        strlist_free(&disabled);
    }
    if (g_patch_n > 0) {
        StrList enabled = {0}, off = {0}, keep_on = {0}, keep_off = {0}, shown = {0};
        snprintf(path, sizeof(path), "%s\\patches.json", g_root);
        char* chosen = read_text_file(path);
        json_string_array(chosen, "enabled", &keep_on);
        json_string_array(chosen, "disabled", &keep_off);
        free(chosen);
        for (int i = 0; i < g_patch_n; ++i) strlist_add(&shown, g_patch_keys[i]);
        for (int i = 0; i < keep_on.n; ++i)
            if (strlist_index(&shown, keep_on.v[i]) < 0) strlist_add(&enabled, keep_on.v[i]);
        for (int i = 0; i < keep_off.n; ++i)
            if (strlist_index(&shown, keep_off.v[i]) < 0) strlist_add(&off, keep_off.v[i]);
        for (int i = 0; i < g_patch_n; ++i)
            strlist_add(lv_state(patch_lv, i) ? &enabled : &off, g_patch_keys[i]);
        FILE* f = fopen(path, "wb");
        if (f) {
            fputs("{\n", f);
            write_json_array(f, "enabled", &enabled, 0);
            write_json_array(f, "disabled", &off, 1);
            fputs("}\n", f);
            fclose(f);
        }
        StrList* all[] = {&enabled, &off, &keep_on, &keep_off, &shown};
        for (unsigned i = 0; i < sizeof(all) / sizeof(all[0]); ++i) strlist_free(all[i]);
    }
}

static void mod_move(int step) {
    HWND lv = field_by_key("mods_list")->hwnd;
    int i = ListView_GetNextItem(lv, -1, LVNI_SELECTED), j = i + step;
    if (i < 0 || j < 0 || j >= ListView_GetItemCount(lv)) return;
    char a[512], b[512];
    ListView_GetItemText(lv, i, 0, a, (int)sizeof(a));
    ListView_GetItemText(lv, j, 0, b, (int)sizeof(b));
    int ca = lv_state(lv, i), cb = lv_state(lv, j);
    ListView_SetItemText(lv, i, 0, b);
    ListView_SetItemText(lv, j, 0, a);
    ListView_SetCheckState(lv, i, cb);
    ListView_SetCheckState(lv, j, ca);
    ListView_SetItemState(lv, i, 0, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_SetItemState(lv, j, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
}

static void on_browse(int index) {
    const Field* f = &g_fields[index];
    wchar_t title[96];
    swprintf(title, sizeof(title) / sizeof(title[0]), L"Pick a folder");
    pick_folder_into(g_pages[f->page], f->id, title);
    if (!strcmp(f->key, "mods_dir") || !strcmp(f->key, "patches_dir")) lists_refresh();
}

/* The Play page shortcuts: 0 game folder, 1 saves folder, 2 last run log, 3 bbport.ini. */
static void on_open(int which) {
    char path[MAX_PATH + 16], v[MAX_PATH];
    if (which == 0) {
        GetWindowTextA(field_by_key("game_dir")->hwnd, path, sizeof(path));
    } else if (which == 1) {
        GetWindowTextA(field_by_key("user_dir")->hwnd, v, sizeof(v));
        if (v[0]) snprintf(path, sizeof(path), "%s", v);
        else snprintf(path, sizeof(path), "%s\\user", g_root);
    } else if (which == 2) {
        snprintf(path, sizeof(path), "%s\\launcher-run.log", g_root);
    } else {
        snprintf(path, sizeof(path), "%s", g_ini);
    }
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
        char msg[MAX_PATH + 64];
        snprintf(msg, sizeof(msg), "Not there yet: %s", path);
        status(msg);
        return;
    }
    ShellExecuteA(g_main, "open", path, NULL, NULL, SW_SHOWNORMAL);
}

/* --- pages --- */
static HWND make_page(void) {
    /* Same wndproc as main so page controls deliver WM_COMMAND/WM_HSCROLL; only the
     * main window owns Close/quit. */
    return CreateWindowA("BbPage", "", WS_CHILD, 0, 0, 100, 100, g_main, NULL, NULL, NULL);
}

static void place_pages(void) {
    RECT rc;
    GetWindowRect(g_tabs, &rc);
    MapWindowPoints(NULL, g_main, (POINT*)&rc, 2);
    TabCtrl_AdjustRect(g_tabs, FALSE, &rc);
    for (int i = 0; i < PG_COUNT; ++i)
        SetWindowPos(g_pages[i], NULL, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                     SWP_NOZORDER);
}

/* A group box opens at a K_HEAD field and is sized when the next one (or the page) ends. */
static void end_group(HWND group, int group_y, int* y) {
    int h = *y - group_y + 6;
    SetWindowPos(group, NULL, S(6), S(group_y), S(PAGE_W - 12), S(h), SWP_NOZORDER);
    *y = group_y + h;
}

static int build_page(int pg, int y0) {
    HWND page = g_pages[pg];
    int y = y0, ix = 0; /* ix: indent inside a group box */
    HWND group = NULL;
    int group_y = 0;
    for (int i = 0; i < NFIELDS; ++i) {
        Field* f = &g_fields[i];
        if (f->page != pg) continue;
        f->id = ID_FIELD_BASE + i;
        switch (f->kind) {
        case K_HEAD:
            if (group) end_group(group, group_y, &y);
            if (y > y0) y += 4;
            group_y = y;
            group = CreateWindowA("BUTTON", f->label, WS_CHILD | WS_VISIBLE | BS_GROUPBOX, S(6),
                                  S(y), S(PAGE_W - 12), S(40), page, NULL, NULL, NULL);
            SendMessageA(group, WM_SETFONT, (WPARAM)g_font, 1);
            y += 16;
            ix = 8;
            break;
        case K_CHECK:
            f->hwnd = w_check(page, f->id, f->label, LABEL_X + ix, y, PAGE_W - 24 - ix);
            y += CHECK_H;
            break;
        case K_COMBO:
            w_label(page, 0, f->label, LABEL_X, y + 4, LABEL_W);
            f->hwnd = w_combo(page, f->id, CTRL_X, y, CTRL_W, f->labels, f->n);
            y += ROW_H;
            break;
        case K_TEXT:
            w_label(page, 0, f->label, LABEL_X, y + 4, LABEL_W);
            f->hwnd = w_edit(page, f->id, CTRL_X, y, CTRL_W);
            y += ROW_H;
            break;
        case K_FOLDER:
            w_label(page, 0, f->label, LABEL_X, y + 4, LABEL_W);
            f->hwnd = w_edit(page, f->id, CTRL_X, y, CTRL_W - 52);
            w_button(page, ID_BROWSE_BASE + i, "Browse...", CTRL_X + CTRL_W - 48, y - 1, 48, 23);
            y += ROW_H;
            if (!strcmp(f->key, "game_dir")) {
                w_label(page, ID_GAME_NOTE, "", CTRL_X, y - 3, CTRL_W);
                y += 14;
                w_label(page, ID_GAME_INFO, "", CTRL_X, y - 3, CTRL_W);
                y += 16;
            }
            break;
        case K_LIST: {
            int mods = !strcmp(f->key, "mods_list");
            int w = PAGE_W - 24 - ix - (mods ? 70 : 0);
            f->hwnd = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "", WS_CHILD | WS_VISIBLE |
                                      WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS |
                                      LVS_NOCOLUMNHEADER, S(LABEL_X + ix), S(y), S(w), S(f->n),
                                      page, (HMENU)(INT_PTR)f->id, NULL, NULL);
            SendMessageA(f->hwnd, WM_SETFONT, (WPARAM)g_font, 1);
            ListView_SetExtendedListViewStyle(f->hwnd, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
            LVCOLUMNA col = {.mask = LVCF_WIDTH, .cx = S(w - 24)};
            SendMessageA(f->hwnd, LVM_INSERTCOLUMNA, 0, (LPARAM)&col);
            if (mods) {
                int bx = LABEL_X + ix + w + 6;
                w_button(page, ID_MOD_UP, "Move up", bx, y, 64, 23);
                w_button(page, ID_MOD_DOWN, "Move down", bx, y + 28, 64, 23);
                w_button(page, ID_LISTS_REFRESH, "Refresh", bx, y + 56, 64, 23);
            }
            y += f->n + 6;
            break;
        }
        case K_SLIDER:
            w_label(page, 0, f->label, LABEL_X, y + 4, LABEL_W);
            f->hwnd = CreateWindowA(TRACKBAR_CLASSA, "", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                    TBS_HORZ | TBS_NOTICKS, S(CTRL_X), S(y), S(CTRL_W - 40), S(22),
                                    page, (HMENU)(INT_PTR)f->id, NULL, NULL);
            SendMessageA(f->hwnd, TBM_SETRANGE, TRUE, MAKELPARAM(0, 40));
            w_label(page, ID_VALUE_BASE + i, "0.50", CTRL_X + CTRL_W - 32, y + 4, 32);
            y += ROW_H;
            break;
        }
    }
    if (group) end_group(group, group_y, &y);
    return y;
}

/* Play: a dark banner with the game's icon and name across the top, like the title strip of a
 * 90s game launcher. Returns the y below it. */
static int build_play_banner(HWND page) {
    HINSTANCE inst = GetModuleHandleA(NULL);
    HWND bar = CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE, S(6), S(6), S(PAGE_W - 12), S(48),
                             page, (HMENU)(INT_PTR)ID_BANNER, NULL, NULL);
    (void)bar;
    HWND icon = CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE | SS_ICON, S(14), S(14), S(32),
                              S(32), page, (HMENU)(INT_PTR)ID_BANNER_ICON, NULL, NULL);
    HICON ico = (HICON)LoadImageA(inst, MAKEINTRESOURCEA(1), IMAGE_ICON, S(32), S(32), LR_DEFAULTCOLOR);
    if (ico) SendMessageA(icon, STM_SETICON, (WPARAM)ico, 0);
    HWND title = CreateWindowA("STATIC", "BLOODBORNE", WS_CHILD | WS_VISIBLE, S(56), S(9), S(330),
                               S(28), page, (HMENU)(INT_PTR)ID_BANNER_TITLE, NULL, NULL);
    SendMessageA(title, WM_SETFONT, (WPARAM)g_title_font, 1);
    HWND sub = CreateWindowA("STATIC", "Native PC port", WS_CHILD | WS_VISIBLE, S(58), S(35),
                             S(330), S(14), page, (HMENU)(INT_PTR)ID_BANNER_SUB, NULL, NULL);
    SendMessageA(sub, WM_SETFONT, (WPARAM)g_font, 1);
    return 60;
}

/* Play: shortcuts to the things that are looked at most when something goes wrong, and the
 * big Play / Save / Close buttons under them. */
static void build_play_shortcuts(HWND page, int y) {
    static const char* names[4] = {"Open game folder", "Open saves folder", "Open last run log",
                                   "Open bbport.ini"};
    y += 4;
    HWND group = CreateWindowA("BUTTON", "Shortcuts", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, S(6),
                               S(y), S(PAGE_W - 12), S(74), page, NULL, NULL, NULL);
    SendMessageA(group, WM_SETFONT, (WPARAM)g_font, 1);
    for (int i = 0; i < 4; ++i)
        w_button(page, ID_OPEN_BASE + i, names[i], 14 + (i % 2) * 188, y + 16 + (i / 2) * 28, 182, 23);
    int by = y + 74 + 8;
    HWND play = w_button(page, ID_PLAY, "&Play Bloodborne", 6, by, 192, 42);
    HWND save = w_button(page, ID_SAVE, "&Save", 204, by, 90, 42);
    HWND close = w_button(page, ID_CLOSE, "&Close", 300, by, 90, 42);
    SendMessageA(play, WM_SETFONT, (WPARAM)g_play_font, 1);
    SendMessageA(save, WM_SETFONT, (WPARAM)g_play_font, 1);
    SendMessageA(close, WM_SETFONT, (WPARAM)g_play_font, 1);
    SendMessageA(play, BM_SETSTYLE, BS_DEFPUSHBUTTON, TRUE);
}

static void show_page(int sel) {
    for (int i = 0; i < PG_COUNT; ++i) ShowWindow(g_pages[i], i == sel ? SW_SHOW : SW_HIDE);
}

/* Enemy control and the free camera share their buttons: one at a time. */
static void exclusive_cheats(int clicked_id) {
    Field* control = field_by_key("cheat_enemy_control");
    Field* camera = field_by_key("debug_camera");
    if (clicked_id == control->id && SendMessageA(control->hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED)
        SendMessageA(camera->hwnd, BM_SETCHECK, BST_UNCHECKED, 0);
    if (clicked_id == camera->id && SendMessageA(camera->hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED)
        SendMessageA(control->hwnd, BM_SETCHECK, BST_UNCHECKED, 0);
}

/* --- window procedure (main window and pages) --- */
static LRESULT CALLBACK wndproc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == ID_PLAY) on_run();
        else if (id == ID_MOD_UP) mod_move(-1);
        else if (id == ID_MOD_DOWN) mod_move(1);
        else if (id == ID_LISTS_REFRESH) lists_refresh();
        else if (id == ID_CLOSE) SendMessageA(g_main, WM_CLOSE, 0, 0);
        else if (id == ID_SAVE) {
            if (save_fields()) status("Settings saved.");
        } else if (id >= ID_OPEN_BASE && id < ID_OPEN_BASE + 4) {
            on_open(id - ID_OPEN_BASE);
        } else if (id >= ID_BROWSE_BASE && id < ID_BROWSE_BASE + NFIELDS) {
            on_browse(id - ID_BROWSE_BASE);
        } else if (id >= ID_FIELD_BASE && id < ID_FIELD_BASE + NFIELDS) {
            const Field* f = &g_fields[id - ID_FIELD_BASE];
            if (f->kind == K_CHECK && HIWORD(wp) == BN_CLICKED) exclusive_cheats(id);
            if (f->kind == K_FOLDER && !strcmp(f->key, "game_dir") && HIWORD(wp) == EN_CHANGE)
                refresh_game_note();
        }
        break;
    }
    case WM_CTLCOLORSTATIC: {
        int cid = GetDlgCtrlID((HWND)lp);
        if (cid >= ID_BANNER && cid <= ID_BANNER_ICON) {
            SetBkColor((HDC)wp, RGB(0, 0, 0));
            SetTextColor((HDC)wp, cid == ID_BANNER_TITLE ? RGB(196, 24, 24) : RGB(170, 170, 170));
            return (LRESULT)g_black;
        }
        break;
    }
    case WM_HSCROLL: {
        for (int i = 0; i < NFIELDS; ++i) {
            if (g_fields[i].kind == K_SLIDER && g_fields[i].hwnd == (HWND)lp) {
                char v[16];
                slider_text(&g_fields[i], v, sizeof(v));
                SetWindowTextA(GetDlgItem(w, ID_VALUE_BASE + i), v);
            }
        }
        break;
    }
    case WM_NOTIFY: {
        NMHDR* n = (NMHDR*)lp;
        if (n->idFrom == ID_TABS && n->code == TCN_SELCHANGE) show_page(TabCtrl_GetCurSel(g_tabs));
        break;
    }
    case WM_CLOSE:
        if (w == g_main) {
            static char now[32768];
            snapshot(now, sizeof(now));
            if (strcmp(now, g_snapshot) != 0) {
                int answer = MessageBoxA(g_main, "Save your changes before closing?", "Bloodborne",
                                         MB_YESNOCANCEL | MB_ICONQUESTION);
                if (answer == IDCANCEL || (answer == IDYES && !save_fields())) return 0;
            }
            DestroyWindow(w);
        }
        break;
    case WM_DESTROY:
        if (w == g_main) PostQuitMessage(0);
        break;
    case WM_DPICHANGED: {
        /* Best effort for monitor moves: the window follows the suggested rectangle. */
        g_dpi = HIWORD(wp);
        if (g_dpi < 96) g_dpi = 96;
        RECT* suggested = (RECT*)lp;
        SetWindowPos(w, NULL, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        break;
    }
    }
    return DefWindowProcA(w, m, wp, lp);
}

/* The classic dialog font: MS Shell Dlg 8 pt (Microsoft Sans Serif on current Windows). */
static HFONT make_font(void) {
    LOGFONTA lf = {0};
    lf.lfHeight = -MulDiv(8, g_dpi, 72);
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    snprintf(lf.lfFaceName, sizeof(lf.lfFaceName), "MS Shell Dlg");
    return CreateFontIndirectA(&lf);
}

static void build_ui(void) {
    INITCOMMONCONTROLSEX icc = {.dwSize = sizeof(icc), .dwICC = ICC_TAB_CLASSES | ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&icc);
    g_tabs = CreateWindowA(WC_TABCONTROLA, "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | TCS_TABS,
                           S(TABS_X), S(TABS_Y), S(TABS_W), S(TABS_H), g_main,
                           (HMENU)(INT_PTR)ID_TABS, NULL, NULL);
    SendMessageA(g_tabs, WM_SETFONT, (WPARAM)g_font, 1);
    for (int i = 0; i < PG_COUNT; ++i) {
        TCITEMA ti = {.mask = TCIF_TEXT, .pszText = (char*)kPageNames[i]};
        TabCtrl_InsertItem(g_tabs, i, &ti);
        g_pages[i] = make_page();
        if (i == PG_PLAY) {
            int top = build_play_banner(g_pages[i]);
            build_play_shortcuts(g_pages[i], build_page(i, top));
        } else {
            build_page(i, 8);
        }
    }
    place_pages();
    show_page(0);

    /* A sunken status line under the tabs. */
    g_status = CreateWindowA("STATIC", " Ready.", WS_CHILD | WS_VISIBLE | SS_SUNKEN |
                             SS_LEFTNOWORDWRAP, S(TABS_X), S(TABS_Y + TABS_H + 6), S(TABS_W), S(18),
                             g_main, (HMENU)(INT_PTR)ID_STATUS, NULL, NULL);
    SendMessageA(g_status, WM_SETFONT, (WPARAM)g_font, 1);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show) {
    (void)prev;
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
                    .hIcon = LoadIcon(inst, MAKEINTRESOURCE(1)),
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
    g_font = make_font();
    g_black = CreateSolidBrush(RGB(0, 0, 0));
    g_title_font = CreateFontA(-MulDiv(18, g_dpi, 72), 0, 0, 0, FW_BOLD, TRUE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_ROMAN, "Times New Roman");
    g_play_font = CreateFontA(-MulDiv(12, g_dpi, 72), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              DEFAULT_QUALITY, DEFAULT_PITCH, "MS Shell Dlg");
    RECT rc = {0, 0, S(WIN_W), S(WIN_H)};
    AdjustWindowRect(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    g_main = CreateWindowA("BbLauncher", "Bloodborne", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
                           WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left,
                           rc.bottom - rc.top, NULL, NULL, inst, NULL);
    build_ui();
    load_fields();
    lists_refresh();
    snapshot(g_snapshot, sizeof(g_snapshot));
    /* --page=N (0-based) opens on that tab. */
    const char* opt = cmd ? strstr(cmd, "--page=") : NULL;
    if (opt) {
        int sel = atoi(opt + 7);
        if (sel >= 0 && sel < PG_COUNT) {
            TabCtrl_SetCurSel(g_tabs, sel);
            show_page(sel);
        }
    }
    ShowWindow(g_main, show);
    UpdateWindow(g_main);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        /* Tab navigation across controls. */
        int cur = TabCtrl_GetCurSel(g_tabs);
        if (!(cur >= 0 && IsDialogMessageA(g_pages[cur], &msg)) && !IsDialogMessageA(g_main, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
    CoUninitialize();
    return 0;
}
