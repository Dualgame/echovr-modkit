/* Offline test of report_flow.h ( pause popups, five-tile card, D-S2: card -> p23 preset, fallback F, parity) against stub engine calls.
 * Build + run (Linux / WSL):  cc -std=c11 -Wall -Wextra -o /tmp/trf test_report_flow.c && /tmp/trf
 * Quest path (the tablet SUBMIT fills, the body's platform):  add -DRP_PLATFORM='"quest"' to the same line.
 * Optional: RP_RECIPE_DIR=<the popup tree recipe directory holding reason_*.json> adds the pause tile label check.
 * No game, no device: the stubs record page enables, canvas writes and tablet list rows; the host is a fake rp_http. */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include "csymbol64.h"

static int g_fail, g_checks;
#define CHECK(c, ...) do { g_checks++; if (!(c)) { g_fail++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static int g_verbose;
static int g_log_xlog, g_log_back;   /*: the stock X "<" log lines; "<" press lines (: "back one step") */
static int g_log_bk_ok, g_log_bk_fail, g_log_bk_wrong, g_log_cancel_list;   /*: back -> card / not re-opened / wrong card / CANCEL list */
static char g_log_bk_last[512];
static int g_log_dfail;
static int g_log_xp_unread, g_log_xp_retry_ok, g_log_xp_retry_no;   /* the card id read at the REPORT press / its retry */
static int g_log_xgrace, g_log_mbgrace, g_log_grace_rel, g_log_grace_time;   /* the stock X eaten in the grace; grace ended by the release / by time */
static int g_log_consumed;   /* w23e: "press consumed" log lines (the stock menubutton under the layer) */
static int g_log_so_off, g_log_so_rel, g_log_so_held, g_log_so_fb, g_log_so_foreign, g_log_so_stale, g_log_so_missing;   /* stock-off */
static char g_log_so_last[512];
static int g_log_lo_off, g_log_co, g_log_card_closed, g_log_res_ok;   /* layer-off / card-off lines; closed-card press; result OK */
static char g_log_co_last[512], g_log_df_last[512];
static int g_log_tnotshown, g_log_pnotshown, g_log_xfwd, g_log_bkuser, g_log_warnoff;   /* late results logged; "<" -> X; walk stopped; warn-off */
static void rp_log(const char *fmt, ...)
{
    char b[512];
    va_list a; va_start(a, fmt); vsnprintf(b, sizeof b, fmt, a); va_end(a);
    if (strstr(b, "under the layer: press consumed")) g_log_consumed++;
    if (strstr(b, "stock-off: ")) snprintf(g_log_so_last, sizeof g_log_so_last, "%s", b);
    int so = strstr(b, "stock-off: ") != NULL;   /* the REPORT layer's set (layer-off / card-off are counted apart) */
    if (strstr(b, "layer-off: ") && strstr(b, "zones off (engine)")) g_log_lo_off++;
    if (strstr(b, "card-off: ")) { snprintf(g_log_co_last, sizeof g_log_co_last, "%s", b); g_log_co++; }
    if (strstr(b, "card REPORT press ignored (the card is closed")) g_log_card_closed++;
    if (strstr(b, "tablet result OK: LOBBY list")) g_log_res_ok++;
    if (so && strstr(b, "zones off (engine)")) g_log_so_off++;
    if (so && strstr(b, "zones released (")) { g_log_so_rel++; if (strstr(b, " held ")) g_log_so_held++; }
    if (strstr(b, "stock-off: fallback swallow (")) g_log_so_fb++;
    if (strstr(b, "already set by another user")) g_log_so_foreign++;
    if (strstr(b, "record dropped") || strstr(b, "handle changed")) g_log_so_stale++;
    if (so && strstr(b, "swallow fallback (zone not found)")) g_log_so_missing++;
    if (strstr(b, "stock X under the layer: press ->")) g_log_xlog++;
    if (strstr(b, "stock X under the layer (grace): press consumed")) g_log_xgrace++;
    if (strstr(b, "under the layer: press consumed (grace)")) g_log_mbgrace++;
    if (strstr(b, "grace ended by the press release")) g_log_grace_rel++;
    if (strstr(b, "grace ended by time")) g_log_grace_time++;
    if (strstr(b, "card target id read (REPORT press): unreadable, retry")) g_log_xp_unread++;
    if (strstr(b, "card target id read (") && strstr(b, "): ok on retry")) g_log_xp_retry_ok++;
    if (strstr(b, "card target id read (") && (strstr(b, "still unreadable") || strstr(b, "another player's id"))) g_log_xp_retry_no++;
    if (strstr(b, "tablet \"<\": back one step")) g_log_back++;
    if (strstr(b, ": back -> card of ")) { g_log_bk_ok++; snprintf(g_log_bk_last, sizeof g_log_bk_last, "%s", b); }
    if (strstr(b, ": back: direct open (slot")) { g_log_dfail++; snprintf(g_log_df_last, sizeof g_log_df_last, "%s", b); }
    if (strstr(b, ": back: card not re-opened (")) { g_log_bk_fail++; snprintf(g_log_bk_last, sizeof g_log_bk_last, "%s", b); }
    if (strstr(b, ": back: idx ")) g_log_bk_wrong++;
    if (strstr(b, "cancelled (user_list, card closed")) g_log_cancel_list++;
    if (strstr(b, "tablet result '") && strstr(b, "not shown")) g_log_tnotshown++;
    if (strstr(b, "popup result '") && strstr(b, "not shown")) g_log_pnotshown++;
    if (strstr(b, "with no layer (fallback mode): stock X sent")) g_log_xfwd++;
    if (strstr(b, "the user pressed the LOBBY list")) g_log_bkuser++;
    if (strstr(b, "warn-off: ")) g_log_warnoff++;
    if (g_verbose) printf("  log: %s\n", b);
}
static int xchg(int *p, int v) { int o = *p; *p = v; return o; }
#define RP_XCHG(p, v) xchg((int *)(p), (v))
static uint64_t rp_sym(const char *s) { return csym64(s); }
#define RP_CARD_EL_DEFAULT "party_label"

/* ---- fake world ---- */
#define NW 256
static struct { uint64_t actor, el; char text[160]; int shown; float rgb[3]; int has_text, has_show, has_rgb; } W[NW];
static int g_nw;
static int find_w(uint64_t a, uint64_t e)
{
    for (int i = 0; i < g_nw; i++) if (W[i].actor == a && W[i].el == e) return i;
    W[g_nw].actor = a; W[g_nw].el = e; W[g_nw].shown = -1;
    return g_nw++;
}
static uint64_t g_last_page;
static int g_pages_shown, g_popup_marker = 1, g_card_five = 1, g_settext_card_el;
static int g_pause_on, g_toggles, g_p1_en, g_p22_en, g_p23_en;   /* evt_dbg_toggle_menu_vis flips g_pause_on; p23 loads only while it is on */
static int g_more_rows, g_big_lists;                             /* recorder: list rows holding "MORE", setups of > 4 rows */
static char g_xpid[32] = "DMO-1003";                             /* the card's xplatformid */
static char g_list_rows[8][64];
static int g_list_n, g_list_calls;
static char g_post_body[1024];
static int g_posts, g_gets;
static int g_post_status;   /* 0 = 201; else the fake POST /reports status (409 already reported, -1 host down) */
static char g_prompt_back[16];   /* the tablet prompt's footer label (param_prompt_back) */
static char g_prompt_msg[64];
static uint64_t S_NUM, S_NAMES[5], S_SETUP_LIST_T;
static uint64_t g_last_transition;
static uint64_t g_now = 10000;   /* the test clock (tick) */
static uint64_t g_fires[64];
static int g_nfires;
/* P1: the platform's button4 enable (PC); rc 1 enabled, 0 not run */
#define RP_HAVE_CARD_FRIEND 1
static int g_b4_calls, g_b4_rc = 1;
static int rp_card_button4(void *game) { (void)game; g_b4_calls++; return g_b4_rc; }

/* clearyoffset on the tablet's ncaScript (rp_call_on) and the press guard's REAL reads (rp_getreal) */
static int g_clear_calls, g_call_on_other, g_list_noclear, g_cleared;   /* g_cleared: a clearyoffset since the last list setup */
static int g_real_ok = 1;
static float g_real_y, g_real_h = 150.0f, g_real_w = 1000.0f;            /* row pitch 0.15 */
/*: the card's OpenLobbyMemberSocialControls p_userid=<slot>. g_od_mode 0 = refused (-1), 1 = runs: the card's xplatformid
 * becomes g_od_map[slot] ("" = no card: unchanged) g_od_delay ms later; g_od_busy: busy answers first. rp_setvar clears g_xpid. */
static int g_od_mode, g_od_busy, g_od_calls, g_od_slot[16], g_od_delay, g_od_pend = -1, g_sv_ok = 1, g_sv_calls;
static char g_od_map[8][32];
static uint64_t g_od_at;
static void od_reset(void) { g_od_mode = g_od_busy = g_od_calls = g_od_delay = 0; g_od_pend = -1; memset(g_od_map, 0, sizeof g_od_map); g_sv_ok = 1; g_sv_calls = 0; }
static int rp_setvar(void *game, uint64_t actor, uint64_t comp, uint64_t var, const char *str)
{ (void)game; (void)actor; (void)comp; (void)var; if (!g_sv_ok || !g_od_mode) return 0; g_sv_calls++; snprintf(g_xpid, sizeof g_xpid, "%s", str); return 1; }
static int rp_call_on(void *game, uint64_t actor, uint64_t comp, uint64_t fn, int n, const uint64_t *pname, const void *const *pval)
{
    (void)game; (void)pname; (void)pval;
    if (fn == csym64("OpenLobbyMemberSocialControls")) {
        if (actor != 0x61c4b2b9153a12b7ull || comp != csym64("ncaScript") || n != 1 || pname[0] != csym64("p_userid")) { g_call_on_other++; return -1; }
        if (!g_od_mode) return -1;
        if (g_od_busy) { if (g_od_busy > 0) g_od_busy--; return 1; }
        int sl = *(const int32_t *)pval[0];
        if (g_od_calls < 16) g_od_slot[g_od_calls] = sl;
        g_od_calls++;
        g_od_pend = sl >= 0 && sl < 8 && g_od_map[sl][0] ? sl : -1;
        g_od_at = g_now + (uint64_t)g_od_delay;
        return 0;
    }
    if (fn != csym64("clearyoffset") || comp != csym64("ncaScript") || actor != 0x6c1f6ff04e070923ull || n) { g_call_on_other++; return -1; }
    g_clear_calls++; g_cleared = 1; g_real_y = 0;
    return 0;
}
static int rp_getreal(void *game, uint64_t actor, uint64_t comp, uint64_t var, float *out)
{
    (void)game;
    if (!g_real_ok || actor != 0x6c1f6ff04e070923ull || comp != csym64("ncaScript")) return 0;
    if (var == csym64("list_y_offset")) *out = g_real_y;
    else if (var == csym64("list_entry_height")) *out = g_real_h;
    else if (var == csym64("world_to_canvas")) *out = g_real_w;
    else return 0;
    return 1;
}
static int g_list_busy;   /* > 0: the next setupcustomuserlist calls answer busy (1), counting down */
/*: state_mgr.usernamepressed pressed_user_index=<visible row>. The card's xplatformid becomes g_un_map[row] ("" in the
 * map = the row opens no card: xplatformid unchanged) g_un_delay ms after the call (the engine runs the call deferred).
 * g_un_busy > 0: that many busy answers first (-1: always busy); g_un_refuse: refused (-1). */
static char g_un_map[8][32];
static int g_un_calls, g_un_idx[32], g_un_busy, g_un_refuse, g_un_delay, g_un_pend = -1, g_un_early, g_tr_busy;   /* g_un_early: calls before user_list; g_tr_busy: busy transitions */
static uint64_t g_un_at;
static void un_reset(void) { memset(g_un_map, 0, sizeof g_un_map); g_un_calls = g_un_busy = g_un_refuse = g_un_delay = 0; g_un_pend = -1; }
static int rp_call(void *game, uint64_t fn, int n, const uint64_t *pname, const void *const *pval)
{
    if (fn == S_SETUP_LIST_T && g_list_busy > 0) { g_list_busy--; return 1; }
    if (fn == csym64("usernamepressed")) {
        if (n != 1 || pname[0] != csym64("pressed_user_index")) return -1;
        if (g_un_refuse) return -1;
        if (g_un_busy) { if (g_un_busy > 0) g_un_busy--; return 1; }
        int r = *(const int32_t *)pval[0];
        if (g_un_calls < 32) g_un_idx[g_un_calls] = r;
        g_un_calls++;
        g_un_early += g_last_transition != csym64("user_list");
        g_un_pend = r >= 0 && r < 8 && g_un_map[r][0] ? r : -1;
        g_un_at = g_now + (uint64_t)g_un_delay;
        return 0;
    }
    (void)game;
    if (fn == csym64("transitionarmcomputertostate") && g_tr_busy > 0) { g_tr_busy--; return 1; }
    if (fn == csym64("transitionarmcomputertostate") && n == 1) g_last_transition = *(const uint64_t *)pval[0];
    if (fn == csym64("setupgenericprompt"))   /* the tablet prompt's message, compared with the pause done page */
        for (int k = 0; k < n; k++) {
            if (pname[k] == csym64("param_prompt_message")) snprintf(g_prompt_msg, sizeof g_prompt_msg, "%s", (const char *)pval[k]);
            if (pname[k] == csym64("param_prompt_back")) snprintf(g_prompt_back, sizeof g_prompt_back, "%s", (const char *)pval[k]);
        }
    if (fn == S_SETUP_LIST_T) {
        g_list_calls++;
        g_list_n = 0;
        for (int k = 0; k < n; k++) {
            if (pname[k] == S_NUM) g_list_n = *(const int32_t *)pval[k];
            for (int i = 0; i < 5; i++) if (pname[k] == S_NAMES[i]) snprintf(g_list_rows[i], 64, "%s", (const char *)pval[k]);
        }
        if (!g_cleared) g_list_noclear++;
        g_cleared = 0;
        if (g_list_n > 4) g_big_lists++;
        for (int i = 0; i < g_list_n && i < 5; i++) if (strstr(g_list_rows[i], "MORE")) g_more_rows++;
    }
    return 0;
}
static uint64_t RP_CARD_T = 0x61c4b2b9153a12b7ull;
static int rp_settext(void *game, uint64_t actor, uint64_t el, const char *s)
{
    (void)game;
    if (actor == RP_CARD_T && el == csym64("party_label")) g_settext_card_el++;
    int i = find_w(actor, el); snprintf(W[i].text, sizeof W[i].text, "%s", s); W[i].has_text = 1; return 1;
}
static int rp_gettext(void *game, uint64_t actor, uint64_t el, char *out, size_t cap)
{
    (void)game;
    if (actor == RP_CARD_T && el == csym64("name_text")) { snprintf(out, cap, "Bob"); return 1; }
    for (int i = 0; i < g_nw; i++)   /* a written element reads back what was written */
        if (W[i].actor == actor && W[i].el == el && W[i].has_text) { snprintf(out, cap, "%s", W[i].text); return 1; }
    out[0] = 0; return 1;
}
static int rp_fire(void *game, uint64_t holder, uint64_t sym)
{
    (void)game; (void)holder;
    if (sym == csym64("evt_dbg_toggle_menu_vis")) { g_toggles++; g_pause_on = !g_pause_on; }
    if (g_nfires < 64) g_fires[g_nfires++] = sym;
    return 1;
}
static int g_press_comp_n; static uint64_t g_press_comp[3];   /* rp_press_comp: calls, last (actor, event, component) */
static int rp_press_comp(void *game, uint64_t actor, uint64_t ev0, uint64_t comp)
{ (void)game; g_press_comp_n++; g_press_comp[0] = actor; g_press_comp[1] = ev0; g_press_comp[2] = comp; return 1; }
static int rp_rewrite(const uint64_t *ev, uint64_t sym) { *(uint64_t *)ev = sym; return 1; }
static const char *PLAYERS_JSON =
    "{\"session_id\":\"S-1\",\"match_type\":\"Echo_Arena\",\"players\":["
    "{\"user_id\":\"1001\",\"display_name\":\"P1\"},{\"user_id\":\"1002\",\"display_name\":\"P2\"},"
    "{\"user_id\":\"1003\",\"display_name\":\"P3\"},{\"user_id\":\"1004\",\"display_name\":\"P4\"},"
    "{\"user_id\":\"1005\",\"display_name\":\"P5\"},{\"user_id\":\"1006\",\"display_name\":\"P6\"},"
    "{\"user_id\":\"1007\",\"display_name\":\"P7\"},{\"user_id\":\"1008\",\"display_name\":\"P8\"}]}";
static void (*g_http_hook)(void);   /* runs while a request is "in flight" */
static int rp_http(const char *method, int client_api, const char *path, const char *body, char *out, size_t cap)
{
    (void)client_api;
    if (g_http_hook) g_http_hook();
    if (!strcmp(method, "GET") && !strncmp(path, "/reports/players", 16)) { g_gets++; snprintf(out, cap, "%s", PLAYERS_JSON); return 200; }
    if (!strcmp(method, "POST") && !strcmp(path, "/reports")) { g_posts++; snprintf(g_post_body, sizeof g_post_body, "%s", body); snprintf(out, cap, "{}"); return g_post_status ? g_post_status : 201; }
    return 404;
}
static int rp_user(char *out, size_t cap) { snprintf(out, cap, "4242"); return 1; }
/* ev[2] handles of the fake world: 0xCA4D the card, 0x0E0n popup row n (pointer events on p22) */
static uint64_t test_actor(uint64_t a)
{
    if (a == 0xCA4Dull) return RP_CARD_T;
    if (a == 0x7AB1ull) return 0x6c1f6ff04e070923ull;   /* w23e: the arm tablet state_mgr actor */
    if (a == 0x7AB3ull) return 0x63e9a9ed5e5fabbbull;   /*: the tablet COMPONENT actor (stock X) */
    if (a == 0x7AB2ull) return csym64("some_other_actor");
    if (a >= 0x0E01ull && a <= 0x0E06ull) { char b[16]; snprintf(b, sizeof b, "ffa_rp_row%d", (int)(a - 0x0E00)); return csym64(b); }
    return 0;
}
#define rp_actor(gs, a) test_actor(a)
/* the platform's CR15PointerCS::ScrollAmount reader (1 read, 0 unavailable) */
static int g_scroll_ok = 1, g_scroll_calls;
static float g_scroll_a;
static int rp_scroll_amount(void *game, float *out) { (void)game; g_scroll_calls++; if (!g_scroll_ok) return 0; *out = g_scroll_a; return 1; }
struct sess_snapshot_fwd;
#define RP_STUB_SESS 1
static int rp_sprite_uv(void *game, uint64_t actor, uint64_t el, uint64_t region) { (void)game; (void)actor; (void)el; (void)region; return 1; }
static int rp_show(void *game, uint64_t actor, uint64_t el, uint32_t show) { (void)game; int i = find_w(actor, el); W[i].shown = (int)show; W[i].has_show = 1; return 1; }
static int g_xpid_unread;   /*: 1 = the card's xplatformid read fails */
static int rp_getvar(void *game, uint64_t actor, uint64_t comp, uint64_t var, char *out, size_t cap)
{ (void)game; (void)actor; (void)comp; (void)var;
  if (g_un_pend >= 0 && g_now >= g_un_at) { snprintf(g_xpid, sizeof g_xpid, "%s", g_un_map[g_un_pend]); g_un_pend = -1; }
  if (g_od_pend >= 0 && g_now >= g_od_at) { snprintf(g_xpid, sizeof g_xpid, "%s", g_od_map[g_od_pend]); g_od_pend = -1; }
  if (g_xpid_unread) return 0;
  snprintf(out, cap, "%s", g_xpid); return 1; }
static int g_ac_two = 1; /* the canvas has acr_sent_l1 (w23b two-line sentence); 0 = a w23a canvas */
static int g_ac_fill = 1; /* the canvas has acr_submit_fill (w23b on); 0 = a w23a canvas: no fill write */
static int g_ac_bound;   /* the generic prompt actor draws ffa_ac_report (w23a); 0: stock canvas -> ac falls back to popup */
static int rp_has_el(void *game, uint64_t actor, uint64_t el)
{
    (void)game;
    if (actor == RP_CARD_T && el == csym64("report_label")) return g_card_five;
    if (actor == 0x8a5dcee389df5b7cull && el == csym64("acr_root")) return g_ac_bound;
    if (actor == 0x8a5dcee389df5b7cull && el == csym64("acr_sent_l1")) return g_ac_two;
    if (actor == 0x8a5dcee389df5b7cull && el == csym64("acr_submit_fill")) return g_ac_fill;
    return 1;
}
static int rp_color(void *game, uint64_t actor, uint64_t el, const float rgb[3])
{ (void)game; int i = find_w(actor, el); memcpy(W[i].rgb, rgb, sizeof W[i].rgb); W[i].has_rgb = 1; return 1; }
static int rp_show_page(void *game, uint64_t owner)
{
    (void)game; g_last_page = owner; g_pages_shown++;
    g_p1_en += owner == 0x68db70ece7c24901ull; g_p22_en += owner == csym64("ffa_rp_list_page"); g_p23_en += owner == csym64("ffa_rp_reasons_page");
    return 1;
}
static uint64_t g_p23_at;   /* M3: p23's owner loads only from this time on (0 = at once) */
static int rp_has_actor(void *game, uint64_t actor)
{
    (void)game;
    if (actor == csym64("ffa_rp_reasons_page")) return g_popup_marker && g_pause_on && g_now >= g_p23_at;
    return actor == csym64("ffa_rp_list_page") ? g_popup_marker : 1;
}
/* M2: the platform's arm-tablet state-stack reader. The stub tablet's top is the last transition (user_actions before
 * any), deactivated while g_tab_closed, g_tab_force when set; g_tab_ok = 0: the reader is unavailable */
static int g_tab_ok = 1, g_tab_closed;
static uint64_t g_tab_force;
static int rp_tablet_top(void *game, uint64_t *top)
{
    (void)game;
    if (!g_tab_ok) return 0;
    *top = g_tab_closed ? csym64("deactivated") : g_tab_force ? g_tab_force : g_last_transition ? g_last_transition : csym64("user_actions");
    return 1;
}
static uintptr_t g_card_key = 1;   /* a new value = a new card gamespace (rp_live_tick re-decides five-tile per key) */
static uintptr_t rp_actor_key(void *game, uint64_t actor) { (void)game; (void)actor; return g_card_key; }

/* the header expects these before its own definitions (platform tentative definitions) */
static int g_rp_on, g_rp_ready;
static uint64_t g_rp_tile_uid, g_rp_card_event, g_rp_card_uid, g_rp_card_el, S_STATE_MGR;

#include "../src/sess_snapshot.h"
/* g_sess_n > 0: the reader answers with a local player (slot 0) and g_sess_n others; entries with g_sess_bot set are bots
 * (provider & 0xf == 6, account != 0, as CNSLobby::EntrantIsBot). 0: unverified build -> the host fallback rows. */
static int g_sess_n, g_sess_bot[16];
static int g_sess_quest;   /* 1 = the Quest slot shape: the local player holds the LAST slot, others[k] = idx k, slot k - 1  */
static int rp_sess(void *game, int want, sess_snapshot *s)
{
    (void)game; (void)want;
    if (g_sess_n <= 0) return -100;
    memset(s, 0, sizeof *s);
    s->has_sessionid = 1; snprintf(s->sessionid, sizeof s->sessionid, "S-RD");
    s->has_lob = 1; snprintf(s->match_type, sizeof s->match_type, "Echo_Arena");
    s->n_players = g_sess_n + 1;
    s->players[0].local = 1; s->players[0].has_userid = 1; s->players[0].provider = 1; s->players[0].userid = 4242;
    snprintf(s->players[0].name, sizeof s->players[0].name, "ME");
    if (g_sess_quest) s->players[0].slot = (uint16_t)g_sess_n;
    for (int k = 1; k <= g_sess_n; k++) {
        sess_player *p = &s->players[k];
        p->i = (uint16_t)k; p->slot = (uint16_t)(g_sess_quest ? k - 1 : k); p->has_userid = 1; p->has_entrant = 1;
        p->provider = g_sess_bot[k - 1] ? 0x6 : 0x1; p->userid = 2000u + (uint64_t)k;
        snprintf(p->name, sizeof p->name, "%s%d", g_sess_bot[k - 1] ? "BOT" : "H", k);
    }
    return 0;
}

#ifndef RP_PLATFORM   /* the Quest path: add -DRP_PLATFORM='"quest"' to the build line (the suite then pins the Quest fills) */
#define RP_PLATFORM "pc"
#endif
#define RP_HAVE_ZONE_OFF 1   /* stock-off: the fake button pool below (off by default: the suites above run the fallback swallow) */
#include "../src/report_flow.h"

/* ---- stock-off: a fake CR15ButtonInteractCS pool laid out as the engine's (finding): handle index -> dense u16 at
 * [cs+0xe0] (u32 rows, generation high), reasons u16 [cs+0xf8] per dense slot, CI [cs+0x110] stride 0x190 (+0 data index),
 * rows [[cs+0xd0]] stride 0x128 (+0 zone, +8 actor). Dense slots run backwards so the handle -> slot mapping is exercised.
 * rp_zone_cat implements the bit semantics: Disable ORs 0x80, Enable clears only 0x80; live = no reason bit. */
#define FZ_N 32   /* the REPORT set + all 10 of our layer zones */
static const uint64_t GPA_SO = 0x8a5dcee389df5b7cull;   /* the generic prompt actor: our own zones */
static struct { uint64_t name, actor; uint8_t pad[0x128 - 16]; } g_fz_rows[FZ_N];
static uint8_t g_fz_ci[FZ_N * 0x190], g_fz_cs[0x200];
static uint32_t g_fz_dense[FZ_N], g_fz_h[FZ_N];
static uint16_t g_fz_reason[FZ_N];
static uintptr_t g_fz_rows_p;
static int g_fz_n, g_fz_ready, g_fz_card = 1, g_fz_dis, g_fz_en, g_fz_fault, g_fz_catfail, g_fz_badrow, g_fz_ldis, g_fz_len;
static uintptr_t g_fz_gs = 0x6001;
static int fz_rd(void *d, uintptr_t src, size_t n) { if (!src) return -1; memcpy(d, (const void *)src, n); return 0; }
static void fz_put(size_t off, const void *p) { uintptr_t v = (uintptr_t)p; memcpy(g_fz_cs + off, &v, sizeof v); }
static int fz_add(uint64_t actor, uint64_t name, uint16_t reasons)
{
    int i = g_fz_n++, d = FZ_N - 1 - i;
    uint16_t di = (uint16_t)i;
    g_fz_rows[i].name = name; g_fz_rows[i].actor = actor;
    memcpy(g_fz_ci + d * 0x190, &di, 2);
    g_fz_dense[i] = (uint32_t)d | 0x00050000u;
    g_fz_h[i] = (uint32_t)i | 0x00010000u;
    g_fz_reason[d] = reasons;
    return i;
}
static int fz_idx(uint64_t actor, uint64_t name) { for (int i = 0; i < g_fz_n; i++) if (g_fz_rows[i].actor == actor && g_fz_rows[i].name == name) return i; return -1; }
static uint16_t *fz_rs(int i) { return &g_fz_reason[(uint16_t)g_fz_dense[i]]; }
static int fz_so(int e) { return fz_idx(g_rpt_so_def[e].actor, rpt_so_zone(&g_rpt_so, e)); }
static uint16_t fz_reason_of(int e) { return *fz_rs(fz_so(e)); }
static void fz_init(void)
{
    memset(g_fz_rows, 0, sizeof g_fz_rows); memset(g_fz_ci, 0, sizeof g_fz_ci); memset(g_fz_cs, 0, sizeof g_fz_cs);
    memset(g_fz_reason, 0, sizeof g_fz_reason); g_fz_n = 0;
    g_fz_rows_p = (uintptr_t)g_fz_rows;
    fz_put(0xd0, &g_fz_rows_p); fz_put(0xe0, g_fz_dense); fz_put(0xf8, g_fz_reason); fz_put(0x110, g_fz_ci);
    rpt_so_syms(&g_rpt_so);
    for (int e = 0; e < RPT_SO_N; e++)   /* the device's REPORT view: X, footer, button5 live; the LOBBY-only zones off by the stock (bit 1) */
        fz_add(g_rpt_so_def[e].actor, rpt_so_zone(&g_rpt_so, e), e == RPT_SO_X || !g_rpt_so_def[e].hold || e == RPT_MB_FOOTER ? 0 : 1);
    fz_add(GPA_SO, csym64("ffa_acr_back"), 0);   /* our own "<": never touched */
    fz_add(GPA_SO, csym64("ffa_acr_cancel"), 0);
    g_fz_dis = g_fz_en = g_fz_fault = g_fz_catfail = g_fz_badrow = 0; g_fz_card = 1;
}
static int rp_zone_ready(const char **why) { *why = "test: Enable / Disable bytes differ"; return g_fz_ready; }
static int rp_zone_find(void *game, uint64_t actor, uint64_t zone, rp_zref *z)
{
    (void)game;
    if (!g_fz_ready || g_fz_fault) return -1;
    if (actor == 0x61c4b2b9153a12b7ull && !g_fz_card) return 0;
    int i = fz_idx(actor, zone);
    if (i < 0) return 0;
    z->gs = g_fz_gs; z->cs = (uintptr_t)g_fz_cs; z->h = g_fz_h[i];
    return rp_zone_check(fz_rd, z->cs, g_fz_badrow ? g_fz_h[(i + 1) % g_fz_n] : z->h, actor, zone, &z->reasons);
}
static int rp_zone_cat(void *game, const rp_zref *z, int off)
{
    (void)game;
    if (g_fz_catfail) return 0;
    uint16_t *r = &g_fz_reason[(uint16_t)g_fz_dense[z->h & 0xffff]];
    int own = g_fz_rows[z->h & 0xffff].actor == GPA_SO;   /* our layer zones: counted apart (g_fz_ldis / g_fz_len) */
    if (off) { *r |= RP_SO_BIT; if (own) g_fz_ldis++; else g_fz_dis++; } else { *r &= (uint16_t)~RP_SO_BIT; if (own) g_fz_len++; else g_fz_en++; }
    return 1;
}

/* ---- helpers ---- */
static void tick(int ms) { g_now += (uint64_t)ms; report_worker_step(); report_tick(NULL, g_now); report_worker_step(); g_now += 20; report_tick(NULL, g_now); }
static int press_btn(int b)   /* popup button via the target chain */
{
    uint64_t ev[5] = { csym64("evt_btn_clicked"), 0, 0, g_rpp_uid[b], 0 };
    int r = report_event(ev, 1, NULL);
    tick(300);
    return r;
}
static void press_row(int n) { uint64_t ev[5] = { S_PRESSED[n], 0, 0, 0, 0 }; report_event(ev, 0, NULL); tick(300); }
static const char *wtext(uint64_t a, const char *el) { return W[find_w(a, csym64(el))].text; }
static int wshown(uint64_t a, const char *el) { return W[find_w(a, csym64(el))].shown; }
static const float *wrgb(uint64_t a, const char *el) { return W[find_w(a, csym64(el))].rgb; }
static uint64_t A(const char *n) { return csym64(n); }
/* the tablet list fits: at most 4 rows (about 4 show), each at most 26 characters */
static void scroll_ev(int n)   /* n x rad_report_scroll_down (n < 0: _up), one tick each */
{
    for (int i = 0; i < (n < 0 ? -n : n); i++) {
        uint64_t ev[5] = { csym64(n < 0 ? "rad_report_scroll_up" : "rad_report_scroll_down"), 0, 0, 0, 0 };
        report_event(ev, 0, NULL); tick(50);
    }
}
static void run_amount(float a, int ms)   /* the stub ScrollAmount held for ms, one tick per 20 ms */
{
    g_scroll_a = a;
    for (int t = 0; t < ms; t += 20) { g_now += 20; report_tick(NULL, g_now); }
    g_scroll_a = 0;
}
static int rows_fit(void)
{
    if (g_list_n > 4) return 0;
    for (int i = 0; i < g_list_n; i++) if (strlen(g_list_rows[i]) > 26) return 0;
    return 1;
}

static void test_ids(void)
{
    CHECK(g_rpp_uid[RPP_B_ROW1] == 0x03b3265128417456ull, "row1 uid %016llx", (unsigned long long)g_rpp_uid[RPP_B_ROW1]);
    CHECK(g_rpp_owner[RPP_LIST] == 0xdc44fc9933702657ull, "list page owner");
    CHECK(g_rp_card5_uid == 0x2fd59a8b4a7b59dfull && csym64("button5") == g_rp_card5_uid, "button5 uid");
    CHECK(g_rp_card5_event == 0xfdb213d9dc5e4826ull, "card5 event = delegate_OnFullyDepressed");
    /* the w22a names and the pointer events radmenu.c knows by hash */
    CHECK(g_rpp_actor[RPP_B_LIST_SUBMIT] == 0xe6b8387a8d5de6ecull && g_rpp_uid[RPP_B_LIST_SUBMIT] == 0x47b47a2b184dd1bcull, "list SUBMIT actor / uid");
    CHECK(csym64("ffa_rp_row_peek") == 0x03b3263f07517859ull && g_rpp_actor[RPP_PEEK] == csym64("ffa_rp_row6"), "peek canvas name, peek = row6");
    CHECK(S_PTR_START == 0x55c56d4a9201a66cull && S_PTR_END == 0x7005afbd6c613578ull && S_PTR_HOVER == 0x0c40e9c29ebcc824ull
          && S_PTR_IDLE == 0x27b7ebd0100cb25dull, "pointer event symbols = radmenu.c EV_PTR_*");
}

static void test_popup(void)
{
    uint64_t ev[5] = { csym64("evt_btn_clicked"), 0, 0, g_rp_tile_uid, 0 };
    CHECK(report_event(ev, 1, NULL) == 1, "tile press consumed on the target chain");
    tick(100);
    tick(100);
    CHECK(g_gets == 1, "host fallback GET (reader unverified): %d", g_gets);
    CHECK(g_last_page == A("ffa_rp_list_page"), "list page enabled");
    CHECK(g_rpp_page == RPP_LIST && g_rp_nrows == 8, "8 rows (page %d rows %d)", g_rpp_page, g_rp_nrows);
    CHECK(!strcmp(wtext(A("ffa_rp_row1"), "buttontext"), "P1"), "row1 = P1 (%s)", wtext(A("ffa_rp_row1"), "buttontext"));
    CHECK(!strcmp(wtext(A("ffa_rp_row5"), "subtext"), "Online") && !wtext(A("ffa_rp_row6"), "subtext")[0], "row5 Online, peek row: name only");
    CHECK(!strcmp(wtext(A("ffa_rp_row6"), "buttontext"), "P6") && wshown(A("ffa_rp_row6"), "buttontext") == 1, "peek row = P6");
    scroll_ev(3);   /* to the end: names 4-8 */
    CHECK(!strcmp(wtext(A("ffa_rp_row1"), "buttontext"), "P4") && !strcmp(wtext(A("ffa_rp_row5"), "buttontext"), "P8"), "scrolled: row1 P4, row5 P8 (%s)", wtext(A("ffa_rp_row1"), "buttontext"));
    CHECK(wshown(A("ffa_rp_row6"), "buttontext") == 0 && !wtext(A("ffa_rp_row6"), "buttontext")[0], "peek hidden at the end");
    press_btn(RPP_PEEK);   /* the hidden peek row: ignored */
    CHECK(g_rpp_page == RPP_LIST, "empty row ignored");
    press_btn(RPP_B_ROW1 + 4);   /* P8 */
    CHECK(g_last_page == A("ffa_rp_reasons_page") && g_rpp_page == RPP_REASONS, "reasons page");
    CHECK(strstr(wtext(A("ffa_rp_reasons_frame"), "sentence"), "P8 will be reported") != NULL, "sentence names P8");
    CHECK(wshown(A("ffa_rp_mg"), "check") == 0 && g_rpp_mg == 0, "MG default off");
    CHECK(!W[find_w(A("ffa_rp_mg"), csym64("buttonbg"))].has_show && !W[find_w(A("ffa_rp_mg"), csym64("buttonoutline"))].has_show
          && !W[find_w(A("ffa_rp_mg"), csym64("buttonicon"))].has_show && !W[find_w(A("ffa_rp_mg"), csym64("buttontext"))].has_show,
          "report_mute_ghost=1: no Mute & Ghost box hide writes");
    CHECK(wrgb(A("ffa_rp_submit"), "buttontext")[0] < 0.6f, "SUBMIT dim");
    press_btn(RPP_B_SUBMIT);
    CHECK(g_posts == 0 && g_rpp_page == RPP_REASONS, "SUBMIT ignored without a reason");
    press_btn(RPP_B_REASON1 + 2);
    CHECK(wrgb(A("ffa_rp_reason3"), "buttonhighlight")[1] > 0.6f && wshown(A("ffa_rp_reason3"), "selected") == 1, "reason3 teal + selected");
    CHECK(wrgb(A("ffa_rp_reason1"), "buttonhighlight")[1] < 0.4f && wshown(A("ffa_rp_reason1"), "selected") == 0, "reason1 stock");
    press_btn(RPP_B_REASON1 + 4);
    CHECK(wshown(A("ffa_rp_reason3"), "selected") == 0 && wshown(A("ffa_rp_reason5"), "selected") == 1, "single select");
    CHECK(wrgb(A("ffa_rp_submit"), "buttontext")[0] > 0.9f, "SUBMIT enabled");
    press_btn(RPP_B_MG);
    CHECK(wshown(A("ffa_rp_mg"), "check") == 1, "MG toggled on");
    { uint64_t e2[5] = { csym64("evt_btn_clicked"), 0, 0, g_rpp_uid[RPP_B_MG], 0 }; report_event(e2, 0, NULL); g_now += 10; report_tick(NULL, g_now); }
    CHECK(g_rpp_mg == 1, "same press on the core chain within 250 ms debounced");
    press_btn(RPP_B_SUBMIT);
    tick(100);
    CHECK(g_posts == 1, "POST sent");
    CHECK(strstr(g_post_body, "\"reason\":\"unsportsmanlike\"") && strstr(g_post_body, "\"mute_ghost\":true")
          && strstr(g_post_body, "\"target_user_id\":\"1008\"") && strstr(g_post_body, "\"session_id\":\"S-1\""), "body %s", g_post_body);
    CHECK(g_last_page == A("ffa_rp_done_page") && !strcmp(wtext(A("ffa_rp_done_frame"), "sentence"), "Report submitted. Thank you."), "done page");
    press_btn(RPP_B_OK);
    CHECK(g_last_page == 0x68db70ece7c24901ull && g_rpp_page == -1, "OK -> pause p1");
    /* CANCEL from the reasons page */
    ev[0] = csym64("evt_btn_clicked"); ev[3] = g_rp_tile_uid; report_event(ev, 1, NULL); tick(100); tick(100);
    press_btn(RPP_B_ROW1);
    press_btn(RPP_B_CANCEL);
    CHECK(g_last_page == 0x68db70ece7c24901ull && g_posts == 1, "CANCEL -> p1, no POST");
    /* no marker: the tablet flow */
    g_popup_marker = 0;
    int before = g_pages_shown;
    ev[0] = csym64("evt_btn_clicked"); ev[3] = g_rp_tile_uid; report_event(ev, 1, NULL); tick(100);
    CHECK(g_pages_shown == before && g_rp_state == RP_OPENING, "no marker -> tablet staging (state %d)", g_rp_state);
    rp_reset("test"); g_rpp_wait = 0; g_popup_marker = 1;
}

static void test_five(void)
{
    g_pause_on = 0;
    g_card_five = 1; g_rp_five_key = 0; g_rp_five = -1;
    int relabels = g_settext_card_el;
    tick(1100);
    CHECK(g_rp_five == 1, "five-tile detected");
    CHECK(g_settext_card_el == relabels, "no v1 relabel on the five-tile canvas");
    CHECK(wshown(RP_CARD_T, "report_highlight") == 0 && wshown(RP_CARD_T, "report_label") == 1, "open: highlight hidden, label shown (reader unavailable)");
    uint64_t inv[5] = { g_rp_card_event, 0, 0xCA4D, g_rp_card_uid, 0 };
    CHECK(report_event(inv, 1, NULL) == 0 && inv[0] == g_rp_card_event, "INVITE keeps its stock action");
    uint64_t b5[5] = { g_rp_card5_event, 0, 0xCA4D, g_rp_card5_uid, 0 };
    CHECK(report_event(b5, 1, NULL) == 1 && b5[0] == S_RP_NOOP, "button5 press consumed");
    tick(100);
    CHECK(g_rp_state == RP_PRESET, "button5 -> p23 preset (state %d)", g_rp_state);
    CHECK(g_rp_learn5_on == 1, "learn logging on after the card open");
    rp_reset("test"); g_rpp_page = -1; g_rpp_preset = 0; g_pause_on = 0;
}

/*: report=auto acts only while the popup marker page is loaded; stock content stays untouched */
static void test_auto(void)
{
    int on = g_rp_on, five = g_card_five;
    g_rp_on = -1; g_rp_live = -1; g_rp_next_live = 0; g_popup_marker = 0;
    g_card_five = 0; g_card_key++;   /* stock card canvas: no report_label */
    tick(1100);
    CHECK(g_rp_live == 0, "auto, no marker: inert (live %d)", g_rp_live);
    int gets = g_gets, pages = g_pages_shown;
    uint64_t ev[5] = { csym64("evt_btn_clicked"), 0, 0, g_rp_tile_uid, 0 };
    CHECK(report_event(ev, 1, NULL) == 0 && ev[0] == csym64("evt_btn_clicked"), "auto, no marker: tile event untouched");
    tick(1100); tick(100);
    CHECK(g_gets == gets && g_pages_shown == pages && g_rp_state == RP_IDLE && g_rpp_page == -1, "auto, no marker: no flow");
    g_popup_marker = 1; tick(1100);
    CHECK(g_rp_live == 1, "auto + marker: live");
    ev[0] = csym64("evt_btn_clicked");
    CHECK(report_event(ev, 1, NULL) == 1, "auto + marker: tile consumed");
    tick(100); tick(100);
    CHECK(g_rpp_page == RPP_LIST, "auto + marker: list page");
    g_popup_marker = 0; tick(1100);
    CHECK(g_rp_live == 0 && g_rpp_page == -1, "marker gone: flow dropped");
    /*: the five-tile card (report_label) wakes the flow without the pause pages */
    g_card_five = 1; g_card_key++; tick(1100);
    CHECK(g_rp_live == 1, "auto + five-tile card, no popup marker: live");
    g_card_five = 0; g_card_key++; tick(1100);
    CHECK(g_rp_live == 0, "stock card again: inert");
    g_card_five = five; g_card_key++; g_popup_marker = 1; g_rp_on = on; tick(1100);
}

/*: report_fake_rows (test only) -> 8 real + 12 synthetic = 20 rows (the list scrolls), a synthetic
 * target never POSTs */
static void test_fake(void)
{
    CHECK(report_option("report_fake_rows", "99") && g_rp_fake_rows == RP_FAKE_MAX, "report_fake_rows capped at %d", RP_FAKE_MAX);
    report_option("report_fake_rows", "12");
    int posts = g_posts;
    uint64_t ev[5] = { csym64("evt_btn_clicked"), 0, 0, g_rp_tile_uid, 0 };
    report_event(ev, 1, NULL); tick(100); tick(100);
    CHECK(g_rpp_page == RPP_LIST && g_rp_nrows == 20, "20 rows (%d)", g_rp_nrows);
    CHECK(!strcmp(wtext(A("ffa_rp_row1"), "buttontext"), "P1") && !strcmp(wtext(A("ffa_rp_row6"), "buttontext"), "P6")
          && wshown(A("ffa_rp_row6"), "buttontext") == 1, "top: P1 + peek P6");
    scroll_ev(30);   /* clamps at 20 - 5 */
    CHECK(g_rpp_off == 15 && !strcmp(wtext(A("ffa_rp_row1"), "buttontext"), "TEST 8") && !strcmp(wtext(A("ffa_rp_row5"), "buttontext"), "TEST 12"),
          "end: off %d, row1 %s", g_rpp_off, wtext(A("ffa_rp_row1"), "buttontext"));
    CHECK(wshown(A("ffa_rp_row6"), "buttontext") == 0, "end: peek hidden");
    CHECK(wrgb(A("ffa_rp_row6"), "buttonhighlight")[0] < 0.45f, "hidden row hover retinted to the body");
    CHECK(wrgb(A("ffa_rp_row5"), "buttonhighlight")[0] > 0.55f, "used row hover keeps its tint");
    CHECK(wshown(A("ffa_rp_row6"), "buttonicon") == 0 && wshown(A("ffa_rp_row6"), "buttonoutline") == 0 && wshown(A("ffa_rp_row6"), "buttonbg") == 0,
          "hidden row hides buttonicon / buttonoutline / buttonbg");
    CHECK(wshown(A("ffa_rp_row2"), "buttonicon") == 1, "used row shows buttonicon");
    scroll_ev(-3);   /* names 13-17: TEST 5 .. TEST 9, peek TEST 10 */
    CHECK(wrgb(A("ffa_rp_row6"), "buttonhighlight")[0] > 0.55f && wshown(A("ffa_rp_row6"), "buttontext") == 1, "peek hover restored");
    press_btn(RPP_B_ROW1);
    CHECK(g_rpp_page == RPP_REASONS && g_rp_target_fake == 1 && strstr(wtext(A("ffa_rp_reasons_frame"), "sentence"), "TEST 5 will"), "test row target");
    CHECK(wshown(A("ffa_rp_mg"), "check") == 0 && wshown(A("ffa_rp_mg"), "check2") == 0, "tick: both strokes hidden by default");
    press_btn(RPP_B_MG);
    CHECK(wshown(A("ffa_rp_mg"), "check") == 1 && wshown(A("ffa_rp_mg"), "check2") == 1, "tick: both strokes shown after a press");
    press_btn(RPP_B_REASON1);
    press_btn(RPP_B_SUBMIT);
    tick(100); tick(100);
    CHECK(g_posts == posts, "synthetic target: no POST (%d -> %d)", posts, g_posts);
    CHECK(g_last_page == A("ffa_rp_done_page") && !strcmp(wtext(A("ffa_rp_done_frame"), "sentence"), "Test row: report not sent."), "test row done page");
    press_btn(RPP_B_OK);
    /* a real row in the same list still POSTs */
    ev[0] = csym64("evt_btn_clicked"); report_event(ev, 1, NULL); tick(100); tick(100);
    press_btn(RPP_B_ROW1 + 1); press_btn(RPP_B_REASON1); press_btn(RPP_B_SUBMIT); tick(100); tick(100);
    CHECK(g_posts == posts + 1 && g_rp_target_fake == 0 && strstr(g_post_body, "\"target_user_id\":\"1002\""), "real row posts");
    press_btn(RPP_B_OK);
    report_option("report_fake_rows", "0");
}

/* the one `report=` parser both platforms call */
static void test_parse(void)
{
    static const struct { const char *v; int want; } c[] = {
        { "", -1 }, { "auto", -1 }, { "AUTO", -1 }, { " auto ", -1 }, { "-1", -1 }, { "-5", -1 },
        { "0", 0 }, { "1", 1 }, { "0x1", 1 }, { "0x0", 0 }, { "2", 1 }, { " 1 ", 1 }, { "on", 1 }, { "off", 0 },
        { "ON", 1 }, { "Off", 0 }, { "true", 1 }, { "false", 0 }, { "abc", -1 }, { "1x", -1 }, { "0xzz", -1 }, { "\t0\r", 0 } };
    for (size_t i = 0; i < sizeof c / sizeof c[0]; i++) {
        int g = rp_parse_report(c[i].v);
        CHECK(g == c[i].want, "report=\"%s\" -> %d, want %d", c[i].v, g, c[i].want);
    }
    CHECK(rp_parse_report(NULL) == -1, "NULL -> auto");
}

/* AI bots (provider & 0xf == 6 with an account) are left out of the reader rows; host rows are untouched */
static void test_bots(void)
{
    g_sess_n = 4; g_sess_bot[0] = 0; g_sess_bot[1] = 1; g_sess_bot[2] = 1; g_sess_bot[3] = 0;
    CHECK(g_rp_hide_bots == 1, "report_hide_bots defaults on");
    CHECK(rp_rows_from_reader(NULL) == 1 && g_rp_nrows == 2 && !strcmp(g_rp_rows[0], "H1") && !strcmp(g_rp_rows[1], "H4"),
          "bots hidden: %d rows (%s, %s)", g_rp_nrows, g_rp_rows[0], g_rp_rows[1]);
    CHECK(!strcmp(g_rp_rowid[1], "2004") && g_rp_rowslot[1] == 4, "row ids keep their slots after a skipped bot");
    CHECK(report_option("report_hide_bots", "0") && g_rp_hide_bots == 0, "report_hide_bots=0");
    rp_rows_from_reader(NULL);
    CHECK(g_rp_nrows == 4 && !strcmp(g_rp_rows[1], "BOT2"), "report_hide_bots=0: bots back (%d)", g_rp_nrows);
    report_option("report_hide_bots", "1");
    /* only bots: the popup says there is nobody to report */
    g_sess_n = 3; g_sess_bot[0] = g_sess_bot[1] = g_sess_bot[2] = 1;
    uint64_t ev[5] = { csym64("evt_btn_clicked"), 0, 0, g_rp_tile_uid, 0 };
    report_event(ev, 1, NULL); tick(100); tick(100);
    CHECK(g_rp_nrows == 0 && g_last_page == A("ffa_rp_done_page")
          && !strcmp(wtext(A("ffa_rp_done_frame"), "sentence"), "No other players to report."), "only bots: %s", wtext(A("ffa_rp_done_frame"), "sentence"));
    press_btn(RPP_B_OK);
    /* a bot plus a human: the human is row 1 and reports normally */
    g_sess_n = 2; g_sess_bot[0] = 1; g_sess_bot[1] = 0;
    ev[0] = csym64("evt_btn_clicked");
    report_event(ev, 1, NULL); tick(100); tick(100);
    CHECK(g_rpp_page == RPP_LIST && g_rp_nrows == 1 && !strcmp(wtext(A("ffa_rp_row1"), "buttontext"), "H2"), "human is row 1");
    CHECK(wshown(A("ffa_rp_row2"), "buttontext") == 0, "row 2 hidden");
    press_btn(RPP_B_ROW1); press_btn(RPP_B_REASON1); press_btn(RPP_B_SUBMIT); tick(100); tick(100);
    CHECK(strstr(g_post_body, "\"target_user_id\":\"2002\"") != NULL, "POST targets the human: %s", g_post_body);
    press_btn(RPP_B_OK);
    g_sess_n = 0;
    /* host-fallback rows are never filtered */
    rp_parse_host(PLAYERS_JSON);
    CHECK(g_rp_nrows == 8, "host rows untouched (%d)", g_rp_nrows);
}

/* the POST body carries "entry" (what opened the flow) and "platform" */
static void test_entry(void)
{
    int posts = g_posts;
    /* pause tile -> popup pages */
    uint64_t ev[5] = { csym64("evt_btn_clicked"), 0, 0, g_rp_tile_uid, 0 };
    g_rp_entry = ""; report_event(ev, 1, NULL); tick(100); tick(100);
    CHECK(!strcmp(g_rp_entry, "pause"), "tile + popup marker -> entry pause (%s)", g_rp_entry);
    press_btn(RPP_B_ROW1); press_btn(RPP_B_REASON1); press_btn(RPP_B_SUBMIT); tick(100); tick(100);
    CHECK(g_posts == posts + 1 && strstr(g_post_body, ",\"entry\":\"pause\",\"platform\":\"" RP_PLATFORM "\"}") != NULL, "pause body %s", g_post_body);
    CHECK(strstr(g_post_body, "\"match_type\":\"Echo_Arena\",\"user_id\":\"4242\",\"entry\"") != NULL, "existing fields keep their order: %s", g_post_body);
    press_btn(RPP_B_OK);
    /* pause tile, no popup marker -> tablet */
    g_popup_marker = 0; g_rp_entry = "";
    ev[0] = csym64("evt_btn_clicked");
    report_event(ev, 1, NULL); tick(100);
    CHECK(!strcmp(g_rp_entry, "tablet"), "tile without marker -> entry tablet (%s)", g_rp_entry);
    rp_reset("test"); g_popup_marker = 1;
    /* a card press */
    g_card_five = 1; g_rp_five_key = 0; g_rp_five = -1; tick(1100);
    uint64_t b5[5] = { g_rp_card5_event, 0, 0xCA4D, g_rp_card5_uid, 0 };
    report_event(b5, 1, NULL); tick(100);
    CHECK(!strcmp(g_rp_entry, "card"), "card press -> entry card (%s)", g_rp_entry);
    rp_reset("test"); g_pause_on = 0;
    /* entry unknown: no "entry" key, platform stays */
    g_rp_entry = ""; g_rp_target_fake = 0;
    snprintf(g_rp_target, sizeof g_rp_target, "X"); snprintf(g_rp_target_id, sizeof g_rp_target_id, "77"); g_rp_reason = 0;
    rp_submit(1);
    CHECK(!strstr(g_rp_post_body, "\"entry\"") && strstr(g_rp_post_body, ",\"platform\":\"" RP_PLATFORM "\"}") != NULL, "unknown entry: %s", g_rp_post_body);
    rp_reset("test");
}

/* ADD FRIEND on the PC card: enabled on each open, again +1 s, on a card event after a reopen, not after a close */
static void test_friend(void)
{
    g_card_five = 1; g_rp_five_key = 0; g_rp_five = -1; g_rp_f4_open = 0; g_rp_f4_closed_at = 0;
    CHECK(g_rp_addfriend == 0, "card_add_friend defaults to 0 (the PC tile flickers under the stock threads)");
    CHECK(report_option("card_add_friend", "2") && g_rp_addfriend == 2 && report_option("card_add_friend", "1") && g_rp_addfriend == 1
          && report_option("card_add_friend", "") && g_rp_addfriend == 0, "card_add_friend parsed (0 / 1 / 2, empty = 0)");
    report_option("card_add_friend", "1");   /* the opt-in path below */
    int c0 = g_b4_calls;
    tick(1100);
    CHECK(g_rp_five == 1 && g_b4_calls == c0 + 1, "card open -> button4 enabled once (%d)", g_b4_calls - c0);
    CHECK(wshown(RP_CARD_T, "friend_label") == 1 && wshown(RP_CARD_T, "friend_highlight") == 0, "friend_label shown, friend_highlight hidden");
    tick(1100);
    CHECK(g_b4_calls == c0 + 2, "re-applied on the next poll (%d)", g_b4_calls - c0);
    tick(1100); tick(1100);
    CHECK(g_b4_calls == c0 + 2, "no more applies while the card stays open (%d)", g_b4_calls - c0);
    uint64_t hover[5] = { csym64("evt_card_hover"), 0, 0xCA4D, 0, 0 };
    report_event(hover, 0, NULL); tick(1100);
    CHECK(g_b4_calls == c0 + 2, "a card event on an open card does nothing (%d)", g_b4_calls - c0);
    uint64_t close[5] = { g_rp_card5_event, 0, 0xCA4D, csym64("close_button"), 0 };
    CHECK(report_event(close, 1, NULL) == 0 && close[0] == g_rp_card5_event, "close_button press is not consumed");
    tick(1100);
    CHECK(g_rp_f4_open == 0, "close_button -> card believed closed");
    report_event(hover, 0, NULL); tick(1100);
    CHECK(g_b4_calls == c0 + 2 && g_rp_f4_open == 0, "an event right after the close is the close, not an open (%d)", g_b4_calls - c0);
    tick(1100);
    report_event(hover, 0, NULL); tick(1100);
    CHECK(g_b4_calls == c0 + 3 && g_rp_f4_open == 1, "a later card event = reopen of the same player: enabled again (%d)", g_b4_calls - c0);
    tick(1100);
    CHECK(g_b4_calls == c0 + 4, "and once more +1 s (%d)", g_b4_calls - c0);
    /* mode 2: every poll */
    report_option("card_add_friend", "2");
    int c1 = g_b4_calls;
    tick(1100); tick(1100);
    CHECK(g_b4_calls == c1 + 2, "card_add_friend=2 applies on every poll (%d)", g_b4_calls - c1);
    /* off */
    report_option("card_add_friend", "0");
    g_rp_five_key = 0; g_rp_five = -1; c1 = g_b4_calls;
    tick(1100);
    CHECK(g_b4_calls == c1, "card_add_friend=0: no engine call");
    /* a new player on the card = a new open; a refused hook leaves the labels alone */
    report_option("card_add_friend", "1");
    g_b4_rc = 0; g_rp_five_key = 0; g_rp_five = -1;
    W[find_w(RP_CARD_T, csym64("friend_label"))].shown = -1;
    tick(1100);
    CHECK(wshown(RP_CARD_T, "friend_label") == -1, "hook not run: friend_label untouched");
    g_b4_rc = 1;
    /* a stock (non-five) canvas: no ADD FRIEND work */
    g_card_five = 0; g_rp_five_key = 0; g_rp_five = -1; c1 = g_b4_calls;
    tick(1100); tick(1100);
    CHECK(g_b4_calls == c1, "stock canvas: no button4 enable");
    g_card_five = 1;
}

static void hook_reset(void) { rp_reset("test (late reply)"); }

/* review fixes: option words, entry reset, late worker reply */
static void test_review(void)
{
    /* rp_opt01 words */
    CHECK(rp_opt01("off", 1) == 0 && rp_opt01("OFF", 1) == 0 && rp_opt01("false", 1) == 0 && rp_opt01("on", 0) == 1
          && rp_opt01("True", 0) == 1 && rp_opt01("0", 1) == 0 && rp_opt01("1", 0) == 1 && rp_opt01("", 1) == 1 && rp_opt01("", 0) == 0
          && rp_opt01("maybe", 1) == 1 && rp_opt01("maybe", 0) == 0, "rp_opt01: on/off/true/false/0/1, unknown keeps the default");
    CHECK(report_option("report_hide_bots", "off") && g_rp_hide_bots == 0 && report_option("report_hide_bots", "on") && g_rp_hide_bots == 1,
          "report_hide_bots=off works");
    /* entry reset */
    g_rp_entry = "card";
    rp_reset("test");
    CHECK(g_rp_entry[0] == 0, "rp_reset clears the entry");
    /* a worker reply that lands after a reset is dropped, and the next job still completes */
    rp_start_job(RP_JOB_HOST);
    g_http_hook = hook_reset;
    report_worker_step();
    g_http_hook = NULL;
    CHECK(g_rp_job == RP_JOB_NONE && g_rp_job_done == 0, "late reply after a reset dropped (job %d done %d)", g_rp_job, g_rp_job_done);
    int gets = g_gets;
    rp_start_job(RP_JOB_HOST);
    report_worker_step();
    CHECK(g_rp_job == RP_JOB_HOST && g_rp_job_done == 1 && g_rp_job_status == 200 && g_gets == gets + 1, "the next job completes");
    rp_reset("test");
}

/* tablet / pause parity: the tablet and the pause popup behave the same except layout. Default report_mute_ghost=0. */
static void card_press(void)   /* the five-tile card's REPORT (button5) */
{
    uint64_t b5[5] = { g_rp_card5_event, 0, 0xCA4D, g_rp_card5_uid, 0 };
    report_event(b5, 1, NULL);
}
static void five_open(void) { g_card_five = 1; g_rp_five_key = 0; g_rp_five = -1; g_card_key++; tick(1100); }
static void strip_entry(char *out, size_t cap, const char *body)   /* the body without its,"entry":"…" member */
{
    snprintf(out, cap, "%s", body);
    char *e = strstr(out, ",\"entry\":\"");
    if (!e) return;
    char *q = strchr(e + 10, '"');
    if (q) memmove(e, q + 1, strlen(q + 1) + 1);
}
static int file_has(const char *path, const char *needle)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    static char buf[8192];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = 0;
    return strstr(buf, needle) != NULL;
}
static void test_parity(void)
{
    rp_reset("test"); g_rpp_page = -1; g_rpp_wait = g_rpp_post = 0;
    report_option("report_mute_ghost", "0");
    char pb[1024], tb[1024], a[1024], b[1024];
    /* rows: same reader, bots hidden on BOTH paths, same order */
    g_sess_n = 4; g_sess_bot[0] = 0; g_sess_bot[1] = 1; g_sess_bot[2] = 0; g_sess_bot[3] = 0;
    uint64_t ev[5] = { csym64("evt_btn_clicked"), 0, 0, g_rp_tile_uid, 0 };
    report_event(ev, 1, NULL); tick(100); tick(100);
    CHECK(g_rpp_page == RPP_LIST && !strcmp(wtext(A("ffa_rp_row1"), "buttontext"), "H1") && !strcmp(wtext(A("ffa_rp_row2"), "buttontext"), "H3")
          && !strcmp(wtext(A("ffa_rp_row3"), "buttontext"), "H4"), "pause rows H1 H3 H4 (bot hidden)");
    /* pause: target H3, reason 2 (cheating); Mute & Ghost hidden and inert */
    press_btn(RPP_B_ROW1 + 1);
    CHECK(!strcmp(wtext(A("ffa_rp_reasons_frame"), "sentence"), "H3 will be reported for violating the Echo VR Code of Conduct:"), "pause sentence");
    CHECK(wshown(A("ffa_rp_mg"), "buttontext") == 0 && wshown(A("ffa_rp_mg"), "check") == 0 && wshown(A("ffa_rp_mg"), "check2") == 0,
          "pause: Mute & Ghost hidden (label, tick)");
    CHECK(W[find_w(A("ffa_rp_mg"), csym64("buttonbg"))].has_show && wshown(A("ffa_rp_mg"), "buttonbg") == 0
          && W[find_w(A("ffa_rp_mg"), csym64("buttonoutline"))].has_show && wshown(A("ffa_rp_mg"), "buttonoutline") == 0
          && W[find_w(A("ffa_rp_mg"), csym64("buttonicon"))].has_show && wshown(A("ffa_rp_mg"), "buttonicon") == 0,
          "pause: Mute & Ghost box hidden (buttonbg, buttonoutline, buttonicon)");
    press_btn(RPP_B_MG);
    CHECK(g_rpp_mg == 0 && wshown(A("ffa_rp_mg"), "check") == 0, "pause: hidden Mute & Ghost press ignored");
    int posts = g_posts;
    press_btn(RPP_B_SUBMIT);
    CHECK(g_posts == posts, "pause: SUBMIT ignored without a reason");
    press_btn(RPP_B_REASON1 + 1); press_btn(RPP_B_SUBMIT); tick(100); tick(100);
    snprintf(pb, sizeof pb, "%s", g_post_body);
    CHECK(strstr(pb, "\"mute_ghost\":false") && strstr(pb, "\"entry\":\"pause\""), "pause body: %s", pb);
    char pdone[96]; snprintf(pdone, sizeof pdone, "%s", wtext(A("ffa_rp_done_frame"), "sentence"));
    press_btn(RPP_B_OK);
    /* card -> p23 preset (D-S2): the same page, keys and SUBMIT rule; the target H3 by the card's xplatformid */
    snprintf(g_xpid, sizeof g_xpid, "DMO-2003");
    g_pause_on = 0;
    five_open();
    card_press(); tick(100); tick(RPP_PRESET_SETTLE_MS + 100);
    CHECK(g_rpp_page == RPP_REASONS && g_rpp_preset == 1
          && !strcmp(wtext(A("ffa_rp_reasons_frame"), "sentence"), "H3 will be reported for violating the Echo VR Code of Conduct:"), "card sentence");
    CHECK(wshown(A("ffa_rp_mg"), "buttontext") == 0 && wshown(A("ffa_rp_mg"), "check") == 0, "card: Mute & Ghost hidden");
    posts = g_posts;
    press_btn(RPP_B_SUBMIT);
    CHECK(g_posts == posts && g_rpp_page == RPP_REASONS, "card: SUBMIT ignored without a reason");
    press_btn(RPP_B_REASON1 + 1); press_btn(RPP_B_SUBMIT); tick(100); tick(100);
    snprintf(tb, sizeof tb, "%s", g_post_body);
    CHECK(strstr(tb, "\"entry\":\"card\"") != NULL, "card body: %s", tb);
    strip_entry(a, sizeof a, pb); strip_entry(b, sizeof b, tb);
    CHECK(!strcmp(a, b), "body identical except entry:\n  pause %s\n  card  %s", a, b);
    CHECK(!strcmp(wtext(A("ffa_rp_done_frame"), "sentence"), pdone), "card done text = pause done text (%s)", pdone);
    press_btn(RPP_B_OK);
    /* fallback F: the same body; its tablet prompt reads the same g_rp_msg string */
    report_option("report_tablet_reasons", "field");
    card_press(); tick(100);
    posts = g_posts;
    press_row(2);
    CHECK(g_posts == posts && g_rp_state == RP_FIELD, "field: SUBMIT ignored without a reason");
    press_row(1); press_row(1); press_row(2); tick(100); tick(100);
    strip_entry(b, sizeof b, g_post_body);
    CHECK(g_posts == posts + 1 && !strcmp(a, b), "field body identical except entry:\n  pause %s\n  field %s", a, b);
    CHECK(!strcmp(pdone, g_rp_msg[RP_MSG_SUBMITTED]) && !strcmp(g_prompt_msg, pdone), "201 text same: pause '%s' tablet '%s'", pdone, g_prompt_msg);
    report_option("report_tablet_reasons", "popup");
    rp_reset("test"); g_pause_on = 0;
    /* one label table: the six keys in Coffey's order; the field row fits "<label> >" */
    static const char *const keys[RP_NREASON] = { "hate_speech", "cheating", "harassment", "inappropriate", "unsportsmanlike", "impersonation" };
    for (int r = 0; r < RP_NREASON; r++)
        CHECK(!strcmp(g_rp_reason_key[r], keys[r]) && g_rp_reason_label[r][0] && strlen(g_rp_reason_row[r]) + 2 <= RP_ROWCHARS, "reason %d = %s", r, keys[r]);
    /* result texts: ONE table, both surfaces */
    static const int sts[4] = { 201, 409, 500, RP_ST_TEST };
    static const char *const want[4] = { "Report submitted. Thank you.", "Player already reported.", "Report not sent.", "Test row: report not sent." };
    for (int i = 0; i < 4; i++) CHECK(!strcmp(g_rp_msg[rp_result_msg(sts[i])], want[i]), "status %d -> %s", sts[i], want[i]);
    /* a long name is cut to one row for fallback F's sentence only (the body keeps the full name) */
    snprintf(g_rp_target, sizeof g_rp_target, "%s", "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
    { char row[64]; snprintf(row, sizeof row, "%s", g_rp_target); rp_row_cut(row);
      CHECK(strlen(row) == 26 && !strncmp(row, g_rp_target, 26), "row cut to 26: %s", row);
      rp_name_row(row, sizeof row, RP_SENTENCE_ROW_FMT);
      CHECK(!strcmp(row, "Report ABCDEFGHIJKLMNOPQR:"), "sentence row cuts the name: %s", row);
      snprintf(row, sizeof row, "%s", "AAAAAAAAAAAAAAAAAAAAAAAAA\xc3\xa9Z"); rp_row_cut(row);   /* 25 + a 2-byte char across the cut */
      CHECK(strlen(row) == 25, "row cut on a UTF-8 boundary: %zu", strlen(row)); }
    g_rp_target[0] = 0;
    snprintf(g_xpid, sizeof g_xpid, "DMO-1003");
    g_card_five = 0; g_rp_five_key = 0; g_rp_five = -1; g_card_key++;
    g_sess_n = 0;
    /* pause tile labels (popup tree recipes, two text lines) = the tablet labels; RP_RECIPE_DIR = the directory of the
     * popup tree's reason_*.json recipes (unset: skipped) */
    const char *rdir = getenv("RP_RECIPE_DIR");
    static const char *const rj[4] = { "reason_harass.json", "reason_inappr.json", "reason_unsport.json", "reason_imperson.json" };
    static const char *const l1[4] = { "\"Harassment\"", "\"Inappropriate\"", "\"Unsportsmanlike\"", "\"Impersonation\"" };
    static const char *const l2[4] = { "\"or bullying\"", "\"behavior\"", "\"conduct\"", "\"of RAD staff\"" };
    if (!rdir || !*rdir) printf("  note: RP_RECIPE_DIR unset: pause tile label check skipped\n");
    for (int i = 0; rdir && *rdir && i < 4; i++) {
        char path[512]; snprintf(path, sizeof path, "%s/%s", rdir, rj[i]);
        char full[64]; snprintf(full, sizeof full, "%.*s %.*s", (int)strlen(l1[i]) - 2, l1[i] + 1, (int)strlen(l2[i]) - 2, l2[i] + 1);
        int h1 = file_has(path, l1[i]), h2 = file_has(path, l2[i]);
        if (h1 < 0) { printf("  note: %s not found : pause label check skipped\n", path); break; }
        CHECK(h1 == 1 && h2 == 1 && !strcmp(full, g_rp_reason_label[2 + i]), "pause tile %s = tablet label %s", rj[i], g_rp_reason_label[2 + i]);
    }
    report_option("report_mute_ghost", "1");
}

/* D-S2 (design items 1-3 and 5-7; item 4 = test_parity): the card's REPORT opens p23 preset, fallback F is one
 * 4-row page, and no tablet path draws a MORE row or a custom list of more than 4 rows */
static void tile_tablet(void)   /* pause REPORT tile into the tablet (no popup marker or report_popup=0), up to the LOBBY tab */
{
    uint64_t ev[5] = { csym64("evt_btn_clicked"), 0, 0, g_rp_tile_uid, 0 };
    report_event(ev, 1, NULL);
    tick(100);
    for (int i = 0; i < 40 && g_rp_state == RP_OPENING; i++) tick(100);
    g_pause_on = 0;   /* the stub's toggle flipped it: the pause close */
}
static void test_s022(void)
{
    report_option("report_mute_ghost", "0");
    rp_reset("test"); g_rpp_page = -1; g_rpp_preset = 0; g_pause_on = 0; g_popup_marker = 1;
    g_more_rows = g_big_lists = 0;
    /* 1. no MORE, ever: the tile -> tablet path, card -> p23, card -> fallback F with every reason; 1, 5, 6, 15 players */
    static const int np[4] = { 1, 5, 6, 15 };
    for (int t = 0; t < 4; t++) {
        char want[16];
        g_sess_n = np[t]; for (int i = 0; i < 16; i++) g_sess_bot[i] = 0;
        snprintf(g_xpid, sizeof g_xpid, "DMO-%d", 2000 + np[t]);
        snprintf(want, sizeof want, "H%d", np[t]);
        int calls = g_list_calls;
        g_nfires = 0;
        g_popup_marker = 0; tile_tablet(); g_popup_marker = 1;
        CHECK(g_list_calls == calls && g_rp_state == RP_IDLE && g_nfires == 3 && g_fires[1] == csym64("evt_debug_arm_computer_open")
              && g_fires[2] == csym64("evt_debug_arm_computer_lobby"), "%d players: tile -> tablet LOBBY tab, no custom list (fires %d)", np[t], g_nfires);
        five_open(); card_press(); tick(100); tick(RPP_PRESET_SETTLE_MS + 100);
        CHECK(g_rpp_page == RPP_REASONS && g_list_calls == calls && !strcmp(g_rp_target, want), "%d players: card -> p23 for %s (%s)", np[t], want, g_rp_target);
        press_btn(RPP_B_CANCEL);
        report_option("report_tablet_reasons", "field");
        card_press(); tick(100);
        for (int r = 0; r <= RP_NREASON; r++) {
            press_row(1);
            CHECK(g_list_n == 4 && rows_fit() && strstr(g_list_rows[1], g_rp_reason_row[r % RP_NREASON]) != NULL, "%d players: field reason %d (%s)", np[t], r, g_list_rows[1]);
        }
        press_row(3);
        CHECK(g_rp_state == RP_IDLE, "%d players: field CANCEL", np[t]);
        report_option("report_tablet_reasons", "popup");
        rp_reset("test");
    }
    CHECK(g_more_rows == 0 && g_big_lists == 0, "no MORE row (%d), no custom list > 4 rows (%d)", g_more_rows, g_big_lists);
    /* 2. card -> preset: one pause toggle, p23 enabled after the settle, p22 never, the sentence names the target, entry card */
    g_sess_n = 3; snprintf(g_xpid, sizeof g_xpid, "DMO-2002");
    five_open();
    int tg = g_toggles, p22 = g_p22_en, p23 = g_p23_en, p1 = g_p1_en;
    card_press(); tick(100);
    CHECK(g_rp_state == RP_PRESET && g_toggles == tg + 1 && g_pause_on && g_p23_en == p23, "card REPORT: pause toggled on, p23 not yet (state %d)", g_rp_state);
    tick(RPP_PRESET_SETTLE_MS + 100);
    CHECK(g_toggles == tg + 1 && g_p23_en == p23 + 1 && g_p22_en == p22 && g_last_page == A("ffa_rp_reasons_page") && g_rpp_preset == 1
          && g_rp_state == RP_IDLE, "p23 enabled once, p22 never (p23 %d p22 %d)", g_p23_en - p23, g_p22_en - p22);
    CHECK(strstr(wtext(A("ffa_rp_reasons_frame"), "sentence"), "H2 will be reported") != NULL && !strcmp(g_rp_entry, "card"), "sentence names H2, entry card");
    /* 3. preset CANCEL and the done page's OK toggle the pause menu off; no p1 */
    press_btn(RPP_B_CANCEL);
    CHECK(g_toggles == tg + 2 && !g_pause_on && g_rpp_page == -1 && g_rpp_preset == 0 && g_rp_state == RP_IDLE && g_p1_en == p1, "preset CANCEL: pause off, no p1");
    card_press(); tick(100); tick(RPP_PRESET_SETTLE_MS + 100);
    press_btn(RPP_B_REASON1 + 3); press_btn(RPP_B_SUBMIT); tick(100); tick(100);
    CHECK(g_last_page == A("ffa_rp_done_page") && strstr(g_post_body, "\"reason\":\"inappropriate\"") && strstr(g_post_body, "\"target_user_id\":\"2002\"")
          && strstr(g_post_body, "\"entry\":\"card\"") && strstr(g_post_body, "\"mute_ghost\":false"), "preset SUBMIT -> p24: %s", g_post_body);
    press_btn(RPP_B_OK);
    CHECK(g_toggles == tg + 4 && !g_pause_on && g_rpp_page == -1 && g_rp_state == RP_IDLE && g_p1_en == p1, "preset OK: pause off, no p1 (toggles %d)", g_toggles - tg);
    g_pause_on = 1; tg = g_toggles;   /* the pause menu already up: p23 at once */
    card_press(); tick(100);
    CHECK(g_toggles == tg && g_rpp_page == RPP_REASONS && g_rpp_preset == 1, "pause up: p23 without a toggle");
    press_btn(RPP_B_CANCEL);
    g_popup_marker = 0; tg = g_toggles;   /* p23 never loads: the toggle undone, fallback F */
    card_press(); tick(100);
    for (int i = 0; i < 40 && g_rp_state == RP_PRESET; i++) tick(100);
    CHECK(g_toggles == tg + 2 && !g_pause_on && g_rp_state == RP_FIELD && g_list_n == 4 && !strcmp(g_list_rows[1], "Select Reason >"),
          "p23 missing: toggle undone, field page (state %d, %s)", g_rp_state, g_list_rows[1]);
    g_popup_marker = 1; rp_reset("test");
    /* 5. report_popup=0: the tile opens the tablet on LOBBY with no custom list; the card takes fallback F */
    report_option("report_popup", "0");
    int calls = g_list_calls;
    g_nfires = 0;
    tile_tablet();
    CHECK(g_list_calls == calls && g_rp_state == RP_IDLE && g_nfires == 3 && g_fires[2] == csym64("evt_debug_arm_computer_lobby"), "report_popup=0: tablet LOBBY, no custom list");
    tg = g_toggles;
    card_press(); tick(100);
    CHECK(g_rp_state == RP_FIELD && g_toggles == tg && g_rpp_page == -1, "report_popup=0: card -> field (state %d)", g_rp_state);
    rp_reset("test");
    report_option("report_popup", "1");
    /* 6. fallback F: 4 rows, the field steps through the 6 keys and wraps, clearyoffset before each show, the press guard */
    report_option("report_tablet_reasons", "field");
    int clr = g_clear_calls, lc = g_list_calls, noclr = g_list_noclear;
    card_press(); tick(100);
    CHECK(g_list_n == 4 && rows_fit() && !strcmp(g_list_rows[0], "Report H2:") && !strcmp(g_list_rows[1], "Select Reason >") && !strcmp(g_list_rows[2], "SUBMIT")
          && !strcmp(g_list_rows[3], "CANCEL") && g_last_transition == csym64("user_list_custom"),
          "field page: %s | %s | %s | %s", g_list_rows[0], g_list_rows[1], g_list_rows[2], g_list_rows[3]);
    for (int r = 0; r < 2 * RP_NREASON; r++) {
        press_row(1);
        char row[64]; snprintf(row, sizeof row, "%s >", g_rp_reason_row[r % RP_NREASON]);
        CHECK(g_rp_gsel == r % RP_NREASON && !strcmp(g_list_rows[1], row), "field step %d -> %s (%s)", r, g_rp_reason_key[r % RP_NREASON], g_list_rows[1]);
    }
    CHECK(g_clear_calls - clr == g_list_calls - lc && g_list_noclear == noclr && g_clear_calls > clr, "clearyoffset precedes each show (%d / %d)", g_clear_calls - clr, g_list_calls - lc);
    int sel = g_rp_gsel;
    g_real_y = 0.12f * 0.15f; press_row(1);
    CHECK(g_rp_gsel == sel, "list scrolled 0.12 pitch: press dropped");
    g_real_y = -0.12f * 0.15f; press_row(1);
    CHECK(g_rp_gsel == sel, "scrolled -0.12 pitch: press dropped");
    g_real_y = 0.05f * 0.15f; press_row(1);
    CHECK(g_rp_gsel == (sel + 1) % RP_NREASON, "0.05 pitch: press kept");
    g_real_ok = 0; g_real_y = 1.0f; press_row(1);
    CHECK(g_rp_gsel == (sel + 2) % RP_NREASON, "REAL read not run: guard off");
    g_real_ok = 1; g_real_y = 0;
    press_row(0);
    CHECK(g_rp_state == RP_FIELD && g_rp_gsel == (sel + 2) % RP_NREASON, "sentence row: no-op");
    press_row(4);
    CHECK(g_rp_state == RP_FIELD, "a fifth-row press: no-op");
    press_row(3);
    CHECK(g_rp_state == RP_IDLE && !strcmp(g_prompt_msg, g_rp_msg[RP_MSG_CANCELLED]), "field CANCEL -> %s", g_prompt_msg);
    report_option("report_mute_ghost", "1");
    card_press(); tick(100);
    CHECK(g_list_n == 4 && !strcmp(g_list_rows[3], "[ ] Mute & Ghost"), "report_mute_ghost=1: the check in place of CANCEL (%s)", g_list_rows[3]);
    rp_reset("test");
    report_option("report_mute_ghost", "0");
    report_option("report_tablet_reasons", "popup");
    /* 7. removed keys parse as ignored; the own row on the card: no popup, "no target" */
    static const char *const gone[] = { "report_grid", "report_reason_ui", "report_list_paging", "report_player_card", "report_player_card_stock",
        "report_player_card_header", "report_player_card_header_actor", "report_player_card_header_element", "report_player_card_glyphs" };
    for (size_t i = 0; i < sizeof gone / sizeof gone[0]; i++) CHECK(report_option(gone[i], "1") && g_rp_treasons == RP_TR_POPUP, "%s: ignored", gone[i]);
    CHECK(report_option("report_tablet_reasons", "FIELD") && g_rp_treasons == RP_TR_FIELD && report_option("report_tablet_reasons", "bogus") && g_rp_treasons == RP_TR_AC
          && report_option("report_tablet_reasons", "field") && report_option("report_tablet_reasons", "") && g_rp_treasons == RP_TR_AC
          && report_option("report_tablet_reasons", "Popup") && g_rp_treasons == RP_TR_POPUP, "report_tablet_reasons ac | popup | field");
    snprintf(g_xpid, sizeof g_xpid, "DMO-4242");   /* the local user's own row */
    five_open();
    tg = g_toggles; p23 = g_p23_en; g_prompt_msg[0] = 0;
    card_press(); tick(100); tick(RPP_PRESET_SETTLE_MS + 100);
    CHECK(g_toggles == tg && g_p23_en == p23 && g_rp_state == RP_IDLE && !strcmp(g_prompt_msg, g_rp_msg[RP_MSG_NO_TARGET]), "own row: no popup, '%s'", g_prompt_msg);
    CHECK(g_more_rows == 0 && g_big_lists == 0 && g_call_on_other == 0, "recorder: no MORE (%d), no > 4 rows (%d), no other call_on (%d)", g_more_rows, g_big_lists, g_call_on_other);
    snprintf(g_xpid, sizeof g_xpid, "DMO-1003");
    g_card_five = 0; g_rp_five_key = 0; g_rp_five = -1; g_card_key++;
    g_sess_n = 0;
    rp_reset("test");
    report_option("report_mute_ghost", "1");
}

/* the p22 list scrolls (5 rows + a peek row), no MORE anywhere, stick / impulse / hover / event steps */
static void open_p22(void) { uint64_t ev[5] = { csym64("evt_btn_clicked"), 0, 0, g_rp_tile_uid, 0 }; report_event(ev, 1, NULL); tick(100); tick(100); }
/* a pointer event, then one tick (the dwell clock starts at the first tick that sees it) */
static void ptr_ev(const char *e, uint64_t h) { uint64_t ev[5] = { csym64(e), 0, h, 0, 0 }; report_event(ev, 0, NULL); tick(0); }
static int any_more_text(void)
{
    for (int i = 0; i < g_nw; i++) if (W[i].has_text && strstr(W[i].text, "MORE")) return 1;
    return 0;
}
static int wrote_actor(uint64_t a)
{
    for (int i = 0; i < g_nw; i++) if (W[i].actor == a && (W[i].has_text || W[i].has_show == 1 || W[i].has_rgb)) return 1;
    return 0;
}
static void test_scroll(void)
{
    int posts = g_posts;
    report_option("report_hide_bots", "1");
    memset(g_sess_bot, 0, sizeof g_sess_bot);
    g_scroll_ok = 1; g_scroll_a = 0;
    /* 1. N = 1, 5, 6, 7, 15: no "MORE" text, no write to ffa_rp_more; the peek row shows iff N > off + 5 */
    static const int ns[5] = { 1, 5, 6, 7, 15 };
    for (int t = 0; t < 5; t++) {
        g_sess_n = ns[t];
        open_p22();
        CHECK(g_rpp_page == RPP_LIST && g_rp_nrows == ns[t] && g_rpp_off == 0, "N=%d: list, off 0 (%d rows)", ns[t], g_rp_nrows);
        CHECK(wshown(A("ffa_rp_row6"), "buttontext") == (ns[t] > 5), "N=%d: peek %s", ns[t], ns[t] > 5 ? "shown" : "hidden");
        if (ns[t] > 5) CHECK(!strcmp(wtext(A("ffa_rp_row6"), "buttontext"), "H6"), "N=%d: peek = H6 (%s)", ns[t], wtext(A("ffa_rp_row6"), "buttontext"));
        scroll_ev(20);
        CHECK(g_rpp_off == (ns[t] > 5 ? ns[t] - 5 : 0) && wshown(A("ffa_rp_row6"), "buttontext") == 0, "N=%d: end off %d, peek hidden", ns[t], g_rpp_off);
        press_btn(RPP_B_LIST_CANCEL);
    }
    CHECK(!any_more_text() && !wrote_actor(A("ffa_rp_more")), "no MORE text, no write to ffa_rp_more");
    /* 2. stick: +1 for 0.5 s = 3 steps, clamp at N - 5, -1 back to 0, dead zone, a one-tick impulse = 1 row */
    g_sess_n = 15;
    open_p22();
    run_amount(1.0f, 500);
    CHECK(g_rpp_off == 3, "+1 for 0.5 s: 3 steps (off %d)", g_rpp_off);
    run_amount(1.0f, 3000);
    CHECK(g_rpp_off == 10, "clamped at N - 5 (off %d)", g_rpp_off);
    run_amount(-1.0f, 3000);
    CHECK(g_rpp_off == 0, "-1 back to 0 (off %d)", g_rpp_off);
    run_amount(0.2f, 1000); run_amount(-0.2f, 1000);
    CHECK(g_rpp_off == 0, "dead zone: no step (off %d)", g_rpp_off);
    run_amount(1.0f, 20); tick(20);
    CHECK(g_rpp_off == 1, "one-tick impulse = 1 row (off %d)", g_rpp_off);
    run_amount(-1.0f, 40); tick(20);
    CHECK(g_rpp_off == 0, "two-tick impulse up = 1 row (off %d)", g_rpp_off);
    report_option("report_scroll_sign", "-1");
    run_amount(1.0f, 500);
    CHECK(g_rpp_off == 0, "report_scroll_sign=-1: +1 scrolls up, clamped at 0 (off %d)", g_rpp_off);
    run_amount(-1.0f, 500);
    CHECK(g_rpp_off == 3, "report_scroll_sign=-1: -1 scrolls down (off %d)", g_rpp_off);
    report_option("report_scroll_sign", "1");
    press_btn(RPP_B_LIST_CANCEL);
    /* 3. after 2 steps: row 1 = name 3, peek = name 8, the p23 sentence names the pick; reopen -> off 0; list SUBMIT inert */
    open_p22();
    scroll_ev(2);
    CHECK(!strcmp(wtext(A("ffa_rp_row1"), "buttontext"), "H3") && !strcmp(wtext(A("ffa_rp_row6"), "buttontext"), "H8"),
          "2 steps: row1 %s, peek %s", wtext(A("ffa_rp_row1"), "buttontext"), wtext(A("ffa_rp_row6"), "buttontext"));
    press_btn(RPP_B_LIST_SUBMIT);
    CHECK(g_rpp_page == RPP_LIST && g_rpp_off == 2 && g_posts == posts, "list SUBMIT: no change, no POST");
    CHECK(wrgb(A("ffa_rp_list_submit"), "text")[0] < 0.6f && wrgb(A("ffa_rp_list_submit"), "Background")[0] < 0.3f, "list SUBMIT dim");
    press_btn(RPP_B_ROW1 + 4);
    CHECK(g_rpp_page == RPP_REASONS && strstr(wtext(A("ffa_rp_reasons_frame"), "sentence"), "H7 will be reported") != NULL,
          "row 5 after 2 steps = H7: %s", wtext(A("ffa_rp_reasons_frame"), "sentence"));
    press_btn(RPP_B_CANCEL);
    open_p22();
    CHECK(g_rpp_off == 0 && !strcmp(wtext(A("ffa_rp_row1"), "buttontext"), "H1"), "reopen: off 0");
    scroll_ev(1);
    press_btn(RPP_PEEK);   /* the peek row is a row too: names[off + 5] */
    CHECK(g_rpp_page == RPP_REASONS && strstr(wtext(A("ffa_rp_reasons_frame"), "sentence"), "H7 will be reported") != NULL, "peek press = H7");
    press_btn(RPP_B_CANCEL);
    /* 4. reader off -> hover: 700 ms on the peek row = one step, then one per 350 ms; the end event stops it; row 1 goes up */
    g_scroll_ok = 0;
    open_p22();
    ptr_ev("delegate_OnStartIntersection", 0x0E06);
    tick(300); tick(300);   /* 640 ms */
    CHECK(g_rpp_off == 0, "hover: no step before 700 ms (off %d)", g_rpp_off);
    tick(60);
    CHECK(g_rpp_off == 1, "hover: first step at 700 ms (off %d)", g_rpp_off);
    tick(330);
    CHECK(g_rpp_off == 2, "hover: next step 350 ms later (off %d)", g_rpp_off);
    ptr_ev("delegate_OnEndIntersection", 0x0E06);
    tick(500); tick(500);
    CHECK(g_rpp_off == 2, "hover: the end event stops it (off %d)", g_rpp_off);
    ptr_ev("bhvr_delegate_OnPointerInteractHover", 0x0E01);
    tick(700);
    CHECK(g_rpp_off == 1, "hover on row 1 scrolls up (off %d)", g_rpp_off);
    ptr_ev("bhvr_delegate_OnPointerInteractHover", 0x0E03);   /* onto another row: stops */
    tick(800);
    CHECK(g_rpp_off == 1, "pointer on another row: no step (off %d)", g_rpp_off);
    /* reader on: auto takes the stick, a dwell does nothing */
    g_scroll_ok = 1;
    ptr_ev("delegate_OnStartIntersection", 0x0E06);
    tick(500); tick(500);
    CHECK(g_rpp_off == 1, "auto + reader on: no hover steps (off %d)", g_rpp_off);
    ptr_ev("delegate_OnEndIntersection", 0x0E06);
    /* report_list_scroll=hover ignores the stick; =off ignores everything */
    CHECK(report_option("report_list_scroll", "hover") && g_rpp_scroll_mode == RPP_SCR_HOVER, "report_list_scroll=hover");
    run_amount(1.0f, 500);
    CHECK(g_rpp_off == 1, "hover mode ignores the stick (off %d)", g_rpp_off);
    report_option("report_list_scroll", "off");
    scroll_ev(2); run_amount(1.0f, 500);
    CHECK(g_rpp_off == 1, "off: no scroll at all (off %d)", g_rpp_off);
    report_option("report_list_scroll", "bogus");
    CHECK(g_rpp_scroll_mode == RPP_SCR_AUTO && report_option("report_scroll_probe", "1") && g_rpp_scroll_probe == 1, "bogus -> auto; probe parsed");
    report_option("report_scroll_probe", "0");
    press_btn(RPP_B_LIST_CANCEL);
    /* 5. report_hide_bots=0 with 3 bots + 5 humans -> 8 rows (peek); =1 -> 5 rows, no peek */
    g_sess_n = 8; for (int k = 0; k < 8; k++) g_sess_bot[k] = k < 3;
    report_option("report_hide_bots", "0");
    open_p22();
    CHECK(g_rp_nrows == 8 && wshown(A("ffa_rp_row6"), "buttontext") == 1 && !strcmp(wtext(A("ffa_rp_row1"), "buttontext"), "BOT1"), "bots shown: 8 rows + peek");
    press_btn(RPP_B_LIST_CANCEL);
    report_option("report_hide_bots", "1");
    open_p22();
    CHECK(g_rp_nrows == 5 && wshown(A("ffa_rp_row6"), "buttontext") == 0 && !strcmp(wtext(A("ffa_rp_row1"), "buttontext"), "H4"), "bots hidden: 5 rows, no peek");
    press_btn(RPP_B_LIST_CANCEL);
    memset(g_sess_bot, 0, sizeof g_sess_bot);
    g_sess_n = 0;
    CHECK(g_posts == posts && g_scroll_calls > 0, "no POST in the scroll tests; the reader was polled (%d)", g_scroll_calls);
}

/* the card's REPORT on the tablet's generic prompt with the w23a layer (report_tablet_reasons=ac, the
 * default). Auto-fallback to popup when unbound; the layer hidden at load; parity with p23 (same body but entry, same
 * keys, SUBMIT ignored before a pick, Mute & Ghost hidden with mute_ghost:false, same result string); every exit
 * restores the layer (hidden) and the stock prims (shown). */
static const uint64_t GPA = 0x8a5dcee389df5b7cull;
static void press_zone(const char *z)
{
    uint64_t ev[5] = { csym64("delegate_OnFullyDepressed"), 0, 0, csym64(z), 0 };
    report_event(ev, 1, NULL);
    tick(300);
}
static void press_footer(void)   /* the stock modal footer zone on state_mgr, actor unresolved: never consumed, CANCEL route */
{
    uint64_t ev[5] = { csym64("delegate_OnFullyDepressed"), 0, 0, 0x5071f3ac46630320ull, 0 };
    CHECK(report_event(ev, 1, NULL) == 0 && ev[0] == csym64("delegate_OnFullyDepressed"), "stock footer press not consumed");
    tick(300);
}
/*: one stock-zone event on the tablet state_mgr actor (ev[2] 0x7AB1); 1 consumed, 0 passed, -1 mixed */
static int sz_event(const char *ev0, uint64_t comp)
{
    uint64_t ev[5] = { csym64(ev0), 0, 0x7AB1ull, comp, 0 };
    int r = report_event(ev, 1, NULL);
    return r == 1 && ev[0] == S_RP_NOOP ? 1 : r == 0 && ev[0] == csym64(ev0) ? 0 : -1;
}
static int x_event(const char *ev0, uint64_t actor);   /* the stock X event (below) */
/* the tablet SUBMIT fill, pinned from each canvas's own tint ops: [0 pc, 1 quest][0 = acr_submit_fill as authored (disabled),
 * 1 = acr_cancel_fill (an enabled SUBMIT wears CANCEL's fill)]. PC = the refitted PC canvas, Quest = the Quest canvas. */
static const float T_FILL[2][2][3] = {
    { { 0.298f, 0.298f, 0.329f }, { 0.408f, 0.378f, 0.385f } },
    { { 0.227f, 0.235f, 0.263f }, { 0.420f, 0.396f, 0.404f } },
};
#define T_PQ (RP_PLATFORM[0] == 'q')
static int fill_is(int on)
{
    const float *g = wrgb(GPA, "acr_submit_fill"), *t = T_FILL[T_PQ][on];
    if (!W[find_w(GPA, csym64("acr_submit_fill"))].has_rgb) return 0;
    for (int k = 0; k < 3; k++) if (g[k] - t[k] > 1e-4f || t[k] - g[k] > 1e-4f) return 0;
    return 1;
}
static int restored(void)
{
    static const char *const st[] = { "header", "messagetext", "cancelbackground", "cancelbox", "canceltext", "ui_ar_tablet_close_button" };
    int ok = wshown(GPA, "acr_root") == 0;
    for (int i = 0; i < 6; i++) ok &= wshown(GPA, st[i]) == 1;
    return ok && !g_rpt_on;
}
static void tr_open(void) { card_press(); tick(100); tick(RPT_CLOSE_MS + 50); tick(RPT_SETTLE_MS + 100); }   /* the card close, then the set-up */
static void test_tr(void)
{
    rp_reset("test"); g_rpp_page = -1; g_rpp_preset = 0; g_pause_on = 0; g_popup_marker = 1;
    report_option("report_mute_ghost", "0");
    report_option("report_tablet_reasons", "");
    CHECK(g_rp_treasons == RP_TR_AC, "report_tablet_reasons defaults to ac");
    g_sess_n = 3; for (int i = 0; i < 16; i++) g_sess_bot[i] = 0;
    snprintf(g_xpid, sizeof g_xpid, "DMO-2002");
    /* 1. not bound (stock canvas): the card takes the popup preset, the layer is never written */
    g_ac_bound = 0; g_rpt_bound = -1; five_open();
    int root_w = W[find_w(GPA, csym64("acr_root"))].has_show;
    card_press(); tick(100); tick(RPP_PRESET_SETTLE_MS + 100);
    CHECK(g_rpp_page == RPP_REASONS && g_rpp_preset == 1 && !g_rpt_on && !root_w && !W[find_w(GPA, csym64("acr_root"))].has_show,
          "unbound: ac -> popup p23 preset, no layer write");
    press_btn(RPP_B_CANCEL);
    /* p23 preset body (target H2, cheating) for the parity check */
    card_press(); tick(100); tick(RPP_PRESET_SETTLE_MS + 100);
    press_btn(RPP_B_REASON1 + 1); press_btn(RPP_B_SUBMIT); tick(100); tick(100);
    char pop[1024]; snprintf(pop, sizeof pop, "%s", g_post_body);
    char pdone[96]; snprintf(pdone, sizeof pdone, "%s", wtext(A("ffa_rp_done_frame"), "sentence"));
    press_btn(RPP_B_OK);
    /* 2. bound: hidden once at load (the 1 s poll), then the card opens the prompt + layer */
    g_ac_bound = 1; five_open();
    CHECK(W[find_w(GPA, csym64("acr_root"))].has_show && wshown(GPA, "acr_root") == 0, "bound: layer hidden at load");
    int tg = g_toggles, p23 = g_p23_en, pc = g_press_comp_n;
    tr_open();
    CHECK(g_press_comp_n == pc + 1 && g_press_comp[0] == RP_CARD && g_press_comp[1] == csym64("delegate_OnFullyDepressed")
          && g_press_comp[2] == csym64("close_button"), "ac: the panel presses the five-tile card's close_button once");
    CHECK(!strcmp(wtext(GPA, "acr_sent_l1"), "") && !strcmp(wtext(GPA, "acr_sent_l2"), ""), "ac: short name = one line, l1 / l2 empty");
    CHECK(g_rpt_on && g_last_transition == csym64("generic_prompt") && g_toggles == tg && g_p23_en == p23 && g_rpp_page == -1,
          "ac: generic_prompt, no pause toggle, no p23");
    CHECK(wshown(GPA, "acr_root") == 1 && wshown(GPA, "header") == 0 && wshown(GPA, "messagetext") == 0 && wshown(GPA, "canceltext") == 0
          && wshown(GPA, "ui_ar_tablet_close_button") == 0, "ac: layer shown, stock header / message / footer / X hidden");
    CHECK(!strcmp(wtext(GPA, "acr_sentence"), "H2 will be reported for violating the Echo VR Code of Conduct:"), "ac sentence: %s", wtext(GPA, "acr_sentence"));
    int nsel = 0; for (int r = 1; r <= 6; r++) { char e[16]; snprintf(e, sizeof e, "acr_r%d_sel", r); nsel += wshown(GPA, e) != 0; }
    CHECK(nsel == 0 && wshown(GPA, "acr_mg") == 0 && wrgb(GPA, "acr_submit_bg")[0] == RPP_DIM[0], "ac: no pick, M&G hidden, SUBMIT dim");
    CHECK(fill_is(0), "ac (%s): no pick, SUBMIT fill = the canvas's disabled fill", RP_PLATFORM);
    /* 3. SUBMIT before a pick, hidden M&G press: ignored */
    int posts = g_posts;
    press_zone("ffa_acr_submit");
    CHECK(g_posts == posts && g_rpt_on && g_rp_state != RP_POSTING, "ac: SUBMIT ignored without a reason");
    press_zone("ffa_acr_mg");
    CHECK(g_rpp_mg == 0, "ac: hidden Mute & Ghost press ignored");
    /* 4. pick cheating: tile 2 teal only, SUBMIT full; SUBMIT -> POST, result on the stock prompt, layer restored */
    press_zone("ffa_acr_reason2"); tick(100);
    nsel = 0; for (int r = 1; r <= 6; r++) { char e[16]; snprintf(e, sizeof e, "acr_r%d_sel", r); nsel += wshown(GPA, e) == 1; }
    CHECK(nsel == 1 && wshown(GPA, "acr_r2_sel") == 1 && wrgb(GPA, "acr_submit_bg")[0] == 1.0f && wrgb(GPA, "acr_submit_text")[0] == 1.0f,
          "ac: reason2 picked, SUBMIT full");
    CHECK(fill_is(1), "ac (%s): reason picked, SUBMIT fill = CANCEL's fill (%.3f %.3f %.3f)", RP_PLATFORM,
          wrgb(GPA, "acr_submit_fill")[0], wrgb(GPA, "acr_submit_fill")[1], wrgb(GPA, "acr_submit_fill")[2]);
    press_zone("ffa_acr_submit"); tick(100); tick(100);
    CHECK(g_posts == posts + 1 && !strcmp(g_post_body, pop), "ac body == p23 preset body:\n  ac    %s\n  popup %s", g_post_body, pop);
    CHECK(strstr(g_post_body, "\"entry\":\"card\"") && strstr(g_post_body, "\"mute_ghost\":false") && strstr(g_post_body, "\"reason\":\"cheating\""),
          "ac body: %s", g_post_body);
    CHECK(restored() && !strcmp(g_prompt_msg, pdone) && g_last_transition == csym64("generic_prompt"),
          "ac SUBMIT: layer restored, result '%s' = popup '%s'", g_prompt_msg, pdone);
    CHECK(fill_is(0), "ac SUBMIT: the closed layer's SUBMIT fill back to disabled");
    /* 4b. the result screens (201 sent, 409 already reported, host down) are the STOCK prompt with the layer off, as Coffey
     * #29 (a stand-in, not a designed screen; "<" is drawn on the report steps only, the X closes the tablet everywhere
     * else): no "<" (its zone is inert: no back step, no card re-open), the corner X is the stock close-tablet once the
     * press-through hold ends, and the prompt's own OK is the acknowledge. */
    {
        static const int rst[3] = { 0, 409, -1 };
        static const char *const rmsg[3] = { "Report submitted. Thank you.", "Player already reported.", "Report not sent." };
        for (int i = 0; i < 3; i++) {
            if (i) {
                tick(1500); g_post_status = rst[i]; tr_open();
                CHECK(fill_is(0), "result %d: reopened panel, SUBMIT fill disabled", rst[i]);
                press_zone("ffa_acr_reason2"); tick(100);
                CHECK(fill_is(1), "result %d: picked, SUBMIT fill = CANCEL's", rst[i]);
                press_zone("ffa_acr_submit"); tick(100); tick(100); g_post_status = 0;
            }
            CHECK(restored() && !g_rpt_on && !strcmp(g_prompt_msg, rmsg[i]) && !strcmp(g_prompt_back, "OK") && g_last_transition == csym64("generic_prompt"),
                  "result %d: stock prompt '%s' / '%s', layer off", rst[i] ? rst[i] : 201, g_prompt_msg, g_prompt_back);
            tick(1100);   /* the press-through hold after the close */
            int bk0 = g_log_back;
            press_zone("ffa_acr_back"); tick(100);
            CHECK(g_log_back == bk0 && g_rpt_bk == RPT_BK_OFF && g_last_transition == csym64("generic_prompt"),
                  "result %d: \"<\" inert (no back step, prompt kept)", rst[i] ? rst[i] : 201);
            CHECK(!g_rpt_x_grace && x_event("delegate_OnPress", 0x7AB3ull) == 0, "result %d: the corner X is the stock close-tablet", rst[i] ? rst[i] : 201);
        }
    }
    /* 5. every other exit restores: CANCEL, "<", the stock footer -> user_list; timeout; marker gone */
    static const char *const ex[3] = { "ffa_acr_cancel", "ffa_acr_back", "footer" };
    for (int i = 0; i < 3; i++) {
        tr_open();
        CHECK(g_rpt_on && wshown(GPA, "acr_root") == 1 && fill_is(0), "%s: layer up, SUBMIT fill disabled", ex[i]);
        press_zone("ffa_acr_reason1"); tick(100);
        CHECK(fill_is(1), "%s: picked, SUBMIT fill = CANCEL's", ex[i]);
        posts = g_posts;
        if (i == 2) press_footer(); else press_zone(ex[i]);
        CHECK(restored() && g_last_transition == csym64("user_list") && g_posts == posts, "%s: restored, back to user_list", ex[i]);
        CHECK(fill_is(0), "%s: SUBMIT fill back to disabled", ex[i]);
    }
    {   /* a canvas without acr_submit_fill (w23a): probed once per load, never written */
        int fi = find_w(GPA, csym64("acr_submit_fill"));
        g_ac_fill = 0; g_rpt_bound = -1; W[fi].has_rgb = 0;
        tr_open(); press_zone("ffa_acr_reason1"); tick(100);
        CHECK(g_rpt_on && !W[fi].has_rgb, "no acr_submit_fill on the canvas: no fill write");
        press_zone("ffa_acr_cancel");
        CHECK(restored() && !W[fi].has_rgb, "no acr_submit_fill: none on close either");
        g_ac_fill = 1; g_rpt_bound = -1;
    }
    tr_open();
    for (int t = 0; t <= RP_TIMEOUT_MS / 1000 + 1 && g_rpt_on; t++) tick(1000);
    CHECK(restored() && g_last_transition == csym64("user_list"), "timeout: restored, back to user_list (M2 b)");
    tr_open();
    g_rp_on = -1;   /* report=auto: the marker decides */
    g_popup_marker = 0; g_card_five = 0; g_card_key++; tick(1100);
    CHECK(restored(), "marker gone: restored");
    g_popup_marker = 1; five_open(); tick(1100);
    g_rp_on = 1;
    /* 6. a zone press with no tablet report: ignored, nothing written */
    posts = g_posts;
    press_zone("ffa_acr_reason3");
    CHECK(!g_rpt_on && g_rpp_reason != 2 && wshown(GPA, "acr_root") == 0 && g_posts == posts, "zone press outside the flow: ignored");
    /* 7. report_mute_ghost=1: the check shows, toggles, and reaches the body */
    report_option("report_mute_ghost", "1");
    tr_open();
    CHECK(wshown(GPA, "acr_mg") == 1 && wshown(GPA, "acr_mg_check") == 0, "mg=1: box shown, unchecked");
    press_zone("ffa_acr_mg"); tick(100);
    CHECK(g_rpp_mg == 1 && wshown(GPA, "acr_mg_check") == 1, "mg=1: checked");
    press_zone("ffa_acr_reason6"); press_zone("ffa_acr_submit"); tick(100); tick(100);
    CHECK(strstr(g_post_body, "\"mute_ghost\":true") && strstr(g_post_body, "\"reason\":\"impersonation\"") && restored(), "mg=1 body: %s", g_post_body);
    /* 8. report_tablet_reasons=popup with the canvas bound: p23 as before, the layer untouched */
    report_option("report_tablet_reasons", "popup");
    card_press(); tick(100); tick(RPP_PRESET_SETTLE_MS + 100);
    CHECK(g_rpp_page == RPP_REASONS && !g_rpt_on && wshown(GPA, "acr_root") == 0, "popup preset: layer untouched");
    press_btn(RPP_B_CANCEL);
    g_ac_bound = 0; g_rpt_bound = -1;
    rp_reset("test"); g_sess_n = 0; snprintf(g_xpid, sizeof g_xpid, "DMO-1003");
}

/* review: M1 the layer hidden whatever the report gates say; M2 the tablet leaving the panel
 * mid-flow restores at once, the lapse goes back to user_list, the set-up waits for the card close, the reader may be
 * off; M3 a late p23 owner and the +500 ms re-enable; m1 a card REPORT while posting; m3 the popup lapse closes and a
 * stale CANCEL still closes. */
static void submit_no_worker(void)   /* SUBMIT taken on the tablet, the POST not yet run by the worker */
{
    uint64_t ev[5] = { csym64("delegate_OnFullyDepressed"), 0, 0, csym64("ffa_acr_submit"), 0 };
    report_event(ev, 1, NULL); g_now += 20; report_tick(NULL, g_now);
}
static void test_review_s022(void)
{
    rp_reset("test"); g_rpp_page = -1; g_rpp_preset = 0; g_pause_on = 0; g_popup_marker = 1;
    report_option("report_mute_ghost", "0");
    report_option("report_tablet_reasons", "");
    g_sess_n = 3; for (int i = 0; i < 16; i++) g_sess_bot[i] = 0;
    snprintf(g_xpid, sizeof g_xpid, "DMO-2002");
    g_ac_bound = 1;
    /* M1: report=0, a failed prologue check (ready 0), auto on a tree without our markers (not live) */
    static const int on_v[3] = { 0, 1, -1 }, ready_v[3] = { 1, 0, 1 };
    static const char *const m1n[3] = { "report=0", "prologue failed", "auto, not live" };
    for (int k = 0; k < 3; k++) {
        g_rp_on = on_v[k]; g_rp_ready = ready_v[k];
        if (k == 2) { g_popup_marker = 0; g_card_five = 0; }
        g_card_key++; g_rpt_bound = -1;
        rp_show(NULL, GPA, csym64("acr_root"), 1);   /* the canvas as an unhidden prompt would draw it */
        tick(300);
        CHECK(wshown(GPA, "acr_root") == 0 && !g_rpt_on, "M1 %s: layer hidden at bind", m1n[k]);
        rp_show(NULL, GPA, csym64("acr_root"), 1);
        tick(1100);
        CHECK(wshown(GPA, "acr_root") == 0, "M1 %s: the hide is re-asserted", m1n[k]);
    }
    g_rp_on = 1; g_rp_ready = 1; g_popup_marker = 1; five_open();
    /* M2 a: the tablet closed mid-flow: restored at once (no 120 s wait, no transition); a stock prompt after it is stock */
    tr_open();
    CHECK(g_rpt_on && g_rpt_seen && wshown(GPA, "acr_root") == 1, "M2: panel up, generic_prompt seen on top");
    uint64_t lt = g_last_transition;
    g_tab_closed = 1; tick(150);
    CHECK(restored() && g_last_transition == lt, "M2a: tablet closed mid-flow: restored at once, no transition");
    g_tab_closed = 0;
    rp_show_prompt(NULL, "Party is full."); tick(1100);
    CHECK(wshown(GPA, "acr_root") == 0 && wshown(GPA, "header") == 1 && wshown(GPA, "messagetext") == 1,
          "M2a: a stock prompt after the close (tablet reopened) stays stock");
    /* M2 a: the tablet moved to another state (a tab, a late card-close transition) */
    tr_open(); g_tab_force = csym64("user_list"); tick(150);
    CHECK(restored(), "M2a: tablet on user_list mid-flow: restored");
    g_tab_force = 0;
    /* M2 c: the prompt set-up waits RPT_CLOSE_MS for the card close's handler */
    g_last_transition = 0;
    card_press(); tick(100);
    CHECK(g_rpt_on && g_last_transition != csym64("generic_prompt") && g_rpt_setup_at, "M2c: set-up waits for the card close");
    tick(RPT_CLOSE_MS + 50); tick(RPT_SETTLE_MS + 100);
    CHECK(g_last_transition == csym64("generic_prompt") && wshown(GPA, "acr_root") == 1, "M2c: then generic_prompt + layer");
    press_zone("ffa_acr_cancel");
    /* M2 c: generic_prompt never on top (the close moved the tablet after our transition): given up after RPT_REACH_MS */
    g_tab_force = csym64("user_actions");
    card_press(); tick(100); tick(RPT_CLOSE_MS + 50); tick(1000);
    CHECK(g_rpt_on, "M2c: generic_prompt not yet on top: tolerated");
    tick(RPT_REACH_MS);
    CHECK(restored(), "M2c: generic_prompt never reached: restored");
    g_tab_force = 0;
    /* M2: the tablet closed while SUBMIT posts: restored, one POST, the result is not pushed onto the closed tablet */
    tr_open(); press_zone("ffa_acr_reason1"); submit_no_worker();
    CHECK(g_rp_state == RP_POSTING && g_rpt_post, "M2: posting");
    g_tab_closed = 1; g_now += 150; report_tick(NULL, g_now);
    CHECK(restored() && !g_rpt_post, "M2: tablet closed while posting: restored");
    int posts = g_posts;
    snprintf(g_prompt_msg, sizeof g_prompt_msg, "-"); tick(100); tick(100);
    CHECK(g_posts == posts + 1 && !strcmp(g_prompt_msg, "-") && g_rp_state == RP_IDLE, "M2: its result logged, not shown");
    g_tab_closed = 0;
    /* reader unavailable: no early restore; the lapse stays the exit and goes back to user_list */
    g_tab_ok = 0; tr_open(); g_tab_closed = 1; tick(500);
    CHECK(g_rpt_on, "reader off: no early restore");
    for (int t = 0; t <= RP_TIMEOUT_MS / 1000 + 1 && g_rpt_on; t++) tick(1000);
    CHECK(restored() && g_last_transition == csym64("user_list"), "reader off: the lapse restores, back to user_list");
    g_tab_ok = 1; g_tab_closed = 0;
    /* m1: a card REPORT press while the tablet's SUBMIT posts is ignored (no second panel, no reset of the POST) */
    tr_open(); press_zone("ffa_acr_reason2"); submit_no_worker();
    int pc = g_press_comp_n; posts = g_posts;
    card_press(); g_now += 20; report_tick(NULL, g_now);
    CHECK(g_rp_state == RP_POSTING && g_rpt_post && g_rpt_on && g_press_comp_n == pc, "m1: card press while posting ignored");
    tick(100); tick(100);
    CHECK(g_posts == posts + 1 && restored() && !strcmp(g_prompt_msg, g_rp_msg[RP_MSG_SUBMITTED]), "m1: one POST, its result shown");
    /* M3: p23's owner loads 2 s after our toggle: the settle counts from its first tick, then one re-enable at +500 ms */
    report_option("report_tablet_reasons", "popup");
    g_pause_on = 0; g_p23_at = g_now + 2000;
    int p23 = g_p23_en, tg = g_toggles;
    card_press(); tick(100);
    CHECK(g_toggles == tg + 1 && g_p23_en == p23, "M3: pause menu toggled, p23 not loaded");
    tick(2000);
    CHECK(g_p23_en == p23 && g_rp_state == RP_PRESET, "M3: owner just loaded (2 s after the toggle): not enabled at once");
    tick(1000);
    CHECK(g_p23_en == p23, "M3: still inside the settle counted from the owner");
    tick(600);
    CHECK(g_p23_en == p23 + 1 && g_rpp_page == RPP_REASONS, "M3: enabled after the settle");
    tick(600);
    CHECK(g_p23_en == p23 + 2 && g_rpp_page == RPP_REASONS, "M3: re-enabled once at +500 ms (a late p1 enable covered)");
    tick(1000);
    CHECK(g_p23_en == p23 + 2, "M3: only once");
    press_btn(RPP_B_CANCEL);
    g_p23_at = 0;
    /* m3: the popup lapse closes the pages (preset: the pause menu toggled off); a CANCEL on a page still drawn closes */
    card_press(); tick(100); tick(RPP_PRESET_SETTLE_MS + 100);
    CHECK(g_rpp_page == RPP_REASONS && g_rpp_preset && g_pause_on, "m3: preset p23 up");
    tg = g_toggles;
    for (int t = 0; t <= RP_TIMEOUT_MS / 1000 + 1 && g_rpp_page >= 0; t++) tick(1000);
    CHECK(g_rpp_page == -1 && g_toggles == tg + 1 && !g_pause_on, "m3: lapse closes, pause menu toggled off");
    int p1 = g_p1_en;
    press_btn(RPP_B_CANCEL);
    CHECK(g_p1_en == p1 + 1 && g_rpp_page == -1, "m3: CANCEL on a stale page closes it (p1 enabled)");
    press_btn(RPP_B_REASON1);
    CHECK(g_rpp_page == -1 && g_rpp_reason != 0, "m3: other stale presses stay ignored");
    g_ac_bound = 0; g_rpt_bound = -1;
    report_option("report_tablet_reasons", "");
    rp_reset("test"); g_sess_n = 0; snprintf(g_xpid, sizeof g_xpid, "DMO-1003");
}

/* w23b: the tablet sentence fits the panel for every name the card header can show (31 bytes) */
static void tr_sent_case_p(const char *name, int lines, const char *want_name, const char *pause_name)
{
    char one[2 * RP_NAMELEN + 64], l1[RP_HDRLEN + 40], l2[40], join[256], want[256];
    snprintf(g_rp_target, sizeof g_rp_target, "%s", name);
    int n = rpt_sentence(one, sizeof one, l1, l2, sizeof l1);
    snprintf(want, sizeof want, RP_SENTENCE_FMT, want_name);
    if (n == 1) snprintf(join, sizeof join, "%s", one); else snprintf(join, sizeof join, "%s %s", l1, l2);
    CHECK(n == lines, "sentence '%s': %d lines (want %d)", name, n, lines);
    CHECK(!strcmp(join, want), "sentence '%s' = the expected words: '%s'", name, join);
    int pg = g_rpp_page, so = g_rpp_sent_ok[RPP_REASONS];   /* m2: the pause p23 text as rpp_apply writes it */
    g_rpp_page = RPP_REASONS; g_rpp_sent_ok[RPP_REASONS] = 1; rpp_apply(NULL);
    char pw[256];   /* the pause line: the same words; a name over RPP_SENT_NAME_CP code points ends in "..." (one text primitive) */
    snprintf(pw, sizeof pw, RP_SENTENCE_FMT, pause_name ? pause_name : want_name);
    CHECK(!strcmp(pw, wtext(A("ffa_rp_reasons_frame"), "sentence")), "sentence '%s' = rpp_apply's pause text: '%s' / '%s'",
          name, pw, wtext(A("ffa_rp_reasons_frame"), "sentence"));
    CHECK(rp_u8len(wtext(A("ffa_rp_reasons_frame"), "sentence")) <= 60 + RPP_SENT_NAME_CP, "pause sentence '%s' fits one line: %zu code points",
          name, rp_u8len(wtext(A("ffa_rp_reasons_frame"), "sentence")));
    g_rpp_page = pg; g_rpp_sent_ok[RPP_REASONS] = so;
    CHECK(rp_u8len(one) <= RPT_LINE_CHARS && rp_u8len(l1) <= RPT_LINE_CHARS && rp_u8len(l2) <= RPT_LINE_CHARS
          && strlen(l1) < sizeof l1 && (n == 1 ? !l1[0] && !l2[0] : !one[0]), "sentence '%s' fits: %zu / %zu / %zu code points",
          name, rp_u8len(one), rp_u8len(l1), rp_u8len(l2));
}
static void tr_sent_case(const char *name, int lines, const char *want_name) { tr_sent_case_p(name, lines, want_name, NULL); }
static void test_tr_sentence(void)
{
    char save[RP_NAMELEN]; snprintf(save, sizeof save, "%s", g_rp_target);
    tr_sent_case("AccountName", 1, "AccountName");
    tr_sent_case("AccountName [DEMO]", 2, "AccountName [DEMO]");                      /* the Quest overflow */
    tr_sent_case_p("ABCDEFGHIJKLMNOPQRSTUVWXYZ01234", 2, "ABCDEFGHIJKLMNOPQRSTUVWXYZ01234", "ABCDEFGHIJKLMNO...");   /* 31 bytes = RP_HDRLEN-1 */
    tr_sent_case_p("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", 2, "ABCDEFGHIJKLMNOPQRSTUVWXYZ01234", "ABCDEFGHIJKLMNO..."); /* cut to 31 */
    tr_sent_case_p("AccountName [DEMO]X", 2, "AccountName [DEMO]X", "AccountName [DE...");   /* 19 code points: the first cut */
    tr_sent_case_p("Abcdefghijklmn  Zed", 2, "Abcdefghijklmn  Zed", "Abcdefghijklmn...");     /* no space before the dots */
    /* 20 x U+00E9 (40 bytes) -> 15 whole characters (30 bytes), never half a sequence */
    char e[64] = "", w[64] = ""; for (int i = 0; i < 20; i++) strcat(e, "\xc3\xa9"); for (int i = 0; i < 15; i++) strcat(w, "\xc3\xa9");
    tr_sent_case(e, 2, w);
    { /* 19 x U+00E9: the 31-byte cut leaves 15 code points first, so the pause needs no dots */
      char e2[64] = "", t15[64] = ""; for (int i = 0; i < 19; i++) strcat(e2, "\xc3\xa9"); for (int i = 0; i < 15; i++) strcat(t15, "\xc3\xa9");
      tr_sent_case_p(e2, 2, t15, t15);
      char a[96] = "ABCDEFGHIJ", pz[96] = "ABCDEFGHIJ";   /* 10 ASCII + 10 x U+00E9 = 30 bytes, 20 code points: cut after 5 whole e-acutes */
      for (int i = 0; i < 10; i++) strcat(a, "\xc3\xa9");
      for (int i = 0; i < 5; i++) strcat(pz, "\xc3\xa9");
      strcat(pz, "...");
      tr_sent_case_p(a, 2, a, pz); }
    tr_sent_case("Bad\x01Name\x1f", 1, "BadName");   /* m2: control characters dropped on every surface */
    tr_sent_case("", 1, "PLAYER");
    /* through the surface: a 31-byte name writes l1 / l2 and blanks the one-line element; a w23a canvas keeps one line */
    snprintf(g_rp_target, sizeof g_rp_target, "ABCDEFGHIJKLMNOPQRSTUVWXYZ01234");
    g_rpt_two = 1; rpt_apply(NULL);
    CHECK(!strcmp(wtext(GPA, "acr_sentence"), "") && !strcmp(wtext(GPA, "acr_sent_l1"), "ABCDEFGHIJKLMNOPQRSTUVWXYZ01234 will be reported for violating")
          && !strcmp(wtext(GPA, "acr_sent_l2"), "the Echo VR Code of Conduct:"), "31-byte name: two lines on the layer (%s | %s)",
          wtext(GPA, "acr_sent_l1"), wtext(GPA, "acr_sent_l2"));
    g_rpt_two = 0; rpt_apply(NULL);
    CHECK(!strcmp(wtext(GPA, "acr_sentence"), "ABCDEFGHIJKLMNOPQRSTUVWXYZ01234 will be reported for violating the Echo VR Code of Conduct:"),
          "one-line canvas (no acr_sent_l1): one line as before");
    rp_show(NULL, GPA, csym64("acr_root"), 0);
    snprintf(g_rp_target, sizeof g_rp_target, "%s", save);
}

/* w23e: CANCEL / SUBMIT lowered over the stock menubutton4 / menubutton3 zones. While the tablet surface
 * owns the prompt, every button event of menubutton1-4 on the tablet actor is consumed (no stock route) and logged; an
 * event of another actor / a non-button event / outside the flow passes; the stock footer is consumed once drawn; every
 * exit (CANCEL, SUBMIT, the tablet closed, the lapse) ends the consumption; the tablet M&G label is Coffey's full text. */
static int mb_event(const char *ev0, const char *mb, uint64_t actor)   /* 1 consumed (ev[0] = the no-op), 0 passed */
{
    uint64_t ev[5] = { csym64(ev0), 0, actor, csym64(mb), 0 };
    int r = report_event(ev, 1, NULL);
    return r == 1 && ev[0] == S_RP_NOOP ? 1 : r == 0 && ev[0] == csym64(ev0) ? 0 : -1;
}
static void test_w23e(void)
{
    rp_reset("test"); g_rpp_page = -1; g_rpp_preset = 0; g_pause_on = 0; g_popup_marker = 1;
    report_option("report_mute_ghost", "1");
    report_option("report_tablet_reasons", "");
    g_sess_n = 3; for (int i = 0; i < 16; i++) g_sess_bot[i] = 0;
    snprintf(g_xpid, sizeof g_xpid, "DMO-2002");
    g_ac_bound = 1; g_rpt_bound = -1; five_open();
    const uint64_t TAB = 0x7AB1ull;
    /* outside the flow: the stock menubuttons are untouched */
    CHECK(!g_rpt_on && mb_event("delegate_OnFullyDepressed", "menubutton3", TAB) == 0
          && mb_event("delegate_OnFullyDepressed", "menubutton4", TAB) == 0, "no panel: menubutton3 / 4 pass");
    tr_open();
    CHECK(g_rpt_on && wshown(GPA, "acr_root") == 1, "panel up");
    CHECK(!strcmp(wtext(GPA, "acr_mg_text"), "Mute & Ghost Reported Player"), "tablet M&G label '%s'", wtext(GPA, "acr_mg_text"));
    /* the panel up: every button event of menubutton1-4 on the tablet actor consumed, the panel stays */
    static const char *const evs[6] = { "delegate_OnFullyDepressed", "delegate_OnPressed", "delegate_OnPress",
        "delegate_OnPressStarted", "delegate_OnRelease", "delegate_OnReleased" };
    int ok = 1;
    for (int m = 1; m <= 4; m++)
        for (int e = 0; e < 6; e++) { char b[16]; snprintf(b, sizeof b, "menubutton%d", m); ok &= mb_event(evs[e], b, TAB) == 1; }
    CHECK(ok, "menubutton1-4 x 6 button events consumed on the tablet actor");
    int lc = g_log_consumed;
    tick(300);
    CHECK(g_rpt_on && wshown(GPA, "acr_root") == 1 && g_log_consumed == lc + 1, "consumed presses keep the panel, logged once per tick (%d)",
          g_log_consumed - lc);
    CHECK(mb_event("delegate_OnFullyDepressed", "menubutton3", 0x7AB2ull) == 0, "menubutton3 of another actor passes");
    CHECK(mb_event("bhvr_delegate_OnPointerInteractIdle", "menubutton3", TAB) == 0, "a non-button event of menubutton3 passes");
    CHECK(mb_event("delegate_OnFullyDepressed", "menubutton5", TAB) == 0, "a name outside menubutton1-4 passes");
    /* one touch on SUBMIT = our zone + the stock menubutton3 under it: only ours acts */
    press_zone("ffa_acr_reason4");
    int posts = g_posts;
    CHECK(mb_event("delegate_OnFullyDepressed", "menubutton3", TAB) == 1, "SUBMIT touch: menubutton3 consumed");
    press_zone("ffa_acr_submit"); tick(100); tick(100);
    CHECK(g_posts == posts + 1 && strstr(g_post_body, "\"reason\":\"inappropriate\"") && restored(),
          "SUBMIT over menubutton3 posts once and restores (%s)", g_post_body);
    CHECK(mb_event("delegate_OnFullyDepressed", "menubutton3", TAB) == 0, "after SUBMIT: menubutton3 passes again");
    /* CANCEL over menubutton4: back to user_list, then the stock button passes */
    tr_open();
    CHECK(mb_event("delegate_OnFullyDepressed", "menubutton4", TAB) == 1, "CANCEL touch: menubutton4 consumed");
    press_zone("ffa_acr_cancel");
    CHECK(restored() && g_last_transition == csym64("user_list") && mb_event("delegate_OnFullyDepressed", "menubutton4", TAB) == 1,
          "CANCEL restores; the same press going on into menubutton4 is still consumed (grace)");
    {   /* the closing press's menubutton4 release ends the grace early: a deliberate LOBBY tab press then passes */
        int mg = g_log_mbgrace, gr = g_log_grace_rel;
        tick(40);
        CHECK(g_log_mbgrace == mg, "CANCEL: the same menubutton4 press logs once, not again in the grace (%d)", g_log_mbgrace - mg);
        CHECK(mb_event("delegate_OnRelease", "menubutton4", TAB) == 1, "CANCEL: the press's release still consumed");
        tick(40); tick(RPT_X_GRACE_REL_MS + 10);
        CHECK(!g_rpt_x_grace && g_log_grace_rel == gr + 1 && mb_event("delegate_OnFullyDepressed", "menubutton4", TAB) == 0,
              "CANCEL: the release ended the grace; a new LOBBY tab press passes (%d)", g_log_grace_rel - gr);
    }
    tick(1500);
    CHECK(mb_event("delegate_OnFullyDepressed", "menubutton4", TAB) == 0, "grace lapsed: menubutton4 passes again");
    /* the stock footer between the buttons on the tablet actor: consumed once the layer is drawn  */
    tr_open();
    CHECK(sz_event("delegate_OnFullyDepressed", 0x5071f3ac46630320ull) == 1, "stock footer on the tablet actor consumed");
    tick(300);
    CHECK(g_rpt_on && wshown(GPA, "acr_root") == 1, "stock footer press keeps the panel");
    press_zone("ffa_acr_cancel");
    CHECK(restored() && g_last_transition == csym64("user_list"), "CANCEL after the footer press restores");
    /* the tablet closed mid-flow (M2 a) and the lapse end the consumption at once */
    tr_open();
    g_tab_closed = 1; tick(150);
    CHECK(restored() && mb_event("delegate_OnFullyDepressed", "menubutton3", TAB) == 0, "tablet closed: restored, menubutton3 passes");
    g_tab_closed = 0;
    tr_open();
    for (int t = 0; t <= RP_TIMEOUT_MS / 1000 + 1 && g_rpt_on; t++) tick(1000);
    CHECK(restored() && mb_event("delegate_OnFullyDepressed", "menubutton4", TAB) == 0, "lapse: restored, menubutton4 passes");
    /* a stale consumed flag from before the open is not logged against the new panel */
    tr_open(); tick(300);
    lc = g_log_consumed;   /* one touch seen on both chains a frame apart: one log line */
    mb_event("delegate_OnFullyDepressed", "menubutton3", TAB); g_now += 10; report_tick(NULL, g_now);
    mb_event("delegate_OnFullyDepressed", "menubutton3", TAB); g_now += 10; report_tick(NULL, g_now);
    CHECK(g_log_consumed == lc + 1, "both chains, one log line (%d)", g_log_consumed - lc);
    press_zone("ffa_acr_cancel");
    g_ac_bound = 0; g_rpt_bound = -1;
    rp_reset("test"); g_sess_n = 0; snprintf(g_xpid, sizeof g_xpid, "DMO-1003");
}

/*: real finger presses between CANCEL and SUBMIT (stock footer 0x5071f3ac46630320: popped the prompt)
 * and left of "<" (0x6567539d25efbdc0: closed the tablet) ran stock routes. While the layer is drawn every button event of
 * the stock state_mgr zones under the face is consumed on the tablet actor; the footer stays the bare prompt's exit
 * until the layer is drawn; every exit ends the consumption. */
static void test_s023(void)
{
    rp_reset("test"); g_rpp_page = -1; g_rpp_preset = 0; g_pause_on = 0; g_popup_marker = 1;
    report_option("report_mute_ghost", "0");
    report_option("report_tablet_reasons", "");
    g_sess_n = 3; for (int i = 0; i < 16; i++) g_sess_bot[i] = 0;
    snprintf(g_xpid, sizeof g_xpid, "DMO-2002");
    g_ac_bound = 1; g_rpt_bound = -1; five_open();
    static const char *const evs[6] = { "delegate_OnFullyDepressed", "delegate_OnPressed", "delegate_OnPress",
        "delegate_OnPressStarted", "delegate_OnRelease", "delegate_OnReleased" };
    const uint64_t FOOT = 0x5071f3ac46630320ull, Z6567 = 0x6567539d25efbdc0ull;
    const uint64_t sz[6] = { FOOT, Z6567, csym64("BackButton"), 0x645c2234c519a275ull, 0x645c2234c519a276ull, 0x645c2234c519a277ull };
    /* outside the flow: every stock zone passes */
    int ok = 1;
    for (int z = 0; z < 6; z++) for (int e = 0; e < 6; e++) ok &= sz_event(evs[e], sz[z]) == 0;
    CHECK(!g_rpt_on && ok, "no panel: footer / 0x6567 / BackButton / dots pass");
    /* the set-up window (prompt transition queued, layer not drawn yet): the footer is still the bare prompt's exit */
    card_press(); tick(100); tick(RPT_CLOSE_MS + 50);
    CHECK(g_rpt_on && !g_rpt_drawn && wshown(GPA, "acr_root") != 1, "set-up window, layer not drawn");
    CHECK(sz_event("delegate_OnPressStarted", Z6567) == 1, "set-up window: 0x6567 consumed");
    CHECK(sz_event("delegate_OnFullyDepressed", FOOT) == 0, "set-up window: footer passes (stock CANCEL visible)");
    tick(300);
    CHECK(restored() && g_last_transition == csym64("user_list"), "set-up window footer = CANCEL route, restored");
    /* drawn: all six zones x six button events consumed, the panel stays, one log line */
    tr_open();
    CHECK(g_rpt_on && g_rpt_drawn && wshown(GPA, "acr_root") == 1, "panel drawn");
    ok = 1;
    for (int z = 0; z < 6; z++) for (int e = 0; e < 6; e++) ok &= sz_event(evs[e], sz[z]) == 1;
    CHECK(ok, "footer / 0x6567 / BackButton / dots x 6 button events consumed on the tablet actor");
    uint64_t lt = g_last_transition;
    int lc = g_log_consumed, posts = g_posts;
    tick(300);
    CHECK(g_rpt_on && wshown(GPA, "acr_root") == 1 && g_last_transition == lt && g_posts == posts && g_log_consumed == lc + 1,
          "consumed presses keep the panel, no transition, one log line (%d)", g_log_consumed - lc);
    /* probe A: the finger between CANCEL and SUBMIT = footer OnPressStarted ... OnFullyDepressed: nothing happens */
    lc = g_log_consumed;
    CHECK(sz_event("delegate_OnPressStarted", FOOT) == 1, "swallow A: footer OnPressStarted consumed");
    g_now += 30; report_tick(NULL, g_now);
    CHECK(sz_event("delegate_OnFullyDepressed", FOOT) == 1, "swallow A: footer OnFullyDepressed consumed (not CANCEL)");
    tick(300);
    CHECK(g_rpt_on && wshown(GPA, "acr_root") == 1 && g_last_transition == lt && g_log_consumed >= lc + 1,
          "swallow A: panel stays, 'footer ... press consumed' logged (%d)", g_log_consumed - lc);
    /* probe B: left of "<" = 0x6567 OnPressStarted: consumed, panel stays */
    lc = g_log_consumed;
    CHECK(sz_event("delegate_OnPressStarted", Z6567) == 1, "swallow B: 0x6567 OnPressStarted consumed");
    tick(300);
    CHECK(g_rpt_on && wshown(GPA, "acr_root") == 1 && g_log_consumed == lc + 1, "swallow B: panel stays, logged");
    /* t5: one log line per PRESS (in / hold / hold / out), the swallow applied to every event */
    { tick(1500);
      lc = g_log_consumed;
      CHECK(sz_event("delegate_OnPressStarted", FOOT) == 1, "t5: in consumed");
      tick(40);
      CHECK(sz_event("delegate_OnFullyDepressed", FOOT) == 1, "t5: hold consumed");
      tick(400);
      CHECK(sz_event("delegate_OnFullyDepressed", FOOT) == 1, "t5: hold 2 consumed");
      tick(400);
      CHECK(sz_event("delegate_OnReleased", FOOT) == 1, "t5: out consumed");
      tick(40);
      CHECK(g_log_consumed == lc + 1, "t5: one press, one log line (%d)", g_log_consumed - lc);
      /* a second press: its own line */
      CHECK(sz_event("delegate_OnPressStarted", FOOT) == 1, "t5: press 2 in consumed");
      tick(40);
      CHECK(sz_event("delegate_OnReleased", FOOT) == 1, "t5: press 2 out consumed");
      tick(40);
      CHECK(g_log_consumed == lc + 2, "t5: two presses, two lines (%d)", g_log_consumed - lc);
      /* an in + out inside one tick still logs once, and clears the latch */
      sz_event("delegate_OnPressStarted", FOOT); sz_event("delegate_OnReleased", FOOT); tick(40);
      CHECK(g_log_consumed == lc + 3, "t5: in+out in one tick, one line (%d)", g_log_consumed - lc);
      /* the out event is lost: the next press logs after the gap */
      sz_event("delegate_OnPressStarted", FOOT); tick(40);
      CHECK(g_log_consumed == lc + 4, "t5: lost-out press logged (%d)", g_log_consumed - lc);
      sz_event("delegate_OnPressStarted", FOOT); tick(40);
      CHECK(g_log_consumed == lc + 4, "t5: same press, no out yet, still one line (%d)", g_log_consumed - lc);
      tick(RPT_MB_PRESS_GAP_MS);
      sz_event("delegate_OnPressStarted", FOOT); tick(40);
      CHECK(g_log_consumed == lc + 5, "t5: after a lost out + gap the next press logs (%d)", g_log_consumed - lc); }
    /* other actor / non-button event / unknown zone pass */
    { uint64_t ev[5] = { csym64("delegate_OnPressStarted"), 0, 0x7AB2ull, Z6567, 0 };
      CHECK(report_event(ev, 1, NULL) == 0 && ev[0] == csym64("delegate_OnPressStarted"), "0x6567 of another actor passes"); }
    CHECK(sz_event("bhvr_delegate_OnPointerInteractIdle", Z6567) == 0, "a non-button event of 0x6567 passes");
    CHECK(sz_event("delegate_OnPressStarted", csym64("swiperegion")) == 0, "swiperegion passes");
    /* our zones still act: pick, SUBMIT posts once and restores; then everything passes again */
    press_zone("ffa_acr_reason2");
    CHECK(sz_event("delegate_OnFullyDepressed", FOOT) == 1, "footer still consumed after a pick");
    press_zone("ffa_acr_submit"); tick(100); tick(100);
    CHECK(g_posts == posts + 1 && restored() && !g_rpt_drawn, "SUBMIT posts once and restores");
    ok = 1;
    for (int z = 1; z < 6; z++) ok &= sz_event("delegate_OnFullyDepressed", sz[z]) == 0;
    CHECK(ok && g_rpt_res, "after SUBMIT (result step): every stock zone but the OK passes");
    /* the result prompt's OK (the stock footer) pops one state; rad-menu reached the prompt by a transition (nothing under
     * it), so the OK is consumed and rad-menu lands on the LOBBY list itself (card closed) */
    { int nf = g_nfires, ro = g_log_res_ok;
      CHECK(sz_event("delegate_OnFullyDepressed", FOOT) == 1, "result OK consumed (no stock pop to an empty stack)");
      tick(100);
      CHECK(!g_rpt_res && g_last_transition == csym64("user_list") && g_nfires == nf && !g_rpt_bk && g_log_res_ok == ro + 1,
            "result OK -> user_list, no card re-open");
      CHECK(sz_event("delegate_OnReleased", FOOT) == 1, "result OK: its release still consumed (grace)");
      tick(RPT_X_GRACE_MS + 100); }
    /* "<", the tablet closed and the lapse end the consumption too */
    tr_open(); press_zone("ffa_acr_back");
    CHECK(restored() && sz_event("delegate_OnPressStarted", Z6567) == 1 && sz_event("delegate_OnFullyDepressed", FOOT) == 1,
          "\"<\" restores; 0x6567 / footer under the closing press still consumed (grace)");
    tick(1500);
    CHECK(sz_event("delegate_OnPressStarted", Z6567) == 0 && sz_event("delegate_OnFullyDepressed", FOOT) == 0,
          "grace lapsed: 0x6567 / footer pass");
    tr_open(); g_tab_closed = 1; tick(150);
    CHECK(restored() && sz_event("delegate_OnFullyDepressed", FOOT) == 0, "tablet closed: restored, footer passes");
    g_tab_closed = 0;
    tr_open();
    for (int t = 0; t <= RP_TIMEOUT_MS / 1000 + 1 && g_rpt_on; t++) tick(1000);
    CHECK(restored() && sz_event("delegate_OnPressStarted", Z6567) == 0, "lapse: restored, 0x6567 passes");
    /* a reopen clears the drawn flag: the next set-up window has the footer exit again */
    card_press(); tick(100); tick(RPT_CLOSE_MS + 50);
    CHECK(g_rpt_on && !g_rpt_drawn && sz_event("delegate_OnFullyDepressed", FOOT) == 0, "reopen: drawn cleared");
    tick(300);
    CHECK(restored(), "reopen footer exit restores");
    g_ac_bound = 0; g_rpt_bound = -1;
    rp_reset("test"); g_sess_n = 0; snprintf(g_xpid, sizeof g_xpid, "DMO-1003");
}

/*: the stock tablet X (zone 0x58cba4.. on the tablet component actor) shares the "<" centre: under the drawn layer it is
 * a no-op and the press is the "<" action once; PC order = "<" zone then X, Quest order = X only. */
static int x_event(const char *ev0, uint64_t actor)
{
    uint64_t ev[5] = { csym64(ev0), 0, actor, 0x58cba490dc70b632ull, 0 };
    int r = report_event(ev, 1, NULL);
    return r == 1 && ev[0] == S_RP_NOOP ? 1 : r == 0 && ev[0] == csym64(ev0) ? 0 : -1;
}
static uint64_t now_since_back(void) { return g_now - g_rpt_back_t; }   /* ms since the press that closed the layer */
static void test_s025(void)
{
    static const char *const evs[6] = { "delegate_OnFullyDepressed", "delegate_OnPressed", "delegate_OnPress",
        "delegate_OnPressStarted", "delegate_OnRelease", "delegate_OnReleased" };
    const uint64_t TC = 0x7AB3ull;
    rp_reset("test"); g_rpp_page = -1; g_rpp_preset = 0; g_pause_on = 0; g_popup_marker = 1;
    report_option("report_mute_ghost", "0"); report_option("report_tablet_reasons", "");
    g_sess_n = 3; for (int i = 0; i < 16; i++) g_sess_bot[i] = 0;
    snprintf(g_xpid, sizeof g_xpid, "DMO-2002");
    g_ac_bound = 1; g_rpt_bound = -1; five_open();
    /* layer off: untouched */
    int ok = 1;
    for (int e = 0; e < 6; e++) ok &= x_event(evs[e], TC) == 0;
    CHECK(ok && !g_rpt_on, "no panel: the X passes (closes the tablet)");
    /* set-up window (on, not drawn): untouched */
    card_press(); tick(100); tick(RPT_CLOSE_MS + 50);
    CHECK(g_rpt_on && !g_rpt_drawn, "set-up window");
    ok = 1; for (int e = 0; e < 6; e++) ok &= x_event(evs[e], TC) == 0;
    CHECK(ok, "bare stock prompt (layer not drawn): the X stays stock");
    g_tab_closed = 1; tick(150); g_tab_closed = 0;
    CHECK(restored(), "set-up window ends (tablet closed)");
    /* Quest order: X only */
    tr_open();
    CHECK(g_rpt_on && g_rpt_drawn, "panel drawn");
    ok = 1; for (int e = 0; e < 6; e++) ok &= x_event(evs[e], 0x7AB2ull) == 0 && x_event(evs[e], 0x7AB1ull) == 0;
    CHECK(ok, "X zone on another actor (state_mgr / other): untouched");
    int xl = g_log_xlog, bk = g_log_back;
    ok = 1; for (int e = 0; e < 6; e++) ok &= x_event(evs[e], TC) == 1;
    CHECK(ok, "Quest: X x 6 button events -> all NOOP");
    tick(300);
    CHECK(restored() && g_log_back == bk + 1 && g_log_xlog == xl + 1, "Quest: one \"<\" (%d), one log (%d)", g_log_back - bk, g_log_xlog - xl);
    /* a second press after the release: acts again (a fresh panel) */
    tick(1200);
    tr_open(); bk = g_log_back; xl = g_log_xlog;
    CHECK(x_event("delegate_OnPressStarted", TC) == 1 && x_event("delegate_OnPress", TC) == 1, "press 2 consumed");
    tick(300);
    CHECK(restored() && g_log_back == bk + 1 && g_log_xlog == xl + 1, "second press: second \"<\"");
    /* same panel, release between: a second press acts again; a repeat without release does not */
    tick(1200);
    tr_open(); bk = g_log_back;
    x_event("delegate_OnPressStarted", TC); x_event("delegate_OnPress", TC); tick(300);
    CHECK(g_log_back == bk + 1, "press start + press = one \"<\"");
    /* PC order: "<" zone then the X events of the same press */
    tick(1200);
    tr_open(); bk = g_log_back; xl = g_log_xlog; int xg = g_log_xgrace, gr = g_log_grace_rel;
    { uint64_t ev[5] = { csym64("delegate_OnFullyDepressed"), 0, 0, csym64("ffa_acr_back"), 0 }; report_event(ev, 1, NULL); }
    tick(40);   /* the tick closed the panel (g_rpt_on = 0) before the X events land */
    ok = 1; for (int e = 0; e < 6; e++) ok &= x_event(evs[e], TC) == 1;
    CHECK(ok, "PC: X events after the \"<\" close are still consumed (grace)");
    tick(300);
    CHECK(g_log_back == bk + 1 && g_log_xlog == xl && g_log_xgrace == xg + 1,
          "PC order: exactly one \"<\" (%d), the X logged as grace, not as \"<\" (%d / %d)", g_log_back - bk, g_log_xlog - xl, g_log_xgrace - xg);
    tick(RPT_X_GRACE_REL_MS + 10);
    CHECK(g_log_grace_rel == gr + 1 && !g_rpt_x_grace, "PC order: the X release ended the grace (%d)", g_log_grace_rel - gr);
    /* the grace ends with the closing press: a deliberate X 200 ms after that press's release closes the tablet (stock) */
    tick(1200);
    tr_open(); bk = g_log_back; gr = g_log_grace_rel;
    CHECK(x_event("delegate_OnPressStarted", TC) == 1 && x_event("delegate_OnPress", TC) == 1, "release: X press consumed");
    tick(40);
    CHECK(restored() && g_log_back == bk + 1 && g_rpt_x_grace, "release: \"<\" ran, grace on while the finger is down");
    tick(40);
    CHECK(x_event("delegate_OnFullyDepressed", TC) == 1, "release: the same press (no out event yet) still consumed");
    x_event("delegate_OnRelease", TC); tick(40);
    CHECK(g_rpt_x_grace && x_event("delegate_OnReleased", TC) == 1, "release: OnReleased within RPT_X_GRACE_REL_MS still consumed");
    tick(RPT_X_GRACE_REL_MS + 20); tick(30);
    CHECK(!g_rpt_x_grace && g_log_grace_rel == gr + 1 && now_since_back() < RPT_X_GRACE_MS, "release: grace ended by the release (%d)", g_log_grace_rel - gr);
    CHECK(x_event("delegate_OnPress", TC) == 0, "release: a deliberate X right after passes (stock close)");
    /* no release seen: the grace still lapses by time */
    tick(1200);
    tr_open(); int gt = g_log_grace_time;
    press_zone("ffa_acr_back"); tick(300);
    CHECK(g_rpt_x_grace, "time: no release, the grace holds");
    {   /* a new tab press in the grace is eaten and says so */
        int mg = g_log_mbgrace;
        CHECK(mb_event("delegate_OnPressStarted", "menubutton1", 0x7AB1ull) == 1, "time: menubutton1 in the grace consumed");
        tick(30);
        CHECK(g_log_mbgrace == mg + 1, "time: its log says \"(grace)\" (%d)", g_log_mbgrace - mg);
    }
    tick(RPT_X_GRACE_MS);
    CHECK(!g_rpt_x_grace && g_log_grace_time == gt + 1, "time: grace ended by time (%d)", g_log_grace_time - gt);
    /* PC order, X events before the tick */
    tick(1200);
    tr_open(); bk = g_log_back;
    { uint64_t ev[5] = { csym64("delegate_OnFullyDepressed"), 0, 0, csym64("ffa_acr_back"), 0 }; report_event(ev, 1, NULL); }
    x_event("delegate_OnPressStarted", TC); x_event("delegate_OnPress", TC); tick(300);
    CHECK(g_log_back == bk + 1, "PC order (same tick): exactly one \"<\" (%d)", g_log_back - bk);
    /* the grace lapses: a bare X passes again */
    tick(1500);
    CHECK(x_event("delegate_OnPress", TC) == 0, "grace lapsed, layer off: the X stays stock");
    tick(100);
    g_ac_bound = 0; g_rpt_bound = -1;
    rp_reset("test"); g_sess_n = 0; snprintf(g_xpid, sizeof g_xpid, "DMO-1003");
}

/*: "<" = back ONE step = the report target's card. The layer closes, the tablet returns to user_list, then
 * state_mgr.usernamepressed is called for candidate rows (the matched users[] index first) until the card's xplatformid
 * reads back as the target; a wrong card is closed again; all wrong = give up, list left open. CANCEL = the same step by
 * default, report_tablet_cancel=list = the old landing. */
static void bk_run(void) { for (int i = 0; i < 400 && g_rpt_bk; i++) tick(30); }
static void bk_gap(void) { bk_run(); tick(1200); }   /* the "<" dedupe / X grace lapse between cases */
static void bk_open(void) { un_reset(); snprintf(g_xpid, sizeof g_xpid, "DMO-2002"); tr_open(); }   /* the card shows H2 */
static int bk_last(const char *needle) { return strstr(g_log_bk_last, needle) != NULL; }
static void test_s026(void)
{
    const uint64_t TC = 0x7AB3ull;
    rp_reset("test"); g_rpp_page = -1; g_rpp_preset = 0; g_pause_on = 0; g_popup_marker = 1;
    report_option("report_mute_ghost", "0"); report_option("report_tablet_reasons", ""); report_option("report_tablet_cancel", "");
    CHECK(g_rpt_cancel_card == 1, "back: report_tablet_cancel defaults to card");
    report_option("report_tablet_cancel", "bogus");
    CHECK(g_rpt_cancel_card == 1, "back: report_tablet_cancel=bogus -> card");
    g_sess_n = 3; for (int i = 0; i < 16; i++) g_sess_bot[i] = 0;
    snprintf(g_xpid, sizeof g_xpid, "DMO-2002");
    g_ac_bound = 1; g_rpt_bound = -1; five_open(); un_reset(); tick(1200);

    /* 1. "<": layer closed, user_list, then usernamepressed with the matched index (H2 = users[] 2); readback = target */
    tr_open();
    CHECK(g_rpt_on && g_rpt_drawn && g_rpt_bk_i == 2 && g_rpt_bk_slot == 2, "back 1: panel up, target idx 2 slot 2 remembered");
    snprintf(g_xpid, sizeof g_xpid, "%s", "");   /* the card close left no id: a change proves the open */
    snprintf(g_un_map[2], 32, "DMO-2002");
    int pc = g_press_comp_n, ok = g_log_bk_ok, bk = g_log_back, fl = g_log_bk_fail;
    press_zone("ffa_acr_back");
    CHECK(restored() && g_last_transition == csym64("user_list") && g_log_back == bk + 1, "back 1: layer closed, user_list, one \"<\" line");
    CHECK(g_rpt_bk == RPT_BK_WAIT && g_un_calls == 0, "back 1: the call waits for the transition + %d ms", RPT_BK_LIST_MS);
    bk_run();
    CHECK(g_un_calls == 1 && g_un_idx[0] == 2, "back 1: usernamepressed pressed_user_index=2 once (calls %d, idx %d)", g_un_calls, g_un_idx[0]);
    CHECK(g_log_bk_ok == ok + 1 && g_log_bk_fail == fl && bk_last("tablet \"<\": back -> card of ") && bk_last("(idx 2, tries 1)")
          && !bk_last("unchanged"), "back 1: done: %s", g_log_bk_last);
    CHECK(g_press_comp_n == pc && !g_rpt_on && g_rpt_bk == RPT_BK_OFF && !g_rp_card5_seen[0], "back 1: the right card left open, card poll re-arms");
    bk_gap();

    /* 2. mismatch -> the card closed, the next candidate (row 1; the slot 2 duplicate skipped) -> target */
    bk_open();
    snprintf(g_un_map[2], 32, "DMO-2001"); snprintf(g_un_map[1], 32, "DMO-2002");
    pc = g_press_comp_n; ok = g_log_bk_ok; int wr = g_log_bk_wrong;
    press_zone("ffa_acr_back"); bk_run();
    CHECK(g_un_calls == 2 && g_un_idx[0] == 2 && g_un_idx[1] == 1, "back 2: candidates 2 then 1 (calls %d)", g_un_calls);
    CHECK(g_log_bk_wrong == wr + 1 && g_press_comp_n == pc + 1 && g_press_comp[0] == RP_CARD && g_press_comp[2] == csym64("close_button"),
          "back 2: the wrong card (2001) closed once by its close_button");
    CHECK(g_log_bk_ok == ok + 1 && bk_last("(idx 1, tries 2)") && !strcmp(g_xpid, "DMO-2002"), "back 2: %s", g_log_bk_last);
    bk_gap();

    /* 3. every candidate shows another player: each closed, give up, the LOBBY list left open */
    bk_open();
    for (int r = 0; r < 5; r++) snprintf(g_un_map[r], 32, "DMO-2003");
    pc = g_press_comp_n; ok = g_log_bk_ok; fl = g_log_bk_fail;
    press_zone("ffa_acr_back"); bk_run();
    CHECK(g_un_calls == 5 && g_un_idx[0] == 2 && g_un_idx[1] == 1 && g_un_idx[2] == 3 && g_un_idx[3] == 4 && g_un_idx[4] == 0,
          "back 3: candidates 2, 1, 3, 4, 0 once each (calls %d)", g_un_calls);
    CHECK(g_press_comp_n == pc + 5 && g_log_bk_ok == ok && g_log_bk_fail == fl + 1 && bk_last("not re-opened (no candidate row showed the target: 5 tried")
          && g_last_transition == csym64("user_list") && g_rpt_bk == RPT_BK_OFF, "back 3: 5 closes, give up: %s", g_log_bk_last);
    bk_gap();

    /* 4. the stock X under the layer (Quest order) = "<" = the same back */
    bk_open();
    snprintf(g_xpid, sizeof g_xpid, "%s", ""); snprintf(g_un_map[2], 32, "DMO-2002");
    ok = g_log_bk_ok; bk = g_log_back;
    CHECK(x_event("delegate_OnPressStarted", TC) == 1 && x_event("delegate_OnPress", TC) == 1, "back 4: X consumed");
    tick(300);
    CHECK(restored() && g_last_transition == csym64("user_list") && g_log_back == bk + 1, "back 4: X -> \"<\": layer closed, user_list");
    bk_run();
    CHECK(g_un_calls == 1 && g_un_idx[0] == 2 && g_log_bk_ok == ok + 1 && bk_last("tablet \"<\": back -> card of "), "back 4: X -> card: %s", g_log_bk_last);
    x_event("delegate_OnReleased", TC);
    bk_gap();

    /* 5. CANCEL: the card by default; report_tablet_cancel=list = list, card closed, no call */
    bk_open();
    snprintf(g_xpid, sizeof g_xpid, "%s", ""); snprintf(g_un_map[2], 32, "DMO-2002");
    ok = g_log_bk_ok; int cl = g_log_cancel_list;
    press_zone("ffa_acr_cancel");
    CHECK(restored() && g_last_transition == csym64("user_list"), "back 5: CANCEL: layer closed, user_list");
    bk_run();
    CHECK(g_un_calls == 1 && g_log_bk_ok == ok + 1 && bk_last("tablet CANCEL: back -> card of ") && g_log_cancel_list == cl,
          "back 5: CANCEL -> card: %s", g_log_bk_last);
    bk_gap();
    report_option("report_tablet_cancel", "list");
    CHECK(g_rpt_cancel_card == 0, "back 5: report_tablet_cancel=list parsed");
    bk_open();
    pc = g_press_comp_n; ok = g_log_bk_ok; fl = g_log_bk_fail;
    press_zone("ffa_acr_cancel"); bk_run(); tick(1000);
    CHECK(restored() && g_last_transition == csym64("user_list") && g_un_calls == 0 && g_log_cancel_list == cl + 1
          && g_log_bk_ok == ok && g_log_bk_fail == fl && g_press_comp_n == pc && g_rpt_bk == RPT_BK_OFF,
          "back 5: CANCEL (list): user_list, no usernamepressed, no card press");
    /* "<" stays back with report_tablet_cancel=list */
    tick(600); tr_open(); snprintf(g_xpid, sizeof g_xpid, "%s", ""); snprintf(g_un_map[2], 32, "DMO-2002"); ok = g_log_bk_ok;
    press_zone("ffa_acr_back"); bk_run();
    CHECK(g_un_calls == 1 && g_log_bk_ok == ok + 1, "back 5: \"<\" still re-opens the card with report_tablet_cancel=list");
    report_option("report_tablet_cancel", "");
    bk_gap();

    /* 6. busy: retried on the next ticks, then the call; always busy: give up after %d */
    bk_open();
    snprintf(g_xpid, sizeof g_xpid, "%s", ""); snprintf(g_un_map[2], 32, "DMO-2002"); g_un_busy = 3;
    ok = g_log_bk_ok;
    press_zone("ffa_acr_back"); bk_run();
    CHECK(g_un_busy == 0 && g_un_calls == 1 && g_log_bk_ok == ok + 1 && bk_last("(idx 2, tries 1)"), "back 6: busy x3 then done: %s", g_log_bk_last);
    bk_gap();
    /* the user_list transition itself busy: usernamepressed only after it ran */
    bk_open(); snprintf(g_xpid, sizeof g_xpid, "%s", ""); snprintf(g_un_map[2], 32, "DMO-2002"); ok = g_log_bk_ok;
    { uint64_t ev[5] = { csym64("delegate_OnFullyDepressed"), 0, 0, csym64("ffa_acr_back"), 0 }; report_event(ev, 1, NULL); }
    g_tr_busy = 40; tick(30); bk_run();   /* 40 busy answers outlast RPT_BK_LIST_MS */
    CHECK(g_tr_busy == 0 && g_un_calls == 1 && g_log_bk_ok == ok + 1 && g_last_transition == csym64("user_list"), "back 6: busy transition retried, then the card");
    bk_gap();
    bk_open(); g_un_busy = -1; fl = g_log_bk_fail;
    press_zone("ffa_acr_back"); bk_run();
    CHECK(g_un_calls == 0 && g_log_bk_fail == fl + 1 && bk_last("(state_mgr stayed busy)") && g_rpt_bk == RPT_BK_OFF, "back 6: always busy: %s", g_log_bk_last);
    bk_gap();

    /* 7. the deferred call: the stale target id before it is not taken for an open of a wrong player */
    bk_open();
    snprintf(g_xpid, sizeof g_xpid, "DMO-2002");   /* stale: the card's id from before the close */
    snprintf(g_un_map[2], 32, "DMO-2001"); snprintf(g_un_map[1], 32, "DMO-2002"); g_un_delay = 300;
    ok = g_log_bk_ok; wr = g_log_bk_wrong;
    press_zone("ffa_acr_back"); bk_run();
    CHECK(g_un_calls == 2 && g_log_bk_wrong == wr + 1 && g_log_bk_ok == ok + 1 && bk_last("(idx 1, tries 2)") && !bk_last("unchanged"),
          "back 7: stale target id waited out, wrong card seen, next: %s", g_log_bk_last);
    bk_gap();
    /* 7b. no change at all (stale == target, the call changed nothing): taken after the window, flagged */
    bk_open(); snprintf(g_xpid, sizeof g_xpid, "DMO-2002"); ok = g_log_bk_ok;
    press_zone("ffa_acr_back"); bk_run();
    CHECK(g_un_calls == 1 && g_log_bk_ok == ok + 1 && bk_last("(idx 2, tries 1)") && bk_last("unchanged by the call"), "back 7b: %s", g_log_bk_last);
    bk_gap();

    /* 8. the lapse goes to the list only (not a press) */
    bk_open(); fl = g_log_bk_fail; ok = g_log_bk_ok;
    for (int t = 0; t <= RP_TIMEOUT_MS / 1000 + 1 && g_rpt_on; t++) tick(1000);
    bk_run();
    CHECK(restored() && g_last_transition == csym64("user_list") && g_un_calls == 0 && g_log_bk_ok == ok && g_log_bk_fail == fl,
          "back 8: lapse: user_list, no re-open");

    /* 9. the tablet closed before the call / refused call / unreadable id / close_card=0 / a new panel: no wrong card */
    bk_open(); fl = g_log_bk_fail;
    press_zone("ffa_acr_back"); g_tab_closed = 1; bk_run(); g_tab_closed = 0;
    CHECK(g_un_calls == 0 && g_log_bk_fail == fl + 1 && bk_last("the tablet left the LOBBY list"), "back 9a: %s", g_log_bk_last);
    bk_gap();
    bk_open(); g_un_refuse = 1; fl = g_log_bk_fail;
    press_zone("ffa_acr_back"); bk_run();
    CHECK(g_un_calls == 0 && g_log_bk_fail == fl + 1 && bk_last("usernamepressed refused"), "back 9b: %s", g_log_bk_last);
    bk_gap();
    bk_open(); fl = g_log_bk_fail;
    press_zone("ffa_acr_back"); g_xpid_unread = 1; pc = g_press_comp_n; bk_run(); g_xpid_unread = 0;
    CHECK(g_un_calls == 1 && g_log_bk_fail == fl + 1 && bk_last("card xplatformid unreadable") && g_press_comp_n == pc + 1,
          "back 9c: unreadable -> the card closed again: %s", g_log_bk_last);
    bk_gap();
    report_option("report_tablet_close_card", "0");
    bk_open(); fl = g_log_bk_fail;
    press_zone("ffa_acr_back"); bk_run();
    CHECK(g_un_calls == 0 && g_log_bk_fail == fl + 1 && bk_last("report_tablet_close_card=0"), "back 9d: %s", g_log_bk_last);
    report_option("report_tablet_close_card", "1");
    bk_gap();
    bk_open(); fl = g_log_bk_fail;
    press_zone("ffa_acr_back"); tr_open();
    CHECK(g_rpt_on && g_rpt_bk == RPT_BK_OFF && g_un_calls == 0 && g_log_bk_fail == fl + 1 && bk_last("a new REPORT panel opened"),
          "back 9e: a new panel drops the re-open: %s", g_log_bk_last);
    report_option("report_tablet_cancel", "list"); press_zone("ffa_acr_cancel"); report_option("report_tablet_cancel", "");
    bk_gap();
    CHECK(g_un_early == 0, "back: usernamepressed never called before the user_list transition ran (%d)", g_un_early);
    g_ac_bound = 0; g_rpt_bound = -1; un_reset();
    rp_reset("test"); g_sess_n = 0; snprintf(g_xpid, sizeof g_xpid, "DMO-1003");
}

/*: the PRIMARY back re-open = clear the card id, OpenLobbyMemberSocialControls p_userid=<slot>, read-back == the string
 * recorded at the REPORT press; mismatch / empty -> the candidate walk once. */
static void test_k1798(void)
{
    rp_reset("test"); g_rpp_page = -1; g_rpp_preset = 0; g_pause_on = 0; g_popup_marker = 1;
    report_option("report_mute_ghost", "0"); report_option("report_tablet_reasons", ""); report_option("report_tablet_cancel", "");
    g_sess_n = 3; for (int i = 0; i < 16; i++) g_sess_bot[i] = 0;
    snprintf(g_xpid, sizeof g_xpid, "DMO-2002");
    g_ac_bound = 1; g_rpt_bound = -1; five_open(); un_reset(); od_reset(); tick(1200);
    int ok, fl, df, pc;

    /* 1. direct success: one call p_userid=2, no usernamepressed, the id cleared first */
    tr_open();
    CHECK(!strcmp(g_rpt_bk_xp, "DMO-2002") && g_rpt_bk_slot == 2, "direct 1: id + slot recorded at the press (%s, %d)", g_rpt_bk_xp, g_rpt_bk_slot);
    g_od_mode = 1; snprintf(g_od_map[2], 32, "DMO-2002"); g_od_delay = 150;
    ok = g_log_bk_ok; fl = g_log_bk_fail; df = g_log_dfail;
    press_zone("ffa_acr_back"); bk_run();
    CHECK(g_od_calls == 1 && g_od_slot[0] == 2 && g_un_calls == 0 && g_sv_calls == 1, "direct 1: one direct call slot 2, no usernamepressed, cleared (%d/%d/%d)", g_od_calls, g_un_calls, g_sv_calls);
    CHECK(g_log_bk_ok == ok + 1 && g_log_dfail == df && g_log_bk_fail == fl && bk_last("tablet \"<\": back -> card of ") && bk_last("(slot 2, direct)")
          && !bk_last("not cleared") && g_rpt_bk == RPT_BK_OFF, "direct 1: %s", g_log_bk_last);
    bk_gap();

    /* 2. mismatch: another player's card -> closed once, then the candidate walk finds the target */
    bk_open(); od_reset(); g_od_mode = 1; snprintf(g_od_map[2], 32, "DMO-2001"); snprintf(g_un_map[2], 32, "DMO-2001"); snprintf(g_un_map[1], 32, "DMO-2002");
    tr_open(); ok = g_log_bk_ok; df = g_log_dfail; pc = g_press_comp_n;
    press_zone("ffa_acr_back"); bk_run();
    CHECK(g_od_calls == 1 && g_log_dfail == df + 1 && g_press_comp_n >= pc + 1 && g_press_comp[0] == RP_CARD && g_press_comp[2] == csym64("close_button"),
          "direct 2: wrong card closed, fallback logged");
    CHECK(g_un_calls >= 1 && g_log_bk_ok == ok + 1 && bk_last("(idx 1, tries 2)") && g_od_calls == 1, "direct 2: walk once -> %s", g_log_bk_last);
    bk_gap();

    /* 3. empty (no card opened): the walk, no card close needed */
    bk_open(); od_reset(); g_od_mode = 1; un_reset(); snprintf(g_un_map[2], 32, "DMO-2002");
    tr_open(); ok = g_log_bk_ok; df = g_log_dfail;
    press_zone("ffa_acr_back"); bk_run();
    CHECK(g_od_calls == 1 && g_log_dfail == df + 1 && g_un_calls == 1 && g_log_bk_ok == ok + 1 && bk_last("(idx 2, tries 1)"), "direct 3: empty -> walk: %s", g_log_bk_last);
    bk_gap();

    /* 4. busy x3 then done; 5. always busy gives up (no walk: the card script never answered) */
    bk_open(); od_reset(); g_od_mode = 1; snprintf(g_od_map[2], 32, "DMO-2002"); g_od_busy = 3;
    tr_open(); ok = g_log_bk_ok;
    press_zone("ffa_acr_back"); bk_run();
    CHECK(g_od_busy == 0 && g_od_calls == 1 && g_log_bk_ok == ok + 1 && bk_last("direct"), "direct 4: busy x3 then direct: %s", g_log_bk_last);
    bk_gap();
    bk_open(); od_reset(); g_od_mode = 1; g_od_busy = -1; fl = g_log_bk_fail;
    tr_open(); press_zone("ffa_acr_back"); bk_run();
    CHECK(g_od_calls == 0 && g_un_calls == 0 && g_log_bk_fail == fl + 1 && bk_last("card script stayed busy"), "direct 5: always busy: %s", g_log_bk_last);
    bk_gap();

    /* 6. X under the layer and CANCEL take the same direct path; 7. no setter: no clear, equal read-back accepted with a note */
    bk_open(); od_reset(); g_od_mode = 1; snprintf(g_od_map[2], 32, "DMO-2002");
    tr_open(); ok = g_log_bk_ok;
    press_zone("ffa_acr_cancel"); bk_run();
    CHECK(g_od_calls == 1 && g_un_calls == 0 && g_log_bk_ok == ok + 1 && bk_last("tablet CANCEL: back -> card of ") && bk_last("(slot 2, direct)"), "direct 6: CANCEL direct: %s", g_log_bk_last);
    bk_gap();
    bk_open(); od_reset(); g_od_mode = 1; g_sv_ok = 0; snprintf(g_od_map[2], 32, "DMO-2002");
    tr_open(); ok = g_log_bk_ok;
    press_zone("ffa_acr_back"); bk_run();
    CHECK(g_sv_calls == 0 && g_log_bk_ok == ok + 1 && bk_last("not cleared"), "direct 7: no setter: %s", g_log_bk_last);
    bk_gap();

    /* 8. report_tablet_cancel=list unchanged: no direct call, no card */
    bk_open(); od_reset(); g_od_mode = 1; snprintf(g_od_map[2], 32, "DMO-2002");
    tr_open(); report_option("report_tablet_cancel", "list"); press_zone("ffa_acr_cancel"); report_option("report_tablet_cancel", "");
    bk_run();
    CHECK(g_od_calls == 0 && g_un_calls == 0 && g_rpt_bk == RPT_BK_OFF && g_last_transition == csym64("user_list"), "direct 8: cancel=list leaves the list, no re-open");
    bk_gap();

    /* 9. the Quest shape (: target idx 1, slot 0, the local player last): the direct call passes the SLOT, not the
     * users[] index. Only slot 0 opens the target's card here, so a swapped p->i / p->slot fails both checks. */
    g_sess_quest = 1; g_sess_n = 1; tick(1200);
    un_reset(); od_reset(); snprintf(g_xpid, sizeof g_xpid, "DMO-2001"); g_od_mode = 1; snprintf(g_od_map[0], 32, "DMO-2001");
    tr_open(); ok = g_log_bk_ok; df = g_log_dfail;
    CHECK(g_rpt_on && g_rpt_bk_i == 1 && g_rpt_bk_slot == 0 && !strcmp(g_rpt_bk_xp, "DMO-2001"),
          "direct 9: Quest shape recorded idx %d slot %d (%s)", g_rpt_bk_i, g_rpt_bk_slot, g_rpt_bk_xp);
    press_zone("ffa_acr_back"); bk_run();
    CHECK(g_od_calls == 1 && g_od_slot[0] == 0 && g_un_calls == 0 && g_log_dfail == df && g_log_bk_ok == ok + 1 && bk_last("(slot 0, direct)"),
          "direct 9: p_userid = slot 0 (calls %d, slot %d): %s", g_od_calls, g_od_slot[0], g_log_bk_last);
    bk_gap();
    g_sess_quest = 0; g_sess_n = 3;

    /* 10. the card's xplatformid unreadable at the REPORT press (one other player: the target still resolves). The read
     * is retried before the direct path is armed (card poll or back start); a good id = the direct path, a still-unreadable
     * one or another player's id = the walk. */
    g_sess_n = 1; tick(1200);
    un_reset(); od_reset(); snprintf(g_xpid, sizeof g_xpid, "DMO-2001"); g_od_mode = 1; snprintf(g_od_map[1], 32, "DMO-2001");
    int xu = g_log_xp_unread, xo = g_log_xp_retry_ok, xn = g_log_xp_retry_no;
    g_xpid_unread = 1; card_press(); tick(100); g_xpid_unread = 0; tick(RPT_CLOSE_MS + 50); tick(RPT_SETTLE_MS + 100);
    CHECK(g_rpt_on && g_rpt_bk_slot == 1 && g_log_xp_unread == xu + 1, "direct 10a: unreadable at the press, logged (%d)", g_log_xp_unread - xu);
    ok = g_log_bk_ok; df = g_log_dfail;
    press_zone("ffa_acr_back"); bk_run();
    CHECK(g_log_xp_retry_ok == xo + 1 && !strcmp(g_rpt_bk_xp, "DMO-2001") && g_od_calls == 1 && g_od_slot[0] == 1 && g_un_calls == 0
          && g_log_bk_ok == ok + 1 && bk_last("(slot 1, direct)"), "direct 10a: the retry read the id, direct open: %s", g_log_bk_last);
    bk_gap();
    un_reset(); od_reset(); snprintf(g_xpid, sizeof g_xpid, "DMO-2001"); g_od_mode = 1; snprintf(g_od_map[1], 32, "DMO-2001");
    g_xpid_unread = 1; tr_open(); xn = g_log_xp_retry_no;
    press_zone("ffa_acr_back"); tick(30);
    CHECK(g_log_xp_retry_no == xn + 1 && !g_rpt_bk_xp[0] && g_od_calls == 0, "direct 10b: still unreadable -> no direct call (%d)", g_log_xp_retry_no - xn);
    g_xpid_unread = 0; bk_gap();
    un_reset(); od_reset(); snprintf(g_xpid, sizeof g_xpid, "DMO-2001"); g_od_mode = 1; snprintf(g_od_map[1], 32, "DMO-2001");
    g_xpid_unread = 1; card_press(); tick(100); g_xpid_unread = 0; snprintf(g_xpid, sizeof g_xpid, "DMO-2009");   /* another id */
    tick(RPT_CLOSE_MS + 50); tick(RPT_SETTLE_MS + 100); xn = g_log_xp_retry_no;
    press_zone("ffa_acr_back"); tick(30);
    CHECK(g_log_xp_retry_no == xn + 1 && !g_rpt_bk_xp[0] && g_od_calls == 0, "direct 10c: another player's id at the retry is not used (%d)", g_log_xp_retry_no - xn);
    bk_gap();
    g_sess_n = 3;
    g_ac_bound = 0; g_rpt_bound = -1; un_reset(); od_reset();
    rp_reset("test"); g_sess_n = 0; snprintf(g_xpid, sizeof g_xpid, "DMO-1003");
}

/* stock-off (stock-off-under-layers.md): the stock zones under the drawn REPORT layer are held off in the engine (bit 0x80) and
 * released on every exit (the hold zones RPT_SO_HOLD_MS later); the swallow is the fallback only (checks failed, zone not
 * found, foreign bit, fault). Against the fake pool above. */
static int so_all(int want_bit)   /* every entry of the set has (1) / lacks (0) our bit */
{
    for (int e = 0; e < RPT_SO_N; e++) if (!!(fz_reason_of(e) & RP_SO_BIT) != want_bit) return 0;
    return 1;
}
static int so_owned(void) { int n = 0; for (int e = 0; e < RPT_SO_N; e++) n += g_rpt_so.own[e] != 0; return n; }
static int so_ours_untouched(void)   /* our own layer zones: live while the layer is drawn, held off (bit 0x80 only) otherwise */
{
    uint16_t want = g_rpt_drawn ? 0 : RP_SO_BIT;
    return *fz_rs(fz_idx(GPA_SO, csym64("ffa_acr_back"))) == want && *fz_rs(fz_idx(GPA_SO, csym64("ffa_acr_cancel"))) == want;
}
static int so_nzones(int hold)   /* entries of the def table with this hold flag (hold 0 = the card zones, released at close) */
{
    int n = 0; for (int e = 0; e < RPT_SO_N; e++) n += !!g_rpt_so_def[e].hold == hold; return n;
}
static int so_held_but_card(void)   /* the post-close hold: the card zones (hold 0) released, the rest still off */
{
    for (int e = 0; e < RPT_SO_N; e++)
        if (!!(fz_reason_of(e) & RP_SO_BIT) != !!g_rpt_so_def[e].hold) return 0;
    return 1;
}
static void so_stock_lobby(void)   /* the stock user_list: menubutton1-4 re-enabled (bit 1 cleared only) */
{
    for (int e = 0; e < 4; e++) *fz_rs(fz_so(e)) &= (uint16_t)~1u;
}
static void so_stock_report(void)  /* back to the device's REPORT view stock bits for the next case */
{
    for (int e = 0; e < RPT_SO_N; e++) {
        uint16_t *r = fz_rs(fz_so(e));
        *r = (uint16_t)((*r & RP_SO_BIT) | (e == RPT_SO_X || !g_rpt_so_def[e].hold || e == RPT_MB_FOOTER ? 0 : 1));
    }
}
static void so_gap(void) { tick(1200); so_stock_report(); }
static void test_stockoff(void)
{
    const uint64_t TC = 0x7AB3ull;
    rp_reset("test"); g_rpp_page = -1; g_rpp_preset = 0; g_pause_on = 0; g_popup_marker = 1;
    report_option("report_mute_ghost", "0"); report_option("report_tablet_reasons", ""); report_option("report_tablet_cancel", "");
    g_sess_n = 3; for (int i = 0; i < 16; i++) g_sess_bot[i] = 0;
    snprintf(g_xpid, sizeof g_xpid, "DMO-2002");
    g_ac_bound = 1; g_rpt_bound = -1; five_open();
    fz_init();
    /* 0. the set: 18 zones, 0..9 = the swallow list in order, then swiperegion, the X, the card's 6 zones (button5 first) */
    int ok = RPT_SO_N == 18 && so_nzones(1) == 12 && so_nzones(0) == 6 && RPT_SO_N <= RPT_SO_MAX;
    for (int m = 0; m < RPT_NMB; m++) ok &= g_rpt_so.sym[m] == S_RPT_MB[m] && g_rpt_so_def[m].actor == RP_TABLET;
    CHECK(ok, "stock-off: set entries 0..9 = the swallow list");
    CHECK(g_rpt_so.sym[RPT_SO_X] == RP_TABLET_X && g_rpt_so_def[RPT_SO_X].actor == RP_TABLET_COMP && rpt_so_zone(&g_rpt_so, RPT_SO_B5) == 0x2fd59a8b4a7b59dfull
          && g_rpt_so_def[RPT_SO_B5].actor == RP_CARD && !g_rpt_so_def[RPT_SO_B5].hold && g_rpt_so.sym[RPT_SO_SWIPE] == 0x556819fa696d48c4ull,
          "stock-off: X / button5 (no hold) / swiperegion entries");
    /* rp_zone_check: the layout read back; a handle of another zone is refused */
    { uint16_t r = 0xffff; int i = fz_so(RPT_SO_X), j = fz_so(0);
      CHECK(rp_zone_check(fz_rd, (uintptr_t)g_fz_cs, g_fz_h[i], RP_TABLET_COMP, RP_TABLET_X, &r) == 1 && r == 0, "rp_zone_check: X found, reasons 0");
      CHECK(rp_zone_check(fz_rd, (uintptr_t)g_fz_cs, g_fz_h[j], RP_TABLET_COMP, RP_TABLET_X, &r) == 0, "rp_zone_check: another zone's handle refused");
      CHECK(rp_zone_check(fz_rd, (uintptr_t)g_fz_cs, 0xffffu, RP_TABLET_COMP, RP_TABLET_X, &r) == 0, "rp_zone_check: none handle refused"); }

    /* 1. checks failed -> fallback swallow: no engine call, the X under the layer still runs "<" */
    g_fz_ready = 0; int fb = g_log_so_fb; rpt_so_start();
    CHECK(g_rp_so_engine == 0 && g_log_so_fb == fb + 1 && strstr(g_log_so_last, "fallback swallow (test: Enable / Disable bytes differ)"),
          "stock-off: checks failed -> 'fallback swallow (<why>)' logged at start");
    tr_open();
    CHECK(g_rpt_on && g_rpt_drawn && g_fz_dis == 0 && so_all(0) && so_owned() == 0, "fallback: drawn, no zone touched");
    CHECK(x_event("delegate_OnPressStarted", TC) == 1 && sz_event("delegate_OnPress", csym64("menubutton4")) == 1, "fallback: X / menubutton4 swallowed");
    { int bk = g_log_back; tick(300); CHECK(restored() && g_log_back == bk + 1, "fallback: X press -> one \"<\""); }
    so_gap();

    /* 2. engine: drawn -> all 13 off by bit 0x80, logged once, our own zones untouched */
    g_fz_ready = 1; rpt_so_start();
    CHECK(g_rp_so_engine == 1 && strstr(g_log_so_last, "stock-off: engine (bit 0x80 on 18 zones"), "stock-off: engine path logged at start");
    int off = g_log_so_off;
    card_press(); tick(100); tick(RPT_CLOSE_MS + 50);
    CHECK(g_rpt_on && !g_rpt_drawn && g_fz_dis == 0 && so_all(0), "engine: set-up window: nothing off before the draw");
    tick(RPT_SETTLE_MS + 100);
    CHECK(g_rpt_drawn && so_all(1) && so_owned() == RPT_SO_N && g_fz_dis == RPT_SO_N && so_ours_untouched(), "engine: drawn: %d zones off (%d calls)", RPT_SO_N, g_fz_dis);
    CHECK(g_log_so_off == off + 1 && strstr(g_log_so_last, "18 zones off (engine)"), "engine: '18 zones off (engine)' logged once");
    /* re-asserted every tick while drawn, idempotent: no new call, no new log */
    for (int i = 0; i < 20; i++) tick(150);
    CHECK(g_rpt_on && g_fz_dis == RPT_SO_N && g_log_so_off == off + 1 && so_all(1), "engine: re-assert idempotent (%d calls, %d logs)", g_fz_dis, g_log_so_off - off);
    /* the stock bit 1 does not matter; someone clearing our bit: re-asserted on the next pass */
    *fz_rs(fz_so(RPT_SO_X)) &= (uint16_t)~RP_SO_BIT; tick(150);
    CHECK((fz_reason_of(RPT_SO_X) & RP_SO_BIT) && g_fz_dis == RPT_SO_N + 1, "engine: cleared bit re-asserted");
    /* the hook does not swallow a held zone (the engine sends none): the swallow is the fallback only */
    CHECK(x_event("delegate_OnPress", TC) == 0 && sz_event("delegate_OnPress", csym64("menubutton4")) == 0
          && sz_event("delegate_OnPress", 0x5071f3ac46630320ull) == 0, "engine: held X / menubutton4 / footer not swallowed by the hook");
    /* a re-created component (generation change) while drawn: the old record dropped, the new handle taken */
    { int st = g_log_so_stale, d0 = g_fz_dis; g_fz_h[fz_so(RPT_MB_FOOTER)] += 0x10000u; *fz_rs(fz_so(RPT_MB_FOOTER)) &= (uint16_t)~RP_SO_BIT; tick(150);
      CHECK(g_log_so_stale == st + 1 && g_fz_dis == d0 + 1 && (fz_reason_of(RPT_MB_FOOTER) & RP_SO_BIT) && g_rpt_so.rec[RPT_MB_FOOTER].h == g_fz_h[fz_so(RPT_MB_FOOTER)],
            "engine: generation change while drawn: re-taken (%d)", g_log_so_stale - st); }
    /* 3. "<" spot with the X off: our "<" zone gets the press, the X mapping never runs */
    { int bk = g_log_back, xl = g_log_xlog, rel = g_log_so_rel, held = g_log_so_held, en = g_fz_en;
      char hb[96]; snprintf(hb, sizeof hb, "%d zones released (ffa_acr_back), %d held 1000 ms", so_nzones(0), so_nzones(1));
      press_zone("ffa_acr_back");
      CHECK(!g_rpt_on && g_log_back == bk + 1 && g_log_xlog == xl, "engine: \"<\" spot -> \"<\" (no X mapping)");
      CHECK(g_log_so_rel == rel + 1 && g_log_so_held == held + 1 && strstr(g_log_so_last, hb) && g_fz_en == en + so_nzones(0),
            "engine: close: card zones released now, hold zones held ('%s')", g_log_so_last);
      CHECK(so_held_but_card() && so_owned() == so_nzones(1), "engine: hold: X / menubuttons still off");
      so_stock_lobby();   /* the stock's user_list re-enable during the hold  */
      CHECK((fz_reason_of(3) & RP_SO_BIT) && fz_reason_of(3) != 0, "engine: stock Enable(1) in the hold: menubutton4 still off");
      char hend[64]; snprintf(hend, sizeof hend, "%d zones released (hold end)", so_nzones(1));
      tick(400);
      CHECK(fz_reason_of(3) & RP_SO_BIT, "engine: still held ~0.7 s after the close");
      tick(600);
      CHECK(so_all(0) && so_owned() == 0 && fz_reason_of(3) == 0 && fz_reason_of(RPT_SO_X) == 0 && fz_reason_of(4) == 0 && fz_reason_of(5) == 1,
            "engine: hold end: released to the stock's current state (menubutton4 live, X live, 0x6567 still stock-off)");
      CHECK(g_log_so_rel == rel + 2 && strstr(g_log_so_last, hend) && g_fz_en == en + RPT_SO_N, "engine: hold end logged ('%s')", g_log_so_last); }
    so_gap(); bk_run();
    /* 4. every other exit releases: CANCEL, the bare stock footer, SUBMIT result, lapse, tablet closed, popup marker gone */
    static const char *const ex[6] = { "CANCEL", "footer", "SUBMIT", "lapse", "tablet closed", "marker gone" };
    for (int i = 0; i < 6; i++) {
        tr_open();
        CHECK(g_rpt_drawn && so_all(1), "exit %s: zones off", ex[i]);
        int en = g_fz_en;
        if (i == 0) press_zone("ffa_acr_cancel");
        else if (i == 1) { /* the footer route (its event, actor unresolved: the engine sends none while it is off) */ uint64_t ev[5] = { csym64("delegate_OnFullyDepressed"), 0, 0, 0x5071f3ac46630320ull, 0 }; report_event(ev, 1, NULL); tick(300); }
        else if (i == 2) { press_zone("ffa_acr_reason2"); press_zone("ffa_acr_submit"); tick(100); tick(100); }
        else if (i == 3) { for (int t = 0; t <= RP_TIMEOUT_MS / 1000 + 1 && g_rpt_on; t++) tick(1000); }
        else if (i == 4) { g_tab_closed = 1; tick(150); g_tab_closed = 0; }
        else { g_rp_on = -1; g_popup_marker = 0; g_card_five = 0; g_card_key++; tick(1100); }
        CHECK(!g_rpt_on, "exit %s: closed", ex[i]);
        if (i != 3) CHECK(so_held_but_card() || so_all(0), "exit %s: card zones released at close", ex[i]);
        tick(1100); tick(100);
        CHECK(so_all(0) && so_owned() == 0 && g_fz_en == en + RPT_SO_N && so_ours_untouched(), "exit %s: all %d released (%d)", ex[i], RPT_SO_N, g_fz_en - en);
        if (i == 5) { g_popup_marker = 1; five_open(); tick(1100); g_rp_on = 1; }
        bk_run(); so_gap();
    }
    /* 5. a new panel while drawn: release into the hold, the next draw takes them back (no gap, no new disable) */
    tr_open();
    { int d0 = g_fz_dis; rpt_open(NULL, g_now);
      CHECK(so_held_but_card() && g_rpt_so.holding, "new panel: the old set enters the hold");
      tick(RPT_CLOSE_MS + 50); tick(RPT_SETTLE_MS + 100);
      CHECK(g_rpt_drawn && so_all(1) && !g_rpt_so.holding && g_fz_dis == d0 + so_nzones(0), "new panel: drawn again, hold cancelled (only the card zones re-disabled: %d)", g_fz_dis - d0);
      tick(1500); CHECK(so_all(1), "new panel: no stale hold end while drawn");
      press_zone("ffa_acr_cancel"); tick(1100); CHECK(so_all(0), "new panel: released at its close"); }
    bk_run(); so_gap();
    /* 6. foreign bit: skipped, logged once, keeps the swallow, never cleared by us */
    *fz_rs(fz_so(1)) |= RP_SO_BIT;
    { int fo = g_log_so_foreign; char fb[64]; snprintf(fb, sizeof fb, "%d zones off (engine)", RPT_SO_N - 1);
      tr_open();
      CHECK(!g_rpt_so.own[1] && so_owned() == RPT_SO_N - 1 && strstr(g_log_so_last, fb), "foreign: menubutton2 skipped ('%s')", g_log_so_last);
      CHECK(g_log_so_foreign == fo + 1, "foreign: logged once");
      CHECK(sz_event("delegate_OnPress", csym64("menubutton2")) == 1 && sz_event("delegate_OnPress", csym64("menubutton1")) == 0,
            "foreign: menubutton2 keeps the swallow, menubutton1 (held) does not");
      press_zone("ffa_acr_cancel"); tick(1100); tick(100);
      CHECK((fz_reason_of(1) & RP_SO_BIT) && !(fz_reason_of(0) & RP_SO_BIT), "foreign: the other user's bit left alone at release");
      bk_run(); so_gap(); tr_open();
      CHECK(g_log_so_foreign == fo + 1, "foreign: not logged again");
      press_zone("ffa_acr_cancel"); tick(1100); }
    *fz_rs(fz_so(1)) &= (uint16_t)~RP_SO_BIT;
    bk_run(); so_gap();
    /* 7. a generation change during the hold: the stale record is dropped, no Enable on it */
    tr_open();
    { int st = g_log_so_stale, en = g_fz_en;
      press_zone("ffa_acr_cancel");
      g_fz_h[fz_so(RPT_SO_X)] += 0x10000u;
      tick(1100); tick(100);
      CHECK(g_log_so_stale == st + 1 && g_fz_en == en + RPT_SO_N - 1 && so_owned() == 0, "hold: stale X dropped, %d enabled (%d)", RPT_SO_N - 1, g_fz_en - en);
      *fz_rs(fz_so(RPT_SO_X)) &= (uint16_t)~RP_SO_BIT; }
    bk_run(); so_gap();
    /* 8. card not loaded: the 6 card zones not found -> 12 zones, logged once; a wrong row (verify) = not found too */
    g_fz_card = 0;
    { int mi = g_log_so_missing;
      tr_open();
      CHECK(so_owned() == so_nzones(1) && !g_rpt_so.own[RPT_SO_B5] && g_log_so_missing == mi + so_nzones(0), "card not loaded: card zones skipped, logged once each");
      press_zone("ffa_acr_cancel"); tick(1100); CHECK(so_all(0), "card not loaded: released"); }
    g_fz_card = 1;
    bk_run(); so_gap();
    g_fz_badrow = 1; tr_open();
    CHECK(so_owned() == 0 && so_all(0), "row check fails (handle of another zone): nothing disabled");
    CHECK(x_event("delegate_OnPress", TC) == 1, "row check fails: the X falls back to the swallow");
    tick(300); g_fz_badrow = 0;
    bk_run(); so_gap();
    /* 9. disable not run: that zone falls back */
    g_fz_catfail = 1; tr_open();
    CHECK(so_owned() == 0 && g_rpt_on, "disable not run: all fall back");
    press_zone("ffa_acr_cancel"); g_fz_catfail = 0;
    bk_run(); so_gap();
    /* 10. a fault while drawn: the engine path off, every zone back to the swallow */
    tr_open();
    CHECK(so_owned() == RPT_SO_N, "fault: zones held first");
    fb = g_log_so_fb; g_fz_fault = 1; tick(150);
    CHECK(g_rp_so_engine == 0 && so_owned() == 0 && g_log_so_fb == fb + 1, "fault: engine off, fallback logged");
    CHECK(x_event("delegate_OnPressStarted", TC) == 1, "fault: the X swallow is back");
    tick(300); g_fz_fault = 0;
    bk_run(); so_gap(); fz_init();
    /* back to the default for any later suite: checks failed (the fallback) */
    g_fz_ready = 0; rpt_so_start();
    g_ac_bound = 0; g_rpt_bound = -1;
    rp_reset("test"); g_sess_n = 0; snprintf(g_xpid, sizeof g_xpid, "DMO-1003");
}

/* our own layer zones off while the layer is not drawn (the result prompt's
 * X stays live), the closed card's tiles held off (button5 dropped while closed), the result prompt's OK lands on the
 * LOBBY list, the direct card re-open waits by time AND ticks. Against the fake pool (engine route on). */
static uint16_t *fz_card(const char *z) { return fz_rs(fz_idx(RP_CARD, csym64(z))); }
static int card_tiles(int want_bit)   /* button1..5 all have (1) / lack (0) our bit */
{
    static const char *const t[5] = { "button1", "button2", "button3", "button4", "button5" };
    for (int i = 0; i < 5; i++) if (!!(*fz_card(t[i]) & RP_SO_BIT) != want_bit) return 0;
    return 1;
}
static int rpc_owned(void) { int n = 0; for (int i = 0; i < RPC_SO_N; i++) n += g_rpc_so.rec[i].st != 0; return n; }
static void test_edge_cases_a(void)
{
    const uint64_t TC = 0x7AB3ull, FOOT = 0x5071f3ac46630320ull;
    rp_reset("test"); g_rpp_page = -1; g_rpp_preset = 0; g_pause_on = 0; g_popup_marker = 1;
    report_option("report_mute_ghost", "0"); report_option("report_tablet_reasons", ""); report_option("report_tablet_cancel", "");
    g_sess_n = 3; for (int i = 0; i < 16; i++) g_sess_bot[i] = 0;
    snprintf(g_xpid, sizeof g_xpid, "DMO-2002");
    g_ac_bound = 1; g_rpt_bound = -1; five_open(); un_reset(); od_reset();
    fz_init(); g_fz_ready = 1; rpt_so_start(); tick(300);
    uint16_t *cb = fz_card("close_button"), *back = fz_rs(fz_idx(GPA_SO, csym64("ffa_acr_back")));

    /* 1. no panel: our layer zones off (bit 0x80 only), logged under layer-off, never counted as the REPORT set */
    CHECK(!g_rpt_on && *back == RP_SO_BIT && so_ours_untouched() && g_fz_ldis >= 2 && g_log_lo_off >= 1, "layer zones off with no panel (%04x)", *back);
    /* 2. the card open (close_button live): tiles live */
    CHECK(g_rp_card_open == 1 && card_tiles(0) && rpc_owned() == 0, "card open: tiles live");
    /* 3. the stock closes the card (close_button bit 1): tiles held off; a button5 event is dropped, no panel */
    *cb |= 1; tick(150);
    CHECK(g_rp_card_open == 0 && card_tiles(1) && rpc_owned() == 5 && strstr(g_log_co_last, "5 zones off (engine): closed card"),
          "card closed: button1-5 off ('%s')", g_log_co_last);
    CHECK(!(*cb & RP_SO_BIT), "card closed: close_button itself never held");
    { int cc = g_log_card_closed, posts = g_posts; card_press(); tick(300);
      CHECK(!g_rpt_on && g_log_card_closed == cc + 1 && g_posts == posts, "closed card: button5 event dropped, no REPORT panel"); }
    /* 4. the card opens again: released, the REPORT press opens the panel */
    *cb &= (uint16_t)~1u; tick(150);
    CHECK(g_rp_card_open == 1 && card_tiles(0) && rpc_owned() == 0, "card re-opened: tiles released");
    tr_open(); *cb |= 1;   /* the panel's close_button press: the stock close thread disables it */
    tick(150);
    CHECK(g_rpt_on && g_rpt_drawn && g_rp_card_open == 0 && rpc_owned() == 0 && card_tiles(1), "panel: card tiles held by the REPORT set only");
    CHECK(*back == 0 && so_ours_untouched() && g_fz_len >= 2, "drawn: our layer zones live");
    /* 5. CANCEL: our zones off at once; the closed card's tiles taken over without a gap; the back path re-opens the card */
    od_reset(); g_od_mode = 1; snprintf(g_od_map[2], 32, "DMO-2002"); g_od_delay = 150;
    { uint64_t ev[5] = { csym64("delegate_OnFullyDepressed"), 0, 0, csym64("ffa_acr_cancel"), 0 }; report_event(ev, 1, NULL); g_now += 20; report_tick(NULL, g_now); }
    CHECK(!g_rpt_on && *back == RP_SO_BIT && rpc_owned() == 5 && card_tiles(1), "CANCEL: layer zones off and card tiles held in the same tick");
    { int ok = g_log_bk_ok; bk_run(); *cb &= (uint16_t)~1u; tick(150);
      CHECK(g_log_bk_ok == ok + 1 && g_rp_card_open == 1 && card_tiles(0) && rpc_owned() == 0, "back -> card: tiles live again"); }
    bk_gap();
    /* 6. the result prompt: our zones off (the "<" spot gives the X to the stock), the X passes, OK -> LOBBY list */
    tr_open(); *cb |= 1;
    press_zone("ffa_acr_reason2"); press_zone("ffa_acr_submit"); tick(100); tick(100);
    CHECK(!g_rpt_on && g_rpt_res && !strcmp(g_prompt_back, "OK") && *back == RP_SO_BIT, "result prompt up, layer zones off");
    tick(1100);   /* the hold ends: the stock X / footer live */
    CHECK(!(fz_reason_of(RPT_SO_X) & RP_SO_BIT) && x_event("delegate_OnPressStarted", TC) == 0, "result prompt: the stock X passes (closes the tablet)");
    { uint64_t ev[5] = { csym64("delegate_OnFullyDepressed"), 0, 0, csym64("ffa_acr_back"), 0 };   /* a stray "<" event: inert */
      int on = g_rpt_on; report_event(ev, 1, NULL); tick(100); CHECK(!g_rpt_on && on == 0 && g_rpt_res, "result prompt: a \"<\" event changes nothing"); }
    { int ro = g_log_res_ok;
      CHECK(sz_event("delegate_OnPressStarted", FOOT) == 1, "result OK consumed");
      tick(100);
      CHECK(!g_rpt_res && g_last_transition == csym64("user_list") && g_log_res_ok == ro + 1 && !g_rpt_bk, "result OK -> user_list (card closed)"); }
    tick(RPT_X_GRACE_MS + 100);
    /* 7. the result prompt left by the stock (tablet closed): the OK is no longer ours */
    rp_prompt("x"); tick(100);
    CHECK(g_rpt_res, "result prompt (by rp_prompt) up");
    g_tab_closed = 1; tick(RPT_RES_SETTLE_MS + 100);
    CHECK(!g_rpt_res && sz_event("delegate_OnPressStarted", FOOT) == 0, "result prompt left: footer passes again");
    g_tab_closed = 0; tick(200);
    /* 8. the direct re-open waits by time AND ticks: an id 1.5 s late is still a direct open; empty fails only after 2 s */
    *cb &= (uint16_t)~1u; tick(150);
    bk_open(); *cb |= 1; od_reset(); g_od_mode = 1; snprintf(g_od_map[2], 32, "DMO-2002"); g_od_delay = 1500;
    { int ok = g_log_bk_ok, df = g_log_dfail; press_zone("ffa_acr_back"); bk_run();
      CHECK(g_log_bk_ok == ok + 1 && g_log_dfail == df && bk_last("(slot 2, direct)") && g_un_calls == 0, "direct: 1.5 s late id still direct: %s", g_log_bk_last); }
    *cb &= (uint16_t)~1u; bk_gap();
    bk_open(); *cb |= 1; od_reset(); g_od_mode = 1; un_reset(); snprintf(g_un_map[2], 32, "DMO-2002");
    { int df = g_log_dfail; uint64_t t0 = g_now; press_zone("ffa_acr_back");
      for (int i = 0; i < 400 && g_log_dfail == df; i++) tick(30);
      const char *a = strstr(g_log_df_last, "after ");
      unsigned long ms = a ? strtoul(a + 6, NULL, 10) : 0;
      CHECK(g_log_dfail == df + 1 && ms >= RPT_BK_DIRECT_MS && g_now - t0 >= RPT_BK_DIRECT_MS && strstr(g_log_df_last, "ticks"), "direct: empty judged after >= 2 s: %s", g_log_df_last);
      bk_run(); CHECK(bk_last("(idx 2, tries 1)"), "direct: empty -> walk: %s", g_log_bk_last); }
    { /* a slow client: 2 s pass in a few ticks: still waits for RPT_BK_DIRECT_TICKS */
      *cb &= (uint16_t)~1u; bk_gap();
      bk_open(); *cb |= 1; od_reset(); g_od_mode = 1; un_reset(); snprintf(g_un_map[2], 32, "DMO-2002");
      int df = g_log_dfail; press_zone("ffa_acr_back");
      for (int i = 0; i < 6; i++) { g_now += 500; report_tick(NULL, g_now); }
      CHECK(g_log_dfail == df && g_rpt_bk == RPT_BK_DREAD, "direct: 3 s in 6 ticks: still waiting (frames)");
      for (int i = 0; i < RPT_BK_DIRECT_TICKS && g_log_dfail == df; i++) { g_now += 100; report_tick(NULL, g_now); }
      CHECK(g_log_dfail == df + 1, "direct: judged once the ticks passed"); bk_run(); }
    *cb &= (uint16_t)~1u; bk_gap();
    /* 9. the engine route gone (fault): card state unknown, button5 passes to rad-menu again */
    g_fz_fault = 1; tick(150);
    CHECK(g_rp_so_engine == 0 && g_rp_card_open == -1 && rpc_owned() == 0, "fault: card state unknown, nothing held");
    g_fz_fault = 0;
    fz_init(); g_fz_ready = 0; rpt_so_start(); tick(150);
    g_ac_bound = 0; g_rpt_bound = -1; od_reset(); un_reset();
    rp_reset("test"); g_sess_n = 0; snprintf(g_xpid, sizeof g_xpid, "DMO-1003");
}

/* the edge-case fixes (zones of the REPORT layer with the flow off, the hidden Mute & Ghost, the "<" in fallback
 * mode, a POST still running when the user backs out, the warning card's REPORT tile, the user's row tap in the back walk) */
static int g_inj_ev_b = -1;   /* g_http_hook: a popup button pressed while the POST is "in flight" */
static void inj_popup_press(void) { if (g_inj_ev_b >= 0) { uint64_t ev[5] = { csym64("evt_btn_clicked"), 0, 0, g_rpp_uid[g_inj_ev_b], 0 }; report_event(ev, 1, NULL); g_inj_ev_b = -1; } }
static uint16_t *lz(const char *z) { return fz_rs(fz_idx(GPA_SO, csym64(z))); }
static int layer_all(int want_bit)   /* the 10 layer zones all have (1) / lack (0) our bit */
{
    for (int i = 0; i < RPL_SO_N; i++) if (!!(*fz_rs(fz_idx(GPA_SO, csym64(g_rpl_so_def[i].zone))) & RP_SO_BIT) != want_bit) return 0;
    return 1;
}
static void test_edge_cases_b(void)
{
    rp_reset("test"); g_rpp_page = -1; g_rpp_preset = 0; g_pause_on = 0; g_popup_marker = 1;
    report_option("report_mute_ghost", "0"); report_option("report_tablet_reasons", ""); report_option("report_tablet_cancel", "list");
    g_sess_n = 3; for (int i = 0; i < 16; i++) g_sess_bot[i] = 0;
    snprintf(g_xpid, sizeof g_xpid, "DMO-2002");
    g_ac_bound = 1; g_rpt_bound = -1; five_open(); un_reset(); od_reset();
    fz_init();
    for (int i = 0; i < RPL_SO_N; i++) if (fz_idx(GPA_SO, csym64(g_rpl_so_def[i].zone)) < 0) fz_add(GPA_SO, csym64(g_rpl_so_def[i].zone), 0);
    g_fz_ready = 1; rpt_so_start(); tick(300);
    uint16_t *cb = fz_card("close_button"), *mg = lz("ffa_acr_mg"), *r1 = lz("ffa_acr_reason1"), *b5 = fz_rs(fz_so(RPT_SO_B5));
    CHECK(layer_all(1), "no panel: all 10 layer zones off");

    /* report_mute_ghost=0: the drawn layer releases 9 zones, Mute & Ghost stays off; =1 releases all 10 */
    tr_open(); *cb |= 1; tick(150);
    CHECK(g_rpt_on && g_rpt_drawn && (*mg & RP_SO_BIT) && *r1 == 0 && *lz("ffa_acr_submit") == 0, "mute-ghost gate: M&G held off (%04x), reasons live", *mg);
    press_zone("ffa_acr_cancel"); tick(300);
    CHECK(!g_rpt_on && layer_all(1), "mute-ghost gate: CANCEL: all 10 off again");
    *cb &= (uint16_t)~1u; tick(1200);
    report_option("report_mute_ghost", "1");
    tr_open(); *cb |= 1; tick(150);
    CHECK(g_rpt_drawn && *mg == 0 && *r1 == 0, "mute-ghost gate: report_mute_ghost=1: M&G live (%04x)", *mg);
    press_zone("ffa_acr_cancel"); tick(300); *cb &= (uint16_t)~1u; tick(1200);
    report_option("report_mute_ghost", "0");

    /* report=0 or a failed check: the guard still holds our zones once stock-off is decided (report_so_check runs first) */
    {
        static const int on_v[2] = { 0, 1 }, ready_v[2] = { 1, 0 };
        for (int k = 0; k < 2; k++) {
            g_rp_on = on_v[k]; g_rp_ready = ready_v[k];
            for (int i = 0; i < RPL_SO_N; i++) { rpt_so_drop(&g_rpl_so, i); *fz_rs(fz_idx(GPA_SO, csym64(g_rpl_so_def[i].zone))) = 0; }
            g_rp_so_engine = -1; tick(300);
            CHECK(layer_all(0), "stock-off before checks %s: stock-off not decided: the stock-enabled zones stay live (the old order)", k ? "check failed" : "report=0");
            rpt_so_start(); tick(300);
            CHECK(layer_all(1), "stock-off before checks %s: decided first: all 10 layer zones held off with the flow off", k ? "check failed" : "report=0");
            { uint64_t ev[5] = { csym64("delegate_OnFullyDepressed"), 0, 0, csym64("ffa_acr_back"), 0 }; int pc = g_press_comp_n;
              report_event(ev, 1, NULL); tick(100);
              CHECK(g_press_comp_n == pc, "stock-off before checks: engine route: no X forward (the zone is off)"); }
        }
        g_rp_on = 1; g_rp_ready = 1;
    }

    /* fallback mode (no engine route): our "<" with no layer sends the stock X (report off and on); once per press */
    g_fz_ready = 0; rpt_so_start(); tick(RPT_X_GRACE_MS + 100);
    for (int k = 0; k < 2; k++) {
        g_rp_on = k;
        int xf = g_log_xfwd, pc = g_press_comp_n;
        uint64_t ev[5] = { csym64("delegate_OnFullyDepressed"), 0, 0, csym64("ffa_acr_back"), 0 };
        report_event(ev, 0, NULL); report_event(ev, 1, NULL); tick(100);
        CHECK(g_log_xfwd == xf + 1 && g_press_comp_n == pc + 1 && g_press_comp[0] == 0x63e9a9ed5e5fabbbull
              && g_press_comp[1] == csym64("delegate_OnPress") && g_press_comp[2] == 0x58cba490dc70b632ull,
              "fallback back press report=%d: \"<\" with no layer -> one stock X press (%d)", k, g_press_comp_n - pc);
        tick(600);
    }
    g_rp_on = 1;
    g_fz_ready = 1; rpt_so_start(); tick(300);

    /* Tablet: "<" while the POST runs: the panel closes now, the late result is logged, no prompt over the list / card */
    {
        report_option("report_tablet_cancel", "");
        tr_open(); *cb |= 1; tick(150);
        press_zone("ffa_acr_reason2");
        int tn = g_log_tnotshown, posts = g_posts;
        submit_no_worker();
        CHECK(g_rpt_on && g_rp_state == RP_POSTING, "back during POST: SUBMIT taken, POST pending");
        { uint64_t ev[5] = { csym64("delegate_OnFullyDepressed"), 0, 0, csym64("ffa_acr_back"), 0 }; report_event(ev, 1, NULL); g_now += 20; report_tick(NULL, g_now); }
        CHECK(!g_rpt_on && g_rpt_orphan, "back during POST: \"<\" closed the panel during the POST");
        tick(100); tick(100);
        CHECK(g_posts == posts + 1 && g_log_tnotshown == tn + 1 && !g_rpt_res && !g_rpt_orphan, "back during POST: the late result logged, no result prompt");
        bk_run(); *cb &= (uint16_t)~1u; bk_gap(); g_rp_state = RP_IDLE;
    }
    /* Popup: CANCEL on p23 while the POST runs: p24 never enabled */
    {
        rp_reset("test"); g_rpp_page = -1; od_reset(); un_reset();
        open_p22();
        CHECK(g_rpp_page == RPP_LIST, "cancel during POST: p22 open (page %d)", g_rpp_page);
        press_btn(RPP_B_ROW1); press_btn(RPP_B_REASON1 + 1);
        CHECK(g_rpp_page == RPP_REASONS && g_rpp_reason == 1, "cancel during POST: p23, reason picked");
        int pn = g_log_pnotshown;
        g_inj_ev_b = RPP_B_CANCEL; g_http_hook = inj_popup_press;
        press_btn(RPP_B_SUBMIT);
        g_http_hook = NULL; tick(100);
        CHECK(g_log_pnotshown == pn + 1 && g_rpp_page == -1 && !g_rpp_orphan && !g_rpp_post, "cancel during POST: closed while posting, result logged only (page %d)", g_rpp_page);
        rp_reset("test"); g_rpp_page = -1; tick(300);
    }

    /* the open card shows a player the session cannot match: report_label hidden AND button5 held off; the close hands
     * the tile to the closed-card set; a re-open is judged again at once; a matched player releases it */
    {
        int wo = g_log_warnoff;
        snprintf(g_xpid, sizeof g_xpid, "DMO-9999"); tick(1100);
        CHECK(wshown(RP_CARD_T, "report_label") == 0 && (*b5 & RP_SO_BIT) && g_rpw_so.rec[0].st && g_log_warnoff > wo,
              "warning card: warning card: label hidden, button5 held (%04x)", *b5);
        *cb |= 1; tick(150);
        CHECK(!g_rpw_so.rec[0].st && (*b5 & RP_SO_BIT) && rpc_owned() == 5, "warning card: card closed: the closed-card set holds the tiles");
        *cb &= (uint16_t)~1u; tick(150);
        CHECK(g_rpw_so.rec[0].st && (*b5 & RP_SO_BIT) && rpc_owned() == 0, "warning card: re-opened on the same player: held again in the same tick");
        snprintf(g_xpid, sizeof g_xpid, "DMO-2002"); tick(1100);
        CHECK(!g_rpw_so.rec[0].st && !(*b5 & RP_SO_BIT) && wshown(RP_CARD_T, "report_label") == 1, "warning card: a player in the session: button5 live, label shown");
    }

    /* a tablet press during the back walk (the user tapped a LOBBY row) stops the walk: no card closed */
    {
        report_option("report_tablet_cancel", "");
        bk_open(); *cb |= 1; od_reset(); g_od_mode = 1; snprintf(g_od_map[2], 32, "DMO-7007"); g_od_delay = 150;
        int bu = g_log_bkuser;
        press_zone("ffa_acr_back");
        CHECK(g_rpt_bk != RPT_BK_OFF, "back walk stops on tablet press: back walk running (%d)", g_rpt_bk);
        int pc = g_press_comp_n;
        { uint64_t ev[5] = { csym64("delegate_OnFullyDepressed"), 0, 0x7AB1, 0x1234ull, 0 }; report_event(ev, 0, NULL); }
        bk_run();
        CHECK(g_log_bkuser == bu + 1 && g_press_comp_n == pc && !g_rpt_bk && g_un_calls == 0, "back walk stops on tablet press: walk stopped, no close press (%s)", g_log_bk_last);
        *cb &= (uint16_t)~1u; bk_gap();
        /* control: without the tap the wrong card is closed and the walk runs (unchanged) */
        bk_open(); *cb |= 1; od_reset(); g_od_mode = 1; snprintf(g_od_map[2], 32, "DMO-7007"); g_od_delay = 150; un_reset(); snprintf(g_un_map[2], 32, "DMO-2002");
        pc = g_press_comp_n; bu = g_log_bkuser;
        press_zone("ffa_acr_back"); bk_run();
        CHECK(g_log_bkuser == bu && g_press_comp_n > pc, "back walk control: wrong card closed, walk ran");
        *cb &= (uint16_t)~1u; bk_gap();
    }
    fz_init(); g_fz_ready = 0; rpt_so_start(); tick(150);
    g_ac_bound = 0; g_rpt_bound = -1; od_reset(); un_reset();
    rp_reset("test"); g_sess_n = 0; snprintf(g_xpid, sizeof g_xpid, "DMO-1003");
    report_option("report_tablet_cancel", "");
}

int main(int argc, char **argv)
{
    (void)argv; g_verbose = argc > 1;
    g_rp_on = 1; g_rp_ready = 1;
    g_rp_tile_uid = 0xaa15d7d322666e42ull;
    g_rp_card_event = 0xfdb213d9dc5e4826ull;
    g_rp_card_uid = 0x41d2ce330c2a131bull;
    report_init_syms();
    S_STATE_MGR = csym64("state_mgr");
    S_SETUP_LIST_T = csym64("setupcustomuserlist");
    S_NUM = csym64("param_custom_num_users");
    for (int i = 0; i < 5; i++) { char b[64]; snprintf(b, sizeof b, "param_user_list_custom_name_%d", i); S_NAMES[i] = csym64(b); }
    { const char *o[] = { "report_grid", "1", "report_popup", "1", "report_bogus", "1" };
      CHECK(report_option(o[0], o[1]) && report_option(o[2], o[3]) && !report_option(o[4], o[5]), "report_option keys"); }
    /* the legacy tests below were written with the recorded-only Mute & Ghost box: report_mute_ghost=1 for them;
     * test_parity runs the default (0 = hidden on every entry) */
    CHECK(g_rp_mgshow == 0, "report_mute_ghost defaults to 0 (hidden)");
    CHECK(report_option("report_mute_ghost", "1") && g_rp_mgshow == 1, "report_mute_ghost=1 parsed");
    test_ids();
    test_popup();
    test_five();
    test_auto();
    test_fake();
    test_parse();
    test_bots();
    test_entry();
    test_friend();
    test_review();
    test_parity();
    test_s022();
    test_scroll();
    test_tr();
    test_tr_sentence();
    test_review_s022();
    test_w23e();
    test_s023();
    test_s025();
    test_s026();
    test_k1798();
    test_stockoff();
    test_edge_cases_a();
    test_edge_cases_b();
    printf("%d checks, %d failed\n", g_checks, g_fail);
    return g_fail != 0;
}
