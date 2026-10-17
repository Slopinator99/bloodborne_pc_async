/* bb-keys.h: Windows key codes to the key names bbport.ini's key_<action> lines take: SDL scancode
 * names ("E", "Left Shift", "Keypad 1", ...), which src/input_bindings.c turns back into scancodes
 * with SDL_GetScancodeFromName. Header-only and free of the Win32 API but for its constants, so a
 * test can walk every key code and ask SDL whether the name it yields is one it knows. */
#ifndef BB_KEYS_H
#define BB_KEYS_H

#include <windows.h>

/* The SDL name of a key press, or NULL for a key the game has no name for. vk is the virtual-key
 * code of WM_KEYDOWN, scan its scan code (bits 16..23 of lParam) and extended bit 24 of lParam:
 * with them the generic Shift, Ctrl and Alt keys become their left or right form, and Enter on
 * the numpad becomes "Keypad Enter". */
static const char* key_name(unsigned vk, unsigned scan, int extended) {
    static const char* letters[] = {"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
                                    "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z"};
    static const char* digits[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
    static const char* fkeys[] = {"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12"};
    static const char* pad[] = {"Keypad 0", "Keypad 1", "Keypad 2", "Keypad 3", "Keypad 4",
                                "Keypad 5", "Keypad 6", "Keypad 7", "Keypad 8", "Keypad 9"};
    if (vk >= 'A' && vk <= 'Z') return letters[vk - 'A'];
    if (vk >= '0' && vk <= '9') return digits[vk - '0'];
    if (vk >= VK_F1 && vk <= VK_F12) return fkeys[vk - VK_F1];
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) return pad[vk - VK_NUMPAD0];
    switch (vk) {
    case VK_ESCAPE: return "Escape";
    case VK_BACK: return "Backspace";
    case VK_DELETE: return "Delete";
    case VK_INSERT: return "Insert";
    case VK_HOME: return "Home";
    case VK_END: return "End";
    case VK_PRIOR: return "PageUp";
    case VK_NEXT: return "PageDown";
    case VK_TAB: return "Tab";
    case VK_CAPITAL: return "CapsLock";
    case VK_RETURN: return extended ? "Keypad Enter" : "Return";
    case VK_SHIFT: return scan == 0x36 ? "Right Shift" : "Left Shift";
    case VK_LSHIFT: return "Left Shift";
    case VK_RSHIFT: return "Right Shift";
    case VK_CONTROL: return extended ? "Right Ctrl" : "Left Ctrl";
    case VK_LCONTROL: return "Left Ctrl";
    case VK_RCONTROL: return "Right Ctrl";
    case VK_MENU: return extended ? "Right Alt" : "Left Alt";
    case VK_LMENU: return "Left Alt";
    case VK_RMENU: return "Right Alt";
    case VK_LWIN: return "Left GUI";
    case VK_RWIN: return "Right GUI";
    case VK_SPACE: return "Space";
    case VK_UP: return "Up";
    case VK_DOWN: return "Down";
    case VK_LEFT: return "Left";
    case VK_RIGHT: return "Right";
    case VK_DECIMAL: return "Keypad .";
    case VK_DIVIDE: return "Keypad /";
    case VK_MULTIPLY: return "Keypad *";
    case VK_SUBTRACT: return "Keypad -";
    case VK_ADD: return "Keypad +";
    case VK_OEM_3: return "`";
    case VK_OEM_MINUS: return "-";
    case VK_OEM_PLUS: return "=";
    case VK_OEM_4: return "[";
    case VK_OEM_6: return "]";
    case VK_OEM_5: return "\\";
    case VK_OEM_1: return ";";
    case VK_OEM_7: return "'";
    case VK_OEM_COMMA: return ",";
    case VK_OEM_PERIOD: return ".";
    case VK_OEM_2: return "/";
    case VK_SNAPSHOT: return "PrintScreen";
    case VK_SCROLL: return "ScrollLock";
    case VK_PAUSE: return "Pause";
    default: return NULL;
    }
}

#endif
