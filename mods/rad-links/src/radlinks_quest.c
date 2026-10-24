/* libradlinks.so — rad-links for Echo VR Quest (libr15.so of 6528386917200980, vc 4987566).
 *
 * The Quest port of radlinks.c (PC). It shares no code with the PC file; the wizard logic is copied. The menu pages
 * are PC's bytes on the Quest, and the same click metrics reach the services, so the design is PC's:
 *   p40 CREATE INVITE CODE & LINK  -> POST /invites                 -> the code (log)
 *   p41 JOIN                       -> join the invite we created
 *   p34 ENTER (JOIN VIA CODE)      -> GET /invites/{code}           -> join its link
 *     404 / 410 / 403 / no host    -> a message in the field's title bar (INVALID CODE, CODE EXPIRED, ...)
 *     403 "needs a password"       -> title ENTER PASSWORD; the next ENTER is the password: GET ?password=
 *
 * Loaded by the libevrmod.so mod loader (a DT_NEEDED of libr15.so; it dlopens this library from its constructor),
 * or by LD_PRELOAD. No instruction is patched: it
 * waits for libr15.so, checks the prologues of every function it calls, waits for GNU_RELRO, then swaps RELRO words
 * (mprotect RW, compare-and-swap, mprotect R):
 *   tick    CR15Game vtable slot 0x362ee80 (stock UpdateReset 0x17ec928): game actions (the join) run here
 *   target  GOT JUMP_SLOT 0x36dbf40 (CncaGameSpace::SendComponentEvent 0x1807264): the wizard's card / button events
 *   metric  GOT JUMP_SLOT 0x36da2c8 (NR15Logger::MetricLog(char const*, u64) 0x120cc50, whose only caller is
 *           CR15NetJsonLog::WriteLog through the PLT): settings and the typed code
 * Per word (no module is detected by name and nothing waits for a named module):
 *   stock value                                   -> swap
 *   points into an executable mapping (/proc/self/maps) of another loaded module (dladdr; not libr15.so, not this
 *   library, compared by load base)               -> chain onto it (our hook calls what the word held)
 *   anything else                                 -> refuse that hook, log why; rad-links stays inert from there
 * Order: when a module from this library's own directory is mapped before it (an earlier LD_PRELOAD entry, e.g. a
 * another hook tool preloaded first), it waits, bounded (quiet 1 s, max 3 s), for the three words to settle, so that module
 * gets the first swap; alone it does not wait.
 * A tool loaded AFTER rad-links that refuses non-stock words will refuse the words rad-links already holds; load
 * such a tool first.
 *
 * The player id: the JSON user_id is broken on the Quest, so it is built as the metric builder means to:
 * CR15NetGame::XPlatformId() 0x1244628 -> {type, id}, "%s-%llu" with the prefix table STM PSN XBX OVR-ORG OVR BOT DMO.
 * Services: the engine's own answer to CJson::TString(&netgame->config,
 * "apiservice_host", <API literal at base + 0x2baa029>, 0), asked on the game thread before the first request: the
 * loaded config (the /sdcard file, tier 2 or the APK's) wins, else the literal (rewritten per APK by
 * the APK builder). The same rule the libr15.so API-config patch gives the engine's own API connections. The host
 * answers /invites on its services port. "http://HOST[:PORT]/..."; HOST may be a name (getaddrinfo); https and
 * unresolvable hosts fail the request with a log line (no silent fallback). Plain HTTP/1.0 on a worker thread.
 * Log: logcat tag `radlinks`, and /data/data/com.readyatdawn.r15/files/radlinks.log.
 * Every VA was found by disassembly of that one build. */
#define _GNU_SOURCE
#include <android/log.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <link.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define TAG "radlinks"

/* ---- CSymbol64 ---- */
static uint64_t g_csym_seeds[256];
static pthread_once_t g_csym_once = PTHREAD_ONCE_INIT;
static void csym64_init(void)
{
    const uint64_t poly = 0x95AC9329AC4BC9B5ull;
    for (int i = 0; i < 256; i++) {
        uint64_t v = (i & 0x80) ? 0x2B5926535897936Aull : 0;
        if (i & 0x40) v ^= poly;
        for (int s = 0x20; s; s >>= 1) {
            v <<= 1;
            if (i & s) v ^= poly;
        }
        g_csym_seeds[i] = v << 1;
    }
}
static uint64_t csym64(const char *s)
{
    pthread_once(&g_csym_once, csym64_init);
    uint64_t r = ~0ull;
    for (; *s; s++) {
        unsigned c = (unsigned char)*s;
        if (c >= 'A' && c <= 'Z') c += 32;
        r = (r << 8) ^ g_csym_seeds[r >> 56] ^ c;
    }
    return r;
}

/* ---- profile: libr15.so of Quest 6528386917200980 (sha256 8dd9a961…), found by disassembly ---- */
typedef struct { uint32_t va; uint8_t bytes[16]; const char *what; } Prologue;
typedef struct {
    uint32_t va_tick_slot, va_tick_orig;
    uint32_t va_send_target, got_send_target;
    uint32_t va_metric, got_metric;
    /* CR15Game: netgame*, the -lobbyid block (GUID, team, gametype; +0x8568 is the region on the Quest), menu gs */
    uint32_t o_game_netgame, o_game_lobbyid, o_game_team, o_game_gametype, o_game_menu_gs;
    uint32_t o_ng_lobby, o_lobby_state;
    uint32_t va_begin_mp, va_join, va_leave, va_cancel, va_mp_transition, va_xplatform_id;
    /* the gamespace and actor fields, and the page-enable route */
    uint32_t o_gs_actors, o_at_names, o_at_count;
    uint32_t va_find_page_cs, va_enable_page, va_invoke_set, va_invoke_clear, vt_cs_valid;
    uint32_t va_api_url;
    /* write-back of the invite texts: the script calls the stock key handler makes, plus CScriptCS::SetString */
    uint32_t va_find_script, va_set_string, va_find_canvas, va_canvas_set_text;
    /* services host: CJson::TString(path, def, flags) const, and CR15NetGame's local-config CJson */
    uint32_t va_json_tstring, o_ng_config;
    Prologue checks[16];
} Profile;

static const Profile g_profile = {
    0x362ee80, 0x17ec928,
    0x1807264, 0x36dbf40,
    0x120cc50, 0x36da2c8,
    0x8510, 0x8538, 0x8558, 0x8560, 0x7af0,
    0x40, 0x448,
    0x1201be4, 0x12462a8, 0x128c240, 0x1245be8, 0x12005fc, 0x1244628,
    0x370, 0x80, 0xa8,
    0x206f410, 0x1f79314, 0x17ebe48, 0x17ebf34, 0x1d0,
    0x2baa029,
    0x1291c2c, 0x168f34c, 0x1cbb408, 0x139111c,
    0xfa2e7c, 0x63240,
    {
        { 0x1807264, { 0xff,0x03,0x02,0xd1, 0xf6,0x2b,0x00,0xf9, 0xf5,0x53,0x06,0xa9, 0xf3,0x7b,0x07,0xa9 }, "send_target" },
        { 0x17ec928, { 0x08,0x80,0x60,0x39, 0x48,0x00,0x10,0x37, 0xc0,0x03,0x5f,0xd6, 0x08,0x00,0x40,0xf9 }, "update_reset" },
        { 0x120cc50, { 0xf9,0x63,0xbc,0xa9, 0xf7,0x5b,0x01,0xa9, 0xf5,0x53,0x02,0xa9, 0xf3,0x7b,0x03,0xa9 }, "metric_log" },
        { 0x1201be4, { 0xff,0x83,0x01,0xd1, 0xff,0xfb,0x04,0xa9, 0x68,0x00,0xc0,0xd2, 0x09,0x00,0x80,0x92 }, "begin_mp" },
        /* the first 16 bytes equal the SNSUserID overload 0x1245fa0: the address is what tells them apart */
        { 0x12462a8, { 0xff,0xc3,0x02,0xd1, 0xf9,0x63,0x07,0xa9, 0xf7,0x5b,0x08,0xa9, 0xf5,0x53,0x09,0xa9 }, "join" },
        { 0x128c240, { 0xff,0x43,0x01,0xd1, 0xf6,0x13,0x00,0xf9, 0xf5,0x53,0x03,0xa9, 0xf3,0x7b,0x04,0xa9 }, "leave_game" },
        { 0x1245be8, { 0xf6,0x0f,0x1d,0xf8, 0xf5,0x53,0x01,0xa9, 0xf3,0x7b,0x02,0xa9, 0x28,0xcb,0x00,0xf0 }, "cancel" },
        { 0x12005fc, { 0xff,0x03,0x01,0xd1, 0xf3,0x7b,0x03,0xa9, 0x13,0x78,0x7d,0xf9, 0xd3,0x01,0x00,0xb4 }, "mp_transition" },
        { 0x1244628, { 0xfe,0x0f,0x1f,0xf8, 0x00,0x68,0x54,0xf9, 0x80,0x00,0x00,0xb4, 0x63,0x5a,0xf3,0x97 }, "xplatform_id" },
        { 0x1f79314, { 0xff,0xc3,0x02,0xd1, 0xfc,0x6f,0x05,0xa9, 0xfa,0x67,0x06,0xa9, 0xf8,0x5f,0x07,0xa9 }, "enable_page" },
        { 0x206f410, { 0xff,0x83,0x01,0xd1, 0xf8,0x13,0x00,0xf9, 0xf7,0x5b,0x03,0xa9, 0xf5,0x53,0x04,0xa9 }, "find_page_cs" },
        { 0x1291c2c, { 0xff,0x83,0x01,0xd1, 0xf8,0x13,0x00,0xf9, 0xf7,0x5b,0x03,0xa9, 0xf5,0x53,0x04,0xa9 }, "find_script" },
        { 0x168f34c, { 0x08,0x64,0x40,0xf9, 0x29,0x3c,0x40,0x92, 0x29,0xf5,0x7e,0xd3, 0x0a,0x17,0x80,0x52 }, "set_string" },
        /* the same 16 bytes as find_script / find_page_cs (one template): the address tells them apart */
        { 0x1cbb408, { 0xff,0x83,0x01,0xd1, 0xf8,0x13,0x00,0xf9, 0xf7,0x5b,0x03,0xa9, 0xf5,0x53,0x04,0xa9 }, "find_canvas" },
        { 0x139111c, { 0x43,0x02,0x00,0xb4, 0xff,0x83,0x00,0xd1, 0xf3,0x7b,0x01,0xa9, 0xf3,0x03,0x03,0xaa }, "canvas_set_text" },
        /* NRadEngine::CJson::TString(char const*, char const*, unsigned int) const (dynsym) */
        { 0xfa2e7c, { 0xfc,0x63,0xbc,0xa9, 0xf7,0x5b,0x01,0xa9, 0xf5,0x53,0x02,0xa9, 0xf3,0x7b,0x03,0xa9 }, "json_tstring" },
    },
};

/* Wizard page owners and element ids: PC's, the same bytes on the Quest. */
#define P34_JOIN_CODE  0x2d50833b445fe66cull
#define P35_LINK       0xb7eca761e624f927ull
#define P36_EXPERIENCE 0x6a9e38e434a28911ull
#define P37_ARENA_MODE 0x1726509ab6803498ull
#define P38_COMBAT_MODE 0x04a63327518dd0e4ull
#define P39_MAP        0x5d4b7b11b22d1872ull
#define P40_SETTINGS   0xeb6ff9ddc99fe9f5ull
#define P41_SHARE      0xe8b448bf5e62ef7dull
#define UID_CREATE     0xf307e1db6745521cull
#define UID_CARD_L     0x35ec861316acbb37ull
#define UID_CARD_M     0x35ec861316acbb34ull
#define UID_CARD_R     0x35ec861316acbb35ull
#define SYM_NONE       0xffffffffffffffffull
#define P51_STATUS     0xb245345073f0d3b3ull
/* p41's code text (mnu_master CScriptCR, read from PC 6323983201049540; the Quest has the same bytes):
 * script ui_mnu_deeplinking_invite_code on these two actors (the p41 widget, and a page-less twin). Its
 * UpdateCode() draws `text_hidden ? code_text_string : curr_text_string` into the actor's canvasui element codetext,
 * and nothing in the stock graph writes curr_text_string ("<notset>"). UpdateCode refuses outside calls (concurrent
 * calls 0: "Tried to call function UpdateCode externally", device-observed), so: set curr_text_string (the eye
 * toggle redraws from it), and write the element directly with CCanvasUICS::SetText. */
#define ACTOR_INVITE_CODE  0xa9bd0cb4e6f2004cull
#define ACTOR_INVITE_CODE2 0xe28a9b96434eeff4ull
#define SYM_NCA_SCRIPT     0x6aea99eeb2bab6c4ull
#define SYM_CURR_TEXT      0x7de33d6d994ad2d7ull   /* curr_text_string, char[64] */
#define SYM_CANVASUI       0x35ec9a0128ec916full   /* the actor's canvasui component */
#define SYM_CODETEXT       0x35e290123dfa9c72ull   /* its element codetext (text_name, from the parent's Enable) */
/* two texts no stock script fills (read from PC 6323983201049540): p41 EXPIRES
 * (ui_mnu_deeplinking_invite_expiration, element expirationtime, shipped "EXPIRES: ??:??:??:??") and the p35 link
 * (ui_mnu_deeplinking_view_link, element link_text, a 52-character shipped URL) */
#define ACTOR_EXPIRES      0x0ec8e16986ef88c8ull
#define SYM_EXPIRESTEXT    0xd38c12d22d3552ebull
#define ACTOR_LINK_VIEW    0xc3f9f167b2a06043ull
#define SYM_LINKTEXT       0x3657cd7405999810ull
/* p34 messages (as radlinks.c). The ENTER CODE field is actor 0xad97… (CCanvasUICR mnu_master
 * record 71), canvas ui_mnu_deeplinking_text_input 0x487df6d160c8da42: title_text (the bar above the field, shipped
 * "TITLE TEXT"; the join page's script sets it, its literal is "ENTER CODE") and inputfield (the R15TextEntryBehavior's
 * own text, the player's typing: left alone). read from PC 6323983201049540, the same bytes here. */
#define ACTOR_CODE_FIELD   0xad97e58a7b13425full
#define SYM_TITLETEXT      0xa8452de53657bd6eull
#define P34_TITLE          "ENTER CODE"
#define PASSWORD_WINDOW_MS 120000   /* how long a password prompt waits for its ENTER */

static uint64_t SYM_CARD, SYM_BTN, SYM_TEXT, SYM_LINK_BTN, SYM_BACK, SYM_CANCEL;

typedef void (*TickFn)(void *game);
typedef void (*SendCoreFn)(void *gs, const uint64_t *event, uint32_t flags);
typedef void (*MetricFn)(const char *json, uint64_t len);
typedef void (*JoinFn)(void *lobby, uint64_t gametype, const void *guid, uint16_t team);
typedef void (*LeaveFn)(void *netgame, uint32_t reason);
typedef void (*CancelFn)(void *lobby, uint32_t status);
typedef void (*GameFn)(void *game);
typedef struct { uint64_t type, id; } SNSUserID;   /* returned in x0:x1 */
typedef SNSUserID (*XPlatFn)(const void *netgame);
typedef struct { void *cs; uint32_t idx; uint32_t pad; } PageHandle;
typedef void (*FindPageFn)(PageHandle *out, void *gs, uint64_t actor, uint64_t component);
typedef uint8_t (*ValidFn)(void *cs, uint32_t idx);
typedef void (*EnablePageFn)(void *cs, uint32_t idx);
typedef void (*FindScriptFn)(PageHandle *out, void *gs, uint64_t actor, uint64_t name);   /* {CScriptCS*, index} */
typedef void (*SetStringFn)(void *cs, uint32_t idx, uint64_t sym, const char *s);   /* copies (63 chars max) */
typedef uint8_t (*CanvasSetTextFn)(void *cs, uint32_t idx, uint64_t element, const char *s);   /* 1 = written */
typedef const char *(*JsonTStringFn)(const void *json, const char *path, const char *def, uint32_t flags);

static const Profile *g_prof = &g_profile;
static uint8_t *g_base;
static TickFn g_tick_orig;
static SendCoreFn g_target_orig;
static MetricFn g_metric_orig;
static char g_hooked[32] = "-";

#define ULL(x) ((unsigned long long)(x))
#define U(p) ((uintptr_t)(p))

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}
static void sleep_ms(unsigned ms)
{
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    while (nanosleep(&ts, &ts) && errno == EINTR) {}
}

/* ---- log: logcat + append file ---- */
static pthread_mutex_t g_log_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_log_fd = -1, g_log_tries;

static void rlog(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void rlog(const char *fmt, ...)
{
    char buf[1536];
    struct timespec ts;
    struct tm tm;
    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tm);
    int n = snprintf(buf, sizeof buf, "%02d:%02d:%02d.%03ld ", tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec / 1000000);
    va_list ap;
    va_start(ap, fmt);
    int m = vsnprintf(buf + n, sizeof buf - n - 1, fmt, ap);
    va_end(ap);
    n += m < 0 ? 0 : m >= (int)(sizeof buf - n - 1) ? (int)(sizeof buf - n - 2) : m;
    __android_log_print(ANDROID_LOG_INFO, TAG, "%s", buf + 13);
    buf[n++] = '\n';
    pthread_mutex_lock(&g_log_lock);
    if (g_log_fd < 0 && g_log_tries < 50) {
        g_log_tries++;
        g_log_fd = open("/data/data/com.readyatdawn.r15/files/radlinks.log", O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    }
    if (g_log_fd >= 0 && write(g_log_fd, buf, n) < 0) {}
    pthread_mutex_unlock(&g_log_lock);
}

/* ---- fault-guarded reads (write() from the address into a private pipe) ---- */
static pthread_mutex_t g_pipe_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_probe_pipe[2] = { -1, -1 };

static int safe_copy(void *dst, uintptr_t src, size_t n)
{
    if (!n) return 0;
    if (!src) return -1;
    int rc = 0;
    pthread_mutex_lock(&g_pipe_lock);
    if (g_probe_pipe[0] < 0 && pipe2(g_probe_pipe, O_CLOEXEC | O_NONBLOCK)) { g_probe_pipe[0] = -1; rc = -1; }
    for (size_t off = 0; !rc && off < n;) {
        size_t k = n - off > 4096 ? 4096 : n - off;
        ssize_t w = write(g_probe_pipe[1], (const void *)(src + off), k);
        if (w <= 0) { rc = -1; break; }
        if (read(g_probe_pipe[0], (uint8_t *)dst + off, (size_t)w) != w) { rc = -1; break; }
        off += (size_t)w;
    }
    if (rc && g_probe_pipe[0] >= 0) {
        uint8_t junk[512];
        while (read(g_probe_pipe[0], junk, sizeof junk) > 0) {}
    }
    pthread_mutex_unlock(&g_pipe_lock);
    return rc;
}
static __thread int t_fault;
static uint64_t R64(uintptr_t a) { uint64_t v = 0; if (safe_copy(&v, a, 8)) t_fault = 1; return v; }
static uint32_t R32(uintptr_t a) { uint32_t v = 0; if (safe_copy(&v, a, 4)) t_fault = 1; return v; }

/* ---- wizard state: written by the hooks (game thread), read by the worker under the lock ---- */
enum { EXP_NONE, EXP_LOBBY, EXP_ARENA, EXP_COMBAT };
enum { MODE_NONE, MODE_CUSTOM, MODE_AI, MODE_PAYLOAD, MODE_CAPTURE, MODE_RANDOM };

typedef struct {
    int experience, mode, map;
    char code[16];
    char user[64];               /* "OVR-ORG-<n>" from XPlatformId, refreshed on the tick */
    char gametype[64];
    char link[48];
} Wizard;

static Wizard g_wiz;
static pthread_mutex_t g_wlock = PTHREAD_MUTEX_INITIALIZER;

typedef struct { int experience, mode, map; const char *gametype, *level; } Route;
static const Route g_routes[] = {   /* as radlinks.c */
    { EXP_LOBBY,  MODE_NONE,    0, "social_2.0_private",  "mpl_lobby_b2" },
    { EXP_ARENA,  MODE_CUSTOM,  0, "echo_arena_private",  "mpl_arena_a" },
    { EXP_ARENA,  MODE_AI,      0, "echo_arena_private_ai", "mpl_arena_a" },
    { EXP_COMBAT, MODE_PAYLOAD, 1, "echo_combat_private", "mpl_combat_fission" },
    { EXP_COMBAT, MODE_PAYLOAD, 2, "echo_combat_private", "mpl_combat_gauss" },
    { EXP_COMBAT, MODE_CAPTURE, 1, "echo_combat_private", "mpl_combat_dyson" },
    { EXP_COMBAT, MODE_CAPTURE, 2, "echo_combat_private", "mpl_combat_combustion" },
    { EXP_COMBAT, MODE_RANDOM,  0, "echo_combat_private", NULL },
};

static const Route *route_of(const Wizard *w)
{
    for (size_t i = 0; i < sizeof g_routes / sizeof g_routes[0]; i++) {
        const Route *r = &g_routes[i];
        if (r->experience == w->experience && r->mode == w->mode && r->map == w->map) return r;
    }
    return NULL;
}

static const char *exp_name(int e) { static const char *n[] = { "-", "LOBBY", "ARENA", "COMBAT" }; return n[e & 3]; }
static const char *mode_name(int m)
{
    static const char *n[] = { "-", "CUSTOM MATCH", "AI TEAMMATES", "PAYLOAD", "CAPTURE POINT", "RANDOM" };
    return m >= 0 && m <= MODE_RANDOM ? n[m] : "?";
}
static int card_index(uint64_t uid) { return uid == UID_CARD_L ? 1 : uid == UID_CARD_M ? 2 : uid == UID_CARD_R ? 3 : 0; }

/* ---- worker jobs (hooks -> worker) and game actions (worker / hooks -> tick) ---- */
enum { JOB_CREATE = 1, JOB_RESOLVE };
typedef struct { int kind; char code[16]; Wizard wiz; char password[64], region[32]; int expires_s; } Job;
enum { ACT_JOIN = 1, ACT_SHOW_CODE, ACT_P34_TEXT };
typedef struct { int kind; char link[48], why[64], gametype[64]; int tries; uint64_t begin_at; } Action;   /* SHOW_CODE: link = the code; P34_TEXT: why = the text */

/* The password prompt: the code whose resolve answered 403 "needs a password" (worker writes, hooks read; g_wlock). */
static struct { char code[16]; uint64_t until; } g_pw;

#define JCAP 8
static Job g_jobs[JCAP];
static int g_njobs;
static pthread_mutex_t g_jlock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_jcond = PTHREAD_COND_INITIALIZER;

#define ACAP 8
static Action g_acts[ACAP];
static volatile int g_nacts;
static pthread_mutex_t g_alock = PTHREAD_MUTEX_INITIALIZER;

static void push_job(const Job *j)
{
    pthread_mutex_lock(&g_jlock);
    if (g_njobs < JCAP) g_jobs[g_njobs++] = *j;
    else rlog("job queue full: dropped job %d", j->kind);
    pthread_cond_signal(&g_jcond);
    pthread_mutex_unlock(&g_jlock);
}

static void push_action(const Action *a)
{
    pthread_mutex_lock(&g_alock);
    if (g_nacts < ACAP) g_acts[g_nacts++] = *a;
    else rlog("action queue full: dropped action %d", a->kind);
    pthread_mutex_unlock(&g_alock);
}

/* ---- the player id (workaround), game thread ---- */
static void refresh_user(uint8_t *game)
{
    static const char *prefix[] = { "STM", "PSN", "XBX", "OVR-ORG", "OVR", "BOT", "DMO" };
    t_fault = 0;
    uintptr_t ng = R64(U(game) + g_prof->o_game_netgame);
    if (t_fault || !ng) return;
    SNSUserID u = ((XPlatFn)(g_base + g_prof->va_xplatform_id))((void *)ng);
    unsigned t = (unsigned)(u.type & 0xf);
    if (!u.id || t < 1 || t > 7) return;
    char id[64];
    snprintf(id, sizeof id, "%s-%llu", prefix[t - 1], ULL(u.id));
    pthread_mutex_lock(&g_wlock);
    int changed = strcmp(id, g_wiz.user) != 0;
    if (changed) snprintf(g_wiz.user, sizeof g_wiz.user, "%s", id);
    pthread_mutex_unlock(&g_wlock);
    if (changed) rlog("player id: %s-<n> (type %u)", prefix[t - 1], t);   /* no ids in the log */
}

/* ---- the join ---- */

/* index of actor `name` in gamespace gs ([gs+0x370] actor data: names +0x80, count +0xa8), or -1 */
static int64_t find_actor(uintptr_t gs, uint64_t name)
{
    t_fault = 0;
    uintptr_t ad = R64(gs + g_prof->o_gs_actors);
    uint64_t n = ad ? R64(ad + g_prof->o_at_count) : 0;
    uintptr_t names = ad ? R64(ad + g_prof->o_at_names) : 0;
    if (t_fault || !names || n > 0xfffe) return -1;
    static uint64_t tab[0xffff];   /* game thread only */
    if (safe_copy(tab, names, n * 8)) return -1;
    for (uint64_t i = 0; i < n; i++)
        if (tab[i] == name) return (int64_t)i;
    return -1;
}

/* Shows a menu page as the stock R15UIPage2EnablePageNode does. Game thread. 0 = done. */
static int show_page(uint8_t *game, uint64_t owner)
{
    t_fault = 0;
    uintptr_t gs = R64(U(game) + g_prof->o_game_menu_gs);
    int64_t k = !t_fault && gs ? find_actor(gs, owner) : -1;
    if (k < 0) return 1;
    PageHandle h = { NULL, 0, 0 };
    ((FindPageFn)(g_base + g_prof->va_find_page_cs))(&h, (void *)gs, 0x0000ffffffff0000ull | (uint64_t)k, SYM_NONE);
    if (!h.cs || !((ValidFn)(*(void ***)h.cs)[g_prof->vt_cs_valid / 8])(h.cs, h.idx)) return 1;
    ((GameFn)(g_base + g_prof->va_invoke_set))(game);
    ((EnablePageFn)(g_base + g_prof->va_enable_page))(h.cs, h.idx);
    ((GameFn)(g_base + g_prof->va_invoke_clear))(game);
    return 0;
}

/* P2 texts of the last created invite (worker writes, tick reads; under g_wlock) */
static struct { char code[16], expires[32], link[64]; } g_p2;

/* One canvas text: actor's canvasui element <- s. 1 = written. Game thread. */
static int canvas_text(uintptr_t gs, uint64_t actor, uint64_t element, const char *s)
{
    int64_t k = find_actor(gs, actor);
    if (k < 0) return 0;
    PageHandle c = { NULL, 0, 0 };
    ((FindScriptFn)(g_base + g_prof->va_find_canvas))(&c, (void *)gs, 0x0000ffffffff0000ull | (uint64_t)k, SYM_CANVASUI);
    if (!c.cs || !((ValidFn)(*(void ***)c.cs)[g_prof->vt_cs_valid / 8])(c.cs, c.idx)) return 0;
    return ((CanvasSetTextFn)(g_base + g_prof->va_canvas_set_text))(c.cs, c.idx, element, s) != 0;
}

/* P2: write the created invite into the wizard: p41's INVITE CODE (the canvas element, and the script's
 * curr_text_string so the eye toggle redraws it) and EXPIRES, and the p35 link. Game thread. SetString is ignored
 * until the script is live, and the page's own Enable may redraw after us, so the tick applies it three times, 1 s
 * apart. 0 = applied, 1 = retry. */
static int show_code(uint8_t *game, Action *a)
{
    if (a->begin_at && now_ms() < a->begin_at) return 1;
    t_fault = 0;
    uintptr_t gs = R64(U(game) + g_prof->o_game_menu_gs);
    if (t_fault || !gs) return 1;
    char code[16], expires[32], link[64];
    pthread_mutex_lock(&g_wlock);
    memcpy(code, g_p2.code, sizeof code);
    memcpy(expires, g_p2.expires, sizeof expires);
    memcpy(link, g_p2.link, sizeof link);
    pthread_mutex_unlock(&g_wlock);
    static const uint64_t actors[] = { ACTOR_INVITE_CODE, ACTOR_INVITE_CODE2 };
    int done = 0;
    for (size_t i = 0; i < sizeof actors / sizeof actors[0]; i++) {
        int64_t k = find_actor(gs, actors[i]);
        if (k < 0) continue;
        PageHandle h = { NULL, 0, 0 };
        ((FindScriptFn)(g_base + g_prof->va_find_script))(&h, (void *)gs, 0x0000ffffffff0000ull | (uint64_t)k, SYM_NCA_SCRIPT);
        if (h.cs && ((ValidFn)(*(void ***)h.cs)[g_prof->vt_cs_valid / 8])(h.cs, h.idx))
            ((SetStringFn)(g_base + g_prof->va_set_string))(h.cs, h.idx, SYM_CURR_TEXT, code);
        done += canvas_text(gs, actors[i], SYM_CODETEXT, code);
    }
    if (!done) return 1;
    int ex = canvas_text(gs, ACTOR_EXPIRES, SYM_EXPIRESTEXT, expires), lk = canvas_text(gs, ACTOR_LINK_VIEW, SYM_LINKTEXT, link);
    a->gametype[0]++;   /* applications so far (gametype is unused by SHOW_CODE) */
    rlog("code %s written to p41 (%d canvas(es), expires %s, link %s; pass %d)", code, done, ex ? "yes" : "no",
         lk ? "yes" : "no", a->gametype[0]);
    if (a->gametype[0] >= 3) return 0;
    a->begin_at = now_ms() + 1000;
    a->tries = 0;
    return 1;
}

/* p34: a message in the code field's title bar. Applied three times, 1 s apart, as
 * show_code (the page's own Enable may redraw the title after us). Game thread. 0 = applied, 1 = retry. */
static int p34_text(uint8_t *game, Action *a)
{
    if (a->begin_at && now_ms() < a->begin_at) return 1;
    t_fault = 0;
    uintptr_t gs = R64(U(game) + g_prof->o_game_menu_gs);
    if (t_fault || !gs) return 1;
    if (!canvas_text(gs, ACTOR_CODE_FIELD, SYM_TITLETEXT, a->why)) return 1;
    if (!a->gametype[0]++) rlog("p34 title: \"%s\"", a->why);
    if (a->gametype[0] >= 3) return 0;
    a->begin_at = now_ms() + 1000;
    a->tries = 0;
    return 1;
}

static volatile int g_p34_dirty;   /* the title holds one of our messages */
static void p34_say(const char *text)   /* any thread */
{
    __atomic_store_n(&g_p34_dirty, strcmp(text, P34_TITLE) != 0, __ATOMIC_RELAXED);
    Action a;
    memset(&a, 0, sizeof a);
    a.kind = ACT_P34_TEXT;
    snprintf(a.why, sizeof a.why, "%s", text);
    push_action(&a);
}

/* "cf741582-eb0a-4a14-8636-62abffe43e5a" -> the 16-byte GUID layout (u32, u16, u16, u8[8]) of SUuid */
static int parse_guid(const char *s, uint8_t g[16])
{
    unsigned a, b, c, d[8];
    if (strlen(s) != 36 || sscanf(s, "%8x-%4x-%4x-%2x%2x-%2x%2x%2x%2x%2x%2x", &a, &b, &c, &d[0], &d[1], &d[2], &d[3],
                                  &d[4], &d[5], &d[6], &d[7]) != 11)
        return 0;
    memcpy(g, &a, 4);
    uint16_t b16 = (uint16_t)b, c16 = (uint16_t)c;
    memcpy(g + 4, &b16, 2);
    memcpy(g + 6, &c16, 2);
    for (int i = 0; i < 8; i++) g[8 + i] = (uint8_t)d[i];
    return 1;
}

/* NetGame state [ng+0] (GameStateString 0x124d478): 3 logged in (the main menu), 4 loading global, 5 lobby, 6-8
 * loading, 9 in game. As radlinks.c:
 * - state 3: stage CR15Game's -lobbyid block (GUID +0x8538, gametype +0x8560) and run CR15Game::BeginMultiplayer()
 *   0x1201be4, what PLAY runs (CR15NetBeginMultiplayerNode). Its WithLobbyId branch copies the GUID to ng+0x70 and
 *   clears the block; CR15NetGame::FindLobby (state 5) re-reads +0x8560, so the gametype word is restored only when
 *   the netgame reaches state 6. The STATUS page (p51) first, multiplayer a second later.
 * - state 5 / 9: Join(lobby, gametype, &guid, team), as /join_session (LeaveGame first in a match).
 * Returns 0 = done, 1 = not now (retry). */
static uint64_t g_saved_gametype, g_restore_by;   /* g_restore_by: restore at this time even if the join stalls */
static int g_restore_gametype;                     /* 1 staged, 2 the netgame has left state 3 */
enum { NG_LOGGED_IN = 3, NG_LOBBY = 5, NG_IN_GAME = 9 };
static char g_wait_why[96];
static int join_link(uint8_t *game, Action *a)
{
    uint8_t guid[16];
    if (!parse_guid(a->link, guid)) { rlog("join %s: not a GUID", a->link); return 0; }
    t_fault = 0;
    uintptr_t ng = R64(U(game) + g_prof->o_game_netgame), lobby = ng ? R64(ng + g_prof->o_ng_lobby) : 0;
    int st = ng ? (int)R32(ng) : -1, ls = lobby ? (int)R32(lobby + g_prof->o_lobby_state) : -1;
    if (t_fault) { rlog("join %s: fault reading the netgame", a->link); return 0; }
    char why[96];
    snprintf(why, sizeof why, "netgame %s, state %d, lobby %s, state %d", ng ? "set" : "null", st, lobby ? "set" : "null", ls);
    if (ng && st == NG_LOGGED_IN && !g_restore_gametype)
        why[0] = 0;
    else if (lobby && (st == NG_LOBBY || st == NG_IN_GAME) && ls != 2 && ls != 3 && ls != 7 && ls != 8)
        why[0] = 0;
    if (why[0]) {
        if (strcmp(why, g_wait_why)) rlog("join %s: waiting (%s)", a->link, why);
        snprintf(g_wait_why, sizeof g_wait_why, "%s", why);
        return 1;
    }
    g_wait_why[0] = 0;
    uint64_t gt = a->gametype[0] ? csym64(a->gametype) : SYM_NONE;
    if (st == NG_LOGGED_IN && !a->begin_at) {
        rlog("join %s (%s): main menu: STATUS page %s", a->link, a->why,
             show_page(game, P51_STATUS) ? "not found (joining without it)" : "shown");
        a->begin_at = now_ms() + 1000;
        return 1;
    }
    if (st == NG_LOGGED_IN && now_ms() < a->begin_at) return 1;
    if (st == NG_LOGGED_IN) {
        uint64_t *gtw = (uint64_t *)(game + g_prof->o_game_gametype);
        g_saved_gametype = *gtw;
        g_restore_gametype = 1;
        g_restore_by = now_ms() + 60000;
        memcpy(game + g_prof->o_game_lobbyid, guid, 16);
        *gtw = gt;
        rlog("join %s (%s): main menu: -lobbyid staged (team word %04x, gametype %s), beginning multiplayer", a->link,
             a->why, *(uint16_t *)(game + g_prof->o_game_team), a->gametype[0] ? a->gametype : "-1");
        ((GameFn)(g_base + g_prof->va_mp_transition))(game);
        ((GameFn)(g_base + g_prof->va_begin_mp))(game);
        rlog("join %s: multiplayer begun, netgame state %u", a->link, R32(ng));
        return 0;
    }
    rlog("join %s (%s): netgame state %d, lobby state %d, gametype %s", a->link, a->why, st, ls,
         a->gametype[0] ? a->gametype : "-1");
    ((GameFn)(g_base + g_prof->va_mp_transition))(game);
    if (st == NG_IN_GAME) ((LeaveFn)(g_base + g_prof->va_leave))((void *)ng, 0);
    else if (ls >= 4 && ls <= 6) ((CancelFn)(g_base + g_prof->va_cancel))((void *)lobby, 0x1b);   /* as DeepLinkCB */
    ((JoinFn)(g_base + g_prof->va_join))((void *)lobby, gt, guid, 0xffff);
    rlog("join %s: requested", a->link);
    return 0;
}

/* ---- event hook ---- */
static uint64_t actor_name(uint8_t *gs, uint64_t sactor)
{
    t_fault = 0;
    uintptr_t ad = R64(U(gs) + g_prof->o_gs_actors);
    uint64_t idx = sactor & 0xffff, n = ad ? R64(ad + g_prof->o_at_count) : 0;
    uintptr_t names = ad ? R64(ad + g_prof->o_at_names) : 0;
    if (t_fault || !names || idx >= n || n > 0xfffe) return 0;
    uint64_t v = R64(names + idx * 8);
    return t_fault ? 0 : v;
}

static void on_wizard_event(uint8_t *gs, const uint64_t *ev)
{
    uint64_t sym = ev[0], comp = ev[3];
    if (sym != SYM_CARD && sym != SYM_BTN && sym != SYM_TEXT && sym != SYM_LINK_BTN && sym != SYM_BACK
        && sym != SYM_CANCEL)
        return;
    uint64_t owner = actor_name(gs, ev[2]);
    if (owner != P34_JOIN_CODE && owner != P35_LINK && owner != P36_EXPERIENCE && owner != P37_ARENA_MODE
        && owner != P38_COMBAT_MODE && owner != P39_MAP && owner != P40_SETTINGS && owner != P41_SHARE)
        return;
    rlog("event %016llx owner=%016llx component=%016llx x=%016llx", ULL(sym), ULL(owner), ULL(comp), ULL(ev[4]));

    if (sym == SYM_CARD) {
        int c = card_index(comp);
        pthread_mutex_lock(&g_wlock);
        if (owner == P36_EXPERIENCE) {
            g_wiz.experience = c == 1 ? EXP_LOBBY : c == 2 ? EXP_ARENA : c == 3 ? EXP_COMBAT : EXP_NONE;
            g_wiz.mode = MODE_NONE;
            g_wiz.map = 0;
        } else if (owner == P37_ARENA_MODE) {
            g_wiz.mode = c == 1 ? MODE_CUSTOM : c == 2 ? MODE_AI : MODE_NONE;
        } else if (owner == P38_COMBAT_MODE) {
            g_wiz.mode = c == 1 ? MODE_PAYLOAD : c == 2 ? MODE_CAPTURE : c == 3 ? MODE_RANDOM : MODE_NONE;
            g_wiz.map = 0;
        } else if (owner == P39_MAP) {
            g_wiz.map = c == 1 ? 1 : c == 2 ? 2 : 0;
        }
        rlog("wizard: experience=%s mode=%s map=%d (card %d)", exp_name(g_wiz.experience), mode_name(g_wiz.mode),
             g_wiz.map, c);
        pthread_mutex_unlock(&g_wlock);
        return;
    }
    if (sym == SYM_BTN && comp == UID_CREATE && owner == P40_SETTINGS) {
        rlog("CREATE pressed (the request follows the page's create_invite metric)");
        return;
    }
    if (sym == SYM_BTN && comp == UID_CREATE && owner == P41_SHARE) {
        Action a = { ACT_JOIN, "", "", "", 0, 0 };
        pthread_mutex_lock(&g_wlock);
        snprintf(a.link, sizeof a.link, "%s", g_wiz.link);
        snprintf(a.why, sizeof a.why, "%s", g_wiz.code);
        snprintf(a.gametype, sizeof a.gametype, "%s", g_wiz.gametype);
        pthread_mutex_unlock(&g_wlock);
        if (!a.link[0]) { rlog("JOIN pressed: no invite created yet"); return; }
        rlog("JOIN pressed: our invite %s", a.why);
        push_action(&a);
        return;
    }
    if (sym == SYM_TEXT && owner == P34_JOIN_CODE) rlog("ENTER pressed (the code follows in the join metric)");
    if ((sym == SYM_CANCEL || sym == SYM_BACK) && owner == P34_JOIN_CODE) {   /* leaving p34 ends a password prompt */
        pthread_mutex_lock(&g_wlock);
        int had = g_pw.code[0] != 0;
        g_pw.code[0] = 0;
        pthread_mutex_unlock(&g_wlock);
        if (had) rlog("p34 left: password prompt dropped");
        if (had || g_p34_dirty) p34_say(P34_TITLE);
    }
}

/* ---- the click-metric JSON ---- */
static int json_str(const char *js, const char *key, char *out, size_t cap)
{
    char pat[64];
    snprintf(pat, sizeof pat, "\"%s\"", key);
    const char *k = strstr(js, pat);
    if (!k) return 0;
    k = strchr(k + strlen(pat), ':');
    if (!k) return 0;
    while (*++k == ' ') {}
    if (*k != '"') return 0;
    const char *e = strchr(++k, '"');
    if (!e || (size_t)(e - k) >= cap) return 0;
    memcpy(out, k, e - k);
    out[e - k] = 0;
    return 1;
}

static int expires_seconds(const char *s)
{
    int n = atoi(s);
    if (n <= 0) return 0;
    return strstr(s, "MIN") ? n * 60 : strstr(s, "HOUR") ? n * 3600 : strstr(s, "DAY") ? n * 86400 : 0;
}

static void on_metric(const char *js)
{
    char panel[64], event[64];
    if (!strstr(js, "\"panel_type\":\"deep_linking\"")) return;
    if (!json_str(js, "panel_name", panel, sizeof panel) || !json_str(js, "event_name", event, sizeof event)) return;
    rlog("metric %s/%s", panel, event);   /* the JSON user_id is not read: "{ " on the Quest */
    if (!strcmp(panel, "create_game_review_settings") && !strcmp(event, "create_invite")) {
        Job j;
        memset(&j, 0, sizeof j);
        j.kind = JOB_CREATE;
        char v[64] = "";
        pthread_mutex_lock(&g_wlock);
        j.wiz = g_wiz;
        pthread_mutex_unlock(&g_wlock);
        json_str(js, "Password", j.password, sizeof j.password);
        json_str(js, "Server_region", j.region, sizeof j.region);
        j.expires_s = json_str(js, "Expires_after", v, sizeof v) ? expires_seconds(v) : 0;
        rlog("create: experience=%s mode=%s map=%d password=%s region=%s expires=%s (%d s)",
             exp_name(j.wiz.experience), mode_name(j.wiz.mode), j.wiz.map, j.password[0] ? "set" : "none", j.region,
             v, j.expires_s);
        push_job(&j);
    } else if (!strcmp(panel, "join_via_code") && !strcmp(event, "join")) {
        Job j;
        memset(&j, 0, sizeof j);
        j.kind = JOB_RESOLVE;
        char typed[64] = "";
        if (!json_str(js, "join_code", typed, sizeof typed) || !strcmp(typed, "<invalid>")) {
            rlog("join via code: no code (%s)", typed);
            return;
        }
        /* After a 403 "needs a password" the next ENTER is the password for that code. The field still holds the
         * code unless the player cleared it, so a typed "<code><password>" counts as the password alone. */
        pthread_mutex_lock(&g_wlock);
        if (g_pw.code[0] && now_ms() >= g_pw.until) g_pw.code[0] = 0;
        snprintf(j.code, sizeof j.code, "%s", g_pw.code);
        pthread_mutex_unlock(&g_wlock);
        if (j.code[0] && !strcasecmp(typed, j.code)) {   /* ENTER again on the code itself: still waiting */
            p34_say("ENTER PASSWORD");
            return;
        }
        if (j.code[0]) {
            size_t n = strlen(j.code);
            const char *pw = !strncasecmp(typed, j.code, n) && typed[n] ? typed + n : typed;
            snprintf(j.password, sizeof j.password, "%s", pw);
            rlog("join via code: password for code=%s (%zu chars)", j.code, strlen(j.password));
        } else if (strlen(typed) >= sizeof j.code) {
            rlog("join via code: %zu characters, too long for a code", strlen(typed));
            p34_say("INVALID CODE");
            return;
        } else {
            snprintf(j.code, sizeof j.code, "%s", typed);
            rlog("join via code: code=%s", j.code);
        }
        push_job(&j);
    }
}

/* Runs on the thread that logs: on the Quest a script worker, not the tick's thread (tids 3990 vs 3936, run
 * 20261020-201635-host), as are the wizard events. Both hooks only copy and queue under locks; copies and returns. */
static void metric_hook(const char *json, uint64_t len)
{
    if (json && len > 0 && len < 4096) {
        char buf[4096];
        if (!safe_copy(buf, U(json), len)) {
            buf[len] = 0;
            on_metric(buf);
        }
    } else if (json && len >= 4096) {
        rlog("metric line of %llu B not read (over 4095 B)", ULL(len));   /* the wizard's lines are < 300 B */
    }
    g_metric_orig(json, len);
}

static void target_hook(void *gs, const uint64_t *ev, uint32_t flags)
{
    on_wizard_event((uint8_t *)gs, ev);
    g_target_orig(gs, ev, flags);
}

/* ---- services host: asked by the worker, answered on the game thread by the engine's getter ---- */
static char g_svc_url[256];
static char g_svc_src[96];
static int g_svc_req;   /* 0 idle, 1 asked (worker), 4 answering (tick), 2 answered, 3 worker gave up */

/* Game thread: CJson::TString(&netgame->config, "apiservice_host", literal, 0) -- the rule the engine's API stubs use. */
static void services_answer(uint8_t *g)
{
    const char *lit = (const char *)(U(g_base) + g_prof->va_api_url), *s = NULL;
    t_fault = 0;
    uintptr_t ng = R64(U(g) + g_prof->o_game_netgame);
    if (ng && !t_fault)
        s = ((JsonTStringFn)(U(g_base) + g_prof->va_json_tstring))((const void *)(ng + g_prof->o_ng_config),
                                                                   "apiservice_host", lit, 0);
    int from_cfg = s && s != lit && *s;
    snprintf(g_svc_url, sizeof g_svc_url, "%s", from_cfg ? s : lit);
    snprintf(g_svc_src, sizeof g_svc_src, "%s", from_cfg ? "the config's apiservice_host"
                                               : ng ? "the libr15.so API literal; no apiservice_host in the config"
                                                    : "the libr15.so API literal; no netgame yet");
    __atomic_store_n(&g_svc_req, 2, __ATOMIC_RELEASE);
}

/* Game actions run here, on the game thread; the queue is taken with a try-lock (never blocks a frame). */
static void tick_hook(void *game)
{
    static uint64_t next_user;
    uint8_t *g = (uint8_t *)game;
    uint64_t t = now_ms();
    if (t >= next_user) { next_user = t + 2000; refresh_user(g); }
    /* answer when the worker asks, or unasked once the netgame first reaches logged-in (so the log names the host
     * at login, not at the first invite) */
    int sq = __atomic_load_n(&g_svc_req, __ATOMIC_ACQUIRE);
    if (sq == 0 || sq == 1) {
        t_fault = 0;
        uintptr_t ng = sq == 0 ? R64(U(g) + g_prof->o_game_netgame) : 0;
        int ready = sq == 1 || (ng && !t_fault && (int)R32(ng) == NG_LOGGED_IN && !t_fault);
        if (ready && __atomic_compare_exchange_n(&g_svc_req, &sq, 4, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
            services_answer(g);
    }
    if (g_restore_gametype) {
        t_fault = 0;
        uintptr_t ng = R64(U(g) + g_prof->o_game_netgame);
        int st = ng && !t_fault ? (int)R32(ng) : -1;
        /* restore at state >= 6 (FindLobby has read it), back at the menu after leaving it (the join failed), or
         * after 60 s whatever the state (a join parked in state 4/5 must not leave the invite's gametype for PLAY) */
        if (st >= 6 || (st == NG_LOGGED_IN && g_restore_gametype == 2) || t >= g_restore_by) {
            *(uint64_t *)(g + g_prof->o_game_gametype) = g_saved_gametype;
            rlog("gametype word restored (netgame state %d%s)", st, st < 6 && t >= g_restore_by ? ", after 60 s" : "");
            g_restore_gametype = 0;
        } else if (st != NG_LOGGED_IN) {
            g_restore_gametype = 2;
        }
    }
    if (g_nacts && !pthread_mutex_trylock(&g_alock)) {
        Action batch[ACAP];
        int n = g_nacts;
        memcpy(batch, g_acts, n * sizeof *batch);
        g_nacts = 0;
        pthread_mutex_unlock(&g_alock);
        for (int i = 0; i < n; i++) {
            int again = batch[i].kind == ACT_JOIN ? join_link(g, &batch[i]) :
                        batch[i].kind == ACT_SHOW_CODE ? show_code(g, &batch[i]) :
                        batch[i].kind == ACT_P34_TEXT ? p34_text(g, &batch[i]) : 0;
            if (again && ++batch[i].tries < 900) push_action(&batch[i]);   /* about 10 s of frames */
            else if (again && batch[i].kind == ACT_SHOW_CODE) rlog("code %s: p41's script never took it", batch[i].link);
            else if (again && batch[i].kind == ACT_P34_TEXT) rlog("p34 title \"%s\": the field's canvas never took it", batch[i].why);
            else if (again) rlog("join %s: the game never became ready, dropped", batch[i].link);
        }
    }
    g_tick_orig(game);
}

/* ---- HTTP (worker thread only) ---- */
static char g_svc_host[128];
static char g_svc_port[8];
static int g_svc_state;   /* 0 not asked yet, 1 usable, -1 unusable (logged once) */

/* Before the first request: ask the game thread (3 s), else read the literal here; parse "http://HOST[:PORT]/...". */
static int services_ready(void)
{
    if (g_svc_state) return g_svc_state > 0;
    int idle = 0;
    __atomic_compare_exchange_n(&g_svc_req, &idle, 1, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);   /* no-op if answered */
    uint64_t t0 = now_ms();
    while (__atomic_load_n(&g_svc_req, __ATOMIC_ACQUIRE) != 2 && now_ms() - t0 < 3000) sleep_ms(20);
    int asked = 1;   /* give up only if the tick has not started answering; else wait for it */
    if (__atomic_compare_exchange_n(&g_svc_req, &asked, 3, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        char url[96] = "";
        if (!safe_copy(url, U(g_base) + g_prof->va_api_url, sizeof url - 1)) url[sizeof url - 1] = 0;
        snprintf(g_svc_url, sizeof g_svc_url, "%s", url);
        snprintf(g_svc_src, sizeof g_svc_src, "the libr15.so API literal; the game thread did not answer in 3 s");
    } else {
        while (__atomic_load_n(&g_svc_req, __ATOMIC_ACQUIRE) != 2) sleep_ms(5);
    }
    const char *u = g_svc_url;
    g_svc_state = -1;
    if (!strncasecmp(u, "https://", 8)) {
        rlog("services: UNUSABLE: %.120s (from %s) is https; rad-links speaks plain HTTP only", u, g_svc_src);
        return 0;
    }
    if (strncasecmp(u, "http://", 7)) {
        rlog("services: UNUSABLE: cannot parse %.120s (from %s); want http://HOST[:PORT]/...", u, g_svc_src);
        return 0;
    }
    u += 7;
    size_t hl = strcspn(u, ":/");
    if (!hl || hl >= sizeof g_svc_host) {
        rlog("services: UNUSABLE: bad host in %.120s (from %s)", g_svc_url, g_svc_src);
        return 0;
    }
    memcpy(g_svc_host, u, hl);
    g_svc_host[hl] = 0;
    int port = 80;
    if (u[hl] == ':' && (sscanf(u + hl + 1, "%d", &port) != 1 || port <= 0 || port > 65535)) {
        rlog("services: UNUSABLE: bad port in %.120s (from %s)", g_svc_url, g_svc_src);
        return 0;
    }
    snprintf(g_svc_port, sizeof g_svc_port, "%d", port);
    g_svc_state = 1;
    /* a trial lookup, so a bad name is in the log at login and not only at the first invite (retried per request) */
    struct addrinfo hints = { 0 }, *res = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int gai = getaddrinfo(g_svc_host, g_svc_port, &hints, &res);
    char ip[64] = "?";
    if (!gai && res) {
        const void *a = res->ai_family == AF_INET6 ? (const void *)&((struct sockaddr_in6 *)res->ai_addr)->sin6_addr
                                                   : (const void *)&((struct sockaddr_in *)res->ai_addr)->sin_addr;
        inet_ntop(res->ai_family, a, ip, sizeof ip);
    }
    if (res) freeaddrinfo(res);
    if (gai) rlog("services: %s:%s (from %s); cannot resolve %s now: %s", g_svc_host, g_svc_port, g_svc_src, g_svc_host,
                  gai_strerror(gai));
    else rlog("services: %s:%s (from %s) -> %s", g_svc_host, g_svc_port, g_svc_src, ip);
    return 1;
}

/* One request (HTTP/1.0, Connection: close, 5 s timeouts); returns the HTTP status (0 = transport error), body in out.
 * The host is resolved per request (getaddrinfo: an IPv4/IPv6 literal or a name). */
static int http(const char *method, const char *path, const char *body, char *out, size_t cap)
{
    out[0] = 0;
    if (!services_ready()) { snprintf(out, cap, "no usable services host (%.80s)", g_svc_url); return 0; }
    struct addrinfo hints = { 0 }, *res = NULL, *ai;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int gai = getaddrinfo(g_svc_host, g_svc_port, &hints, &res);
    if (gai) {
        snprintf(out, cap, "resolve %s: %s", g_svc_host, gai_strerror(gai));
        rlog("services: cannot resolve %s: %s", g_svc_host, gai_strerror(gai));
        return 0;
    }
    int s = -1, err = 0;
    for (ai = res; ai; ai = ai->ai_next) {
        s = socket(ai->ai_family, ai->ai_socktype | SOCK_CLOEXEC, ai->ai_protocol);
        if (s < 0) { err = errno; continue; }
        struct timeval tv = { 5, 0 };
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
        if (!connect(s, ai->ai_addr, ai->ai_addrlen)) break;
        err = errno;
        close(s);
        s = -1;
    }
    freeaddrinfo(res);
    if (s < 0) {
        snprintf(out, cap, "connect %s:%s errno %d", g_svc_host, g_svc_port, err);
        return 0;
    }
    char req[1024];
    size_t blen = body ? strlen(body) : 0;
    int n = snprintf(req, sizeof req, "%s %s HTTP/1.0\r\nHost: %s:%s\r\nConnection: close\r\n", method, path, g_svc_host,
                     g_svc_port);
    if (body) n += snprintf(req + n, sizeof req - n, "Content-Type: application/json\r\nContent-Length: %zu\r\n", blen);
    n += snprintf(req + n, sizeof req - n, "\r\n%s", body ? body : "");
    int status = 0;
    if (n > 0 && (size_t)n < sizeof req && send(s, req, (size_t)n, MSG_NOSIGNAL) == n) {
        static char resp[8192];
        size_t got = 0;
        ssize_t r;
        while (got < sizeof resp - 1 && (r = recv(s, resp + got, sizeof resp - 1 - got, 0)) > 0) got += (size_t)r;
        resp[got] = 0;
        char *bodyp = strstr(resp, "\r\n\r\n");
        if (sscanf(resp, "HTTP/%*s %d", &status) != 1) status = 0;
        snprintf(out, cap, "%s", bodyp ? bodyp + 4 : resp);
    } else {
        snprintf(out, cap, "send errno %d", errno);
    }
    close(s);
    return status;
}

static void do_create(const Job *j)
{
    const Route *rt = route_of(&j->wiz);
    if (!rt) {
        rlog("create: no session route for experience=%s mode=%s map=%d", exp_name(j->wiz.experience),
             mode_name(j->wiz.mode), j->wiz.map);
        return;
    }
    char body[512], resp[2048], code[16], link[48], err[256], exp_at[48] = "";
    if (!j->wiz.user[0]) { rlog("create: no player id yet (XPlatformId)"); return; }
    int n = snprintf(body, sizeof body, "{\"user_id\":\"%s\",\"gametype\":\"%s\"", j->wiz.user, rt->gametype);
    if (rt->level) n += snprintf(body + n, sizeof body - n, ",\"level\":\"%s\"", rt->level);
    if (j->expires_s) n += snprintf(body + n, sizeof body - n, ",\"expires_s\":%d", j->expires_s);
    if (j->password[0] && strcasecmp(j->password, "OFF") && !strchr(j->password, '"') && !strchr(j->password, '\\'))
        n += snprintf(body + n, sizeof body - n, ",\"password\":\"%s\"", j->password);
    /* Region: the UI label ("US WEST"); the host maps it (Regions.cs) and falls back to a "default" server. */
    if (j->region[0] && !strchr(j->region, '"') && !strchr(j->region, '\\'))
        n += snprintf(body + n, sizeof body - n, ",\"region\":\"%s\"", j->region);
    snprintf(body + n, sizeof body - n, "}");
    int st = http("POST", "/invites", body, resp, sizeof resp);
    if (st != 200 || !json_str(resp, "code", code, sizeof code) || !json_str(resp, "link", link, sizeof link)) {
        if (!json_str(resp, "error", err, sizeof err)) snprintf(err, sizeof err, "%s", resp);
        rlog("create %s/%s: HTTP %d %s", rt->gametype, rt->level ? rt->level : "-", st, err);
        return;
    }
    pthread_mutex_lock(&g_wlock);
    snprintf(g_wiz.code, sizeof g_wiz.code, "%s", code);
    snprintf(g_wiz.link, sizeof g_wiz.link, "%s", link);
    snprintf(g_wiz.gametype, sizeof g_wiz.gametype, "%s", rt->gametype);
    pthread_mutex_unlock(&g_wlock);
    char srv_region[32] = "";
    json_str(resp, "region", srv_region, sizeof srv_region);
    rlog("created code=%s link=%s gametype=%s level=%s region=%s", code, link, rt->gametype, rt->level ? rt->level : "-",
         srv_region[0] ? srv_region : "-");
    json_str(resp, "expires_at", exp_at, sizeof exp_at);   /* "2026-10-21T05:38:59.63Z" -> "EXPIRES: 05:38 UTC" */
    pthread_mutex_lock(&g_wlock);
    snprintf(g_p2.code, sizeof g_p2.code, "%s", code);
    if (strlen(exp_at) >= 16 && exp_at[10] == 'T') snprintf(g_p2.expires, sizeof g_p2.expires, "EXPIRES: %.5s UTC", exp_at + 11);
    else snprintf(g_p2.expires, sizeof g_p2.expires, "EXPIRES: -");
    snprintf(g_p2.link, sizeof g_p2.link, "-lobbyid %s", link);   /* the boot argument that joins it, 45 chars */
    pthread_mutex_unlock(&g_wlock);
    Action a = { ACT_SHOW_CODE, "", "create", "", 0, 0 };
    snprintf(a.link, sizeof a.link, "%s", code);
    push_action(&a);
}

/* The p34 message for a failed resolve (host Invites.cs: 404 unknown, 410 expired / revoked / server gone, 403 needs
 * or wrong password, 503 no free game server: the code stays valid, retry); 0 = no answer. */
static const char *resolve_message(int st, const char *err)
{
    if (st == 404) return "INVALID CODE";
    if (st == 503) return "NO SERVER FREE";
    if (st == 410) return strstr(err, "expired") ? "CODE EXPIRED" : "CODE CLOSED";
    if (st == 403) return strstr(err, "needs a password") ? "ENTER PASSWORD" : "WRONG PASSWORD";
    if (st == 0) return "NO CONNECTION";
    return "SERVER ERROR";
}

static void do_resolve(const Job *j)
{
    char path[256], resp[2048], link[48], err[256];
    size_t n = (size_t)snprintf(path, sizeof path, "/invites/%s", j->code);
    if (j->password[0] && n < sizeof path) {   /* GET /invites/{code}?password=… (Invites.cs), percent-encoded */
        n += (size_t)snprintf(path + n, sizeof path - n, "?password=");
        for (const unsigned char *p = (const unsigned char *)j->password; *p && n < sizeof path - 4; p++)
            n += (size_t)(isalnum(*p) || *p == '-' || *p == '_' || *p == '.' || *p == '~'
                              ? snprintf(path + n, sizeof path - n, "%c", *p)
                              : snprintf(path + n, sizeof path - n, "%%%02X", *p));
    }
    int st = http("GET", path, NULL, resp, sizeof resp);
    if (st != 200 || !json_str(resp, "link", link, sizeof link)) {
        if (!json_str(resp, "error", err, sizeof err)) snprintf(err, sizeof err, "%s", resp);
        rlog("resolve %s%s: HTTP %d %s", j->code, j->password[0] ? " (with password)" : "", st, err);
        const char *msg = resolve_message(st == 200 ? 500 : st, err);
        pthread_mutex_lock(&g_wlock);
        if (st == 403) {   /* needs / wrong password: (re)open the prompt for this code */
            snprintf(g_pw.code, sizeof g_pw.code, "%s", j->code);
            g_pw.until = now_ms() + PASSWORD_WINDOW_MS;
        } else {
            g_pw.code[0] = 0;
        }
        pthread_mutex_unlock(&g_wlock);
        p34_say(msg);
        return;
    }
    pthread_mutex_lock(&g_wlock);
    g_pw.code[0] = 0;
    pthread_mutex_unlock(&g_wlock);
    if (g_p34_dirty) p34_say(P34_TITLE);   /* leave the field as the page set it, for the next visit */
    Action a = { ACT_JOIN, "", "", "", 0, 0 };
    json_str(resp, "gametype", a.gametype, sizeof a.gametype);
    rlog("resolved code=%s link=%s gametype=%s", j->code, link, a.gametype);
    snprintf(a.link, sizeof a.link, "%s", link);
    snprintf(a.why, sizeof a.why, "%s", j->code);
    push_action(&a);
}

static void *worker_thread(void *unused)
{
    (void)unused;
    for (;;) {
        pthread_mutex_lock(&g_jlock);
        while (!g_njobs) {
            if (!g_svc_state && __atomic_load_n(&g_svc_req, __ATOMIC_ACQUIRE) == 2) {   /* answered at login: log it */
                pthread_mutex_unlock(&g_jlock);
                services_ready();
                pthread_mutex_lock(&g_jlock);
                continue;
            }
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 1;
            pthread_cond_timedwait(&g_jcond, &g_jlock, &ts);
        }
        Job j = g_jobs[0];
        memmove(g_jobs, g_jobs + 1, --g_njobs * sizeof *g_jobs);
        pthread_mutex_unlock(&g_jlock);
        if (j.kind == JOB_CREATE) do_create(&j);
        else if (j.kind == JOB_RESOLVE) do_resolve(&j);
    }
    return NULL;
}

/* ---- attach: find libr15.so, check identity, wait for RELRO, swap / chain each word ---- */
static uintptr_t g_found;
static uintptr_t g_self_base;     /* this library's load base (dladdr of the constructor) */
static char g_self_dir[256];      /* and its directory, with the trailing '/' */
static int find_cb(struct dl_phdr_info *info, size_t size, void *data)
{
    (void)size; (void)data;
    const char *n = info->dlpi_name, *s = n ? strrchr(n, '/') : NULL;
    const char *base = s ? s + 1 : n;
    if (n && !strcmp(base, "libr15.so") && info->dlpi_addr && info->dlpi_phnum) g_found = (uintptr_t)info->dlpi_addr;
    return 0;
}

/* An "earlier sibling": a module from this library's own directory (the APK's lib/arm64-v8a), mapped before it in
 * link-map order, that is neither libr15.so nor this library: an LD_PRELOAD entry ahead of us, or a loader.
 * No name is looked at. Such a module may hook the same words; see settle_words(). */
struct order_scan { int seen_self, siblings; };
static int order_cb(struct dl_phdr_info *info, size_t size, void *data)
{
    (void)size;
    struct order_scan *o = data;
    const char *n = info->dlpi_name;
    size_t dl = strlen(g_self_dir);
    if (o->seen_self || !n || !dl) return 0;
    if ((uintptr_t)info->dlpi_addr == g_self_base) { o->seen_self = 1; return 0; }
    if ((uintptr_t)info->dlpi_addr == g_found) return 0;
    if (!strncmp(n, g_self_dir, dl) && !strchr(n + dl, '/')) o->siblings++;
    return 0;
}

static int page_perms(uintptr_t addr, char perms[5])
{
    FILE *f = fopen("/proc/self/maps", "re");
    char ln[512];
    int rc = -1;
    if (!f) return -1;
    while (fgets(ln, sizeof ln, f)) {
        unsigned long lo, hi;
        char p[5];
        if (sscanf(ln, "%lx-%lx %4s", &lo, &hi, p) == 3 && addr >= lo && addr < hi) {
            memcpy(perms, p, 5);
            rc = 0;
            break;
        }
    }
    fclose(f);
    return rc;
}

/* A non-stock word may be chained only when it points into an EXECUTABLE mapping (/proc/self/maps 'x') of a loaded
 * module (dladdr) that is neither libr15.so nor this library (compared by load base, not by name). NULL + *why
 * otherwise. */
static const char *foreign_module(uintptr_t cur, const char **why)
{
    char p[5] = "";
    Dl_info di;
    if (page_perms(cur, p)) { *why = "not in any mapping"; return NULL; }
    if (p[2] != 'x') { *why = "not in an executable mapping"; return NULL; }
    if (!dladdr((void *)cur, &di) || !di.dli_fbase) { *why = "executable but in no loaded module"; return NULL; }
    if (U(di.dli_fbase) == U(g_base)) { *why = "inside libr15.so but not its stock target"; return NULL; }
    if (U(di.dli_fbase) == g_self_base) { *why = "already this library's own hook"; return NULL; }
    return di.dli_fname ? di.dli_fname : "(unnamed module)";
}

/* Swap the word at `at` for `value`; *orig gets what it held (before the store: the hook may run at once): the stock
 * target (swap), or a hook inside another loaded executable module (chain). Anything else is refused and logged.
 * The store is a compare-and-swap, so a module swapping the same word concurrently is never overwritten: on a lost
 * race the new value is judged again. */
static int swap_word(uintptr_t at, uintptr_t stock, uintptr_t value, void **orig, const char *what)
{
    long pg = sysconf(_SC_PAGESIZE);
    uintptr_t page = at & ~(uintptr_t)(pg - 1);
    for (int tries = 0; tries < 4; tries++) {
        uintptr_t cur = __atomic_load_n((uintptr_t *)at, __ATOMIC_ACQUIRE);
        if (cur != stock) {
            const char *why = "", *m = foreign_module(cur, &why);
            if (!m) {
                rlog("%s: %p holds %p, not the stock %p and not chainable (%s): refusing this hook", what, (void *)at,
                     (void *)cur, (void *)stock, why);
                return 0;
            }
            rlog("%s: holds %p in %s (not the stock %p): chaining", what, (void *)cur, m, (void *)stock);
        }
        if (mprotect((void *)page, (size_t)pg, PROT_READ | PROT_WRITE)) {
            rlog("%s: mprotect RW failed (errno %d): refusing this hook", what, errno);
            return 0;
        }
        __atomic_store_n((void **)orig, (void *)cur, __ATOMIC_RELEASE);
        uintptr_t expect = cur;
        int ok = __atomic_compare_exchange_n((uintptr_t *)at, &expect, value, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
        if (mprotect((void *)page, (size_t)pg, PROT_READ)) rlog("%s: mprotect R failed (errno %d): left writable", what, errno);
        if (ok) {
            rlog("%s hooked", what);
            return 1;
        }
        rlog("%s: changed to %p while swapping: judging again", what, (void *)expect);
    }
    rlog("%s: kept changing: refusing this hook", what);
    return 0;
}

/* With an earlier sibling mapped (another hook tool preloaded first), give it the first swap: rad-links accepts
 * a non-stock word (chains), some tools refuse one. Both threads see the same relocation event, so without this
 * the order is a race. Bounded and name-free: wait until none of the three words has changed for SETTLE_QUIET_MS,
 * at most SETTLE_MAX_MS. Alone (no sibling) there is no wait. */
#define SETTLE_QUIET_MS 1000
#define SETTLE_MAX_MS 3000
static void settle_words(const uintptr_t w[3], int siblings)
{
    if (!siblings) return;
    uint64_t t0 = now_ms(), last = t0;
    uint64_t v[3];
    int changed = 0;
    for (int i = 0; i < 3; i++) v[i] = R64(w[i]);
    while (now_ms() - last < SETTLE_QUIET_MS && now_ms() - t0 < SETTLE_MAX_MS) {
        sleep_ms(20);
        for (int i = 0; i < 3; i++) {
            uint64_t x = R64(w[i]);
            if (x != v[i]) { v[i] = x; last = now_ms(); changed++; }
        }
    }
    rlog("%d earlier module(s) in %s: words settled after %llu ms (%d change(s) seen)", siblings, g_self_dir,
         ULL(now_ms() - t0), changed);
}

static void *attach_thread(void *unused)
{
    (void)unused;
    uint64_t t0 = now_ms();
    while (!g_found) {
        dl_iterate_phdr(find_cb, NULL);
        if (g_found) break;
        if (now_ms() - t0 > 300000) { rlog("inert: libr15.so not mapped after 300 s"); return NULL; }
        sleep_ms(100);
    }
    g_base = (uint8_t *)g_found;
    struct order_scan os = { 0, 0 };
    dl_iterate_phdr(order_cb, &os);
    rlog("libr15.so mapped after %llu ms; %d earlier module(s) in this library's directory", ULL(now_ms() - t0),
         os.siblings);
    t0 = now_ms();
    for (size_t i = 0; i < sizeof g_prof->checks / sizeof g_prof->checks[0]; i++) {
        const Prologue *p = &g_prof->checks[i];
        uint8_t got[16] = { 0 };
        if (safe_copy(got, U(g_base) + p->va, 16)) {
            if (now_ms() - t0 > 60000) { rlog("inert: %s at +%#x unreadable after 60 s", p->what, p->va); return NULL; }
            sleep_ms(20);
            i--;
            continue;
        }
        if (memcmp(got, p->bytes, 16)) {
            rlog("inert: %s prologue at +%#x differs: not libr15.so 6528386917200980", p->what, p->va);
            return NULL;
        }
    }
    uintptr_t slot = U(g_base) + g_prof->va_tick_slot, gtarget = U(g_base) + g_prof->got_send_target,
              gmetric = U(g_base) + g_prof->got_metric;
    uintptr_t want_tick = U(g_base) + g_prof->va_tick_orig, want_target = U(g_base) + g_prof->va_send_target,
              want_metric = U(g_base) + g_prof->va_metric;
    /* relocated + RELRO read-only (the metric slot is ours alone, so its stock value is the relocation signal) */
    t0 = now_ms();
    for (;;) {
        char p1[5] = "", p2[5] = "", p3[5] = "";
        t_fault = 0;
        uintptr_t vm = R64(gmetric);
        int prot = !page_perms(slot, p1) && !page_perms(gtarget, p2) && !page_perms(gmetric, p3)
                   && p1[1] == '-' && p2[1] == '-' && p3[1] == '-';
        if (!t_fault && vm == want_metric && prot) break;
        if (now_ms() - t0 > 60000) { rlog("inert: not relocated / RELRO not read-only after 60 s"); return NULL; }
        sleep_ms(20);
    }
    {
        const uintptr_t words[3] = { gmetric, gtarget, slot };
        settle_words(words, os.siblings);
    }
    SYM_CARD = csym64("evt_card_clicked");
    SYM_BTN = csym64("evt_btn_clicked");
    SYM_TEXT = csym64("evt_text_submitted");
    SYM_LINK_BTN = csym64("evt_link_button_clicked");
    SYM_BACK = csym64("evt_back_btn_clicked");
    SYM_CANCEL = csym64("evt_submission_canceled");
    /* all three are needed (target feeds the wizard, tick runs its actions, metric reads settings and the code): the
     * first refused word leaves rad-links inert; a word already swapped stays, and its hook only chains */
    if (!swap_word(gmetric, want_metric, U(metric_hook), (void **)&g_metric_orig, "metric GOT (NR15Logger::MetricLog)")) {
        rlog("inert: metric word refused");
        return NULL;
    }
    if (!swap_word(gtarget, want_target, U(target_hook), (void **)&g_target_orig, "target GOT (SendComponentEvent)")) {
        rlog("inert: target word refused (metric hooked, chaining only)");
        return NULL;
    }
    if (!swap_word(slot, want_tick, U(tick_hook), (void **)&g_tick_orig, "tick vtable slot (CR15Game+0x150)")) {
        rlog("inert: tick word refused (metric + target hooked; the wizard's actions cannot run)");
        return NULL;
    }
    snprintf(g_hooked, sizeof g_hooked, "metric,target,tick");
    pthread_t th;
    if (pthread_create(&th, NULL, worker_thread, NULL)) rlog("worker thread failed");
    else pthread_detach(th);
    rlog("ready: hooks %s", g_hooked);
    return NULL;
}

__attribute__((constructor)) static void radlinks_init(void)
{
    char exe[256];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    exe[n > 0 ? n : 0] = 0;
    if (!strstr(exe, "app_process")) return;
    unsetenv("LD_PRELOAD");
    Dl_info self;
    if (dladdr((void *)radlinks_init, &self) && self.dli_fbase) {
        g_self_base = U(self.dli_fbase);
        const char *sl = self.dli_fname ? strrchr(self.dli_fname, '/') : NULL;
        if (sl && (size_t)(sl - self.dli_fname + 1) < sizeof g_self_dir) {
            memcpy(g_self_dir, self.dli_fname, (size_t)(sl - self.dli_fname + 1));
            g_self_dir[sl - self.dli_fname + 1] = 0;
        }
    }
    rlog("rad-links loaded in pid %d; waiting for libr15.so", getpid());
    pthread_t th;
    if (pthread_create(&th, NULL, attach_thread, NULL)) rlog("attach thread failed");
    else pthread_detach(th);
}
