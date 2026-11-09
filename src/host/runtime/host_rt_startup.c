/* Startup: the static initialisers the C runtime runs before WinMain. */
#include "host_rt.h"

/* 0x413220 (a jump to 0x413230): four words from .rdata (0x416180) into 0x41a508. */
void fs98_host_rt_413220(void)
{
    G32(0x41a508) = G32(0x416180);
    G32(0x41a50c) = G32(0x416184);
    G32(0x41a510) = G32(0x416188);
    G32(0x41a514) = G32(0x41618c);
}
