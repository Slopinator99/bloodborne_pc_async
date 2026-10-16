/* bb-settings.h: reader/writer for the Python launcher's settings.json
 * (%APPDATA%\bbport-launcher\settings.json), so bb-launcher and the old launcher share one
 * store. The file is one flat JSON object. Every value is kept as its raw JSON text: keys this
 * launcher does not know (online_*, passwords, ...) survive a save untouched, nested values
 * included. Header-only; included by bb-launcher.c and tests/test_launcher_settings.c. */
#ifndef BB_SETTINGS_H
#define BB_SETTINGS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define CFG_MAX 192

static char g_cfg[MAX_PATH];
static struct {
    char key[64];
    char* raw;
} g_kv[CFG_MAX];
static int g_kvn;
/* The file existed but did not parse to the end: the first save keeps a .bak copy. */
static int g_cfg_bad;

static const char* json_ws(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    return p;
}

/* p is at an opening quote; returns the position after the closing one, or NULL. */
static const char* json_skip_string(const char* p) {
    for (++p; *p && *p != '"'; ++p)
        if (*p == '\\' && p[1]) ++p;
    return *p ? p + 1 : NULL;
}

static const char* json_skip_value(const char* p) {
    if (*p == '"') return json_skip_string(p);
    if (*p == '{' || *p == '[') {
        int depth = 0;
        for (; *p; ++p) {
            if (*p == '"') {
                p = json_skip_string(p);
                if (!p) return NULL;
                --p;
            } else if (*p == '{' || *p == '[') {
                ++depth;
            } else if ((*p == '}' || *p == ']') && --depth == 0) {
                return p + 1;
            }
        }
        return NULL;
    }
    while (*p && !strchr(",}] \t\r\n", *p)) ++p;
    return p;
}

static int json_hex4(const char* p) {
    int v = 0;
    for (int i = 0; i < 4; ++i) {
        char c = p[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= c - '0';
        else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
        else return -1;
    }
    return v;
}

static size_t json_utf8(unsigned cp, char* out) {
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

/* raw is a JSON string literal including its quotes; out gets the UTF-8 text. */
static void json_decode(const char* raw, char* out, size_t n) {
    size_t o = 0;
    const char* p = raw + 1;
    while (*p && *p != '"' && o + 5 < n) {
        if (*p != '\\') {
            out[o++] = *p++;
            continue;
        }
        ++p;
        switch (*p) {
        case 'n': out[o++] = '\n'; break;
        case 't': out[o++] = '\t'; break;
        case 'r': out[o++] = '\r'; break;
        case 'b': out[o++] = '\b'; break;
        case 'f': out[o++] = '\f'; break;
        case 'u': {
            int cp = json_hex4(p + 1);
            if (cp < 0) {
                out[o++] = 'u';
                break;
            }
            p += 4;
            if (cp >= 0xD800 && cp < 0xDC00 && p[1] == '\\' && p[2] == 'u') {
                int lo = json_hex4(p + 3);
                if (lo >= 0xDC00 && lo < 0xE000) {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    p += 6;
                }
            }
            o += json_utf8((unsigned)cp, out + o);
            break;
        }
        case 0: --p; break;
        default: out[o++] = *p; break; /* \\ \" \/ */
        }
        ++p;
    }
    out[o] = 0;
}

/* Returns a malloc'd JSON string literal for the UTF-8 text s. */
static char* json_quote(const char* s) {
    char* out = malloc(strlen(s) * 6 + 3);
    if (!out) return NULL;
    size_t o = 0;
    out[o++] = '"';
    for (; *s; ++s) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') {
            out[o++] = '\\';
            out[o++] = (char)c;
        } else if (c == '\n') {
            out[o++] = '\\';
            out[o++] = 'n';
        } else if (c == '\r') {
            out[o++] = '\\';
            out[o++] = 'r';
        } else if (c == '\t') {
            out[o++] = '\\';
            out[o++] = 't';
        } else if (c < 0x20) {
            o += (size_t)sprintf(out + o, "\\u%04x", c);
        } else {
            out[o++] = (char)c; /* UTF-8 bytes pass through, as Python's ensure_ascii=False */
        }
    }
    out[o++] = '"';
    out[o] = 0;
    return out;
}

static int cfg_find(const char* key) {
    for (int i = 0; i < g_kvn; ++i)
        if (!strcmp(g_kv[i].key, key)) return i;
    return -1;
}

static void cfg_set_raw(const char* key, const char* raw) {
    int i = cfg_find(key);
    if (i < 0) {
        if (g_kvn >= CFG_MAX) return;
        i = g_kvn++;
        snprintf(g_kv[i].key, sizeof(g_kv[i].key), "%s", key);
        g_kv[i].raw = NULL;
    }
    free(g_kv[i].raw);
    g_kv[i].raw = _strdup(raw);
}

static void cfg_set_str(const char* key, const char* value) {
    char* q = json_quote(value);
    if (q) {
        cfg_set_raw(key, q);
        free(q);
    }
}

static void cfg_set_bool(const char* key, int on) {
    cfg_set_raw(key, on ? "true" : "false");
}

/* Strings are decoded; true/false come back as "1"/"0"; numbers as their text. Returns 1 when
 * the key holds a value, else 0 with dflt (when not NULL) copied to out. */
static int cfg_get_str(const char* key, char* out, size_t n, const char* dflt) {
    int i = cfg_find(key);
    if (i >= 0 && strcmp(g_kv[i].raw, "null") != 0) {
        const char* r = g_kv[i].raw;
        if (*r == '"') json_decode(r, out, n);
        else if (!strcmp(r, "true")) snprintf(out, n, "1");
        else if (!strcmp(r, "false")) snprintf(out, n, "0");
        else snprintf(out, n, "%s", r);
        return 1;
    }
    if (dflt) snprintf(out, n, "%s", dflt);
    return 0;
}

static void cfg_clear(void) {
    for (int i = 0; i < g_kvn; ++i) {
        free(g_kv[i].raw);
        g_kv[i].raw = NULL;
    }
    g_kvn = 0;
    g_cfg_bad = 0;
}

/* Reads g_cfg. A missing file is an empty store; an unreadable one keeps what parsed and
 * marks the store bad so the first save backs the original up. */
static void cfg_load(void) {
    cfg_clear();
    FILE* f = fopen(g_cfg, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = size >= 0 ? malloc((size_t)size + 1) : NULL;
    if (!buf) {
        fclose(f);
        g_cfg_bad = 1;
        return;
    }
    size_t got = fread(buf, 1, (size_t)size, f);
    buf[got] = 0;
    fclose(f);

    const char* p = buf;
    if (!memcmp(p, "\xEF\xBB\xBF", 3)) p += 3;
    p = json_ws(p);
    if (*p != '{') {
        g_cfg_bad = got != 0;
        free(buf);
        return;
    }
    ++p;
    int closed = 0;
    for (;;) {
        p = json_ws(p);
        if (*p == '}') {
            closed = 1;
            break;
        }
        if (*p != '"') break;
        const char* kend = json_skip_string(p);
        if (!kend) break;
        char key[64];
        size_t kl = (size_t)(kend - p - 2);
        if (kl >= sizeof(key)) kl = sizeof(key) - 1;
        memcpy(key, p + 1, kl);
        key[kl] = 0;
        p = json_ws(kend);
        if (*p != ':') break;
        p = json_ws(p + 1);
        const char* vend = json_skip_value(p);
        if (!vend || vend == p) break;
        char* raw = malloc((size_t)(vend - p) + 1);
        if (!raw) break;
        memcpy(raw, p, (size_t)(vend - p));
        raw[vend - p] = 0;
        cfg_set_raw(key, raw);
        free(raw);
        p = json_ws(vend);
        if (*p == ',') {
            ++p;
            continue;
        }
        closed = *p == '}';
        break;
    }
    g_cfg_bad = !closed;
    free(buf);
}

/* Writes the store through a temporary file. Returns 1 on success. */
static int cfg_save(void) {
    char dir[MAX_PATH];
    snprintf(dir, sizeof(dir), "%s", g_cfg);
    char* slash = strrchr(dir, '\\');
    if (slash) {
        *slash = 0;
        CreateDirectoryA(dir, NULL);
    }
    if (g_cfg_bad) {
        char bak[MAX_PATH + 8];
        snprintf(bak, sizeof(bak), "%s.bak", g_cfg);
        CopyFileA(g_cfg, bak, FALSE);
        g_cfg_bad = 0;
    }
    char tmp[MAX_PATH + 8];
    snprintf(tmp, sizeof(tmp), "%s.tmp", g_cfg);
    FILE* f = fopen(tmp, "wb");
    if (!f) return 0;
    fputs("{\n", f);
    for (int i = 0; i < g_kvn; ++i)
        fprintf(f, "  \"%s\": %s%s\n", g_kv[i].key, g_kv[i].raw, i + 1 < g_kvn ? "," : "");
    fputs("}\n", f);
    if (fclose(f) != 0) return 0;
    return MoveFileExA(tmp, g_cfg, MOVEFILE_REPLACE_EXISTING) != 0;
}

#endif
