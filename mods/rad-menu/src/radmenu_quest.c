/* libradmenu.so — rad-menu for Echo VR Quest (libr15.so of 6528386917200980, vc 4987566).
 *
 * The Quest port of src/radmenu.c (PC). It shares no
 * code with the PC file, as rad-links does not. 
 *   - installer (radlinks_quest.c's attach path: wait for libr15.so, 16-byte prologue checks, RELRO wait, per-word
 *     swap-or-chain) for three words: tick = CR15Game vftable slot 0x362ee80 (stock UpdateReset 0x17ec928), core =
 *     GOT 0x36d52c8 (SendComponentEventGlobal 0x180281c: page on/off, pointer, dots, friend rows, HELP presses),
 *     target = GOT 0x36dbf40 (CncaGameSpace::SendComponentEvent 0x1807264: the news tile click rewrite, SEASON PASS
 *     template, SAFE ZONE). rad-links holds tick + target too: either load order chains.
 *   - fault guard: every engine pointer read goes through safe_copy (pipe write), as radlinks_quest.c.
 *   - carousel + dots (dot_box colours), dot press -> slide at once, hover pause (pointer start/end + hover/idle
 *     events about the news tile or a dot hold the rotation; resume hover_resume_ms after), slide-1 art = the season
 *     tile's texture (FindPrimitiveID 0x138fc98 + SetOverrideTexture 0x13924ec; a NULL slot = the tile's own art,
 *     put back by clearing), news click at slide 1 -> season click event, at slide 2 -> p42.
 *   - tab titles + highlight, footer copy (ElementText 0x1a627a4: the ONLINE footer strings into the HELP / SETTINGS
 *     copies), HELP SEASON PASS -> the page-5 season route, SAFE ZONE -> p48 (own event, evt_card_clicked fallback).
 *   - /friends feed: worker thread, HTTP/1.0 GET /friends?user_id=<XPlatformId>&format=tsv; a row press with a session
 *     calls rad-links' exported radlinks_follow(guid, gametype) (dlsym, resolved per press).
 *   - options: radmenu.ini (key=value, optional [radmenu] section, PC's keys), first found of
 *     /sdcard/Android/data/com.readyatdawn.r15/files/radmenu.ini, /sdcard/radmenu.ini,
 *     /data/data/com.readyatdawn.r15/files/radmenu.ini; compiled defaults otherwise. enabled=0 installs no hook;
 *     carousel=0 / tab_title=0 turn the news tile and the banner writes off.
 *   - tree gate: every canvas write, click rewrite and page route runs only while the minted actor ffa_news_dot0 is
 *     in the menu gamespace (checked at each page-24 enable and on a gamespace change); /friends polls only while the
 *     minted friend rows are there too, and backs off 300 s (doubling to 30 min) after 400/404/405/409 or a transport
 *     error. On the stock menu the plugin only tracks pages. radmenu.log moves to radmenu.log.1 at 256 KB.
 * Quest VAs (ELF-dynsym confirmed @ Quest 6528386917200980): Find<CCanvasUICS> 0x1cbb408, CCanvasUICS::SetText
 * 0x139111c, SetColor 0x1391208 (handle, CSymbol64, C3Vector const&), ShowUIElement 0x1391298 (handle, CSymbol64,
 * uint), FindPrimitiveID 0x138fc98 (handle, CSymbol64, CUICanvasInstance*&) -> id, SetOverrideTexture 0x13924ec,
 * CUICanvasInstance::ElementText 0x1a627a4 (inst, id) const, CncaGame::Set/ClearAllInvokeImmediate 0x17ebe48 /
 * 0x17ebf34, page enable 0x1f79314 + find 0x206f410 (rad-links'), menu gamespace = game + 0x7af0, actor table
 * gs+0x370 (names +0x80, count +0xa8), CS valid = vtable + 0x1d0. Prim layout (disasm-only, = PC): prims inst+0x90,
 * stride 0xe0, type +0x08, slot +0x0c, type-3 override table inst+0xc8.
 * QUIT WARNING (quit_warning=1, on by default): PC's section with Quest calls, see below. */
#define _GNU_SOURCE
#include <android/log.h>
#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <link.h>
#include <netdb.h>
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

#define TAG "radmenu"
#define ULL(x) ((unsigned long long)(x))
#define U(p) ((uintptr_t)(p))

/* ---- CSymbol64 (copied from radlinks_quest.c) ---- */
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

/* ---- the Quest profile (libr15.so 6528386917200980; ELF-dynsym / radlinks_quest.c) ---- */
typedef struct { uint32_t va; uint8_t bytes[16]; const char *what; } Prologue;
typedef struct { void *cs; uint32_t idx, pad; } Handle;
typedef void (*TickFn)(void *game);
typedef void (*SendCoreFn)(void *gs, const uint64_t *event, uint32_t flags);
typedef void (*FindFn)(Handle *out, void *gs, uint64_t sactor, uint64_t comp);
typedef uint8_t (*ValidFn)(void *cs, uint32_t idx);
typedef uint8_t (*SetTextFn)(void *cs, uint32_t idx, uint64_t element, const char *s);
typedef void (*SetColorFn)(void *cs, uint32_t idx, uint64_t element, const float *rgb);
typedef void (*ShowFn)(void *cs, uint32_t idx, uint64_t element, uint32_t show);
typedef struct { uint64_t type, id; } SNSUserID;
typedef SNSUserID (*XPlatFn)(const void *netgame);
typedef const char *(*JsonTStringFn)(const void *json, const char *path, const char *def, uint32_t flags);
typedef uint32_t (*FindPrimFn)(void *cs, uint32_t idx, uint64_t element, void **inst);   /* id by value */
typedef void (*SetOvrFn)(void *cs, uint32_t idx, uint64_t element, const void *tex);
typedef const char *(*ElementTextFn)(const void *inst, uint32_t id);
typedef void (*GameFn)(void *game);
typedef void (*EnablePageFn)(void *cs, uint32_t idx);
typedef struct {
    uint32_t va_tick_slot, va_tick_orig, va_send_core, got_send_core, va_send_target, got_send_target;
    uint32_t o_game_netgame, o_game_menu_gs, o_ng_config, o_gs_actors, o_at_names, o_at_count, vt_cs_valid;
    uint32_t va_find_canvas, va_canvas_set_text, va_canvas_set_color, va_canvas_show;
    uint32_t va_xplatform_id, va_json_tstring, va_api_url;
    uint32_t va_find_prim, va_set_override_tex, va_element_text;
    uint32_t o_inst_prims, prim_stride, o_prim_type, o_prim_slot, o_inst_sprite_tex;
    uint32_t va_find_page_cs, va_enable_page, va_invoke_set, va_invoke_clear;
    Prologue checks[16];
} Profile;
static const Profile g_profile = {
    0x362ee80, 0x17ec928, 0x180281c, 0x36d52c8, 0x1807264, 0x36dbf40,
    0x8510, 0x7af0, 0x63240, 0x370, 0x80, 0xa8, 0x1d0,
    0x1cbb408, 0x139111c, 0x1391208, 0x1391298,
    0x1244628, 0xfa2e7c, 0x2baa029,
    0x138fc98, 0x13924ec, 0x1a627a4,
    0x90, 0xe0, 0x08, 0x0c, 0xc8,
    0x206f410, 0x1f79314, 0x17ebe48, 0x17ebf34,
    {
        { 0x180281c, { 0xff,0x03,0x02,0xd1, 0xf7,0x5b,0x05,0xa9, 0xf5,0x53,0x06,0xa9, 0xf3,0x7b,0x07,0xa9 }, "send_core" },
        { 0x17ec928, { 0x08,0x80,0x60,0x39, 0x48,0x00,0x10,0x37, 0xc0,0x03,0x5f,0xd6, 0x08,0x00,0x40,0xf9 }, "update_reset" },
        { 0x1cbb408, { 0xff,0x83,0x01,0xd1, 0xf8,0x13,0x00,0xf9, 0xf7,0x5b,0x03,0xa9, 0xf5,0x53,0x04,0xa9 }, "find_canvas" },
        { 0x139111c, { 0x43,0x02,0x00,0xb4, 0xff,0x83,0x00,0xd1, 0xf3,0x7b,0x01,0xa9, 0xf3,0x03,0x03,0xaa }, "canvas_set_text" },
        { 0x1391208, { 0xff,0x83,0x00,0xd1, 0xf3,0x7b,0x01,0xa9, 0x28,0xc5,0x00,0xf0, 0x08,0x81,0x30,0x91 }, "canvas_set_color" },
        { 0x1391298, { 0xff,0x83,0x00,0xd1, 0xf3,0x7b,0x01,0xa9, 0xf3,0x03,0x03,0x2a, 0x21,0x7c,0x40,0x92 }, "canvas_show" },
        { 0x1244628, { 0xfe,0x0f,0x1f,0xf8, 0x00,0x68,0x54,0xf9, 0x80,0x00,0x00,0xb4, 0x63,0x5a,0xf3,0x97 }, "xplatform_id" },
        { 0xfa2e7c,  { 0xfc,0x63,0xbc,0xa9, 0xf7,0x5b,0x01,0xa9, 0xf5,0x53,0x02,0xa9, 0xf3,0x7b,0x03,0xa9 }, "json_tstring" },
        { 0x1807264, { 0xff,0x03,0x02,0xd1, 0xf6,0x2b,0x00,0xf9, 0xf5,0x53,0x06,0xa9, 0xf3,0x7b,0x07,0xa9 }, "send_target" },
        { 0x138fc98, { 0xfe,0x0f,0x1f,0xf8, 0x21,0x7c,0x40,0x92, 0x0c,0xf2,0xed,0x97, 0x00,0x7c,0x40,0x92 }, "find_prim" },
        { 0x13924ec, { 0xff,0x83,0x00,0xd1, 0xf3,0x7b,0x01,0xa9, 0xf3,0x03,0x03,0xaa, 0x21,0x7c,0x40,0x92 }, "set_override_tex" },
        { 0x1a627a4, { 0xff,0xc3,0x00,0xd1, 0xf4,0x0b,0x00,0xf9, 0xf3,0x7b,0x02,0xa9, 0xe1,0x1b,0x00,0xb9 }, "element_text" },
        { 0x206f410, { 0xff,0x83,0x01,0xd1, 0xf8,0x13,0x00,0xf9, 0xf7,0x5b,0x03,0xa9, 0xf5,0x53,0x04,0xa9 }, "find_page_cs" },
        { 0x1f79314, { 0xff,0xc3,0x02,0xd1, 0xfc,0x6f,0x05,0xa9, 0xfa,0x67,0x06,0xa9, 0xf8,0x5f,0x07,0xa9 }, "enable_page" },
        { 0x17ebe48, { 0xf3,0x7b,0xbf,0xa9, 0x08,0x58,0x7d,0xf9, 0xf3,0x03,0x00,0xaa, 0xa8,0x00,0x00,0xb4 }, "invoke_set" },
        { 0x17ebf34, { 0xfa,0x0f,0x1b,0xf8, 0xf9,0x63,0x01,0xa9, 0xf7,0x5b,0x02,0xa9, 0xf5,0x53,0x03,0xa9 }, "invoke_clear" },
    },
};
static const Profile *g_prof = &g_profile;
static uint8_t *g_base;   /* libr15.so load base */
static TickFn g_tick_orig;
static SendCoreFn g_core_orig, g_target_orig;
/* Report Player v1 options and state shared with report_flow.h (tentative definitions; the header repeats them) */
static int g_rp_on = -1, g_rp_ready;   /* report: -1 auto (default auto), 0 off, 1 any level */
static uint64_t g_rp_tile_uid = 0xaa15d7d322666e42ull,   /* = RP_TILE_UID_DEFAULT (report_flow.h) */
                g_rp_card_event, g_rp_card_uid, g_rp_card_el, S_STATE_MGR;
static char g_rp_card_event_s[96], g_rp_card_el_s[96];   /* names: csym64 at report_start */
static char g_rp_glyph_s[64] = "cancel_button";            /* report_card_glyph (empty = off); csym64 at report_start */
static int report_option(const char *k, const char *v);    /* report keys (report_flow.h) */
static int rp_parse_report(const char *v);    /* the shared `report=` parser (report_flow.h) */
#define NG_LOGGED_IN 3

/* ---- constants: PC's, the same bytes on the Quest ---- */
#define P24_HOME     0x05ce113632359ce3ull
#define P5_TILES     0x08324e714b4a21aaull
#define P58_HELP     0xe73618208c520c8dull
#define P30_SETTINGS 0x05eb6c644ad6ef27ull
#define ACTOR_BANNER 0x99af55d93126a17aull
#define RAIL_ONLINE  0x7593d0177b013e69ull
#define RAIL_SETTING 0x623b5f0e1e11a695ull
#define RAIL_HELP    0x71f7c477f0619237ull
#define RAIL_QUIT    0x720d06f0ec096825ull
#define ACTOR_NEWS   0x27dfdb284b22a843ull
#define ACTOR_SEASON 0x9e0bf5fe92aba570ull
#define P42_QUIT     0x791142542a3a6dc5ull
#define P48_LEGAL    0x63ebda9f6d08e654ull   /* the SAFE ZONE target ( recipe) */
#define EV_NEWS      0xfb75d76107e450d8ull   /* evt_news_info_clicked (news tile click_event row) */
#define EV_SEASON    0x0b64e3596273caacull   /* the season tile's click_event row */
#define EV_HELP_SEASON 0x8adf5a10c6b3d9b6ull /* csym64("evt_ffa_help_season_clicked") */
#define EV_HELP_SAFE 0x27091852f7d04146ull   /* csym64("evt_ffa_help_safezone_clicked") */
#define EV_CARD_CLICKED 0x8a53d85233b8739eull /* csym64("evt_card_clicked") */
#define ACTOR_SAFE   0x6bead65a6955c109ull   /* the HELP SAFE ZONE card (p58) */
#define SEASON_COMP  0x6aea99eeb2bab6c4ull   /* the page-5 owner's one CScriptCR component (stream-confirmed) */
#define EV_PTR_HOVER 0x0c40e9c29ebcc824ull   /* bhvr_delegate_OnPointerInteractHover */
#define EV_PTR_IDLE  0x27b7ebd0100cb25dull   /* bhvr_delegate_OnPointerInteractIdle */
#define EV_PTR_START 0x55c56d4a9201a66cull   /* delegate_OnStartIntersection */
#define EV_PTR_END   0x7005afbd6c613578ull   /* delegate_OnEndIntersection */
/* FTUE + combat lock (stream-confirmed mnu_master @ 6323983201049540; the Quest menu has the same bytes) */
#define P25_CORE_TUT 0x90fb42d145ca476bull   /* home picker rows: core tutorials / multiplayer tutorials / NPE home */
#define P26_MP_TUT   0xf0b6d16660e5e275ull
#define P27_NPE_HOME 0xe6652937bf86afc9ull
#define P34_JOIN     0x2d50833b445fe66cull   /* JOIN VIA CODE */
#define P36_EXPER    0x6a9e38e434a28911ull   /* CREATE GAME: CHOOSE EXPERIENCE */
#define CARD_CREATE  0xff5375e5a4dee29dull
#define CARD_JOIN    0x097b785350a61c5full
#define CARD_COMBAT  0x876f7826ee807123ull   /* p36 ECHO COMBAT card (right slot) */
#define UID_CARD_R   0x35ec861316acbb35ull   /* evt_card_clicked ev[3] of the right card on p36-p39 */
#define ROW_MYCODE   0x94d77fe1ca3e487full   /* home row MY INVITE CODE field */
#define ROW_LINK     0x37181a73f772c20dull   /* home row INVITE LINK */
#define ROW_FRIENDS  0x1b71f837dc9e5669ull   /* INVITE FRIENDS bar */
#define UID_MY_LINK  0x3414b253e2c886e0ull   /* INVITE LINK's component uid (ev[3] of its press) */
#define EV_EYE       0xc2a9462dedd77c58ull   /* evt_ffa_my_code_eye_clicked */
#define EL_CARDIMAGE 0xb40d036053c7e447ull   /* ui_mnu_card elements */
#define EL_CARDTEXT  0x35ec86133dfa9c72ull
#define EL_CARDSUB   0xdb9f609fbd9f4f80ull
#define EL_LOCKSPRITE 0xd65c2439a9d088d5ull  /* prim 6, the lock face */
#define EL_LOCKTEXT  0x68101e824402fbbaull   /* prim 7, shipped "Requires Oculus Link or Air Link" */
#define FTUE_TEXT    "Complete the tutorial first"
#define SYM_NONE     0xffffffffffffffffull
#define EV_PAGE_ON   0xd64f3478683e4a9cull
#define EV_PAGE_OFF  0x7aad67cf0466af38ull
#define SYM_CANVASUI 0x35ec9a0128ec916full
#define NSLIDE 3
static const uint64_t ACTOR_DOT[NSLIDE] = { 0x1cfbcecb4b2d74a4ull, 0x1cfbcecb4b2d74a5ull, 0x1cfbcecb4b2d74a6ull };
static const uint64_t EV_DOT[NSLIDE] = { 0x368bf9e7a924467eull, 0x1dd2dfb4f1b3d514ull, 0x6039b541180b60aaull };
static const char *const g_title[NSLIDE] = { "SELF-GOALING PATCH", "SEASON 7 LAUNCH", "EARLY QUIT INFO" };
/* banner title per tab: home = MAIN MENU as PC radmenu.c and the Coffey frames */
static const char *const g_tab_title[3] = { "MAIN MENU", "SETTINGS", "HELP" };
static uint64_t SYM_TEXT, SYM_DOT_ON, SYM_DOT_OFF, SYM_DOT_BOX, SYM_BACKGROUND, SYM_CODETEXT, SYM_CODETEXT_OFF, SYM_BG,
                SYM_NOOP, SYM_HIGHLIGHT, SYM_LINK_BTN;
/* options (radmenu.ini, PC's keys; these are the compiled defaults) */
static int g_period_ms = 5000, g_tab_hl = 1, g_friends = 1, g_fr_period_ms = 5000, g_hide_empty = 1;
static int g_sprite = 1, g_dots_mode = 0, g_fixed = -1, g_hover_pause = 1, g_hover_resume_ms = 2000, g_square_dot = 1;
static int g_help_season = 1, g_season_p24 = 0, g_help_safe = 1, g_help_safe_card = 1, g_footer_copy = 1;
static int g_enabled = 1, g_carousel = 1, g_tab_title_on = 1;   /* master switch (0 = no hook at all), carousel, banner */
static int g_ftue_mode = 1;     /* ftue = off (0) | auto (1, default: the picker's NPE page) | force (2, gate test) */
static int g_combat_lock;       /* combat_lock = off (0) | purchase (1) | hardware (2): test face on the p36 combat card */
static const char *const g_ftue_word[3] = { "off", "auto", "force" }, *const g_cl_word[3] = { "off", "purchase", "hardware" };
static const char *const g_lo_word[3] = { "off", "auto", "force" };
static int g_lo_mode = 1;             /* lockout_lock = off (0) | auto (1, default; never locks a stock library) | force (2) */
static int g_lo_force_sec = 900;      /* lockout_force_sec */
static char g_lo_prefix[32] = "LOCKOUT";   /* lockout_text: prim 7 = "<text> hh:mm:ss" ("" = the time only) */
static int opt_word(const char *v, const char *const *words, int n, int def)
{
    for (int i = 0; i < n; i++)
        if (!strcasecmp(v, words[i])) return i;
    return def;
}
static char g_dot_glyph[8] = "o";
static uint64_t g_season_comp = SEASON_COMP;
/* QUIT WARNING options (radmenu.ini quit_warning, quit_warning_text, quit_warning_element, quit_warning_actor) */
/* The warning element is one unwrapped line (~60 chars at its size on PC): Coffey's full copy (
 * "Quitting early affects the game for everyone. Additional quits will incur a penalty. Are you sure you want to
 * quit?") clipped at both ends on a device run; quit_warning_text can still set it. */
#define QW_TEXT_DEFAULT "Quitting early affects everyone and adds a penalty. Quit?"
static int g_qw_on = 1;   /* quit_warning, default on */
static char g_qw_text[256] = QW_TEXT_DEFAULT, g_qw_el_s[96], g_qw_actor_s[96];

/* ---- log: logcat (tag radmenu) + files/radmenu.log ---- */
#define LOG_PATH "/data/data/com.readyatdawn.r15/files/radmenu.log"
#define LOG_CAP (256 * 1024)   /* past this the file moves to radmenu.log.1 and a new one starts */
static pthread_mutex_t g_log_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_log_fd = -1, g_log_tries;
static off_t g_log_size;
static void log_open(int trunc)
{
    g_log_fd = open(LOG_PATH, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC | (trunc ? O_TRUNC : 0), 0644);
    g_log_size = g_log_fd >= 0 ? lseek(g_log_fd, 0, SEEK_END) : 0;
    if (g_log_size < 0) g_log_size = 0;
}
static void rlog_to(int file, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#define rlog(...) rlog_to(1, __VA_ARGS__)
#define rlog_cat(...) rlog_to(0, __VA_ARGS__)   /* logcat only: steady per-period lines stay out of radmenu.log */
static void rlog_to(int file, const char *fmt, ...)
{
    char buf[1024];
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
    if (!file) return;
    buf[n++] = '\n';
    pthread_mutex_lock(&g_log_lock);
    if (g_log_fd < 0 && g_log_tries < 50) {
        g_log_tries++;
        log_open(0);
    }
    if (g_log_fd >= 0 && g_log_size + n > LOG_CAP) {
        close(g_log_fd);
        g_log_fd = -1;
        if (rename(LOG_PATH, LOG_PATH ".1")) {}
        log_open(1);
    }
    if (g_log_fd >= 0 && write(g_log_fd, buf, n) > 0) g_log_size += n;
    pthread_mutex_unlock(&g_log_lock);
}

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

/* ---- options: radmenu.ini (key=value lines, '#' / ';' comments, an optional [radmenu] section; other sections are
 * skipped). The first readable path wins. Unknown keys are logged and ignored. ---- */
static char g_ini_user[64];
static const char *const g_ini_paths[] = {
    "/sdcard/Android/data/com.readyatdawn.r15/files/radmenu.ini",
    "/sdcard/radmenu.ini",
    "/data/data/com.readyatdawn.r15/files/radmenu.ini",
};
static int ini_int(const char *v, int lo, int hi, int def)
{
    char *e;
    long x = strtol(v, &e, 0);
    return (e == v || x < lo || x > hi) ? def : (int)x;
}
static void read_options(void)
{
    FILE *f = NULL;
    const char *path = NULL;
    for (size_t i = 0; !f && i < sizeof g_ini_paths / sizeof g_ini_paths[0]; i++)
        if ((f = fopen(g_ini_paths[i], "re"))) path = g_ini_paths[i];
    if (!f) { rlog("options: no radmenu.ini (compiled defaults: enabled=1 carousel=1 tab_title=1 ftue=auto lockout_lock=auto quit_warning=1 combat_lock=off)"); return; }
    char ln[256];
    int in_sec = 1, keys = 0;
    while (fgets(ln, sizeof ln, f)) {
        char *k = ln + strspn(ln, " \t\r\n");
        if (!*k || *k == '#' || *k == ';') continue;
        if (*k == '[') { in_sec = !strncasecmp(k, "[radmenu]", 9); continue; }
        if (!in_sec) continue;
        char *eq = strchr(k, '=');
        if (!eq) continue;
        char *ke = eq;
        while (ke > k && (ke[-1] == ' ' || ke[-1] == '\t')) ke--;
        *ke = 0;
        char *v = eq + 1 + strspn(eq + 1, " \t");
        v[strcspn(v, "\r\n")] = 0;
        for (char *t = v + strlen(v); t > v && (t[-1] == ' ' || t[-1] == '\t'); ) *--t = 0;
        keys++;
        if (!strcasecmp(k, "enabled")) g_enabled = ini_int(v, 0, 1, 1);
        else if (!strcasecmp(k, "carousel")) g_carousel = ini_int(v, 0, 1, 1);
        else if (!strcasecmp(k, "tab_title")) g_tab_title_on = ini_int(v, 0, 1, 1);
        else if (!strcasecmp(k, "period_ms")) g_period_ms = ini_int(v, 500, 600000, 5000);
        else if (!strcasecmp(k, "sprite")) g_sprite = ini_int(v, 0, 1, 1);
        else if (!strcasecmp(k, "dots")) g_dots_mode = ini_int(v, 0, 2, 0);
        else if (!strcasecmp(k, "dot_glyph")) snprintf(g_dot_glyph, sizeof g_dot_glyph, "%s", v);
        else if (!strcasecmp(k, "fixed")) g_fixed = ini_int(v, -1, NSLIDE - 1, -1);
        else if (!strcasecmp(k, "hover_pause")) g_hover_pause = ini_int(v, 0, 1, 1);
        else if (!strcasecmp(k, "hover_resume_ms")) g_hover_resume_ms = ini_int(v, 0, 60000, 2000);
        else if (!strcasecmp(k, "friends")) g_friends = ini_int(v, 0, 1, 1);
        else if (!strcasecmp(k, "friends_period_ms")) g_fr_period_ms = ini_int(v, 1000, 600000, 5000);
        else if (!strcasecmp(k, "user_id")) snprintf(g_ini_user, sizeof g_ini_user, "%s", v);
        else if (!strcasecmp(k, "tab_highlight")) g_tab_hl = ini_int(v, 0, 2, 1);
        else if (!strcasecmp(k, "square_dot")) g_square_dot = ini_int(v, 0, 1, 1);
        else if (!strcasecmp(k, "help_season")) g_help_season = ini_int(v, 0, 1, 1);
        else if (!strcasecmp(k, "season_p24_first")) g_season_p24 = ini_int(v, 0, 1, 0);
        else if (!strcasecmp(k, "season_comp")) { if (*v) g_season_comp = strtoull(v, NULL, 0); }
        else if (!strcasecmp(k, "help_safezone")) g_help_safe = ini_int(v, 0, 1, 1);
        else if (!strcasecmp(k, "help_safezone_card")) g_help_safe_card = ini_int(v, 0, 1, 1);
        else if (!strcasecmp(k, "footer_copy")) g_footer_copy = ini_int(v, 0, 1, 1);
        else if (!strcasecmp(k, "hide_empty_rows")) g_hide_empty = ini_int(v, 0, 1, 1);
        else if (!strcasecmp(k, "ftue")) g_ftue_mode = opt_word(v, g_ftue_word, 3, 1);
        else if (!strcasecmp(k, "combat_lock")) g_combat_lock = opt_word(v, g_cl_word, 3, 0);
        else if (!strcasecmp(k, "lockout_lock")) g_lo_mode = opt_word(v, g_lo_word, 3, 1);
        else if (!strcasecmp(k, "lockout_force_sec")) g_lo_force_sec = ini_int(v, 5, 86400, 900);
        else if (!strcasecmp(k, "lockout_text")) snprintf(g_lo_prefix, sizeof g_lo_prefix, "%s", v);
        else if (!strcasecmp(k, "report")) g_rp_on = rp_parse_report(v);   /* the one shared parser (report_flow.h), same on PC */
        else if (!strcasecmp(k, "report_tile_uid")) { if (*v) g_rp_tile_uid = strtoull(v, NULL, 0); }   /* empty: default */
        else if (!strcasecmp(k, "report_card_event")) snprintf(g_rp_card_event_s, sizeof g_rp_card_event_s, "%s", v);
        else if (!strcasecmp(k, "report_card_uid")) g_rp_card_uid = *v ? strtoull(v, NULL, 0) : 0;
        else if (!strcasecmp(k, "report_card_element")) snprintf(g_rp_card_el_s, sizeof g_rp_card_el_s, "%s", v);
        else if (!strcasecmp(k, "report_card_glyph")) snprintf(g_rp_glyph_s, sizeof g_rp_glyph_s, "%s", v);
        else if (!strcasecmp(k, "quit_warning")) g_qw_on = ini_int(v, 0, 1, 1);
        else if (!strcasecmp(k, "quit_warning_text")) snprintf(g_qw_text, sizeof g_qw_text, "%s", *v ? v : QW_TEXT_DEFAULT);
        else if (!strcasecmp(k, "quit_warning_element")) snprintf(g_qw_el_s, sizeof g_qw_el_s, "%s", v);
        else if (!strcasecmp(k, "quit_warning_actor")) snprintf(g_qw_actor_s, sizeof g_qw_actor_s, "%s", v);
        else if (report_option(k, v)) {}
        else { rlog("options: unknown key \"%s\" ignored", k); keys--; }
    }
    fclose(f);
    rlog("options (%s, %d keys): enabled=%d carousel=%d tab_title=%d period_ms=%d sprite=%d dots=%d dot_glyph=\"%s\" "
         "fixed=%d hover_pause=%d hover_resume_ms=%d",
         path, keys, g_enabled, g_carousel, g_tab_title_on, g_period_ms, g_sprite, g_dots_mode, g_dot_glyph, g_fixed, g_hover_pause, g_hover_resume_ms);
    rlog("options: tab_highlight=%d square_dot=%d help_season=%d season_p24_first=%d season_comp=%016llx help_safezone=%d "
         "help_safezone_card=%d footer_copy=%d friends=%d friends_period_ms=%d hide_empty_rows=%d user_id %s",
         g_tab_hl, g_square_dot, g_help_season, g_season_p24, ULL(g_season_comp), g_help_safe, g_help_safe_card,
         g_footer_copy, g_friends, g_fr_period_ms, g_hide_empty, g_ini_user[0] ? "set (ini)" : "from XPlatformId");
    rlog("options: ftue=%s combat_lock=%s", g_ftue_word[g_ftue_mode], g_cl_word[g_combat_lock]);
    rlog("options: lockout_lock=%s lockout_force_sec=%d lockout_text=\"%s\"", g_lo_word[g_lo_mode], g_lo_force_sec, g_lo_prefix);
    rlog("options: quit_warning=%d quit_warning_element=%s quit_warning_actor=%s quit_warning_text=\"%s\"", g_qw_on,
         g_qw_el_s[0] ? g_qw_el_s : "(default)", g_qw_actor_s[0] ? g_qw_actor_s : "(scan)", g_qw_text);
}

/* ---- fault-guarded reads (radlinks_quest.c: write from the address into a private pipe) ---- */
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

/* ---- canvas access (game thread) ---- */
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

static int canvas_of(uintptr_t gs, uint64_t actor, Handle *c)
{
    int64_t k = find_actor(gs, actor);
    if (k < 0) return 0;
    memset(c, 0, sizeof *c);
    ((FindFn)(g_base + g_prof->va_find_canvas))(c, (void *)gs, 0x0000ffffffff0000ull | (uint64_t)k, SYM_CANVASUI);
    return c->cs && ((ValidFn)(*(void ***)c->cs)[g_prof->vt_cs_valid / 8])(c->cs, c->idx);
}

static int canvas_text(uintptr_t gs, uint64_t actor, uint64_t element, const char *s)
{
    Handle c;
    return canvas_of(gs, actor, &c) && ((SetTextFn)(g_base + g_prof->va_canvas_set_text))(c.cs, c.idx, element, s) != 0;
}

static int canvas_color(uintptr_t gs, uint64_t actor, uint64_t element, const float rgb[3])
{
    Handle c;
    if (!canvas_of(gs, actor, &c)) return 0;
    float v[4] __attribute__((aligned(16))) = { rgb[0], rgb[1], rgb[2], 0 };   /* C3Vector: 12 B used, 16 B safe */
    ((SetColorFn)(g_base + g_prof->va_canvas_set_color))(c.cs, c.idx, element, v);
    return 1;
}

static int canvas_show(uintptr_t gs, uint64_t actor, uint64_t element, uint32_t show)
{
    Handle c;
    if (!canvas_of(gs, actor, &c)) return 0;
    ((ShowFn)(g_base + g_prof->va_canvas_show))(c.cs, c.idx, element, show);
    return 1;
}

/* The texture a sprite element draws now (its instance's type-3 override slot), or NULL. FindPrimitiveID logs an
 * engine error line for a missing element (q3 art finding): call it only for elements the canvas has. */
static const void *sprite_tex(uintptr_t gs, uint64_t actor, uint64_t element)
{
    Handle c;
    void *inst = NULL;
    if (!canvas_of(gs, actor, &c)) return NULL;
    uint32_t id = ((FindPrimFn)(g_base + g_prof->va_find_prim))(c.cs, c.idx, element, &inst);
    if (id == 0xffffffffu || !inst) return NULL;
    t_fault = 0;
    uintptr_t prims = R64(U(inst) + g_prof->o_inst_prims);
    if (t_fault || !prims) return NULL;
    uintptr_t prim = prims + (uint64_t)id * g_prof->prim_stride;
    uint32_t type = R32(prim + g_prof->o_prim_type), slot = R32(prim + g_prof->o_prim_slot);
    uintptr_t tbl = R64(U(inst) + g_prof->o_inst_sprite_tex);
    if (t_fault || type != 3 || slot > 64 || !tbl) return NULL;
    const void *tex = (const void *)(uintptr_t)R64(tbl + 8ull * slot);
    return t_fault ? NULL : tex;
}

/* tex NULL = clear the override (the engine's own ClearOverrideTexture path): the tile's own sprite draws again. */
static int set_tex(uintptr_t gs, uint64_t actor, uint64_t element, const void *tex)
{
    Handle c;
    if (!canvas_of(gs, actor, &c)) return 0;
    ((SetOvrFn)(g_base + g_prof->va_set_override_tex))(c.cs, c.idx, element, tex);
    return 1;
}

/* footer copy: an element's current text (ElementText on the instance FindPrimitiveID returns); 1 = read,
 * 0 = no such element / no text, -1 = the actor's canvas is not there. */
static int canvas_get_text(uintptr_t gs, uint64_t actor, uint64_t element, char *out, size_t cap)
{
    Handle c;
    void *inst = NULL;
    out[0] = 0;
    if (!canvas_of(gs, actor, &c)) return -1;   /* no canvas (yet): not a missing element */
    uint32_t id = ((FindPrimFn)(g_base + g_prof->va_find_prim))(c.cs, c.idx, element, &inst);
    if (id == 0xffffffffu || !inst) return 0;
    const char *s = ((ElementTextFn)(g_base + g_prof->va_element_text))(inst, id);
    if (!s) return 0;
    size_t n = 0;
    for (; n + 1 < cap; n++) {
        char ch;
        if (safe_copy(&ch, U(s) + n, 1) || !ch) break;
        out[n] = ch;
    }
    out[n] = 0;
    return 1;
}

/* Shows a menu page as the stock R15UIPage2EnablePageNode does (radlinks_quest.c show_page). Game thread. 0 = done. */
static int show_page(uint8_t *game, uint64_t owner)
{
    t_fault = 0;
    uintptr_t gs = R64(U(game) + g_prof->o_game_menu_gs);
    int64_t k = !t_fault && gs ? find_actor(gs, owner) : -1;
    if (k < 0) return 1;
    Handle h = { NULL, 0, 0 };
    ((FindFn)(g_base + g_prof->va_find_page_cs))(&h, (void *)gs, 0x0000ffffffff0000ull | (uint64_t)k, SYM_NONE);
    if (!h.cs || !((ValidFn)(*(void ***)h.cs)[g_prof->vt_cs_valid / 8])(h.cs, h.idx)) return 1;
    ((GameFn)(g_base + g_prof->va_invoke_set))(game);
    ((EnablePageFn)(g_base + g_prof->va_enable_page))(h.cs, h.idx);
    ((GameFn)(g_base + g_prof->va_invoke_clear))(game);
    return 0;
}

/* ---- fix 2: HELP / SETTINGS footer strings = ONLINE's (PC radmenu.c, same actors and elements) ---- */
#define ACTOR_RADV   0xc11d245b2fac7825ull
#define ACTOR_STATUS 0x7da8ba854cd99a2bull
typedef struct { uint64_t src, dst[2], element; const char *literal; } FooterEl;
static const FooterEl g_footer[5] = {
    { ACTOR_RADV, { 0x5ad7052968bc8cbcull, 0xad6477c0ea361317ull }, 0xa6fbcbfe579dd357ull, "rad15_live" },
    { ACTOR_RADV, { 0x5ad7052968bc8cbcull, 0xad6477c0ea361317ull }, 0x457626a49b59152aull, "YYYY-MM-DDTHH:MM:SSZ" },
    { ACTOR_RADV, { 0x5ad7052968bc8cbcull, 0xad6477c0ea361317ull }, 0x8607a8176ff5bd98ull, "999999" },
    { ACTOR_STATUS, { 0x16b4cfb2bc8da93eull, 0xa784cd5c9fea2ccdull }, 0x53f0bc198da47f87ull, "SERVER STATUS: " },
    { ACTOR_STATUS, { 0x16b4cfb2bc8da93eull, 0xa784cd5c9fea2ccdull }, 0xd51a85db5e5629edull,
      "Planned maintenance on xxxxxxx at xxxxxxxxx. Please see discord for more information." },
};
static char g_footer_val[5][128], g_footer_logged[5][128];
static uint64_t g_footer_next;
static int g_footer_miss[5];   /* reads with no element: after 5 the element is not asked again (engine error lines) */

static void apply_footer(uintptr_t gs, uint64_t now, int tab)
{
    if (!g_footer_copy || now < g_footer_next) return;
    g_footer_next = now + 1000;
    for (int i = 0; i < 5; i++) {
        char cur[128];
        if (g_footer_miss[i] >= 5) continue;
        int got = canvas_get_text(gs, g_footer[i].src, g_footer[i].element, cur, sizeof cur);
        if (got < 0) continue;
        if (!got && ++g_footer_miss[i] == 5) rlog("FOOTER el %d: no text after 5 reads: not asked again", i);
        if (got > 0) g_footer_miss[i] = 0;
        if (got > 0 && cur[0]
            && strcmp(cur, g_footer[i].literal) && strcmp(cur, g_footer_val[i])) {
            snprintf(g_footer_val[i], sizeof g_footer_val[i], "%s", cur);
            rlog("FOOTER el %d: ONLINE reads \"%s\" (cached)", i, cur);
        }
    }
    /* (Quest only): the ONLINE version string draws garbled. The HELP / SETTINGS copies sit at the source's
     * exact pose (polish/build.sh) and, before their first write, hold the template literals; if they draw on p24
     * too (the inferred cause: the garble = literal glyphs over the real ones, field by field), the overlap is the
     * garble. Guard: write the copies on EVERY tab once ONLINE's values are known, so any overlap is identical text.
     * Diag: on ONLINE, log the source and both copies' text once per value (logcat tag radmenu, "FOOTER diag"). */
    if (tab < 0) return;
    if (tab == 0) {
        static char diag_logged[5][128];
        for (int i = 0; i < 3; i++) {   /* the three version fields */
            char src[128], d0[128], d1[128];
            if (canvas_get_text(gs, g_footer[i].src, g_footer[i].element, src, sizeof src) <= 0) continue;
            if (!strcmp(diag_logged[i], src)) continue;
            int r0 = canvas_get_text(gs, g_footer[i].dst[0], g_footer[i].element, d0, sizeof d0);
            int r1 = canvas_get_text(gs, g_footer[i].dst[1], g_footer[i].element, d1, sizeof d1);
            rlog("FOOTER diag ONLINE el %d: src \"%s\" (%zu B) | help copy %d \"%s\" | settings copy %d \"%s\"", i, src,
                 strlen(src), r0, r0 > 0 ? d0 : "", r1, r1 > 0 ? d1 : "");
            snprintf(diag_logged[i], sizeof diag_logged[i], "%s", src);
        }
    }
    for (int i = 0; i < 5; i++) {
        if (!g_footer_val[i][0]) continue;
        int ok = canvas_text(gs, g_footer[i].dst[0], g_footer[i].element, g_footer_val[i])
               + canvas_text(gs, g_footer[i].dst[1], g_footer[i].element, g_footer_val[i]);
        if (strcmp(g_footer_logged[i], g_footer_val[i])) {
            rlog("FOOTER el %d: \"%s\" -> HELP / SETTINGS copies (%d of 2 written, tab %d)", i, g_footer_val[i], ok, tab);
            snprintf(g_footer_logged[i], sizeof g_footer_logged[i], "%s", g_footer_val[i]);
        }
    }
}

/* ---- state from the page events (core hook, any thread) ---- */
static volatile int g_p24_on, g_p5_state = -1, g_p30_on, g_p58_on, g_reset, g_jump = -1, g_fr_press = -1;
static volatile int g_ptr_mask;      /* pointer on: bits 0-3 ray news/dot0..2, bits 4-7 hover */
static volatile int g_safe_ptr;      /* pointer on the SAFE ZONE card (bit 0 ray, bit 1 hover) */
static volatile int g_shown;         /* the slide the tile shows now */
static volatile int g_go_p42, g_go_p48, g_go_season;   /* season: 1 = send next tick, 2 = p24 shown, send now */
static volatile int g_p5_have;
static uint64_t g_hs_ev[5], g_p5_ev[5];
static uint32_t g_p5_flags;
static int g_slide, g_active, g_tab = -2, g_paused, g_text_fail;
static const void *g_news_tex, *g_last_set;   /* the news tile's own texture (NULL = its own sprite), our last write */
static int g_news_known;
static uint64_t g_next, g_reassert, g_title_next, g_color_next;
/* tree gate: 1 = the minted marker ffa_news_dot0 is in the menu gamespace (our menu tree is loaded). Set by the tick
 * at every page-24 enable and on a gamespace change; read by the target hook (any thread) and the friends worker.
 * 0 = the stock menu: no write to any canvas, no click rewrite, no /friends poll (page tracking only). */
static volatile int g_tree, g_fr_tree;
static uintptr_t g_tree_gs;

static uint64_t actor_name(uintptr_t gs, uint64_t sactor)
{
    t_fault = 0;
    uintptr_t ad = R64(gs + g_prof->o_gs_actors);
    uint64_t idx = sactor & 0xffff, n = ad ? R64(ad + g_prof->o_at_count) : 0;
    uintptr_t names = ad ? R64(ad + g_prof->o_at_names) : 0;
    uint64_t v = names && idx < n ? R64(names + idx * 8) : 0;
    return t_fault ? 0 : v;
}

#define NFRIEND 5
#define ACTOR_FRIEND0 0xecce8089fb0c0501ull
static const uint64_t EV_FRIEND[NFRIEND] = { 0xe2e33c85cad96ff8ull, 0x9f0856702361da46ull, 0xb45170237bf6492cull,
                                             0x64de839bf010b13aull, 0x4f87a5c8a8872250ull };

/* bit of the news tile / dot i in the pointer mask, or -1 */
static int ptr_bit(uint64_t actor)
{
    if (actor == ACTOR_NEWS) return 0;
    for (int i = 0; i < NSLIDE; i++)
        if (actor == ACTOR_DOT[i]) return 1 + i;
    return -1;
}

/* ---- FTUE dim + combat lock (PC parity). Only with the tree marker; ftue=off / combat_lock=off write
 * nothing. FindPrimitiveID logs an engine error for a missing element, so every element is looked up once first. ---- */
static volatile int g_ftue_src;    /* the home picker row that enabled at this menu load: 25 / 26 / 27, 0 none */
static volatile int g_pick_on;     /* bits 0-2: p25 / p26 / p27 enabled now */
static volatile int g_lock_ptr;    /* pointer on CREATE (bit 0 ray, 4 hover) / JOIN (bit 1, 5) / PLAY (bit 2, 6) */
static volatile int g_back24;      /* FTUE: p34 / p36 opened anyway: show p24 again */
static volatile int g_p36_go;      /* p36 enabled: apply the combat lock face */
static volatile int g_ftue;
static int g_ftue_pass, g_cl_pass;
static uint64_t g_ftue_next, g_cl_next;
static const float GREY[3] = { 0.35f, 0.35f, 0.35f };   /* SetColor is RGB only: "dim" = grey */

static int elem_has(uintptr_t gs, uint64_t actor, uint64_t element)
{
    Handle c;
    void *inst = NULL;
    if (!canvas_of(gs, actor, &c)) return 0;
    uint32_t id = ((FindPrimFn)(g_base + g_prof->va_find_prim))(c.cs, c.idx, element, &inst);
    return id != 0xffffffffu && inst != NULL;
}
static int grey(uintptr_t gs, uint64_t actor, uint64_t el) { return elem_has(gs, actor, el) && canvas_color(gs, actor, el, GREY); }
static int show_el(uintptr_t gs, uint64_t actor, uint64_t el) { return elem_has(gs, actor, el) && canvas_show(gs, actor, el, 1); }

static int lock_face(uintptr_t gs, uint64_t card, const char *msg, int sprite)
{
    int n = grey(gs, card, EL_CARDIMAGE) + grey(gs, card, EL_CARDTEXT) + grey(gs, card, EL_CARDSUB)
          + show_el(gs, card, EL_LOCKTEXT);
    if (sprite) n += show_el(gs, card, EL_LOCKSPRITE);
    return n + (elem_has(gs, card, EL_LOCKTEXT) && canvas_text(gs, card, EL_LOCKTEXT, msg));
}

static int ftue_dim(uintptr_t gs)
{
    return lock_face(gs, CARD_CREATE, FTUE_TEXT, 0) + lock_face(gs, CARD_JOIN, FTUE_TEXT, 0)
         + grey(gs, ROW_MYCODE, SYM_CODETEXT) + grey(gs, ROW_MYCODE, SYM_TEXT) + grey(gs, ROW_LINK, SYM_TEXT)
         + grey(gs, ROW_FRIENDS, SYM_TEXT);
}

static void ftue_tick(uint8_t *game, uintptr_t gs, uint64_t now, int reset)
{
    if (g_ftue_mode && !g_ftue && (g_ftue_src || g_ftue_mode == 2)) {
        int src = __atomic_load_n(&g_ftue_src, __ATOMIC_ACQUIRE);
        __atomic_store_n(&g_ftue, 1, __ATOMIC_RELEASE);
        if (src) rlog("FTUE on (picker page p%d): page 24 %s", src, show_page(game, P24_HOME) ? "not found" : "enabled");
        else rlog("FTUE on (ftue=force)");
        g_ftue_pass = 3;
        g_ftue_next = now;
    }
    if (!g_ftue) return;
    if (reset) { g_ftue_pass = 3; g_ftue_next = now; }
    if (__atomic_exchange_n(&g_back24, 0, __ATOMIC_ACQ_REL))
        rlog("FTUE: a locked page opened anyway: page 24 %s", show_page(game, P24_HOME) ? "not found" : "enabled");
    if (g_ftue_pass > 0 && g_p24_on && now >= g_ftue_next) {
        int n = ftue_dim(gs);
        if (g_ftue_pass == 3)
            rlog("FTUE dim: %d writes (CREATE / JOIN grey + lock text, invite row grey)%s", n,
                 g_pick_on ? "; picker page still enabled (no DisablePage pin)" : "");
        g_ftue_pass--;
        g_ftue_next = now + 1000;
    }
}

static void combat_lock_tick(uintptr_t gs, uint64_t now)
{
    static const char *const msg[3] = { "", "Requires Combat Purchase", "Requires Oculus Link or Air Link" };
    if (!g_combat_lock) return;
    if (__atomic_exchange_n(&g_p36_go, 0, __ATOMIC_ACQ_REL)) { g_cl_pass = 3; g_cl_next = now; }   /* p36 may UnlockCard on enable */
    if (g_cl_pass > 0 && now >= g_cl_next) {
        int n = lock_face(gs, CARD_COMBAT, msg[g_combat_lock], 1);
        if (g_cl_pass == 3) rlog("COMBAT LOCK (%s): p36 combat card face, %d writes", g_cl_word[g_combat_lock], n);
        g_cl_pass--;
        g_cl_next = now + 1000;
    }
}

/* ---- lockout card lock (PC parity; recipe). Tree marker only;
 * lockout_lock=off writes nothing. State (auto): ng = [game+0x8510] (the session reader's pointer), expiry i64 at
 * ng+0x64828, flags u8 at ng+0x64847 (disasm-only @ 6528386917200980, sns early-quit-wire-and-pc-runtime.md:
 * offsets equal PC's). Locked = flags bit 1 && expiry > now; a stock libr15.so never locks. Every element is
 * looked up first (FindPrimitiveID logs an engine error for a missing one). ---- */
#define CARD_PLAY    0xa3d3ef315cb999dbull   /* home rec-27 row 13 play_card */
#define P29_NOTICE   0x9344b604478e3cbdull   /* mnu_master p29 quit_notice_page (page-table, PC = Quest) */
#define P49_BEGIN_MP 0x2fe7102931d1c4e0ull   /* p49 begin_multiplayer_page: PLAY's stock target */
#define LO_NOTICE_ACTOR 0x600058d5720197fcull /* p29's element owner (click-table; canvas = quit_notice, inferred) */
#define EL_LO_BODY   0xa2ff5667e38ab50dull   /* lockoutnoticebodytext1 */
#define EL_QN_BODY   0xd2c8dda0c5d553a4ull   /* quitnoticebodytext */
#define EL_LO_TIMER  0xcf9363cdc1963db6ull   /* lockouttimertext */
#define EL_LO_ICON   0x7a1ceaf22bcb77c6ull   /* lockouticon */
#define LO_NOTICE_LINE "You quit matches early. Play unlocks when the timer ends."
static const struct { uint32_t va; uint8_t s[4], p[4]; } LO_SITE[3] = {   /* scripts/quest_libr15_earlyquit_patch.py */
    { 0x126ac3c, { 0x08, 0xe9, 0x02, 0x12 }, { 0x08, 0x1d, 0x00, 0x12 } },   /* a  */
    { 0x126acb0, { 0x02, 0x00, 0x80, 0x92 }, { 0xe2, 0x03, 0x03, 0xaa } },   /* b2 */
    { 0x2321cf0, { 0x7f, 0x02, 0x00, 0x39 }, { 0x69, 0x02, 0x00, 0x39 } } }; /* c3 */
static const uint64_t LO_CARD[3] = { CARD_PLAY, CARD_CREATE, CARD_JOIN };
static const float WHITE[3] = { 1.0f, 1.0f, 1.0f }, CARD_SUB_RGB[3] = { 0.812f, 0.812f, 0.812f };
static volatile int g_lo_locked, g_lo_go, g_lo_back, g_p29_on, g_lo_ours;
static volatile uint64_t g_lo_p29_press;
static int g_lo_src = -1, g_lo_dimmed, g_lo_restore, g_lo_logged_dim, g_lo_logged_note;
static long long g_lo_force_end, g_lo_last_rem = -2;
static uint64_t g_lo_next;

static long long lo_remaining(uint8_t *game)
{
    long long now = (long long)time(NULL);
    if (g_lo_mode == 2) {
        if (!g_lo_force_end) g_lo_force_end = now + g_lo_force_sec;
        return g_lo_force_end > now ? g_lo_force_end - now : 0;
    }
    if (g_lo_src < 0) {
        int st[3];
        for (int i = 0; i < 3; i++) {
            uint8_t got[4];
            st[i] = safe_copy(got, U(g_base) + LO_SITE[i].va, 4) ? -1 : !memcmp(got, LO_SITE[i].s, 4) ? 0
                  : !memcmp(got, LO_SITE[i].p, 4) ? 1 : -1;
        }
        g_lo_src = (st[0] < 0 || st[1] < 0 || st[2] < 0) ? 0 : (st[0] && st[1] && st[2]) ? 2 : 1;
        rlog("LOCKOUT source: net game +0x64828 / +0x64847; libr15 sites a=%d b2=%d c3=%d (0 stock, 1 patched, -1 other): %s",
             st[0], st[1], st[2], g_lo_src == 0 ? "unverified build, lockout_lock=auto never locks" : g_lo_src == 2
             ? "early-quit patch on" : "stock or partly patched library (a stock one never shows a lockout)");
    }
    if (g_lo_src <= 0) return 0;
    uint64_t ng = 0;
    long long exp = -1;
    uint8_t fl = 0;
    if (safe_copy(&ng, U(game) + g_prof->o_game_netgame, 8) || !ng || safe_copy(&exp, ng + 0x64828, 8)
        || safe_copy(&fl, ng + 0x64847, 1))
        return 0;
    long long rem = (fl & 0x02) && exp != -1 && exp > now ? exp - now : 0;
    if (rem > 366LL * 86400) rem = 0;
    if (g_lo_last_rem == -2 || (rem > 0) != (g_lo_last_rem > 0))
        rlog("LOCKOUT state: %s (flags 0x%02x, expiry %lld, now %lld)", rem > 0 ? "locked" : "none", fl, exp, now);
    return rem;
}

static void lo_hms(char *out, size_t cap, long long s)
{
    if (s < 0) s = 0;
    snprintf(out, cap, "%02lld:%02lld:%02lld", s / 3600, (s / 60) % 60, s % 60);
}

static int set_rgb(uintptr_t gs, uint64_t a, uint64_t el, const float *rgb) { return elem_has(gs, a, el) && canvas_color(gs, a, el, rgb); }
static int hide_el(uintptr_t gs, uint64_t a, uint64_t el) { return elem_has(gs, a, el) && canvas_show(gs, a, el, 0); }
static int set_txt(uintptr_t gs, uint64_t a, uint64_t el, const char *s) { return elem_has(gs, a, el) && canvas_text(gs, a, el, s); }

static int lo_restore_cards(uintptr_t gs)
{
    int n = 0;
    for (int i = 0; i < 3; i++) {
        n += hide_el(gs, LO_CARD[i], EL_LOCKSPRITE);
        if (g_ftue && i) continue;   /* CREATE / JOIN keep the FTUE face (re-applied below) */
        n += set_rgb(gs, LO_CARD[i], EL_CARDIMAGE, WHITE) + set_rgb(gs, LO_CARD[i], EL_CARDTEXT, WHITE)
           + set_rgb(gs, LO_CARD[i], EL_CARDSUB, CARD_SUB_RGB) + hide_el(gs, LO_CARD[i], EL_LOCKTEXT)
           + set_txt(gs, LO_CARD[i], EL_LOCKTEXT, "Requires Oculus Link or Air Link");
    }
    if (g_ftue) { g_ftue_pass = 3; g_ftue_next = 0; }
    return n;
}

static int lo_notice_face(uintptr_t gs, const char *hms)
{
    /* the stock body is two sentences on one unwrapped line (clipped both ends) and the timer starts hidden */
    return show_el(gs, LO_NOTICE_ACTOR, EL_LO_BODY) + hide_el(gs, LO_NOTICE_ACTOR, EL_QN_BODY)
         + set_txt(gs, LO_NOTICE_ACTOR, EL_LO_BODY, LO_NOTICE_LINE)
         + show_el(gs, LO_NOTICE_ACTOR, EL_LO_ICON) + show_el(gs, LO_NOTICE_ACTOR, EL_LO_TIMER)
         + set_txt(gs, LO_NOTICE_ACTOR, EL_LO_TIMER, hms)
         + canvas_text(gs, ACTOR_BANNER, SYM_TEXT, "LOCKOUT NOTICE");
}

static void lockout_tick(uint8_t *game, uintptr_t gs, uint64_t now, int reset)
{
    if (!g_lo_mode) return;
    if (reset) { g_lo_next = 0; g_lo_logged_dim = 0; }
    int go = __atomic_exchange_n(&g_lo_go, 0, __ATOMIC_ACQ_REL), back = __atomic_exchange_n(&g_lo_back, 0, __ATOMIC_ACQ_REL);
    uint64_t pt = __atomic_load_n(&g_lo_p29_press, __ATOMIC_ACQUIRE);
    if (pt && now >= pt + 700) {   /* p29's own buttons did not leave the page: back to home */
        __atomic_store_n(&g_lo_p29_press, 0, __ATOMIC_RELEASE);
        if (g_lo_ours && g_p29_on)
            rlog("LOCKOUT NOTICE: page 29 still up 0.7 s after a press: page 24 %s", show_page(game, P24_HOME) ? "not found" : "enabled");
    }
    if (!go && !back && now < g_lo_next) return;
    long long rem = lo_remaining(game);
    char hms[16], msg[64];
    lo_hms(hms, sizeof hms, rem);
    __atomic_store_n(&g_lo_locked, rem > 0, __ATOMIC_RELEASE);
    if (rem > 0) {
        if (go || back) {
            int r = show_page(game, P29_NOTICE);
            rlog("LOCKOUT NOTICE (%s): page 29 %s, %s left", go ? "card press" : "a locked page opened anyway",
                 r ? "not found" : "enabled", hms);
            __atomic_store_n(&g_lo_ours, !r, __ATOMIC_RELEASE);
            g_lo_logged_note = 0;
            __atomic_store_n(&g_lo_p29_press, 0, __ATOMIC_RELEASE);
        }
        if (g_p24_on) {
            int n = 0;
            snprintf(msg, sizeof msg, "%s%s%s", g_lo_prefix, g_lo_prefix[0] ? " " : "", hms);
            for (int i = 0; i < 3; i++) n += lock_face(gs, LO_CARD[i], msg, 1);
            if (!g_lo_logged_dim) rlog("LOCKOUT dim (%s): %d writes (PLAY / CREATE / JOIN grey + lock, \"%s\")",
                                       g_lo_word[g_lo_mode], n, msg);
            g_lo_logged_dim = g_lo_dimmed = 1;
        }
        if (g_p29_on) {
            int n = lo_notice_face(gs, hms);
            if (!g_lo_logged_note) rlog("LOCKOUT NOTICE face: %d writes (lockout body, timer %s, banner)", n, hms);
            g_lo_logged_note = 1;
        }
    } else {
        if (g_lo_last_rem > 0 || g_lo_dimmed) {
            if (g_lo_last_rem > 0) rlog("LOCKOUT over (%s): restoring", g_lo_word[g_lo_mode]);
            g_lo_restore = 3;
            g_lo_dimmed = 0;
            if (g_lo_ours && g_p29_on)
                rlog("LOCKOUT over while the notice was up: page 24 %s", show_page(game, P24_HOME) ? "not found" : "enabled");
            __atomic_store_n(&g_lo_ours, 0, __ATOMIC_RELEASE);
        }
        if (go || back) rlog("LOCKOUT: queued notice dropped (no lockout now)");
        if (g_lo_restore > 0 && g_p24_on) {
            int n = lo_restore_cards(gs);
            if (g_lo_restore == 3) rlog("LOCKOUT restore: %d writes (stock card colours, lock text hidden)", n);
            g_lo_restore--;
        }
    }
    g_lo_last_rem = rem;
    g_lo_next = now + 1000;
}

static void pointer_event(void *gs, const uint64_t *ev)
{
    uint64_t who = actor_name(U(gs), ev[2]);
    int on = ev[0] == EV_PTR_START || ev[0] == EV_PTR_HOVER, hov = ev[0] == EV_PTR_HOVER || ev[0] == EV_PTR_IDLE;
    if (who == ACTOR_SAFE) {
        int sb = hov ? 2 : 1;
        if (on) __atomic_or_fetch(&g_safe_ptr, sb, __ATOMIC_ACQ_REL);
        else __atomic_and_fetch(&g_safe_ptr, ~sb, __ATOMIC_ACQ_REL);
        return;
    }
    if (who == CARD_CREATE || who == CARD_JOIN || who == CARD_PLAY) {   /* FTUE /: which card a press is from */
        int cb = (who == CARD_CREATE ? 1 : who == CARD_JOIN ? 2 : 4) << (hov ? 4 : 0);
        if (on) __atomic_or_fetch(&g_lock_ptr, cb, __ATOMIC_ACQ_REL);
        else __atomic_and_fetch(&g_lock_ptr, ~cb, __ATOMIC_ACQ_REL);
        return;
    }
    int b = ptr_bit(who);
    if (b < 0) return;
    int bit = 1 << (b + (hov ? 4 : 0));
    if (on) __atomic_or_fetch(&g_ptr_mask, bit, __ATOMIC_ACQ_REL);
    else __atomic_and_fetch(&g_ptr_mask, ~bit, __ATOMIC_ACQ_REL);
}

/* core hook body (SendComponentEventGlobal): ev[0] = event, ev[2] = SActor of the page owner / pointer target */
static int report_event(const uint64_t *ev, int target, void *gs);
static void qw_event(void *gs, const uint64_t *ev);
static void core_body(void *gs, const uint64_t *ev)
{
    uint64_t e0 = ev[0], e2;
    report_event(ev, 0, (void *)(gs));   /* the engine's own SComponentEvent const&: valid */
    qw_event(gs, ev);                    /* QUIT WARNING: p7 on / off, any level */
    if (e0 == EV_PTR_HOVER || e0 == EV_PTR_IDLE || e0 == EV_PTR_START || e0 == EV_PTR_END) { pointer_event(gs, ev); return; }
    for (int i = 0; i < NSLIDE; i++)
        if (e0 == EV_DOT[i]) { __atomic_store_n(&g_jump, i, __ATOMIC_RELEASE); rlog("dot click event %d (global)", i); return; }
    for (int i = 0; i < NFRIEND; i++)
        if (e0 == EV_FRIEND[i]) { __atomic_store_n(&g_fr_press, i, __ATOMIC_RELEASE); rlog("friend row %d click event (global)", i + 1); return; }
    if (e0 == EV_HELP_SEASON) { rlog("HELP SEASON click event (global)"); return; }
    if (e0 == EV_HELP_SAFE) { rlog("HELP SAFE ZONE click event (global)"); return; }
    if (e0 != EV_PAGE_ON && e0 != EV_PAGE_OFF) return;
    e2 = ev[2];
    uint64_t owner = actor_name(U(gs), e2);
    int on = e0 == EV_PAGE_ON;
    if (owner == P24_HOME) {
        __atomic_store_n(&g_p24_on, on, __ATOMIC_RELEASE);
        if (on) __atomic_store_n(&g_reset, 1, __ATOMIC_RELEASE);
        else __atomic_store_n(&g_lock_ptr, 0, __ATOMIC_RELEASE);   /* a disabled element gets no _OnEndIntersection */
    }
    else if (owner == P25_CORE_TUT || owner == P26_MP_TUT || owner == P27_NPE_HOME) {
        int pb = owner == P25_CORE_TUT ? 0 : owner == P26_MP_TUT ? 1 : 2;
        if (on) __atomic_or_fetch(&g_pick_on, 1 << pb, __ATOMIC_ACQ_REL);
        else __atomic_and_fetch(&g_pick_on, ~(1 << pb), __ATOMIC_ACQ_REL);
        if (on && g_ftue_mode == 1) __atomic_store_n(&g_ftue_src, 25 + pb, __ATOMIC_RELEASE);
        rlog("page %d (home picker row) %s", 25 + pb, on ? "enabled" : "disabled");
        return;
    }
    else if (owner == P29_NOTICE) {
        __atomic_store_n(&g_p29_on, on, __ATOMIC_RELEASE);
        if (!on) __atomic_store_n(&g_lo_ours, 0, __ATOMIC_RELEASE);
        if (g_lo_mode) rlog("page 29 (quit / lockout notice) %s", on ? "enabled" : "disabled");
        return;
    }
    else if ((owner == P34_JOIN || owner == P36_EXPER || owner == P49_BEGIN_MP) && on) {
        int tr = __atomic_load_n(&g_tree, __ATOMIC_ACQUIRE);
        if (__atomic_load_n(&g_lo_locked, __ATOMIC_ACQUIRE) && tr) __atomic_store_n(&g_lo_back, 1, __ATOMIC_RELEASE);   /* */
        else if (g_ftue && tr && owner != P49_BEGIN_MP) __atomic_store_n(&g_back24, 1, __ATOMIC_RELEASE);
        if (owner == P36_EXPER && g_combat_lock) __atomic_store_n(&g_p36_go, 1, __ATOMIC_RELEASE);
        return;
    }
    else if (owner == P5_TILES) __atomic_store_n(&g_p5_state, on, __ATOMIC_RELEASE);
    else if (owner == P58_HELP) __atomic_store_n(&g_p58_on, on, __ATOMIC_RELEASE);
    else if (owner == P30_SETTINGS) __atomic_store_n(&g_p30_on, on, __ATOMIC_RELEASE);
    else return;
    rlog("page %016llx %s", ULL(owner), on ? "enabled" : "disabled");
}

static void core_hook(void *gs, const uint64_t *ev, uint32_t flags)
{
    core_body(gs, ev);
    g_core_orig(gs, ev, flags);
}

/* ev[0] rewrite in place (PC's target hook): only when the event's page is writable (/proc/self/maps). */
static int page_perms(uintptr_t addr, char perms[5]);
static int rewrite(const uint64_t *ev, uint64_t sym)
{
    char p[5] = "";
    if (page_perms(U(ev), p) || p[1] != 'w') return 0;
    *(volatile uint64_t *)(uintptr_t)ev = sym;
    return 1;
}

/*: the stock card uid in the press event names the card (PC press log); pointer bits only as a fallback */
static const char *lo_card_name(uint64_t comp, long lp)
{
    if (comp == 0x35ec861316abbb34ull) return "PLAY";
    if (comp == 0x35ec861316abbb32ull) return "CREATE";
    if (comp == 0x35ec861316abbb37ull) return "JOIN";
    return (lp & 0x44) ? "PLAY" : (lp & 0x11) ? "CREATE" : (lp & 0x22) ? "JOIN" : "unknown";
}

/* target hook body (CncaGameSpace::SendComponentEvent, any thread the engine sends from). 1 = ev[0] was rewritten. */
static int target_body(void *gs, const uint64_t *ev, uint32_t flags)
{
    uint64_t e0 = ev[0];
    if (report_event(ev, 1, (void *)(gs))) return 1;
    if (!__atomic_load_n(&g_tree, __ATOMIC_ACQUIRE)) return 0;   /* stock menu: every click goes through untouched */
    int lp = __atomic_load_n(&g_lock_ptr, __ATOMIC_ACQUIRE);
    if (__atomic_load_n(&g_lo_locked, __ATOMIC_ACQUIRE) && g_p24_on && !g_p58_on && e0 == EV_CARD_CLICKED
        && actor_name(U(gs), ev[2]) == P24_HOME) {
        /*: with p24 up and HELP down, a card press to the home owner is PLAY / CREATE / JOIN (TUTORIALS is on p58) */
        const char *what = lo_card_name(ev[3], lp);
        int w = rewrite(ev, SYM_NOOP);
        __atomic_store_n(&g_lo_go, 1, __ATOMIC_RELEASE);
        rlog("LOCKOUT: %s card press (component %016llx) %s; notice queued", what, ULL(ev[3]),
             w ? "consumed" : "not writable (stock route runs; the page redirect catches it)");
        return w;
    }
    if (__atomic_load_n(&g_lo_ours, __ATOMIC_ACQUIRE) && g_p29_on && actor_name(U(gs), ev[2]) == P29_NOTICE) {
        rlog("LOCKOUT NOTICE: event %016llx component %016llx to page 29 (left as sent)", ULL(e0), ULL(ev[3]));
        __atomic_store_n(&g_lo_p29_press, now_ms(), __ATOMIC_RELEASE);
        return 0;
    }
    /* a card press counts by the pointer (lp) or, with no pointer (Quest hand / event), by its component uid to p24 */
    const char *fc = e0 == EV_CARD_CLICKED ? lo_card_name(ev[3], lp & 0x33) : "";
    if (g_ftue && g_p24_on && (e0 == EV_EYE || (e0 == EV_CARD_CLICKED && ((lp & 0x33) || ((!strcmp(fc, "CREATE")
        || !strcmp(fc, "JOIN")) && actor_name(U(gs), ev[2]) == P24_HOME))) || (e0 == SYM_LINK_BTN && ev[3] == UID_MY_LINK))) {
        const char *what = e0 == EV_EYE ? "MY INVITE CODE eye" : e0 == SYM_LINK_BTN ? "INVITE LINK"
                         : !strcmp(fc, "CREATE") ? "CREATE card" : "JOIN card";
        int w = rewrite(ev, SYM_NOOP);
        rlog("FTUE: %s press %s", what, w ? "consumed" : "not writable (stock route runs)");
        return w;
    }
    if (g_combat_lock && e0 == EV_CARD_CLICKED && ev[3] == UID_CARD_R && actor_name(U(gs), ev[2]) == P36_EXPER) {
        int w = rewrite(ev, SYM_NOOP);
        /* TODO store route: a purchase-locked press should enable p6 with the blackboard "hyperlink" set; that needs
         * a blackboard string write pin no recipe has pinned yet. Until then the press is only consumed. */
        rlog("COMBAT LOCK (%s): combat card press %s; no store route (blackboard string write not pinned)",
             g_cl_word[g_combat_lock], w ? "consumed" : "not writable (stock route runs)");
        return w;
    }
    if (e0 == EV_HELP_SAFE || (e0 == EV_CARD_CLICKED && g_help_safe_card && g_safe_ptr && g_p58_on)) {
        int own = e0 == EV_HELP_SAFE, go = own ? g_help_safe : 1;
        rlog("HELP SAFE ZONE pressed (%s to %016llx component=%016llx, pointer %d)%s", own ? "own event" : "evt_card_clicked",
             ULL(actor_name(U(gs), ev[2])), ULL(ev[3]), g_safe_ptr, go ? ": page 48 queued" : ": help_safezone=0, ignored");
        if (go) __atomic_store_n(&g_go_p48, 1, __ATOMIC_RELEASE);
        return 0;
    }
    if (e0 == EV_HELP_SEASON) {   /* no stock handler knows it; the tick forwards it to the season route */
        g_hs_ev[1] = ev[1];
        g_hs_ev[2] = ev[2];
        rlog("HELP SEASON PASS pressed (to %016llx component=%016llx)%s", ULL(actor_name(U(gs), ev[2])), ULL(ev[3]),
             g_help_season ? ": season route queued" : ": help_season=0, ignored");
        if (g_help_season) __atomic_store_n(&g_go_season, 1, __ATOMIC_RELEASE);
        return 0;
    }
    if (e0 != EV_NEWS && e0 != EV_SEASON) return 0;
    uint64_t owner = actor_name(U(gs), ev[2]);
    int k = (g_active && g_p24_on) ? g_shown : 0;
    if (owner == P5_TILES && !g_p5_have) {   /* a real page-5 event: the template for the HELP season send */
        memcpy(g_p5_ev, ev, sizeof g_p5_ev);
        g_p5_flags = flags;
        __atomic_store_n(&g_p5_have, 1, __ATOMIC_RELEASE);
        rlog("page-5 event cached (gs %016llx actor %016llx comp %016llx flags %u)", ULL(ev[1]), ULL(ev[2]), ULL(ev[3]), flags);
    }
    if (e0 == EV_SEASON) { rlog("season click event to %016llx component=%016llx", ULL(owner), ULL(ev[3])); return 0; }
    if (owner != P5_TILES || k == 0) {
        rlog("CLICK news tile at slide %d (owner %016llx): stock news route", k, ULL(owner));
        return 0;
    }
    if (k == 1) {
        int w = rewrite(ev, EV_SEASON);
        rlog("CLICK news tile at slide 1: %s", w ? "REDIRECT -> season click event (stock season route, no page 8)"
                                                 : "event not writable: stock news route runs");
        return w;
    }
    int w = rewrite(ev, SYM_NOOP);
    __atomic_store_n(&g_go_p42, 1, __ATOMIC_RELEASE);
    rlog("CLICK news tile at slide 2: %s; page 42 queued", w ? "news event consumed (no page 8)"
                                                             : "event not writable (page 8 will open too)");
    return w;
}

static void target_hook(void *gs, const uint64_t *ev, uint32_t flags)
{
    uint64_t was = ev[0];
    int w = target_body(gs, ev, flags);
    g_target_orig(gs, ev, flags);
    if (w) rewrite(ev, was);   /* a caller that reuses the event sees its own symbol again */
}

/* HELP SEASON PASS -> EV_SEASON to the page-5 owner (PC season_send): built from the last real page-5 event, or from
 * the press's gs id + the owner's SActor index + its one script component (season_comp). Sent through the word the
 * target GOT held (rad-links' hook or the stock function). Game thread. */
static void season_send(uint8_t *game, uintptr_t gs)
{
    int64_t k = find_actor(gs, P5_TILES);
    if (k < 0) { rlog("HELP SEASON: page-5 owner not found: nothing sent"); __atomic_store_n(&g_go_season, 0, __ATOMIC_RELEASE); return; }
    if (g_season_p24 && g_p5_state == 0 && g_go_season == 1) {
        rlog("HELP SEASON: page 5 off: show page 24 first (%s), send next tick", show_page(game, P24_HOME) ? "page not found" : "enabled");
        __atomic_store_n(&g_go_season, 2, __ATOMIC_RELEASE);
        return;
    }
    __atomic_store_n(&g_go_season, 0, __ATOMIC_RELEASE);
    uint64_t ev[5];
    uint32_t flags = 0;
    if (g_p5_have) { memcpy(ev, g_p5_ev, sizeof ev); flags = g_p5_flags; }
    else { ev[1] = g_hs_ev[1]; ev[2] = (g_hs_ev[2] & ~0xffffull) | ((uint64_t)k & 0xffff); ev[3] = g_season_comp; ev[4] = 0; }
    ev[0] = EV_SEASON;
    if (actor_name(gs, ev[2]) != P5_TILES) { rlog("HELP SEASON: SActor %016llx does not name the page-5 owner: nothing sent", ULL(ev[2])); return; }
    SendCoreFn send = g_target_orig ? g_target_orig : (SendCoreFn)(g_base + g_prof->va_send_target);
    send((void *)gs, ev, flags);
    rlog("HELP SEASON: season click event sent to page-5 owner (%s; gs %016llx actor %016llx comp %016llx flags %u, page 5 %d)",
         g_p5_have ? "cached from a real page-5 event" : "built", ULL(ev[1]), ULL(ev[2]), ULL(ev[3]), flags, g_p5_state);
}

/* ---- carousel + tabs (game thread) ---- */
static const char *const g_dots[NSLIDE] = { "O . .", ". O .", ". . O" };
static void face_text(int k, char *out, size_t cap)
{
    if (g_dots_mode == 1) snprintf(out, cap, "%s  %s", g_title[k], g_dots[k]);
    else if (g_dots_mode == 2) snprintf(out, cap, "%s\n%s", g_title[k], g_dots[k]);
    else snprintf(out, cap, "%s", g_title[k]);
}

/* Puts slide k on the tile. log = 1 on a slide change (else a quiet re-assert). The full SLIDE line goes to the file
 * once per slide per page-24 visit, and again whenever its outcome changes; the steady rotation logs nothing. */
static char g_slide_logged[NSLIDE][192];
static void apply_face(uintptr_t gs, int k, int log)
{
    char text[40];
    face_text(k, text, sizeof text);
    int ok = canvas_text(gs, ACTOR_NEWS, SYM_TEXT, text), boxes = 0, dots_ok = 0;
    const char *img = "unchanged";
    const void *cur = NULL, *season = NULL, *want = NULL, *post = NULL;
    if (g_sprite) {
        cur = sprite_tex(gs, ACTOR_NEWS, SYM_BG);
        season = sprite_tex(gs, ACTOR_SEASON, SYM_BG);
        /* (a): a NULL slot is the tile's own static sprite, not "unknown". Adopt the slot on the first sample, or
         * when the game wrote it after us (a late dyn-load); never adopt the season art. */
        if ((!g_news_known || cur != g_last_set) && (cur == NULL || cur != season)) { g_news_tex = cur; g_news_known = 1; }
        want = k == 1 ? season : g_news_tex;
        if (k == 1 && !season) img = "season texture not found";
        else {
            img = set_tex(gs, ACTOR_NEWS, SYM_BG, want) ? (want ? (k == 1 ? "season" : "news") : "news (cleared)") : "not written";
            g_last_set = want;
        }
    }
    static const float on_rgb[3] = { 1.0f, 0.55f, 0.10f }, off_rgb[3] = { 0.85f, 0.87f, 0.90f };
    if (g_square_dot)
        for (int i = 0; i < NSLIDE; i++) boxes += canvas_color(gs, ACTOR_DOT[i], SYM_DOT_BOX, i == k ? on_rgb : off_rgb);
    for (int i = 0; i < NSLIDE; i++) {   /* square_dot: the glyph elements stay empty; else the glyph marks the slide */
        const char *g = g_square_dot ? "" : g_dot_glyph;
        dots_ok += canvas_text(gs, ACTOR_DOT[i], SYM_DOT_ON, i == k ? g : "");
        dots_ok += canvas_text(gs, ACTOR_DOT[i], SYM_DOT_OFF, i == k ? "" : g);
    }
    __atomic_store_n(&g_shown, k, __ATOMIC_RELEASE);
    if (log) {
        if (g_sprite) post = sprite_tex(gs, ACTOR_NEWS, SYM_BG);   /* the slot after our write */
        char sig[192];
        snprintf(sig, sizeof sig, "text %s, %d dot boxes, %d of %d dot texts; image: %s (wanted %p, news tile tex now %p, "
                 "season tile tex %p)", ok ? "written" : "NOT written", boxes, dots_ok, 2 * NSLIDE, img, want, post, season);
        if (strcmp(sig, g_slide_logged[k])) {
            rlog("SLIDE %d \"%s\": %s", k, text, sig);
            snprintf(g_slide_logged[k], sizeof g_slide_logged[k], "%s", sig);
        }   /* unchanged outcome: silent in both logs (logcat too), so the steady rotation writes nothing */
    } else if (ok != !g_text_fail) rlog("re-assert slide %d: text %s", k, ok ? "written" : "NOT written");
    g_text_fail = !ok;
}

static void apply_tabs(uintptr_t gs, uint64_t now)
{
    int tab = g_p24_on ? 0 : g_p30_on ? 1 : g_p58_on ? 2 : -1;
    if (tab == g_tab && now < g_title_next) return;
    int change = tab != g_tab, title = 0;
    if (tab >= 0 && g_tab_title_on) title = canvas_text(gs, ACTOR_BANNER, SYM_TEXT, g_tab_title[tab]);
    int painted = 0;
    if (g_tab_hl && tab >= 0 && (change || now >= g_color_next)) {
        static const uint64_t rail[4] = { RAIL_ONLINE, RAIL_SETTING, RAIL_HELP, RAIL_QUIT };
        static const float blue[3] = { 0.239f, 0.396f, 0.569f }, slate[3] = { 0.325f, 0.369f, 0.427f };
        for (int i = 0; i < 4; i++) {
            if (g_tab_hl == 2) painted += i == tab ? canvas_show(gs, rail[i], SYM_HIGHLIGHT, 1)
                                                   : (change ? canvas_show(gs, rail[i], SYM_HIGHLIGHT, 0) : 0);
            else painted += canvas_color(gs, rail[i], SYM_BACKGROUND, i == tab ? blue : slate);
        }
        g_color_next = now + 1000;
    }
    if (change) rlog("TAB %d (p24 %d, p30 %d, p58 %d): banner \"%s\" %s, %d rail colours", tab, g_p24_on, g_p30_on,
                     g_p58_on, tab >= 0 ? g_tab_title[tab] : "-",
                     !g_tab_title_on ? "off (tab_title=0)" : title ? "written" : "NOT written", painted);
    g_tab = tab;
    g_title_next = now + 500;
}

/* ---- FRIENDS rows (tree ffa_q1: our minted panel; the stock Quest FRIENDS LIST is a launcher button) ---- */
typedef struct { char name[48], status[16], text[32], session[48], gametype[64]; } Friend;   /* TSV cols 0..4 */
static pthread_mutex_t g_fr_lock = PTHREAD_MUTEX_INITIALIZER;
static Friend g_fr[NFRIEND];
static int g_fr_n, g_fr_have, g_fr_seq, g_fr_applied = -1;
static char g_fr_written[NFRIEND][96];
static uint64_t g_fr_flash[NFRIEND], g_fr_reassert;
static const char *g_fr_flash_msg[NFRIEND];   /* the 3 s status text: NOTHING TO JOIN or JOINING */

/* "See this friend": rad-links' exported radlinks_follow(guid, gametype) (0 = queued; rad-links joins on its own
 * tick). Resolved per press (dlopen RTLD_NOLOAD of the loaded library, then the global scope), so load order does not
 * matter; none when rad-links is not loaded or predates the export. */
typedef int (*FollowFn)(const char *guid, const char *gametype);
static char g_rl_path[256];
static int rl_cb(struct dl_phdr_info *info, size_t size, void *data)
{
    (void)size; (void)data;
    const char *n = info->dlpi_name, *b = n ? strrchr(n, '/') : NULL;
    b = b ? b + 1 : n;
    /* libradlinks.so, or the loader's plugin name libevrmod_radlinks*.so */
    if (b && strstr(b, "radlinks") && strlen(n) < sizeof g_rl_path) { snprintf(g_rl_path, sizeof g_rl_path, "%s", n); return 1; }
    return 0;
}
static int follow_friend(const Friend *f, int *rc)
{
    g_rl_path[0] = 0;
    dl_iterate_phdr(rl_cb, NULL);
    void *h = g_rl_path[0] ? dlopen(g_rl_path, RTLD_NOW | RTLD_NOLOAD) : NULL;
    FollowFn fn = (FollowFn)(h ? dlsym(h, "radlinks_follow") : NULL);
    if (!fn) fn = (FollowFn)dlsym(RTLD_DEFAULT, "radlinks_follow");
    *rc = fn ? fn(f->session, f->gametype) : -3;
    if (h) dlclose(h);
    return fn != NULL;
}
static const uint64_t FR_SLAB_EL[3] = { 0x88d3c40b6cbbeb5eull, 0x4eca5fb423c99908ull, 0x4fab74ca1111beb2ull };
static int g_fr_shown[NFRIEND] = { -1, -1, -1, -1, -1 };
static pthread_mutex_t g_user_lock = PTHREAD_MUTEX_INITIALIZER;
static char g_user[64];
static int fr_online(const Friend *f) { return strcmp(f->status, "offline") != 0; }

static void refresh_user(uint8_t *game)
{
    static const char *prefix[] = { "STM", "PSN", "XBX", "OVR-ORG", "OVR", "BOT", "DMO" };
    t_fault = 0;
    uintptr_t ng = R64(U(game) + g_prof->o_game_netgame);
    if (t_fault || !ng || (int)R32(ng) < NG_LOGGED_IN || t_fault) return;
    SNSUserID u = ((XPlatFn)(g_base + g_prof->va_xplatform_id))((void *)ng);
    unsigned t = (unsigned)(u.type & 0xf);
    if (!u.id || t < 1 || t > 7) return;
    char id[64];
    snprintf(id, sizeof id, "%s-%llu", prefix[t - 1], ULL(u.id));
    pthread_mutex_lock(&g_user_lock);
    int changed = strcmp(id, g_user) != 0;
    if (changed) snprintf(g_user, sizeof g_user, "%s", id);
    pthread_mutex_unlock(&g_user_lock);
    if (changed) rlog("player id: %s-<n> (type %u)", prefix[t - 1], t);   /* no ids in the log */
}

static void ini_user(void)
{
    if (!g_ini_user[0]) return;
    pthread_mutex_lock(&g_user_lock);
    snprintf(g_user, sizeof g_user, "%s", g_ini_user);
    pthread_mutex_unlock(&g_user_lock);
}

static void apply_friends(uintptr_t gs, uint64_t now)
{
    int press = __atomic_exchange_n(&g_fr_press, -1, __ATOMIC_ACQ_REL);
    Friend fr[NFRIEND];
    int n, have, seq;
    pthread_mutex_lock(&g_fr_lock);
    memcpy(fr, g_fr, sizeof fr);
    n = g_fr_n; have = g_fr_have; seq = g_fr_seq;
    pthread_mutex_unlock(&g_fr_lock);
    if (press >= n && press < NFRIEND) {   /* an empty row (its slab hidden, or the NO FRIENDS ONLINE note): no flash */
        rlog("FRIEND row %d pressed: empty row: ignored", press + 1);
        press = -1;
    }
    if (press >= 0 && press < NFRIEND) {
        /* a row with a joinable session joins it through rad-links and shows JOINING for 3 s; else NOTHING TO JOIN */
        const char *msg = "NOTHING TO JOIN";
        int rc = 0;
        if (!fr_online(&fr[press]) || !fr[press].session[0])
            rlog("FRIEND row %d pressed: %s session=- -> NOTHING TO JOIN", press + 1, fr[press].status);
        else if (!follow_friend(&fr[press], &rc))
            rlog("FRIEND row %d pressed: %s -> NOTHING TO JOIN (libradlinks.so or its radlinks_follow export not loaded)",
                 press + 1, fr[press].status);
        else if (rc) rlog("FRIEND row %d pressed: %s -> NOTHING TO JOIN (radlinks_follow refused: %d)", press + 1, fr[press].status, rc);
        else {
            msg = "JOINING";
            rlog("FRIEND row %d pressed: %s gametype=%s -> JOINING (radlinks_follow queued)", press + 1, fr[press].status,
                 fr[press].gametype[0] ? fr[press].gametype : "-1");   /* no session id in the log */
        }
        g_fr_flash_msg[press] = msg;
        g_fr_flash[press] = now + 3000;
        g_fr_reassert = 0;
    }
    if (!have) return;
    int fresh = seq != g_fr_applied;
    for (int i = 0; i < NFRIEND; i++)
        if (g_fr_flash[i] && now >= g_fr_flash[i]) { g_fr_flash[i] = 0; fresh = 1; }
    if (!fresh && press < 0 && now < g_fr_reassert) return;
    g_fr_applied = seq;
    g_fr_reassert = now + 2000;
    for (int i = 0; i < NFRIEND; i++) {
        uint64_t actor = ACTOR_FRIEND0 + (uint64_t)i;
        const char *name = "", *on = "", *off = "", *st = "empty";
        if (i < n) {
            name = fr[i].name;
            st = fr[i].status;
            if (fr_online(&fr[i])) on = fr[i].text; else off = fr[i].text;
        } else if (n == 0 && i == 0) {
            name = "NO FRIENDS ONLINE";
            on = "INVITE FRIENDS BELOW";
            st = "none";
        }
        if (g_fr_flash[i]) {
            const char *m = g_fr_flash_msg[i] ? g_fr_flash_msg[i] : "NOTHING TO JOIN";
            if (i < n && !fr_online(&fr[i])) { off = m; on = ""; }
            else { on = m; off = ""; }
        }
        if (g_hide_empty) {
            int show = !(i >= n && !(n == 0 && i == 0)), w = 0;
            for (int e = 0; e < 3; e++) w += canvas_show(gs, actor, FR_SLAB_EL[e], (uint32_t)show);
            if (show != g_fr_shown[i]) {
                rlog("friends: row %d slab %s (%d of 3 element writes)", i + 1, show ? "shown" : "HIDDEN (empty row)", w);
                g_fr_shown[i] = show;
            }
        }
        int ok = canvas_text(gs, actor, SYM_TEXT, name);
        ok &= canvas_text(gs, actor, SYM_CODETEXT, on);
        ok &= canvas_text(gs, actor, SYM_CODETEXT_OFF, off);
        char state[96];
        snprintf(state, sizeof state, "%s|%s|%s%s", name, st, on[0] ? on : off, ok ? "" : "|x");
        if (strcmp(state, g_fr_written[i])) {
            /* no display names in the log: a 16-bit tag tells rows apart (the placeholder row prints as is) */
            char tag[24];
            if (i < n) snprintf(tag, sizeof tag, "name#%04x", (unsigned)(csym64(name) & 0xffff));
            else snprintf(tag, sizeof tag, "%s", name[0] ? "placeholder" : "-");
            rlog("friends: row %d %s %s \"%s\" %s", i + 1, tag, st, on[0] ? on : off, ok ? "written" : "NOT written");
            snprintf(g_fr_written[i], sizeof g_fr_written[i], "%s", state);
        }
    }
}

/* ---- services host: asked by the worker, answered on the game thread (radlinks_quest.c) ---- */
static char g_svc_url[256], g_svc_src[96], g_svc_host[128], g_svc_port[8];
static int g_svc_req, g_svc_state;   /* req: 0 idle, 1 asked, 4 answering, 2 answered, 3 gave up */
static int g_svc_done;      /* worker: g_svc_url is final (the tick may compare against it) */
static int g_svc_changed;   /* tick: the host differs from g_svc_url at a page-24 enable: resolve again, reset backoff */

/* The services URL the game uses now (the config's apiservice_host, else the libr15.so literal). Game thread. */
static int services_url(uint8_t *g, char *out, size_t cap)
{
    const char *lit = (const char *)(U(g_base) + g_prof->va_api_url), *s = NULL;
    t_fault = 0;
    uintptr_t ng = R64(U(g) + g_prof->o_game_netgame);
    if (ng && !t_fault)
        s = ((JsonTStringFn)(U(g_base) + g_prof->va_json_tstring))((const void *)(ng + g_prof->o_ng_config),
                                                                   "apiservice_host", lit, 0);
    int from_cfg = s && s != lit && *s;
    snprintf(out, cap, "%s", from_cfg ? s : lit);
    return from_cfg;
}

static void services_answer(uint8_t *g)
{
    int from_cfg = services_url(g, g_svc_url, sizeof g_svc_url);
    snprintf(g_svc_src, sizeof g_svc_src, "%s", from_cfg ? "the config's apiservice_host" : "the libr15.so API literal");
    __atomic_store_n(&g_svc_req, 2, __ATOMIC_RELEASE);
}

/* At a page-24 enable: a changed services host lifts the /friends backoff (the worker resolves it again). */
static void services_recheck(uint8_t *g)
{
    if (!__atomic_load_n(&g_svc_done, __ATOMIC_ACQUIRE)) return;
    char url[sizeof g_svc_url];
    services_url(g, url, sizeof url);
    if (!strcmp(url, g_svc_url)) return;
    __atomic_store_n(&g_svc_done, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&g_svc_changed, 1, __ATOMIC_RELEASE);
}

static int services_ready(void)
{
    if (g_svc_state) return g_svc_state > 0;
    int idle = 0;
    __atomic_compare_exchange_n(&g_svc_req, &idle, 1, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
    uint64_t t0 = now_ms();
    while (__atomic_load_n(&g_svc_req, __ATOMIC_ACQUIRE) != 2 && now_ms() - t0 < 3000) sleep_ms(20);
    int asked = 1;   /* give up only if the tick has not started answering (then the literal); else wait for it */
    if (__atomic_compare_exchange_n(&g_svc_req, &asked, 3, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        char url[96] = "";
        if (!safe_copy(url, U(g_base) + g_prof->va_api_url, sizeof url - 1)) url[sizeof url - 1] = 0;
        else url[0] = 0;
        snprintf(g_svc_url, sizeof g_svc_url, "%s", url);
        snprintf(g_svc_src, sizeof g_svc_src, "the libr15.so API literal; the game thread did not answer in 3 s");
    } else {
        while (__atomic_load_n(&g_svc_req, __ATOMIC_ACQUIRE) != 2) sleep_ms(5);
    }
    const char *u = g_svc_url;
    g_svc_state = -1;
    __atomic_store_n(&g_svc_done, 1, __ATOMIC_RELEASE);
    if (strncasecmp(u, "http://", 7)) { rlog("friends: services UNUSABLE: %.120s (from %s); plain http:// only", u, g_svc_src); return 0; }
    u += 7;
    size_t hl = strcspn(u, ":/");
    if (!hl || hl >= sizeof g_svc_host) { rlog("friends: services UNUSABLE: bad host (from %s)", g_svc_src); return 0; }
    memcpy(g_svc_host, u, hl);
    g_svc_host[hl] = 0;
    int port = 80;
    if (u[hl] == ':' && (sscanf(u + hl + 1, "%d", &port) != 1 || port <= 0 || port > 65535)) { rlog("friends: services UNUSABLE: bad port"); return 0; }
    snprintf(g_svc_port, sizeof g_svc_port, "%d", port);
    g_svc_state = 1;
    rlog("friends: services %s:%s (from %s)", g_svc_host, g_svc_port, g_svc_src);
    return 1;
}

/* One GET (HTTP/1.0, Connection: close, 5 s timeouts); HTTP status (0 = transport error), body in out. Worker only. */
static int http_get(const char *path, char *out, size_t cap)
{
    out[0] = 0;
    if (!services_ready()) { snprintf(out, cap, "no usable services host"); return 0; }
    struct addrinfo hints = { 0 }, *res = NULL, *ai;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int gai = getaddrinfo(g_svc_host, g_svc_port, &hints, &res);
    if (gai) { snprintf(out, cap, "resolve %s: %s", g_svc_host, gai_strerror(gai)); return 0; }
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
    if (s < 0) { snprintf(out, cap, "connect %s:%s errno %d", g_svc_host, g_svc_port, err); return 0; }
    char req[512];
    int n = snprintf(req, sizeof req, "GET %s HTTP/1.0\r\nHost: %s:%s\r\nConnection: close\r\n\r\n", path, g_svc_host, g_svc_port);
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

static void tsv_field(const char *ln, int col, char *out, size_t cap, int up)
{
    for (; col > 0 && ln; col--) { ln = strchr(ln, '\t'); if (ln) ln++; }
    size_t n = 0;
    for (; ln && *ln && *ln != '\t' && *ln != '\n' && *ln != '\r' && n + 1 < cap; ln++)
        out[n++] = (char)(up && *ln >= 'a' && *ln <= 'z' ? *ln - 32 : (*ln < 0x20 || *ln > 0x7e) ? '?' : *ln);
    out[n] = 0;
}

/* One GET /friends; returns the HTTP status (0 = transport error). backoff_s = the wait the worker will apply after
 * this status (0 = the normal period), printed on the status-change line. */
/* ---- Report Player v1 (report_flow.h): the Quest glue ----
 * Script calls (device-observed @ Quest 6528386917200980; VAs ELF-dynsym):
 * CLocalComponentHandle<CScriptCS>::Find 0x1291c2c (16-byte Find template; +0x30 = its component type), CScriptCS::
 * FunctionReserveSlot 0x168f4ac (slot by value), FunctionSetParam 0x168f750, DeferredFunctionCall 0x168f9e8, inside
 * Set/ClearAllInvokeImmediate. Gamespace list: game +0x1f48 (16-byte entries, gs at +8), count
 * +0x1f78, gs->game +0x08, gs id +0x58. Events through the core site (SendComponentEventGlobal 0x180281c). */
#define RP_TABLET      0x6c1f6ff04e070923ull   /* as report_flow.h */
#define RP_VA_FIND_SCRIPT 0x1291c2cu
#define RP_VA_RESERVE     0x168f4acu
#define RP_VA_SETPARAM    0x168f750u
#define RP_VA_DEFERRED    0x168f9e8u
#define RP_O_GS_LIST  0x1f48u
#define RP_O_GS_COUNT 0x1f78u
#define RP_O_GS_GAME  0x08u
#define RP_O_GS_ID    0x58u
static const Prologue RP_CHECKS[5] = {
    { 0x1291c2c, { 0xff,0x83,0x01,0xd1, 0xf8,0x13,0x00,0xf9, 0xf7,0x5b,0x03,0xa9, 0xf5,0x53,0x04,0xa9 }, "find_script" },
    { 0x1291c5c, { 0x08,0xc9,0x00,0x90, 0x08,0xcd,0x1b,0x91, 0x08,0xf1,0x7d,0x92, 0xa9,0x62,0x52,0x39 }, "find_script+0x30" },
    { 0x168f4ac, { 0xff,0xc3,0x00,0xd1, 0xf5,0x53,0x01,0xa9, 0xf3,0x7b,0x02,0xa9, 0x09,0x64,0x40,0xf9 }, "fn_reserve_slot" },
    { 0x168f750, { 0x9f,0x04,0x00,0x31, 0x00,0x03,0x00,0x54, 0x88,0xfc,0x60,0xd3, 0x1f,0x05,0x00,0x31 }, "fn_set_param" },
    { 0x168f9e8, { 0xfc,0x4f,0xbe,0xa9, 0xe9,0x83,0x03,0xd1, 0xfd,0x7b,0x01,0xa9, 0xfd,0x43,0x00,0x91 }, "fn_deferred_call" },
};
#define RP_CARD_EL_DEFAULT "friend_label"
typedef uint64_t (*RpReserveFn)(void *cs, uint32_t idx, uint64_t fn, uint32_t auto_retire, uint32_t quiet);
typedef void (*RpSetParamFn)(void *cs, uint32_t idx, uint64_t param, const void *value, uint64_t memslot);
typedef void (*RpCallFn)(void *cs, const uint8_t *params);
#define RP_XCHG(p, v) __atomic_exchange_n((p), (v), __ATOMIC_ACQ_REL)
#define rp_log rlog
#define rp_actor(gs, a) actor_name(U(gs), (a))
static uint64_t rp_sym(const char *s) { return csym64(s); }
static int rp_rewrite(const uint64_t *ev, uint64_t sym) { return rewrite(ev, sym); }

/* the gamespace holding `actor` and its actor index, or 0 (game thread: find_actor's table) */
static uintptr_t rp_actor_gs(void *game, uint64_t actor, int64_t *idx)
{
    t_fault = 0;
    uintptr_t list = R64(U(game) + RP_O_GS_LIST);
    uint64_t n = R64(U(game) + RP_O_GS_COUNT);
    if (t_fault || !list || n > 64) return 0;
    for (uint64_t i = 0; i < n; i++) {
        uintptr_t gs = R64(list + i * 16 + 8);
        if (t_fault) return 0;
        if (!gs || R64(gs + RP_O_GS_GAME) != U(game) || t_fault) { t_fault = 0; continue; }
        int64_t k = find_actor(gs, actor);
        if (k >= 0) { *idx = k; return gs; }
    }
    return 0;
}

/* a script function call on `comp` of `actor`, the way a script's external-function node calls it */
static int rp_call_on(void *game, uint64_t actor, uint64_t comp, uint64_t fn, int n, const uint64_t *pname, const void *const *pval)
{
    int64_t k;
    uintptr_t gs = rp_actor_gs(game, actor, &k);
    if (!gs) return -1;
    Handle h;
    memset(&h, 0, sizeof h);
    ((FindFn)(g_base + RP_VA_FIND_SCRIPT))(&h, (void *)gs, 0x0000ffffffff0000ull | (uint64_t)k, comp);
    if (!h.cs || !((ValidFn)(*(void ***)h.cs)[g_prof->vt_cs_valid / 8])(h.cs, h.idx)) return -1;
    ((GameFn)(g_base + g_prof->va_invoke_set))(game);
    uint64_t ms = ((RpReserveFn)(g_base + RP_VA_RESERVE))(h.cs, h.idx, fn, 1, 0);
    int rc = 1;
    if (ms != ~0ull && ms >> 32 != 0xffffffffull) {
        for (int i = 0; i < n; i++) ((RpSetParamFn)(g_base + RP_VA_SETPARAM))(h.cs, h.idx, pname[i], pval[i], ms);
        uint8_t p[0x38];
        memset(p, 0xff, sizeof p);
        memcpy(p, &h.idx, 4);
        memset(p + 4, 0, 4);
        memcpy(p + 8, &ms, 8);
        memset(p + 0x26, 0, 2);
        ((RpCallFn)(g_base + RP_VA_DEFERRED))(h.cs, p);
        rc = 0;
    }
    ((GameFn)(g_base + g_prof->va_invoke_clear))(game);
    return rc;
}

static int rp_call(void *game, uint64_t fn, int n, const uint64_t *pname, const void *const *pval)
{
    return rp_call_on(game, RP_TABLET, S_STATE_MGR, fn, n, pname, pval);
}

static int rp_settext(void *game, uint64_t actor, uint64_t el, const char *s)
{
    int64_t k;
    uintptr_t gs = rp_actor_gs(game, actor, &k);
    return gs && elem_has(gs, actor, el) && canvas_text(gs, actor, el, s);
}

static int rp_gettext(void *game, uint64_t actor, uint64_t el, char *out, size_t cap)
{
    int64_t k;
    uintptr_t gs = rp_actor_gs(game, actor, &k);
    out[0] = 0;
    return gs && canvas_get_text(gs, actor, el, out, cap) == 1;
}

static int rp_fire(void *game, uint64_t holder, uint64_t sym)
{
    int64_t k;
    uintptr_t gs = rp_actor_gs(game, holder, &k);
    if (!gs) return 0;
    t_fault = 0;
    uint64_t id = R64(gs + RP_O_GS_ID);
    if (t_fault) return 0;
    uint64_t ev[5] = { sym, id, SYM_NONE, SYM_NONE, SYM_NONE };
    ((SendCoreFn)(g_base + g_prof->va_send_core))((void *)gs, ev, 1);
    return 1;
}

/* a TARGETED event to one static actor's component (SActor 0x0000ffffffff0000 | index, a static actor's handle), as
 * SendEventNode builds it for a real press (w23b: the five-tile card's close_button). Through the original site
 * (g_target_orig), so our own target hook does not see it. */
static int rp_press_comp(void *game, uint64_t actor, uint64_t ev0, uint64_t comp)
{
    int64_t k;
    uintptr_t gs = rp_actor_gs(game, actor, &k);
    if (!gs) return 0;
    t_fault = 0;
    uint64_t id = R64(gs + RP_O_GS_ID);
    if (t_fault) return 0;
    uint64_t ev[5] = { ev0, id, 0x0000ffffffff0000ull | ((uint64_t)k & 0xffff), comp, SYM_NONE };
    SendCoreFn send = g_target_orig ? g_target_orig : (SendCoreFn)(g_base + g_prof->va_send_target);
    send((void *)gs, ev, 1);
    return 1;
}

static int rp_user(char *out, size_t cap)
{
    pthread_mutex_lock(&g_user_lock);
    snprintf(out, cap, "%s", g_user);
    pthread_mutex_unlock(&g_user_lock);
    return out[0] != 0;
}

/* worker thread: HTTP/1.0 to the client API (127.0.0.1:6721) or the services host */
static int rp_http(const char *method, int client_api, const char *path, const char *body, char *out, size_t cap)
{
    out[0] = 0;
    if (!client_api && !services_ready()) return 0;
    const char *host = client_api ? "127.0.0.1" : g_svc_host, *port = client_api ? "6721" : g_svc_port;
    struct addrinfo hints, *res = NULL, *ai;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &res)) return 0;
    int s = -1;
    for (ai = res; ai; ai = ai->ai_next) {
        s = socket(ai->ai_family, ai->ai_socktype | SOCK_CLOEXEC, ai->ai_protocol);
        if (s < 0) continue;
        struct timeval tv = { 5, 0 };
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
        if (!connect(s, ai->ai_addr, ai->ai_addrlen)) break;
        close(s);
        s = -1;
    }
    freeaddrinfo(res);
    if (s < 0) return 0;
    char req[2048];   /* per call: the report and QUIT WARNING workers both use rp_http */
    size_t bl = body ? strlen(body) : 0;
    int n = body ? snprintf(req, sizeof req, "%s %s HTTP/1.0\r\nHost: %s:%s\r\nConnection: close\r\n"
                            "Content-Type: application/json\r\nContent-Length: %zu\r\n\r\n%s", method, path, host, port, bl, body)
                 : snprintf(req, sizeof req, "%s %s HTTP/1.0\r\nHost: %s:%s\r\nConnection: close\r\n\r\n", method, path, host, port);
    int status = 0;
    if (n > 0 && (size_t)n < sizeof req && send(s, req, (size_t)n, MSG_NOSIGNAL) == n) {
        size_t got = 0;
        ssize_t r;
        while (got < cap - 1 && (r = recv(s, out + got, cap - 1 - got, 0)) > 0) got += (size_t)r;
        out[got] = 0;
        if (sscanf(out, "HTTP/%*s %d", &status) != 1) status = 0;
        char *bp = strstr(out, "\r\n\r\n");
        if (bp) memmove(out, bp + 4, strlen(bp + 4) + 1);
    }
    close(s);
    return status;
}

/* ---- in-process session reader (sess_snapshot.h) ----
 * ng = [game+0x8510] (device-observed Quest 6528386917200980); names by CR15NetGame::UserName 0x124f73c (dlsym, must
 * equal the profile address); the lobby-entrant / LobbyId bodies the header reads inline are pinned by their bytes
 * (dynsym + disasm-only). Any mismatch = unverified build: reader off. Reads by safe_copy;
 * UserName is called only after its bytes matched. Game thread only. */
#include "sess_snapshot.h"
static const Prologue SESS_CHECKS[7] = {
    { 0x12027c8, { 0x08,0xa2,0x90,0x52, 0x00,0x68,0x68,0xf8, 0xc0,0x03,0x5f,0xd6, 0x08,0xa2,0x90,0x52 }, "CR15Game::NetGame" },
    { 0x124f73c, { 0xf5,0x53,0xbe,0xa9, 0xf3,0x7b,0x01,0xa9, 0x08,0xc4,0x41,0x79, 0xf3,0x03,0x00,0xaa }, "CR15NetGame::UserName(u16)" },
    { 0x192d1b0, { 0x08,0xb4,0x41,0xf9, 0x1f,0x01,0x01,0xeb, 0xc9,0x00,0x00,0x54, 0x08,0xb0,0x41,0xf9 }, "CNSLobby::EntrantUserID" },
    { 0x192d1c0, { 0x09,0x1b,0x80,0x52, 0x28,0x20,0x09,0x9b, 0x00,0x05,0x40,0xa9, 0xc0,0x03,0x5f,0xd6 }, "CNSLobby::EntrantUserID+0x10" },
    { 0x192d3b4, { 0x08,0xb4,0x41,0xf9, 0x1f,0x01,0x01,0xeb, 0xc9,0x00,0x00,0x54, 0x08,0xb0,0x41,0xf9 }, "CNSLobby::EntrantPing" },
    { 0x192d3c4, { 0x09,0x1b,0x80,0x52, 0x28,0x20,0x09,0x9b, 0x00,0x15,0x41,0x79, 0xc0,0x03,0x5f,0xd6 }, "CNSLobby::EntrantPing+0x10" },
    { 0x1918c74, { 0x00,0x30,0x07,0x91, 0xc0,0x03,0x5f,0xd6, 0x09,0xb4,0x41,0xf9, 0xc9,0x00,0x00,0xb4 }, "CNSLobby::LobbyId (nsl+0x1cc)" },
};
#define SESS_VA_USERNAME 0x124f73cu
static int g_sess_ok = -1;   /* -1 not checked, 0 unverified build, 1 ok (set once on the attach thread) */
static int q_sess_read(void *dst, uintptr_t src, size_t n) { return safe_copy(dst, src, n); }
static void sess_attach(void)
{
    if (__atomic_load_n(&g_sess_ok, __ATOMIC_ACQUIRE) >= 0) return;
    int ok = 1;
    for (int i = 0; i < 7 && ok; i++) {
        uint8_t got[16];
        if (safe_copy(got, U(g_base) + SESS_CHECKS[i].va, 16) || memcmp(got, SESS_CHECKS[i].bytes, 16)) {
            rlog("session reader off (unverified build): %s at %#x differs", SESS_CHECKS[i].what, SESS_CHECKS[i].va);
            ok = 0;
        }
    }
    if (ok) {
        void *h = dlopen("libr15.so", RTLD_NOW | RTLD_NOLOAD), *f = h ? dlsym(h, "_ZNK10NRadEngine8NRadGame11CR15NetGame8UserNameEt") : NULL;
        if (h) dlclose(h);   /* NOLOAD handle: drops only the reference this dlopen added */
        if (f && U(f) != U(g_base) + SESS_VA_USERNAME) { rlog("session reader off (unverified build): dlsym UserName %p != base + %#x", f, SESS_VA_USERNAME); ok = 0; }
        else rlog("session reader: NetGame / UserName / lobby-entrant / LobbyId bytes match; UserName via %s", f ? "dlsym" : "profile address");
    }
    __atomic_store_n(&g_sess_ok, ok, __ATOMIC_RELEASE);
}
static int rp_sess(void *game, int want, sess_snapshot *s)
{
    if (__atomic_load_n(&g_sess_ok, __ATOMIC_ACQUIRE) != 1) { memset(s, 0, sizeof *s); return -100; }   /* RP_SESS_UNVERIFIED */
    sess_profile prof = { "6528386917200980", 0x8510, q_sess_read, (sess_username_fn)(U(g_base) + SESS_VA_USERNAME), NULL };
    return sess_read(game, &prof, want, s);
}

/* report_card_glyph: CCanvasUICS::SetSpriteUV(SCompactPoolHandle, CSymbol64 element, CSymbol64 region) 0x1391384
 * (ELF-dynsym 6528386917200980; the region is looked up in the canvas's own uvmap) and ShowUIElement (profile).
 * Bytes checked at report_start. */
static const Prologue RP_SPRITE_UV = {
    0x1391384, { 0xff,0x83,0x00,0xd1, 0xf3,0x7b,0x01,0xa9, 0xf3,0x03,0x03,0xaa, 0x21,0x7c,0x40,0x92 }, "SetSpriteUV" };
typedef void (*SpriteUVFn)(void *cs, uint32_t idx, uint64_t element, uint64_t region);
static int g_rp_uv_ok;
static int rp_sprite_uv(void *game, uint64_t actor, uint64_t el, uint64_t region)
{
    int64_t k;
    Handle c;
    uintptr_t gs = g_rp_uv_ok ? rp_actor_gs(game, actor, &k) : 0;
    if (!gs || !elem_has(gs, actor, el) || !canvas_of(gs, actor, &c)) return 0;
    ((SpriteUVFn)(g_base + RP_SPRITE_UV.va))(c.cs, c.idx, el, region);
    return 1;
}
static int rp_show(void *game, uint64_t actor, uint64_t el, uint32_t show)
{
    int64_t k;
    uintptr_t gs = rp_actor_gs(game, actor, &k);
    return gs && elem_has(gs, actor, el) && canvas_show(gs, actor, el, show);
}

/* rp_getvar (card target): a STRING variable of an actor's script component:
 * CLocalComponentHandle<CScriptCS>::Find 0x1291c2c (checked in RP_CHECKS) + CScriptCS::String 0x168f0b4 (ELF-dynsym
 * 6528386917200980; "" when the script is not initialised). SetString 0x168f34c shares its first 0x24 bytes, so the
 * check runs to +0x28 (String's tail call `b` CScriptInstance::String). Checked at report_start. */
static const uint8_t RP_GET_STRING_BYTES[40] = {
    0x08,0x64,0x40,0xf9, 0x29,0x3c,0x40,0x92, 0x29,0xf5,0x7e,0xd3, 0x0a,0x17,0x80,0x52, 0x08,0x69,0x69,0x78,
    0x09,0x7c,0x40,0xf9, 0xe1,0x03,0x02,0xaa, 0x08,0x25,0x0a,0x9b, 0x00,0x0d,0x40,0xf9, 0x52,0xce,0xe2,0x17 };
#define RP_VA_GET_STRING 0x168f0b4u
typedef const char *(*RpGetStringFn)(void *cs, uint32_t idx, uint64_t var);
static int g_rp_var_ok;
static int rp_getvar(void *game, uint64_t actor, uint64_t comp, uint64_t var, char *out, size_t cap)
{
    int64_t k;
    out[0] = 0;
    uintptr_t gs = g_rp_var_ok ? rp_actor_gs(game, actor, &k) : 0;
    if (!gs) return 0;
    Handle h;
    memset(&h, 0, sizeof h);
    ((FindFn)(g_base + RP_VA_FIND_SCRIPT))(&h, (void *)gs, 0x0000ffffffff0000ull | (uint64_t)k, comp);
    if (!h.cs || !((ValidFn)(*(void ***)h.cs)[g_prof->vt_cs_valid / 8])(h.cs, h.idx)) return 0;
    const char *s = ((RpGetStringFn)(g_base + RP_VA_GET_STRING))(h.cs, h.idx, var);
    if (!s) return 0;
    snprintf(out, cap, "%.63s", s);   /* the variable holds 63 chars + NUL (the card accessor's copy loop) */
    return 1;
}

/* rp_setvar ( back re-open: clear the card's xplatformid before OpenLobbyMemberSocialControls): CScriptCS::SetString
 * 0x168f34c (the quit warning's setter; checked at report_start). Copies 63 chars max. 1 = written. Game thread.
 * Its first 0x24 bytes are the thunk String / Real share (RP_GET_STRING_BYTES), so +0 alone cannot tell a write from a
 * read twin: +0x24 is checked too, where SetString differs: mov x8,x3; mov x3,#-1 (the length cap); mov x2,x8; b 0xf0fbb0
 * (String has `b` at +0x24). ELF-dynsym / disasm-only @ 6528386917200980. */
#define RP_VA_SET_STRING_RP 0x168f34cu
static const uint8_t RP_SET_STRING_TAIL[16] = { 0xe8,0x03,0x03,0xaa, 0x03,0x00,0x80,0x92, 0xe2,0x03,0x08,0xaa, 0x0d,0x02,0xe2,0x17 };
static int g_rp_setvar_ok;
static int rp_setvar(void *game, uint64_t actor, uint64_t comp, uint64_t var, const char *str)
{
    int64_t k;
    uintptr_t gs = g_rp_setvar_ok ? rp_actor_gs(game, actor, &k) : 0;
    if (!gs) return 0;
    Handle h;
    memset(&h, 0, sizeof h);
    ((FindFn)(g_base + RP_VA_FIND_SCRIPT))(&h, (void *)gs, 0x0000ffffffff0000ull | (uint64_t)k, comp);
    if (!h.cs || !((ValidFn)(*(void ***)h.cs)[g_prof->vt_cs_valid / 8])(h.cs, h.idx)) return 0;
    ((void (*)(void *, uint32_t, uint64_t, const char *))(g_base + RP_VA_SET_STRING_RP))(h.cs, h.idx, var, str);
    return 1;
}

/* rp_getreal (fallback F's press guard): a REAL variable of an actor's script component: CScriptCS::Real 0x168ef9c
 * (ELF-dynsym 6528386917200980; s0; b CScriptInstance::Real by its PLT stub 0xefc060, 0.0 when the script is not
 * initialised). Real shares String's first 16 bytes, so two rows (+0, +0x18, as the test hook checks it) are checked at report_start;
 * a mismatch leaves the guard off. */
#define RP_VA_GET_REAL 0x168ef9cu
static const uint8_t RP_GET_REAL_BYTES[16] = { 0x08,0x64,0x40,0xf9, 0x29,0x3c,0x40,0x92, 0x29,0xf5,0x7e,0xd3, 0x0a,0x17,0x80,0x52 };
/* mov x1,x2; madd x8,x8,x10,x9; ldr x0,[x8,#0x18]; b 0xefc060 */
static const uint8_t RP_GET_REAL_TAIL[16] = { 0xe1,0x03,0x02,0xaa, 0x08,0x25,0x0a,0x9b, 0x00,0x0d,0x40,0xf9, 0x28,0xb4,0xe1,0x17 };
typedef float (*RpGetRealFn)(void *cs, uint32_t idx, uint64_t var);
static int g_rp_real_ok;
static int rp_getreal(void *game, uint64_t actor, uint64_t comp, uint64_t var, float *out)
{
    int64_t k;
    uintptr_t gs = g_rp_real_ok ? rp_actor_gs(game, actor, &k) : 0;
    if (!gs) return 0;
    Handle h;
    memset(&h, 0, sizeof h);
    ((FindFn)(g_base + RP_VA_FIND_SCRIPT))(&h, (void *)gs, 0x0000ffffffff0000ull | (uint64_t)k, comp);
    if (!h.cs || !((ValidFn)(*(void ***)h.cs)[g_prof->vt_cs_valid / 8])(h.cs, h.idx)) return 0;
    *out = ((RpGetRealFn)(g_base + RP_VA_GET_REAL))(h.cs, h.idx, var);
    return 1;
}

/* (report_flow.h header) element probe, SetColor, page enable and actor presence in any gamespace */
static int rp_has_el(void *game, uint64_t actor, uint64_t el)
{
    int64_t k;
    uintptr_t gs = rp_actor_gs(game, actor, &k);
    return gs && elem_has(gs, actor, el);
}

static int rp_color(void *game, uint64_t actor, uint64_t el, const float rgb[3])
{
    int64_t k;
    uintptr_t gs = rp_actor_gs(game, actor, &k);
    return gs && elem_has(gs, actor, el) && canvas_color(gs, actor, el, rgb);
}

static uintptr_t rp_actor_key(void *game, uint64_t actor)
{
    int64_t k;
    return rp_actor_gs(game, actor, &k);
}

static int rp_has_actor(void *game, uint64_t actor) { return rp_actor_key(game, actor) != 0; }

/* EnablePage on the page component of `owner` in whichever gamespace holds it (show_page reads the menu gamespace
 * only; the pause pages live in mnu_master_mp_ingame) */
static int rp_show_page(void *game, uint64_t owner)
{
    int64_t k;
    uintptr_t gs = rp_actor_gs(game, owner, &k);
    if (!gs) return 0;
    Handle h;
    memset(&h, 0, sizeof h);
    ((FindFn)(g_base + g_prof->va_find_page_cs))(&h, (void *)gs, 0x0000ffffffff0000ull | (uint64_t)k, SYM_NONE);
    if (!h.cs || !((ValidFn)(*(void ***)h.cs)[g_prof->vt_cs_valid / 8])(h.cs, h.idx)) return 0;
    ((GameFn)(g_base + g_prof->va_invoke_set))(game);
    ((EnablePageFn)(g_base + g_prof->va_enable_page))(h.cs, h.idx);
    ((GameFn)(g_base + g_prof->va_invoke_clear))(game);
    return 1;
}

/* ---- the engine's own scroll axis for the pause REPORT list (CR15PointerCS::ScrollAmount) ----
 * disasm-only @ Quest 6528386917200980 (ELF-dynsym names): CR15PointerCS::ScrollAmount(SCompactPoolHandle) const 0x1ea6798
 * = records [cs+0xf8] + (u16 [[cs+0xc8] + handle*4]) * 0x58, f32 +0x14; CR15PointerCS::Update 0x1ea5de4 walks u16 count
 * [cs+0xf4] over the dense list [cs+0xd8] (loop at 0x1ea5f3c) and stores ScrollAmount (s8 / s9 by the hand u32 rec+0x10)
 * at 0x1ea6100; CncaGameSpace::FindComponentSystemMT(CSymbol64) const 0x1804a10 bisects 16-byte {type, cs} entries at
 * [gs+0x420], count [gs+0x450]. The system is taken by its key csym64("R15Pointer") 0x85d04532edaeb196 (in .rodata at
 * 0x31822b0) and accepted only when its vptr is _ZTVN10NRadEngine8NRadGame13CR15PointerCSE 0x3688ab0 + 0x10; with no
 * keyed match the table is scanned by vptr once per gamespace. Read-only (safe_copy), no engine call; bytes checked at
 * report_start. The PC twin is radmenu.c (same layout, disasm-only @ 6323983201049540). */
#define RP_VT_POINTER_CS   (0x3688ab0u + 0x10u)
#define RP_POINTER_TYPE    0x85d04532edaeb196ull
#define RP_O_GS_CS_TBL     0x420u
#define RP_O_GS_CS_N       0x450u
#define RP_O_PCS_HANDLES   0xc8u
#define RP_O_PCS_DENSE     0xd8u
#define RP_O_PCS_COUNT     0xf4u
#define RP_O_PCS_RECS      0xf8u
#define RP_PCS_STRIDE      0x58u
#define RP_O_PREC_HAND     0x10u
#define RP_O_PREC_SCROLL   0x14u
static const Prologue RP_SCROLL_CHECKS[5] = {
    { 0x1ea6798, { 0x08,0x64,0x40,0xf9, 0x29,0x3c,0x40,0x92, 0x29,0xf5,0x7e,0xd3, 0x0a,0x0b,0x80,0x52 }, "CR15PointerCS::ScrollAmount" },
    { 0x1ea67ac, { 0x09,0x7c,0x40,0xf9, 0x1f,0x20,0x03,0xd5, 0x08,0x25,0x0a,0x9b, 0x00,0x15,0x40,0xbd }, "CR15PointerCS::ScrollAmount+0x14" },
    { 0x1ea5f3c, { 0x68,0x6e,0x40,0xf9, 0x69,0x66,0x40,0xf9, 0x78,0x7e,0x40,0xf9, 0x1a,0x79,0x7c,0x78 }, "CR15PointerCS::Update loop" },
    { 0x1ea60fc, { 0x2a,0x05,0x00,0x79, 0x20,0x15,0x00,0xbd, 0x54,0x00,0x00,0x34, 0x48,0xf0,0xff,0x34 }, "CR15PointerCS::Update store" },
    { 0x1804a10, { 0x09,0x28,0x42,0xf9, 0x29,0x02,0x00,0xb4, 0x08,0x10,0x42,0xf9, 0xea,0x03,0x1f,0xaa }, "CncaGameSpace::FindComponentSystemMT const" },
};
static int g_rp_scroll_ok;   /* the bytes matched (and no fault since); game thread after report_start */
static int rp_scroll_amount(void *game, float *out);   /* defined after report_flow.h */

/* ---- review M2: the arm tablet's state-stack top (state_mgr's CScriptStateStackCS) ----
 * CLocalComponentHandle<CScriptStateStackCS>::Find 0x274aac0 (+0x30 = the adrp/add of ScriptStateStack
 * 0x822d952a336593f1), then the CI read as CScriptStateStackCS::CurrentStateName 0x169702c reads it (its whole body up to
 * the ret is checked: 0xc8, 0x58, 0xf8, 0x40, and the stack pointer 0x38 at +0x30): slot = u16 [[cs+0xc8] + idx*4],
 * ci = [cs+0xf8] + slot*0x58, stack [ci+0x38], count [ci+0x40], top = stack[count-1]. ELF-dynsym 6528386917200980;
 * the same read answers `sstate` in the test hook (verified PC + Quest). Reads through safe_copy; checked at report_start. */
#define RP_VA_FIND_SSTATE 0x274aac0u
static const Prologue RP_SSTATE_CHECKS[6] = {
    { 0x274aac0, { 0xff,0x83,0x01,0xd1, 0xf8,0x13,0x00,0xf9, 0xf7,0x5b,0x03,0xa9, 0xf5,0x53,0x04,0xa9 }, "CScriptStateStackCS Find" },
    { 0x274aaf0, { 0x88,0x27,0x00,0xd0, 0x08,0xed,0x32,0x91, 0x08,0xf1,0x7d,0x92, 0xa9,0x62,0x52,0x39 }, "CScriptStateStackCS Find+0x30" },
    { 0x169702c, { 0x08,0x64,0x40,0xf9, 0x29,0x3c,0x40,0x92, 0x29,0xf5,0x7e,0xd3, 0x0a,0x0b,0x80,0x52 }, "CurrentStateName" },
    { 0x169703c, { 0x08,0x69,0x69,0x78, 0x09,0x7c,0x40,0xf9, 0x1f,0x20,0x03,0xd5, 0x0a,0x25,0x0a,0x9b }, "CurrentStateName+0x10" },
    { 0x169704c, { 0x4a,0x21,0x40,0xf9, 0xea,0x00,0x00,0xb4, 0x0b,0x0b,0x80,0x52, 0x08,0x25,0x0b,0x9b }, "CurrentStateName+0x20" },
    { 0x169705c, { 0x08,0x1d,0x40,0xf9, 0x08,0x0d,0x0a,0x8b, 0x00,0x81,0x5f,0xf8, 0xc0,0x03,0x5f,0xd6 }, "CurrentStateName+0x30" },
};
static int g_rp_sstate_ok;   /* the bytes matched; game thread after report_start */
static int rp_tablet_top(void *game, uint64_t *top);   /* defined after report_flow.h */

/* ---- stock-off (report_flow.h RP_SO note) ----
 * ELF-dynsym @ Quest 6528386917200980: CR15ButtonInteractCS::EnableComponent(SCompactPoolHandle, EComponentEnableCategory)
 * 0x1d96298 / DisableComponent 0x1d96354 (args x0 cs, w1 handle, w2 category; the immediate slots, both directions) and
 * Component(SActor, CSymbol64) const 0x1d95cd0 (returns the u32 handle in w0; (u16) 0xffff = none). R_AARCH64_ABS64 on
 * _ZTVN10NRadEngine8NRadGame20CR15ButtonInteractCSE (0x3680250, address point +0x10): slot +0x1b8 = Component, +0x208 =
 * Enable, +0x210 = Disable. The bytes below and the three relocated slots are checked at report_start (any mismatch:
 * stock-off off, the swallow stays). The system = the gamespace CS-table entry with that vptr (key
 * csym64("R14ButtonInteract") first, then any); the handle is verified by rp_zone_check through safe_copy. */
#define RP_HAVE_ZONE_OFF 1
#define RP_VA_BTN_VT         (0x3680250u + 0x10u)
#define RP_VA_BTN_COMPONENT  0x1d95cd0u
#define RP_VA_BTN_ENABLE     0x1d96298u
#define RP_VA_BTN_DISABLE    0x1d96354u
#define RP_BTN_TYPE          0xf5574a730bbb0428ull   /* csym64("R14ButtonInteract") */
static const Prologue RP_ZONE_CHECKS[7] = {
    { 0x1d96298, { 0xf6,0x0f,0x1d,0xf8, 0xf5,0x53,0x01,0xa9, 0xf3,0x7b,0x02,0xa9, 0x08,0x70,0x40,0xf9 }, "CR15ButtonInteractCS::EnableComponent" },
    { 0x1d962a8, { 0x29,0x3c,0x40,0x92, 0x35,0xf5,0x7e,0xd3, 0xf3,0x03,0x01,0xaa, 0x01,0x69,0x75,0x78 }, "EnableComponent+0x10" },
    { 0x1d962b8, { 0x08,0x7c,0x40,0xf9, 0x09,0x18,0x42,0x79, 0xf4,0x03,0x00,0xaa, 0x2a,0xf8,0x7f,0xd3 }, "EnableComponent+0x20" },
    { 0x1d96354, { 0xf7,0x5b,0xbd,0xa9, 0xf5,0x53,0x01,0xa9, 0xf3,0x7b,0x02,0xa9, 0x08,0x70,0x40,0xf9 }, "CR15ButtonInteractCS::DisableComponent" },
    { 0x1d96364, { 0x29,0x3c,0x40,0x92, 0x36,0xf5,0x7e,0xd3, 0x17,0x18,0x42,0x79, 0x15,0x69,0x76,0x78 }, "DisableComponent+0x10" },
    { 0x1d96374, { 0x08,0x7c,0x40,0xf9, 0xf3,0x03,0x00,0xaa, 0xf4,0x03,0x01,0xaa, 0xa9,0xfa,0x7f,0xd3 }, "DisableComponent+0x20" },
    { 0x1d95cd0, { 0xfe,0x0f,0x1f,0xf8, 0x08,0x00,0x40,0xf9, 0x08,0xd9,0x40,0xf9, 0x00,0x01,0x3f,0xd6 }, "CR15ButtonInteractCS::Component" },
};
static int g_rp_zone_ok;                     /* bytes + slots matched; game thread after report_start */
static char g_rp_zone_why[96] = "report flow inert";

#include "report_flow.h"

typedef uint32_t (*RpBtnCompFn)(void *cs, uint64_t sactor, uint64_t name);
typedef void (*RpBtnCatFn)(void *cs, uint32_t h, uint16_t category);
static int rp_zone_ready(const char **why) { *why = g_rp_zone_why; return g_rp_zone_ok; }

/* rp_actor_gs, remembered per actor (find_actor copies the whole actor table): re-checked by the gamespace's game pointer
 * and the actor name at the remembered index, else looked up again */
static uintptr_t rp_zone_gs(void *game, uint64_t actor, int64_t *k)
{
    static void *m_game[4];
    static uint64_t m_actor[4];
    static uintptr_t m_gs[4];
    static int64_t m_k[4];
    static int m_next;
    for (int i = 0; i < 4; i++) {
        if (!m_gs[i] || m_game[i] != game || m_actor[i] != actor) continue;
        t_fault = 0;
        uintptr_t gs = m_gs[i];
        uintptr_t ad = R64(gs + g_prof->o_gs_actors);
        uint64_t n = ad && !t_fault ? R64(ad + g_prof->o_at_count) : 0;
        uintptr_t names = ad && !t_fault ? R64(ad + g_prof->o_at_names) : 0;
        if (!t_fault && R64(gs + RP_O_GS_GAME) == U(game) && names && (uint64_t)m_k[i] < n && R64(names + (uint64_t)m_k[i] * 8) == actor
            && !t_fault) { *k = m_k[i]; return gs; }
        t_fault = 0;
        m_gs[i] = 0;
    }
    uintptr_t gs = rp_actor_gs(game, actor, k);
    if (gs) { m_game[m_next] = game; m_actor[m_next] = actor; m_gs[m_next] = gs; m_k[m_next] = *k; m_next = (m_next + 1) & 3; }
    return gs;
}

/* 1 found (z filled, verified), 0 actor / zone not loaded, -1 unavailable (game thread) */
static int rp_zone_find(void *game, uint64_t actor, uint64_t zone, rp_zref *z)
{
    if (!g_rp_zone_ok) return -1;
    int64_t k;
    uintptr_t gs = rp_zone_gs(game, actor, &k);
    if (!gs) return 0;
    uint64_t sa = 0x0000ffffffff0000ull | (uint64_t)k;
    uintptr_t vt = U(g_base) + RP_VA_BTN_VT;
    static uint8_t buf[256 * 16];
    t_fault = 0;
    uintptr_t tbl = R64(gs + RP_O_GS_CS_TBL);
    uint64_t m = R64(gs + RP_O_GS_CS_N);
    if (t_fault || !tbl || m > 4096) { t_fault = 0; return 0; }
    for (int pass = 0; pass < 2; pass++)   /* the keyed entry first, then any entry with the vtable */
        for (uint64_t k0 = 0; k0 < m; k0 += 256) {
            uint64_t c = m - k0 < 256 ? m - k0 : 256;
            if (safe_copy(buf, tbl + k0 * 16, (size_t)(c * 16))) return 0;
            for (uint64_t j = 0; j < c; j++) {
                uint64_t key, cs;
                uint16_t rs = 0;
                memcpy(&key, buf + j * 16, 8);
                memcpy(&cs, buf + j * 16 + 8, 8);
                if (!cs || (pass == 0) != (key == RP_BTN_TYPE)) continue;
                t_fault = 0;
                if (R64((uintptr_t)cs) != vt || t_fault) { t_fault = 0; continue; }
                uint32_t h = ((RpBtnCompFn)(g_base + RP_VA_BTN_COMPONENT))((void *)(uintptr_t)cs, sa, zone);
                if ((uint16_t)h == 0xffff || !rp_zone_check(safe_copy, (uintptr_t)cs, h, actor, zone, &rs)) continue;
                z->gs = gs; z->cs = (uintptr_t)cs; z->h = h; z->reasons = rs;
                return 1;
            }
        }
    return 0;
}

/* off: DisableComponent(h, 0x80), else EnableComponent(h, 0x80); 1 run, 0 not run (game thread) */
static int rp_zone_cat(void *game, const rp_zref *z, int off)
{
    if (!g_rp_zone_ok || !z->cs) return 0;
    t_fault = 0;
    if (R64(z->cs) != U(g_base) + RP_VA_BTN_VT || t_fault) { t_fault = 0; return 0; }
    ((GameFn)(g_base + g_prof->va_invoke_set))(game);
    ((RpBtnCatFn)(g_base + (off ? RP_VA_BTN_DISABLE : RP_VA_BTN_ENABLE)))((void *)z->cs, z->h, (uint16_t)RP_SO_BIT);
    ((GameFn)(g_base + g_prof->va_invoke_clear))(game);
    return 1;
}

/* 1 read (*top = the top state's symbol, 0 = an empty stack), 0 unavailable (game thread) */
static int rp_tablet_top(void *game, uint64_t *top)
{
    if (!g_rp_sstate_ok) return 0;
    int64_t k;
    uintptr_t gs = rp_actor_gs(game, RP_TABLET, &k);
    if (!gs) return 0;
    Handle h;
    memset(&h, 0, sizeof h);
    ((FindFn)(g_base + RP_VA_FIND_SSTATE))(&h, (void *)gs, 0x0000ffffffff0000ull | (uint64_t)k, S_STATE_MGR);
    if (!h.cs || !((ValidFn)(*(void ***)h.cs)[g_prof->vt_cs_valid / 8])(h.cs, h.idx)) return 0;
    t_fault = 0;
    uintptr_t cs = U(h.cs);
    uint16_t slot = (uint16_t)R32(R64(cs + 0xc8) + (uint64_t)(h.idx & 0xffff) * 4);
    uintptr_t ci = R64(cs + 0xf8) + (uint64_t)slot * 0x58;
    uint64_t depth = t_fault ? 0 : R64(ci + 0x40);
    uintptr_t stack = t_fault ? 0 : R64(ci + 0x38);
    if (t_fault || depth > 0xffff || (depth && !stack)) { t_fault = 0; return 0; }
    *top = depth ? R64(stack + (depth - 1) * 8) : 0;
    if (t_fault) { t_fault = 0; return 0; }
    return 1;
}

/* the cs of a gamespace's CR15PointerCS, or 0: the keyed entry, else (vptr scan) any entry with its vtable */
static uintptr_t rp_pointer_cs(uintptr_t gs, uintptr_t vt, int *keyed)
{
    static uint8_t buf[256 * 16];
    uintptr_t tbl = R64(gs + RP_O_GS_CS_TBL);
    uint64_t m = R64(gs + RP_O_GS_CS_N);
    if (t_fault || !tbl || m > 4096) return 0;
    for (int pass = 0; pass < 2; pass++)
        for (uint64_t k0 = 0; k0 < m; k0 += 256) {
            uint64_t c = m - k0 < 256 ? m - k0 : 256;
            if (safe_copy(buf, tbl + k0 * 16, (size_t)(c * 16))) { t_fault = 1; return 0; }
            for (uint64_t k = 0; k < c; k++) {
                uint64_t key, cs;
                memcpy(&key, buf + k * 16, 8);
                memcpy(&cs, buf + k * 16 + 8, 8);
                if (!cs || (pass == 0 && key != RP_POINTER_TYPE)) continue;
                if (R64(cs) == vt && !t_fault) { *keyed += key == RP_POINTER_TYPE; return (uintptr_t)cs; }
                t_fault = 0;
            }
        }
    return 0;
}

/* the signed max-|ScrollAmount| over every CR15PointerCS record of every gamespace (game thread); 1 read, 0 unavailable.
 * The systems are re-found once a second (cached with their gamespace); each read re-checks the vptr. */
static int rp_scroll_amount(void *game, float *out)
{
    static uintptr_t cs_of[8], gs_of[8];
    static int ncs = -1, logged = -2;
    static uint64_t next;
    if (!g_rp_scroll_ok) return 0;
    uintptr_t vt = U(g_base) + RP_VT_POINTER_CS;
    uint64_t now = now_ms();
    t_fault = 0;
    if (ncs < 0 || now >= next) {
        next = now + 1000;
        ncs = 0;
        int keyed = 0;
        uintptr_t list = R64(U(game) + RP_O_GS_LIST);
        uint64_t n = R64(U(game) + RP_O_GS_COUNT);
        for (uint64_t i = 0; !t_fault && list && n <= 64 && i < n && ncs < 8; i++) {
            uintptr_t gs = R64(list + i * 16 + 8);
            if (t_fault || !gs || R64(gs + RP_O_GS_GAME) != U(game) || t_fault) { t_fault = 0; continue; }
            uintptr_t cs = rp_pointer_cs(gs, vt, &keyed);
            t_fault = 0;
            if (cs) { gs_of[ncs] = gs; cs_of[ncs++] = cs; }
        }
        if (ncs != logged) {
            rlog("report: scroll reader: %d CR15PointerCS system(s) (%d under key %016llx)", ncs, keyed, ULL(RP_POINTER_TYPE));
            logged = ncs;
        }
    }
    float best = 0.0f;
    for (int j = 0; j < ncs; j++) {
        uintptr_t cs = cs_of[j];
        if (R64(cs) != vt || t_fault) { t_fault = 0; next = 0; continue; }   /* unloaded: re-find on the next tick */
        uint8_t hdr[0x38];   /* cs+0xc8 .. cs+0xff: handles, dense, count, records */
        if (safe_copy(hdr, cs + RP_O_PCS_HANDLES, sizeof hdr)) { next = 0; continue; }
        uint64_t hnd, dense, recs;
        uint16_t cnt;
        memcpy(&hnd, hdr, 8);
        memcpy(&dense, hdr + (RP_O_PCS_DENSE - RP_O_PCS_HANDLES), 8);
        memcpy(&cnt, hdr + (RP_O_PCS_COUNT - RP_O_PCS_HANDLES), 2);
        memcpy(&recs, hdr + (RP_O_PCS_RECS - RP_O_PCS_HANDLES), 8);
        uint16_t dl[64];
        if (!hnd || !dense || !recs || cnt > 64 || (cnt && safe_copy(dl, dense, cnt * 2u))) continue;
        for (uint16_t i = 0; i < cnt; i++) {
            uint32_t h = R32(hnd + dl[i] * 4u);
            uintptr_t r = recs + (uint16_t)h * RP_PCS_STRIDE;
            float f;
            uint32_t fb = R32(r + RP_O_PREC_SCROLL);
            if (t_fault) { t_fault = 0; break; }
            memcpy(&f, &fb, 4);
            if (g_rpp_scroll_probe && f != 0.0f)
                rlog("report: scroll probe gs %#lx rec %u hand %u amount %.3f", (unsigned long)gs_of[j], i, R32(r + RP_O_PREC_HAND), (double)f);
            if (f * f > best * best) best = f;
        }
    }
    t_fault = 0;
    *out = best;
    return ncs > 0;
}

/* ---- PAUSE MENU banner (the PC twin is radmenu.c pause_banner_tick) ----
 * Rev c of the pause GRID (Quest twin of ffa_w18c and later) keeps the stock frame on pause p1; its banner
 * 0x445e05b8ad2bf412 (canvas menu_central_region, element `text`) gets "PAUSED" from the pause home at runtime (no
 * level row holds the string). While the grid marker (footer actor ffa_pause_status 0xb8ccd5ac45079093) is loaded, the
 * banner reads "PAUSE MENU": read back every 250 ms, rewritten when it differs. Off with tab_title=0. */
#define PAUSE_BANNER 0x445e05b8ad2bf412ull
#define PAUSE_MARKER 0xb8ccd5ac45079093ull   /* ffa_pause_status (w18c footer, SERVER STATUS) */
static void pause_banner_tick(void *game)
{
    static uint64_t next, next_marker;
    static int marker = -1, written;
    uint64_t now = now_ms();
    if (!g_tab_title_on || now < next) return;
    next = now + 250;
    if (now >= next_marker) {   /* the marker scan walks every gamespace: once a second */
        int m = rp_has_actor(game, PAUSE_MARKER) != 0;
        next_marker = now + 1000;
        if (m != marker) {
            rlog(m ? "pause banner: grid marker ffa_pause_status loaded: banner %016llx reads PAUSE MENU"
                   : "pause banner: grid marker not loaded: stock banner (%016llx)", ULL(PAUSE_BANNER));
            marker = m;
            written = 0;
        }
    }
    if (marker != 1) return;
    char cur[32];
    if (rp_gettext(game, PAUSE_BANNER, SYM_TEXT, cur, sizeof cur) && !strcmp(cur, "PAUSE MENU")) return;
    int ok = rp_settext(game, PAUSE_BANNER, SYM_TEXT, "PAUSE MENU") != 0;
    if (!written || ok != (written == 1)) {
        rlog("pause banner: \"%s\" -> PAUSE MENU %s", cur, ok ? "written" : "NOT written (banner not loaded)");
        written = ok ? 1 : 2;
    }
}

static void *report_worker(void *unused)
{
    (void)unused;
    for (;;) { report_worker_step(); sleep_ms(50); }
    return NULL;
}

/* the tick's part: answer a services-host question while no menu gamespace runs (a match), then the flow */
static void report_tick_q(void *game)
{
    rpt_guard(game, now_ms());   /* M1: the report layer stays hidden whatever report= / the checks say */
    if (!g_rp_on || !g_rp_ready) return;
    if (__atomic_load_n(&g_svc_req, __ATOMIC_ACQUIRE) == 1) services_answer((uint8_t *)game);
    if (!g_user[0]) refresh_user((uint8_t *)game);
    report_tick(game, now_ms());
}

/* stock-off: Enable / Disable / Component bytes and the three relocated vtable slots. Run first in report_start, ahead
 * of the report= and prologue gates: the guard holds the REPORT layer's own zones off on every generic prompt even
 * when the flow is off (report=0) or inert (a failed check) */
static void report_so_check(void)
{
    static const uint32_t slot[3] = { 0x1b8u, 0x208u, 0x210u };
    static const uint32_t target[3] = { RP_VA_BTN_COMPONENT, RP_VA_BTN_ENABLE, RP_VA_BTN_DISABLE };
    g_rp_zone_why[0] = 0;
    for (int i = 0; i < 7 && !g_rp_zone_why[0]; i++) {
        uint8_t got[16];
        if (safe_copy(got, U(g_base) + RP_ZONE_CHECKS[i].va, 16) || memcmp(got, RP_ZONE_CHECKS[i].bytes, 16))
            snprintf(g_rp_zone_why, sizeof g_rp_zone_why, "%s at %#x differs", RP_ZONE_CHECKS[i].what, RP_ZONE_CHECKS[i].va);
    }
    for (int i = 0; i < 3 && !g_rp_zone_why[0]; i++) {
        uint64_t v = 0;
        if (safe_copy(&v, U(g_base) + RP_VA_BTN_VT + slot[i], 8) || v != U(g_base) + target[i])
            snprintf(g_rp_zone_why, sizeof g_rp_zone_why, "vtable %#x slot +%#x is not %#x", RP_VA_BTN_VT, slot[i], target[i]);
    }
    g_rp_zone_ok = !g_rp_zone_why[0];
    rpt_so_start();   /* logs the active path: engine or fallback swallow (<why>) */
}

/* after the hooks are in (attach thread): checks the five script-call prologues, then starts the worker */
static void report_start(void)
{
    report_so_check();
    if (!g_rp_on) return;
    for (int i = 0; i < 5; i++) {
        uint8_t got[16];
        if (safe_copy(got, U(g_base) + RP_CHECKS[i].va, 16) || memcmp(got, RP_CHECKS[i].bytes, 16)) {
            rlog("report: %s at %#x differs from the profile: report flow inert", RP_CHECKS[i].what, RP_CHECKS[i].va);
            return;
        }
    }
    if (g_rp_card_event_s[0]) g_rp_card_event = !strncasecmp(g_rp_card_event_s, "0x", 2) ? strtoull(g_rp_card_event_s, NULL, 16) : csym64(g_rp_card_event_s);
    if (g_rp_card_el_s[0]) g_rp_card_el = !strncasecmp(g_rp_card_el_s, "0x", 2) ? strtoull(g_rp_card_el_s, NULL, 16) : csym64(g_rp_card_el_s);
    report_init_syms();
    sess_attach();
    g_rp_glyph = g_rp_glyph_s[0] ? csym64(g_rp_glyph_s) : 0;
    if (g_rp_glyph) {
        uint8_t got[16];
        g_rp_uv_ok = !safe_copy(got, U(g_base) + RP_SPRITE_UV.va, 16) && !memcmp(got, RP_SPRITE_UV.bytes, 16);
        if (!g_rp_uv_ok) rlog("report: SetSpriteUV at %#x differs from the profile: card glyph off", RP_SPRITE_UV.va);
    }
    {
        uint8_t got[40];
        g_rp_var_ok = !safe_copy(got, U(g_base) + RP_VA_GET_STRING, 40) && !memcmp(got, RP_GET_STRING_BYTES, 40);
        {
            uint8_t sg[0x24], st[16];
            g_rp_setvar_ok = !safe_copy(sg, U(g_base) + RP_VA_SET_STRING_RP, sizeof sg) && !memcmp(sg, RP_GET_STRING_BYTES, sizeof sg)
                          && !safe_copy(st, U(g_base) + RP_VA_SET_STRING_RP + 0x24, 16) && !memcmp(st, RP_SET_STRING_TAIL, 16);
            if (!g_rp_setvar_ok) rlog("report: CScriptCS::SetString at %#x differs: card id clear off", RP_VA_SET_STRING_RP);
        }
        if (g_rp_var_ok) rlog("report: card target by the card's xplatformid (CScriptCS::String bytes match)");
        else rlog("report: CScriptCS::String at %#x differs: card target falls back to 2-player sessions only", RP_VA_GET_STRING);
    }
    {
        uint8_t a[16], b[16];
        g_rp_real_ok = !safe_copy(a, U(g_base) + RP_VA_GET_REAL, 16) && !memcmp(a, RP_GET_REAL_BYTES, 16)
                    && !safe_copy(b, U(g_base) + RP_VA_GET_REAL + 0x18, 16) && !memcmp(b, RP_GET_REAL_TAIL, 16);
        if (!g_rp_real_ok) rlog("report: CScriptCS::Real at %#x differs: field press guard off", RP_VA_GET_REAL);
    }
    {
        int ok = 1;
        for (int i = 0; i < 5 && ok; i++) {
            uint8_t got[16];
            ok = !safe_copy(got, U(g_base) + RP_SCROLL_CHECKS[i].va, 16) && !memcmp(got, RP_SCROLL_CHECKS[i].bytes, 16);
            if (!ok) rlog("report: %s at %#x differs: list scroll by hover dwell only", RP_SCROLL_CHECKS[i].what, RP_SCROLL_CHECKS[i].va);
        }
        g_rp_scroll_ok = ok;
        rlog("report: list scroll report_list_scroll=%s report_scroll_probe=%d report_scroll_sign=%d (ScrollAmount reader %s)",
             g_rpp_scr_name[g_rpp_scroll_mode], g_rpp_scroll_probe, g_rpp_scroll_sign, ok ? "bytes match" : "off");
    }
    {
        int ok = 1;
        for (int i = 0; i < 6 && ok; i++) {
            uint8_t got[16];
            ok = !safe_copy(got, U(g_base) + RP_SSTATE_CHECKS[i].va, 16) && !memcmp(got, RP_SSTATE_CHECKS[i].bytes, 16);
            if (!ok) rlog("report: %s at %#x differs: a tablet closed mid-flow restores at the lapse", RP_SSTATE_CHECKS[i].what,
                          RP_SSTATE_CHECKS[i].va);
        }
        g_rp_sstate_ok = ok;
        if (ok) rlog("report: tablet state reader on (CScriptStateStackCS Find / CurrentStateName bytes match)");
    }
    pthread_t th;
    if (pthread_create(&th, NULL, report_worker, NULL)) { rlog("report: no worker thread: inert"); return; }
    pthread_detach(th);
    __atomic_store_n(&g_rp_ready, 1, __ATOMIC_RELEASE);
    rlog("report: ready (tile uid %016llx, card event %016llx uid %016llx element %016llx%s)", ULL(g_rp_tile_uid),
         ULL(g_rp_card_event), ULL(g_rp_card_uid), ULL(g_rp_card_el), g_rp_card_event ? "" : ": card learn mode, no relabel");
    rlog("report: card glyph %s (report_card_glyph=%s)", g_rp_glyph && g_rp_uv_ok ? "on" : "off", g_rp_glyph_s[0] ? g_rp_glyph_s : "(empty)");
    rlog("report: report_tablet_reasons=%s report_popup=%d (event %016llx) report_card5_event=%016llx report_card5_uid=%016llx "
         "report_card5_learn=%d", g_rp_tr_name[g_rp_treasons], g_rpp_on, ULL(g_rpp_event), ULL(g_rp_card5_event), ULL(g_rp_card5_uid),
         g_rp_card5_learn);
    rlog("report: report=%s (tile uid %016llx)%s", g_rp_on < 0 ? "auto: acts only while ffa_rp_list_page is loaded" : "1: any level",
         ULL(g_rp_tile_uid), g_rp_fake_rows ? "" : "; report_fake_rows=0");
    if (g_rp_fake_rows) rlog("report: *** TEST ONLY: report_fake_rows=%d synthetic players appended to the list; never POSTed ***", g_rp_fake_rows);
}

/* ---- QUIT WARNING (; PC radmenu.c's section, Quest calls) ----
 * On each p7 enable (owner 0xd436ecc9f7f9164d, any level) the tick reads the client lobby in process (sess_snapshot.h
 * meta, no HTTP); only a lobby that is not private and not social_2.0* (any case) writes quit_warning_text, 3 passes 1 s apart,
 * by (a) CScriptCS::SetString 0x168f34c (63 chars kept: cut after the last
 * sentence that fits) on p7's ncaScript, and (b) CCanvasUICS::SetText on the warning element (quit_warning_element,
 * default 0x0ba8656934c17e91) of the confirm widget: quit_warning_actor if set, else each actor of p7's gamespace (then
 * the others) whose live canvas has the three ui_mnu_popup elements, keeping those whose question text holds "sure"
 * when any does (at most QW_MAXW, resolved on pass 1). FindPrimitiveID logs an engine error line for a missing
 * element, so the scan asks confirmationtext first and runs once per public-match p7 enable. Any other answer keeps
 * stock (logged once per reason); a var / element written earlier is put back to "". Log lines "QUIT WARNING:".
 * Off unless quit_warning=1; route (a) off when the Find<CScriptCS> / SetString prologues differ. */
#define P7_QUIT_CONFIRM   0xd436ecc9f7f9164dull
#define QW_EL_WARN        0x0ba8656934c17e91ull   /* ui_mnu_popup element 2 (shown by the arena / combat branch) */
#define QW_EL_CONFIRM     0xf30458cef12c13acull   /* confirmationtext */
#define QW_EL_OTHER       0xc98cfc243010a465ull   /* element 3 */
#define QW_SCRIPT_COMP    0x6aea99eeb2bab6c4ull   /* ncaScript (default script component) */
#define QW_MAXW 4
static const Prologue QW_SET_STRING = {
    0x168f34c, { 0x08,0x64,0x40,0xf9, 0x29,0x3c,0x40,0x92, 0x29,0xf5,0x7e,0xd3, 0x0a,0x17,0x80,0x52 }, "set_string" };
typedef void (*QwSetStringFn)(void *cs, uint32_t idx, uint64_t var, const char *s);   /* copies (63 chars max) */
static uint64_t g_qw_el = QW_EL_WARN, g_qw_actor, g_qw_sym_var;
static char g_qw_var[64];                      /* route (a)'s text (<= 63 chars) */
static int g_qw_ready, g_qw_var_ok, g_qw_gen, g_qw_p7_on;
static int g_qw_var_written, g_qw_el_written;  /* tick only: something to put back on a stock verdict */
static int g_qw_nw;                            /* tick only: the widgets pass 1 resolved */
static uintptr_t g_qw_wgs[QW_MAXW];
static uint64_t g_qw_wk[QW_MAXW], g_qw_wname[QW_MAXW];

/* event hook: p7 on / off (any gamespace) */
static void qw_event(void *gs, const uint64_t *ev)
{
    if (!__atomic_load_n(&g_qw_ready, __ATOMIC_ACQUIRE) || (ev[0] != EV_PAGE_ON && ev[0] != EV_PAGE_OFF)) return;
    if (actor_name(U(gs), ev[2]) != P7_QUIT_CONFIRM) return;
    int on = ev[0] == EV_PAGE_ON;
    __atomic_store_n(&g_qw_p7_on, on, __ATOMIC_RELEASE);
    if (on) __atomic_add_fetch(&g_qw_gen, 1, __ATOMIC_ACQ_REL);
    rlog("QUIT WARNING: p7 %s", on ? "enabled: reading the session in process" : "disabled");
}

/* the live canvas of actor index k in gs */
static int qw_canvas(uintptr_t gs, uint64_t k, Handle *c)
{
    memset(c, 0, sizeof *c);
    ((FindFn)(g_base + g_prof->va_find_canvas))(c, (void *)gs, 0x0000ffffffff0000ull | k, SYM_CANVASUI);
    return c->cs && ((ValidFn)(*(void ***)c->cs)[g_prof->vt_cs_valid / 8])(c->cs, c->idx);
}

static int qw_elem(Handle *c, uint64_t el, void **inst, uint32_t *id)
{
    *inst = NULL;
    *id = ((FindPrimFn)(g_base + g_prof->va_find_prim))(c->cs, c->idx, el, inst);
    return *id != 0xffffffffu && *inst != NULL;
}

/* an element's text on a canvas handle (ElementText), "" when none */
static void qw_el_text(Handle *c, uint64_t el, char *out, size_t cap)
{
    void *inst;
    uint32_t id;
    out[0] = 0;
    if (!qw_elem(c, el, &inst, &id)) return;
    const char *s = ((ElementTextFn)(g_base + g_prof->va_element_text))(inst, id);
    size_t n = 0;
    for (; s && n + 1 < cap; n++) {
        char ch;
        if (safe_copy(&ch, U(s) + n, 1) || !ch) break;
        out[n] = ch;
    }
    out[n] = 0;
}

static int qw_has_sure(Handle *c)
{
    char t[160];
    for (int e = 0; e < 2; e++) {
        qw_el_text(c, e ? QW_EL_OTHER : QW_EL_CONFIRM, t, sizeof t);
        if (strcasestr(t, "sure")) return 1;
    }
    return 0;
}

/* fills g_qw_w* with the widget actors (see the section comment); returns how many. Game thread. */
static int qw_resolve(void *game, uintptr_t p7gs)
{
    static uint64_t tab[0xffff];   /* game thread only */
    int sure[QW_MAXW], found = 0, nsure = 0;
    g_qw_nw = 0;
    t_fault = 0;
    uintptr_t list = R64(U(game) + RP_O_GS_LIST);
    uint64_t n = R64(U(game) + RP_O_GS_COUNT);
    if (t_fault || !list || n > 64) n = 0;
    for (int pass = 0; pass < 2 && !found; pass++)   /* p7's gamespace, then the others */
        for (uint64_t i = 0; i < n && found < QW_MAXW; i++) {
            t_fault = 0;
            uintptr_t gs = R64(list + i * 16 + 8);
            if (t_fault || !gs || R64(gs + RP_O_GS_GAME) != U(game) || t_fault || (gs == p7gs) != (pass == 0)) continue;
            uintptr_t ad = R64(gs + g_prof->o_gs_actors);
            uint64_t cnt = ad ? R64(ad + g_prof->o_at_count) : 0;
            uintptr_t names = ad ? R64(ad + g_prof->o_at_names) : 0;
            if (t_fault || !names || cnt > 0xfffe || safe_copy(tab, names, cnt * 8)) continue;
            for (uint64_t k = 0; k < cnt && found < QW_MAXW; k++) {
                Handle c;
                void *inst;
                uint32_t id;
                if (g_qw_actor && tab[k] != g_qw_actor) continue;
                if (!qw_canvas(gs, k, &c)) continue;
                if (!g_qw_actor && (!qw_elem(&c, QW_EL_CONFIRM, &inst, &id) || !qw_elem(&c, QW_EL_OTHER, &inst, &id))) continue;
                if (!qw_elem(&c, g_qw_el, &inst, &id)) continue;
                g_qw_wgs[found] = gs;
                g_qw_wk[found] = k;
                g_qw_wname[found] = tab[k];
                sure[found] = !g_qw_actor && qw_has_sure(&c);
                nsure += sure[found];
                found++;
            }
        }
    for (int j = 0; j < found; j++) {
        if (nsure && !sure[j]) continue;
        g_qw_wgs[g_qw_nw] = g_qw_wgs[j];
        g_qw_wk[g_qw_nw] = g_qw_wk[j];
        g_qw_wname[g_qw_nw] = g_qw_wname[j];
        rlog("QUIT WARNING: widget %d = actor %016llx (gamespace %s, index %llu)%s", g_qw_nw, ULL(g_qw_wname[j]),
             g_qw_wgs[j] == p7gs ? "of p7" : "other", ULL(g_qw_wk[j]), sure[j] ? ", question text holds \"sure\"" : "");
        g_qw_nw++;
    }
    if (!g_qw_nw) rlog("QUIT WARNING: no widget actor found (element %016llx%s)", ULL(g_qw_el), g_qw_actor ? ", quit_warning_actor set" : "");
    return g_qw_nw;
}

/* route (a): quit_warning_text on p7's script; 1 = written. Game thread. */
static int qw_setvar(void *game, const char *s)
{
    int64_t k;
    uintptr_t gs = rp_actor_gs(game, P7_QUIT_CONFIRM, &k);
    if (!gs || !g_qw_var_ok) return 0;
    Handle h;
    memset(&h, 0, sizeof h);
    ((FindFn)(g_base + RP_VA_FIND_SCRIPT))(&h, (void *)gs, 0x0000ffffffff0000ull | (uint64_t)k, QW_SCRIPT_COMP);
    if (!h.cs || !((ValidFn)(*(void ***)h.cs)[g_prof->vt_cs_valid / 8])(h.cs, h.idx)) return 0;
    ((QwSetStringFn)(g_base + QW_SET_STRING.va))(h.cs, h.idx, g_qw_sym_var, s);
    return 1;
}

/* route (b) on every resolved widget; returns how many took the text */
static int qw_settext(const char *s)
{
    int ok = 0;
    for (int j = 0; j < g_qw_nw; j++) {
        Handle c;
        void *inst;
        uint32_t id;
        if (qw_canvas(g_qw_wgs[j], g_qw_wk[j], &c) && qw_elem(&c, g_qw_el, &inst, &id)
            && ((SetTextFn)(g_base + g_prof->va_canvas_set_text))(c.cs, c.idx, g_qw_el, s) != 0) ok++;
    }
    return ok;
}

static void qw_apply(void *game, int pass)
{
    int64_t k;
    uintptr_t p7gs = rp_actor_gs(game, P7_QUIT_CONFIRM, &k);
    if (!p7gs) { rlog("QUIT WARNING: pass %d/3: p7 owner in no gamespace", pass); return; }
    int a = qw_setvar(game, g_qw_var);
    if (a) g_qw_var_written = 1;
    if (pass == 1) qw_resolve(game, p7gs);
    int b = qw_settext(g_qw_text);
    if (b) g_qw_el_written = 1;
    rlog("QUIT WARNING: pass %d/3: (a) var quit_warning_text %s; (b) element %016llx set on %d/%d widget(s)", pass,
         !g_qw_var_ok ? "off" : a ? "written" : "no ncaScript on p7", ULL(g_qw_el), b, g_qw_nw);
    for (int j = 0; pass == 3 && j < g_qw_nw; j++) {
        Handle c;
        char t[160];
        if (!qw_canvas(g_qw_wgs[j], g_qw_wk[j], &c)) continue;
        qw_el_text(&c, g_qw_el, t, sizeof t);
        rlog("QUIT WARNING: readback actor %016llx element %016llx = \"%s\"", ULL(g_qw_wname[j]), ULL(g_qw_el), t);
    }
}

/* tick (game thread): the verdict for a new p7 enable from the in-process reader (rp_public_match, report_flow.h;
 * no HTTP), then the three passes */
static void qw_tick(void *game)
{
    static int seen, passes;
    static uint64_t next;
    static char last_why[160];
    if (!__atomic_load_n(&g_qw_ready, __ATOMIC_ACQUIRE)) return;
    uint64_t now = now_ms();
    int gen = __atomic_load_n(&g_qw_gen, __ATOMIC_ACQUIRE);
    if (gen != seen) {
        char why[160];
        seen = gen;
        int v = rp_public_match(game, why, sizeof why);
        if (v) {
            rlog("QUIT WARNING: public match (%s): writing the warning, 3 passes 1 s apart", why);
            last_why[0] = 0;
            passes = 3;
            next = now;
        } else {
            passes = 0;
            if (strcmp(why, last_why)) rlog("QUIT WARNING: stock text kept (%s)", why);
            snprintf(last_why, sizeof last_why, "%s", why);
            if (g_qw_var_written || g_qw_el_written) {
                int a = g_qw_var_written && qw_setvar(game, "");
                int b = g_qw_el_written ? qw_settext("") : 0;
                rlog("QUIT WARNING: put back: var %s, element cleared on %d widget(s)", a ? "\"\"" : "untouched", b);
                g_qw_var_written = g_qw_el_written = 0;
            }
        }
    }
    if (passes && now >= next) {
        if (!__atomic_load_n(&g_qw_p7_on, __ATOMIC_ACQUIRE)) { rlog("QUIT WARNING: p7 closed with %d pass(es) left", passes); passes = 0; return; }
        qw_apply(game, 4 - passes);
        passes--;
        next = now + 1000;
    }
}

/* the route (a) text: the whole text when it fits 63 chars, else up to its last sentence end (else word) that fits */
static void qw_cut_var(void)
{
    size_t n = strlen(g_qw_text), cut = 0;
    if (n < sizeof g_qw_var) { snprintf(g_qw_var, sizeof g_qw_var, "%s", g_qw_text); return; }
    for (size_t i = 0; i < sizeof g_qw_var - 1; i++)
        if ((g_qw_text[i] == '.' || g_qw_text[i] == '?' || g_qw_text[i] == '!') && g_qw_text[i + 1] == ' ') cut = i + 1;
    if (!cut)
        for (size_t i = 0; i < sizeof g_qw_var - 1; i++)
            if (g_qw_text[i] == ' ') cut = i;
    if (!cut) cut = sizeof g_qw_var - 1;
    memcpy(g_qw_var, g_qw_text, cut);
    g_qw_var[cut] = 0;
}

/* after the hooks are in (attach thread): options to symbols, prologue checks, the reader */
static void qw_start(void)
{
    if (!g_qw_on) return;
    if (g_qw_el_s[0]) g_qw_el = !strncasecmp(g_qw_el_s, "0x", 2) ? strtoull(g_qw_el_s, NULL, 16) : csym64(g_qw_el_s);
    if (g_qw_actor_s[0]) g_qw_actor = !strncasecmp(g_qw_actor_s, "0x", 2) ? strtoull(g_qw_actor_s, NULL, 16) : csym64(g_qw_actor_s);
    g_qw_sym_var = csym64("quit_warning_text");
    qw_cut_var();
    int va = 1;
    const Prologue *chk[3] = { &RP_CHECKS[0], &RP_CHECKS[1], &QW_SET_STRING };
    for (int i = 0; i < 3; i++) {
        uint8_t got[16];
        if (safe_copy(got, U(g_base) + chk[i]->va, 16) || memcmp(got, chk[i]->bytes, 16)) {
            rlog("QUIT WARNING: %s at %#x differs from the profile: route (a) off", chk[i]->what, chk[i]->va);
            va = 0;
        }
    }
    g_qw_var_ok = va;
    sess_attach();
    if (__atomic_load_n(&g_sess_ok, __ATOMIC_ACQUIRE) != 1) rlog("QUIT WARNING: no session reader (unverified build): the warning never shows");
    __atomic_store_n(&g_qw_ready, 1, __ATOMIC_RELEASE);
    rlog("QUIT WARNING: ready (var %016llx route (a) %s, element %016llx, actor %s; var text \"%s\")", ULL(g_qw_sym_var),
         va ? "on" : "off", ULL(g_qw_el), g_qw_actor ? g_qw_actor_s : "by scan", g_qw_var);
}

static int backoff_status(int st) { return st == 0 || st == 400 || st == 404 || st == 405 || st == 409; }
static int friends_poll(int *last_status, unsigned next_backoff_s)
{
    char user[64], path[160];
    static char body[8192];
    pthread_mutex_lock(&g_user_lock);
    snprintf(user, sizeof user, "%s", g_user);
    pthread_mutex_unlock(&g_user_lock);
    if (user[0]) snprintf(path, sizeof path, "/friends?user_id=%s&format=tsv", user);
    else snprintf(path, sizeof path, "/friends?format=tsv");
    int st = http_get(path, body, sizeof body);
    if (st != 200) {
        if (st != *last_status) {
            char b[120];
            snprintf(b, sizeof b, "%s", body);
            for (char *q = b; *q; q++) if (*q == '\r' || *q == '\n') *q = ' ';
            char bo[48] = "";
            if (backoff_status(st)) snprintf(bo, sizeof bo, "; backing off %u s", next_backoff_s);
            rlog("friends: GET /friends%s -> %d %s (rows keep their texts)%s", user[0] ? " (user_id)" : "", st, b, bo);
        }
        *last_status = st;
        return st;
    }
    Friend fr[NFRIEND];
    int n = 0;
    memset(fr, 0, sizeof fr);
    for (const char *ln = body; *ln && n < NFRIEND;) {
        const char *e = strchr(ln, '\n');
        if (*ln != '\n') {
            tsv_field(ln, 0, fr[n].name, sizeof fr[n].name, 1);
            tsv_field(ln, 1, fr[n].status, sizeof fr[n].status, 0);
            tsv_field(ln, 2, fr[n].text, sizeof fr[n].text, 1);
            tsv_field(ln, 3, fr[n].session, sizeof fr[n].session, 0);
            tsv_field(ln, 4, fr[n].gametype, sizeof fr[n].gametype, 0);
            if (fr[n].name[0] && fr[n].status[0]) n++;
        }
        if (!e) break;
        ln = e + 1;
    }
    pthread_mutex_lock(&g_fr_lock);
    int same = g_fr_have && n == g_fr_n && !memcmp(fr, g_fr, sizeof fr);
    memcpy(g_fr, fr, sizeof fr);
    g_fr_n = n;
    g_fr_have = 1;
    if (!same) g_fr_seq++;
    pthread_mutex_unlock(&g_fr_lock);
    if (!same || *last_status != 200) rlog("friends: GET /friends%s -> 200, %d friend(s)%s", user[0] ? " (user_id)" : "", n, same ? "" : " (changed)");
    *last_status = 200;
    return 200;
}

static void *friends_worker(void *unused)
{
    (void)unused;
    int last = -1;
    unsigned backoff_s = 0;   /* 0, else 300 s doubling up to 30 min after 400/404/405/409 or a transport error */
    uint64_t next_ok = 0;
    rlog("friends: polling every %d ms while page 24 shows and the friend rows are in the tree", g_fr_period_ms);
    for (;;) {
        if (__atomic_exchange_n(&g_svc_changed, 0, __ATOMIC_ACQ_REL)) {
            g_svc_state = 0;
            __atomic_store_n(&g_svc_req, 0, __ATOMIC_RELEASE);
            if (backoff_s) rlog("friends: services host changed: backoff lifted");
            backoff_s = 0;
            next_ok = 0;
            last = -1;
        }
        int user;
        pthread_mutex_lock(&g_user_lock);
        user = g_user[0] != 0;
        pthread_mutex_unlock(&g_user_lock);
        if (__atomic_load_n(&g_p24_on, __ATOMIC_ACQUIRE) && __atomic_load_n(&g_fr_tree, __ATOMIC_ACQUIRE) && user
            && now_ms() >= next_ok) {
            unsigned nb = backoff_s ? (backoff_s * 2 > 1800 ? 1800 : backoff_s * 2) : 300;
            int st = friends_poll(&last, nb);
            if (st == 200) { backoff_s = 0; next_ok = 0; }
            else if (backoff_status(st)) { backoff_s = nb; next_ok = now_ms() + (uint64_t)nb * 1000; }
        }
        sleep_ms((unsigned)g_fr_period_ms);
    }
    return NULL;
}

/* Tree gate (game thread): is our minted tree in the menu gamespace? Asked on a gamespace change and at every page-24
 * enable (a reload can reuse the same pointer). Each evaluation forgets the news tile's texture and our last write
 * (raw engine pointers, re-sampled before the next write); a new gamespace also drops the page-5 event template (it
 * carries the gamespace id), the tab and the carousel state. */
static void tree_eval(uintptr_t gs, const char *why)
{
    g_news_known = 0;
    g_news_tex = g_last_set = NULL;
    if (gs != g_tree_gs) {
        __atomic_store_n(&g_p5_have, 0, __ATOMIC_RELEASE);
        memset(g_footer_miss, 0, sizeof g_footer_miss);
        g_active = 0;
        g_tab = -2;
        g_tree_gs = gs;
        __atomic_store_n(&g_ftue, 0, __ATOMIC_RELEASE);
        g_ftue_pass = g_cl_pass = 0;
        g_lo_force_end = 0;   /*: a new menu gamespace = fresh canvases; force re-arms */
        g_lo_dimmed = g_lo_restore = g_lo_logged_dim = 0;
        g_lo_last_rem = -2;
        __atomic_store_n(&g_lo_ours, 0, __ATOMIC_RELEASE);
        __atomic_store_n(&g_lo_locked, 0, __ATOMIC_RELEASE);
    }
    int t = find_actor(gs, ACTOR_DOT[0]) >= 0;
    int f = t && g_friends && find_actor(gs, ACTOR_FRIEND0) >= 0;
    __atomic_store_n(&g_tree, t, __ATOMIC_RELEASE);
    __atomic_store_n(&g_fr_tree, f, __ATOMIC_RELEASE);
    rlog("tree marker %s (%s): %s; friend rows %s", t ? "present" : "absent", why,
         t ? "menu writes on" : "stock menu, no writes (page tracking only)",
         !g_friends ? "off (friends=0)" : f ? "present, /friends polls" : "absent, no /friends");
}

/* ---- tick hook (before the chained / stock UpdateReset) ---- */
static void tick_body(void *game)
{
    static uint64_t next_user;
    report_tick_q(game);   /* Report Player v1: any level (report_flow.h); inert unless report=1 */
    qw_tick(game);         /* QUIT WARNING: any level; inert unless quit_warning=1 */
    pause_banner_tick(game);   /* w18c pause GRID: the stock pause banner reads PAUSE MENU (any level) */
    t_fault = 0;
    uintptr_t gs = R64(U(game) + g_prof->o_game_menu_gs);
    if (!gs || t_fault) {   /* no menu gamespace: the gate closes; the next one is judged afresh */
        if (g_tree_gs) {
            g_tree_gs = 0;
            __atomic_store_n(&g_tree, 0, __ATOMIC_RELEASE);
            __atomic_store_n(&g_fr_tree, 0, __ATOMIC_RELEASE);
            __atomic_store_n(&g_ftue_src, 0, __ATOMIC_RELEASE);   /* the next menu load is judged afresh */
        }
        return;
    }
    uint64_t now = now_ms();
    int sq = __atomic_load_n(&g_svc_req, __ATOMIC_ACQUIRE);
    if (sq == 1 && __atomic_compare_exchange_n(&g_svc_req, &sq, 4, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
        services_answer((uint8_t *)game);
    int reset = __atomic_exchange_n(&g_reset, 0, __ATOMIC_ACQ_REL);
    if (gs != g_tree_gs || reset) tree_eval(gs, gs != g_tree_gs ? "menu gamespace changed" : "page 24 enabled");
    if (reset && g_fr_tree) services_recheck((uint8_t *)game);
    if (!g_tree) {   /* stock menu: drop queued work, write nothing */
        g_active = 0;
        g_tab = -2;
        g_paused = 0;
        __atomic_store_n(&g_ptr_mask, 0, __ATOMIC_RELEASE);
        __atomic_store_n(&g_jump, -1, __ATOMIC_RELEASE);
        __atomic_store_n(&g_fr_press, -1, __ATOMIC_RELEASE);
        __atomic_store_n(&g_go_p42, 0, __ATOMIC_RELEASE);
        __atomic_store_n(&g_go_p48, 0, __ATOMIC_RELEASE);
        __atomic_store_n(&g_go_season, 0, __ATOMIC_RELEASE);
        __atomic_store_n(&g_back24, 0, __ATOMIC_RELEASE);
        __atomic_store_n(&g_p36_go, 0, __ATOMIC_RELEASE);
        __atomic_store_n(&g_lo_go, 0, __ATOMIC_RELEASE);
        __atomic_store_n(&g_lo_back, 0, __ATOMIC_RELEASE);
        __atomic_store_n(&g_lo_locked, 0, __ATOMIC_RELEASE);
        return;
    }
    ftue_tick((uint8_t *)game, gs, now, reset);
    combat_lock_tick(gs, now);
    lockout_tick((uint8_t *)game, gs, now, reset);
    if (g_fr_tree && !g_ini_user[0] && now >= next_user) { next_user = now + 2000; refresh_user((uint8_t *)game); }
    int on = g_carousel && g_p24_on && g_p5_state != 0;
    if (reset) {
        memset(g_slide_logged, 0, sizeof g_slide_logged);
        g_slide = g_fixed >= 0 ? g_fixed : 0;
        g_next = now + g_period_ms;
        if (on) { rlog("page 24 shown: carousel reset to slide %d", g_slide); apply_face(gs, g_slide, 1); }
        g_reassert = now + 2000;
        g_fr_reassert = 0;
    }
    int jump = __atomic_exchange_n(&g_jump, -1, __ATOMIC_ACQ_REL);
    int mask = __atomic_load_n(&g_ptr_mask, __ATOMIC_ACQUIRE), hovered = g_hover_pause && mask != 0;
    if (on && hovered && g_next < now + (uint64_t)g_hover_resume_ms) g_next = now + (uint64_t)g_hover_resume_ms;
    if (on && hovered != g_paused) {
        if (hovered) rlog("HOVER pause (pointer mask %02x)", mask);
        else rlog("hover end: rotation resumes in %d ms", g_hover_resume_ms);
        g_paused = hovered;
    }
    if (on) {
        if (jump >= 0 && jump < NSLIDE && g_active) {
            rlog("DOT %d pressed: slide %d -> %d, period restarts", jump, g_slide, jump);
            g_slide = jump;
            g_next = now + g_period_ms;
            if (hovered && g_next < now + (uint64_t)g_hover_resume_ms) g_next = now + (uint64_t)g_hover_resume_ms;
            apply_face(gs, g_slide, 1);
            g_reassert = now + 2000;
        }
        if (!g_active) {
            rlog("carousel on (page 24 enabled, page 5 state %d)", g_p5_state);
            if (g_fixed >= 0) g_slide = g_fixed;
            g_next = now + g_period_ms;
            apply_face(gs, g_slide, 1);
            g_reassert = now + 2000;
        }
        if (now >= g_next && g_fixed < 0) {
            g_slide = (g_slide + 1) % NSLIDE;
            g_next = now + g_period_ms;
            apply_face(gs, g_slide, 1);
            g_reassert = now + 2000;
        } else if (now >= g_reassert) {   /* the tile's own script may redraw: keep our face */
            apply_face(gs, g_slide, 0);
            g_reassert = now + 2000;
        }
    } else if (g_active) {
        rlog("carousel off (page 24 %d, page 5 %d): stock face back", g_p24_on, g_p5_state);
        apply_face(gs, 0, 1);
        g_slide = 0;
        __atomic_store_n(&g_ptr_mask, 0, __ATOMIC_RELEASE);   /* a disabled element gets no _Idle / _OnEndIntersection */
        g_paused = 0;
    }
    g_active = on;
    apply_tabs(gs, now);
    apply_footer(gs, now, g_tab);
    if (g_fr_tree && g_p24_on) apply_friends(gs, now);
    if (g_go_season && g_help_season) season_send((uint8_t *)game, gs);
    if (__atomic_exchange_n(&g_go_p42, 0, __ATOMIC_ACQ_REL))
        rlog("REDIRECT slide 2: show page 42 (owner %016llx): %s", ULL(P42_QUIT), show_page((uint8_t *)game, P42_QUIT) ? "page not found" : "enabled");
    if (__atomic_exchange_n(&g_go_p48, 0, __ATOMIC_ACQ_REL))
        rlog("HELP SAFE ZONE: show page 48 (owner %016llx): %s", ULL(P48_LEGAL), show_page((uint8_t *)game, P48_LEGAL) ? "page not found" : "enabled");
}

static void tick_hook(void *game)
{
    tick_body(game);
    g_tick_orig(game);
}

/* ---- attach (radlinks_quest.c's path): find libr15.so, check identity, wait for RELRO, swap / chain each word ---- */
static uintptr_t g_found, g_self_base;
static char g_self_dir[256];
static int find_cb(struct dl_phdr_info *info, size_t size, void *data)
{
    (void)size; (void)data;
    const char *n = info->dlpi_name, *s = n ? strrchr(n, '/') : NULL;
    const char *base = s ? s + 1 : n;
    if (n && !strcmp(base, "libr15.so") && info->dlpi_addr && info->dlpi_phnum) g_found = (uintptr_t)info->dlpi_addr;
    return 0;
}

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
        if (ok) { rlog("%s hooked", what); return 1; }
        rlog("%s: changed to %p while swapping: judging again", what, (void *)expect);
    }
    rlog("%s: kept changing: refusing this hook", what);
    return 0;
}

#define SETTLE_QUIET_MS 1000
#define SETTLE_MAX_MS 3000
#define NWORDS 3
static void settle_words(const uintptr_t w[NWORDS], int siblings)
{
    if (!siblings) return;
    uint64_t t0 = now_ms(), last = t0, v[NWORDS];
    int changed = 0;
    for (int i = 0; i < NWORDS; i++) v[i] = R64(w[i]);
    while (now_ms() - last < SETTLE_QUIET_MS && now_ms() - t0 < SETTLE_MAX_MS) {
        sleep_ms(20);
        for (int i = 0; i < NWORDS; i++) {
            uint64_t x = R64(w[i]);
            if (x != v[i]) { v[i] = x; last = now_ms(); changed++; }
        }
    }
    rlog("%d earlier module(s): words settled after %llu ms (%d change(s) seen)", siblings, ULL(now_ms() - t0), changed);
}

/* the word is relocated: the stock target, or a hook in another executable module (rad-links chained first) */
static int relocated(uintptr_t at, uintptr_t stock)
{
    t_fault = 0;
    uintptr_t v = R64(at);
    const char *why;
    return !t_fault && (v == stock || foreign_module(v, &why) != NULL);
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
    rlog("libr15.so mapped after %llu ms; %d earlier module(s) in this library's directory", ULL(now_ms() - t0), os.siblings);
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
        if (memcmp(got, p->bytes, 16)) { rlog("inert: %s prologue at +%#x differs: not libr15.so 6528386917200980", p->what, p->va); return NULL; }
    }
    uintptr_t slot = U(g_base) + g_prof->va_tick_slot, gcore = U(g_base) + g_prof->got_send_core;
    uintptr_t gtarget = U(g_base) + g_prof->got_send_target;
    uintptr_t want_tick = U(g_base) + g_prof->va_tick_orig, want_core = U(g_base) + g_prof->va_send_core;
    uintptr_t want_target = U(g_base) + g_prof->va_send_target;
    t0 = now_ms();
    for (;;) {
        char p1[5] = "", p2[5] = "", p3[5] = "";
        int prot = !page_perms(slot, p1) && !page_perms(gcore, p2) && !page_perms(gtarget, p3) && p1[1] == '-' &&
                   p2[1] == '-' && p3[1] == '-';
        if (prot && relocated(gcore, want_core) && relocated(slot, want_tick) && relocated(gtarget, want_target)) break;
        if (now_ms() - t0 > 60000) { rlog("inert: not relocated / RELRO not read-only after 60 s"); return NULL; }
        sleep_ms(20);
    }
    {
        const uintptr_t words[NWORDS] = { gcore, slot, gtarget };
        settle_words(words, os.siblings);
    }
    if (!swap_word(gcore, want_core, U(core_hook), (void **)&g_core_orig, "core GOT (SendComponentEventGlobal)")) {
        rlog("inert: core word refused");
        return NULL;
    }
    /* target before tick: the tick may send the season event through g_target_orig */
    int target = swap_word(gtarget, want_target, U(target_hook), (void **)&g_target_orig, "target GOT (SendComponentEvent)");
    if (!target) rlog("target word refused: news-click redirect, SEASON PASS and SAFE ZONE off (the rest runs)");
    if (!swap_word(slot, want_tick, U(tick_hook), (void **)&g_tick_orig, "tick vtable slot (CR15Game UpdateReset)")) {
        rlog("inert: tick word refused (core%s hooked, page tracking only)", target ? " + target" : "");
        return NULL;
    }
    if (g_friends) {   /* polls only while the tree's friend rows are present (tree_eval) */
        pthread_t th;
        if (pthread_create(&th, NULL, friends_worker, NULL)) rlog("friends: no worker thread");
        else pthread_detach(th);
    }
    report_start();
    qw_start();
    rlog("ready: hooks core,%stick; carousel %d (%d ms), tab_title %d, tab_highlight %d, friends %d, sprite %d, hover_pause %d, "
         "footer_copy %d; writes only with the tree marker ffa_news_dot0", target ? "target," : "", g_carousel, g_period_ms,
         g_tab_title_on, g_tab_hl, g_friends, g_sprite, g_hover_pause, g_footer_copy);
    return NULL;
}

__attribute__((constructor)) static void radmenu_init(void)
{
    char exe[256];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    exe[n > 0 ? n : 0] = 0;
    if (!strstr(exe, "app_process")) return;
    SYM_TEXT = csym64("text");
    SYM_DOT_ON = csym64("dot_on");
    SYM_DOT_OFF = csym64("dot_off");
    SYM_DOT_BOX = csym64("dot_box");
    SYM_BACKGROUND = csym64("Background");
    SYM_CODETEXT = csym64("codetext");
    SYM_CODETEXT_OFF = csym64("codetext_off");
    SYM_BG = csym64("background_image");
    SYM_NOOP = csym64("evt_radmenu_carousel_consumed");
    SYM_LINK_BTN = csym64("evt_link_button_clicked");
    if (csym64("evt_ffa_my_code_eye_clicked") != EV_EYE) rlog("WARNING: eye event symbol differs from csym64 of its name");
    SYM_HIGHLIGHT = csym64("highlight");
    int ok = SYM_DOT_BOX == 0x2fd3808b61765892ull && SYM_BACKGROUND == 0xd66db0afd38fc682ull &&
             SYM_CODETEXT == 0x35e290123dfa9c72ull && csym64("evt_ffa_news_dot0_clicked") == EV_DOT[0] &&
             csym64("ffa_news_dot0") == ACTOR_DOT[0] && csym64("evt_ffa_help_season_clicked") == EV_HELP_SEASON &&
             csym64("evt_ffa_help_safezone_clicked") == EV_HELP_SAFE && csym64("evt_card_clicked") == EV_CARD_CLICKED &&
             csym64("evt_ffa_friend_row1_clicked") == EV_FRIEND[0];
    if (!ok) { rlog("radmenu: csym64 self-check FAILED: inert"); return; }
    Dl_info self;
    if (dladdr((void *)radmenu_init, &self) && self.dli_fbase) {
        g_self_base = U(self.dli_fbase);
        const char *sl = self.dli_fname ? strrchr(self.dli_fname, '/') : NULL;
        if (sl && (size_t)(sl - self.dli_fname + 1) < sizeof g_self_dir) {
            memcpy(g_self_dir, self.dli_fname, (size_t)(sl - self.dli_fname + 1));
            g_self_dir[sl - self.dli_fname + 1] = 0;
        }
    }
    rlog("rad-menu loaded in pid %d; csym64 self-check ok; waiting for libr15.so", getpid());
    read_options();
    if (!g_enabled) { rlog("disabled (radmenu.ini enabled=0): no hook installed, fully inert"); return; }
    ini_user();
    pthread_t th;
    if (pthread_create(&th, NULL, attach_thread, NULL)) rlog("attach thread failed");
    else pthread_detach(th);
}
