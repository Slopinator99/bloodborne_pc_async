/* Walks every key code bb-keys.h names and asks SDL whether it knows the name, the way the game's
 * src/input_bindings.c does (SDL_GetScancodeFromName). Prints "vk extended name" for each, and the
 * names SDL rejects after "BAD". Exit code 1 when SDL rejects any. */
#include <SDL3/SDL.h>
#include <stdio.h>
#include "../packaging/windows/bb-keys.h"

int main(void) {
    int bad = 0;
    for (unsigned vk = 0; vk < 256; ++vk) {
        for (int extended = 0; extended < 2; ++extended) {
            const char* name = key_name(vk, 0, extended);
            if (!name) continue;
            printf("%u %d %s\n", vk, extended, name);
            SDL_Scancode sc = SDL_GetScancodeFromName(name);
            if (sc == SDL_SCANCODE_UNKNOWN || SDL_strcasecmp(SDL_GetScancodeName(sc), name) != 0) {
                printf("BAD %s\n", name);
                bad = 1;
            }
        }
    }
    /* The right shift is told apart by its scan code. */
    printf("%u scan36 %s\n", (unsigned)VK_SHIFT, key_name(VK_SHIFT, 0x36, 0));
    printf("%u scan2a %s\n", (unsigned)VK_SHIFT, key_name(VK_SHIFT, 0x2A, 0));
    return bad;
}
