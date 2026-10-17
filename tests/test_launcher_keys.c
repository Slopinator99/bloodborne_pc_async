/* Prints "vk extended name" for every key code that bb-keys.h gives a name, for the Python
 * test to check against the names the game accepts. */
#include <stdio.h>
#include "../packaging/windows/bb-keys.h"

int main(void) {
    for (unsigned vk = 0; vk < 256; ++vk) {
        for (int extended = 0; extended < 2; ++extended) {
            const char* name = key_name(vk, 0, extended);
            if (name) printf("%u %d %s\n", vk, extended, name);
        }
    }
    /* The right shift is told apart by its scan code. */
    printf("%u scan36 %s\n", (unsigned)VK_SHIFT, key_name(VK_SHIFT, 0x36, 0));
    printf("%u scan2a %s\n", (unsigned)VK_SHIFT, key_name(VK_SHIFT, 0x2A, 0));
    return 0;
}
