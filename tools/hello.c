/* HELLO.EXE - smallest possible DJGPP program with the same CWSDSTUB binding as
 * MAZE2.EXE. If this does not run on a machine, the loader path is the problem,
 * not the game. Prints the DPMI host details and waits for a key. */
#include <stdio.h>
#include <dpmi.h>
#include <conio.h>
#include <dos.h>
int main(void) {
    __dpmi_version_ret v;
    __dpmi_free_mem_info m;
    printf("HELLO from DJGPP: 32-bit image started OK\n");
    if (__dpmi_get_version(&v) == 0)
        printf("DPMI %d.%02d flags %04x cpu %d\n", v.major, v.minor, v.flags, v.cpu);
    if (__dpmi_get_free_memory_information(&m) == 0)
        printf("largest free block %lu KB, total %lu KB\n", m.largest_available_free_block_in_bytes / 1024, m.total_number_of_physical_pages * 4);
    printf("DOS %d.%d\n", _osmajor, _osminor);
    printf("press a key\n");
    getch();
    return 0;
}
