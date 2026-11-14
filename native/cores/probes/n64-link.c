/* Enlace diagnóstico: estos estados vacíos NO deben usarse en una WUHB. */
#include "../../src/n64/n64_engine.h"

int savestates_job;
void savestates_save(void) {}
void savestates_load(void) {}

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
