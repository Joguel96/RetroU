/* Enlace diagnóstico del motor y del adaptador de estados; no se distribuye. */
#include "../../src/n64/n64_engine.h"


static int on_vi(void *user, unsigned vi, unsigned ports[4])
{
    (void)user;
    (void)vi;
    (void)ports;
    return 1;
}

int main(void)
{
    N64EngineHost host = { .on_vi = on_vi, .on_rumble = 0, .user = 0 };
    return n64_engine_play("sd:/retrou/roms/n64/probe.z64", "sd:/retrou/n64", &host);
}
