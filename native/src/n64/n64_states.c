/* N64 save states at Not64's instruction boundary, in RetroU's own data
 * directory. The register/memory layout follows the pinned engine's v4
 * states_wiiu.c; no Our64 menu, pause loop or automatic-save scheduler enters
 * this adapter. In-game EEPROM/Flash/SRAM stay outside the state. */
#include "n64_states.h"

#include <gctypes.h>
#include "main/rom.h"
#include "main/plugin.h"
#include "main/savestates.h"
#include "r4300/r4300.h"
#include "r4300/macros.h"
#include "r4300/interupt.h"
#include "r4300/Invalid_Code.h"
#include "gc_memory/memory.h"
#include "gc_memory/tlb.h"
#include "gc_memory/MEM2.h"
#include "player.h"
#include "report.h"
#include "saves.h"
#include "states.h"

#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

extern unsigned long interp_addr;
extern int llbit;
extern long long int reg_cop1_fgr_64[32];
void save_flashram_infos(char *buf);
void load_flashram_infos(char *buf);
void tlb_mem2_init(void);

int savestates_job;

typedef struct { void *at; unsigned size; } StatePart;
static char s_flash_infos[24], s_events[1024];
static uint32_t s_misc[4];
static unsigned char *s_raw, *s_work;
static unsigned s_work_size;
static char s_dir[384];
static int s_ready, s_slot, s_result, s_load_pending;
static uint16_t s_thumbnail[THUMB_W * THUMB_H];

#define N64_STATE_MAX_PARTS 32
#define N64_V3_SAVE_MEMORY (0x800 + FLASHRAM_SIZE + SRAM_SIZE + MEMPACK_SIZE)
#define N64_RAW_MAX (0x800000 + N64_V3_SAVE_MEMORY + 0x10000 + sizeof(s_events))

static unsigned state_parts(StatePart *p)
{
    unsigned n = 0;
#define PART(a, s) (p[n].at = (void *)(a), p[n].size = (s), n++)
    PART(&rdram_register, sizeof(rdram_register));
    PART(&MI_register, sizeof(MI_register));
    PART(&pi_register, sizeof(pi_register));
    PART(&sp_register, sizeof(sp_register));
    PART(&rsp_register, sizeof(rsp_register));
    PART(&si_register, sizeof(si_register));
    PART(&vi_register, sizeof(vi_register));
    PART(&ri_register, sizeof(ri_register));
    PART(&ai_register, sizeof(ai_register));
    PART(&dpc_register, sizeof(dpc_register));
    PART(&dps_register, sizeof(dps_register));
    PART(SP_DMEM, 0x1000);
    PART(SP_IMEM, 0x1000);
    PART(PIF_RAM, 0x40);
    PART(s_flash_infos, sizeof(s_flash_infos));
    PART(tlb_e, sizeof(tlb_e));
    PART(&llbit, sizeof(llbit));
    PART(reg, sizeof(reg));
    PART(reg_cop0, sizeof(reg_cop0));
    PART(reg_cop1_fgr_64, sizeof(reg_cop1_fgr_64));
    PART(&FCR0, sizeof(FCR0));
    PART(&FCR31, sizeof(FCR31));
    PART(s_misc, sizeof(s_misc));
#undef PART
    return n;
}

static unsigned capture(void)
{
    StatePart p[N64_STATE_MAX_PARTS];
    save_flashram_infos(s_flash_infos);
    s_misc[0] = interp_addr;
    s_misc[1] = next_interupt;
    s_misc[2] = next_vi;
    s_misc[3] = vi_field;
    memcpy(s_raw, rdram, 0x800000);
    unsigned at = 0x800000, n = state_parts(p);
    for (unsigned i = 0; i < n; ++i) {
        memcpy(s_raw + at, p[i].at, p[i].size);
        at += p[i].size;
    }
    unsigned events_len = save_eventqueue_infos_max(s_events, sizeof(s_events));
    memcpy(s_raw + at, &events_len, 4);
    memcpy(s_raw + at + 4, s_events, events_len);
    return at + 4 + events_len;
}

static unsigned layout(const unsigned char *buf, unsigned size, unsigned version)
{
    StatePart p[N64_STATE_MAX_PARTS];
    unsigned n = state_parts(p);
    unsigned first = 0x800000 + (version == 3 ? N64_V3_SAVE_MEMORY : 0);
    unsigned at = first;
    for (unsigned i = 0; i < n; ++i) at += p[i].size;
    unsigned events_len;
    if (size < at + 4) return 0;
    memcpy(&events_len, buf + at, 4);
    if (events_len > sizeof(s_events) || size != at + 4 + events_len) return 0;
    return first;
}

static void rebuild_tlb(void)
{
    tlb_mem2_init();
    for (int e = 0; e < 32; ++e) {
        const tlb *t = &tlb_e[e];
        unsigned long start[2] = {t->start_even, t->start_odd};
        unsigned long end[2] = {t->end_even, t->end_odd};
        unsigned long phys[2] = {t->phys_even, t->phys_odd};
        int valid[2] = {t->v_even, t->v_odd};
        int dirty[2] = {t->d_even, t->d_odd};
        for (int h = 0; h < 2; ++h) {
            if (!valid[h] || start[h] >= end[h] ||
                (start[h] >= 0x80000000 && end[h] < 0xC0000000) ||
                phys[h] >= 0x20000000)
                continue;
            for (unsigned long i = start[h]; i < end[h]; ++i) {
                tlb_LUT_r[i >> 12] = 0x80000000 | (phys[h] + i - start[h]);
                if (dirty[h])
                    tlb_LUT_w[i >> 12] = 0x80000000 | (phys[h] + i - start[h]);
            }
        }
    }
}

static int restore(unsigned size, unsigned version)
{
    StatePart p[N64_STATE_MAX_PARTS];
    unsigned at = layout(s_raw, size, version);
    if (!at) return -1;
    memcpy(rdram, s_raw, 0x800000);
    unsigned n = state_parts(p);
    for (unsigned i = 0; i < n; ++i) {
        memcpy(p[i].at, s_raw + at, p[i].size);
        at += p[i].size;
    }
    load_flashram_infos(s_flash_infos);
    set_fpr_pointers(Status);
    static const int rounding[4] = {0x33F, 0xF3F, 0xB3F, 0x73F};
    rounding_mode = rounding[FCR31 & 3];
    rebuild_tlb();
    for (int i = 0; i < 0x100000; ++i) invalid_code_set(i, 1);
    interp_addr = s_misc[0];
    next_interupt = s_misc[1];
    next_vi = s_misc[2];
    vi_field = s_misc[3];
    load_eventqueue_infos((char *)s_raw + at + 4);
    last_addr = interp_addr;
    skip_jump = delay_slot = 0;
    video_vi_status_changed();
    video_vi_width_changed();
    aiDacrateChanged(player_is_pal() ? SYSTEM_PAL : SYSTEM_NTSC);
    return 0;
}

static int slot_path(char *out, size_t size, int slot)
{
    int n = snprintf(out, size, "%s/slot%d.o64s", s_dir, slot + 1);
    return n > 0 && (size_t)n < size ? 0 : -1;
}

int n64_states_init(const char *root)
{
    char id[64], path[384];
    if (!root || !*root) return -1;
    saves_game_id(id, sizeof(id), ROM_HEADER.Name, ROM_HEADER.CRC1);
    int n = snprintf(path, sizeof(path), "%s/states", root);
    if (n <= 0 || (size_t)n >= sizeof(path)) return -1;
    mkdir(path, 0777);
    n = snprintf(s_dir, sizeof(s_dir), "%s/%s", path, id);
    if (n <= 0 || (size_t)n >= sizeof(s_dir)) return -1;
    mkdir(s_dir, 0777);
    s_raw = memalign(64, N64_RAW_MAX);
    s_work_size = statefile_work_size(N64_RAW_MAX);
    s_work = memalign(64, s_work_size);
    if (!s_raw || !s_work) {
        n64_states_finish();
        report_line("N64: sin memoria para estados");
        return -1;
    }
    s_slot = -1;
    s_result = 0;
    s_load_pending = 0;
    savestates_job = 0;
    s_ready = 1;
    return 0;
}

void n64_states_finish(void)
{
    s_ready = 0;
    savestates_job = 0;
    s_load_pending = 0;
    free(s_raw); s_raw = NULL;
    free(s_work); s_work = NULL;
}

int n64_states_request_save(int slot, const uint16_t *thumbnail)
{
    if (!s_ready || slot < 0 || slot >= 4 || savestates_job || s_load_pending) return -1;
    if (thumbnail) memcpy(s_thumbnail, thumbnail, THUMB_BYTES);
    else memset(s_thumbnail, 0, THUMB_BYTES);
    s_slot = slot;
    s_result = 0;
    savestates_job = SAVESTATE;
    return 0;
}

int n64_states_request_load(int slot)
{
    if (!s_ready || slot < 0 || slot >= 4 || savestates_job || s_load_pending) return -1;
    s_slot = slot;
    s_result = 0;
    s_load_pending = 1;
    stop = 1;
    return 0;
}

int n64_states_result(void) { return s_result; }
int n64_states_save_pending(void) { return (savestates_job & SAVESTATE) != 0; }
int n64_states_read_thumb(int slot, uint16_t *thumbnail)
{
    char path[400];
    state_info info = {0};
    if (!s_ready || !thumbnail || slot < 0 || slot >= 4 ||
        slot_path(path, sizeof(path), slot)) return 0;
    memset(thumbnail, 0, THUMB_BYTES);
    return statefile_peek(path, ROM_HEADER.CRC1, ROM_HEADER.CRC2,
                          &info, thumbnail) == STATEFILE_OK && info.has_thumb;
}
int n64_states_load_pending(void) { return s_load_pending; }

void savestates_save(void)
{
    if (!s_ready || !(savestates_job & SAVESTATE) || skip_jump || delay_slot || !gfx_idle()) return;
    char path[400];
    int result = STATEFILE_WRITE_FAILED;
    if (!slot_path(path, sizeof(path), s_slot)) {
        state_info info = {.crc1 = ROM_HEADER.CRC1, .crc2 = ROM_HEADER.CRC2,
                           .slot = (uint8_t)(s_slot + 1)};
        unsigned size = capture();
        result = statefile_write(path, &info, s_raw, size, s_thumbnail, s_work, s_work_size);
    }
    s_result = result == STATEFILE_OK ? 1 : -result - 1;
    report_line("N64: guardar estado slot=%d resultado=%d", s_slot + 1, result);
    savestates_job &= ~SAVESTATE;
}

int n64_states_apply_load(void)
{
    if (!s_ready || !s_load_pending) return -1;
    s_load_pending = 0;
    char path[400];
    int result = STATEFILE_DAMAGED;
    unsigned size = 0;
    state_info info = {0};
    if (!slot_path(path, sizeof(path), s_slot))
        result = statefile_read(path, ROM_HEADER.CRC1, ROM_HEADER.CRC2,
                                &info, s_raw, N64_RAW_MAX, &size);
    if (result == STATEFILE_OK && restore(size, info.version))
        result = STATEFILE_DAMAGED;
    s_result = result == STATEFILE_OK ? 1 : -result - 1;
    report_line("N64: cargar estado slot=%d resultado=%d", s_slot + 1, result);
    return result == STATEFILE_OK ? 0 : -1;
}

/* No LOADSTATE jobs are posted while go() runs. Loading a card must stop the
 * recompiler and reset the graphics pipeline before restore, as in Our64. */
void savestates_load(void)
{
    report_line("N64: LOADSTATE inesperado durante go()");
    savestates_job &= ~LOADSTATE;
}
