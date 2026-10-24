/* rad-links.dll — invite codes for the shipped CREATE GAME / JOIN VIA CODE wizard.
 *
 * Echo VR PC final (6323983201049540) ships the create / join wizard (mnu_master pages 34–41) as a client-side
 * stub: no page talks to the services. This plugin watches the wizard's stock script events, keeps
 * the player's choices, and turns two presses into requests to our EchoRelay services port (/invites):
 *   p40 CREATE INVITE CODE & LINK  -> POST /invites                 -> the code (log; P2 shows it on p41)
 *   p41 JOIN                       -> join the invite we created
 *   p34 ENTER (JOIN VIA CODE)      -> GET /invites/{code}           -> join its link
 *     404 / 410 / 403 / no host    -> a message in the field's title bar (INVALID CODE, CODE EXPIRED, ...)
 *     403 "needs a password"       -> title ENTER PASSWORD; the next ENTER is the password: GET ?password=
 * The choices come from the card events (SendComponentEvent); the settings, the typed code and the player id come
 * from the click-metric JSON the stock pages already log (remote-log submit 0x113960; the same lines reach the
 * services as SNSRemoteLogSetv3): create_invite {game_type, Password, Server_region, Expires_after},
 * join_via_code / join {join_code}, and user_id in every line.
 *
 * Threads: the event hook only records and queues; HTTP runs on one worker thread (WinHTTP, 5 s timeouts); a game
 * action (the join) is queued back and run from the per-frame CR15Game tick slot, never on the worker.
 * Log: <exe dir>\_local\radlinks.log. Services: <exe dir>\_local\config.json "apiservice_host" (the address the
 * client already logs in to). No pipe, no test commands: this DLL is meant for players.
 *
 * Two ways to load it. Under the evrmod broker (winmm.dll, ABI >= 2) it registers its three callbacks instead and
 * the broker owns the hook sites, so load order against other plugins does not matter (evrmod_plugin_init). Under
 * the broker it also takes the build identity (api->build), the CSymbol64 hash (api->csym64) and its fault-guarded
 * reads (api->read) from the broker. Standalone (injected into the process some other way) it hooks the same tick
 * slot and SendComponentEvent prologue itself, chains onto a hook another tool already put there, and uses the
 * shared PC profile header (evrmod_profile_pc.h) and its own fault guard.
 *
 * Every RVA comes from static disassembly of that one exe; any other image is refused. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include "evrmod.h"               /* the winmm.dll broker API (build.cmd adds its include dir) */
#include "evrmod_profile_pc.h"   /* build stamp + CSymbol64 */

#pragma comment(lib, "winhttp.lib")

typedef struct {
    const char *name;
    uint32_t timestamp, size_of_image;
    uint32_t rva_tick_slot, rva_tick_orig;   /* CR15Game vftable slot +0x150 (per frame) and its stock target */
    uint32_t rva_send_target;                /* CncaGameSpace::SendComponentEvent(SComponentEvent const&, uint) */
    uint8_t  target_prologue[15];
    uint32_t rva_remote_log;                 /* void(const char *json, size_t len): every remote-log JSON line */
    uint8_t  remote_log_prologue[16];
    uint32_t o_gs_id, o_gs_actors, o_at_names, o_at_count;
    /* the join: CR15Game* global, game -> netgame -> client lobby, their states, Join / LeaveGame / Cancel */
    uint32_t rva_game, o_game_netgame, o_ng_lobby, o_lobby_state;
    uint32_t rva_join, rva_leave, rva_cancel;
    uint8_t  join_bytes[8], leave_bytes[8], cancel_bytes[8];
    /* from the main menu: the -lobbyid block of CR15Game (GUID, team, gametype) and "Beginning multiplayer" */
    uint32_t o_game_lobbyid, o_game_team, o_game_gametype, rva_begin_mp;
    uint8_t  begin_mp_bytes[8];
    uint32_t rva_mp_transition;              /* sends g_menu_mp_transition to the menu gamespace (game+0x7af8) */
    uint8_t  mp_transition_bytes[8];
    /* showing a page (as the stock page-enable path does): CncaGame gamespace list / count, CncaGameSpace
     * game pointer and actor table, the page-component lookup, its check slot, Set/ClearInvokeImmediate, EnablePage */
    uint32_t o_gs_list, o_gs_count, o_gs_game;
    uint32_t rva_find_page_cs, vt_cs_valid, rva_invoke_set, rva_invoke_clear, rva_enable_page;
    /* write-back of the invite texts (found by disassembly, matched by shape to the Quest build's dynsym functions,
     * which are verified on device): CLocalComponentHandle<CScriptCS>::Find, CScriptCS::SetString, the CCanvasUICS Find, SetText */
    uint32_t rva_find_script, rva_set_string, rva_find_canvas, rva_canvas_set_text;
    uint8_t  find_script_bytes[8], set_string_bytes[8], find_canvas_bytes[8], canvas_set_text_bytes[8];
} Profile;

static const Profile g_profiles[] = {
    {
        EVRMOD_PC_FINAL_NAME,
        EVRMOD_PC_FINAL_TS, EVRMOD_PC_FINAL_SOI,
        0x16d1100, 0x5194f0,
        0x50fb60,
        { 0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x6c, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18 },
        0x113960,                            /* only caller 0x1cd129 (JSON-log submit 0x1ccf40) */
        { 0x48, 0x89, 0x5c, 0x24, 0x08,              /* mov [rsp+8],rbx   */
          0x57,                                      /* push rdi          */
          0x48, 0x83, 0xec, 0x30,                    /* sub rsp,30h       */
          0x48, 0x8b, 0xda,                          /* mov rbx,rdx       */
          0x48, 0x8b, 0xf9 },                        /* mov rdi,rcx       */
        0x58, 0x370, 0x80, 0xa8,             /* page / gamespace offsets, verified on device */
        0x20a0478, 0x8518, 0x40, 0x448,      /* found by disassembly */
        0x178700, 0x1796b0, 0x15b7a0,        /* CR15NetClientLobby::Join, CR15NetGame::LeaveGame, lobby cancel */
        { 0x48, 0x89, 0x5c, 0x24, 0x10, 0x48, 0x89, 0x6c },
        { 0x40, 0x55, 0x57, 0x48, 0x8b, 0xec, 0x48, 0x83 },
        { 0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x6c },
        0x8540, 0x8560, 0x8568, 0xfddd0,     /* 0xfddd0 = what the menu's PLAY task (0xf53380) runs; -> 0xfda70 */
        { 0x48, 0x83, 0xec, 0x78, 0x33, 0xc0, 0xc7, 0x44 },
        0x104240,                            /* what /join_session calls before Join */
        { 0x40, 0x53, 0x48, 0x83, 0xec, 0x50, 0x48, 0x8b },
        0x1f50, 0x1f80, 0x08,
        0xc06230, 0x1c8, 0x510540, 0x4e8960, 0xa697a0,
        0x168340, 0x4692a0, 0x91daf0, 0x37d470,   /* the two Finds share one template's bytes (as 0xc06230) */
        { 0x48, 0x89, 0x5c, 0x24, 0x08, 0x4c, 0x89, 0x44 },
        { 0x49, 0x8b, 0xc0, 0x4d, 0x8b, 0xd9, 0x44, 0x0f },
        { 0x48, 0x89, 0x5c, 0x24, 0x08, 0x4c, 0x89, 0x44 },
        { 0x40, 0x53, 0x48, 0x83, 0xec, 0x40, 0x49, 0x8b },
    },
};

/* Wizard page owners and element ids (verified on device @ 6323983201049540). */
#define P34_JOIN_CODE  0x2d50833b445fe66cull
#define P35_LINK       0xb7eca761e624f927ull
#define P36_EXPERIENCE 0x6a9e38e434a28911ull
#define P37_ARENA_MODE 0x1726509ab6803498ull
#define P38_COMBAT_MODE 0x04a63327518dd0e4ull
#define P39_MAP        0x5d4b7b11b22d1872ull
#define P40_SETTINGS   0xeb6ff9ddc99fe9f5ull
#define P41_SHARE      0xe8b448bf5e62ef7dull
#define UID_CREATE     0xf307e1db6745521cull  /* CREATE INVITE CODE & LINK on p40; the same element reads JOIN on p41 */
#define UID_CARD_L     0x35ec861316acbb37ull  /* card actors left / middle / right (p36–p39) */
#define UID_CARD_M     0x35ec861316acbb34ull
#define UID_CARD_R     0x35ec861316acbb35ull
#define COMP_TEXT      0x261ea41076011186ull  /* p34 ENTER CODE field component */
#define SYM_NONE       0xffffffffffffffffull
#define LEVEL_MNU_MASTER 0xac360e41e4ede056ull  /* the menu gamespace's level id */
#define P51_STATUS     0xb245345073f0d3b3ull  /* mnu_master page 51: STATUS + tips, what PLAY shows while connecting */
/* Invite texts (read from the mnu_master CScriptCR / canvas records of 6323983201049540): p41 INVITE CODE = actor 0xa9bd…
 * (script ui_mnu_deeplinking_invite_code; its UpdateCode() refuses outside calls) and a page-less twin; p41
 * EXPIRES; the p35 link. Each is the actor's canvasui element; no stock script fills them. */
#define ACTOR_INVITE_CODE  0xa9bd0cb4e6f2004cull
#define ACTOR_INVITE_CODE2 0xe28a9b96434eeff4ull
#define ACTOR_EXPIRES      0x0ec8e16986ef88c8ull
#define ACTOR_LINK_VIEW    0xc3f9f167b2a06043ull
#define SYM_NCA_SCRIPT     0x6aea99eeb2bab6c4ull
#define SYM_CURR_TEXT      0x7de33d6d994ad2d7ull   /* curr_text_string, char[64]: the eye toggle redraws from it */
#define SYM_CANVASUI       0x35ec9a0128ec916full
#define SYM_CODETEXT       0x35e290123dfa9c72ull
#define SYM_EXPIRESTEXT    0xd38c12d22d3552ebull
#define SYM_LINKTEXT       0x3657cd7405999810ull
/* p34 messages. The ENTER CODE field is actor 0xad97… (CCanvasUICR mnu_master record 71),
 * canvas ui_mnu_deeplinking_text_input 0x487df6d160c8da42: title_text (the bar above the field, shipped "TITLE TEXT";
 * the join page's script sets it, its literal is "ENTER CODE") and inputfield (the R15TextEntryBehavior's own text,
 * the player's typing: left alone). Read from the PC 6323983201049540 resources; the Quest build has the same bytes. */
#define ACTOR_CODE_FIELD   0xad97e58a7b13425full
#define SYM_TITLETEXT      0xa8452de53657bd6eull
#define P34_TITLE          "ENTER CODE"
#define PASSWORD_WINDOW_MS 120000   /* how long a password prompt waits for its ENTER */

static uint64_t SYM_CARD, SYM_BTN, SYM_TEXT, SYM_LINK_BTN, SYM_BACK, SYM_CANCEL;

typedef void (__fastcall *TickFn)(void *game);
typedef void (__fastcall *SendCoreFn)(void *gs, const uint64_t *event, uint32_t flags);
typedef void (__fastcall *RemoteLogFn)(const char *json, size_t len);
typedef void (__fastcall *JoinFn)(void *lobby, uint64_t gametype, const void *guid, uint16_t team);
typedef void (__fastcall *LeaveFn)(void *netgame, int reason);
typedef void (__fastcall *CancelFn)(void *lobby, uint32_t status);
typedef void (__fastcall *BeginMpFn)(void *game);
typedef void (__fastcall *GameFn)(void *game);
typedef struct { void *cs; uint32_t idx; } PageHandle;   /* {CR15UIPage2CS*, component index} */
typedef void *(__fastcall *FindPageFn)(PageHandle *out, void *gs, uint64_t actor, uint64_t component);
typedef uint8_t (__fastcall *ValidFn)(void *cs, uint32_t idx);
typedef void (__fastcall *EnablePageFn)(void *cs, uint32_t idx);
typedef void (__fastcall *SetStringFn)(void *cs, uint32_t idx, uint64_t sym, const char *s);
typedef uint32_t (__fastcall *CanvasSetTextFn)(void *cs, uint32_t idx, uint64_t element, const char *s);   /* 0 = not written */

static const Profile *g_prof;
static uint8_t *g_base;
static TickFn g_tick_orig;
static SendCoreFn g_target_orig;
static RemoteLogFn g_remote_log_orig;
static int g_brokered;   /* loaded by a live evrmod broker (ABI >= 2): it owns the sites */

/* Fault-guarded copy, the contract of the broker's api->read (evrmod.h); standalone fallback. */
static int self_read(void *dst, const void *src, size_t n)
{
    __try { memcpy(dst, src, n); return 1; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
/* Hash and guarded read: the shared header / self_read standalone, the broker's services under it. */
static uint64_t (*g_csym64)(const char *s) = evrmod_csym64;
static int (*g_read)(void *dst, const void *src, size_t n) = self_read;
static HANDLE g_log = INVALID_HANDLE_VALUE;
static SRWLOCK g_log_lock = SRWLOCK_INIT;

static void rlog(const char *fmt, ...)
{
    char buf[1536];
    SYSTEMTIME t;
    GetLocalTime(&t);
    int n = _snprintf_s(buf, sizeof buf, _TRUNCATE, "%02u:%02u:%02u.%03u ",
                        t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_list ap;
    va_start(ap, fmt);
    int m = _vsnprintf_s(buf + n, sizeof buf - n - 2, _TRUNCATE, fmt, ap);
    va_end(ap);
    n += m < 0 ? (int)strlen(buf + n) : m;
    buf[n++] = '\r';
    buf[n++] = '\n';
    if (g_log == INVALID_HANDLE_VALUE) return;
    AcquireSRWLockExclusive(&g_log_lock);
    DWORD w;
    WriteFile(g_log, buf, (DWORD)n, &w, NULL);
    ReleaseSRWLockExclusive(&g_log_lock);
}

/* ---- wizard state: written by the event hook (game thread), read by the worker under the lock ---- */

enum { EXP_NONE, EXP_LOBBY, EXP_ARENA, EXP_COMBAT };
enum { MODE_NONE, MODE_CUSTOM, MODE_AI, MODE_PAYLOAD, MODE_CAPTURE, MODE_RANDOM };

typedef struct {
    int experience, mode, map;   /* map: 0 none, 1 left card, 2 right card (p39 fills them by mode) */
    char code[16];               /* the code our last create got (p41 JOIN joins it) */
    char user[64];               /* user_id of the metric lines (the logged-in player) */
    char gametype[64];           /* of the invite we created */
    char link[48];
} Wizard;

static Wizard g_wiz;
static SRWLOCK g_wlock = SRWLOCK_INIT;

/* Wizard -> session (the gametype names are in the exe; the card -> level pairing is inferred, except SURGE =
 * mpl_combat_gauss from p39's own symbols). Payload vs capture point is the level: one private combat gametype. */
typedef struct { int experience, mode, map; const char *gametype, *level; } Route;
static const Route g_routes[] = {
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

static int card_index(uint64_t uid)   /* 1 left, 2 middle, 3 right, 0 other */
{
    return uid == UID_CARD_L ? 1 : uid == UID_CARD_M ? 2 : uid == UID_CARD_R ? 3 : 0;
}

/* ---- worker jobs (event hook -> worker) and game actions (worker -> tick) ---- */

enum { JOB_CREATE = 1, JOB_RESOLVE };
typedef struct { int kind; char code[16]; Wizard wiz; char password[64], region[32]; int expires_s; } Job;
enum { ACT_JOIN = 1, ACT_SHOW_CODE, ACT_P34_TEXT };
typedef struct { int kind; char link[48], why[64], gametype[64]; int tries; ULONGLONG begin_at; } Action;   /* P34_TEXT: why = the text */

/* The password prompt: the code whose resolve answered 403 "needs a password" (worker writes, hooks read; g_wlock). */
static struct { char code[16]; ULONGLONG until; } g_pw;

#define JCAP 8
static Job g_jobs[JCAP];
static int g_njobs;
static SRWLOCK g_jlock = SRWLOCK_INIT;
static HANDLE g_job_event;

#define ACAP 8
static Action g_acts[ACAP];
static int g_nacts;
static SRWLOCK g_alock = SRWLOCK_INIT;

static void push_job(const Job *j)
{
    AcquireSRWLockExclusive(&g_jlock);
    if (g_njobs < JCAP) g_jobs[g_njobs++] = *j;
    else rlog("job queue full: dropped job %d", j->kind);
    ReleaseSRWLockExclusive(&g_jlock);
    SetEvent(g_job_event);
}

static void push_action(const Action *a)
{
    AcquireSRWLockExclusive(&g_alock);
    if (g_nacts < ACAP) g_acts[g_nacts++] = *a;
    else rlog("action queue full: dropped action %d", a->kind);
    ReleaseSRWLockExclusive(&g_alock);
}

/* ---- the join ---- */

/* Shows a mnu_master page as the stock R15UIPage2EnablePageNode does: child pages follow,
 * exclusive siblings close. Game thread only. 0 = done. */
static int show_page(uint8_t *game, uint64_t owner)
{
    uint64_t n = *(uint64_t *)(game + g_prof->o_gs_count);
    uint8_t *list = *(uint8_t **)(game + g_prof->o_gs_list);
    for (uint64_t i = 0; list && n <= 64 && i < n; i++) {
        uint8_t *gs = *(uint8_t **)(list + i * 16 + 8);
        if (!gs || *(void **)(gs + g_prof->o_gs_game) != game || *(uint64_t *)(gs + g_prof->o_gs_id) != LEVEL_MNU_MASTER)
            continue;
        uint8_t *tbl = *(uint8_t **)(gs + g_prof->o_gs_actors);
        uint64_t *names = tbl ? *(uint64_t **)(tbl + g_prof->o_at_names) : NULL;
        uint64_t count = tbl ? *(uint64_t *)(tbl + g_prof->o_at_count) : 0;
        for (uint64_t k = 0; names && count <= 0xfffe && k < count; k++) {
            if (names[k] != owner) continue;
            PageHandle h = { NULL, 0 };
            ((FindPageFn)(g_base + g_prof->rva_find_page_cs))(&h, gs, 0x0000ffffffff0000ull | k, SYM_NONE);
            if (!h.cs || !((ValidFn)(*(void ***)h.cs)[g_prof->vt_cs_valid / 8])(h.cs, h.idx)) return 1;
            ((GameFn)(g_base + g_prof->rva_invoke_set))(game);
            ((EnablePageFn)(g_base + g_prof->rva_enable_page))(h.cs, h.idx);
            ((GameFn)(g_base + g_prof->rva_invoke_clear))(game);
            return 0;
        }
    }
    return 1;
}

/* The mnu_master gamespace and the index of `actor` in it, or NULL. Game thread only. */
static uint8_t *menu_actor(uint8_t *game, uint64_t actor, uint64_t *idx)
{
    uint64_t n = *(uint64_t *)(game + g_prof->o_gs_count);
    uint8_t *list = *(uint8_t **)(game + g_prof->o_gs_list);
    for (uint64_t i = 0; list && n <= 64 && i < n; i++) {
        uint8_t *gs = *(uint8_t **)(list + i * 16 + 8);
        if (!gs || *(void **)(gs + g_prof->o_gs_game) != game || *(uint64_t *)(gs + g_prof->o_gs_id) != LEVEL_MNU_MASTER)
            continue;
        uint8_t *tbl = *(uint8_t **)(gs + g_prof->o_gs_actors);
        uint64_t *names = tbl ? *(uint64_t **)(tbl + g_prof->o_at_names) : NULL;
        uint64_t count = tbl ? *(uint64_t *)(tbl + g_prof->o_at_count) : 0;
        for (uint64_t k = 0; names && count <= 0xfffe && k < count; k++)
            if (names[k] == actor) { *idx = k; return gs; }
        return NULL;
    }
    return NULL;
}

/* One canvas text: actor's canvasui element <- s. 1 = written. Game thread. */
static int canvas_text(uint8_t *game, uint64_t actor, uint64_t element, const char *s)
{
    uint64_t k;
    uint8_t *gs = menu_actor(game, actor, &k);
    PageHandle c = { NULL, 0 };
    if (!gs) return 0;
    ((FindPageFn)(g_base + g_prof->rva_find_canvas))(&c, gs, 0x0000ffffffff0000ull | k, SYM_CANVASUI);
    if (!c.cs || !((ValidFn)(*(void ***)c.cs)[g_prof->vt_cs_valid / 8])(c.cs, c.idx)) return 0;
    return ((CanvasSetTextFn)(g_base + g_prof->rva_canvas_set_text))(c.cs, c.idx, element, s) != 0;
}

/* P2 texts of the last created invite (worker writes, tick reads; under g_wlock) */
static struct { char code[16], expires[32], link[64]; } g_p2;

/* P2: write the created invite into the wizard (as radlinks_quest.c): p41 INVITE CODE (the canvas element, and
 * the script's curr_text_string) and EXPIRES, and the p35 link. Applied three times, 1 s apart (SetString is ignored
 * until the script is live; the page's own Enable may redraw after us). Game thread. 0 = applied, 1 = retry. */
static int show_code(uint8_t *game, Action *a)
{
    if (a->begin_at && GetTickCount64() < a->begin_at) return 1;
    char code[16], expires[32], link[64];
    AcquireSRWLockShared(&g_wlock);
    memcpy(code, g_p2.code, sizeof code);
    memcpy(expires, g_p2.expires, sizeof expires);
    memcpy(link, g_p2.link, sizeof link);
    ReleaseSRWLockShared(&g_wlock);
    static const uint64_t actors[] = { ACTOR_INVITE_CODE, ACTOR_INVITE_CODE2 };
    int done = 0;
    for (size_t i = 0; i < sizeof actors / sizeof actors[0]; i++) {
        uint64_t k;
        uint8_t *gs = menu_actor(game, actors[i], &k);
        PageHandle h = { NULL, 0 };
        if (!gs) continue;
        ((FindPageFn)(g_base + g_prof->rva_find_script))(&h, gs, 0x0000ffffffff0000ull | k, SYM_NCA_SCRIPT);
        if (h.cs && ((ValidFn)(*(void ***)h.cs)[g_prof->vt_cs_valid / 8])(h.cs, h.idx))
            ((SetStringFn)(g_base + g_prof->rva_set_string))(h.cs, h.idx, SYM_CURR_TEXT, code);
        done += canvas_text(game, actors[i], SYM_CODETEXT, code);
    }
    if (!done) return 1;
    int ex = canvas_text(game, ACTOR_EXPIRES, SYM_EXPIRESTEXT, expires);
    int lk = canvas_text(game, ACTOR_LINK_VIEW, SYM_LINKTEXT, link);
    a->gametype[0]++;   /* applications so far (gametype is unused by SHOW_CODE) */
    rlog("code %s written to p41 (%d canvas(es), expires %s, link %s; pass %d)", code, done, ex ? "yes" : "no",
         lk ? "yes" : "no", a->gametype[0]);
    if (a->gametype[0] >= 3) return 0;
    a->begin_at = GetTickCount64() + 1000;
    a->tries = 0;
    return 1;
}

/* p34: a message in the code field's title bar (the code field's own message). Applied three times, 1 s apart, as
 * show_code (the page's own Enable may redraw the title after us). Game thread. 0 = applied, 1 = retry. */
static int p34_text(uint8_t *game, Action *a)
{
    if (a->begin_at && GetTickCount64() < a->begin_at) return 1;
    if (!canvas_text(game, ACTOR_CODE_FIELD, SYM_TITLETEXT, a->why)) return 1;
    if (!a->gametype[0]++) rlog("p34 title: \"%s\"", a->why);
    if (a->gametype[0] >= 3) return 0;
    a->begin_at = GetTickCount64() + 1000;
    a->tries = 0;
    return 1;
}

static volatile LONG g_p34_dirty;   /* the title holds one of our messages */
static void p34_say(const char *text)   /* any thread */
{
    InterlockedExchange(&g_p34_dirty, strcmp(text, P34_TITLE) != 0);
    Action a = { ACT_P34_TEXT };
    strcpy_s(a.why, sizeof a.why, text);
    push_action(&a);
}

/* "cf741582-eb0a-4a14-8636-62abffe43e5a" -> the 16-byte Windows GUID layout (u32, u16, u16, u8[8]) that -lobbyid
 * and the session ids use. 0 on a malformed string. */
static int parse_guid(const char *s, uint8_t g[16])
{
    unsigned a, b, c, d[8];
    if (strlen(s) != 36 || sscanf_s(s, "%8x-%4x-%4x-%2x%2x-%2x%2x%2x%2x%2x%2x", &a, &b, &c, &d[0], &d[1], &d[2], &d[3],
                                    &d[4], &d[5], &d[6], &d[7]) != 11)
        return 0;
    memcpy(g, &a, 4);
    uint16_t b16 = (uint16_t)b, c16 = (uint16_t)c;
    memcpy(g + 4, &b16, 2);
    memcpy(g + 6, &c16, 2);
    for (int i = 0; i < 8; i++) g[8 + i] = (uint8_t)d[i];
    return 1;
}

/* Join flow (found by disassembly @ 6323983201049540; NetGame state [ng+0]: 3 logged in, 4 loading global, 5 lobby, 6-8 loading,
 * 9 in game). On the game thread:
 * - state 3 = the main menu (device-observed: no client lobby object yet, ng+0x40 = 0). Do what `-lobbyid` does:
 *   put the GUID in CR15Game's -lobbyid block (+0x8540, gametype +0x8568) and run "Beginning multiplayer" (0xfddd0,
 *   the function the menu's PLAY task runs). It takes the -lobbyid branch (0x15a300: netgame+0x70 = GUID), and on
 *   entering state 5 the netgame joins that GUID (0x16a970 -> Join) instead of matchmaking. The block is cleared by
 *   0xfda70; the gametype word is restored once the join has left state 3. As PLAY does, the STATUS page (mnu_master
 *   51) is shown first and multiplayer begins 1 s later, with g_menu_mp_transition raised.
 * - state 5 / 9: CR15NetClientLobby::Join(lobby, gametype, &guid, team), as /join_session (state 9, after
 *   LeaveGame) does.
 * Returns 0 = done, 1 = not now (retry). */
static uint64_t g_saved_gametype, g_restore_gametype;   /* restore game+0x8568 when the netgame leaves state 3 */
enum { NG_LOGGED_IN = 3, NG_LOBBY = 5, NG_IN_GAME = 9 };
static char g_wait_why[96];   /* why the last join_link said "not now" (logged once per change) */
static int join_link(Action *a)
{
    uint8_t guid[16];
    if (!parse_guid(a->link, guid)) { rlog("join %s: not a GUID", a->link); return 0; }
    uint8_t *game = *(uint8_t **)(g_base + g_prof->rva_game), *ng = game ? *(uint8_t **)(game + g_prof->o_game_netgame) : NULL;
    uint8_t *lobby = ng ? *(uint8_t **)(ng + g_prof->o_ng_lobby) : NULL;
    char why[96];
    _snprintf_s(why, sizeof why, _TRUNCATE, "game=%p netgame=%p lobby=%p", game, ng, lobby);
    if (ng && *(int32_t *)ng == NG_LOGGED_IN && !g_restore_gametype) {
        why[0] = 0;   /* the main menu */
    } else if (lobby) {
        int st = *(int32_t *)ng, ls = *(int32_t *)(lobby + g_prof->o_lobby_state);
        _snprintf_s(why, sizeof why, _TRUNCATE, "netgame state %d, lobby state %d", st, ls);
        if ((st == NG_LOBBY || st == NG_IN_GAME) && ls != 2 && ls != 3 && ls != 7 && ls != 8)
            why[0] = 0;   /* not loading, no lobby request under way */
    }
    if (why[0]) {
        if (strcmp(why, g_wait_why)) rlog("join %s: waiting (%s)", a->link, why);
        strcpy_s(g_wait_why, sizeof g_wait_why, why);
        return 1;
    }
    g_wait_why[0] = 0;
    uint64_t gt = a->gametype[0] ? g_csym64(a->gametype) : SYM_NONE;
    if (*(int32_t *)ng == NG_LOGGED_IN && !a->begin_at) {
        /* As PLAY does: the STATUS page first (the player sees it connect), multiplayer a moment later. */
        rlog("join %s (%s): main menu: STATUS page %s", a->link, a->why,
             show_page(game, P51_STATUS) ? "not found (joining without it)" : "shown");
        a->begin_at = GetTickCount64() + 1000;
        return 1;
    }
    if (*(int32_t *)ng == NG_LOGGED_IN && GetTickCount64() < a->begin_at) return 1;
    if (*(int32_t *)ng == NG_LOGGED_IN) {
        uint64_t *gtw = (uint64_t *)(game + g_prof->o_game_gametype);
        g_saved_gametype = *gtw;
        g_restore_gametype = 1;
        memcpy(game + g_prof->o_game_lobbyid, guid, 16);
        *gtw = gt;
        rlog("join %s (%s): main menu: -lobbyid staged (team word %04x, gametype %s), beginning multiplayer", a->link,
             a->why, *(uint16_t *)(game + g_prof->o_game_team), a->gametype[0] ? a->gametype : "-1");
        ((GameFn)(g_base + g_prof->rva_mp_transition))(game);   /* the menu's "connecting" transition, as /join_session */
        ((BeginMpFn)(g_base + g_prof->rva_begin_mp))(game);
        rlog("join %s: multiplayer begun, netgame state %d", a->link, *(int32_t *)ng);
        return 0;
    }
    int st = *(int32_t *)ng, ls = *(int32_t *)(lobby + g_prof->o_lobby_state);
    rlog("join %s (%s): netgame state %d, lobby state %d, gametype %s", a->link, a->why, st, ls,
         a->gametype[0] ? a->gametype : "-1");
    ((GameFn)(g_base + g_prof->rva_mp_transition))(game);
    if (st == NG_IN_GAME) ((LeaveFn)(g_base + g_prof->rva_leave))(ng, 0);
    else if (ls >= 4 && ls <= 6) ((CancelFn)(g_base + g_prof->rva_cancel))(lobby, 0x1b);   /* as DeepLinkCB does */
    ((JoinFn)(g_base + g_prof->rva_join))(lobby, gt, guid, 0xffff);
    rlog("join %s: requested", a->link);
    return 0;
}

/* ---- event hook ---- */

static uint64_t actor_name(uint8_t *gs, uint64_t sactor)
{
    uint8_t *tbl = NULL;
    uint64_t idx = sactor & 0xffff, *names = NULL, count = 0, name = 0;
    if (!g_read(&tbl, gs + g_prof->o_gs_actors, sizeof tbl) || !tbl) return 0;
    if (!g_read(&names, tbl + g_prof->o_at_names, sizeof names) || !names) return 0;
    if (!g_read(&count, tbl + g_prof->o_at_count, sizeof count) || idx >= count) return 0;
    return g_read(&name, names + idx, sizeof name) ? name : 0;
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
    rlog("event %016llx owner=%016llx component=%016llx x=%016llx", sym, owner, comp, ev[4]);

    if (sym == SYM_CARD) {
        int c = card_index(comp);
        AcquireSRWLockExclusive(&g_wlock);
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
        ReleaseSRWLockExclusive(&g_wlock);
        return;
    }
    if (sym == SYM_BTN && comp == UID_CREATE && owner == P40_SETTINGS) {
        rlog("CREATE pressed (the request follows the page's create_invite metric)");
        return;
    }
    if (sym == SYM_BTN && comp == UID_CREATE && owner == P41_SHARE) {
        Action a = { ACT_JOIN };
        AcquireSRWLockShared(&g_wlock);
        strcpy_s(a.link, sizeof a.link, g_wiz.link);
        strcpy_s(a.why, sizeof a.why, g_wiz.code);
        strcpy_s(a.gametype, sizeof a.gametype, g_wiz.gametype);
        ReleaseSRWLockShared(&g_wlock);
        if (!a.link[0]) { rlog("JOIN pressed: no invite created yet"); return; }
        rlog("JOIN pressed: our invite %s", a.why);
        push_action(&a);
        return;
    }
    if (sym == SYM_TEXT && owner == P34_JOIN_CODE) rlog("ENTER pressed (the code follows in the join metric)");
    if ((sym == SYM_CANCEL || sym == SYM_BACK) && owner == P34_JOIN_CODE) {   /* leaving p34 ends a password prompt */
        AcquireSRWLockExclusive(&g_wlock);
        int had = g_pw.code[0] != 0;
        g_pw.code[0] = 0;
        ReleaseSRWLockExclusive(&g_wlock);
        if (had) rlog("p34 left: password prompt dropped");
        if (had || g_p34_dirty) p34_say(P34_TITLE);
    }
}

/* ---- the click-metric JSON (remote-log submit): settings, typed code, player id ---- */

static int json_str(const char *js, const char *key, char *out, size_t cap);

/* "3 HOURS" / "30 MINUTES" / "1 DAY" -> seconds; 0 = unknown (the host's default then applies). */
static int expires_seconds(const char *s)
{
    int n = atoi(s);
    if (n <= 0) return 0;
    return strstr(s, "MIN") ? n * 60 : strstr(s, "HOUR") ? n * 3600 : strstr(s, "DAY") ? n * 86400 : 0;
}

static void on_metric(const char *js)
{
    char panel[64], event[64], user[64];
    if (!strstr(js, "\"panel_type\":\"deep_linking\"")) return;
    if (!json_str(js, "panel_name", panel, sizeof panel) || !json_str(js, "event_name", event, sizeof event)) return;
    if (json_str(js, "user_id", user, sizeof user)) {
        AcquireSRWLockExclusive(&g_wlock);
        strcpy_s(g_wiz.user, sizeof g_wiz.user, user);
        ReleaseSRWLockExclusive(&g_wlock);
    }
    rlog("metric %s/%s user=%s", panel, event, user);
    if (!strcmp(panel, "create_game_review_settings") && !strcmp(event, "create_invite")) {
        Job j = { JOB_CREATE };
        char v[64];
        AcquireSRWLockShared(&g_wlock);
        j.wiz = g_wiz;
        ReleaseSRWLockShared(&g_wlock);
        json_str(js, "Password", j.password, sizeof j.password);
        json_str(js, "Server_region", j.region, sizeof j.region);
        j.expires_s = json_str(js, "Expires_after", v, sizeof v) ? expires_seconds(v) : 0;
        rlog("create: experience=%s mode=%s map=%d password=%s region=%s expires=%s (%d s)",
             exp_name(j.wiz.experience), mode_name(j.wiz.mode), j.wiz.map, j.password[0] ? "set" : "none", j.region,
             v, j.expires_s);
        push_job(&j);
    } else if (!strcmp(panel, "join_via_code") && !strcmp(event, "join")) {
        Job j = { JOB_RESOLVE };
        char typed[64] = "";
        if (!json_str(js, "join_code", typed, sizeof typed) || !strcmp(typed, "<invalid>")) {
            rlog("join via code: no code (%s)", typed);
            return;
        }
        /* After a 403 "needs a password" the next ENTER is the password for that code. The field still holds the
         * code unless the player cleared it, so a typed "<code><password>" counts as the password alone. */
        AcquireSRWLockExclusive(&g_wlock);
        if (g_pw.code[0] && GetTickCount64() >= g_pw.until) g_pw.code[0] = 0;
        strcpy_s(j.code, sizeof j.code, g_pw.code);
        ReleaseSRWLockExclusive(&g_wlock);
        if (j.code[0] && !_stricmp(typed, j.code)) {   /* ENTER again on the code itself: still waiting */
            p34_say("ENTER PASSWORD");
            return;
        }
        if (j.code[0]) {
            size_t n = strlen(j.code);
            const char *pw = !_strnicmp(typed, j.code, n) && typed[n] ? typed + n : typed;
            strcpy_s(j.password, sizeof j.password, pw);
            rlog("join via code: password for code=%s (%u chars)", j.code, (unsigned)strlen(j.password));
        } else if (strlen(typed) >= sizeof j.code) {
            rlog("join via code: %u characters, too long for a code", (unsigned)strlen(typed));
            p34_say("INVALID CODE");
            return;
        } else {
            strcpy_s(j.code, sizeof j.code, typed);
            rlog("join via code: code=%s", j.code);
        }
        push_job(&j);
    }
}

/* Runs on whatever thread logs (the script's, so the game thread for these); copies and returns. */
static void metric_body(const char *json, size_t len)
{
    __try {
        if (json && len > 0 && len < 4096) {
            char buf[4096];
            memcpy(buf, json, len);
            buf[len] = 0;
            on_metric(buf);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        rlog("fault in the remote-log hook: ignored");
    }
}

static void __fastcall remote_log_hook(const char *json, size_t len) { metric_body(json, len); g_remote_log_orig(json, len); }

static void target_body(void *gs, const uint64_t *ev, uint32_t flags)
{
    (void)flags;
    __try {
        on_wizard_event((uint8_t *)gs, ev);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        rlog("fault in the event hook (event %016llx): ignored", ev[0]);
    }
}

static void __fastcall target_hook(void *gs, const uint64_t *ev, uint32_t flags) { target_body(gs, ev, flags); g_target_orig(gs, ev, flags); }

/* Game actions run here, on the main thread, after the queue is taken with a try-lock (never blocks a frame). */
static void tick_body(void *game)
{
    if (g_restore_gametype) {   /* the staged -gametype is read at state 5; put the player's own value back after */
        __try {
            uint8_t *ng = *(uint8_t **)((uint8_t *)game + g_prof->o_game_netgame);
            int st = ng ? *(int32_t *)ng : -1;
            if (st >= 6 || (st == NG_LOGGED_IN && g_restore_gametype++ > 600)) {
                *(uint64_t *)((uint8_t *)game + g_prof->o_game_gametype) = g_saved_gametype;
                rlog("gametype word restored (netgame state %d)", st);
                g_restore_gametype = 0;
            } else if (st != NG_LOGGED_IN) {
                g_restore_gametype = 2;   /* left state 3: count from here only if it comes back */
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            g_restore_gametype = 0;
        }
    }
    if (g_nacts && TryAcquireSRWLockExclusive(&g_alock)) {
        Action batch[ACAP];
        int n = g_nacts;
        memcpy(batch, g_acts, n * sizeof *batch);
        g_nacts = 0;
        ReleaseSRWLockExclusive(&g_alock);
        for (int i = 0; i < n; i++) {
            int again = 0;
            __try {
                if (batch[i].kind == ACT_JOIN) again = join_link(&batch[i]);
                else if (batch[i].kind == ACT_SHOW_CODE) again = show_code((uint8_t *)game, &batch[i]);
                else if (batch[i].kind == ACT_P34_TEXT) again = p34_text((uint8_t *)game, &batch[i]);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                rlog("fault in action %d: dropped", batch[i].kind);
            }
            if (again && ++batch[i].tries < 900) push_action(&batch[i]);   /* about 10 s of frames */
            else if (again && batch[i].kind == ACT_SHOW_CODE) rlog("code %s: p41's canvas never took it", g_p2.code);
            else if (again && batch[i].kind == ACT_P34_TEXT) rlog("p34 title \"%s\": the field's canvas never took it", batch[i].why);
            else if (again) rlog("join %s: the game never became ready, dropped", batch[i].link);
        }
    }
}

static void __fastcall tick_hook(void *game) { tick_body(game); g_tick_orig(game); }

/* Broker callbacks: the same bodies; the broker calls the original after the last callback. */
static void bk_tick(void *game, void *u) { (void)u; tick_body(game); }
static void bk_target(void *gs, const uint64_t *ev, uint32_t flags, void *u) { (void)u; target_body(gs, ev, flags); }
static void bk_metric(const char *json, size_t len, void *u) { (void)u; metric_body(json, len); }

/* ---- HTTP (worker thread only) ---- */

static wchar_t g_svc_host[256];
static INTERNET_PORT g_svc_port;
static HINTERNET g_http;

/* "apiservice_host": "http://HOST:PORT/api" in the install's _local\\config.json (<root>\\_local, two levels above
 * bin\\win10\\echovr.exe; the exe's own _local is tried first). The services port answers /invites with or
 * without /api, so only host and port are kept. */
static int read_services_host(void)
{
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
    if (f == INVALID_HANDLE_VALUE) { rlog("no _local\\config.json beside the exe or two levels up"); return 0; }
    char buf[8192];
    DWORD got = 0;
    ReadFile(f, buf, sizeof buf - 1, &got, NULL);
    CloseHandle(f);
    buf[got] = 0;
    char *k = strstr(buf, "\"apiservice_host\""), *q1 = k ? strchr(k + 17, '"') : NULL, *q2 = q1 ? strchr(q1 + 1, '"') : NULL;
    if (!q2) { rlog("%s has no apiservice_host", path); return 0; }
    wchar_t url[512];
    int len = MultiByteToWideChar(CP_UTF8, 0, q1 + 1, (int)(q2 - q1 - 1), url, 511);
    url[len] = 0;
    URL_COMPONENTS uc = { sizeof uc };
    uc.lpszHostName = g_svc_host;
    uc.dwHostNameLength = sizeof g_svc_host / sizeof g_svc_host[0];
    if (!WinHttpCrackUrl(url, 0, 0, &uc)) { rlog("cannot parse apiservice_host"); return 0; }
    g_svc_port = uc.nPort;
    rlog("services: %ls:%u (from apiservice_host)", g_svc_host, g_svc_port);
    return 1;
}

/* One request; returns the HTTP status (0 = transport error) and the body in out. */
static int http(const wchar_t *method, const char *path, const char *body, char *out, size_t cap)
{
    out[0] = 0;
    wchar_t wpath[256];
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, 256);
    HINTERNET c = WinHttpConnect(g_http, g_svc_host, g_svc_port, 0), r = NULL;
    int status = 0;
    if (c) r = WinHttpOpenRequest(c, method, wpath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    DWORD blen = body ? (DWORD)strlen(body) : 0;
    if (r && WinHttpSendRequest(r, body ? L"Content-Type: application/json\r\n" : WINHTTP_NO_ADDITIONAL_HEADERS,
                                (DWORD)-1L, (void *)body, blen, blen, 0)
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

/* The string value of "key" in a flat JSON object (our host's answers), or 0. */
static int json_str(const char *js, const char *key, char *out, size_t cap)
{
    char pat[64];
    _snprintf_s(pat, sizeof pat, _TRUNCATE, "\"%s\"", key);
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

/* The logged-in player's id, as the stock metric lines carry it (the host decides how to verify it). */
static int player_id(const Job *j, char *out, size_t cap)
{
    strcpy_s(out, cap, j->wiz.user);
    return out[0] != 0;
}

static void do_create(const Job *j)
{
    const Route *rt = route_of(&j->wiz);
    if (!rt) {
        rlog("create: no session route for experience=%s mode=%s map=%d", exp_name(j->wiz.experience),
             mode_name(j->wiz.mode), j->wiz.map);
        return;
    }
    char user[96], body[512], resp[2048], code[16], link[48], err[256], exp_at[48] = "";
    if (!player_id(j, user, sizeof user)) { rlog("create: no player id in the metric"); return; }
    int n = _snprintf_s(body, sizeof body, _TRUNCATE, "{\"user_id\":\"%s\",\"gametype\":\"%s\"", user, rt->gametype);
    if (rt->level) n += _snprintf_s(body + n, sizeof body - n, _TRUNCATE, ",\"level\":\"%s\"", rt->level);
    if (j->expires_s) n += _snprintf_s(body + n, sizeof body - n, _TRUNCATE, ",\"expires_s\":%d", j->expires_s);
    /* Password: the metric holds the typed text when PASSWORD is ON ("4242"), "" when off. Region: sent as the
     * UI label ("US WEST"); the host maps label -> code -> CSymbol64 and falls back to a "default" server
     * (the services host's region table). */
    if (j->password[0] && _stricmp(j->password, "OFF") && !strchr(j->password, '"') && !strchr(j->password, '\\'))
        n += _snprintf_s(body + n, sizeof body - n, _TRUNCATE, ",\"password\":\"%s\"", j->password);
    if (j->region[0] && !strchr(j->region, '"') && !strchr(j->region, '\\'))
        n += _snprintf_s(body + n, sizeof body - n, _TRUNCATE, ",\"region\":\"%s\"", j->region);
    _snprintf_s(body + n, sizeof body - n, _TRUNCATE, "}");
    int st = http(L"POST", "/invites", body, resp, sizeof resp);
    if (st != 200 || !json_str(resp, "code", code, sizeof code) || !json_str(resp, "link", link, sizeof link)) {
        if (!json_str(resp, "error", err, sizeof err)) strcpy_s(err, sizeof err, resp);
        rlog("create %s/%s: HTTP %d %s", rt->gametype, rt->level ? rt->level : "-", st, err);
        return;
    }
    AcquireSRWLockExclusive(&g_wlock);
    strcpy_s(g_wiz.code, sizeof g_wiz.code, code);
    strcpy_s(g_wiz.link, sizeof g_wiz.link, link);
    strcpy_s(g_wiz.gametype, sizeof g_wiz.gametype, rt->gametype);
    ReleaseSRWLockExclusive(&g_wlock);
    char srv_region[32] = "";
    json_str(resp, "region", srv_region, sizeof srv_region);
    rlog("created code=%s link=%s gametype=%s level=%s region=%s", code, link, rt->gametype, rt->level ? rt->level : "-",
         srv_region[0] ? srv_region : "-");
    json_str(resp, "expires_at", exp_at, sizeof exp_at);   /* "2026-10-21T05:38:59.63Z" -> "EXPIRES: 05:38 UTC" */
    AcquireSRWLockExclusive(&g_wlock);
    strcpy_s(g_p2.code, sizeof g_p2.code, code);
    if (strlen(exp_at) >= 16 && exp_at[10] == 'T')
        _snprintf_s(g_p2.expires, sizeof g_p2.expires, _TRUNCATE, "EXPIRES: %.5s UTC", exp_at + 11);
    else
        strcpy_s(g_p2.expires, sizeof g_p2.expires, "EXPIRES: -");
    _snprintf_s(g_p2.link, sizeof g_p2.link, _TRUNCATE, "-lobbyid %s", link);   /* the boot argument that joins it */
    ReleaseSRWLockExclusive(&g_wlock);
    Action a = { ACT_SHOW_CODE };
    strcpy_s(a.link, sizeof a.link, code);
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
    int n = _snprintf_s(path, sizeof path, _TRUNCATE, "/invites/%s", j->code);
    if (j->password[0] && n > 0) {   /* GET /invites/{code}?password=… (Invites.cs), percent-encoded */
        n += _snprintf_s(path + n, sizeof path - n, _TRUNCATE, "?password=");
        for (const unsigned char *p = (const unsigned char *)j->password; *p && n > 0 && n < (int)sizeof path - 4; p++)
            n += isalnum(*p) || *p == '-' || *p == '_' || *p == '.' || *p == '~'
                     ? _snprintf_s(path + n, sizeof path - n, _TRUNCATE, "%c", *p)
                     : _snprintf_s(path + n, sizeof path - n, _TRUNCATE, "%%%02X", *p);
    }
    int st = http(L"GET", path, NULL, resp, sizeof resp);
    if (st != 200 || !json_str(resp, "link", link, sizeof link)) {
        if (!json_str(resp, "error", err, sizeof err)) strcpy_s(err, sizeof err, resp);
        rlog("resolve %s%s: HTTP %d %s", j->code, j->password[0] ? " (with password)" : "", st, err);
        const char *msg = resolve_message(st == 200 ? 500 : st, err);
        AcquireSRWLockExclusive(&g_wlock);
        if (st == 403) {   /* needs / wrong password: (re)open the prompt for this code */
            strcpy_s(g_pw.code, sizeof g_pw.code, j->code);
            g_pw.until = GetTickCount64() + PASSWORD_WINDOW_MS;
        } else {
            g_pw.code[0] = 0;
        }
        ReleaseSRWLockExclusive(&g_wlock);
        p34_say(msg);
        return;
    }
    AcquireSRWLockExclusive(&g_wlock);
    g_pw.code[0] = 0;
    ReleaseSRWLockExclusive(&g_wlock);
    if (g_p34_dirty) p34_say(P34_TITLE);   /* leave the field as the page set it, for the next visit */
    Action a = { ACT_JOIN };
    json_str(resp, "gametype", a.gametype, sizeof a.gametype);
    rlog("resolved code=%s link=%s gametype=%s", j->code, link, a.gametype);
    strcpy_s(a.link, sizeof a.link, link);
    strcpy_s(a.why, sizeof a.why, j->code);
    push_action(&a);
}

static DWORD WINAPI worker(LPVOID unused)
{
    (void)unused;
    g_http = WinHttpOpen(L"rad-links/0.1", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME,
                         WINHTTP_NO_PROXY_BYPASS, 0);
    if (!g_http) { rlog("WinHttpOpen failed (%lu)", GetLastError()); return 0; }
    WinHttpSetTimeouts(g_http, 5000, 5000, 5000, 5000);
    if (!read_services_host()) rlog("no services host: create / join disabled");
    for (;;) {
        WaitForSingleObject(g_job_event, INFINITE);
        for (;;) {
            Job j = { 0 };
            AcquireSRWLockExclusive(&g_jlock);
            int have = g_njobs > 0;
            if (have) { j = g_jobs[0]; memmove(g_jobs, g_jobs + 1, --g_njobs * sizeof *g_jobs); }
            ReleaseSRWLockExclusive(&g_jlock);
            if (!have) break;
            if (!g_svc_port) { rlog("job %d: no services host", j.kind); continue; }
            if (j.kind == JOB_CREATE) do_create(&j);
            else if (j.kind == JOB_RESOLVE) do_resolve(&j);
        }
    }
}

/* ---- install ---- */

static const Profile *profile_for(uint32_t ts, uint32_t soi)
{
    for (size_t i = 0; i < sizeof g_profiles / sizeof g_profiles[0]; i++)
        if (ts == g_profiles[i].timestamp && soi == g_profiles[i].size_of_image) return &g_profiles[i];
    return NULL;
}

/* Standalone identity: our own image's PE header. Under the broker, api->build() (evrmod_plugin_init). */
static const Profile *match_profile(uint8_t *base)
{
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(base + dos->e_lfanew);
    const Profile *p = profile_for(nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage);
    if (p) return p;
    rlog("unsupported image: TimeDateStamp %08lx SizeOfImage %08lx",
         nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage);
    return NULL;
}

/* 14-byte absolute jump: jmp qword ptr [rip+0]; dq target */
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

/* Stock prologue: steal 15 bytes into a trampoline (three register-home stores, position-free). Already hooked
 * the way other hook tools do (jmp [rip+0] to a loaded module): take over its target and call it in turn. */
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

static void open_log(void)
{
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, path, sizeof path);
    char *slash = n ? strrchr(path, '\\') : NULL;
    if (!slash) return;
    strcpy_s(slash + 1, sizeof path - (slash + 1 - path), "_local");
    CreateDirectoryA(path, NULL);
    strcat_s(path, sizeof path, "\\radlinks.log");
    g_log = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

static int start_worker(void)
{
    g_job_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    HANDLE th = CreateThread(NULL, 0, worker, NULL, 0, NULL);
    if (!th) { rlog("worker thread failed (%lu)", GetLastError()); return 0; }
    CloseHandle(th);
    return 1;
}

/* The game functions rad-links calls must hold the profile's bytes (read through g_read). */
static int same(uint32_t rva, const uint8_t *want, size_t n)
{
    uint8_t got[16];
    return n <= sizeof got && g_read(got, g_base + rva, n) && !memcmp(got, want, n);
}
static int fn_bytes_ok(void)
{
    if (!same(g_prof->rva_join, g_prof->join_bytes, 8) || !same(g_prof->rva_leave, g_prof->leave_bytes, 8)
        || !same(g_prof->rva_cancel, g_prof->cancel_bytes, 8)
        || !same(g_prof->rva_begin_mp, g_prof->begin_mp_bytes, 8)
        || !same(g_prof->rva_mp_transition, g_prof->mp_transition_bytes, 8)
        || !same(g_prof->rva_find_script, g_prof->find_script_bytes, 8)
        || !same(g_prof->rva_set_string, g_prof->set_string_bytes, 8)
        || !same(g_prof->rva_find_canvas, g_prof->find_canvas_bytes, 8)
        || !same(g_prof->rva_canvas_set_text, g_prof->canvas_set_text_bytes, 8)) {
        rlog("Join / LeaveGame / Cancel bytes differ from the profile: refusing");
        return 0;
    }
    return 1;
}

static void init_syms(void)
{
    SYM_CARD = g_csym64("evt_card_clicked");
    SYM_BTN = g_csym64("evt_btn_clicked");
    SYM_TEXT = g_csym64("evt_text_submitted");
    SYM_LINK_BTN = g_csym64("evt_link_button_clicked");
    SYM_BACK = g_csym64("evt_back_btn_clicked");
    SYM_CANCEL = g_csym64("evt_submission_canceled");
}

static BOOL attach(void)
{
    open_log();
    rlog("rad-links attach, pid %lu", GetCurrentProcessId());
    g_base = (uint8_t *)GetModuleHandleA(NULL);
    if (g_brokered) {   /* the broker owns the sites and the identity: evrmod_plugin_init checks and registers */
        rlog("evrmod broker ABI %u loading: identity check and hooks deferred to evrmod_plugin_init", evrmod_broker_loading());
        return TRUE;
    }
    if (!(g_prof = match_profile(g_base))) return FALSE;
    rlog("profile: %s, image base %p", g_prof->name, g_base);
    init_syms();
    if (!hook_site(g_prof->rva_send_target, g_prof->target_prologue, 15, (void *)target_hook, (void **)&g_target_orig)) {
        rlog("SendComponentEvent at %p is neither stock nor a known hook: refusing", g_base + g_prof->rva_send_target);
        return FALSE;
    }
    if (!hook_site(g_prof->rva_remote_log, g_prof->remote_log_prologue, 16, (void *)remote_log_hook,
                   (void **)&g_remote_log_orig)) {
        rlog("remote-log submit at %p is neither stock nor a known hook: refusing", g_base + g_prof->rva_remote_log);
        return FALSE;
    }
    if (!fn_bytes_ok()) return FALSE;
    if (!install_tick()) return FALSE;
    if (!start_worker()) return TRUE;
    rlog("ready: wizard events hooked at %p, tick slot %p", g_base + g_prof->rva_send_target,
         g_base + g_prof->rva_tick_slot);
    return TRUE;
}

/* evrmod broker entry, called right after DllMain. Standalone (no EVRMOD_BROKER, e.g. a plain injector or the
 * ABI-1 loader): DllMain already hooked; do nothing. The worker starts
 * first so a wizard event that arrives right after registration finds its job event. */
__declspec(dllexport) void evrmod_plugin_init(const EvrmodApi *api)
{
    int rc;
    if (!g_brokered) return;
    if (!api || api->abi_version < 2) { rlog("broker ABI < 2 under EVRMOD_BROKER: rad-links off"); return; }
    const EvrmodBuild *b = api->build();
    if (!b || !b->is_known || !(g_prof = profile_for(b->time_date_stamp, b->size_of_image))) {
        rlog("unsupported image per the broker (%s, TimeDateStamp %08lx SizeOfImage %08lx): rad-links off",
             b ? b->name : "NULL", b ? b->time_date_stamp : 0, b ? b->size_of_image : 0);
        return;
    }
    g_base = (uint8_t *)b->image_base;
    g_csym64 = api->csym64;   /* the broker's services from here on */
    g_read = api->read;
    rlog("profile: %s, image base %p (identity from the evrmod broker)", g_prof->name, g_base);
    init_syms();
    if (!fn_bytes_ok()) return;
    if (!start_worker()) return;
    if ((rc = api->register_tick(bk_tick, NULL)) || (rc = api->register_metric_log(bk_metric, NULL))
        || (rc = api->register_send_event(bk_target, NULL))) {
        rlog("the broker refused a hook (%d): rad-links off", rc);
        return;
    }
    rlog("ready: wizard events, remote log and tick registered with the evrmod broker (ABI %u)", api->abi_version);
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
