/* MAIN.DLL: the Flights menu's C routines, .text+0xc560..+0xed10 (MSVC C).
 *
 * They sit over the situation reader and writer (slice 2) and are reached
 * from the host's menus, FACIL and the English\ dialog modules. */

/* .text+0xcdb0 == Linkage +0xb0: save the flight under string 0x2201's
 * title, with the description at .data+0x7fd0 (slice 17).  FACIL calls it.
 * The title buffer starts as the 17 bytes at .data+0x3634 ("Temporary
 * Flight" and its terminator) and zeros to 0x104, so a string the host
 * cannot load leaves that title.  The save is .text+0x120aa, Linkage +0x40,
 * called directly as retail calls it; its result is the answer. */
static uint32_t FS98_STDCALL flight_quicksave(void)
{
    uint8_t title[0x104];
    uint32_t i;

    for (i = 0; i < 17; i++)
        title[i] = M8(V(0x20803634) + i);
    for (; i < sizeof title; i++)
        title[i] = 0;
    H_LOADSTRING(0x2201, ADDR(title), 0x104);
    return save_flight(ADDR(title), V(0x20807fd0));
}
