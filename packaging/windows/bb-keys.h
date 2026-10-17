/* bb-keys.h: Windows key codes to the key names the game's input.ini accepts
 * (src/runtime_input_config.c kKeyNames; launcher/bbport_input_config.py KEY_NAMES). Header-only
 * and free of the Win32 API but for its constants, so a test can walk every key code and check
 * that the names it yields are the ones the game knows. */
#ifndef BB_KEYS_H
#define BB_KEYS_H

#include <windows.h>

/* The name of a key press, or NULL for a key the game has no name for. vk is the virtual-key
 * code of WM_KEYDOWN, scan its scan code (bits 16..23 of lParam) and extended bit 24 of lParam:
 * with them the generic Shift, Ctrl and Alt keys become their left or right form, and Enter
 * on the numpad becomes kpenter. */
static const char* key_name(unsigned vk, unsigned scan, int extended) {
    static const char* letters[] = {"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
                                    "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z"};
    static const char* digits[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
    static const char* fkeys[] = {"f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "f10", "f11", "f12"};
    static const char* pad[] = {"kp0", "kp1", "kp2", "kp3", "kp4", "kp5", "kp6", "kp7", "kp8", "kp9"};
    if (vk >= 'A' && vk <= 'Z') return letters[vk - 'A'];
    if (vk >= '0' && vk <= '9') return digits[vk - '0'];
    if (vk >= VK_F1 && vk <= VK_F12) return fkeys[vk - VK_F1];
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) return pad[vk - VK_NUMPAD0];
    switch (vk) {
    case VK_ESCAPE: return "escape";
    case VK_BACK: return "backspace";
    case VK_DELETE: return "delete";
    case VK_INSERT: return "insert";
    case VK_HOME: return "home";
    case VK_END: return "end";
    case VK_PRIOR: return "pgup";
    case VK_NEXT: return "pgdown";
    case VK_TAB: return "tab";
    case VK_CAPITAL: return "capslock";
    case VK_RETURN: return extended ? "kpenter" : "enter";
    case VK_SHIFT: return scan == 0x36 ? "rshift" : "lshift";
    case VK_LSHIFT: return "lshift";
    case VK_RSHIFT: return "rshift";
    case VK_CONTROL: return extended ? "rctrl" : "lctrl";
    case VK_LCONTROL: return "lctrl";
    case VK_RCONTROL: return "rctrl";
    case VK_MENU: return extended ? "ralt" : "lalt";
    case VK_LMENU: return "lalt";
    case VK_RMENU: return "ralt";
    case VK_LWIN: return "lwin";
    case VK_RWIN: return "rwin";
    case VK_SPACE: return "space";
    case VK_UP: return "up";
    case VK_DOWN: return "down";
    case VK_LEFT: return "left";
    case VK_RIGHT: return "right";
    case VK_DECIMAL: return "kpperiod";
    case VK_DIVIDE: return "kpslash";
    case VK_MULTIPLY: return "kpasterisk";
    case VK_SUBTRACT: return "kpminus";
    case VK_ADD: return "kpplus";
    case VK_OEM_3: return "grave";
    case VK_OEM_MINUS: return "minus";
    case VK_OEM_PLUS: return "equals";
    case VK_OEM_4: return "lbracket";
    case VK_OEM_6: return "rbracket";
    case VK_OEM_5: return "backslash";
    case VK_OEM_1: return "semicolon";
    case VK_OEM_7: return "apostrophe";
    case VK_OEM_COMMA: return "comma";
    case VK_OEM_PERIOD: return "period";
    case VK_OEM_2: return "slash";
    case VK_SNAPSHOT: return "printscreen";
    case VK_SCROLL: return "scrolllock";
    case VK_PAUSE: return "pausebreak";
    default: return NULL;
    }
}

#endif
