#include "n64_engine.h"
#include "n64_report.h"
#include "n64_states.h"
#include "n64_video.h"

#include <gctypes.h>
#include "bench.h"
#include "our64_jit.h"
#include "player.h"
#include "report.h"
#include "saves.h"
#include "writer.h"
#include "main/rom.h"
#include "main/ROM-Cache.h"
#include "main/wii64config.h"
#include "r4300/r4300.h"
#include "r4300/ppc/Wrappers.h"
#include "gc_memory/memory.h"
#include "gc_memory/MEM2.h"
#include "gc_memory/tlb.h"

#include <coreinit/codegen.h>
#include <coreinit/core.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* Not64's platform storage and generated-code small-data base. */
char our64_mem2[OUR64_MEM2_SIZE] __attribute__((aligned(64)));
char *our64_sda_base;
bench_counters g_bench;

extern long long int reg_cop1_fgr_64[32];
extern unsigned int nextLRU;
extern int noCheckInterrupt;
extern const float two16;
extern double our64_cvt_tmp;
extern const double our64_cvt_magic;
void format_mempacks(void);
void our64_dynarec_unlink(void);

static N64EngineHost s_host;
static uint32_t s_ports[4];
static uint32_t s_last_plugin_port0;
static unsigned s_plugin_input_changes;
static int s_plugin_input_seen;
static volatile int s_running;
static volatile int s_stop_requested;
static OSTime s_next_vi;
static OSTime s_vi_period;

static int setup_sda(void)
{
    struct { const void *p; unsigned n; } values[] = {
        {reg, sizeof(long long) * 34}, {reg_cop0, 4 * 32},
        {reg_cop1_simple, 4 * 32}, {reg_cop1_double, 4 * 32},
        {reg_cop1_fgr_64, 8 * 32}, {&llbit, 4}, {&local_rs32, 4},
        {&FCR31, 4}, {&last_addr, 4}, {&next_interupt, 4},
        {&nextLRU, 4}, {&noCheckInterrupt, 4}, {&two16, 4},
        {&our64_cvt_tmp, 8}, {&our64_cvt_magic, 8},
    };
    uintptr_t first = UINTPTR_MAX, end = 0;
    for (unsigned i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        uintptr_t at = (uintptr_t)values[i].p;
        if (at < first) first = at;
        if (at + values[i].n > end) end = at + values[i].n;
    }
    if (end - first > 0xFFF0) return -1;
    our64_sda_base = (char *)first + 0x8000;
    return 0;
}

static int is_pal(void)
{
    switch (ROM_HEADER.Country_code) {
    case 'D': case 'F': case 'I': case 'P': case 'S': case 'U':
    case 'X': case 'Y': return 1;
    default: return 0;
    }
}

int player_is_pal(void) { return is_pal(); }

void n64_engine_request_stop(void)
{
    s_stop_requested = 1;
    if (s_running) stop = 1;
}

uint32_t input_keys(int port)
{
    if (port == 0 && (!s_plugin_input_seen ||
        (s_ports[0] != s_last_plugin_port0 && s_plugin_input_changes < 32))) {
        report_line("N64 plugin input: port1=%08X vi=%u", s_ports[0], g_bench.vis);
        s_plugin_input_seen = 1;
        if (s_ports[0] != s_last_plugin_port0) ++s_plugin_input_changes;
        s_last_plugin_port0 = s_ports[0];
    }
    return port >= 0 && port < 4 ? s_ports[port] : 0;
}

void our64_rumble(int port, int enabled)
{
    if (s_host.on_rumble) s_host.on_rumble(s_host.user, port, enabled);
}

void new_vi(void)
{
    ++g_bench.vis;
    if (g_ai_stream) ai_stream_tick();
    if (s_host.on_vi && s_host.on_vi(s_host.user, g_bench.vis, s_ports))
        s_stop_requested = 1;
    saves_poll(report_now_us());
    if (s_stop_requested) stop = 1;
    OSTime now = OSGetSystemTime();
    s_next_vi += s_vi_period;
    if (s_next_vi > now)
        OSSleepTicks(s_next_vi - now);
    else if (now - s_next_vi > 4 * s_vi_period)
        s_next_vi = now;
}

void new_frame(void) {}
unsigned player_vis(void) { return g_bench.vis; }
double player_ahead_us(void)
{
    if (!s_running || !s_vi_period) return 0;
    OSTime now = OSGetSystemTime();
    OSTime next = s_next_vi + s_vi_period;
    return next > now ? (double)OSTicksToMicroseconds(next - now) : 0;
}
int player_sync_gfx(void) { return 0; }
int player_sync_before(void) { return 0; }
int player_capturing(void) { return 0; }
void player_capture_start(void) {}
void player_capture_task(uint32_t task) { (void)task; }
void bench_on_gfx_task(void) { ++g_bench.gfx_tasks; }

void our64_bad_mem_call(unsigned addr, unsigned value, int count, int type,
                        unsigned pc, int delay)
{
    report_line("N64 memoria: direccion=%08x valor=%08x repeticiones=%d tipo=%d pc=%08x delay=%d",
                addr, value, count, type, pc, delay);
}

void states_write_done(int what, int ok)
{
    if (!ok) report_line("N64 guardado fallido: tipo=%d", what);
}

void DEBUG_print(char *message, int position)
{
    (void)position;
    if (message) report_line("N64 DEBUG: %s", message);
}
void DEBUG_stats(int id, char *info, unsigned type, unsigned adj)
{
    (void)id; (void)info; (void)type; (void)adj;
}
void InitTimer(void) {}
void start_section(int section) { (void)section; }
void end_section(int section) { (void)section; }
void refresh_stat(void) {}

int n64_engine_play(const char *rom_path, const char *root, const N64EngineHost *host)
{
    uintptr_t caller_sda;
    __asm__ volatile("mr %0,13" : "=r"(caller_sda));
    if (host && host->on_stage) host->on_stage(host->user, 2);
    struct stat st;
    if (!rom_path || !root || !host || !host->on_vi ||
        !rom_path[0])
        return -1;
    if (host->on_stage) host->on_stage(host->user, 3);
    if (stat(rom_path, &st) || st.st_size <= 0x1000 || st.st_size > UINT32_MAX) return -1;
    if (host->on_stage) host->on_stage(host->user, 4);
    if (OSGetCoreId() != OSGetCodegenCore()) return -2;
    if (host->on_stage) host->on_stage(host->user, 5);
    if (n64_report_open(root)) return -3;
    if (host->on_stage) host->on_stage(host->user, 6);
    report_line("N64: inicio ROM=%s bytes=%lu", rom_path, (unsigned long)st.st_size);
    s_host = *host;
    memset(s_ports, 0, sizeof(s_ports));
    s_last_plugin_port0 = 0;
    s_plugin_input_changes = 0;
    s_plugin_input_seen = 0;
    memset(&g_bench, 0, sizeof(g_bench));
    s_running = s_stop_requested = 0;
    int result = -4, graphics_up = 0, audio_up = 0;
    int cpu_up = 0, memory_up = 0, saves_up = 0, writer_up = 0, states_up = 0;
    int rom_opened = 0, video_up = 0;
    unsigned gfx_core = OSGetCodegenCore() == 1 ? 2 : 1;
    report_line("N64: iniciando hilo grafico core=%u", gfx_core);
    if (video_thread_start(gfx_core)) goto done;
    graphics_up = 1;
    if (host->on_stage) host->on_stage(host->user, 7);
    report_line("N64: hilo grafico listo");
    if (our64_jit_init() || setup_sda()) goto done;
    if (host->on_stage) host->on_stage(host->user, 8);
    report_line("N64: JIT y SDA listos");
    __asm__ volatile("mr 13,%0" : : "r"(our64_sda_base));
    if (audio_start()) report_line("N64: audio no disponible");
    else audio_up = 1;
    fileBrowser_file file;
    memset(&file, 0, sizeof(file));
    snprintf(file.name, sizeof(file.name), "%s", rom_path);
    file.size = (unsigned)st.st_size;
    format_mempacks();
    tlb_mem2_init();
    report_line("N64: memoria de cartucho lista");
    rom_opened = 1;
    if (rom_read(&file)) { result = -5; goto done; }
    if (host->on_stage) host->on_stage(host->user, 9);
    report_line("N64: ROM leida");
    if (init_memory()) { result = -6; goto done; }
    memory_up = 1;
    player_plugins_init();
    count_per_op = 2;
    cpu_init();
    cpu_up = 1;
    report_line("N64: CPU lista");
    if (!n64_states_init(root)) states_up = 1;
    else report_line("N64: estados no disponibles en esta sesion");
    if (!writer_start()) writer_up = 1;
    if (writer_up && !saves_start(root)) saves_up = 1;
    report_line("N64: iniciando video");
    if (video_start()) { result = -7; goto done; }
    video_up = 1;
    report_line("N64: video listo");
    report_line("N64: tareas graficas puente=%u video=%u", gfx_tasks_posted(), video_tasks_done());
    if (gfx_tasks_posted() != video_tasks_done()) {
        report_line("N64: inicio cancelado por contadores graficos desincronizados");
        result = -8;
        goto done;
    }
    s_vi_period = OSSecondsToTicks(1) / (is_pal() ? 50 : 60);
    s_next_vi = OSGetSystemTime();
    dynacore = 1;
    stop = 0;
    s_running = 1;
    if (host->on_stage) host->on_stage(host->user, 10);
    report_line("N64: ejecutando region=%c", ROM_HEADER.Country_code);
    for (;;) {
        go();
        if (s_stop_requested || !n64_states_load_pending()) break;
        gfx_reset();
        if (!n64_states_apply_load()) our64_dynarec_unlink();
        s_next_vi = OSGetSystemTime();
        stop = 0;
    }
    s_running = 0;
    result = 0;
    report_line("N64: detenido tras %u VI", g_bench.vis);
done:
    s_running = 0;
    if (graphics_up) {
        n64_video_begin_loading();
        n64_video_menu_hide();
    }
    if (video_up) {
        // Match Our64 teardown: no RSP, audio task or DMA survives the ROM.
        gfx_reset();
        report_line("N64: tareas drenadas puente=%u video=%u", gfx_tasks_posted(), video_tasks_done());
        video_stop();
    }
    if (states_up) n64_states_finish();
    if (saves_up) saves_stop();
    if (writer_up) writer_stop();
    if (cpu_up) cpu_deinit();
    if (memory_up) free_memory();
    if (rom_opened) ROMCache_deinit();
    if (audio_up) audio_stop();
    if (graphics_up) {
        video_thread_stop();
        n64_video_release();
    }
    report_line("N64: fin resultado=%d", result);
    report_save();
    n64_report_close();
    memset(&s_host, 0, sizeof(s_host));
    // r13 is callee-saved. The generated-code SDA must not escape this thread entry.
    __asm__ volatile("mr 13,%0" : : "r"(caller_sda));
    return result;
}
