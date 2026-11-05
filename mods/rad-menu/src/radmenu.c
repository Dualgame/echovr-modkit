/* rad-menu.dll — a 3-slide carousel on the main-menu news tile .
 *
 * Echo VR PC final (6323983201049540) only; any other image is refused. Built for the bed-local layout tree
 * ffa_w2: the news tile actor 0x27dfdb284b22a843 (canvas ui_mnu_news_info) sits on page 5
 * (owner 0x08324e714b4a21aa), which shows with the home page 24 (owner 0x05ce113632359ce3); the season tile
 * 0x9e0bf5fe92aba570 (canvas ui_mnu_season_info) is parked under it.
 *
 *   while page 24 is enabled: every period (5 s) slide k = 0,1,2 ->
 *     the news tile's `text` = "SELF-GOALING PATCH" / "SEASON 7 LAUNCH" / "EARLY QUIT INFO" (+ an optional dots suffix)
 *     the news tile's `background_image` texture = its own (k 0, 2; a NULL slot = the canvas's own sprite, put back
 *     by clearing the override) / the season tile's loaded texture (k 1)
 *     pager dots (tree ffa_w9a): 3 minted dot actors 0x1cfbcecb4b2d74a4..a6 (canvas ffa_news_dot); the active
 *     dot's `dot_on` element and the others' `dot_off` element get the glyph, the other element ""
 *   a press on dot i (its click_event evt_ffa_news_dot<i>_clicked) -> slide i at once, the period restarts
 *   the pointer on the news tile or a dot (delegate_OnStartIntersection / bhvr_delegate_OnPointerInteractHover,
 *     global events about that actor) -> the rotation holds; it resumes hover_resume_ms (2 s) after the pointer left
 *   page 24 enabled again -> slide 0; page 24 disabled -> the stock face is put back
 *   a click on the news tile (evt_news_info_clicked, targeted to the page-5 owner):
 *     k 0 -> untouched (stock news popup, page 8)
 *     k 1 -> the event symbol is rewritten in place to the season tile's click event before the stock handler
 *            runs, so the page-5 owner's own season route runs instead (no page 8)
 *     k 2 -> the event symbol is rewritten to a symbol nobody handles (no page 8), and page 42 (EARLY QUIT INFO,
 *            owner 0x791142542a3a6dc5) is shown from the next tick, as R15UIPage2EnablePageNode does
 *
 * Threads: the event hooks only set flags (and rewrite the click in place); every canvas / page call runs on the
 * per-frame CR15Game tick. Log: <exe dir>\_local\radmenu.log. Options: <exe dir>\_local\radmenu.ini, section
 * [radmenu]: period_ms (5000), sprite (1 = swap the image on slide 1), dots (1 = suffix, 2 = second line, 0 = off),
 * dot_glyph (o), hover_pause (1), hover_resume_ms (2000), fixed (-1 = cycle; 0..2 = hold that slide, for a
 * deterministic click test).
 *
 * Copied from rad-links' radlinks.c (profile words, show_page, menu_actor, canvas text, hook
 * install with chaining, broker entry). New here, found by static disassembly of that exe (disasm-only):
 * CCanvasUICS::FindPrimitiveID-style lookup 0x3037c0, the SetOverrideTexture wrapper 0x37d000 and the
 * CUICanvasInstance primitive layout it uses (prims +0x90 stride 0xe0, type +8, slot +0xc, type-3 textures +0xc8).
 *
 * (tree ffa_w11a): CCanvasUICS::SetColor 0x37b750 / ShowUIElement 0x3810f0
 * (disasm-only, bytes checked apart: a mismatch turns only these off) -> the active rail tab's Background is filled
 * blue (tab_highlight), the pager dots' dot_box squares are coloured (square_dot), and the HELP SEASON PASS press
 * (evt_ffa_help_season_clicked) is forwarded to the page-5 owner as the season click event (help_season).
 * (tree ffa_w11b): the HELP SAFE ZONE card's own click event (evt_ffa_help_safezone_clicked, sent by its extra
 * button component) shows page 48 from the next tick, the slide-2 -> p42 path (help_safezone); fallback: an
 * evt_card_clicked while the pointer is on the card and HELP shows does the same (help_safezone_card).
 * (tree ffa_w12a, fixes 2 + 4): the ONLINE footer strings (RAD version, server
 * status) are read with CUICanvasInstance::ElementText 0x71c6b0 and copied into the HELP / SETTINGS copies
 * (footer_copy; log "FOOTER el"); an empty FRIENDS row hides its slab and avatar by ShowUIElement (hide_empty_rows;
 * log "slab HIDDEN"). SetColor takes a C3Vector (no alpha), so ShowUIElement is the hide path.
 * Tree gate (same as the Quest build): every canvas write, click rewrite and page route runs only while the minted
 * actor ffa_news_dot0 is in mnu_master (checked at each page-24 enable and on a gamespace change); /friends polls only
 * while the minted friend rows are there too, and backs off 300 s (doubling to 30 min) after 400/404/405/409 or a
 * transport error. radmenu.ini enabled=0 installs no hook; carousel=0 / tab_title=0 turn those writes off.
 * radmenu.log moves to radmenu.log.1 at 256 KB.
 * QUIT WARNING (quit_warning=1, on by default): on the pause quit confirm p7 in a public
 * match (the net game's client lobby read in process: not private, not social_2.0*), Coffey's warning goes into p7's quit_warning_text and the
 * confirm widget's warning element, 3 x 1 s apart. See the QUIT WARNING section. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <time.h>
#include "evrmod.h"
#include "evrmod_profile_pc.h"
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

typedef struct {
    const char *name;
    uint32_t timestamp, size_of_image;
    uint32_t rva_tick_slot, rva_tick_orig;    /* CR15Game vftable slot +0x150 (per frame) and its stock target */
    uint32_t rva_send_target, rva_send_core;  /* CncaGameSpace::SendComponentEvent / SendComponentEventGlobal */
    uint8_t  send_prologue[15];               /* both sites start with the same three register-home stores */
    uint32_t o_gs_id, o_gs_actors, o_at_names, o_at_count;
    uint32_t o_gs_list, o_gs_count, o_gs_game;
    uint32_t rva_find_page_cs, vt_cs_valid, rva_invoke_set, rva_invoke_clear, rva_enable_page;
    uint32_t rva_find_canvas, rva_canvas_set_text, rva_find_prim, rva_set_override_tex;
    uint32_t o_inst_prims, prim_stride, o_prim_type, o_prim_slot, o_inst_sprite_tex;
    uint32_t chk_rva[8];
    uint8_t  chk_bytes[8][8];
    uint32_t rva_remote_log;                  /* void(const char *json, size_t len): every remote-log JSON line */
    uint8_t  remote_log_prologue[16];
    /* (disasm-only PC 6323983201049540): CCanvasUICS element setters, same (cs, idx,
     * element, arg) shape as SetText. Checked apart from chk_rva: a mismatch only turns the chrome off. */
    uint32_t rva_set_color, rva_show_el;
    uint32_t chk2_rva[2];
    uint8_t  chk2_bytes[2][8];
    /*: CUICanvasInstance::ElementText (const char *(inst, SUIPrimitiveID)), the engine's text reader;
     * disasm-only shape, device-observed on p41. Checked apart: a mismatch only turns fix 2 off. */
    uint32_t rva_element_text;
    uint8_t  chk3_bytes[8];
} Profile;

static const Profile g_profiles[] = {
    {
        EVRMOD_PC_FINAL_NAME, EVRMOD_PC_FINAL_TS, EVRMOD_PC_FINAL_SOI,
        0x16d1100, 0x5194f0,
        0x50fb60, 0x50fed0,
        { 0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x6c, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18 },
        0x58, 0x370, 0x80, 0xa8,              /* rad-links (device-verified) */
        0x1f50, 0x1f80, 0x08,
        0xc06230, 0x1c8, 0x510540, 0x4e8960, 0xa697a0,
        0x91daf0, 0x37d470, 0x3037c0, 0x37d000,
        0x90, 0xe0, 0x08, 0x0c, 0xc8,         /* CUICanvasInstance::SetOverrideTexture 0x727a00 (disasm-only) */
        { 0xc06230, 0x91daf0, 0xa697a0, 0x510540, 0x4e8960, 0x37d470, 0x3037c0, 0x37d000 },
        { { 0x48, 0x89, 0x5c, 0x24, 0x08, 0x4c, 0x89, 0x44 },   /* Find<CR15UIPage2CS> */
          { 0x48, 0x89, 0x5c, 0x24, 0x08, 0x4c, 0x89, 0x44 },   /* Find<CCanvasUICS> */
          { 0x4c, 0x8b, 0xdc, 0x56, 0x41, 0x54, 0x41, 0x56 },   /* EnablePage */
          { 0x40, 0x53, 0x48, 0x83, 0xec, 0x20, 0x48, 0x8b },   /* SetInvokeImmediate */
          { 0x40, 0x53, 0x48, 0x83, 0xec, 0x20, 0x48, 0x83 },   /* ClearInvokeImmediate */
          { 0x40, 0x53, 0x48, 0x83, 0xec, 0x40, 0x49, 0x8b },   /* CCanvasUICS::SetText */
          { 0x4c, 0x89, 0x4c, 0x24, 0x20, 0x53, 0x55, 0x41 },   /* primitive lookup */
          { 0x40, 0x53, 0x48, 0x83, 0xec, 0x40, 0x49, 0x8b } }, /* CCanvasUICS::SetOverrideTexture */
        0x113960,                             /* remote-log submit, from rad-links (device-verified there) */
        { 0x48, 0x89, 0x5c, 0x24, 0x08, 0x57, 0x48, 0x83, 0xec, 0x30, 0x48, 0x8b, 0xda, 0x48, 0x8b, 0xf9 },
        0x37b750, 0x3810f0,                   /* CCanvasUICS::SetColor(C3Vector *) / ShowUIElement(uint) */
        { 0x37b750, 0x3810f0 },
        { { 0x40, 0x53, 0x48, 0x83, 0xec, 0x40, 0x48, 0x8d },   /* SetColor (bytes checked against the basis exe) */
          { 0x40, 0x53, 0x48, 0x83, 0xec, 0x40, 0x41, 0x8b } }, /* ShowUIElement */
        0x71c6b0, { 0x89, 0x54, 0x24, 0x10, 0x53, 0x56, 0x48, 0x83 },   /* ElementText (check bytes) */
    },
};

/* Layout ffa_w2  and mnu_master @ 6323983201049540 (stream-confirmed / device-observed there). */
#define LEVEL_MNU_MASTER 0xac360e41e4ede056ull
#define P24_HOME     0x05ce113632359ce3ull
#define P5_TILES     0x08324e714b4a21aaull
#define P42_QUIT     0x791142542a3a6dc5ull
#define P58_HELP     0xe73618208c520c8dull   /* minted HELP page (menu_layout tree ffa_w6*) */
#define P30_SETTINGS 0x05eb6c644ad6ef27ull
#define ACTOR_BANNER 0x99af55d93126a17aull   /* the frame's "MAIN MENU" banner (menu_central_region, element text) */
#define RAIL_ONLINE  0x7593d0177b013e69ull   /* stock LEGAL, relabelled ONLINE */
#define RAIL_SETTING 0x623b5f0e1e11a695ull
#define RAIL_HELP    0x71f7c477f0619237ull   /* stock CREDITS, relabelled HELP */
#define ACTOR_NEWS   0x27dfdb284b22a843ull
#define ACTOR_SEASON 0x9e0bf5fe92aba570ull
#define EV_NEWS      0xfb75d76107e450d8ull   /* evt_news_info_clicked (news tile click_event row) */
#define EV_SEASON    0x0b64e3596273caacull   /* the season tile's click_event row */
#define EV_HELP_SEASON 0x8adf5a10c6b3d9b6ull /*: csym64("evt_ffa_help_season_clicked"), the HELP SEASON PASS tile */
#define EV_HELP_SAFE 0x27091852f7d04146ull   /*: csym64("evt_ffa_help_safezone_clicked"), the SAFE ZONE card's
                                              * extra button component ffa_safezone_btn (polish/build.sh) */
#define EV_CARD_CLICKED 0x8a53d85233b8739eull /* csym64("evt_card_clicked"): the card module's compiled press event */
#define ACTOR_SAFE   0x6bead65a6955c109ull   /* the HELP SAFE ZONE card (p58) */
#define P48_LEGAL    0x63ebda9f6d08e654ull   /* p48 group_legal_notif_page owner: the SAFE ZONE target ( recipe) */
#define SEASON_COMP  0x6aea99eeb2bab6c4ull   /* the page-5 owner's one CScriptCR component (stream-confirmed) */
#define RAIL_QUIT    0x720d06f0ec096825ull
#define SYM_BACKGROUND_K 0xd66db0afd38fc682ull /* csym64("Background"), ui_mnu_button_small prim 1 */
#define SYM_DOT_BOX_K    0x2fd3808b61765892ull /* csym64("dot_box"), ffa_news_dot box */
#define EV_PAGE_ON   0xd64f3478683e4a9cull   /* delegate_OnPage2Enabled (global; ev[2] = page owner) */
#define EV_PAGE_OFF  0x7aad67cf0466af38ull   /* delegate_OnPage2Disabled */
/* pointer state, global events about the actor under the pointer (ev[2]);
 * (device-observed @ 6323983201049540, final-pc-test-control-primitives.md) */
#define EV_PTR_HOVER 0x0c40e9c29ebcc824ull   /* bhvr_delegate_OnPointerInteractHover (CanvasUI) */
#define EV_PTR_IDLE  0x27b7ebd0100cb25dull   /* bhvr_delegate_OnPointerInteractIdle */
#define EV_PTR_START 0x55c56d4a9201a66cull   /* delegate_OnStartIntersection (R15PointerInterface) */
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
#define SYM_CANVASUI 0x35ec9a0128ec916full
#define NSLIDE 3
/* pager dots (tree ffa_w9a): csym64("ffa_news_dot<i>") and their click events csym64("evt_ffa_news_dot<i>_clicked") */
static const uint64_t ACTOR_DOT[NSLIDE] = { 0x1cfbcecb4b2d74a4ull, 0x1cfbcecb4b2d74a5ull, 0x1cfbcecb4b2d74a6ull };
static const uint64_t EV_DOT[NSLIDE] = { 0x368bf9e7a924467eull, 0x1dd2dfb4f1b3d514ull, 0x6039b541180b60aaull };

static const char *const g_title[NSLIDE] = { "SELF-GOALING PATCH", "SEASON 7 LAUNCH", "EARLY QUIT INFO" };
static const char *const g_dots[NSLIDE] = { "O . .", ". O .", ". . O" };   /* ASCII: no glyph question */
static uint64_t SYM_LINK_BTN;
static uint64_t SYM_TEXT, SYM_BG, SYM_NOOP, SYM_DOT_ON, SYM_DOT_OFF, SYM_BACKGROUND, SYM_HIGHLIGHT, SYM_DOT_BOX;

typedef void (__fastcall *TickFn)(void *game);
typedef void (__fastcall *SendFn)(void *gs, const uint64_t *event, uint32_t flags);
typedef void (__fastcall *GameFn)(void *game);
typedef struct { void *cs; uint32_t idx; } Handle;
typedef void *(__fastcall *FindFn)(Handle *out, void *gs, uint64_t actor, uint64_t component);
typedef uint8_t (__fastcall *ValidFn)(void *cs, uint32_t idx);
typedef void (__fastcall *EnablePageFn)(void *cs, uint32_t idx);
typedef uint32_t (__fastcall *SetTextFn)(void *cs, uint32_t idx, uint64_t element, const char *s);
typedef void (__fastcall *FindPrimFn)(void *cs, uint64_t *id, uint32_t idx, uint64_t element, uint8_t **inst);
typedef void (__fastcall *SetTexFn)(void *cs, uint32_t idx, uint64_t element, const void *tex);
typedef void (__fastcall *SetColorFn)(void *cs, uint32_t idx, uint64_t element, const float *rgb);
typedef void (__fastcall *ShowElFn)(void *cs, uint32_t idx, uint64_t element, uint32_t show);
typedef const char *(__fastcall *ElementTextFn)(void *inst, uint32_t id);

static const Profile *g_prof;
static uint8_t *g_base;
static TickFn g_tick_orig;
static SendFn g_target_orig, g_core_orig;
static int g_brokered;

static int self_read(void *dst, const void *src, size_t n)
{
    __try { memcpy(dst, src, n); return 1; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
static uint64_t (*g_csym64)(const char *s) = evrmod_csym64;
static int (*g_read)(void *dst, const void *src, size_t n) = self_read;
static HANDLE g_log = INVALID_HANDLE_VALUE;
static SRWLOCK g_log_lock = SRWLOCK_INIT;
#define LOG_CAP (256 * 1024)   /* past this radmenu.log moves to radmenu.log.1 and a new one starts */
static char g_log_path[MAX_PATH];
static LONGLONG g_log_size;
static void log_open(DWORD disp)
{
    LARGE_INTEGER sz;
    g_log = CreateFileA(g_log_path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, disp, FILE_ATTRIBUTE_NORMAL, NULL);
    g_log_size = g_log != INVALID_HANDLE_VALUE && GetFileSizeEx(g_log, &sz) ? sz.QuadPart : 0;
}

static void rlog(const char *fmt, ...)
{
    char buf[1024];
    SYSTEMTIME t;
    GetLocalTime(&t);
    int n = _snprintf_s(buf, sizeof buf, _TRUNCATE, "%02u:%02u:%02u.%03u ", t.wHour, t.wMinute, t.wSecond,
                        t.wMilliseconds);
    va_list ap;
    va_start(ap, fmt);
    int m = _vsnprintf_s(buf + n, sizeof buf - n - 2, _TRUNCATE, fmt, ap);
    va_end(ap);
    n += m < 0 ? (int)strlen(buf + n) : m;
    buf[n++] = '\r';
    buf[n++] = '\n';
    if (g_log == INVALID_HANDLE_VALUE) return;
    AcquireSRWLockExclusive(&g_log_lock);
    if (g_log_size + n > LOG_CAP) {
        char old[MAX_PATH + 2];
        CloseHandle(g_log);
        _snprintf_s(old, sizeof old, _TRUNCATE, "%s.1", g_log_path);
        MoveFileExA(g_log_path, old, MOVEFILE_REPLACE_EXISTING);
        log_open(CREATE_ALWAYS);
    }
    DWORD w = 0;
    if (g_log != INVALID_HANDLE_VALUE && WriteFile(g_log, buf, (DWORD)n, &w, NULL)) g_log_size += w;
    ReleaseSRWLockExclusive(&g_log_lock);
}

/* ---- options ---- */
static int g_period_ms = 5000, g_sprite = 1, g_dots_mode = 0, g_fixed = -1, g_hover_pause = 1, g_hover_resume_ms = 2000;
static char g_dot_glyph[8] = "o";
static int g_friends = 1, g_fr_period_ms = 5000;   /* FRIENDS rows */
/* Report Player v1 options and state shared with report_flow.h (tentative definitions; the header repeats them) */
static int g_rp_on = -1, g_rp_ready;   /* report: -1 auto (default auto), 0 off, 1 any level */
static uint64_t g_rp_tile_uid = 0xaa15d7d322666e42ull,   /* = RP_TILE_UID_DEFAULT (report_flow.h) */
                g_rp_card_event, g_rp_card_uid, g_rp_card_el, S_STATE_MGR;
static char g_rp_glyph_s[64] = "cancel_button";   /* report_card_glyph (empty = off); symbol at report_start */
static int report_option(const char *k, const char *v);   /* report keys (report_flow.h) */
static int rp_parse_report(const char *v);   /* the shared `report=` parser (report_flow.h) */
static char g_user[64];                         /* the logged-in player, from the metric lines (or radmenu.ini) */
/*: tab_highlight (1 = SetColor fill on the active rail tab's Background, 2 = ShowUIElement(highlight) fallback,
 * 0 = the old "> X <" text marker), square_dot (1 = colour the dot_box element, 0 = the dot_glyph text),
 * help_season (1 = forward the HELP SEASON PASS press to the page-5 season route), season_p24_first (1 = show p24
 * first when page 5 is off, send on the next tick), season_comp (0x.. component of that send).
 *: help_safezone (1 = evt_ffa_help_safezone_clicked shows p48), help_safezone_card (1 = also an evt_card_clicked
 * while the pointer is on the SAFE ZONE card and HELP shows, the fallback if the card's button component stays silent). */
static int g_tab_hl = 1, g_square_dot = 1, g_help_season = 1, g_season_p24 = 0, g_help_safe = 1, g_help_safe_card = 1;
static uint64_t g_season_comp = SEASON_COMP;
static volatile LONG g_color_ok;                /* the setter bytes matched (and no fault since) */
static volatile LONG g_text_ok;                 /*: the ElementText bytes matched (and no fault since) */
static int g_footer_copy = 1, g_hide_empty = 1; /* fix 2 / fix 4 (radmenu.ini footer_copy, hide_empty_rows) */
static int g_enabled = 1, g_carousel = 1, g_tab_title_on = 1;   /* master switch (0 = no hook at all), carousel, banner */
static int g_ftue_mode = 1;     /* ftue = off (0) | auto (1, default: the picker's NPE page) | force (2, gate test) */
static int g_combat_lock;       /* combat_lock = off (0) | purchase (1) | hardware (2): test face on the p36 combat card */
static const char *const g_lo_word[3] = { "off", "auto", "force" };
static int g_lo_mode = 1;             /* lockout_lock = off (0) | auto (1, default; never locks a stock exe) | force (2) */
static int g_lo_force_sec = 900;      /* lockout_force_sec */
static char g_lo_prefix[32] = "LOCKOUT";   /* lockout_text: prim 7 = "<text> hh:mm:ss" ("" = the time only) */
/* QUIT WARNING options (default on) (radmenu.ini quit_warning, quit_warning_text, quit_warning_element, quit_warning_actor) */
/* The warning element is one unwrapped line (~60 chars at its size on PC): Coffey's full copy (
 * "Quitting early affects the game for everyone. Additional quits will incur a penalty. Are you sure you want to
 * quit?") clipped at both ends on a device run; quit_warning_text can still set it. */
#define QW_TEXT_DEFAULT "Quitting early affects everyone and adds a penalty. Quit?"
static int g_qw_on = 1;   /* quit_warning, default on */
static char g_qw_text[256] = QW_TEXT_DEFAULT, g_qw_el_s[96], g_qw_actor_s[96];
static int opt_word(const char *v, const char *const *words, int n, int def)
{
    for (int i = 0; i < n; i++)
        if (!_stricmp(v, words[i])) return i;
    return def;
}

static void local_path(char *path, size_t cap, const char *leaf)
{
    DWORD n = GetModuleFileNameA(NULL, path, (DWORD)cap);
    char *slash = n ? strrchr(path, '\\') : NULL;
    if (!slash) { path[0] = 0; return; }
    strcpy_s(slash + 1, cap - (slash + 1 - path), "_local");
    CreateDirectoryA(path, NULL);
    strcat_s(path, cap, "\\");
    strcat_s(path, cap, leaf);
}

static void read_options(void)
{
    char ini[MAX_PATH];
    local_path(ini, sizeof ini, "radmenu.ini");
    if (!ini[0]) return;
    g_enabled = GetPrivateProfileIntA("radmenu", "enabled", 1, ini) != 0;
    g_carousel = GetPrivateProfileIntA("radmenu", "carousel", 1, ini) != 0;
    g_tab_title_on = GetPrivateProfileIntA("radmenu", "tab_title", 1, ini) != 0;
    {   /* unknown keys: logged and ignored (as the Quest build) */
        static const char *const known[] = { "enabled", "carousel", "tab_title", "period_ms", "sprite", "dots", "fixed",
            "hover_pause", "hover_resume_ms", "dot_glyph", "friends", "friends_period_ms", "user_id", "tab_highlight",
            "square_dot", "help_season", "season_p24_first", "help_safezone", "help_safezone_card", "footer_copy",
            "hide_empty_rows", "season_comp", "report", "report_tile_uid", "report_card_event", "report_card_uid",
            "report_card_element", "report_card_glyph", "quit_warning", "quit_warning_text", "quit_warning_element",
            "quit_warning_actor", "ftue", "combat_lock", "lockout_lock", "lockout_force_sec", "lockout_text" };
        static char sec[8192];
        DWORD got = GetPrivateProfileSectionA("radmenu", sec, sizeof sec, ini);
        for (char *k = sec; got && *k; k += strlen(k) + 1) {
            char key[64];
            size_t kl = strcspn(k, "=");
            while (kl && (k[kl - 1] == ' ' || k[kl - 1] == '\t')) kl--;
            _snprintf_s(key, sizeof key, _TRUNCATE, "%.*s", (int)kl, k);
            const char *val = k[strcspn(k, "=")] ? k + strcspn(k, "=") + 1 : "";
            while (*val == ' ' || *val == '\t') val++;
            char vbuf[128];
            _snprintf_s(vbuf, sizeof vbuf, _TRUNCATE, "%s", val);
            for (size_t t = strlen(vbuf); t && (vbuf[t - 1] == ' ' || vbuf[t - 1] == '\t'); ) vbuf[--t] = 0;
            int ok = report_option(key, vbuf);
            for (size_t i = 0; !ok && i < sizeof known / sizeof known[0]; i++) ok = !_stricmp(key, known[i]);
            if (!ok && key[0] && key[0] != ';' && key[0] != '#') rlog("options: unknown key \"%s\" ignored", key);
        }
    }
    g_period_ms = GetPrivateProfileIntA("radmenu", "period_ms", 5000, ini);
    if (g_period_ms < 1000) g_period_ms = 1000;
    g_sprite = GetPrivateProfileIntA("radmenu", "sprite", 1, ini) != 0;
    g_dots_mode = GetPrivateProfileIntA("radmenu", "dots", 0, ini);
    g_fixed = (int)GetPrivateProfileIntA("radmenu", "fixed", -1, ini);
    if (g_fixed >= NSLIDE) g_fixed = -1;
    g_hover_pause = GetPrivateProfileIntA("radmenu", "hover_pause", 1, ini) != 0;
    g_hover_resume_ms = GetPrivateProfileIntA("radmenu", "hover_resume_ms", 2000, ini);
    if (g_hover_resume_ms < 0 || g_hover_resume_ms > 60000) g_hover_resume_ms = 2000;
    GetPrivateProfileStringA("radmenu", "dot_glyph", "o", g_dot_glyph, sizeof g_dot_glyph, ini);
    if (!g_dot_glyph[0]) strcpy_s(g_dot_glyph, sizeof g_dot_glyph, "o");
    g_friends = GetPrivateProfileIntA("radmenu", "friends", 1, ini) != 0;
    g_fr_period_ms = GetPrivateProfileIntA("radmenu", "friends_period_ms", 5000, ini);
    if (g_fr_period_ms < 1000) g_fr_period_ms = 1000;
    GetPrivateProfileStringA("radmenu", "user_id", "", g_user, sizeof g_user, ini);
    g_tab_hl = GetPrivateProfileIntA("radmenu", "tab_highlight", 1, ini);
    g_square_dot = GetPrivateProfileIntA("radmenu", "square_dot", 1, ini) != 0;
    g_help_season = GetPrivateProfileIntA("radmenu", "help_season", 1, ini) != 0;
    g_season_p24 = GetPrivateProfileIntA("radmenu", "season_p24_first", 0, ini) != 0;
    g_help_safe = GetPrivateProfileIntA("radmenu", "help_safezone", 1, ini) != 0;
    g_help_safe_card = GetPrivateProfileIntA("radmenu", "help_safezone_card", 1, ini) != 0;
    g_footer_copy = GetPrivateProfileIntA("radmenu", "footer_copy", 1, ini) != 0;
    g_hide_empty = GetPrivateProfileIntA("radmenu", "hide_empty_rows", 1, ini) != 0;
    {
        static const char *const fw[] = { "off", "auto", "force" }, *const cw[] = { "off", "purchase", "hardware" };
        char v[16];
        GetPrivateProfileStringA("radmenu", "ftue", "auto", v, sizeof v, ini);
        g_ftue_mode = opt_word(v, fw, 3, 1);
        GetPrivateProfileStringA("radmenu", "combat_lock", "off", v, sizeof v, ini);
        g_combat_lock = opt_word(v, cw, 3, 0);
        rlog("options: ftue=%s combat_lock=%s", fw[g_ftue_mode], cw[g_combat_lock]);
        GetPrivateProfileStringA("radmenu", "lockout_lock", "auto", v, sizeof v, ini);
        g_lo_mode = opt_word(v, g_lo_word, 3, 1);
        g_lo_force_sec = GetPrivateProfileIntA("radmenu", "lockout_force_sec", 900, ini);
        if (g_lo_force_sec < 5 || g_lo_force_sec > 86400) g_lo_force_sec = 900;
        GetPrivateProfileStringA("radmenu", "lockout_text", "LOCKOUT", g_lo_prefix, sizeof g_lo_prefix, ini);
        rlog("options: lockout_lock=%s lockout_force_sec=%d lockout_text=\"%s\"", g_lo_word[g_lo_mode], g_lo_force_sec, g_lo_prefix);
    }
    rlog("options: help_safezone=%d help_safezone_card=%d", g_help_safe, g_help_safe_card);
    {   /* Report Player (report_flow.h): auto by default (acts only while our popup marker page is loaded) */
        char v[96];
        GetPrivateProfileStringA("radmenu", "report", "", v, sizeof v, ini);
        g_rp_on = rp_parse_report(v);   /* the one shared parser (report_flow.h), same on Quest */
        GetPrivateProfileStringA("radmenu", "report_tile_uid", "", v, sizeof v, ini);
        if (v[0]) g_rp_tile_uid = _strtoui64(v, NULL, 0);   /* empty: RP_TILE_UID_DEFAULT; 0 = no pause entry */
        GetPrivateProfileStringA("radmenu", "report_card_event", "", v, sizeof v, ini);
        g_rp_card_event = !v[0] ? 0 : (v[0] == '0' && (v[1] == 'x' || v[1] == 'X')) ? _strtoui64(v, NULL, 16) : g_csym64(v);
        GetPrivateProfileStringA("radmenu", "report_card_uid", "", v, sizeof v, ini);
        g_rp_card_uid = v[0] ? _strtoui64(v, NULL, 0) : 0;
        GetPrivateProfileStringA("radmenu", "report_card_element", "", v, sizeof v, ini);
        g_rp_card_el = !v[0] ? 0 : (v[0] == '0' && (v[1] == 'x' || v[1] == 'X')) ? _strtoui64(v, NULL, 16) : g_csym64(v);
        GetPrivateProfileStringA("radmenu", "report_card_glyph", "cancel_button", g_rp_glyph_s, sizeof g_rp_glyph_s, ini);
        rlog("options: report=%d report_tile_uid=%016llx report_card_event=%016llx report_card_uid=%016llx",
             g_rp_on, g_rp_tile_uid, g_rp_card_event, g_rp_card_uid);
        rlog("options: report_card_glyph=%s", g_rp_glyph_s[0] ? g_rp_glyph_s : "(off)");
    }
    {   /* QUIT WARNING: on by default; names resolve at qw_start */
        g_qw_on = GetPrivateProfileIntA("radmenu", "quit_warning", 1, ini) != 0;
        GetPrivateProfileStringA("radmenu", "quit_warning_text", QW_TEXT_DEFAULT, g_qw_text, sizeof g_qw_text, ini);
        if (!g_qw_text[0]) strcpy_s(g_qw_text, sizeof g_qw_text, QW_TEXT_DEFAULT);
        GetPrivateProfileStringA("radmenu", "quit_warning_element", "", g_qw_el_s, sizeof g_qw_el_s, ini);
        GetPrivateProfileStringA("radmenu", "quit_warning_actor", "", g_qw_actor_s, sizeof g_qw_actor_s, ini);
        rlog("options: quit_warning=%d quit_warning_element=%s quit_warning_actor=%s quit_warning_text=\"%s\"", g_qw_on,
             g_qw_el_s[0] ? g_qw_el_s : "(default)", g_qw_actor_s[0] ? g_qw_actor_s : "(scan)", g_qw_text);
    }
    {
        char comp[24];
        GetPrivateProfileStringA("radmenu", "season_comp", "", comp, sizeof comp, ini);
        if (comp[0]) g_season_comp = _strtoui64(comp, NULL, 0);
    }
    rlog("options: tab_highlight=%d square_dot=%d help_season=%d season_p24_first=%d season_comp=%016llx",
         g_tab_hl, g_square_dot, g_help_season, g_season_p24, g_season_comp);
    rlog("options: friends=%d friends_period_ms=%d user_id %s", g_friends, g_fr_period_ms,
         g_user[0] ? "set (ini)" : "from the metric, else the client log");
    rlog("options: enabled=%d carousel=%d tab_title=%d period_ms=%d sprite=%d dots=%d dot_glyph=\"%s\" hover_pause=%d "
         "hover_resume_ms=%d fixed=%d (%s)", g_enabled, g_carousel, g_tab_title_on, g_period_ms, g_sprite, g_dots_mode, g_dot_glyph, g_hover_pause, g_hover_resume_ms, g_fixed, ini);
}

/* ---- engine calls (game thread only) ---- */

/* the mnu_master gamespace of this game, or NULL */
static uint8_t *menu_gs(uint8_t *game)
{
    uint64_t n = *(uint64_t *)(game + g_prof->o_gs_count);
    uint8_t *list = *(uint8_t **)(game + g_prof->o_gs_list);
    for (uint64_t i = 0; list && n <= 64 && i < n; i++) {
        uint8_t *gs = *(uint8_t **)(list + i * 16 + 8);
        if (gs && *(void **)(gs + g_prof->o_gs_game) == game && *(uint64_t *)(gs + g_prof->o_gs_id) == LEVEL_MNU_MASTER)
            return gs;
    }
    return NULL;
}

static uint8_t *menu_actor(uint8_t *game, uint64_t actor, uint64_t *idx)
{
    uint8_t *gs = menu_gs(game);
    if (gs) {
        uint8_t *tbl = *(uint8_t **)(gs + g_prof->o_gs_actors);
        uint64_t *names = tbl ? *(uint64_t **)(tbl + g_prof->o_at_names) : NULL;
        uint64_t count = tbl ? *(uint64_t *)(tbl + g_prof->o_at_count) : 0;
        for (uint64_t k = 0; names && count <= 0xfffe && k < count; k++)
            if (names[k] == actor) { *idx = k; return gs; }
        return NULL;
    }
    return NULL;
}

static int valid(Handle *h) { return h->cs && ((ValidFn)(*(void ***)h->cs)[g_prof->vt_cs_valid / 8])(h->cs, h->idx); }

/* Shows a mnu_master page as R15UIPage2EnablePageNode does (rad-links show_page). 0 = done. */
static int show_page(uint8_t *game, uint64_t owner)
{
    uint64_t k;
    uint8_t *gs = menu_actor(game, owner, &k);
    Handle h = { NULL, 0 };
    if (!gs) return 1;
    ((FindFn)(g_base + g_prof->rva_find_page_cs))(&h, gs, 0x0000ffffffff0000ull | k, SYM_NONE);
    if (!valid(&h)) return 1;
    ((GameFn)(g_base + g_prof->rva_invoke_set))(game);
    ((EnablePageFn)(g_base + g_prof->rva_enable_page))(h.cs, h.idx);
    ((GameFn)(g_base + g_prof->rva_invoke_clear))(game);
    return 0;
}

static int canvas_of(uint8_t *game, uint64_t actor, Handle *c)
{
    uint64_t k;
    uint8_t *gs = menu_actor(game, actor, &k);
    c->cs = NULL;
    if (!gs) return 0;
    ((FindFn)(g_base + g_prof->rva_find_canvas))(c, gs, 0x0000ffffffff0000ull | k, SYM_CANVASUI);
    return valid(c);
}

static int canvas_text(uint8_t *game, uint64_t actor, uint64_t element, const char *s)
{
    Handle c;
    return canvas_of(game, actor, &c)
        && ((SetTextFn)(g_base + g_prof->rva_canvas_set_text))(c.cs, c.idx, element, s) != 0;
}

/* The texture a sprite element draws now (its instance's type-3 texture slot), or NULL. */
static const void *sprite_tex(uint8_t *game, uint64_t actor, uint64_t element)
{
    Handle c;
    uint64_t id = ~0ull;
    uint8_t *inst = NULL, *prims = NULL, **slots = NULL;
    uint32_t type = 0, slot = 0;
    const void *tex = NULL;
    if (!canvas_of(game, actor, &c)) return NULL;
    ((FindPrimFn)(g_base + g_prof->rva_find_prim))(c.cs, &id, c.idx, element, &inst);
    if ((uint32_t)id == 0xffffffffu || !inst) return NULL;
    if (!g_read(&prims, inst + g_prof->o_inst_prims, sizeof prims) || !prims) return NULL;
    uint8_t *prim = prims + (uint64_t)(uint32_t)id * g_prof->prim_stride;
    if (!g_read(&type, prim + g_prof->o_prim_type, 4) || type != 3) return NULL;
    if (!g_read(&slot, prim + g_prof->o_prim_slot, 4) || slot > 64) return NULL;
    if (!g_read(&slots, inst + g_prof->o_inst_sprite_tex, sizeof slots) || !slots) return NULL;
    return g_read(&tex, slots + slot, sizeof tex) ? tex : NULL;
}

/* tex NULL = clear the override: the element draws the canvas's own sprite again (the setter stores the pointer as
 * given, no NULL check, and marks the instance dirty: 0x727ab3..0x727ac1, disasm-only; that it redraws is inferred). */
static int set_tex(uint8_t *game, uint64_t actor, uint64_t element, const void *tex)
{
    Handle c;
    if (!canvas_of(game, actor, &c)) return 0;
    ((SetTexFn)(g_base + g_prof->rva_set_override_tex))(c.cs, c.idx, element, tex);
    return 1;
}

/*: 1 = the canvas instance has an element named `element` (FindPrimitiveID, as sprite_tex). The setters are only
 * called on a valid canvas handle and an element that exists, inside __try; a fault turns the chrome off. */
static int elem_found(Handle *c, uint64_t element)
{
    uint64_t id = ~0ull;
    uint8_t *inst = NULL;
    ((FindPrimFn)(g_base + g_prof->rva_find_prim))(c->cs, &id, c->idx, element, &inst);
    return (uint32_t)id != 0xffffffffu && inst != NULL;
}

static int canvas_color(uint8_t *game, uint64_t actor, uint64_t element, const float rgb[3])
{
    Handle c;
    if (!g_color_ok || !canvas_of(game, actor, &c) || !elem_found(&c, element)) return 0;
    __try {
        ((SetColorFn)(g_base + g_prof->rva_set_color))(c.cs, c.idx, element, rgb);
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_color_ok, 0);
        rlog("fault in SetColor (actor %016llx element %016llx): colours off", actor, element);
        return 0;
    }
}

static int canvas_show(uint8_t *game, uint64_t actor, uint64_t element, uint32_t show)
{
    Handle c;
    if (!g_color_ok || !canvas_of(game, actor, &c) || !elem_found(&c, element)) return 0;
    __try {
        ((ShowElFn)(g_base + g_prof->rva_show_el))(c.cs, c.idx, element, show);
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_color_ok, 0);
        rlog("fault in ShowUIElement (actor %016llx element %016llx): colours off", actor, element);
        return 0;
    }
}

/*: an element's current text (ElementText on the instance FindPrimitiveID returns) copied into out; 0 = none. */
static int canvas_get_text(uint8_t *game, uint64_t actor, uint64_t element, char *out, size_t cap)
{
    Handle c;
    uint64_t id = ~0ull;
    uint8_t *inst = NULL;
    out[0] = 0;
    if (!g_text_ok || !canvas_of(game, actor, &c)) return 0;
    ((FindPrimFn)(g_base + g_prof->rva_find_prim))(c.cs, &id, c.idx, element, &inst);
    if ((uint32_t)id == 0xffffffffu || !inst) return 0;
    __try {
        const char *s = ((ElementTextFn)(g_base + g_prof->rva_element_text))(inst, (uint32_t)id);
        size_t n = 0;
        for (; s && n + 1 < cap; n++) {
            char ch;
            if (!g_read(&ch, s + n, 1) || !ch) break;
            out[n] = ch;
        }
        out[n] = 0;
        return s != NULL;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_text_ok, 0);
        rlog("fault in ElementText (actor %016llx element %016llx): footer copy off", actor, element);
        return 0;
    }
}

/* ---- fix 2: HELP / SETTINGS footer strings = ONLINE's ----
 * The per-tab copies (status ffa_status_help / _settings, version ffa_radv_help / _settings) keep their canvas
 * literals: the cloned scripts do not fill them (review-2 N2). The ONLINE originals are read with ElementText and a
 * string that differs from its canvas literal is cached and written into both copies while HELP or SETTINGS shows.
 * Element names are the canvases' own (ui_menu_rad_version prims 0-2, ui_mnu_service_status_text_only prims 0-1). */
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
static ULONGLONG g_footer_next;

static void apply_footer(uint8_t *g, ULONGLONG now, int tab)
{
    if (!g_footer_copy || !g_text_ok || now < g_footer_next) return;
    g_footer_next = now + 1000;
    for (int i = 0; i < 5; i++) {
        char cur[128];
        if (canvas_get_text(g, g_footer[i].src, g_footer[i].element, cur, sizeof cur) && cur[0]
            && strcmp(cur, g_footer[i].literal) && strcmp(cur, g_footer_val[i])) {
            strcpy_s(g_footer_val[i], sizeof g_footer_val[i], cur);
            rlog("FOOTER el %d: ONLINE reads \"%s\" (cached)", i, cur);
        }
    }
    if (tab != 1 && tab != 2) return;
    for (int i = 0; i < 5; i++) {
        if (!g_footer_val[i][0]) continue;
        int ok = canvas_text(g, g_footer[i].dst[0], g_footer[i].element, g_footer_val[i])
               + canvas_text(g, g_footer[i].dst[1], g_footer[i].element, g_footer_val[i]);
        if (strcmp(g_footer_logged[i], g_footer_val[i])) {
            rlog("FOOTER el %d: \"%s\" -> HELP / SETTINGS copies (%d of 2 written)", i, g_footer_val[i], ok);
            strcpy_s(g_footer_logged[i], sizeof g_footer_logged[i], g_footer_val[i]);
        }
    }
}

/* ---- carousel state ---- */
static volatile LONG g_p24_on, g_p5_state = -1;     /* page states from the global page events (-1 unknown) */
static volatile LONG g_reset, g_go_p42, g_go_p48, g_shown;  /* g_go_p48: SAFE ZONE route queued */
static volatile LONG g_safe_ptr;                    /*: pointer on the SAFE ZONE card (bit 0 ray, bit 1 hover) */    /* g_shown = the slide the tile shows now */
static volatile LONG g_jump = -1;                   /* a dot press: the slide to show next tick (-1 none) */
static volatile LONG g_ptr_mask;                    /* pointer on: bits 0-3 ray news/dot0..2, bits 4-7 button hover */
static int g_paused;
static int g_slide, g_active, g_text_fail;
static volatile LONG g_p58_on, g_p30_on;
static int g_tab = -2;                 /* 0 online, 1 settings, 2 help, -1 none */
static ULONGLONG g_title_next, g_color_next;
static ULONGLONG g_next, g_reassert;
static const void *g_news_tex, *g_last_set;          /* the news tile's own texture (NULL = its own sprite), our last write */
static int g_news_known;
/* tree gate: 1 = the minted marker ffa_news_dot0 is in the mnu_master gamespace (our menu tree is loaded). Set by the
 * tick at every page-24 enable and on a gamespace change; read by the click hook and the friends worker. 0 = the
 * stock menu: no write to any canvas, no click rewrite, no /friends poll (page tracking only). */
static volatile LONG g_tree, g_fr_tree, g_fr_kick;
static uint8_t *g_tree_gs;
/* HELP SEASON PASS: the press (ev[1], ev[2]) and the last real event the page-5 owner got (cache, 5 qwords) */
static volatile LONG g_go_season;                   /* 1 = send on the next tick, 2 = p24 shown, send now */
static uint64_t g_hs_ev[5], g_p5_ev[5];
static volatile LONG g_p5_have;
static uint32_t g_p5_flags;

/* ---- FRIENDS rows (tree ffa_w7a): filled from the host's GET /friends?format=tsv ----
 * Rows 0xecce8089fb0c0501..05 (canvas ffa_friend_row v2): `text` = name, `codetext` = cyan status (online / in game),
 * `codetext_off` = grey status (offline); a row press sends evt_ffa_friend_row<n>_clicked. A worker thread polls the
 * services host (apiservice_host of _local\config.json, as rad-links) every friends_period_ms while page 24 shows;
 * the tick writes the rows (game thread only). Host unreachable -> the build-time texts stay. */
#define NFRIEND 5
#define ACTOR_FRIEND0 0xecce8089fb0c0501ull
static const uint64_t EV_FRIEND[NFRIEND] = { 0xe2e33c85cad96ff8ull, 0x9f0856702361da46ull, 0xb45170237bf6492cull,
                                             0x64de839bf010b13aull, 0x4f87a5c8a8872250ull };
static uint64_t SYM_CODETEXT, SYM_CODETEXT_OFF;
typedef struct { char name[48], status[16], text[32], session[48], gametype[64]; } Friend;   /* TSV cols 0..4 */
static SRWLOCK g_fr_lock = SRWLOCK_INIT;
static Friend g_fr[NFRIEND];
static int g_fr_n, g_fr_have;                 /* rows in the snapshot; 1 once the host answered */
static volatile LONG g_fr_seq, g_fr_press = -1;
static LONG g_fr_applied = -1;
static char g_fr_written[NFRIEND][96];        /* last logged row state (tick only) */
static ULONGLONG g_fr_flash[NFRIEND], g_fr_reassert;
static const char *g_fr_flash_msg[NFRIEND];   /* the 3 s status text: NOTHING TO JOIN or JOINING */
static SRWLOCK g_user_lock = SRWLOCK_INIT;
static HANDLE g_fr_wake;
static wchar_t g_svc_host[256];
static INTERNET_PORT g_svc_port;
static HINTERNET g_http;

static int fr_online(const Friend *f) { return strcmp(f->status, "offline") != 0; }
/* fix 4: an empty row hides its slab (prims 0 + 2, boxes) and avatar group (prim 3) by ShowUIElement; the
 * text elements are written "" as before. Element names of ui_mnu_settings_invite_regenerate (ffa_friend_row). */
static const uint64_t FR_SLAB_EL[3] = { 0x88d3c40b6cbbeb5eull, 0x4eca5fb423c99908ull, 0x4fab74ca1111beb2ull };
static int g_fr_shown[NFRIEND] = { -1, -1, -1, -1, -1 };

/* rad-links read_services_host, in substance. Re-read at each page-24 enable; *changed = 1 when host or port moved
 * (logged then, and on the first read). */
static int read_services_host(int *changed)
{
    wchar_t host[256];
    *changed = 0;
    static const char *rel[] = { "_local\\config.json", "..\\..\\_local\\config.json" };
    char path[MAX_PATH];
    HANDLE f = INVALID_HANDLE_VALUE;
    for (int i = 0; i < 2 && f == INVALID_HANDLE_VALUE; i++) {
        DWORD n = GetModuleFileNameA(NULL, path, sizeof path);
        char *slash = n ? strrchr(path, '\\') : NULL;
        if (!slash) return 0;
        strcpy_s(slash + 1, sizeof path - (slash + 1 - path), rel[i]);
        f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    }
    if (f == INVALID_HANDLE_VALUE) {
        if (!g_svc_host[0]) rlog("friends: no _local\\config.json beside the exe or two levels up");
        return 0;
    }
    char buf[8192];
    DWORD got = 0;
    ReadFile(f, buf, sizeof buf - 1, &got, NULL);
    CloseHandle(f);
    buf[got] = 0;
    char *k = strstr(buf, "\"apiservice_host\""), *q1 = k ? strchr(k + 17, '"') : NULL, *q2 = q1 ? strchr(q1 + 1, '"') : NULL;
    if (!q2) { if (!g_svc_host[0]) rlog("friends: %s has no apiservice_host", path); return 0; }
    wchar_t url[512];
    int len = MultiByteToWideChar(CP_UTF8, 0, q1 + 1, (int)(q2 - q1 - 1), url, 511);
    url[len] = 0;
    URL_COMPONENTS uc = { sizeof uc };
    uc.lpszHostName = host;
    uc.dwHostNameLength = sizeof host / sizeof host[0];
    if (!WinHttpCrackUrl(url, 0, 0, &uc)) { if (!g_svc_host[0]) rlog("friends: cannot parse apiservice_host"); return 0; }
    if (wcscmp(host, g_svc_host) || uc.nPort != g_svc_port) {
        *changed = g_svc_host[0] != 0;
        wcscpy_s(g_svc_host, sizeof g_svc_host / sizeof g_svc_host[0], host);
        g_svc_port = uc.nPort;
        rlog("friends: services %ls:%u (from apiservice_host)%s", g_svc_host, g_svc_port, *changed ? ": host changed" : "");
    }
    return 1;
}

/* One GET; returns the HTTP status (0 = transport error) and the body in out (rad-links http, GET only). */
static int http_get(const char *path, char *out, size_t cap)
{
    out[0] = 0;
    wchar_t wpath[256];
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, 256);
    HINTERNET c = WinHttpConnect(g_http, g_svc_host, g_svc_port, 0), r = NULL;
    int status = 0;
    if (c) r = WinHttpOpenRequest(c, L"GET", wpath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (r && WinHttpSendRequest(r, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
        && WinHttpReceiveResponse(r, NULL)) {
        DWORD st = 0, sz = sizeof st;
        WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &st, &sz, NULL);
        status = (int)st;
        size_t got = 0;
        DWORD rd = 0;
        while (got < cap - 1 && WinHttpReadData(r, out + got, (DWORD)(cap - 1 - got), &rd) && rd) got += rd;
        out[got] = 0;
    } else {
        _snprintf_s(out, cap, _TRUNCATE, "winhttp error %lu", GetLastError());
    }
    if (r) WinHttpCloseHandle(r);
    if (c) WinHttpCloseHandle(c);
    return status;
}

/* Copies field `col` (tab-separated) of line `ln` into out, ASCII-uppercased when up. */
static void tsv_field(const char *ln, int col, char *out, size_t cap, int up)
{
    for (; col > 0 && ln; col--) { ln = strchr(ln, '\t'); if (ln) ln++; }
    size_t n = 0;
    for (; ln && *ln && *ln != '\t' && *ln != '\n' && *ln != '\r' && n + 1 < cap; ln++)
        out[n++] = (char)(up && *ln >= 'a' && *ln <= 'z' ? *ln - 32 : (*ln < 0x20 || *ln > 0x7e) ? '?' : *ln);
    out[n] = 0;
}

/* The logged-in player's id from this process's own client log (PC logs no click metric at login). Same
 * source as rad-links' log_user (own copy: no cross-DLL dependency). The client writes
 * <root>\\_local\\r14logs\\[r14(client)]-[date]_[time]_<pid>.log with "[NSUSER] Creating user <id>" and
 * "[LOGIN] Logging in <id>: {...}" (device-observed, 6323983201049540). Last match in the first 1 MiB wins.
 * Friends worker thread. 1 = found. */
static int log_user(char *out, size_t cap)
{
    static const char *rel[] = { "..\\..\\_local\\r14logs\\", "_local\\r14logs\\" };
    static const char *keys[] = { "[NSUSER] Creating user ", "[LOGIN] Logging in " };
    char dir[MAX_PATH], pat[MAX_PATH], best[MAX_PATH] = "";
    FILETIME bt = { 0, 0 };
    for (int i = 0; i < 2; i++) {
        DWORD n = GetModuleFileNameA(NULL, dir, sizeof dir);
        char *slash = n ? strrchr(dir, '\\') : NULL;
        if (!slash) return 0;
        strcpy_s(slash + 1, sizeof dir - (slash + 1 - dir), rel[i]);
        _snprintf_s(pat, sizeof pat, _TRUNCATE, "%s*_%lu.log", dir, GetCurrentProcessId());
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(pat, &fd);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if (CompareFileTime(&fd.ftLastWriteTime, &bt) > 0) {
                bt = fd.ftLastWriteTime;
                _snprintf_s(best, sizeof best, _TRUNCATE, "%s%s", dir, fd.cFileName);
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    if (!best[0]) return 0;
    HANDLE f = CreateFileA(best, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                           OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return 0;
    static char buf[1 << 20];
    DWORD got = 0;
    ReadFile(f, buf, sizeof buf - 1, &got, NULL);
    CloseHandle(f);
    buf[got] = 0;
    for (int i = 0; i < 2; i++) {
        size_t kl = strlen(keys[i]);
        int found = 0;
        for (const char *p = buf; (p = strstr(p, keys[i])) != NULL; p += kl) {
            char id[64];
            size_t n = 0;
            for (const char *q = p + kl; n + 1 < sizeof id && (isalnum((unsigned char)*q) || *q == '-' || *q == '_'); q++)
                id[n++] = *q;
            id[n] = 0;
            if (n >= 4 && n < cap) { strcpy_s(out, cap, id); found = 1; }
        }
        if (found) return 1;
    }
    return 0;
}

/* 1 = a status after which the worker backs off (300 s, doubling to 30 min) */
static int backoff_status(int st) { return st == 0 || st == 400 || st == 404 || st == 405 || st == 409; }

/* One GET /friends; returns the HTTP status (0 = transport error). next_backoff_s is printed on the status-change line
 * when this status starts (or keeps) a backoff. No user id, name or session id is logged. */
static int friends_poll(int *last_status, unsigned next_backoff_s)
{
    char user[64], path[160], body[8192];
    AcquireSRWLockShared(&g_user_lock);
    strcpy_s(user, sizeof user, g_user);
    ReleaseSRWLockShared(&g_user_lock);
    if (!user[0]) {   /* no metric / ini id: the client's own log (PC) */
        char lu[64] = "";
        if (log_user(lu, sizeof lu)) {
            AcquireSRWLockExclusive(&g_user_lock);
            if (!g_user[0]) strcpy_s(g_user, sizeof g_user, lu);
            strcpy_s(user, sizeof user, g_user);
            ReleaseSRWLockExclusive(&g_user_lock);
            rlog("friends: user_id found (from the client log)");
        } else {
            static int said;
            if (!said++) rlog("friends: no user_id yet (no metric line, no login line in the client log); asking without");
        }
    }
    if (user[0]) _snprintf_s(path, sizeof path, _TRUNCATE, "/friends?user_id=%s&format=tsv", user);
    else strcpy_s(path, sizeof path, "/friends?format=tsv");   /* host: the one user logged in from this address */
    int st = http_get(path, body, sizeof body);
    if (st != 200) {
        if (st != *last_status) {
            char b[120];
            _snprintf_s(b, sizeof b, _TRUNCATE, "%s", body);
            for (char *q = b; *q; q++) if (*q == '\r' || *q == '\n') *q = ' ';
            char bo[48] = "";
            if (backoff_status(st)) _snprintf_s(bo, sizeof bo, _TRUNCATE, "; backing off %u s", next_backoff_s);
            rlog("friends: GET /friends%s -> %d %s (rows keep their texts)%s", user[0] ? " (user_id)" : "", st, b, bo);
        }
        *last_status = st;
        return st;
    }
    Friend fr[NFRIEND];
    int n = 0;
    memset(fr, 0, sizeof fr);
    for (const char *ln = body; *ln && n < NFRIEND; ) {
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
    AcquireSRWLockExclusive(&g_fr_lock);
    int same = g_fr_have && n == g_fr_n && !memcmp(fr, g_fr, sizeof fr);
    memcpy(g_fr, fr, sizeof fr);
    g_fr_n = n;
    g_fr_have = 1;
    ReleaseSRWLockExclusive(&g_fr_lock);
    if (!same || *last_status != 200) {
        rlog("friends: GET /friends%s -> 200, %d friend(s)%s", user[0] ? " (user_id)" : "", n, same ? "" : " (changed)");
        InterlockedIncrement(&g_fr_seq);
    }
    *last_status = 200;
    return 200;
}

static DWORD WINAPI friends_worker(LPVOID unused)
{
    (void)unused;
    int last = -1, changed;
    unsigned backoff_s = 0;   /* 0, else 300 s doubling up to 30 min after 400/404/405/409 or a transport error */
    ULONGLONG next_ok = 0;
    /* here, not in friends_start: that may run inside DllMain (loader lock) */
    if (!read_services_host(&changed)) { rlog("friends: no services host: rows keep their build-time texts"); return 0; }
    g_http = WinHttpOpen(L"rad-menu/0.1", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!g_http) { rlog("friends: WinHttpOpen failed (%lu)", GetLastError()); return 0; }
    WinHttpSetTimeouts(g_http, 3000, 3000, 3000, 3000);
    rlog("friends: polling every %d ms while page 24 shows and the friend rows are in the tree", g_fr_period_ms);
    while (WaitForSingleObject(g_fr_wake, (DWORD)g_fr_period_ms) != WAIT_FAILED) {
        if (InterlockedExchange(&g_fr_kick, 0) && read_services_host(&changed) && changed) {
            if (backoff_s) rlog("friends: services host changed: backoff lifted");
            backoff_s = 0;
            next_ok = 0;
            last = -1;
        }
        if (!g_p24_on || !g_fr_tree || GetTickCount64() < next_ok) continue;
        unsigned nb = backoff_s ? (backoff_s * 2 > 1800 ? 1800 : backoff_s * 2) : 300;
        int st = friends_poll(&last, nb);
        if (st == 200) { backoff_s = 0; next_ok = 0; }
        else if (backoff_status(st)) { backoff_s = nb; next_ok = GetTickCount64() + (ULONGLONG)nb * 1000; }
    }
    return 0;
}

static void friends_start(void)
{
    if (!g_friends) { rlog("friends: off (radmenu.ini friends=0)"); return; }
    g_fr_wake = CreateEventA(NULL, FALSE, FALSE, NULL);
    HANDLE t = g_fr_wake ? CreateThread(NULL, 0, friends_worker, NULL, 0, NULL) : NULL;
    if (!t) { rlog("friends: no worker thread (%lu)", GetLastError()); return; }
    CloseHandle(t);
}

/* Any remote-log JSON line: keep its user_id (the logged-in player). Runs on the logging thread; copies only. */
static void metric_body(const char *json, size_t len)
{
    __try {
        if (!json || len == 0 || len > 65536) return;
        static const char key[] = "\"user_id\":\"";
        size_t kl = sizeof key - 1;
        for (size_t i = 0; i + kl < len; i++) {
            if (memcmp(json + i, key, kl)) continue;
            char id[64];
            size_t n = 0;
            for (size_t j = i + kl; j < len && json[j] != '"' && n + 1 < sizeof id; j++) {
                char ch = json[j];
                if (!((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '-' || ch == '_'))
                    return;
                id[n++] = ch;
            }
            id[n] = 0;
            if (!n) return;
            AcquireSRWLockExclusive(&g_user_lock);
            int fresh = strcmp(g_user, id) != 0;
            if (fresh) strcpy_s(g_user, sizeof g_user, id);
            ReleaseSRWLockExclusive(&g_user_lock);
            if (fresh) {
                rlog("friends: user_id found (from a remote-log line)");
                if (g_fr_wake) SetEvent(g_fr_wake);
            }
            return;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

/* 1 = ev[0] is a FRIENDS row press (queued for the tick). */
static int friend_event(uint64_t sym)
{
    for (int i = 0; i < NFRIEND; i++)
        if (sym == EV_FRIEND[i]) { InterlockedExchange(&g_fr_press, i); return 1; }
    return 0;
}

/* "See this friend": rad-links' radlinks_follow(guid, gametype) (0 = queued; rad-links runs its own join path on
 * its tick). Resolved per press, so load order does not matter; none when rad-links is not loaded or too old. */
typedef int (*FollowFn)(const char *guid, const char *gametype);
static int follow_friend(const Friend *f, int *rc)
{
    HMODULE m = GetModuleHandleA("rad-links.dll");               /* rad-links loaded directly */
    if (!m) m = GetModuleHandleA("rad-links-mods.dll");           /* broker route (mods\\ copy) */
    FollowFn fn = m ? (FollowFn)GetProcAddress(m, "radlinks_follow") : NULL;
    *rc = fn ? fn(f->session, f->gametype) : -3;
    return fn != NULL;
}

/* Game thread. Writes the 5 rows from the snapshot (log = on a new snapshot; else a quiet re-assert). */
static void apply_friends(uint8_t *g, ULONGLONG now)
{
    LONG press = InterlockedExchange(&g_fr_press, -1);
    LONG seq = g_fr_seq;
    Friend fr[NFRIEND];
    int n, have;
    AcquireSRWLockShared(&g_fr_lock);
    memcpy(fr, g_fr, sizeof fr);
    n = g_fr_n;
    have = g_fr_have;
    ReleaseSRWLockShared(&g_fr_lock);
    if (press >= n && press < NFRIEND) {   /* an empty row (its slab hidden, or the NO FRIENDS ONLINE note): no flash */
        rlog("FRIEND row %ld pressed: empty row: ignored", press + 1);
        press = -1;
    }
    if (press >= 0 && press < NFRIEND) {
        /* "See this friend": a row with a joinable session (host: in game, or online with an invite of their own)
         * joins it through rad-links and shows JOINING for 3 s; anything else shows NOTHING TO JOIN for 3 s. */
        const char *msg = "NOTHING TO JOIN";
        int rc = 0;
        if (!fr_online(&fr[press]) || !fr[press].session[0]) {
            rlog("FRIEND row %ld pressed: %s session=- -> NOTHING TO JOIN", press + 1, fr[press].status);
        } else if (!follow_friend(&fr[press], &rc)) {
            rlog("FRIEND row %ld pressed: %s -> NOTHING TO JOIN (rad-links.dll or its radlinks_follow export not loaded)",
                 press + 1, fr[press].status);
        } else if (rc) {
            rlog("FRIEND row %ld pressed: %s -> NOTHING TO JOIN (radlinks_follow refused: %d)", press + 1, fr[press].status, rc);
        } else {
            msg = "JOINING";
            rlog("FRIEND row %ld pressed: %s gametype=%s -> JOINING (radlinks_follow queued)", press + 1, fr[press].status,
                 fr[press].gametype[0] ? fr[press].gametype : "-1");   /* no name or session id in the log */
        }
        g_fr_flash_msg[press] = msg;
        g_fr_flash[press] = now + 3000;
        g_fr_reassert = 0;
    }
    if (!have) return;                          /* host never answered: leave the build-time texts */
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
        if (g_hide_empty && g_color_ok) {
            int show = !(i >= n && !(n == 0 && i == 0));
            int w = 0;
            for (int e = 0; e < 3; e++) w += canvas_show(g, actor, FR_SLAB_EL[e], (uint32_t)show);
            if (show != g_fr_shown[i]) {
                rlog("friends: row %d slab %s (%d of 3 element writes)", i + 1, show ? "shown" : "HIDDEN (empty row)", w);
                g_fr_shown[i] = show;
            }
        }
        int ok = canvas_text(g, actor, SYM_TEXT, name);
        ok &= canvas_text(g, actor, SYM_CODETEXT, on);
        ok &= canvas_text(g, actor, SYM_CODETEXT_OFF, off);
        char state[96];
        _snprintf_s(state, sizeof state, _TRUNCATE, "%s|%s|%s%s", name, st, on[0] ? on : off, ok ? "" : "|x");
        if (strcmp(state, g_fr_written[i])) {
            /* no display names in the log: a 16-bit tag tells rows apart (the placeholder row prints as such) */
            char tag[24];
            if (i < n) _snprintf_s(tag, sizeof tag, _TRUNCATE, "name#%04x", (unsigned)(g_csym64(name) & 0xffff));
            else strcpy_s(tag, sizeof tag, name[0] ? "placeholder" : "-");
            rlog("friends: row %d %s %s \"%s\" %s", i + 1, tag, st, on[0] ? on : off, ok ? "written" : "NOT written");
            strcpy_s(g_fr_written[i], sizeof g_fr_written[i], state);
        }
    }
}

static void face_text(int k, char *out, size_t cap)
{
    if (g_dots_mode == 1) _snprintf_s(out, cap, _TRUNCATE, "%s  %s", g_title[k], g_dots[k]);
    else if (g_dots_mode == 2) _snprintf_s(out, cap, _TRUNCATE, "%s\n%s", g_title[k], g_dots[k]);
    else _snprintf_s(out, cap, _TRUNCATE, "%s", g_title[k]);
}

/* Puts slide k on the tile. log = 1 on a slide change (else a quiet re-assert). The detail lines are written once per
 * slide per page-24 visit and again whenever their outcome changes; the steady rotation logs "slide change" only. */
static char g_slide_logged[NSLIDE][256];
static void apply_face(uint8_t *game, int k, int log)
{
    char text[40], sig[256] = "";
    face_text(k, text, sizeof text);
    int ok_text = canvas_text(game, ACTOR_NEWS, SYM_TEXT, text);
    const char *img = "unchanged";
    if (g_sprite) {
        const void *cur = sprite_tex(game, ACTOR_NEWS, SYM_BG), *want = NULL;
        const void *season = sprite_tex(game, ACTOR_SEASON, SYM_BG);
        /* (a): a NULL slot is the tile's own static sprite (no dyn-loaded art yet), not "unknown". Adopt the slot
         * on the first sample, or when the game wrote it after us (a late dyn-load); never adopt the season art. */
        if ((!g_news_known || cur != g_last_set) && (cur == NULL || cur != season)) { g_news_tex = cur; g_news_known = 1; }
        want = k == 1 ? season : g_news_tex;
        if (k == 1 && !season) img = "season texture not found";
        else {
            img = set_tex(game, ACTOR_NEWS, SYM_BG, want) ? (want ? (k == 1 ? "season" : "news") : "news (cleared)")
                                                         : "not written";
            g_last_set = want;
        }
        if (log) {
            const void *post = sprite_tex(game, ACTOR_NEWS, SYM_BG);   /* the slot after our write */
            _snprintf_s(sig, sizeof sig, _TRUNCATE, "  image: %s (wanted %p, news tile tex now %p, season tile tex %p)",
                        img, want, post, season);
        }
    }
    int dots_ok = 0, boxes = 0;
    if (g_square_dot && g_color_ok) {   /* (recipe): orange / white square, no glyph (no bullet in the fonts) */
        static const float on_rgb[3] = { 1.0f, 0.55f, 0.10f }, off_rgb[3] = { 0.85f, 0.87f, 0.90f };
        for (int i = 0; i < NSLIDE; i++) boxes += canvas_color(game, ACTOR_DOT[i], SYM_DOT_BOX, i == k ? on_rgb : off_rgb);
    }
    for (int i = 0; i < NSLIDE; i++) {   /* (b): the pager dots; actors absent on older trees -> 0 written */
        const char *g = boxes ? "" : g_dot_glyph;   /* no dot_box (pre- tree) or square_dot=0: the glyph */
        dots_ok += canvas_text(game, ACTOR_DOT[i], SYM_DOT_ON, i == k ? g : "");
        dots_ok += canvas_text(game, ACTOR_DOT[i], SYM_DOT_OFF, i == k ? "" : g);
    }
    InterlockedExchange(&g_shown, k);
    if (log) {
        size_t l = strlen(sig);
        _snprintf_s(sig + l, sizeof sig - l, _TRUNCATE, "|  dots: %d of %d element writes, %d of %d dot_box colours|text %s",
                    dots_ok, 2 * NSLIDE, boxes, NSLIDE, ok_text ? "written" : "NOT written");
        if (strcmp(sig, g_slide_logged[k])) {
            strcpy_s(g_slide_logged[k], sizeof g_slide_logged[k], sig);
            for (char *line = sig, *bar; line; line = bar) {
                if ((bar = strchr(line, '|')) != NULL) *bar++ = 0;
                if (!strncmp(line, "text ", 5)) rlog("SLIDE %d \"%s\": %s", k, text, line);
                else if (*line) rlog("%s", line);
            }
        }
    } else if (ok_text != !g_text_fail) {
        rlog("re-assert slide %d \"%s\": text %s", k, text, ok_text ? "written" : "NOT written");
    }
    g_text_fail = !ok_text;
}

/* Tab chrome: the shared banner reads the tab's name and the active rail button carries a "> X <"
 * marker (the button module has no selected state, final-pc-menu-chrome-and-tabs.md). Written on a tab change,
 * re-asserted every 0.5 s. The settings header is data-only since (a map-unique canvas copy reading SETTINGS
 * whose text element is renamed, w8-settings-recipe (a)): the old per-frame SETTINGS write here never showed (the
 * settings script rewrote it inside the original tick) and finds no element on+ trees, so it is gone . */
static void apply_tabs(uint8_t *g, ULONGLONG now)
{
    int tab = g_p24_on ? 0 : g_p30_on ? 1 : g_p58_on ? 2 : -1;
    if (tab == g_tab && now < g_title_next) return;
    int change = tab != g_tab;
    if (g_tab_title_on && tab == 2) canvas_text(g, ACTOR_BANNER, SYM_TEXT, "HELP");
    else if (g_tab_title_on && tab == 0 && change) canvas_text(g, ACTOR_BANNER, SYM_TEXT, "MAIN MENU");
    int hl = g_tab_hl && g_color_ok;   /*: a fill (or highlight) marks the tab; text marker only as the fallback */
    if (change || tab >= 0) {
        canvas_text(g, RAIL_ONLINE, SYM_TEXT, tab == 0 && !hl ? "> ONLINE <" : "ONLINE");
        canvas_text(g, RAIL_SETTING, SYM_TEXT, tab == 1 && !hl ? "> SETTINGS <" : "SETTINGS");
        canvas_text(g, RAIL_HELP, SYM_TEXT, tab == 2 && !hl ? "> HELP <" : "HELP");
    }
    int painted = 0;
    if (hl && tab >= 0 && (change || now >= g_color_next)) {   /* tab -1 (a popup): colours stay as they are */
        static const uint64_t rail[4] = { RAIL_ONLINE, RAIL_SETTING, RAIL_HELP, RAIL_QUIT };
        static const float blue[3] = { 0.239f, 0.396f, 0.569f }, slate[3] = { 0.325f, 0.369f, 0.427f };
        for (int i = 0; i < 4; i++) {
            if (g_tab_hl == 2) painted += i == tab ? canvas_show(g, rail[i], SYM_HIGHLIGHT, 1)
                                                   : (change ? canvas_show(g, rail[i], SYM_HIGHLIGHT, 0) : 0);
            else painted += canvas_color(g, rail[i], SYM_BACKGROUND, i == tab ? blue : slate);
        }
        g_color_next = now + 1000;
    }
    if (change) rlog("TAB %d (p24 %ld, p30 %ld, p58 %ld): %s, rail %s (%d element writes)", tab, g_p24_on,
                     g_p30_on, g_p58_on, g_tab_title_on ? "titles written" : "rail titles written, banner off (tab_title=0)", hl ? (g_tab_hl == 2 ? "highlight shown" : "Background colours") : "text marker",
                     painted);
    g_tab = tab;
    g_title_next = now + 500;
}

/* recipe: the HELP SEASON PASS press -> EV_SEASON to the page-5 owner, built from the last real event the
 * owner got (news / season tile press) or, before any, from the press's gs id + the owner's SActor index + its one
 * script component (season_comp). Sent through the stock SendComponentEvent (the trampoline when hooked here). */
static uint64_t actor_name(uint8_t *gs, uint64_t sactor);

static void season_send(uint8_t *g)
{
    uint64_t k, ev[5];
    uint8_t *gs = menu_actor(g, P5_TILES, &k);
    if (!gs) { rlog("HELP SEASON: page-5 owner not found: nothing sent"); return; }
    if (g_season_p24 && g_p5_state == 0 && g_go_season == 1) {
        rlog("HELP SEASON: page 5 off: show page 24 first (%s), send next tick",
             show_page(g, P24_HOME) ? "page not found" : "enabled");
        InterlockedExchange(&g_go_season, 2);
        return;
    }
    InterlockedExchange(&g_go_season, 0);
    uint32_t flags = 0;
    if (g_p5_have) { memcpy(ev, g_p5_ev, sizeof ev); flags = g_p5_flags; }
    else { ev[1] = g_hs_ev[1]; ev[2] = (g_hs_ev[2] & ~0xffffull) | (k & 0xffff); ev[3] = g_season_comp; ev[4] = 0; }
    ev[0] = EV_SEASON;
    if (actor_name(gs, ev[2]) != P5_TILES) {
        rlog("HELP SEASON: SActor %016llx does not name the page-5 owner: nothing sent", ev[2]);
        return;
    }
    SendFn send = g_target_orig ? g_target_orig : (SendFn)(g_base + g_prof->rva_send_target);
    __try {
        send(gs, ev, flags);
        rlog("HELP SEASON: season click event sent to page-5 owner (%s; gs %016llx actor %016llx comp %016llx flags %u, page 5 %ld)",
             g_p5_have ? "cached from a real page-5 event" : "built", ev[1], ev[2], ev[3], flags, g_p5_state);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_help_season = 0;
        rlog("HELP SEASON: fault in the send: help_season off");
    }
}

/* Tree gate (game thread, inside the tick's __try): is our minted tree in mnu_master? Asked on a gamespace change and at
 * every page-24 enable (a reload can reuse the same pointer). Each evaluation forgets the news tile's texture and our
 * last write (raw engine pointers, re-sampled before the next write); a new gamespace also drops the page-5 event
 * template (it carries the gamespace id), the tab and the carousel state. */
/* ---- FTUE dim + combat lock. Only with the tree marker; ftue=off / combat_lock=off write nothing. ---- */
static volatile LONG g_ftue_src;    /* the home picker row that enabled at this menu load: 25 / 26 / 27, 0 none */
static volatile LONG g_pick_on;     /* bits 0-2: p25 / p26 / p27 enabled now */
static volatile LONG g_lock_ptr;    /* pointer on CREATE (bit 0 ray, 4 hover) / JOIN (bit 1, 5) / PLAY (bit 2, 6) */
static volatile LONG g_back24;      /* FTUE: p34 / p36 opened anyway: show p24 again */
static volatile LONG g_p36_go;      /* p36 enabled: apply the combat lock face */
static int g_ftue, g_ftue_pass, g_cl_pass;
static ULONGLONG g_ftue_next, g_cl_next;
static const float GREY[3] = { 0.35f, 0.35f, 0.35f };   /* SetColor is RGB only: "dim" = grey */
static const char *const g_cl_word[3] = { "off", "purchase", "hardware" };

/* A card's lock face: card art and texts grey, prim 7 shown with msg (prim 6, the lock sprite, too if sprite). The
 * number of writes that landed (an element the canvas lacks is skipped). Game thread. */
static int lock_face(uint8_t *g, uint64_t card, const char *msg, int sprite)
{
    int n = canvas_color(g, card, EL_CARDIMAGE, GREY) + canvas_color(g, card, EL_CARDTEXT, GREY)
          + canvas_color(g, card, EL_CARDSUB, GREY) + canvas_show(g, card, EL_LOCKTEXT, 1);
    if (sprite) n += canvas_show(g, card, EL_LOCKSPRITE, 1);
    return n + canvas_text(g, card, EL_LOCKTEXT, msg);
}

static int ftue_dim(uint8_t *g)
{
    return lock_face(g, CARD_CREATE, FTUE_TEXT, 0) + lock_face(g, CARD_JOIN, FTUE_TEXT, 0)
         + canvas_color(g, ROW_MYCODE, SYM_CODETEXT, GREY) + canvas_color(g, ROW_MYCODE, SYM_TEXT, GREY)
         + canvas_color(g, ROW_LINK, SYM_TEXT, GREY) + canvas_color(g, ROW_FRIENDS, SYM_TEXT, GREY);
}

/* Tick, tree marker present. FTUE turns on once per menu load (the picker enabled p25/p26/p27, or ftue=force); the
 * dim is applied 3 times 1 s apart at that point and at every p24 enable, logged once per evaluation. It clears with
 * the menu gamespace (next login / level change); a profile update mid-menu is not seen. */
static void ftue_tick(uint8_t *g, ULONGLONG now, LONG reset)
{
    if (g_ftue_mode && !g_ftue && (g_ftue_src || g_ftue_mode == 2)) {
        LONG src = g_ftue_src;
        g_ftue = 1;
        if (src) rlog("FTUE on (picker page p%ld): page 24 %s", src, show_page(g, P24_HOME) ? "not found" : "enabled");
        else rlog("FTUE on (ftue=force)");
        g_ftue_pass = 3;
        g_ftue_next = now;
    }
    if (!g_ftue) return;
    if (reset) { g_ftue_pass = 3; g_ftue_next = now; }
    if (InterlockedExchange(&g_back24, 0))
        rlog("FTUE: a locked page opened anyway: page 24 %s", show_page(g, P24_HOME) ? "not found" : "enabled");
    if (g_ftue_pass > 0 && g_p24_on && now >= g_ftue_next) {
        int n = ftue_dim(g);
        if (g_ftue_pass == 3)
            rlog("FTUE dim: %d writes (CREATE / JOIN grey + lock text, invite row grey)%s", n,
                 g_pick_on ? "; picker page still enabled (no DisablePage pin)" : "");
        g_ftue_pass--;
        g_ftue_next = now + 1000;
    }
}

static void combat_lock_tick(uint8_t *g, ULONGLONG now)
{
    static const char *const msg[3] = { "", "Requires Combat Purchase", "Requires Oculus Link or Air Link" };
    if (!g_combat_lock) return;
    if (InterlockedExchange(&g_p36_go, 0)) { g_cl_pass = 3; g_cl_next = now; }   /* p36 may UnlockCard on enable */
    if (g_cl_pass > 0 && now >= g_cl_next) {
        int n = lock_face(g, CARD_COMBAT, msg[g_combat_lock], 1);
        if (g_cl_pass == 3) rlog("COMBAT LOCK (%s): p36 combat card face, %d writes", g_cl_word[g_combat_lock], n);
        g_cl_pass--;
        g_cl_next = now + 1000;
    }
}

/* ---- lockout card lock (recipe). Tree marker only;
 * lockout_lock=off writes nothing. State (auto): the net game's early-quit fields, ng = [game+0x8518] (the session
 * reader's pointer), expiry i64 at ng+0x64828 (Unix s), flags u8 at ng+0x64847 (disasm-only @ 6323983201049540, sns
 * early-quit-wire-and-pc-runtime.md /). Locked = flags bit 1 (the stock LOCKOUT tab's gate) && expiry > now.
 * A stock exe stores expiry -1 and masks bit 1: never locked. force = a lockout_force_sec countdown from
 * the first menu tick, for gate tests (it ends: the restore path runs). While locked: PLAY / CREATE / JOIN grey + lock
 * sprite + prim 7 "LOCKOUT hh:mm:ss" (1 s refresh), their presses consumed and stock p29 (quit_notice_page, canvas
 * ui_mnu_earlyquit_quit_notice) shown with its lockout body and timer; p34 / p36 / p49 opened anyway -> p29. Expiry ->
 * stock card colours back (stream-confirmed ui_mnu_card: 1 / 1 / 0.812), p29 -> p24. ---- */
#define CARD_PLAY    0xa3d3ef315cb999dbull   /* home rec-27 row 13 play_card */
#define P29_NOTICE   0x9344b604478e3cbdull   /* mnu_master p29 quit_notice_page (page-table, PC = Quest) */
#define P49_BEGIN_MP 0x2fe7102931d1c4e0ull   /* p49 begin_multiplayer_page: PLAY's stock target */
#define LO_NOTICE_ACTOR 0x600058d5720197fcull /* p29's element owner (click-table; its canvas = quit_notice, inferred) */
#define EL_LO_BODY   0xa2ff5667e38ab50dull   /* lockoutnoticebodytext1 */
#define EL_QN_BODY   0xd2c8dda0c5d553a4ull   /* quitnoticebodytext */
#define EL_LO_TIMER  0xcf9363cdc1963db6ull   /* lockouttimertext ("00:00:00") */
#define EL_LO_ICON   0x7a1ceaf22bcb77c6ull   /* lockouticon */
#define LO_NOTICE_LINE "You quit matches early. Play unlocks when the timer ends."
#define LO_RVA_A  0x1618b7u                  /* the early-quit patch sites (scripts/pc_exe_earlyquit_patch.py) */
#define LO_RVA_B2 0x1ae372u
#define LO_RVA_C  0xd96319u
static const uint8_t LO_A_S[3] = { 0x80, 0xe2, 0xdd }, LO_A_P[3] = { 0x80, 0xe2, 0xff };
static const uint8_t LO_B2_S[11] = { 0x48, 0xc7, 0x81, 0x28, 0x48, 0x06, 0x00, 0xff, 0xff, 0xff, 0xff };
static const uint8_t LO_B2_P[11] = { 0xff, 0x34, 0x24, 0x8f, 0x81, 0x28, 0x48, 0x06, 0x00, 0x66, 0x90 };
static const uint8_t LO_C_S[3] = { 0xc6, 0x03, 0x00 }, LO_C_P[3] = { 0x0f, 0x9f, 0x03 };
static const uint64_t LO_CARD[3] = { CARD_PLAY, CARD_CREATE, CARD_JOIN };
static const char *const LO_CARD_NAME[3] = { "PLAY", "CREATE", "JOIN" };
static const float WHITE[3] = { 1.0f, 1.0f, 1.0f }, CARD_SUB_RGB[3] = { 0.812f, 0.812f, 0.812f };
static volatile LONG g_lo_locked;     /* the tick's verdict, read by the event hooks */
static volatile LONG g_lo_go;         /* a locked card press was consumed: show p29 */
static volatile LONG g_lo_back;       /* p34 / p36 / p49 opened while locked: show p29 instead */
static volatile LONG g_p29_on;
static volatile LONG64 g_lo_p29_press;   /* GetTickCount64 of a press sent to p29 while we opened it (0 none) */
static int g_lo_src = -1;             /* -1 not checked, 0 unverified exe (auto off), 1 sites stock, 2 patched */
static volatile LONG g_lo_ours;      /* p29 was opened by us (the stuck guard applies) */
static int g_lo_dimmed, g_lo_restore, g_lo_logged_dim, g_lo_logged_note;
static long long g_lo_force_end, g_lo_last_rem = -2;
static ULONGLONG g_lo_next;

static int lo_site(uint32_t rva, const uint8_t *s, const uint8_t *p, int n)
{
    uint8_t got[16];
    if (!g_read(got, g_base + rva, (size_t)n)) return -1;
    return !memcmp(got, s, n) ? 0 : !memcmp(got, p, n) ? 1 : -1;
}

/* seconds of lockout left (0 = none). Game thread. */
static long long lo_remaining(uint8_t *g)
{
    long long now = (long long)_time64(NULL);
    if (g_lo_mode == 2) {
        if (!g_lo_force_end) g_lo_force_end = now + g_lo_force_sec;
        return g_lo_force_end > now ? g_lo_force_end - now : 0;
    }
    if (g_lo_src < 0) {   /* lazily: the evrmod early-quit plugin may patch after our attach */
        int a = lo_site(LO_RVA_A, LO_A_S, LO_A_P, 3), b = lo_site(LO_RVA_B2, LO_B2_S, LO_B2_P, 11);
        int c = lo_site(LO_RVA_C, LO_C_S, LO_C_P, 3);
        g_lo_src = (a < 0 || b < 0 || c < 0) ? 0 : (a && b && c) ? 2 : 1;
        rlog("LOCKOUT source: net game +0x64828 / +0x64847; exe sites a=%d b2=%d c=%d (0 stock, 1 patched, -1 other): %s",
             a, b, c, g_lo_src == 0 ? "unverified exe, lockout_lock=auto never locks" : g_lo_src == 2
             ? "early-quit patch on" : "stock or partly patched exe (a stock exe never shows a lockout)");
    }
    if (g_lo_src <= 0) return 0;
    uint8_t *ng = NULL;
    long long exp = -1;
    uint8_t fl = 0;
    if (!g_read(&ng, g + 0x8518, sizeof ng) || !ng || !g_read(&exp, ng + 0x64828, 8) || !g_read(&fl, ng + 0x64847, 1))
        return 0;
    long long rem = (fl & 0x02) && exp != -1 && exp > now ? exp - now : 0;
    if (rem > 366LL * 86400) rem = 0;   /* not a timestamp: treat as no lockout */
    if (g_lo_last_rem == -2 || (rem > 0) != (g_lo_last_rem > 0))
        rlog("LOCKOUT state: %s (flags 0x%02x, expiry %lld, now %lld)", rem > 0 ? "locked" : "none", fl, exp, now);
    return rem;
}

static void lo_hms(char *out, size_t cap, long long s)
{
    if (s < 0) s = 0;
    _snprintf_s(out, cap, _TRUNCATE, "%02lld:%02lld:%02lld", s / 3600, (s / 60) % 60, s % 60);
}

static int lo_restore_cards(uint8_t *g)
{
    int n = 0;
    for (int i = 0; i < 3; i++) {
        n += canvas_show(g, LO_CARD[i], EL_LOCKSPRITE, 0);
        if (g_ftue && i) continue;   /* CREATE / JOIN keep the FTUE face (re-applied below) */
        n += canvas_color(g, LO_CARD[i], EL_CARDIMAGE, WHITE) + canvas_color(g, LO_CARD[i], EL_CARDTEXT, WHITE)
           + canvas_color(g, LO_CARD[i], EL_CARDSUB, CARD_SUB_RGB) + canvas_show(g, LO_CARD[i], EL_LOCKTEXT, 0)
           + canvas_text(g, LO_CARD[i], EL_LOCKTEXT, "Requires Oculus Link or Air Link");
    }
    if (g_ftue) { g_ftue_pass = 3; g_ftue_next = 0; }
    return n;
}

static int lo_notice_face(uint8_t *g, const char *hms)
{
    /* the stock body is two sentences on one unwrapped line (clipped both ends) and the timer starts hidden */
    return canvas_show(g, LO_NOTICE_ACTOR, EL_LO_BODY, 1) + canvas_show(g, LO_NOTICE_ACTOR, EL_QN_BODY, 0)
         + canvas_text(g, LO_NOTICE_ACTOR, EL_LO_BODY, LO_NOTICE_LINE)
         + canvas_show(g, LO_NOTICE_ACTOR, EL_LO_ICON, 1) + canvas_show(g, LO_NOTICE_ACTOR, EL_LO_TIMER, 1)
         + canvas_text(g, LO_NOTICE_ACTOR, EL_LO_TIMER, hms)
         + canvas_text(g, ACTOR_BANNER, SYM_TEXT, "LOCKOUT NOTICE");
}

static void lockout_tick(uint8_t *g, ULONGLONG now, LONG reset)
{
    if (!g_lo_mode) return;
    if (reset) { g_lo_next = 0; g_lo_logged_dim = 0; }
    LONG go = InterlockedExchange(&g_lo_go, 0), back = InterlockedExchange(&g_lo_back, 0);
    LONG64 pt = g_lo_p29_press;
    if (pt && now >= (ULONGLONG)pt + 700) {   /* p29's own buttons did not leave the page: back to home */
        InterlockedExchange64(&g_lo_p29_press, 0);
        if (g_lo_ours && g_p29_on)
            rlog("LOCKOUT NOTICE: page 29 still up 0.7 s after a press: page 24 %s",
                 show_page(g, P24_HOME) ? "not found" : "enabled");
    }
    if (!go && !back && now < g_lo_next) return;
    long long rem = lo_remaining(g);
    char hms[16], msg[64];
    lo_hms(hms, sizeof hms, rem);
    InterlockedExchange(&g_lo_locked, rem > 0);
    if (rem > 0) {
        if (go || back) {
            int r = show_page(g, P29_NOTICE);
            rlog("LOCKOUT NOTICE (%s): page 29 %s, %s left", go ? "card press" : "a locked page opened anyway",
                 r ? "not found" : "enabled", hms);
            InterlockedExchange(&g_lo_ours, !r);
            g_lo_logged_note = 0;
            InterlockedExchange64(&g_lo_p29_press, 0);
        }
        if (g_p24_on) {
            int n = 0;
            _snprintf_s(msg, sizeof msg, _TRUNCATE, "%s%s%s", g_lo_prefix, g_lo_prefix[0] ? " " : "", hms);
            for (int i = 0; i < 3; i++) n += lock_face(g, LO_CARD[i], msg, 1);
            if (!g_lo_logged_dim) rlog("LOCKOUT dim (%s): %d writes (PLAY / CREATE / JOIN grey + lock, \"%s\")",
                                       g_lo_word[g_lo_mode], n, msg);
            g_lo_logged_dim = g_lo_dimmed = 1;
        }
        if (g_p29_on) {
            int n = lo_notice_face(g, hms);
            if (!g_lo_logged_note) rlog("LOCKOUT NOTICE face: %d writes (lockout body, timer %s, banner)", n, hms);
            g_lo_logged_note = 1;
        }
    } else {
        if (g_lo_last_rem > 0 || g_lo_dimmed) {
            if (g_lo_last_rem > 0) rlog("LOCKOUT over (%s): restoring", g_lo_word[g_lo_mode]);
            g_lo_restore = 3;
            g_lo_dimmed = 0;
            if (g_lo_ours && g_p29_on)
                rlog("LOCKOUT over while the notice was up: page 24 %s", show_page(g, P24_HOME) ? "not found" : "enabled");
            InterlockedExchange(&g_lo_ours, 0);
        }
        if (go || back) rlog("LOCKOUT: queued notice dropped (no lockout now)");
        if (g_lo_restore > 0 && g_p24_on) {
            int n = lo_restore_cards(g);
            if (g_lo_restore == 3) rlog("LOCKOUT restore: %d writes (stock card colours, lock text hidden)", n);
            g_lo_restore--;
        }
    }
    g_lo_last_rem = rem;
    g_lo_next = now + 1000;
}

static void tree_eval(uint8_t *g, uint8_t *mgs, const char *why)
{
    uint64_t k;
    g_news_known = 0;
    g_news_tex = g_last_set = NULL;
    if (mgs != g_tree_gs) {
        InterlockedExchange(&g_p5_have, 0);
        g_active = 0;
        g_tab = -2;
        g_tree_gs = mgs;
        g_ftue = g_ftue_pass = g_cl_pass = 0;
        g_lo_force_end = 0;   /*: a new menu gamespace = fresh canvases; force re-arms */
        g_lo_dimmed = g_lo_restore = g_lo_logged_dim = 0;
        g_lo_last_rem = -2;
        InterlockedExchange(&g_lo_ours, 0);
        InterlockedExchange(&g_lo_locked, 0);
    }
    LONG t = mgs && menu_actor(g, ACTOR_DOT[0], &k) != NULL;
    LONG f = t && g_friends && menu_actor(g, ACTOR_FRIEND0, &k) != NULL;
    InterlockedExchange(&g_tree, t);
    InterlockedExchange(&g_fr_tree, f);
    if (f && g_fr_wake) SetEvent(g_fr_wake);
    rlog("tree marker %s (%s): %s; friend rows %s", t ? "present" : "absent", why,
         t ? "menu writes on" : "stock menu, no writes (page tracking only)",
         !g_friends ? "off (friends=0)" : f ? "present, /friends polls" : "absent, no /friends");
}

static void report_tick(void *game, uint64_t now);
static int report_event(const uint64_t *ev, int target, void *gs);
static void report_start(void);
static void qw_tick(void *game, ULONGLONG now);
static void qw_event(void *gs, const uint64_t *ev);
static void qw_start(void);
static void pause_banner_tick(void *game, ULONGLONG now);

static void tick_body(void *game)
{
    uint8_t *g = (uint8_t *)game;
    ULONGLONG now = GetTickCount64();
    __try {
        report_tick(g, now);   /* Report Player v1: any level (report_flow.h); inert unless report=1 */
        qw_tick(g, now);       /* QUIT WARNING: any level; inert unless quit_warning=1 */
        pause_banner_tick(g, now);   /* w18c pause GRID: the stock pause banner reads PAUSE MENU (any level) */
        uint8_t *mgs = menu_gs(g);
        LONG reset = mgs ? InterlockedExchange(&g_reset, 0) : 0;   /* no menu: the page-24 enable waits for it */
        if (!mgs && g_tree_gs) {
            g_tree_gs = NULL;
            InterlockedExchange(&g_tree, 0);
            InterlockedExchange(&g_fr_tree, 0);
            InterlockedExchange(&g_ftue_src, 0);   /* the next menu load is judged afresh */
        }
        if (mgs && (mgs != g_tree_gs || reset)) tree_eval(g, mgs, mgs != g_tree_gs ? "menu gamespace changed" : "page 24 enabled");
        if (!mgs || !g_tree) {   /* no menu / the stock menu: drop queued work, write nothing */
            g_active = 0;
            g_tab = -2;
            g_paused = 0;
            InterlockedExchange(&g_ptr_mask, 0);
            InterlockedExchange(&g_jump, -1);
            InterlockedExchange(&g_fr_press, -1);
            InterlockedExchange(&g_go_p42, 0);
            InterlockedExchange(&g_go_p48, 0);
            InterlockedExchange(&g_go_season, 0);
            InterlockedExchange(&g_back24, 0);
            InterlockedExchange(&g_p36_go, 0);
            InterlockedExchange(&g_lo_go, 0);
            InterlockedExchange(&g_lo_back, 0);
            InterlockedExchange(&g_lo_locked, 0);
            return;
        }
        ftue_tick(g, now, reset);
        combat_lock_tick(g, now);
        lockout_tick(g, now, reset);
        int on = g_carousel && g_p24_on && g_p5_state != 0;
        if (reset) {
            memset(g_slide_logged, 0, sizeof g_slide_logged);
            g_slide = g_fixed >= 0 ? g_fixed : 0;
            g_next = now + g_period_ms;
            if (on) { rlog("page 24 shown: carousel reset to slide %d", g_slide); apply_face(g, g_slide, 1); }
            g_reassert = now + 2000;
        }
        LONG jump = InterlockedExchange(&g_jump, -1);
        int hovered = g_hover_pause && g_ptr_mask != 0;
        if (on && hovered && g_next < now + (ULONGLONG)g_hover_resume_ms)
            g_next = now + (ULONGLONG)g_hover_resume_ms;   /* hold; resume hover_resume_ms after the pointer left */
        if (on && hovered != g_paused) {
            rlog(hovered ? "HOVER pause (pointer mask %02lx)" : "hover end: rotation resumes in %d ms",
                 hovered ? (int)g_ptr_mask : g_hover_resume_ms);
            g_paused = hovered;
        }
        if (on) {
            if (jump >= 0 && jump < NSLIDE && g_active) {
                rlog("DOT %ld pressed: slide %d -> %ld, period restarts", jump, g_slide, jump);
                g_slide = (int)jump;
                g_next = now + g_period_ms;
                if (hovered && g_next < now + (ULONGLONG)g_hover_resume_ms) g_next = now + (ULONGLONG)g_hover_resume_ms;
                apply_face(g, g_slide, 1);
                g_reassert = now + 2000;
            }
            if (!g_active) {
                rlog("carousel on (page 24 enabled, page 5 state %ld)", g_p5_state);
                if (g_fixed >= 0) g_slide = g_fixed;
                g_next = now + g_period_ms;
                apply_face(g, g_slide, 1);
                g_reassert = now + 2000;
            }
            if (now >= g_next && g_fixed < 0) {
                int from = g_slide;
                g_slide = (g_slide + 1) % NSLIDE;
                g_next = now + g_period_ms;
                rlog("slide change %d -> %d", from, g_slide);
                apply_face(g, g_slide, 1);
                g_reassert = now + 2000;
            } else if (now >= g_reassert) {   /* the tile's own script may redraw: keep our face */
                apply_face(g, g_slide, 0);
                g_reassert = now + 2000;
            }
        } else if (g_active) {
            rlog("carousel off (page 24 %ld, page 5 %ld): stock face back", g_p24_on, g_p5_state);
            apply_face(g, 0, 1);
            g_slide = 0;
            InterlockedExchange(&g_ptr_mask, 0);   /* a disabled element gets no _Idle / _OnEndIntersection */
            g_paused = 0;
        }
        g_active = on;
        apply_tabs(g, now);
        apply_footer(g, now, g_tab);
        if (g_fr_tree && g_p24_on) apply_friends(g, now);
        if (g_go_season && g_help_season) season_send(g);
        if (InterlockedExchange(&g_go_p42, 0))
            rlog("REDIRECT slide 2: show page 42 (owner %016llx): %s", P42_QUIT,
                 show_page(g, P42_QUIT) ? "page not found" : "enabled");
        if (InterlockedExchange(&g_go_p48, 0))   /*: the carousel slide-2 path, owner p48 */
            rlog("HELP SAFE ZONE: show page 48 (owner %016llx): %s", P48_LEGAL,
                 show_page(g, P48_LEGAL) ? "page not found" : "enabled");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        rlog("fault in the tick: carousel step skipped");
    }
}

/* ---- event hooks ---- */

static uint64_t actor_name(uint8_t *gs, uint64_t sactor)
{
    uint8_t *tbl = NULL;
    uint64_t idx = sactor & 0xffff, *names = NULL, count = 0, name = 0;
    if (!g_read(&tbl, gs + g_prof->o_gs_actors, sizeof tbl) || !tbl) return 0;
    if (!g_read(&names, tbl + g_prof->o_at_names, sizeof names) || !names) return 0;
    if (!g_read(&count, tbl + g_prof->o_at_count, sizeof count) || idx >= count) return 0;
    return g_read(&name, names + idx, sizeof name) ? name : 0;
}

/* bit of the news tile / dot i in the pointer mask, or -1 */
static int ptr_bit(uint64_t actor)
{
    if (actor == ACTOR_NEWS) return 0;
    for (int i = 0; i < NSLIDE; i++)
        if (actor == ACTOR_DOT[i]) return 1 + i;
    return -1;
}

static void pointer_event(void *gs, const uint64_t *ev)
{
    __try {
        uint64_t who = actor_name((uint8_t *)gs, ev[2]);
        int on = ev[0] == EV_PTR_START || ev[0] == EV_PTR_HOVER;
        if (who == ACTOR_SAFE) {   /*: the SAFE ZONE card fallback reads this */
            LONG sb = (ev[0] == EV_PTR_HOVER || ev[0] == EV_PTR_IDLE) ? 2 : 1;
            if (on) InterlockedOr(&g_safe_ptr, sb);
            else InterlockedAnd(&g_safe_ptr, ~sb);
            return;
        }
        if (who == CARD_CREATE || who == CARD_JOIN || who == CARD_PLAY) {   /* FTUE /: which card a press is from */
            LONG cb = (who == CARD_CREATE ? 1 : who == CARD_JOIN ? 2 : 4) << (ev[0] == EV_PTR_HOVER || ev[0] == EV_PTR_IDLE ? 4 : 0);
            if (on) InterlockedOr(&g_lock_ptr, cb);
            else InterlockedAnd(&g_lock_ptr, ~cb);
            return;
        }
        int b = ptr_bit(who);
        if (b < 0) return;
        LONG bit = 1L << (b + (ev[0] == EV_PTR_HOVER || ev[0] == EV_PTR_IDLE ? 4 : 0));
        if (on) InterlockedOr(&g_ptr_mask, bit);
        else InterlockedAnd(&g_ptr_mask, ~bit);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

static int dot_event(uint64_t sym)
{
    for (int i = 0; i < NSLIDE; i++)
        if (sym == EV_DOT[i]) { InterlockedExchange(&g_jump, i); return 1; }
    return 0;
}

static void core_body(void *gs, const uint64_t *ev)
{
    report_event(ev, 0, (void *)(gs));
    qw_event(gs, ev);
    if (ev[0] == EV_PTR_HOVER || ev[0] == EV_PTR_IDLE || ev[0] == EV_PTR_START || ev[0] == EV_PTR_END) {
        pointer_event(gs, ev);
        return;
    }
    if (dot_event(ev[0])) { rlog("dot click event %016llx (global)", ev[0]); return; }
    if (friend_event(ev[0])) { rlog("friend row click event %016llx (global)", ev[0]); return; }
    if (ev[0] == EV_HELP_SEASON) { rlog("HELP SEASON click event (global)"); return; }
    if (ev[0] == EV_HELP_SAFE) { rlog("HELP SAFE ZONE click event (global)"); return; }
    if (ev[0] != EV_PAGE_ON && ev[0] != EV_PAGE_OFF) return;
    __try {
        uint64_t owner = actor_name((uint8_t *)gs, ev[2]);
        int on = ev[0] == EV_PAGE_ON;
        if (owner == P24_HOME) {
            InterlockedExchange(&g_p24_on, on);
            if (on) {
                InterlockedExchange(&g_reset, 1);
                g_fr_reassert = 0;
                InterlockedExchange(&g_fr_kick, 1);   /* the worker re-reads the services host (a change lifts the backoff) */
                if (g_fr_wake) SetEvent(g_fr_wake);   /* poll /friends now  */
            }
            if (!on) InterlockedExchange(&g_lock_ptr, 0);   /* a disabled element gets no _OnEndIntersection */
            rlog("page 24 %s", on ? "enabled" : "disabled");
        } else if (owner == P25_CORE_TUT || owner == P26_MP_TUT || owner == P27_NPE_HOME) {
            int pb = owner == P25_CORE_TUT ? 0 : owner == P26_MP_TUT ? 1 : 2;
            if (on) InterlockedOr(&g_pick_on, 1L << pb);
            else InterlockedAnd(&g_pick_on, ~(1L << pb));
            if (on && g_ftue_mode == 1) InterlockedExchange(&g_ftue_src, 25 + pb);
            rlog("page %d (home picker row) %s", 25 + pb, on ? "enabled" : "disabled");
        } else if (owner == P29_NOTICE) {
            InterlockedExchange(&g_p29_on, on);
            if (!on) InterlockedExchange(&g_lo_ours, 0);
            if (g_lo_mode) rlog("page 29 (quit / lockout notice) %s", on ? "enabled" : "disabled");
        } else if ((owner == P34_JOIN || owner == P36_EXPER || owner == P49_BEGIN_MP) && on) {
            if (g_lo_locked && g_tree) InterlockedExchange(&g_lo_back, 1);   /*: the notice, not the page */
            else if (g_ftue && g_tree && owner != P49_BEGIN_MP) InterlockedExchange(&g_back24, 1);
            if (owner == P36_EXPER && g_combat_lock) InterlockedExchange(&g_p36_go, 1);
        } else if (owner == P5_TILES) {
            InterlockedExchange(&g_p5_state, on);
            rlog("page 5 %s", on ? "enabled" : "disabled");
        } else if (owner == P58_HELP) {
            InterlockedExchange(&g_p58_on, on);
        } else if (owner == P30_SETTINGS) {
            InterlockedExchange(&g_p30_on, on);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

static int rewrite(const uint64_t *ev, uint64_t sym)
{
    __try { *(volatile uint64_t *)ev = sym; return 1; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

/* ---- Report Player v1 (report_flow.h): the PC glue ----
 * Script calls (disasm-only shapes, device-observed @ 6323983201049540):
 * CLocalComponentHandle<CScriptCS>::Find 0x168340, CScriptCS::FunctionReserveSlot 0x438e80 (hidden out),
 * FunctionSetParam 0x4390b0, DeferredFunctionCall 0x431610 (SFunctionCallParams 0x38 B), inside Set/Clear
 * InvokeImmediate. Events through the core site (SendComponentEventGlobal, the hooked address). */
#define RP_TABLET      0x6c1f6ff04e070923ull   /* as report_flow.h */
#define RP_RVA_FIND_SCRIPT 0x168340u
#define RP_RVA_RESERVE     0x438e80u
#define RP_RVA_SETPARAM    0x4390b0u
#define RP_RVA_DEFERRED    0x431610u
#define RP_O_GS_LIST  0x1f50u
#define RP_O_GS_COUNT 0x1f80u
static const uint32_t RP_CHK_RVA[4] = { RP_RVA_FIND_SCRIPT, RP_RVA_RESERVE, RP_RVA_SETPARAM, RP_RVA_DEFERRED };
static const uint8_t RP_CHK[4][8] = {
    { 0x48, 0x89, 0x5c, 0x24, 0x08, 0x4c, 0x89, 0x44 },   /* Find<CScriptCS> */
    { 0x48, 0x89, 0x5c, 0x24, 0x10, 0x48, 0x89, 0x6c },   /* FunctionReserveSlot */
    { 0x48, 0x8b, 0x81, 0xc8, 0x00, 0x00, 0x00, 0x44 },   /* FunctionSetParam */
    { 0x48, 0x8b, 0x81, 0x80, 0x00, 0x00, 0x00, 0x83 } }; /* DeferredFunctionCall */
#define RP_CARD_EL_DEFAULT "party_label"
typedef uint64_t *(__fastcall *RpReserveFn)(void *cs, uint64_t *out, uint32_t idx, uint64_t fn, uint32_t auto_retire, uint32_t quiet);
typedef void (__fastcall *RpSetParamFn)(void *cs, uint32_t idx, uint64_t param, const void *value, uint64_t memslot);
typedef void (__fastcall *RpCallFn)(void *cs, const uint8_t *params);
static HINTERNET g_rp_http;
#define RP_XCHG(p, v) ((int)InterlockedExchange((volatile LONG *)(p), (LONG)(v)))
#define rp_log rlog
#define rp_actor(gs, a) actor_name((uint8_t *)(gs), (a))
static uint64_t rp_sym(const char *s) { return g_csym64(s); }
static int rp_rewrite(const uint64_t *ev, uint64_t sym) { return rewrite(ev, sym); }

/* the gamespace holding `actor` (any level), or NULL; *sa = its static SActor handle (idx | 0xffff << 16 | 0xffff << 32):
 * the component finds take the handle, not the name */
static uint8_t *rp_actor_gs(uint8_t *game, uint64_t actor, uint64_t *sa)
{
    uint64_t n = *(uint64_t *)(game + RP_O_GS_COUNT);
    uint8_t *list = *(uint8_t **)(game + RP_O_GS_LIST);
    for (uint64_t i = 0; list && n <= 64 && i < n; i++) {
        uint8_t *gs = *(uint8_t **)(list + i * 16 + 8);
        if (!gs || *(void **)(gs + g_prof->o_gs_game) != game) continue;
        uint8_t *tbl = *(uint8_t **)(gs + g_prof->o_gs_actors);
        uint64_t *names = tbl ? *(uint64_t **)(tbl + g_prof->o_at_names) : NULL;
        uint64_t count = tbl ? *(uint64_t *)(tbl + g_prof->o_at_count) : 0;
        for (uint64_t k = 0; names && count <= 0xfffe && k < count; k++)
            if (names[k] == actor) { *sa = 0x0000ffffffff0000ull | k; return gs; }
    }
    return NULL;
}

/* a script function call on `comp` of `actor`, the way a script's external-function node calls it */
static int rp_call_on(void *game, uint64_t actor, uint64_t comp, uint64_t fn, int n, const uint64_t *pname, const void *const *pval)
{
    int rc = -1;
    __try {
        uint64_t sa;
        uint8_t *gs = rp_actor_gs((uint8_t *)game, actor, &sa);
        if (!gs) return -1;
        Handle h = { NULL, 0 };
        ((FindFn)(g_base + RP_RVA_FIND_SCRIPT))(&h, gs, sa, comp);
        if (!valid(&h)) return -1;
        uint64_t ms = ~0ull;
        ((GameFn)(g_base + g_prof->rva_invoke_set))(game);
        ((RpReserveFn)(g_base + RP_RVA_RESERVE))(h.cs, &ms, h.idx, fn, 1, 0);
        if (ms == ~0ull || ms >> 32 == 0xffffffffull) rc = 1;
        else {
            for (int k = 0; k < n; k++) ((RpSetParamFn)(g_base + RP_RVA_SETPARAM))(h.cs, h.idx, pname[k], pval[k], ms);
            uint8_t p[0x38];
            memset(p, 0xff, sizeof p);
            memcpy(p, &h.idx, 4);
            memset(p + 4, 0, 4);
            memcpy(p + 8, &ms, 8);
            memset(p + 0x26, 0, 2);
            ((RpCallFn)(g_base + RP_RVA_DEFERRED))(h.cs, p);
            rc = 0;
        }
        ((GameFn)(g_base + g_prof->rva_invoke_clear))(game);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        rlog("report: script call %016llx faulted: flow off", fn);
        InterlockedExchange((volatile LONG *)&g_rp_ready, 0);
        rc = -1;
    }
    return rc;
}

static int rp_call(void *game, uint64_t fn, int n, const uint64_t *pname, const void *const *pval)
{
    return rp_call_on(game, RP_TABLET, S_STATE_MGR, fn, n, pname, pval);
}

static int rp_canvas(void *game, uint64_t actor, Handle *c)
{
    uint64_t sa;
    uint8_t *gs = rp_actor_gs((uint8_t *)game, actor, &sa);
    if (!gs) return 0;
    c->cs = NULL; c->idx = 0;
    ((FindFn)(g_base + g_prof->rva_find_canvas))(c, gs, sa, SYM_CANVASUI);
    return valid(c);
}

static int rp_settext(void *game, uint64_t actor, uint64_t el, const char *s)
{
    __try {
        Handle c;
        return rp_canvas(game, actor, &c) && elem_found(&c, el)
               && ((SetTextFn)(g_base + g_prof->rva_canvas_set_text))(c.cs, c.idx, el, s) != 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

static int rp_gettext(void *game, uint64_t actor, uint64_t el, char *out, size_t cap)
{
    out[0] = 0;
    if (!g_text_ok) return 0;
    __try {
        Handle c;
        uint8_t *inst = NULL;
        uint64_t id = ~0ull;
        if (!rp_canvas(game, actor, &c)) return 0;
        ((FindPrimFn)(g_base + g_prof->rva_find_prim))(c.cs, &id, c.idx, el, &inst);
        if (!inst || (uint32_t)id == 0xffffffffu) return 0;
        const char *s = ((ElementTextFn)(g_base + g_prof->rva_element_text))(inst, (uint32_t)id);
        if (!s) return 0;
        _snprintf_s(out, cap, _TRUNCATE, "%s", s);
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

static int rp_fire(void *game, uint64_t holder, uint64_t sym)
{
    __try {
        uint64_t sa;
        uint8_t *gs = rp_actor_gs((uint8_t *)game, holder, &sa);
        if (!gs) return 0;
        uint64_t ev[5] = { sym, *(uint64_t *)(gs + g_prof->o_gs_id), SYM_NONE, SYM_NONE, SYM_NONE };
        ((SendFn)(g_base + g_prof->rva_send_core))(gs, ev, 1);
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

/* a TARGETED event to one static actor's component, as SendEventNode builds it for a real press (w23b: the
 * five-tile card's close_button). Through the original site (g_target_orig), so our own target hook does not see it. */
static int rp_press_comp(void *game, uint64_t actor, uint64_t ev0, uint64_t comp)
{
    __try {
        uint64_t sa;
        uint8_t *gs = rp_actor_gs((uint8_t *)game, actor, &sa);
        if (!gs) return 0;
        uint64_t ev[5] = { ev0, *(uint64_t *)(gs + g_prof->o_gs_id), sa, comp, SYM_NONE };
        SendFn send = g_target_orig ? g_target_orig : (SendFn)(g_base + g_prof->rva_send_target);
        send(gs, ev, 1);
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

static int rp_user(char *out, size_t cap)
{
    AcquireSRWLockShared(&g_user_lock);
    _snprintf_s(out, cap, _TRUNCATE, "%s", g_user);
    ReleaseSRWLockShared(&g_user_lock);
    if (!out[0] && log_user(out, cap)) {
        AcquireSRWLockExclusive(&g_user_lock);
        if (!g_user[0]) strcpy_s(g_user, sizeof g_user, out);
        ReleaseSRWLockExclusive(&g_user_lock);
    }
    return out[0] != 0;
}

/* worker thread: client API at 127.0.0.1:6721, or the services host (apiservice_host) */
static int rp_http(const char *method, int client_api, const char *path, const char *body, char *out, size_t cap)
{
    out[0] = 0;
    if (!g_rp_http) return 0;
    int changed;
    if (!client_api && !g_svc_host[0] && !read_services_host(&changed)) return 0;
    wchar_t wpath[256], wm[8];
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, 256);
    MultiByteToWideChar(CP_UTF8, 0, method, -1, wm, 8);
    HINTERNET c = WinHttpConnect(g_rp_http, client_api ? L"127.0.0.1" : g_svc_host, client_api ? 6721 : g_svc_port, 0), r = NULL;
    int status = 0;
    DWORD blen = body ? (DWORD)strlen(body) : 0;
    if (c) r = WinHttpOpenRequest(c, wm, wpath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (r && WinHttpSendRequest(r, body ? L"Content-Type: application/json\r\n" : WINHTTP_NO_ADDITIONAL_HEADERS,
                                body ? (DWORD)-1L : 0, body ? (LPVOID)body : WINHTTP_NO_REQUEST_DATA, blen, blen, 0)
        && WinHttpReceiveResponse(r, NULL)) {
        DWORD st = 0, sz = sizeof st;
        WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &st, &sz, NULL);
        status = (int)st;
        size_t got = 0;
        DWORD rd = 0;
        while (got < cap - 1 && WinHttpReadData(r, out + got, (DWORD)(cap - 1 - got), &rd) && rd) got += rd;
        out[got] = 0;
    }
    if (r) WinHttpCloseHandle(r);
    if (c) WinHttpCloseHandle(c);
    return status;
}

/* ---- in-process session reader (sess_snapshot.h) ----
 * ng = [game+0x8518] (device-observed PC 6323983201049540); names by CR15NetGame::UserName 0x1c98c0; the lobby-entrant
 * and LobbyId bodies the header reads inline are pinned by their bytes (disasm-only). Any
 * mismatch = unverified build: the reader stays off (Report falls back to the host, QUIT WARNING keeps stock). Game
 * thread only; every read is memcpy under __try and the whole read (UserName call included) runs inside another. */
#include "sess_snapshot.h"
static const struct { uint32_t rva; uint8_t n; uint8_t b[37]; } SESS_CHK[4] = {
    { 0x1c98c0, 32, { 0x48, 0x89, 0x5c, 0x24, 0x08, 0x57, 0x48, 0x83, 0xec, 0x20, 0x0f, 0xb7, 0xfa, 0x48, 0x8b, 0xd9,
                      0x66, 0x39, 0xb9, 0xe2, 0x00, 0x00, 0x00, 0x77, 0x6f, 0x66, 0x3b, 0xb9, 0xdc, 0x00, 0x00, 0x00 } },
    { 0x6093c0, 37, { 0x40, 0x53, 0x48, 0x83, 0xec, 0x30, 0x48, 0x8b, 0xda, 0x4c, 0x3b, 0x81, 0x68, 0x03, 0x00, 0x00,
                      0x73, 0x2c, 0x48, 0x8b, 0x81, 0x60, 0x03, 0x00, 0x00, 0x4d, 0x69, 0xc0, 0xd8, 0x00, 0x00, 0x00,
                      0x42, 0x0f, 0x10, 0x04, 0x00 } },
    { 0x6084d0, 32, { 0x48, 0x3b, 0x91, 0x68, 0x03, 0x00, 0x00, 0x73, 0x17, 0x48, 0x8b, 0x81, 0x60, 0x03, 0x00, 0x00,
                      0x48, 0x69, 0xd2, 0xd8, 0x00, 0x00, 0x00, 0x0f, 0xb7, 0x84, 0x10, 0x8a, 0x00, 0x00, 0x00, 0xc3 } },
    { 0x60bf50, 8, { 0x48, 0x8d, 0x81, 0xcc, 0x01, 0x00, 0x00, 0xc3 } } };
static volatile LONG g_sess_ok = -1;   /* -1 not checked, 0 unverified build, 1 ok */
static int pc_sess_read(void *dst, uintptr_t src, size_t n)
{
    __try { memcpy(dst, (const void *)src, n); } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
    return 0;
}
static void sess_attach(void)
{
    if (g_sess_ok >= 0) return;
    LONG ok = 1;
    for (int i = 0; i < 4 && ok; i++) {
        uint8_t got[37];
        if (!g_read(got, g_base + SESS_CHK[i].rva, SESS_CHK[i].n) || memcmp(got, SESS_CHK[i].b, SESS_CHK[i].n)) {
            rlog("session reader off (unverified build): bytes at rva %06x differ", SESS_CHK[i].rva);
            ok = 0;
        }
    }
    if (ok) rlog("session reader: UserName / lobby-entrant / LobbyId bytes match");
    InterlockedExchange(&g_sess_ok, ok);
}
static int rp_sess(void *game, int want, sess_snapshot *s)
{
    if (g_sess_ok != 1) { memset(s, 0, sizeof *s); return -100; }   /* RP_SESS_UNVERIFIED */
    sess_profile prof = { "6323983201049540", 0x8518, pc_sess_read, (sess_username_fn)(g_base + 0x1c98c0), NULL };
    __try { return sess_read(game, &prof, want, s); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return SESS_FAULT; }
}

/* report_card_glyph: CCanvasUICS::SetSpriteUV(handle, element, region CSymbol64) 0x37d220 (disasm-only; region looked up
 * in the canvas's own uvmap) and ShowUIElement (the setter). Bytes checked at report_start. */
#define RP_RVA_SPRITE_UV 0x37d220u
static const uint8_t RP_CHK_SPRITE_UV[16] = { 0x40, 0x53, 0x48, 0x83, 0xec, 0x40, 0x49, 0x8b, 0xd9, 0x48, 0x8d, 0x44, 0x24, 0x38, 0x4d, 0x8b };
typedef void (__fastcall *SpriteUVFn)(void *cs, uint32_t idx, uint64_t element, uint64_t region);
static volatile LONG g_rp_uv_ok;
static int rp_canvas(void *game, uint64_t actor, Handle *c);
static int rp_sprite_uv(void *game, uint64_t actor, uint64_t el, uint64_t region)
{
    if (!g_rp_uv_ok) return 0;
    __try {
        Handle c;
        if (!rp_canvas(game, actor, &c) || !elem_found(&c, el)) return 0;
        ((SpriteUVFn)(g_base + RP_RVA_SPRITE_UV))(c.cs, c.idx, el, region);
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_rp_uv_ok, 0);
        rlog("report: fault in SetSpriteUV: card glyph off");
        return 0;
    }
}
/* rp_getvar (card target): a STRING variable of an actor's script component:
 * CLocalComponentHandle<CScriptCS>::Find 0x168340 + CScriptCS::String 0x46d3f0 (handle -> instance [cs+0xf8] + slot*0xb8
 * +0x18, then a tail jump to CScriptInstance::String 0x1045a20; "" when the script is not initialised; disasm-only
 * @ 6323983201049540). The 32 bytes up to the jump are checked at report_start. */
#define RP_RVA_GET_STRING 0x46d3f0u
static const uint8_t RP_CHK_GET_STRING[32] = {
    0x48, 0x8b, 0x81, 0xc8, 0x00, 0x00, 0x00, 0x44, 0x0f, 0xb7, 0xca, 0x42, 0x0f, 0xb7, 0x14, 0x88,
    0x48, 0x8b, 0x81, 0xf8, 0x00, 0x00, 0x00, 0x4c, 0x69, 0xca, 0xb8, 0x00, 0x00, 0x00, 0x49, 0x8b };
typedef const char *(__fastcall *RpGetStringFn)(void *cs, uint32_t idx, uint64_t var);
static volatile LONG g_rp_var_ok;
static int rp_getvar(void *game, uint64_t actor, uint64_t comp, uint64_t var, char *out, size_t cap)
{
    out[0] = 0;
    if (!g_rp_var_ok) return 0;
    __try {
        uint64_t sa;
        uint8_t *gs = rp_actor_gs((uint8_t *)game, actor, &sa);
        if (!gs) return 0;
        Handle h = { NULL, 0 };
        ((FindFn)(g_base + RP_RVA_FIND_SCRIPT))(&h, gs, sa, comp);
        if (!valid(&h)) return 0;
        const char *s = ((RpGetStringFn)(g_base + RP_RVA_GET_STRING))(h.cs, h.idx, var);
        if (!s) return 0;
        _snprintf_s(out, cap, _TRUNCATE, "%s", s);
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_rp_var_ok, 0);
        rlog("report: fault in CScriptCS::String: card xplatformid read off");
        out[0] = 0;
        return 0;
    }
}

/* rp_setvar ( back re-open: clear the card's xplatformid before OpenLobbyMemberSocialControls): CScriptCS::SetString
 * 0x4692a0 (the quit warning's setter; checked at report_start). 1 = written. Game thread.
 * A write must never land on a read twin, so the whole body is checked, 59 bytes up to and with the tail jump: mov
 * rax,r8; mov r11,r9; movzx r8d,dx; mov r9,-1 (the length cap, String / Real have none); the handle -> instance path; jmp
 * 0x1045830 (CScriptInstance's setter). disasm-only @ 6323983201049540. */
#define RP_RVA_SET_STRING_RP 0x4692a0u
static const uint8_t RP_CHK_SET_STRING_RP[59] = {
    0x49, 0x8b, 0xc0, 0x4d, 0x8b, 0xd9, 0x44, 0x0f, 0xb7, 0xc2, 0x49, 0xc7, 0xc1, 0xff, 0xff, 0xff, 0xff, 0x48, 0x8b, 0x91,
    0xc8, 0x00, 0x00, 0x00, 0x48, 0x8b, 0x89, 0xf8, 0x00, 0x00, 0x00, 0x46, 0x0f, 0xb7, 0x04, 0x82, 0x48, 0x8b, 0xd0, 0x4d,
    0x69, 0xd0, 0xb8, 0x00, 0x00, 0x00, 0x4d, 0x8b, 0xc3, 0x49, 0x8b, 0x4c, 0x0a, 0x18, 0xe9, 0x55, 0xc5, 0xbd, 0x00 };
static volatile LONG g_rp_setvar_ok;
static int rp_setvar(void *game, uint64_t actor, uint64_t comp, uint64_t var, const char *str)
{
    if (!g_rp_setvar_ok) return 0;
    __try {
        uint64_t sa;
        uint8_t *gs = rp_actor_gs((uint8_t *)game, actor, &sa);
        if (!gs) return 0;
        Handle h = { NULL, 0 };
        ((FindFn)(g_base + RP_RVA_FIND_SCRIPT))(&h, gs, sa, comp);
        if (!valid(&h)) return 0;
        ((void (__fastcall *)(void *, uint32_t, uint64_t, const char *))(g_base + RP_RVA_SET_STRING_RP))(h.cs, h.idx, var, str);
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_rp_setvar_ok, 0);
        rlog("report: fault in CScriptCS::SetString: card id clear off");
        return 0;
    }
}

/* rp_getreal (fallback F's press guard): a REAL variable of an actor's script component: Find<CScriptCS> + CScriptCS::Real
 * 0x44e8b0 (xmm0; String's handle -> instance path, then a tail jump to CScriptInstance::Real 0x1044410, type key REAL,
 * 0.0 when the script is not initialised; disasm-only @ 6323983201049540, checked as the test hook checks it). Its first 32 bytes equal
 * String's, so all 43 up to and with the jump are checked at report_start; a mismatch leaves the guard off. */
#define RP_RVA_GET_REAL 0x44e8b0u
static const uint8_t RP_CHK_GET_REAL[43] = {
    0x48, 0x8b, 0x81, 0xc8, 0x00, 0x00, 0x00, 0x44, 0x0f, 0xb7, 0xca, 0x42, 0x0f, 0xb7, 0x14, 0x88,
    0x48, 0x8b, 0x81, 0xf8, 0x00, 0x00, 0x00, 0x4c, 0x69, 0xca, 0xb8, 0x00, 0x00, 0x00, 0x49, 0x8b,
    0xd0, 0x49, 0x8b, 0x4c, 0x01, 0x18, 0xe9, 0x35, 0x5b, 0xbf, 0x00 };
typedef float (__fastcall *RpGetRealFn)(void *cs, uint32_t idx, uint64_t var);
static volatile LONG g_rp_real_ok;
static int rp_getreal(void *game, uint64_t actor, uint64_t comp, uint64_t var, float *out)
{
    if (!g_rp_real_ok) return 0;
    __try {
        uint64_t sa;
        uint8_t *gs = rp_actor_gs((uint8_t *)game, actor, &sa);
        if (!gs) return 0;
        Handle h = { NULL, 0 };
        ((FindFn)(g_base + RP_RVA_FIND_SCRIPT))(&h, gs, sa, comp);
        if (!valid(&h)) return 0;
        *out = ((RpGetRealFn)(g_base + RP_RVA_GET_REAL))(h.cs, h.idx, var);
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_rp_real_ok, 0);
        rlog("report: fault in CScriptCS::Real: field press guard off");
        return 0;
    }
}

static int rp_show(void *game, uint64_t actor, uint64_t el, uint32_t show)
{
    if (!g_color_ok) return 0;
    __try {
        Handle c;
        if (!rp_canvas(game, actor, &c) || !elem_found(&c, el)) return 0;
        ((ShowElFn)(g_base + g_prof->rva_show_el))(c.cs, c.idx, el, show);
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

/* (report_flow.h header) element probe, SetColor, page enable and actor presence in any gamespace */
static int rp_has_el(void *game, uint64_t actor, uint64_t el)
{
    __try {
        Handle c;
        return rp_canvas(game, actor, &c) && elem_found(&c, el);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

static int rp_color(void *game, uint64_t actor, uint64_t el, const float rgb[3])
{
    if (!g_color_ok) return 0;
    __try {
        Handle c;
        if (!rp_canvas(game, actor, &c) || !elem_found(&c, el)) return 0;
        ((SetColorFn)(g_base + g_prof->rva_set_color))(c.cs, c.idx, el, rgb);
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

static uintptr_t rp_actor_key(void *game, uint64_t actor)
{
    __try {
        uint64_t sa;
        return (uintptr_t)rp_actor_gs((uint8_t *)game, actor, &sa);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

static int rp_has_actor(void *game, uint64_t actor) { return rp_actor_key(game, actor) != 0; }

/* EnablePage on the page component of `owner` in whichever gamespace holds it (show_page is mnu_master only; the
 * pause pages live in mnu_master_mp_ingame) */
static int rp_show_page(void *game, uint64_t owner)
{
    __try {
        uint64_t sa;
        uint8_t *gs = rp_actor_gs((uint8_t *)game, owner, &sa);
        Handle h = { NULL, 0 };
        if (!gs) return 0;
        ((FindFn)(g_base + g_prof->rva_find_page_cs))(&h, gs, sa, SYM_NONE);
        if (!valid(&h)) return 0;
        ((GameFn)(g_base + g_prof->rva_invoke_set))(game);
        ((EnablePageFn)(g_base + g_prof->rva_enable_page))(h.cs, h.idx);
        ((GameFn)(g_base + g_prof->rva_invoke_clear))(game);
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

/* ---- ADD FRIEND on the PC five-tile card (the stock script shows it only on Quest; enable its interact component here) ----
 * The stock script enables R14ButtonInteract `button4` only in Quest-gated setupfriendbutton, and its close thread disables
 * it, so the tile is hidden and inert on PC. This is the engine's EnableComponent(actor, type, name, enable=1) without a
 * script context: CComponentSystem *Find(gs, type) 0x4f0c70 (jmp 0x105510), the system's vtable slot +0x1a8
 * (component handle by actor + name), EnableComponent 0x160070(cs, idx, category 1). All disasm-only @ 6323983201049540;
 * the two entries are prologue-checked at report_start, the vtable target must lie in the image. The press then runs the
 * stock R15NetAddFriendNode (platform Social object: pnsovr under OVR, pnsdemo under -noovr); no host call is added. */
#define RP_RVA_CS_FIND     0x4f0c70u
#define RP_RVA_BTN_ENABLE  0x160070u
#define RP_BTN_TYPE        0xf5574a730bbb0428ull   /* csym64("R14ButtonInteract") */
#define RP_BTN4            0x2fd59a8b4a7b59deull   /* csym64("button4") */
#define RP_VT_FIND_NAME    0x1a8u
#define RP_HAVE_CARD_FRIEND 1
static const uint8_t RP_CHK_CS_FIND[8] = { 0xe9, 0x9b, 0x48, 0xc1, 0xff, 0xcc, 0xcc, 0xcc };
static const uint8_t RP_CHK_BTN_ENABLE[8] = { 0x48, 0x89, 0x5c, 0x24, 0x20, 0x55, 0x56, 0x41 };
static volatile LONG g_rp_btn_ok;
typedef void *(__fastcall *RpCsFindFn)(void *gs, uint64_t type);
typedef uint32_t *(__fastcall *RpCompFindFn)(void *cs, uint64_t *out, uint64_t sactor, uint64_t name);
typedef void (__fastcall *RpEnableFn)(void *cs, uint32_t idx, uint16_t category);
static int rp_in_image(const void *p)
{
    const IMAGE_DOS_HEADER *d = (const IMAGE_DOS_HEADER *)g_base;
    const IMAGE_NT_HEADERS *nt = (const IMAGE_NT_HEADERS *)(g_base + d->e_lfanew);
    return (const uint8_t *)p >= g_base && (const uint8_t *)p < g_base + nt->OptionalHeader.SizeOfImage;
}
static int rp_card_button4(void *game);   /* 1 enabled, 0 not run, -1 fault (switches the hook off); defined after report_flow.h (RP_CARD) */

/* ---- the engine's own scroll axis for the pause REPORT list (CR15PointerCS::ScrollAmount) ----
 * All disasm-only @ PC 6323983201049540 (the same layout is disasm-only @ Quest 6528386917200980, radmenu_quest.c):
 *   CR15PointerCS vftable 0x1c85430 (RTTI .?AVCR15PointerCS@NRadGame@NRadEngine@@, stream-confirmed); its Update
 *   (the loop at 0xa00e70..0xa01124) walks u16 count [cs+0xf4], dense list [cs+0xd8] (u16 handle per i), handles
 *   [cs+0xc8] (u32: low u16 = record), records [cs+0xf8] stride 0x58, and stores ScrollAmount f32 at rec+0x14 =
 *   Analog(3) (xmm10) when the hand u32 rec+0x10 is 0, else Analog(4) (xmm11, the result of 0xf984e0) at 0xa010ef / 0xa01102.
 *   The gamespace's component-system table (the one Find 0x4f0c70 -> 0x105510 bisects): 16-byte {type, cs} entries at
 *   [gs+0x420], count [gs+0x450]. The system is taken by its vtable (the authority), its key csym64("R15Pointer")
 *   0x85d04532edaeb196 (once in the exe, 0x1c44818) only logged. Read-only: no engine call. Bytes checked at report_start. */
#define RP_VT_POINTER_CS   0x1c85430u
#define RP_POINTER_TYPE    0x85d04532edaeb196ull
#define RP_O_GS_CS_TBL     0x420u
#define RP_O_GS_CS_N       0x450u
#define RP_O_PCS_HANDLES   0xc8u
#define RP_O_PCS_DENSE     0xd8u
#define RP_O_PCS_COUNT     0xf4u
#define RP_O_PCS_RECS      0xf8u
#define RP_PCS_STRIDE      0x58u
#define RP_PCS_MAXH        4096u    /* handle / record index cap (m4): beyond it the u16 is garbage, not a slot */
#define RP_O_PREC_HAND     0x10u
#define RP_O_PREC_SCROLL   0x14u
static const struct { uint32_t rva; uint8_t n; uint8_t b[24]; } RP_CHK_SCROLL[5] = {
    { 0xa00580, 24, { 0x48,0x8b,0x81,0xc8,0x00,0x00,0x00, 0x44,0x0f,0xb7,0xc2, 0x42,0x0f,0xb7,0x14,0x80, 0x48,0x8b,0x81,0xf8,0x00,0x00,0x00, 0x4c } },
    { 0xa00ed0, 21, { 0x48,0x8b,0x86,0xd8,0x00,0x00,0x00, 0x0f,0xb7,0xcf, 0x0f,0xb7,0x14,0x48, 0x48,0x8b,0x8e,0xc8,0x00,0x00,0x00 } },
    { 0xa010ef, 6,  { 0xf3,0x44,0x0f,0x11,0x53,0x14 } },                              /* movss [rbx+0x14], xmm10 */
    { 0x105520, 16, { 0x48,0x8b,0x91,0x50,0x04,0x00,0x00, 0x48,0x85,0xd2, 0x74,0x6c, 0xf6,0x81,0x58,0x04 } },
    { 0x10555e, 7,  { 0x4c,0x8b,0x97,0x20,0x04,0x00,0x00 } },                         /* mov r10, [rdi+0x420] */
};
static volatile LONG g_rp_scroll_ok;   /* the bytes matched (and no fault since) */
static int rp_scroll_amount(void *game, float *out);   /* defined after report_flow.h */

/* ---- review M2: the arm tablet's state-stack top (state_mgr's CScriptStateStackCS) ----
 * CLocalComponentHandle<CScriptStateStackCS>::Find 0x127e3d0 (the 16-byte Find shape: handle, gs, SActor, component name),
 * then the CI read as CScriptStateStackCS::CurrentStateName 0x431010 reads it: slot = u16 [[cs+0xc8] + idx*4],
 * ci = [cs+0xf8] + slot*0x58, u64 stack [ci+0x38], count [ci+0x40], top = stack[count-1]. disasm-only @
 * 6323983201049540 (the same read answers `sstate` in the test hook, verified PC + Quest). Find's 16 bytes and
 * CurrentStateName's first 48 (every offset read here) are checked at report_start; no engine call past the Find. */
#define RP_RVA_FIND_SSTATE 0x127e3d0u
#define RP_RVA_SSTATE_TOP  0x431010u
static const uint8_t RP_CHK_FIND_SSTATE[16] = { 0x48,0x89,0x5c,0x24,0x08, 0x4c,0x89,0x44,0x24,0x18, 0x48,0x89,0x54,0x24,0x10, 0x56 };
static const uint8_t RP_CHK_SSTATE_TOP[48] = {
    0x48,0x8b,0x81,0xc8,0x00,0x00,0x00, 0x45,0x0f,0xb7,0xc8, 0x46,0x0f,0xb7,0x04,0x88, 0x49,0x6b,0xc0,0x58,
    0x48,0x03,0x81,0xf8,0x00,0x00,0x00, 0x48,0x8b,0x48,0x40, 0x48,0x85,0xc9, 0x74,0x10, 0x48,0x8b,0x40,0x38,
    0x48,0x8b,0x4c,0xc8,0xf8, 0x48,0x8b,0xc2 };
static volatile LONG g_rp_sstate_ok;   /* the bytes matched (and no fault since) */
static int rp_tablet_top(void *game, uint64_t *top);   /* defined after report_flow.h */

/* ---- stock-off (report_flow.h RP_SO note) ----
 * CR15ButtonInteractCS::EnableComponent(h, cat) 0x92bd10 / DisableComponent 0x92b9e0 (the IMMEDIATE slots +0x200 / +0x208
 * of vftable 0x1c6f560, both directions: the queued wrapper 0x9cb780 could reorder against an immediate enable), the
 * component lookup by actor + zone name at slot +0x1a8 (0x2d4cd0, hidden-return u32 handle, as rp_card_button4). Args
 * (rcx cs, edx handle, r8w category). All disasm-only @ 6323983201049540; the bytes below and the three vtable slots are
 * checked at report_start (any mismatch: stock-off off, the swallow stays). The system is the gamespace CS-table entry
 * with that vptr (key csym64("R14ButtonInteract") first, then any); the handle is verified by rp_zone_check. */
#define RP_HAVE_ZONE_OFF 1
#define RP_RVA_BTN_VT        0x1c6f560u
#define RP_RVA_BTN_EN_IMM    0x92bd10u
#define RP_RVA_BTN_DIS_IMM   0x92b9e0u
#define RP_RVA_CS_COMPONENT  0x2d4cd0u
#define RP_VT_BTN_EN         0x200u
#define RP_VT_BTN_DIS        0x208u
static const uint8_t RP_CHK_BTN_EN_IMM[32] = {   /* mov [rsp+8],rbx ... lea rdi,[rcx+0D8h]; movzx eax,dx */
    0x48,0x89,0x5c,0x24,0x08, 0x48,0x89,0x6c,0x24,0x10, 0x48,0x89,0x74,0x24,0x18, 0x57, 0x41,0x56, 0x41,0x57,
    0x48,0x83,0xec,0x20, 0x48,0x8d,0xb9,0xd8,0x00,0x00,0x00, 0x0f };
static const uint8_t RP_CHK_BTN_DIS_IMM[48] = {  /* ... add rcx,0D8h; movzx ebp,word [rdi+10Ch] */
    0x48,0x89,0x5c,0x24,0x08, 0x48,0x89,0x6c,0x24,0x10, 0x48,0x89,0x74,0x24,0x18, 0x48,0x89,0x7c,0x24,0x20,
    0x41,0x56, 0x48,0x83,0xec,0x20, 0x48,0x8b,0xf9, 0x0f,0xb7,0xc2, 0x8b,0xda, 0x48,0x81,0xc1,0xd8,0x00,0x00,0x00,
    0x0f,0xb7,0xaf,0x0c,0x01,0x00,0x00 };
static const uint8_t RP_CHK_CS_COMPONENT[16] = { 0x40,0x53, 0x48,0x83,0xec,0x30, 0x48,0x8b,0x01, 0x48,0x8b,0xda,
    0x4c,0x89,0x44,0x24 };
static volatile LONG g_rp_zone_ok;           /* bytes + slots matched (and no fault since) */
static char g_rp_zone_why[96] = "report flow inert";
#include "report_flow.h"

typedef void (__fastcall *RpBtnCatFn)(void *cs, uint32_t h, uint16_t category);
static int rp_rd_direct(void *dst, uintptr_t src, size_t n) { if (!src) return -1; memcpy(dst, (const void *)src, n); return 0; }
static int rp_zone_ready(const char **why) { *why = g_rp_zone_why; return g_rp_zone_ok != 0; }

/* 1 found (z filled, verified), 0 actor / zone not loaded, -1 unavailable or fault (game thread) */
static int rp_zone_find(void *game, uint64_t actor, uint64_t zone, rp_zref *z)
{
    if (!g_rp_zone_ok) return -1;
    int rc = 0;
    __try {
        uint64_t sa;
        uint8_t *gs = rp_actor_gs((uint8_t *)game, actor, &sa);
        if (!gs) return 0;
        const void *vt = g_base + RP_RVA_BTN_VT;
        uint8_t *tbl = *(uint8_t **)(gs + RP_O_GS_CS_TBL);
        uint64_t m = *(uint64_t *)(gs + RP_O_GS_CS_N);
        for (int pass = 0; pass < 2 && !rc; pass++)   /* the keyed entry first, then any entry with the vtable */
            for (uint64_t k = 0; tbl && m <= 4096 && k < m && !rc; k++) {
                uint64_t key = *(uint64_t *)(tbl + k * 16), out = 0;
                uint8_t *cs = *(uint8_t **)(tbl + k * 16 + 8);
                uint16_t rs = 0;
                if (!cs || (pass == 0) != (key == RP_BTN_TYPE) || *(const void **)cs != vt) continue;
                uint32_t *h = ((RpCompFindFn)(g_base + RP_RVA_CS_COMPONENT))(cs, &out, sa, zone);
                if (!h || (uint16_t)*h == 0xffff || !rp_zone_check(rp_rd_direct, (uintptr_t)cs, *h, actor, zone, &rs)) continue;
                z->gs = (uintptr_t)gs; z->cs = (uintptr_t)cs; z->h = *h; z->reasons = rs;
                rc = 1;
            }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        rlog("report: stock-off: fault in the zone lookup: engine route off");
        InterlockedExchange(&g_rp_zone_ok, 0);
        rc = -1;
    }
    return rc;
}

/* off: DisableComponent(h, 0x80), else EnableComponent(h, 0x80); 1 run, 0 not run, -1 fault (game thread) */
static int rp_zone_cat(void *game, const rp_zref *z, int off)
{
    if (!g_rp_zone_ok || !z->cs) return 0;
    int rc = 0, set = 0;
    __try {
        if (*(const void **)z->cs != (const void *)(g_base + RP_RVA_BTN_VT)) return 0;
        ((GameFn)(g_base + g_prof->rva_invoke_set))(game);
        set = 1;
        ((RpBtnCatFn)(g_base + (off ? RP_RVA_BTN_DIS_IMM : RP_RVA_BTN_EN_IMM)))((void *)z->cs, z->h, (uint16_t)RP_SO_BIT);
        ((GameFn)(g_base + g_prof->rva_invoke_clear))(game);
        set = 0;
        rc = 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (set) {
            __try { ((GameFn)(g_base + g_prof->rva_invoke_clear))(game); } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
        rlog("report: stock-off: fault in %sComponent: engine route off", off ? "Disable" : "Enable");
        InterlockedExchange(&g_rp_zone_ok, 0);
        rc = -1;
    }
    return rc;
}

/* 1 read (*top = the top state's symbol, 0 = an empty stack), 0 unavailable (game thread) */
static int rp_tablet_top(void *game, uint64_t *top)
{
    if (!g_rp_sstate_ok) return 0;
    __try {
        uint64_t sa;
        uint8_t *gs = rp_actor_gs((uint8_t *)game, RP_TABLET, &sa);
        Handle h = { NULL, 0 };
        if (!gs) return 0;
        ((FindFn)(g_base + RP_RVA_FIND_SSTATE))(&h, gs, sa, S_STATE_MGR);
        if (!valid(&h)) return 0;
        uint8_t *cs = (uint8_t *)h.cs;
        uint16_t slot = *(uint16_t *)(*(uint8_t **)(cs + 0xc8) + (uint64_t)(h.idx & 0xffff) * 4);
        uint8_t *ci = *(uint8_t **)(cs + 0xf8) + (uint64_t)slot * 0x58;
        uint64_t depth = *(uint64_t *)(ci + 0x40);
        if (depth > 0xffff) return 0;
        *top = depth ? (*(uint64_t **)(ci + 0x38))[depth - 1] : 0;
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        rlog("report: tablet state reader: fault: off (a tablet closed mid-flow restores at the lapse)");
        InterlockedExchange(&g_rp_sstate_ok, 0);
        return 0;
    }
}

/* the signed max-|ScrollAmount| over every CR15PointerCS record of every gamespace (game thread); 1 read, 0 unavailable.
 * The systems are re-found once a second (cached with their gamespace); each read re-checks the vtable. */
static int rp_scroll_amount(void *game, float *out)
{
    static uint8_t *cs_of[8], *gs_of[8];
    static int ncs = -1, logged = -2;
    static ULONGLONG next;
    if (!g_rp_scroll_ok) return 0;
    const void *vt = g_base + RP_VT_POINTER_CS;
    int rc = 0;
    __try {
        ULONGLONG now = GetTickCount64();
        if (ncs < 0 || now >= next) {
            next = now + 1000;
            ncs = 0;
            uint64_t n = *(uint64_t *)((uint8_t *)game + RP_O_GS_COUNT);
            uint8_t *list = *(uint8_t **)((uint8_t *)game + RP_O_GS_LIST);
            int keyed = 0;
            for (uint64_t i = 0; list && n <= 64 && i < n && ncs < 8; i++) {
                uint8_t *gs = *(uint8_t **)(list + i * 16 + 8);
                if (!gs || *(void **)(gs + g_prof->o_gs_game) != game) continue;
                uint8_t *tbl = *(uint8_t **)(gs + RP_O_GS_CS_TBL);
                uint64_t m = *(uint64_t *)(gs + RP_O_GS_CS_N);
                uint8_t *found = NULL;
                /* the keyed entry first, else any entry with the vtable (as Quest's rp_pointer_cs) */
                for (int pass = 0; pass < 2 && !found; pass++)
                    for (uint64_t k = 0; tbl && m <= 4096 && k < m; k++) {
                        uint8_t *cs = *(uint8_t **)(tbl + k * 16 + 8);
                        uint64_t key = *(uint64_t *)(tbl + k * 16);
                        if (cs && (pass || key == RP_POINTER_TYPE) && *(const void **)cs == vt) {
                            keyed += key == RP_POINTER_TYPE;
                            found = cs;
                            break;
                        }
                    }
                if (found) { gs_of[ncs] = gs; cs_of[ncs++] = found; }
            }
            if (ncs != logged) {
                rlog("report: scroll reader: %d CR15PointerCS system(s) (%d under key %016llx)", ncs, keyed, RP_POINTER_TYPE);
                logged = ncs;
            }
        }
        float best = 0.0f;
        for (int j = 0; j < ncs; j++) {
            uint8_t *cs = cs_of[j];
            if (*(const void **)cs != vt) { next = 0; continue; }   /* unloaded: re-find on the next tick */
            uint16_t cnt = *(uint16_t *)(cs + RP_O_PCS_COUNT);
            uint16_t *dense = *(uint16_t **)(cs + RP_O_PCS_DENSE);
            uint32_t *hnd = *(uint32_t **)(cs + RP_O_PCS_HANDLES);
            uint8_t *recs = *(uint8_t **)(cs + RP_O_PCS_RECS);
            for (uint16_t i = 0; dense && hnd && recs && cnt <= 64 && i < cnt; i++) {
                /* m4: indexes bounded (RP_PCS_MAXH; SEH still catches an unmapped read), and an amount outside the
                 * axis range [-1, 1] (or NaN) is garbage, not input: skipped, so the list never scrolls by itself */
                uint16_t d = dense[i];
                if (d >= RP_PCS_MAXH) continue;
                uint16_t ri = (uint16_t)hnd[d];
                if (ri >= RP_PCS_MAXH) continue;
                uint8_t *r = recs + ri * RP_PCS_STRIDE;
                float f = *(float *)(r + RP_O_PREC_SCROLL);
                if (!(f >= -1.5f && f <= 1.5f)) continue;
                if (g_rpp_scroll_probe && f != 0.0f)
                    rlog("report: scroll probe gs %p rec %u hand %u amount %.3f", (void *)gs_of[j], i, *(uint32_t *)(r + RP_O_PREC_HAND), (double)f);
                if (f * f > best * best) best = f;
            }
        }
        *out = best;
        rc = ncs > 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        rlog("report: scroll reader: fault: off (the list falls back to the hover dwell)");
        InterlockedExchange(&g_rp_scroll_ok, 0);
        rc = 0;
    }
    return rc;
}

static int rp_card_button4(void *game)
{
    if (!g_rp_btn_ok) return 0;
    int rc = 0, set = 0;
    __try {
        uint64_t sa, out = 0;
        uint8_t *gs = rp_actor_gs((uint8_t *)game, RP_CARD, &sa);
        if (!gs) return 0;
        void *cs = ((RpCsFindFn)(g_base + RP_RVA_CS_FIND))(gs, RP_BTN_TYPE);
        if (!cs) return 0;
        void *fn = (*(void ***)cs)[RP_VT_FIND_NAME / 8];
        if (!rp_in_image(fn)) return 0;
        uint32_t *h = ((RpCompFindFn)fn)(cs, &out, sa, RP_BTN4);
        if (!h || (uint16_t)*h == 0xffff) return 0;
        ((GameFn)(g_base + g_prof->rva_invoke_set))(game);
        set = 1;
        ((RpEnableFn)(g_base + RP_RVA_BTN_ENABLE))(cs, *h, 1);
        ((GameFn)(g_base + g_prof->rva_invoke_clear))(game);
        set = 0;
        rc = 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (set) {   /* a fault inside EnableComponent: the invoke flag must not stay raised */
            __try { ((GameFn)(g_base + g_prof->rva_invoke_clear))(game); } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
        rlog("report: ADD FRIEND: fault in the button4 enable: hook off");
        InterlockedExchange(&g_rp_btn_ok, 0);
        rc = -1;
    }
    return rc;
}


/* ---- PAUSE MENU banner  ----
 * Rev c of the pause GRID (tree ffa_w18c and later) keeps the stock frame 0xedcea26f534fa586 on pause p1, whose banner
 * 0x445e05b8ad2bf412 (model mnu_central_region_header, canvas menu_central_region, element `text`) the pause home
 * writes "PAUSED" into at runtime: no level row holds that string, so data cannot change it. While the grid marker
 * (the rev-c footer actor ffa_pause_status 0xb8ccd5ac45079093) is loaded, the banner reads Coffey's "PAUSE MENU",
 * like apply_tabs writes the main-menu banner: read back every 250 ms, rewritten when it differs (the page enable
 * rewrites PAUSED, so a stock flash lasts at most one poll). Off with tab_title=0; a stock tree has no marker. */
#define PAUSE_BANNER 0x445e05b8ad2bf412ull
#define PAUSE_MARKER 0xb8ccd5ac45079093ull   /* ffa_pause_status (w18c footer, SERVER STATUS) */
static void pause_banner_tick(void *game, ULONGLONG now)
{
    static ULONGLONG next, next_marker;
    static int marker = -1, written;
    if (!g_tab_title_on || now < next) return;
    next = now + 250;
    if (now >= next_marker) {   /* the marker scan walks every gamespace: once a second */
        int m = rp_has_actor(game, PAUSE_MARKER);
        next_marker = now + 1000;
        if (m != marker) {
            rlog(m ? "pause banner: grid marker ffa_pause_status loaded: banner %016llx reads PAUSE MENU"
                   : "pause banner: grid marker not loaded: stock banner (%016llx)", PAUSE_BANNER);
            marker = m;
            written = 0;
        }
    }
    if (marker != 1) return;
    char cur[32];
    if (rp_gettext(game, PAUSE_BANNER, SYM_TEXT, cur, sizeof cur) && !strcmp(cur, "PAUSE MENU")) return;
    int ok = rp_settext(game, PAUSE_BANNER, SYM_TEXT, "PAUSE MENU");
    if (!written || ok != (written == 1)) {
        rlog("pause banner: \"%s\" -> PAUSE MENU %s", cur, ok ? "written" : "NOT written (banner not loaded)");
        written = ok ? 1 : 2;
    }
}

static DWORD WINAPI report_worker(LPVOID unused)
{
    (void)unused;
    for (;;) { report_worker_step(); Sleep(50); }
}

/* stock-off: the immediate Enable / Disable, the lookup, and the three vtable slots pointing at them. Run first in
 * report_start, ahead of the report= and script-call gates: the guard holds the REPORT layer's own zones off on every
 * generic prompt even when the flow is off (report=0) or inert (a failed check) */
static void report_so_check(void)
{
    uint8_t a[32], b[48], c[16];
    static const uint32_t slot[3] = { RP_VT_FIND_NAME, RP_VT_BTN_EN, RP_VT_BTN_DIS };
    static const uint32_t target[3] = { RP_RVA_CS_COMPONENT, RP_RVA_BTN_EN_IMM, RP_RVA_BTN_DIS_IMM };
    int ok = 1;
    g_rp_zone_why[0] = 0;
    if (!g_read(a, g_base + RP_RVA_BTN_EN_IMM, 32) || memcmp(a, RP_CHK_BTN_EN_IMM, 32))
        snprintf(g_rp_zone_why, sizeof g_rp_zone_why, "EnableComponent bytes at rva %06x differ", RP_RVA_BTN_EN_IMM);
    else if (!g_read(b, g_base + RP_RVA_BTN_DIS_IMM, 48) || memcmp(b, RP_CHK_BTN_DIS_IMM, 48))
        snprintf(g_rp_zone_why, sizeof g_rp_zone_why, "DisableComponent bytes at rva %06x differ", RP_RVA_BTN_DIS_IMM);
    else if (!g_read(c, g_base + RP_RVA_CS_COMPONENT, 16) || memcmp(c, RP_CHK_CS_COMPONENT, 16))
        snprintf(g_rp_zone_why, sizeof g_rp_zone_why, "component lookup bytes at rva %06x differ", RP_RVA_CS_COMPONENT);
    for (int i = 0; i < 3 && !g_rp_zone_why[0]; i++) {
        uint64_t v = 0;
        if (!g_read(&v, g_base + RP_RVA_BTN_VT + slot[i], 8) || v != (uint64_t)(uintptr_t)(g_base + target[i]))
            snprintf(g_rp_zone_why, sizeof g_rp_zone_why, "vtable %06x slot +%#x is not rva %06x", RP_RVA_BTN_VT, slot[i], target[i]);
    }
    ok = !g_rp_zone_why[0];
    InterlockedExchange(&g_rp_zone_ok, ok);
    rpt_so_start();   /* logs the active path: engine or fallback swallow (<why>) */
}

/* after the hooks are in: checks the four script-call entries, then starts the worker */
static void report_start(void)
{
    report_so_check();
    if (!g_rp_on) return;
    for (int i = 0; i < 4; i++) {
        uint8_t got[8];
        if (!g_read(got, g_base + RP_CHK_RVA[i], 8) || memcmp(got, RP_CHK[i], 8)) {
            rlog("report: script-call entry at rva %06x differs from the profile: report flow inert", RP_CHK_RVA[i]);
            return;
        }
    }
    report_init_syms();
    sess_attach();
    g_rp_glyph = g_rp_glyph_s[0] ? g_csym64(g_rp_glyph_s) : 0;
    if (g_rp_glyph) {
        uint8_t got[16];
        int ok = g_read(got, g_base + RP_RVA_SPRITE_UV, 16) && !memcmp(got, RP_CHK_SPRITE_UV, 16);
        InterlockedExchange(&g_rp_uv_ok, ok);
        if (!ok) rlog("report: SetSpriteUV bytes at rva %06x differ from the profile: card glyph off", RP_RVA_SPRITE_UV);
    }
    {
        uint8_t got[32];
        int ok = g_read(got, g_base + RP_RVA_GET_STRING, 32) && !memcmp(got, RP_CHK_GET_STRING, 32);
        InterlockedExchange(&g_rp_var_ok, ok);
        {
            uint8_t gsb[sizeof RP_CHK_SET_STRING_RP];
            InterlockedExchange(&g_rp_setvar_ok, g_read(gsb, g_base + RP_RVA_SET_STRING_RP, sizeof gsb) && !memcmp(gsb, RP_CHK_SET_STRING_RP, sizeof gsb));
            if (!g_rp_setvar_ok) rlog("report: CScriptCS::SetString bytes at rva %06x differ: card id clear off", RP_RVA_SET_STRING_RP);
        }
        rlog(ok ? "report: card target by the card's xplatformid (CScriptCS::String bytes match)"
                : "report: CScriptCS::String bytes at rva %06x differ: card target falls back to 2-player sessions only", RP_RVA_GET_STRING);
    }
    {
        uint8_t got[43];
        int ok = g_read(got, g_base + RP_RVA_GET_REAL, 43) && !memcmp(got, RP_CHK_GET_REAL, 43);
        InterlockedExchange(&g_rp_real_ok, ok);
        if (!ok) rlog("report: CScriptCS::Real bytes at rva %06x differ: field press guard off", RP_RVA_GET_REAL);
    }
    {
        int ok = 1;
        for (int i = 0; i < 5 && ok; i++) {
            uint8_t got[24];
            ok = g_read(got, g_base + RP_CHK_SCROLL[i].rva, RP_CHK_SCROLL[i].n) && !memcmp(got, RP_CHK_SCROLL[i].b, RP_CHK_SCROLL[i].n);
            if (!ok) rlog("report: CR15PointerCS bytes at rva %06x differ: list scroll by hover dwell only", RP_CHK_SCROLL[i].rva);
        }
        InterlockedExchange(&g_rp_scroll_ok, ok);
        rlog("report: list scroll report_list_scroll=%s report_scroll_probe=%d report_scroll_sign=%d (ScrollAmount reader %s)",
             g_rpp_scr_name[g_rpp_scroll_mode], g_rpp_scroll_probe, g_rpp_scroll_sign, ok ? "bytes match" : "off");
    }
    {
        uint8_t a[16], b[48];
        int ok = g_read(a, g_base + RP_RVA_FIND_SSTATE, 16) && !memcmp(a, RP_CHK_FIND_SSTATE, 16)
              && g_read(b, g_base + RP_RVA_SSTATE_TOP, 48) && !memcmp(b, RP_CHK_SSTATE_TOP, 48);
        InterlockedExchange(&g_rp_sstate_ok, ok);
        rlog(ok ? "report: tablet state reader on (CScriptStateStackCS Find / CurrentStateName bytes match)"
                : "report: CScriptStateStackCS bytes at rva %06x / %06x differ: a tablet closed mid-flow restores at the lapse",
             RP_RVA_FIND_SSTATE, RP_RVA_SSTATE_TOP);
    }
    if (g_rp_addfriend) {
        uint8_t a[8], b[8];
        int ok = g_read(a, g_base + RP_RVA_CS_FIND, 8) && !memcmp(a, RP_CHK_CS_FIND, 8)
              && g_read(b, g_base + RP_RVA_BTN_ENABLE, 8) && !memcmp(b, RP_CHK_BTN_ENABLE, 8);
        InterlockedExchange(&g_rp_btn_ok, ok);
        if (ok) rlog("report: ADD FRIEND on the five-tile card: on (card_add_friend=%d; EnableComponent bytes match)", g_rp_addfriend);
        else rlog("report: ADD FRIEND disabled: component find / enable bytes at rva %06x / %06x differ from the profile",
                  RP_RVA_CS_FIND, RP_RVA_BTN_ENABLE);
    } else rlog("report: ADD FRIEND on the five-tile card: off (card_add_friend=0)");
    if (!g_rp_http) g_rp_http = WinHttpOpen(L"rad-menu/1", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (g_rp_http) { WinHttpSetTimeouts(g_rp_http, 2000, 2000, 3000, 3000); }
    HANDLE t = CreateThread(NULL, 0, report_worker, NULL, 0, NULL);
    if (t) CloseHandle(t);
    InterlockedExchange((volatile LONG *)&g_rp_ready, 1);
    rlog("report: ready (tile uid %016llx, card event %016llx uid %016llx element %016llx%s)", g_rp_tile_uid,
         g_rp_card_event, g_rp_card_uid, g_rp_card_el, g_rp_card_event ? "" : ": card learn mode, no relabel");
    rlog("report: report_tablet_reasons=%s report_popup=%d (event %016llx) report_card5_event=%016llx report_card5_uid=%016llx "
         "report_card5_learn=%d", g_rp_tr_name[g_rp_treasons], g_rpp_on, g_rpp_event, g_rp_card5_event, g_rp_card5_uid, g_rp_card5_learn);
    rlog("report: report=%s (tile uid %016llx)%s", g_rp_on < 0 ? "auto: acts only while ffa_rp_list_page is loaded" : "1: any level",
         g_rp_tile_uid, g_rp_fake_rows ? "" : "; report_fake_rows=0");
    if (g_rp_fake_rows) rlog("report: *** TEST ONLY: report_fake_rows=%d synthetic players appended to the list; never POSTed ***", g_rp_fake_rows);
}

/* ---- QUIT WARNING (; Coffey) ----
 * The pause quit confirm p7 (owner 0xd436ecc9f7f9164d, script ui_mnu_pause_page_quit_confirm 0x72306b442a6857a7)
 * passes its string variable quit_warning_text to the confirm widget (script ui_mnu_quit_confirm 0x5947161c7c583291,
 * canvas ui_mnu_popup 0x674f3b71da94770c), whose echo_arena / echo_combat branch shows a warning element stock never
 * fills. On each p7 enable (any level) the tick reads the client lobby in process (sess_snapshot.h meta, no HTTP);
 * only a lobby that is not private and whose match type is not social_2.0* (any case) writes quit_warning_text,
 * 3 passes 1 s apart, by two routes:
 *   (a) CScriptCS::SetString 0x4692a0 (the slot keeps 63 chars, so a longer text is cut
 *       after its last sentence that fits) on p7's ncaScript component;
 *   (b) CCanvasUICS::SetText on the warning element (quit_warning_element, default 0x0ba8656934c17e91) of the widget:
 *       quit_warning_actor if set, else each actor of p7's gamespace (then the others) whose live canvas has the three
 *       ui_mnu_popup elements (confirmationtext 0xf30458cef12c13ac, 0x0ba8656934c17e91, 0xc98cfc243010a465),
 *       keeping those whose question text holds "sure" when any does; at most QW_MAXW, resolved on pass 1.
 * Any other answer (private, social lobby, no lobby, no reader on an unverified build) keeps stock and is logged once
 * per reason; a var / element written earlier is put back to "". Log lines start "QUIT WARNING:". Off unless
 * quit_warning=1. Bytes checked at qw_start: Find<CScriptCS> + SetString differ -> route (a) off; ElementText is
 * the check (readback and the "sure" preference off when it failed). */
#define P7_QUIT_CONFIRM   0xd436ecc9f7f9164dull
#define QW_EL_WARN        0x0ba8656934c17e91ull   /* ui_mnu_popup element 2 (shown by the arena / combat branch) */
#define QW_EL_CONFIRM     0xf30458cef12c13acull   /* confirmationtext */
#define QW_EL_OTHER       0xc98cfc243010a465ull   /* element 3 */
#define QW_SCRIPT_COMP    0x6aea99eeb2bab6c4ull   /* ncaScript (default script component) */
#define QW_RVA_SET_STRING 0x4692a0u
#define QW_MAXW 4
static const uint8_t QW_CHK_SET_STRING[8] = { 0x49, 0x8b, 0xc0, 0x4d, 0x8b, 0xd9, 0x44, 0x0f };
typedef void (__fastcall *QwSetStringFn)(void *cs, uint32_t idx, uint64_t var, const char *s);
static uint64_t g_qw_el = QW_EL_WARN, g_qw_actor, g_qw_sym_var;
static char g_qw_var[64];                      /* route (a)'s text (<= 63 chars) */
static volatile LONG g_qw_ready, g_qw_var_ok, g_qw_gen, g_qw_p7_on;
static int g_qw_var_written, g_qw_el_written;  /* tick only: something to put back on a stock verdict */
static int g_qw_nw;                            /* tick only: the widgets pass 1 resolved */
static uint8_t *g_qw_wgs[QW_MAXW];
static uint64_t g_qw_wsa[QW_MAXW], g_qw_wname[QW_MAXW];

/* event hook: p7 on / off (any gamespace) */
static void qw_event(void *gs, const uint64_t *ev)
{
    if (!g_qw_ready || (ev[0] != EV_PAGE_ON && ev[0] != EV_PAGE_OFF)) return;
    __try {
        if (actor_name((uint8_t *)gs, ev[2]) != P7_QUIT_CONFIRM) return;
        int on = ev[0] == EV_PAGE_ON;
        InterlockedExchange(&g_qw_p7_on, on);
        if (on) InterlockedIncrement(&g_qw_gen);
        rlog("QUIT WARNING: p7 %s", on ? "enabled: reading the session in process" : "disabled");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

/* the live canvas of actor index k in gs */
static int qw_canvas(uint8_t *gs, uint64_t sa, Handle *c)
{
    c->cs = NULL; c->idx = 0;
    ((FindFn)(g_base + g_prof->rva_find_canvas))(c, gs, sa, SYM_CANVASUI);
    return valid(c);
}

/* an element's text on a canvas handle (ElementText), "" when none */
static void qw_el_text(Handle *c, uint64_t el, char *out, size_t cap)
{
    uint64_t id = ~0ull;
    uint8_t *inst = NULL;
    out[0] = 0;
    if (!g_text_ok) return;
    ((FindPrimFn)(g_base + g_prof->rva_find_prim))(c->cs, &id, c->idx, el, &inst);
    if (!inst || (uint32_t)id == 0xffffffffu) return;
    const char *s = ((ElementTextFn)(g_base + g_prof->rva_element_text))(inst, (uint32_t)id);
    size_t n = 0;
    for (; s && n + 1 < cap; n++) {
        char ch;
        if (!g_read(&ch, s + n, 1) || !ch) break;
        out[n] = ch;
    }
    out[n] = 0;
}

static int qw_has_sure(Handle *c)
{
    char t[160];
    for (int e = 0; e < 2; e++) {
        qw_el_text(c, e ? QW_EL_OTHER : QW_EL_CONFIRM, t, sizeof t);
        for (char *p = t; *p; p++) *p = (char)tolower((unsigned char)*p);
        if (strstr(t, "sure")) return 1;
    }
    return 0;
}

/* fills g_qw_w* with the widget actors (see the section comment); returns how many */
static int qw_resolve(void *game, uint8_t *p7gs)
{
    uint64_t n = *(uint64_t *)((uint8_t *)game + RP_O_GS_COUNT);
    uint8_t *list = *(uint8_t **)((uint8_t *)game + RP_O_GS_LIST);
    int sure[QW_MAXW], found = 0, nsure = 0;
    g_qw_nw = 0;
    for (int pass = 0; pass < 2 && !found; pass++)   /* p7's gamespace, then the others */
        for (uint64_t i = 0; list && n <= 64 && i < n && found < QW_MAXW; i++) {
            uint8_t *gs = *(uint8_t **)(list + i * 16 + 8);
            if (!gs || *(void **)(gs + g_prof->o_gs_game) != game || (gs == p7gs) != (pass == 0)) continue;
            uint8_t *tbl = *(uint8_t **)(gs + g_prof->o_gs_actors);
            uint64_t *names = tbl ? *(uint64_t **)(tbl + g_prof->o_at_names) : NULL;
            uint64_t count = tbl ? *(uint64_t *)(tbl + g_prof->o_at_count) : 0;
            for (uint64_t k = 0; names && count <= 0xfffe && k < count && found < QW_MAXW; k++) {
                Handle c;
                uint64_t sa = 0x0000ffffffff0000ull | k;
                if (g_qw_actor && names[k] != g_qw_actor) continue;
                if (!qw_canvas(gs, sa, &c) || !elem_found(&c, g_qw_el)) continue;
                if (!g_qw_actor && (!elem_found(&c, QW_EL_CONFIRM) || !elem_found(&c, QW_EL_OTHER))) continue;
                g_qw_wgs[found] = gs;
                g_qw_wsa[found] = sa;
                g_qw_wname[found] = names[k];
                sure[found] = !g_qw_actor && qw_has_sure(&c);
                nsure += sure[found];
                found++;
            }
        }
    for (int j = 0; j < found; j++) {
        if (nsure && !sure[j]) continue;
        g_qw_wgs[g_qw_nw] = g_qw_wgs[j];
        g_qw_wsa[g_qw_nw] = g_qw_wsa[j];
        g_qw_wname[g_qw_nw] = g_qw_wname[j];
        rlog("QUIT WARNING: widget %d = actor %016llx (gamespace %s, index %llu)%s", g_qw_nw, g_qw_wname[j],
             g_qw_wgs[j] == p7gs ? "of p7" : "other", g_qw_wsa[j] & 0xffff, sure[j] ? ", question text holds \"sure\"" : "");
        g_qw_nw++;
    }
    if (!g_qw_nw) rlog("QUIT WARNING: no widget actor found (element %016llx%s)", g_qw_el, g_qw_actor ? ", quit_warning_actor set" : "");
    return g_qw_nw;
}

/* route (a): quit_warning_text on p7's script; 1 = written */
static int qw_setvar(void *game, const char *s)
{
    uint64_t sa;
    uint8_t *gs = rp_actor_gs((uint8_t *)game, P7_QUIT_CONFIRM, &sa);
    Handle h = { NULL, 0 };
    if (!gs || !g_qw_var_ok) return 0;
    ((FindFn)(g_base + RP_RVA_FIND_SCRIPT))(&h, gs, sa, QW_SCRIPT_COMP);
    if (!valid(&h)) return 0;
    ((QwSetStringFn)(g_base + QW_RVA_SET_STRING))(h.cs, h.idx, g_qw_sym_var, s);
    return 1;
}

/* route (b) on every resolved widget; returns how many took the text */
static int qw_settext(const char *s)
{
    int ok = 0;
    for (int j = 0; j < g_qw_nw; j++) {
        Handle c;
        if (qw_canvas(g_qw_wgs[j], g_qw_wsa[j], &c) && elem_found(&c, g_qw_el)
            && ((SetTextFn)(g_base + g_prof->rva_canvas_set_text))(c.cs, c.idx, g_qw_el, s) != 0) ok++;
    }
    return ok;
}

static void qw_apply(void *game, int pass)
{
    __try {
        uint64_t sa;
        uint8_t *p7gs = rp_actor_gs((uint8_t *)game, P7_QUIT_CONFIRM, &sa);
        if (!p7gs) { rlog("QUIT WARNING: pass %d/3: p7 owner in no gamespace", pass); return; }
        int a = qw_setvar(game, g_qw_var);
        if (a) g_qw_var_written = 1;
        if (pass == 1) qw_resolve(game, p7gs);
        int b = qw_settext(g_qw_text);
        if (b) g_qw_el_written = 1;
        rlog("QUIT WARNING: pass %d/3: (a) var quit_warning_text %s; (b) element %016llx set on %d/%d widget(s)", pass,
             !g_qw_var_ok ? "off" : a ? "written" : "no ncaScript on p7", g_qw_el, b, g_qw_nw);
        for (int j = 0; pass == 3 && j < g_qw_nw; j++) {
            Handle c;
            char t[160];
            if (!qw_canvas(g_qw_wgs[j], g_qw_wsa[j], &c)) continue;
            qw_el_text(&c, g_qw_el, t, sizeof t);
            rlog("QUIT WARNING: readback actor %016llx element %016llx = \"%s\"", g_qw_wname[j], g_qw_el, t);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_qw_ready, 0);
        rlog("QUIT WARNING: fault in pass %d: quit warning off", pass);
    }
}

/* stock verdict after a write: quit_warning_text and the element back to "" */
static void qw_put_back(void *game)
{
    __try {
        int a = g_qw_var_written && qw_setvar(game, "");
        int b = g_qw_el_written ? qw_settext("") : 0;
        rlog("QUIT WARNING: put back: var %s, element cleared on %d widget(s)", a ? "\"\"" : "untouched", b);
        g_qw_var_written = g_qw_el_written = 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_qw_ready, 0);
        rlog("QUIT WARNING: fault putting the stock text back: quit warning off");
    }
}

/* tick (game thread): the verdict for a new p7 enable from the in-process reader (rp_public_match, report_flow.h;
 * no HTTP), then the three passes */
static void qw_tick(void *game, ULONGLONG now)
{
    static LONG seen;
    static int passes;
    static ULONGLONG next;
    static char last_why[160];
    if (!g_qw_ready) return;
    LONG gen = g_qw_gen;
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
            strcpy_s(last_why, sizeof last_why, why);
            if (g_qw_var_written || g_qw_el_written) qw_put_back(game);
        }
    }
    if (passes && now >= next) {
        if (!g_qw_p7_on) { rlog("QUIT WARNING: p7 closed with %d pass(es) left", passes); passes = 0; return; }
        qw_apply(game, 4 - passes);
        passes--;
        next = now + 1000;
    }
}

/* the route (a) text: the whole text when it fits 63 chars, else up to its last sentence end (else word) that fits */
static void qw_cut_var(void)
{
    size_t n = strlen(g_qw_text), cut = 0;
    if (n < sizeof g_qw_var) { strcpy_s(g_qw_var, sizeof g_qw_var, g_qw_text); return; }
    for (size_t i = 0; i < sizeof g_qw_var - 1; i++)
        if ((g_qw_text[i] == '.' || g_qw_text[i] == '?' || g_qw_text[i] == '!') && g_qw_text[i + 1] == ' ') cut = i + 1;
    if (!cut)
        for (size_t i = 0; i < sizeof g_qw_var - 1; i++)
            if (g_qw_text[i] == ' ') cut = i;
    if (!cut) cut = sizeof g_qw_var - 1;
    memcpy(g_qw_var, g_qw_text, cut);
    g_qw_var[cut] = 0;
}

/* after the hooks are in: options to symbols, byte checks, the reader */
static void qw_start(void)
{
    if (!g_qw_on) return;
    if (g_qw_el_s[0]) g_qw_el = (g_qw_el_s[0] == '0' && (g_qw_el_s[1] == 'x' || g_qw_el_s[1] == 'X')) ? _strtoui64(g_qw_el_s, NULL, 16) : g_csym64(g_qw_el_s);
    if (g_qw_actor_s[0]) g_qw_actor = (g_qw_actor_s[0] == '0' && (g_qw_actor_s[1] == 'x' || g_qw_actor_s[1] == 'X')) ? _strtoui64(g_qw_actor_s, NULL, 16) : g_csym64(g_qw_actor_s);
    g_qw_sym_var = g_csym64("quit_warning_text");
    qw_cut_var();
    uint8_t got[8], got2[8];
    int va = g_read(got, g_base + RP_RVA_FIND_SCRIPT, 8) && !memcmp(got, RP_CHK[0], 8)
             && g_read(got2, g_base + QW_RVA_SET_STRING, 8) && !memcmp(got2, QW_CHK_SET_STRING, 8);
    if (!va) rlog("QUIT WARNING: Find<CScriptCS> / SetString bytes differ from the profile: route (a) off");
    InterlockedExchange(&g_qw_var_ok, va);
    sess_attach();
    if (g_sess_ok != 1) rlog("QUIT WARNING: no session reader (unverified build): the warning never shows");
    InterlockedExchange(&g_qw_ready, 1);
    rlog("QUIT WARNING: ready (var %016llx route (a) %s, element %016llx, actor %s; var text \"%s\")", g_qw_sym_var,
         va ? "on" : "off", g_qw_el, g_qw_actor ? g_qw_actor_s : "by scan", g_qw_var);
}

/*: the stock card uid in the press event names the card (PC press log); pointer bits only as a fallback */
static const char *lo_card_name(uint64_t comp, long lp)
{
    if (comp == 0x35ec861316abbb34ull) return "PLAY";
    if (comp == 0x35ec861316abbb32ull) return "CREATE";
    if (comp == 0x35ec861316abbb37ull) return "JOIN";
    return (lp & 0x44) ? "PLAY" : (lp & 0x11) ? "CREATE" : (lp & 0x22) ? "JOIN" : "unknown";
}

/* 1 = ev[0] was rewritten (the standalone hook puts it back after the stock call). */
static int target_body(void *gs, const uint64_t *ev, uint32_t flags)
{
    if (report_event(ev, 1, (void *)(gs))) return 1;
    if (!g_tree) return 0;   /* stock menu: every click goes through untouched */
    if (g_lo_locked && g_p24_on && !g_p58_on && ev[0] == EV_CARD_CLICKED && actor_name((uint8_t *)gs, ev[2]) == P24_HOME) {
        /*: with p24 up and HELP down, a card press to the home owner is PLAY / CREATE / JOIN (TUTORIALS is on p58) */
        LONG lp = g_lock_ptr;
        const char *what = lo_card_name(ev[3], lp);
        int w = rewrite(ev, SYM_NOOP);
        InterlockedExchange(&g_lo_go, 1);
        rlog("LOCKOUT: %s card press (component %016llx) %s; notice queued", what, ev[3],
             w ? "consumed" : "not writable (stock route runs; the page redirect catches it)");
        return w;
    }
    if (g_lo_ours && g_p29_on && actor_name((uint8_t *)gs, ev[2]) == P29_NOTICE) {
        rlog("LOCKOUT NOTICE: event %016llx component %016llx to page 29 (left as sent)", ev[0], ev[3]);
        InterlockedExchange64(&g_lo_p29_press, (LONG64)GetTickCount64());
        return 0;
    }
    /* a card press counts by the pointer (g_lock_ptr) or, with no pointer (event press), by its component uid to p24 */
    const char *fc = ev[0] == EV_CARD_CLICKED ? lo_card_name(ev[3], g_lock_ptr & 0x33) : "";
    if (g_ftue && g_p24_on && (ev[0] == EV_EYE || (ev[0] == EV_CARD_CLICKED && ((g_lock_ptr & 0x33) || ((!strcmp(fc, "CREATE")
        || !strcmp(fc, "JOIN")) && actor_name((uint8_t *)gs, ev[2]) == P24_HOME)))
                               || (ev[0] == SYM_LINK_BTN && ev[3] == UID_MY_LINK))) {
        const char *what = ev[0] == EV_EYE ? "MY INVITE CODE eye" : ev[0] == SYM_LINK_BTN ? "INVITE LINK"
                         : !strcmp(fc, "CREATE") ? "CREATE card" : "JOIN card";
        int w = rewrite(ev, SYM_NOOP);
        rlog("FTUE: %s press %s", what, w ? "consumed" : "not writable (stock route runs)");
        return w;
    }
    if (g_combat_lock && ev[0] == EV_CARD_CLICKED && ev[3] == UID_CARD_R && actor_name((uint8_t *)gs, ev[2]) == P36_EXPER) {
        int w = rewrite(ev, SYM_NOOP);
        /* TODO store route: a purchase-locked press should enable p6 with the blackboard "hyperlink" set; that needs
         * a blackboard string write pin no recipe has pinned yet. Until then the press is only consumed. */
        rlog("COMBAT LOCK (%s): combat card press %s; no store route (blackboard string write not pinned)",
             g_cl_word[g_combat_lock], w ? "consumed" : "not writable (stock route runs)");
        return w;
    }
    if (ev[0] == EV_HELP_SAFE || (ev[0] == EV_CARD_CLICKED && g_help_safe_card && g_safe_ptr && g_p58_on)) {
        /*: the SAFE ZONE card -> p48 (no stock handler knows EV_HELP_SAFE; a card click is left as sent) */
        __try {
            int own = ev[0] == EV_HELP_SAFE;
            rlog("HELP SAFE ZONE pressed (%s to %016llx component=%016llx, pointer %ld)%s",
                 own ? "own event" : "evt_card_clicked", actor_name((uint8_t *)gs, ev[2]), ev[3], g_safe_ptr,
                 (own ? g_help_safe : 1) ? ": page 48 queued" : ": help_safezone=0, ignored");
            if (own ? g_help_safe : 1) InterlockedExchange(&g_go_p48, 1);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
        return 0;
    }
    if (ev[0] == EV_HELP_SEASON) {   /*: no stock handler knows it; the tick forwards it to the season route */
        __try {
            g_hs_ev[1] = ev[1];
            g_hs_ev[2] = ev[2];
            rlog("HELP SEASON PASS pressed (to %016llx component=%016llx)%s", actor_name((uint8_t *)gs, ev[2]), ev[3],
                 g_help_season ? ": season route queued" : ": help_season=0, ignored");
            if (g_help_season) InterlockedExchange(&g_go_season, 1);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
        return 0;
    }
    if (dot_event(ev[0])) {   /* no stock handler knows the symbol: left as sent */
        rlog("dot click event %016llx to %016llx component=%016llx", ev[0], actor_name((uint8_t *)gs, ev[2]), ev[3]);
        return 0;
    }
    if (friend_event(ev[0])) {   /*: no stock handler knows the symbol either */
        rlog("friend row click event %016llx to %016llx component=%016llx", ev[0], actor_name((uint8_t *)gs, ev[2]), ev[3]);
        return 0;
    }
    if (ev[0] != EV_NEWS && ev[0] != EV_SEASON) return 0;
    __try {
        uint64_t owner = actor_name((uint8_t *)gs, ev[2]);
        int k = (g_active && g_p24_on) ? (int)g_shown : 0;
        if (owner == P5_TILES && !g_p5_have) {   /*: a real page-5 event, the template for the HELP season send */
            memcpy(g_p5_ev, ev, sizeof g_p5_ev);
            g_p5_flags = flags;
            InterlockedExchange(&g_p5_have, 1);
            rlog("page-5 event cached (gs %016llx actor %016llx comp %016llx extra %016llx flags %u)",
                 ev[1], ev[2], ev[3], ev[4], flags);
        }
        if (ev[0] == EV_SEASON) {
            rlog("season click event to %016llx component=%016llx", owner, ev[3]);
            return 0;
        }
        if (owner != P5_TILES || k == 0) {
            rlog("CLICK news tile at slide %d (owner %016llx component=%016llx): stock news route", k, owner, ev[3]);
            return 0;
        }
        if (k == 1) {
            int w = rewrite(ev, EV_SEASON);
            rlog("CLICK news tile at slide 1: %s", w ? "REDIRECT -> season click event (stock season route, no page 8)"
                                                     : "event not writable: stock news route runs");
            return w;
        }
        int w = rewrite(ev, SYM_NOOP);
        InterlockedExchange(&g_go_p42, 1);
        rlog("CLICK news tile at slide 2: %s; page 42 queued", w ? "news event consumed (no page 8)"
                                                                 : "event not writable (page 8 will open too)");
        return w;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

typedef void (__fastcall *RemoteLogFn)(const char *json, size_t len);
static RemoteLogFn g_remote_log_orig;
static void __fastcall remote_log_hook(const char *json, size_t len) { metric_body(json, len); g_remote_log_orig(json, len); }
static void bk_metric(const char *json, size_t len, void *u) { (void)u; metric_body(json, len); }
static void __fastcall tick_hook(void *game) { tick_body(game); g_tick_orig(game); }
static void __fastcall core_hook(void *gs, const uint64_t *ev, uint32_t flags) { core_body(gs, ev); g_core_orig(gs, ev, flags); }
static void __fastcall target_hook(void *gs, const uint64_t *ev, uint32_t flags)
{
    uint64_t was = ev[0];
    int w = target_body(gs, ev, flags);
    g_target_orig(gs, ev, flags);
    if (w) rewrite(ev, was);   /* a caller that reuses the event sees its own symbol again */
}
static void bk_tick(void *game, void *u) { (void)u; tick_body(game); }
static void bk_core(void *gs, const uint64_t *ev, uint32_t flags, void *u) { (void)u; (void)flags; core_body(gs, ev); }
static void bk_target(void *gs, const uint64_t *ev, uint32_t flags, void *u) { (void)u; target_body(gs, ev, flags); }

/* ---- install (rad-links' pattern) ---- */

static const Profile *profile_for(uint32_t ts, uint32_t soi)
{
    for (size_t i = 0; i < sizeof g_profiles / sizeof g_profiles[0]; i++)
        if (ts == g_profiles[i].timestamp && soi == g_profiles[i].size_of_image) return &g_profiles[i];
    return NULL;
}

static void put_jmp(uint8_t *at, void *target)
{
    at[0] = 0xFF; at[1] = 0x25;
    *(uint32_t *)(at + 2) = 0;
    *(uint64_t *)(at + 6) = (uint64_t)target;
}

static int in_module(const void *p)
{
    HMODULE m;
    return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              (LPCWSTR)p, &m);
}

/* Stock prologue: 15 bytes into a trampoline. Already hooked (jmp [rip+0] into a loaded module):
 * take over its target and call it in turn. */
static int hook_site(uint32_t rva, const uint8_t *prologue, size_t steal, void *hook, void **orig)
{
    uint8_t *site = g_base + rva;
    DWORD old;
    if (!memcmp(site, prologue, steal)) {
        uint8_t *tramp = VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!tramp) return 0;
        memcpy(tramp, site, steal);
        put_jmp(tramp + steal, site + steal);
        VirtualProtect(tramp, 64, PAGE_EXECUTE_READ, &old);
        *orig = tramp;
        if (!VirtualProtect(site, steal, PAGE_EXECUTE_READWRITE, &old)) return 0;
        put_jmp(site, hook);
        memset(site + 14, 0x90, steal - 14);
    } else if (site[0] == 0xFF && site[1] == 0x25 && !*(uint32_t *)(site + 2) && in_module(*(void **)(site + 6))) {
        *orig = *(void **)(site + 6);
        if (!VirtualProtect(site, 15, PAGE_EXECUTE_READWRITE, &old)) return 0;
        *(uint64_t *)(site + 6) = (uint64_t)hook;
        rlog("chained onto the hook already at %p (-> %p)", site, *orig);
    } else {
        return 0;
    }
    VirtualProtect(site, steal, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, steal);
    return 1;
}

static int install_tick(void)
{
    uint64_t *slot = (uint64_t *)(g_base + g_prof->rva_tick_slot);
    void *cur = (void *)*slot;
    if (cur != g_base + g_prof->rva_tick_orig) {
        if (!in_module(cur)) { rlog("tick slot holds %p, in no module: refusing", cur); return 0; }
        rlog("tick slot already hooked (%p): chaining", cur);
    }
    g_tick_orig = (TickFn)cur;
    DWORD old;
    if (!VirtualProtect(slot, 8, PAGE_READWRITE, &old)) return 0;
    InterlockedExchange64((volatile LONG64 *)slot, (LONG64)(uintptr_t)tick_hook);
    VirtualProtect(slot, 8, old, &old);
    return 1;
}

static int fn_bytes_ok(void)
{
    for (int i = 0; i < 8; i++) {
        uint8_t got[8];
        if (!g_read(got, g_base + g_prof->chk_rva[i], 8) || memcmp(got, g_prof->chk_bytes[i], 8)) {
            rlog("function at rva %06x differs from the profile: refusing", g_prof->chk_rva[i]);
            return 0;
        }
    }
    int ok2 = 1;
    for (int i = 0; i < 2; i++) {
        uint8_t got[8];
        if (!g_read(got, g_base + g_prof->chk2_rva[i], 8) || memcmp(got, g_prof->chk2_bytes[i], 8)) {
            rlog("colour setter at rva %06x differs from the profile: tab colours / square dots off", g_prof->chk2_rva[i]);
            ok2 = 0;
        }
    }
    InterlockedExchange(&g_color_ok, ok2);
    uint8_t got3[8];
    int ok3 = g_read(got3, g_base + g_prof->rva_element_text, 8) && !memcmp(got3, g_prof->chk3_bytes, 8);
    if (!ok3) rlog("ElementText at rva %06x differs from the profile: footer copy off", g_prof->rva_element_text);
    InterlockedExchange(&g_text_ok, ok3);
    return 1;
}

static void init_syms(void)
{
    SYM_TEXT = g_csym64("text");
    SYM_BG = g_csym64("background_image");
    SYM_NOOP = g_csym64("evt_radmenu_carousel_consumed");
    SYM_LINK_BTN = g_csym64("evt_link_button_clicked");
    if (g_csym64("evt_ffa_my_code_eye_clicked") != EV_EYE) rlog("WARNING: eye event symbol differs from csym64 of its name");
    SYM_DOT_ON = g_csym64("dot_on");
    SYM_DOT_OFF = g_csym64("dot_off");
    SYM_CODETEXT = g_csym64("codetext");
    SYM_CODETEXT_OFF = g_csym64("codetext_off");
    SYM_BACKGROUND = g_csym64("Background");
    SYM_HIGHLIGHT = g_csym64("highlight");
    SYM_DOT_BOX = g_csym64("dot_box");
    if (SYM_BACKGROUND != SYM_BACKGROUND_K || SYM_DOT_BOX != SYM_DOT_BOX_K
        || g_csym64("evt_ffa_help_season_clicked") != EV_HELP_SEASON
        || g_csym64("evt_ffa_help_safezone_clicked") != EV_HELP_SAFE || g_csym64("evt_card_clicked") != EV_CARD_CLICKED)
        rlog("WARNING: symbols differ from csym64 of their names (Background %016llx dot_box %016llx)",
             SYM_BACKGROUND, SYM_DOT_BOX);
    if (g_csym64("evt_ffa_friend_row1_clicked") != EV_FRIEND[0])
        rlog("WARNING: friend row event symbols differ from csym64 of their names");
    if (g_csym64("evt_ffa_news_dot0_clicked") != EV_DOT[0] || g_csym64("ffa_news_dot0") != ACTOR_DOT[0])
        rlog("WARNING: dot symbols differ from csym64 of their names");
    rlog("syms: text %016llx background_image %016llx; evt_news_info_clicked %016llx (profile %016llx)",
         SYM_TEXT, SYM_BG, g_csym64("evt_news_info_clicked"), EV_NEWS);
}

static BOOL attach(void)
{
    char path[MAX_PATH];
    local_path(path, sizeof path, "radmenu.log");
    if (path[0]) {
        strcpy_s(g_log_path, sizeof g_log_path, path);
        log_open(OPEN_ALWAYS);
    }
    rlog("rad-menu attach, pid %lu", GetCurrentProcessId());
    read_options();
    if (!g_enabled) { rlog("disabled (radmenu.ini enabled=0): no hook installed, fully inert"); return TRUE; }
    g_base = (uint8_t *)GetModuleHandleA(NULL);
    if (g_brokered) { rlog("evrmod broker loading: hooks deferred to evrmod_plugin_init"); return TRUE; }
    IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(g_base + ((IMAGE_DOS_HEADER *)g_base)->e_lfanew);
    if (!(g_prof = profile_for(nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage))) {
        rlog("unsupported image (TimeDateStamp %08lx SizeOfImage %08lx): rad-menu off",
             nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage);
        return TRUE;
    }
    rlog("profile: %s, image base %p", g_prof->name, g_base);
    init_syms();
    if (!fn_bytes_ok()) return TRUE;
    if (!hook_site(g_prof->rva_send_core, g_prof->send_prologue, 15, (void *)core_hook, (void **)&g_core_orig)
        || !hook_site(g_prof->rva_send_target, g_prof->send_prologue, 15, (void *)target_hook, (void **)&g_target_orig)) {
        rlog("an event site is neither stock nor a known hook: rad-menu off");
        return TRUE;
    }
    if (!install_tick()) return TRUE;
    if (!hook_site(g_prof->rva_remote_log, g_prof->remote_log_prologue, 16, (void *)remote_log_hook,
                   (void **)&g_remote_log_orig))
        rlog("friends: remote-log submit is neither stock nor a known hook: no user_id from metrics");
    friends_start();
    report_start();
    qw_start();
    rlog("ready: page events %p, clicks %p, tick slot %p", g_base + g_prof->rva_send_core,
         g_base + g_prof->rva_send_target, g_base + g_prof->rva_tick_slot);
    return TRUE;
}

__declspec(dllexport) void evrmod_plugin_init(const EvrmodApi *api)
{
    int rc;
    if (!g_brokered || !g_enabled) return;
    if (!api || api->abi_version < 2) { rlog("broker ABI < 2: rad-menu off (needs the global-event site)"); return; }
    const EvrmodBuild *b = api->build();
    if (!b || !b->is_known || !(g_prof = profile_for(b->time_date_stamp, b->size_of_image))) {
        rlog("unsupported image per the broker: rad-menu off");
        return;
    }
    g_base = (uint8_t *)b->image_base;
    g_csym64 = api->csym64;
    g_read = api->read;
    rlog("profile: %s, image base %p (from the evrmod broker)", g_prof->name, g_base);
    init_syms();
    if (!fn_bytes_ok()) return;
    if ((rc = api->register_tick(bk_tick, NULL)) || (rc = api->register_send_global(bk_core, NULL))
        || (rc = api->register_send_event(bk_target, NULL))) {
        rlog("the broker refused a hook (%d): rad-menu off", rc);
        return;
    }
    rlog("ready: tick, page events and clicks registered with the evrmod broker (ABI %u)", api->abi_version);
    if ((rc = api->register_metric_log(bk_metric, NULL)))
        rlog("friends: the broker refused the metric hook (%d): no user_id from metrics", rc);
    friends_start();
    report_start();
    qw_start();
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        g_brokered = evrmod_broker_loading() >= 2;
        return attach();
    }
    return TRUE;
}
