/* report_flow.h -- Report Player, shared by radmenu.c (PC) and radmenu_quest.c (Quest).
 *
 * D-S2 (: the game's native scroll, no MORE / next-page rows on the arm tablet): the tablet report
 * starts on the stock LOBBY tab (native scroll and re-bind, the real roster); a row opens the five-tile SOCIAL CONTROLS
 * card, whose REPORT press opens the pause reasons page p23 (ffa_rp_reasons_page) with the card's target preset, the same
 * page and code as the pause path (rows, reason keys, SUBMIT rule, result strings, body; only "entry" differs). The other
 * players are read IN PROCESS on the game thread from the net game (sess_snapshot.h: users[] minus the local users, names
 * by CR15NetGame::UserName, user ids from the lobby entrants, session id = the lobby UUID, match type = the client
 * lobby's). Only when that reader is unavailable (unverified build, no net game) the services host's
 * GET /reports/players[?user_id=] is the fallback: {"session_id","match_type","players":[{"user_id","display_name"}]},
 * the reporter left out. The client API's /session is not used (arena / combat telemetry only).
 * Fallback F (report_tablet_reasons=field, report_popup=0, or p23 not loaded after the pause toggle): ONE 4-row page in
 * the tablet's custom list (state_mgr of actor 0x6c1f6ff04e070923: setupcustomuserlist + transitionarmcomputertostate
 * state=user_list_custom, clearyoffset first), a press = user_list_custom_user_<n>_pressed (n 0-4); then POST /reports
 * on the services host and setupgenericprompt REPORT / <result> / OK + state=generic_prompt.
 * Entries: (A) the SOCIAL CONTROLS card: one tile's label is relabelled REPORT (and, report_card_glyph, its sprite set
 * to an atlas region with its highlight hidden); its press (report_card_event [+ report_card_uid]) is consumed and
 * starts the report at the reasons. The card's target: the card script keeps no index. Its p_userid setter (fed by
 * state_mgr.usernamepressed) turns the index into the card's ONE typed script variable, STRING `xplatformid`
 * (CScriptCS::String on the card actor's ncaScript; the variable accessors of ui_ar_mp_player_social_controls are
 * disasm-only @ PC 6323983201049540, the same type key + name immediates sit together in the Quest card .so
 * @ 6528386917200980), which MUTE / GHOST / INVITE / ADD FRIEND act on. rad-menu reads it, takes the account number
 * after its last '-' and picks the reader player with that user id (any slot, all users, not only the 5 list rows);
 * the card's name_text is only a cross-check, and "the only other player" is
 * used only when the session has exactly one other player and the card's id is unreadable or unmatched. Each card open
 * and each REPORT press logs "report: card target idx=<users[] index> slot=<slot> userid=<last 4> xpid=<code>-<last 4>
 * name=#... shown=#... (<how>; name <matches|differs>; <n> other player(s))", so a gate can compare slot with the
 * pressed_user_index it sent.: pressed_user_index is a script call argument (no event carries it), but the selection
 * IS readable as state_mgr's SYMBOL current_xplatformid = CSymbol64 of the selected player's xplatformid string (reset to
 * 0x600783defbae3bfd on every LOBBY reopen; rad-menu has no SYMBOL getter, so it is not read here), and the card opens
 * DIRECTLY by net slot: the card actor's ncaScript OpenLobbyMemberSocialControls p_userid=<slot> (not the users[] index;
 * the direct call leaves current_xplatformid untouched). rad-menu records slot + the card's full xplatformid string at the
 * REPORT press and re-opens the card with that call on back (rpt_back_tick). Without report_card_event the card is left
 * alone and its events are only logged (learn mode). (B) a minted pause tile: its evt_btn_clicked with
 * ev[3] == report_tile_uid closes the pause menu and opens the tablet on its stock LOBBY tab (no custom write).
 * Mute & ghost is recorded in the report only (v1).
 *
 * radmenu.ini report = auto (key absent or empty, the default): the flow acts only while the report popup
 * marker page ffa_rp_list_page is loaded (our tree; checked once a second, logged on change), and is inert on stock
 * content (no writes, every hook returns untouched); report=1 = any level (the old opt-in), report=0 = off.
 * report_tile_uid defaults to RP_TILE_UID_DEFAULT (the w18 tile ffa_report_tile; 0 = no pause entry).
 * report_fake_rows=N (TEST ONLY, default 0, logged loudly): N synthetic players "TEST 1".."TEST N" appended to the
 * popup player list (it scrolls, below); a synthetic target never POSTs (logged, done page "test row").
 * (the pause list scrolls like the game's own lists, no MORE / next-page row): p22 shows
 * 5 full rows (ffa_rp_row1-5) and a clipped peek row (ffa_rp_row6 on the w22a peek canvas: the next name only, hidden at
 * the end), CANCEL and a dim, inert SUBMIT (ffa_rp_list_submit). g_rpp_off (the first visible name) runs 0 ... N - 5 and
 * is 0 on every open. It steps from the engine's own scroll axis, CR15PointerCS::ScrollAmount (the max-|a| over every
 * pointer record of every gamespace, rp_scroll_amount; dead zone 0.25, 6 rows/s while held, a 1-2 tick impulse = 1 row),
 * or, when that reader is unavailable (report_list_scroll=auto) or chosen (=hover), from a pointer dwell: 700 ms on the
 * peek row (or row 1 while off > 0) steps one row, then one every 350 ms. The global events rad_report_scroll_down /
 * rad_report_scroll_up step one row in every mode but off (a public debug event: the probe path). A row press selects
 * names[off + row] plugin-side, so an event press equals a real press.
 * Every engine entry point is prologue-checked by the platform file; a mismatch
 * leaves the flow inert. Names are logged as #xxxx (low 16 bits of their CSymbol64), user ids as their last 4 chars.
 *
 * The platform file includes sess_snapshot.h and defines, before including this header:
 *   rp_log(fmt, ...), RP_XCHG(int *p, int v) (atomic exchange, returns the old value), rp_sym(name),
 *   rp_call(game, fn, n, names[], values[]) -> 0 ok, 1 busy (retry), -1 refused / not loaded (the state manager's script),
 *   rp_call_on(game, actor, component, fn, n, names[], values[]) -> the same for any actor's script component,
 *   rp_getreal(game, actor, component, variable, &f) -> 1 read, 0 not run (a REAL script variable; the press guard),
 *   rp_settext / rp_gettext(game, actor, element, ...) -> 1 ok, rp_fire(game, holder_actor, sym) -> 1 sent,
 *   rp_press_comp(game, actor, event, component) -> 1 sent (a TARGETED event {event, gs id, SActor, component, -1}, as a
 *   real press of that component builds it; w23b closes the five-tile card with it),
 *   rp_tablet_top(game, &top) -> 1 read (top = the arm tablet's state-stack top symbol, 0 = empty), 0 unavailable,
 *   rp_rewrite(ev, sym) -> 1 rewritten, rp_http(method, client_api, path, body, out, cap) -> HTTP status (0 = transport),
 *   rp_user(out, cap) -> 1 when the logged-in user id is known; rp_actor(gs, sactor) -> the actor's name symbol (ev[2] is
 *   an SActor handle); rp_sess(game, want, snap) -> SESS_OK / SESS_* / RP_SESS_UNVERIFIED (game thread);
 *   rp_sprite_uv(game, actor, element, region_sym) / rp_show(game, actor, element, show) -> 1 ok, 0 not run;
 *   rp_getvar(game, actor, component, variable, out, cap) -> 1 read (out may be ""), 0 not run (a STRING script variable);
 *   rp_setvar(game, actor, component, variable, str) -> 1 written (CScriptCS::SetString), 0 not run / bytes differ;
 *   RP_CARD_EL_DEFAULT (the card label element name);
 *   rp_has_el(game, actor, element) -> 1 the actor's canvas has the element (Quest: FindPrimitiveID logs an engine
 *   error for a missing one, so callers cache), rp_color(game, actor, element, rgb[3]) -> 1 set (SetColor, RGB only),
 *   rp_show_page(game, owner) -> 1 enabled (EnablePage on the owner's page component, any gamespace),
 *   rp_has_actor(game, actor) -> 1 loaded, rp_actor_key(game, actor) -> the holding gamespace (0 none; a cache key),
 *   rp_scroll_amount(game, &a) -> 1 read (a = the signed max-|ScrollAmount| over the CR15PointerCS records), 0 the reader
 *   is unavailable (byte checks failed, no pointer system, fault): the list falls back to the hover dwell (game thread).
 *
 * additions (all behind ini keys, see report_option):
 *   - report_tablet_reasons (ac): the card's REPORT opens the tablet's generic prompt with the w23a layer (ac; popup when
 *     ffa_ac_report is not bound), p23 preset (popup), or fallback F (field); see rps_press / rpt_*. report_grid,
 *     report_reason_ui, report_list_paging and report_player_card* are parsed and ignored (removed).
 *   - report_tablet_close_card (1): opening the tablet panel presses the five-tile card's close_button (w23b).
 *   - report_tablet_cancel (card):  "<" = BACK one step, the panel's CANCEL the same by default: layer closed, user_list, the target's card re-opened closed-loop (rpt_back_*); list = CANCEL lands on the list.
 *   - report_popup (1, only when the page ffa_rp_list_page is loaded): the pause REPORT tile opens the report popup pages
 *     (names in g_rpp_* below, ids derived:
 *     owner = csym64(name), click uid = csym64(name + "_uid"), press = report_popup_event with ev[3] = uid).
 *   - five-tile card (canvas has report_label): no v1 relabel / interim glyph, INVITE / ADD FRIEND keep their stock
 *     actions, REPORT = button5 (report_card5_event + report_card5_uid), card events logged for 60 s after each card
 *     open (report_card5_learn), report_highlight hidden at open, report_label hidden while the card's player is not in
 *     the session (the stock warning_message case; proxy: xplatformid matches no reader user). */
#ifndef REPORT_FLOW_H
#define REPORT_FLOW_H

#define RP_TABLET      0x6c1f6ff04e070923ull   /* arm tablet state manager actor (r14_glb_global_mp) */
#define RP_TABLET_COMP 0x63e9a9ed5e5fabbbull   /* arm tablet COMPONENT actor (index 0x10): holds the stock X zone  */
#define RP_TABLET_X    0x58cba490dc70b632ull   /* the stock tablet X zone: closes the whole tablet on delegate_OnPress */
#define RP_CARD        0x61c4b2b9153a12b7ull   /* SOCIAL CONTROLS card actor (ui_ar_mp_player_social_controls) */
#define RP_PAUSE_P1    0x68db70ece7c24901ull   /* pause menu p1 home owner (mnu_master_mp_ingame[_lowspec]) */
#define RP_NCA_SCRIPT  0x6aea99eeb2bab6c4ull   /* csym64("ncaScript"): the card actor's script component */
#define RP_XPID        0x193874e723ec02b6ull   /* csym64("xplatformid"): the card script's STRING variable */
#define RP_MAXROWS     5                       /* the tablet custom list: 5 press gates (fallback F draws 4 rows) */
#define RP_ROWCAP      32                      /* rows kept (the popup pages them by 6): SESS_MAX_USERS + test rows */
#define RP_FAKE_MAX    20                      /* report_fake_rows cap */
#define RP_TILE_UID_DEFAULT 0xaa15d7d322666e42ull   /* csym64("ffa_report_tile_uid"): the w18 pause REPORT tile */
#define RP_NAMELEN     64
#define RP_IDLEN       RP_NAMELEN              /* user ids (host user_id, account ids): never cut below a name's room */
#define RP_TIMEOUT_MS  120000
#define RP_ST_TEST     (-2)                    /* POST status of a synthetic test row (never sent) */
#define RP_SESS_UNVERIFIED (-100)              /* rp_sess: the reader's prologue / build checks failed */
#define RP_NREASON     6

enum { RP_IDLE, RP_OPENING, RP_WAIT_SESSION, RP_POSTING, RP_FIELD, RP_PRESET };   /* RP_PRESET: the pause toggle for p23 */
enum { RP_JOB_NONE, RP_JOB_HOST, RP_JOB_POST };   /* RP_JOB_HOST: GET /reports/players (reader fallback) */

/* Coffey's six, in order; key = the POST /reports reason */
static const char *const g_rp_reason_label[RP_NREASON] = { "Hate speech", "Cheating", "Harassment or bullying",
    "Inappropriate behavior", "Unsportsmanlike conduct", "Impersonation of RAD staff" };
/* fallback F's field row: the same six cut so "<label> >" fits one row (the pause tiles and the POST keep the full wording) */
static const char *const g_rp_reason_row[RP_NREASON] = { "Hate speech", "Cheating", "Harassment or bullying",
    "Inappropriate behavior", "Unsportsmanlike", "Posing as RAD staff" };
static const char *const g_rp_reason_key[RP_NREASON] = { "hate_speech", "cheating", "harassment", "inappropriate",
    "unsportsmanlike", "impersonation" };
/* ---- tablet / pause parity: ONE string table for every report surface. The pause done page (frame `sentence`) and the
 * tablet generic prompt both read g_rp_msg; the pause sentence (RP_SENTENCE_FMT, the full g_rp_target) and fallback F's
 * sentence row (RP_SENTENCE_ROW_FMT, the name cut to fit RP_ROWCHARS, as a list row is narrow) say the same thing; the Mute & Ghost label is one string. Only the layout differs per entry. ---- */
enum { RP_MSG_NO_PLAYERS, RP_MSG_NO_SESSION, RP_MSG_SUBMITTED, RP_MSG_ALREADY, RP_MSG_NOT_SENT, RP_MSG_TEST_ROW,
       RP_MSG_CANCELLED, RP_MSG_NO_TARGET, RP_NMSG };
static const char *const g_rp_msg[RP_NMSG] = { "No other players to report.", "No session found. Report not sent.",
    "Report submitted. Thank you.", "Player already reported.", "Report not sent.", "Test row: report not sent.",
    "Report cancelled.", "Player not found. Report not sent." };
#define RP_SENTENCE_FMT       "%s will be reported for violating the Echo VR Code of Conduct:"   /* pause frame (592 px) */
#define RP_SENTENCE_ROW_FMT   "Report %s:"                 /* tablet: the sentence cut to one list row */
#define RP_ROWCHARS           26                           /* a tablet list row shows about 26 characters */
static const char g_rp_mg_label[] = "Mute & Ghost";   /* was "MUTE & GHOST (IN REPORT)" (clipped in a list row) */
/* w23e: the tablet panel has the room for Coffey's full label (acr_mg_text cap 32); the list row and the
 * pause popup keep the short one (supersedes n3's "one label on both surfaces" for the tablet only) */
static const char g_rp_mg_label_ac[] = "Mute & Ghost Reported Player";
/* POST status -> g_rp_msg index (RP_ST_TEST = a synthetic test row, never sent) */
static int rp_result_msg(int st)
{
    return st == RP_ST_TEST ? RP_MSG_TEST_ROW : st == 201 ? RP_MSG_SUBMITTED : st == 409 ? RP_MSG_ALREADY : RP_MSG_NOT_SENT;
}

/* ---- pause report popups: the contract's names, in ONE table (patch here if the popup tree's contract differs).
 * Buttons: owner = csym64(name), click uid = csym64(name "_uid"). ---- */
#define RPP_ROWS 6                  /* row actors: 5 full rows + the peek row */
#define RPP_VIS  5                  /* full rows (73 px pitch, a 6th row clipped) */
#define RPP_PEEK (RPP_ROWS - 1)     /* ffa_rp_row6, re-bound to the clipped canvas ffa_rp_row_peek (w22a) */
enum { RPP_B_ROW1 = 0, RPP_B_LIST_SUBMIT = RPP_ROWS, RPP_B_LIST_CANCEL, RPP_B_REASON1, RPP_B_MG = RPP_B_REASON1 + RP_NREASON,
       RPP_B_CANCEL, RPP_B_SUBMIT, RPP_B_OK, RPP_NB };
static const char *const g_rpp_btn_name[RPP_NB] = {
    "ffa_rp_row1", "ffa_rp_row2", "ffa_rp_row3", "ffa_rp_row4", "ffa_rp_row5", "ffa_rp_row6", "ffa_rp_list_submit",
    "ffa_rp_list_cancel", "ffa_rp_reason1", "ffa_rp_reason2", "ffa_rp_reason3", "ffa_rp_reason4", "ffa_rp_reason5",
    "ffa_rp_reason6", "ffa_rp_mg", "ffa_rp_cancel", "ffa_rp_submit", "ffa_rp_ok" };
enum { RPP_LIST, RPP_REASONS, RPP_DONE, RPP_NPAGE };
static const char *const g_rpp_page_name[RPP_NPAGE] = { "ffa_rp_list_page", "ffa_rp_reasons_page", "ffa_rp_done_page" };
static const char *const g_rpp_frame_name[RPP_NPAGE] = { "ffa_rp_list_frame", "ffa_rp_reasons_frame", "ffa_rp_done_frame" };
/* canvas-local elements the plugin writes (bit i of g_rpp_has[button]) */
enum { RPP_E_TEXT, RPP_E_SUB, RPP_E_BG, RPP_E_SELC, RPP_E_SELS, RPP_E_CHECK, RPP_E_CHECK2, RPP_E_STEXT, RPP_E_BBG, RPP_E_BOUT, RPP_E_OUT, RPP_E_ICON, RPP_NE };
static const char *const g_rpp_el_name[RPP_NE] = {
    "buttontext",       /* row name / button label; SUBMIT: tinted grey until a reason is picked */
    "subtext",          /* row "Online" */
    "Background",       /* hidden with the text on an unused row */
    "buttonhighlight",  /* reason: SetColor teal when selected, stock otherwise; row: the hover sprite, tinted to the
                           popup body on an unused row (the engine still drew it under the pointer) */
    "selected",         /* reason: optional "selected" sprite, shown on the selected one */
    "check",            /* MUTE & GHOST: visible = on (the tick's long stroke) */
    "check2",           /* MUTE & GHOST tick, short stroke (check.json of the popup tree); shown with check */
    "text",             /* SUBMIT / list SUBMIT: the stock ui_mnu_button_small label element (ffa_rp_button_dim) */
    "buttonbg",         /* row fill (third-button canvas copy ffa_rp_row): hidden on an unused row */
    "buttonoutline",    /* row outline (same canvas) */
    "outline",          /* button outline (ui_mnu_button_small; list SUBMIT) */
    "buttonicon" };     /* UNCONFIRMED candidate: the donor's icon sprite (the popup tree's row.json prim 4: rect 0,0,10,10, tint alpha 0 that
                           the engine does not honour for buttonoutline either); hidden with the row. To confirm: the shot of a hidden
                           row under the pointer (report_fake_rows=8, scrolled to the end) must show no right-edge fragment; if it still does, the
                           element is not this one (candidates left: the `button` panel prim 0, an outline 9-slice edge). */
#define RPP_EL_SENTENCE "sentence"             /* frame text: reasons "<name> will be reported ...", done message */
static const float RPP_TEAL[3] = { 0x2f / 255.0f, 0xa3 / 255.0f, 0xad / 255.0f };   /* contract #2fa3ad */
static const float RPP_STOCK[3] = { 0x45 / 255.0f, 0x5a / 255.0f, 0x6f / 255.0f };  /* contract #455a6f */
static const float RPP_WHITE[3] = { 1.0f, 1.0f, 1.0f };
static const float RPP_DIM[3] = { 0.5f, 0.5f, 0.5f };   /* "50 %": SetColor has no alpha, grey stands in */
static const float RPP_DIM_BG[3] = { 0.22f, 0.25f, 0.29f };   /* SUBMIT background while no reason is picked */
static const float RPP_ROW_HL[3] = { 0.596f, 0.639f, 0.671f };  /* popup tree row.json prim 2 buttonhighlight (#98a3ab) */
static const float RPP_BODY[3] = { 0.439f, 0.478f, 0.525f };    /* popup body #707a86: an unused row's hover blends in */

#include "sess_snapshot.h"

/* options (radmenu.ini) */
static int g_rp_on;                         /* report: -1 auto (default; set by the platform), 0 off, 1 any level */
static volatile int g_rp_live = -1;         /* game thread -> hooks: 1 the flow acts (auto: popup marker loaded) */
static uint64_t g_rp_next_live;
static int g_rp_fake_rows;                  /* report_fake_rows (TEST ONLY, 0) */
static int g_rpt_close_card = 1;             /* report_tablet_close_card: the tablet panel closes the five-tile card (w23b) */
static int g_rpt_cancel_card = 1;            /* report_tablet_cancel: the tablet reasons CANCEL -> 1 the card, 0 the list */
static int g_rp_hide_bots = 1;              /* report_hide_bots: AI bots are no account, left out of the reader rows */
/* report_mute_ghost: 0 (default) = no Mute & Ghost box on any entry, the body sends mute_ghost false;
 * 1 = the recorded-only box (the earlier behaviour, nothing is muted). Not applied by the stock user_actions route: those
 * actions are TOGGLES with no readable state (a muted target would be unmuted), and the stock lobby row of the
 * target is not known once bots are hidden or past row 5 (the row mapping is only 2-player verified). */
static int g_rp_mgshow;
static const char *g_rp_entry = "";         /* what opened the flow: card / pause (tablet: the tile's staged open only); "" = no "entry" */
#ifndef RP_PLATFORM
#ifdef _WIN32
#define RP_PLATFORM "pc"
#else
#define RP_PLATFORM "quest"
#endif
#endif
/* The tablet panel's SUBMIT fill (acr_submit_fill), [platform: 0 pc, 1 quest][0 no reason picked, 1 enabled]. Enabled =
 * the panel's own CANCEL fill (acr_cancel_fill), as the design draws an enabled SUBMIT; disabled = the canvas's authored
 * SUBMIT fill, so a reopened panel looks as loaded. Each platform's canvas carries its own tints (the PC values are
 * refitted for the PC tablet's light response), so these mirror each canvas's tint ops; keep them equal when either
 * canvas is regenerated. */
static const float RPT_SUB_FILL[2][2][3] = {
    { { 0.298f, 0.298f, 0.329f }, { 0.408f, 0.378f, 0.385f } },   /* pc canvas: submit / cancel fill tint */
    { { 0.227f, 0.235f, 0.263f }, { 0.420f, 0.396f, 0.404f } },   /* quest canvas: submit / cancel fill tint */
};
#define RPT_PLAT_QUEST (RP_PLATFORM[0] == 'q')
static uint8_t g_rp_rowfake[RP_ROWCAP];     /* 1 = a synthetic test row */
static int g_rp_target_fake;                /* the current target is a synthetic test row: never POSTed */
static uint64_t g_rp_tile_uid;              /* report_tile_uid (0 = no pause entry) */
static uint64_t g_rp_card_event, g_rp_card_uid;   /* report_card_event / report_card_uid (0 = learn mode / any uid) */
static uint64_t g_rp_card_el;               /* report_card_element (RP_CARD_EL_DEFAULT) */
static int g_rp_ready;                      /* the platform's prologue checks passed */
static uint64_t g_rp_glyph;                 /* report_card_glyph: an atlas region symbol (0 = off) */

/* symbols */
static uint64_t S_STATE_MGR, S_SETUP_LIST, S_SETUP_SEL, S_SEL[RP_MAXROWS], S_TRANSITION, S_SETUP_PROMPT, S_NUM_USERS, S_NAME[RP_MAXROWS], S_STATE,
    S_ST_LIST, S_ST_PROMPT, S_P_HEADER, S_P_MESSAGE, S_P_BACK, S_P_BACK_EN, S_PRESSED[RP_MAXROWS], S_BTN_CLICKED,
    S_TOGGLE_PAUSE, S_TABLET_OPEN, S_TABLET_LOBBY, S_NAME_TEXT, S_RP_NOOP, S_FRIEND_LABEL, S_FRIEND_HL, S_CLEAR_Y, S_LIST_Y,
    S_LIST_H, S_W2C;
static int g_rp_open_step;   /* pause tile: 0 pause closed, 1 tablet opened, 2 LOBBY tab selected */

/* ADD FRIEND on the five-tile card. The platform file defines RP_HAVE_CARD_FRIEND and
 * rp_card_button4(game) -> 1 R14ButtonInteract button4 enabled (engine EnableComponent), 0 not run (a prologue check
 * failed / actor not loaded), -1 fault; this header then shows friend_label and hides friend_highlight, as the stock
 * setupfriendbutton does for a non-friend (those three writes are all it does). card_add_friend (default 0: on PC the
 * stock card's open / close threads re-disable the tile under these writes, so it flickers and is mostly not drawn) is
 * parsed on both platforms; Quest has no hook (the stock script already shows the tile there) so the key is a no-op. */
#define RP_F4_QUIET_MS 1500                  /* card events this soon after a close press are the close, not an open */
static int g_rp_addfriend = 0;               /* card_add_friend: 0 off (default), 1 on each card open (+ card events), 2 also every poll */
static volatile int g_rp_f4_act, g_rp_f4_close;   /* hook -> tick: card events seen / close_button pressed */
static uint64_t g_rp_f4_closed_at, S_CLOSE_BTN;
static int g_rp_f4_done = -9, g_rp_f4_open, g_rp_f4_re;   /* last result logged; card believed open; re-apply on the next poll */
#ifndef RP_HAVE_CARD_FRIEND
static int rp_card_button4(void *game) { (void)game; return 0; }
#endif


/* hook -> tick */
static int g_rp_ev_press = -1, g_rp_ev_tile, g_rp_ev_card, g_rp_learned;

/* tick state (game thread) */
static int g_rp_state, g_rp_retry, g_rp_card_logged;
static uint64_t g_rp_since, g_rp_next_card;
static int g_rp_nrows;
static char g_rp_rows[RP_ROWCAP][RP_NAMELEN], g_rp_rowid[RP_ROWCAP][RP_IDLEN];
static char g_rp_target[RP_NAMELEN], g_rp_target_id[RP_IDLEN];
static int g_rp_reason;
static int g_rp_pending;                    /* 0 none, 1 fallback F's field page, 2 prompt */
static const char *g_rp_prompt_msg;

/* worker <-> tick */
static int g_rp_job, g_rp_job_done, g_rp_job_status;
static volatile int g_rp_job_gen;           /* bumped by every job start and every reset: a worker reply of an older one is dropped */
static char g_rp_post_body[1024];
static char g_rp_resp[32768];
static char g_rp_sess_id[64], g_rp_sess_type[64];
static char g_rp_players_path[160];          /* GET /reports/players[?user_id=…], built on the game thread */
static int g_rp_rowi[RP_ROWCAP], g_rp_rowslot[RP_ROWCAP];   /* reader rows: users[] index, lobby slot (-1 host rows) */
static sess_snapshot g_rp_snap;              /* game thread only */

/* options (report_option; symbols resolved in report_init_syms) */
enum { RP_TR_AC, RP_TR_POPUP, RP_TR_FIELD };   /* report_tablet_reasons */
static const char *const g_rp_tr_name[3] = { "ac", "popup", "field" };
static int g_rp_treasons = RP_TR_AC;         /* ac (default): the tablet's generic prompt with the w23a layer (falls back to popup
                                                when ffa_ac_report is not bound); popup: p23 preset; field: fallback F */
static int g_rpp_on = 1;                     /* report_popup (acts only when ffa_rp_list_page is loaded) */
static char g_rpp_event_s[96] = "evt_btn_clicked";             /* report_popup_event */
static char g_rp_card5_event_s[96] = "delegate_OnFullyDepressed";   /* report_card5_event */
static uint64_t g_rp_card5_uid = 0x2fd59a8b4a7b59dfull;        /* report_card5_uid: csym64("button5") until learned */
static int g_rp_card5_learn = 1;             /* report_card5_learn */

/* fallback F (RP_FIELD), game thread */
static int g_rp_gsel = -1, g_rp_gmg;         /* g_rp_gmg: MUTE & GHOST (report_mute_ghost=1 only), default unchecked */
static char g_rp_grow[RP_MAXROWS][RP_NAMELEN];
static int g_rp_gn = RP_MAXROWS;             /* rows of the field page drawn last */

/* five-tile card */
static uint64_t S_REPORT_LABEL, S_REPORT_HL, g_rp_card5_event;
static int g_rp_five = -1;                   /* tick -> hook: 1 the card canvas has report_label */
static uintptr_t g_rp_five_key;              /* the card's gamespace when g_rp_five was decided */
static int g_rp_learn5, g_rp_learn5_on;      /* card events logged since the last card open; logging on */
static uint64_t g_rp_learn5_until;
static char g_rp_card5_seen[2 * RP_NAMELEN + 2];   /* name_text|xplatformid at the last card open */

/* pause popups, game thread unless noted */
static uint64_t g_rpp_event, g_rpp_owner[RPP_NPAGE], g_rpp_frame[RPP_NPAGE], g_rpp_actor[RPP_NB], g_rpp_uid[RPP_NB],
    g_rpp_el[RPP_NE], S_RPP_SENTENCE;
static int g_rpp_ev = -1;                    /* hook -> tick: the pressed button */
static int g_rpp_page = -1;                  /* the popup page shown by us (-1 none) */
static int g_rpp_off, g_rpp_sel = -1, g_rpp_reason = -1, g_rpp_mg, g_rpp_wait, g_rpp_post, g_rpp_marker = -1;
static int g_rpp_orphan;   /* the popup closed while its POST ran: the result is logged, p24 never enabled */
static int g_rpp_last_b = -1, g_rpp_pass;
static uint64_t g_rpp_last_t, g_rpp_since, g_rpp_next;
static int g_rpp_preset, g_rpp_toggled;      /* p23 opened from the card (CANCEL / OK toggle the pause menu off); we toggled it on */
static uint64_t g_rpp_preset_t, g_rpp_owner_t, g_rpp_reassert_t;   /* toggle sent; p23's owner first seen loaded; the re-enable due */
static uint16_t g_rpp_has[RPP_NB];            /* element bits per button, probed once per popup open */
/* list scroll (report_list_scroll / report_scroll_probe / report_scroll_sign) */
enum { RPP_SCR_AUTO, RPP_SCR_STICK, RPP_SCR_HOVER, RPP_SCR_OFF };
static const char *const g_rpp_scr_name[4] = { "auto", "stick", "hover", "off" };
#define RPP_SCROLL_DEAD   0.25f    /* |ScrollAmount| below this is no input */
#define RPP_SCROLL_RATE   6.0f     /* rows per second at |a| = 1 */
#define RPP_HOVER_FIRST_MS 700     /* dwell on the peek row (or row 1) before the first step */
#define RPP_HOVER_REPEAT_MS 350    /* then one step per this while the pointer stays */
static int g_rpp_scroll_mode = RPP_SCR_AUTO, g_rpp_scroll_probe, g_rpp_scroll_sign = 1;
static volatile int g_rpp_hov;                /* hook -> tick: +1 pointer on the peek row, -1 on row 1, 0 none */
static volatile int g_rpp_scr_ev;             /* hook -> tick: rad_report_scroll_down (+1) / _up (-1) seen */
static float g_rpp_acc;                       /* stick accumulator (rows) */
static int g_rpp_run, g_rpp_run_sign, g_rpp_run_steps;   /* ticks / sign / steps of the current non-zero amount run */
static int g_rpp_src = -1;                    /* the source the tick used last: 1 stick, 0 hover (logged on change) */
static int g_rpp_hov_dir;
static uint64_t g_rpp_scroll_t, g_rpp_hov_next;
static uint64_t S_PTR_START, S_PTR_END, S_PTR_HOVER, S_PTR_IDLE, S_SCROLL_DN, S_SCROLL_UP;
static int g_rpp_sent_ok[RPP_NPAGE];
static char g_rpp_done_msg[96];

static uint16_t rp_tag(const char *s) { return (uint16_t)(rp_sym(s) & 0xffff); }
static const char *rp_mask(const char *id) { size_t n = strlen(id); return n > 4 ? id + n - 4 : "****"; }

static int rp_ieq(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x = (char)(x + 32);
        if (y >= 'A' && y <= 'Z') y = (char)(y + 32);
        if (x != y) return 0;
    }
    return *a == *b;
}

/* the one parser of the `report=` ini value, called by both platforms. -1 = auto (empty / "auto" / "-1" / any
 * other negative number), 0 = off ("0", "off"), 1 = on ("1", "on", or any other positive number: PC's old atoi made
 * 2 on, Quest's old range check made it auto; a hex such as 0x1 is on, 0x0 off). Surrounding blanks are ignored.
 * Garbage -> auto, with a log line. */
static int rp_parse_report(const char *v)
{
    char b[32];
    size_t n = 0;
    while (v && (*v == ' ' || *v == '\t')) v++;
    for (; v && *v && n + 1 < sizeof b; v++) b[n++] = *v;
    b[n] = 0;
    while (n && (b[n - 1] == ' ' || b[n - 1] == '\t' || b[n - 1] == '\r' || b[n - 1] == '\n')) b[--n] = 0;
    if (!n || rp_ieq(b, "auto")) return -1;
    if (rp_ieq(b, "on") || rp_ieq(b, "true")) return 1;
    if (rp_ieq(b, "off") || rp_ieq(b, "false")) return 0;
    char *e;
    long x = strtol(b, &e, 0);
    if (e != b && !*e) return x < 0 ? -1 : x == 0 ? 0 : 1;
    rp_log("report: report=\"%s\" not understood (auto, on, off, 0, 1, -1): auto", b);
    return -1;
}

/* ini notes: radmenu.ini is parsed before the log is open, so report_option's lines are kept here and written by
 * report_init_syms (the report start) */
static char g_rp_early[8][160];
static int g_rp_nearly;
static void rp_note(const char *fmt, ...)
{
    if (g_rp_nearly >= 8) return;
    va_list a;
    va_start(a, fmt);
    vsnprintf(g_rp_early[g_rp_nearly++], sizeof g_rp_early[0], fmt, a);
    va_end(a);
}

/* 0 / 1 / off / on / false / true (any case); empty = def; anything else = def, with a log line */
static int rp_opt01(const char *v, int def)
{
    if (!*v) return def;
    if (!strcmp(v, "0") || rp_ieq(v, "off") || rp_ieq(v, "false")) return 0;
    if (!strcmp(v, "1") || rp_ieq(v, "on") || rp_ieq(v, "true")) return 1;
    rp_note("report: option value \"%s\" not understood (0, 1, off, on, false, true): %d", v, def);
    return def;
}

/* the report radmenu.ini keys (both platforms' parsers call this first); 1 = handled. Values are kept as text until
 * report_init_syms. */
static int report_option(const char *k, const char *v)
{
    if (rp_ieq(k, "report_popup")) g_rpp_on = rp_opt01(v, 1);
    else if (rp_ieq(k, "report_popup_event")) snprintf(g_rpp_event_s, sizeof g_rpp_event_s, "%s", *v ? v : "evt_btn_clicked");
    else if (rp_ieq(k, "report_card5_event")) snprintf(g_rp_card5_event_s, sizeof g_rp_card5_event_s, "%s", *v ? v : "delegate_OnFullyDepressed");
    else if (rp_ieq(k, "report_card5_uid")) g_rp_card5_uid = *v ? strtoull(v, NULL, 0) : 0x2fd59a8b4a7b59dfull;
    else if (rp_ieq(k, "report_card5_learn")) g_rp_card5_learn = rp_opt01(v, 1);
    else if (rp_ieq(k, "report_hide_bots")) g_rp_hide_bots = rp_opt01(v, 1);
    else if (rp_ieq(k, "report_mute_ghost")) g_rp_mgshow = rp_opt01(v, 0);
    else if (rp_ieq(k, "report_tablet_close_card")) g_rpt_close_card = rp_opt01(v, 1);
    else if (rp_ieq(k, "report_tablet_cancel")) {   /*: the tablet reasons CANCEL lands on the card (Coffey) or the list */
        if (*v && !rp_ieq(v, "card") && !rp_ieq(v, "list")) rp_note("report: report_tablet_cancel=\"%s\" not understood (card, list): card", v);
        g_rpt_cancel_card = !rp_ieq(v, "list");
    }
    else if (rp_ieq(k, "card_add_friend")) g_rp_addfriend = *v == '2' ? 2 : rp_opt01(v, 0);
    else if (rp_ieq(k, "report_tablet_reasons")) {
        int m = RP_TR_AC;
        for (int i = 0; i < 3; i++) if (rp_ieq(v, g_rp_tr_name[i])) m = i;
        if (*v && m == RP_TR_AC && !rp_ieq(v, "ac")) rp_note("report: report_tablet_reasons=\"%s\" not understood (ac, popup, field): ac", v);
        g_rp_treasons = m;
    }
    else if (rp_ieq(k, "report_list_scroll")) {
        int m = RPP_SCR_AUTO;
        for (int i = 0; i < 4; i++) if (rp_ieq(v, g_rpp_scr_name[i])) m = i;
        if (*v && m == RPP_SCR_AUTO && !rp_ieq(v, "auto")) rp_note("report: report_list_scroll=\"%s\" not understood (auto, stick, hover, off): auto", v);
        g_rpp_scroll_mode = m;
    }
    else if (rp_ieq(k, "report_scroll_probe")) g_rpp_scroll_probe = rp_opt01(v, 0);
    else if (rp_ieq(k, "report_scroll_sign")) g_rpp_scroll_sign = atoi(v) < 0 ? -1 : 1;
    else if (rp_ieq(k, "report_fake_rows")) { int n = atoi(v); g_rp_fake_rows = n < 0 ? 0 : n > RP_FAKE_MAX ? RP_FAKE_MAX : n; }
    else {
        static const char *const gone[] = { "report_grid", "report_reason_ui", "report_list_paging", "report_player_card",
            "report_player_card_stock", "report_player_card_header", "report_player_card_header_actor",
            "report_player_card_header_element", "report_player_card_glyphs" };
        for (size_t i = 0; i < sizeof gone / sizeof gone[0]; i++)
            if (rp_ieq(k, gone[i])) { rp_note("report: %s ignored (key removed)", gone[i]); return 1; }
        return 0;
    }
    return 1;
}

static uint64_t rp_sym_or_hex(const char *s)
{
    return (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) ? strtoull(s, NULL, 16) : rp_sym(s);
}

static void rpt_init_syms(void);
static void report_init_syms(void)
{
    char b[96];
    for (int i = 0; i < g_rp_nearly; i++) rp_log("%s (radmenu.ini)", g_rp_early[i]);
    g_rp_nearly = 0;
    rpt_init_syms();
    g_rp_card5_event = rp_sym_or_hex(g_rp_card5_event_s);
    g_rpp_event = rp_sym_or_hex(g_rpp_event_s);
    S_REPORT_LABEL = rp_sym("report_label");
    S_REPORT_HL = rp_sym("report_highlight");
    S_RPP_SENTENCE = rp_sym(RPP_EL_SENTENCE);
    for (int i = 0; i < RPP_NPAGE; i++) { g_rpp_owner[i] = rp_sym(g_rpp_page_name[i]); g_rpp_frame[i] = rp_sym(g_rpp_frame_name[i]); }
    for (int i = 0; i < RPP_NB; i++) {
        g_rpp_actor[i] = rp_sym(g_rpp_btn_name[i]);
        snprintf(b, sizeof b, "%s_uid", g_rpp_btn_name[i]);
        g_rpp_uid[i] = rp_sym(b);
    }
    for (int i = 0; i < RPP_NE; i++) g_rpp_el[i] = rp_sym(g_rpp_el_name[i]);
    S_STATE_MGR = rp_sym("state_mgr");
    S_SETUP_LIST = rp_sym("setupcustomuserlist");
    S_SETUP_SEL = rp_sym("setupcustomuserlistselectable");
    S_TRANSITION = rp_sym("transitionarmcomputertostate");
    S_SETUP_PROMPT = rp_sym("setupgenericprompt");
    S_NUM_USERS = rp_sym("param_custom_num_users");
    S_STATE = rp_sym("state");
    S_ST_LIST = rp_sym("user_list_custom");
    S_ST_PROMPT = rp_sym("generic_prompt");
    S_P_HEADER = rp_sym("param_prompt_header");
    S_P_MESSAGE = rp_sym("param_prompt_message");
    S_P_BACK = rp_sym("param_prompt_back");
    S_P_BACK_EN = rp_sym("param_prompt_back_enabled");
    S_BTN_CLICKED = rp_sym("evt_btn_clicked");
    S_TOGGLE_PAUSE = rp_sym("evt_dbg_toggle_menu_vis");
    S_TABLET_OPEN = rp_sym("evt_debug_arm_computer_open");
    S_TABLET_LOBBY = rp_sym("evt_debug_arm_computer_lobby");
    S_NAME_TEXT = rp_sym("name_text");
    S_RP_NOOP = rp_sym("rad_report_consumed");
    for (int i = 0; i < RP_MAXROWS; i++) {
        snprintf(b, sizeof b, "param_user_list_custom_name_%d", i);
        S_NAME[i] = rp_sym(b);
        snprintf(b, sizeof b, "user_list_custom_user_%d_pressed", i);
        S_PRESSED[i] = rp_sym(b);
        snprintf(b, sizeof b, "param_user_list_custom_selectable_%d", i);
        S_SEL[i] = rp_sym(b);
    }
    if (!g_rp_card_el) g_rp_card_el = rp_sym(RP_CARD_EL_DEFAULT);
    S_CLOSE_BTN = rp_sym("close_button");
    S_FRIEND_LABEL = rp_sym("friend_label");
    S_FRIEND_HL = rp_sym("friend_highlight");
    S_CLEAR_Y = rp_sym("clearyoffset");
    S_LIST_Y = rp_sym("list_y_offset");
    S_LIST_H = rp_sym("list_entry_height");
    S_W2C = rp_sym("world_to_canvas");
    S_PTR_START = rp_sym("delegate_OnStartIntersection");          /* R15PointerInterface, ev[2] = the actor under the ray */
    S_PTR_END = rp_sym("delegate_OnEndIntersection");
    S_PTR_HOVER = rp_sym("bhvr_delegate_OnPointerInteractHover");  /* CanvasUI hover */
    S_PTR_IDLE = rp_sym("bhvr_delegate_OnPointerInteractIdle");
    S_SCROLL_DN = rp_sym("rad_report_scroll_down");                /* public debug events: one row per event */
    S_SCROLL_UP = rp_sym("rad_report_scroll_up");
}

/* ---- tablet reasons surface (w23a) ids and hook flags; the surface itself is below rps_press ---- */
#define RP_GPROMPT 0x8a5dcee389df5b7cull     /* the tablet's generic prompt actor (r14_glb_global_mp) */
#define RPT_SETTLE_MS 300                   /* setupgenericprompt + transition first, then the layer goes over it */
#define RPT_CLOSE_MS 200                    /* the five-tile card's close press runs first, then our prompt set-up */
#define RPT_STATE_MS 100                    /* the tablet state-stack read period while the panel is up */
#define RPT_REACH_MS 3000                   /* generic_prompt not on top this long after the set-up: the panel gives up */
enum { RPT_Z_REASON1 = 0, RPT_Z_MG = RP_NREASON, RPT_Z_CANCEL, RPT_Z_SUBMIT, RPT_Z_BACK, RPT_Z_FOOTER, RPT_NZ };
static const char *const g_rpt_zone_name[RPT_NZ] = { "ffa_acr_reason1", "ffa_acr_reason2", "ffa_acr_reason3", "ffa_acr_reason4",
    "ffa_acr_reason5", "ffa_acr_reason6", "ffa_acr_mg", "ffa_acr_cancel", "ffa_acr_submit", "ffa_acr_back",
    "0x5071f3ac46630320" /* the stock modal footer zone on state_mgr: CANCEL here only before the layer draws  */ };
#define RPT_NSTOCK 7                        /* stock prims of 0x51d88ca2bae7aff8 the layer replaces (hidden while it shows) */
static const char *const g_rpt_stock_name[RPT_NSTOCK] = { "header", "messagetext", "0x6aaa715c85b07711", "cancelbackground",
    "cancelbox", "canceltext", "ui_ar_tablet_close_button" };
/* w23e: CANCEL / SUBMIT sit lower (Coffey's footer), over the state_mgr's stock menubutton4 / menubutton3 zones (68 px at
 * y 672..740). While the tablet surface owns the prompt every button event of menubutton1-4 on the tablet
 * actor is consumed (rewritten to the no-op, both chains, as the minted zones), so a touch on our CANCEL / SUBMIT runs
 * only our zone. Nothing is written to the engine: the gate is g_rpt_on, which every exit clears (rpt_close).
 * (real finger presses): the other stock state_mgr button zones that lie under the face are consumed the
 * same way. The stock footer 0x5071f3ac46630320 (px 398..651 x 645..731, between our CANCEL and SUBMIT) popped the
 * prompt (top 0) and 0x6567539d25efbdc0 (845..953 x 151..259, under / left of our "<"; listener delegate_OnPressStarted)
 * closed the tablet (top deactivated), both with no zone of ours. BackButton (under the chip) and the three title-band
 * dots 0x645c2234c519a275..277 are hidden by the layer too. The footer is consumed only once the layer is drawn
 * (g_rpt_drawn): before that (the set-up window, a layer that never draws) the stock prompt shows its own CANCEL and
 * the footer keeps the RPT_Z_FOOTER route (our CANCEL + the stock pop). swiperegion (the whole body, flat) is left alone. */
#define RPT_NMB 10
#define RPT_MB_FOOTER 4                     /* index of the stock footer: needs g_rpt_drawn */
#define RPT_NMB_EV 6
static const char *const g_rpt_mb_name[RPT_NMB] = { "menubutton1", "menubutton2", "menubutton3", "menubutton4",
    "0x5071f3ac46630320", "0x6567539d25efbdc0", "BackButton", "0x645c2234c519a275", "0x645c2234c519a276", "0x645c2234c519a277" };
static const char *const g_rpt_mb_label[RPT_NMB] = { "menubutton1", "menubutton2", "menubutton3", "menubutton4",
    "footer 0x5071f3ac46630320", "zone 0x6567539d25efbdc0", "BackButton", "dot 0x645c2234c519a275", "dot 0x645c2234c519a276",
    "dot 0x645c2234c519a277" };
static const char *const g_rpt_mb_ev_name[RPT_NMB_EV] = { "delegate_OnFullyDepressed", "delegate_OnPressed", "delegate_OnPress",
    "delegate_OnPressStarted", "delegate_OnRelease", "delegate_OnReleased" };
#define RPT_MB_EV_REL 4   /* g_rpt_mb_ev_name[4..5] = OnRelease / OnReleased: the press ends */
static uint64_t S_RPT_MB[RPT_NMB], S_RPT_MB_EV[RPT_NMB_EV];
static volatile int g_rpt_mb_ev = -1;       /* hook -> tick: the stock menubutton whose event was consumed (logged) */
static volatile int g_rpt_mb_rel = -1;      /* hook -> tick: the stock menubutton whose RELEASE event was consumed (ends the press) */
/*: the stock X sits under the layer's "<" (shared centre). Quest delivers the press as the X zone's events only, PC as
 * "<" then X. While the layer is drawn (or within RPT_X_GRACE_MS of a "<": the "<" close clears g_rpt_on before the X
 * events land) every X event is a no-op and the press is the "<" action, once. */
#define RPT_X_GRACE_MS 1000
#define RPT_X_GRACE_REL_MS 100   /* the closing press's release (X / menubutton out event) ends the grace this much later: the
                                  * release comes as two events (OnRelease, OnReleased) that may land in two ticks */
static volatile int g_rpt_x_ev;             /* hook -> tick: an X press-start event was consumed */
static volatile int g_rpt_x_rel;            /* hook -> tick: an X release event was consumed (ends the press) */
static volatile int g_rpt_x_grace;          /* a press closed the layer lately ("<" / CANCEL / footer): the stock zones under it
                                             * (X, menubutton1-4, footer) are still consumed for the rest of that press  */
static int g_rpt_x_latch;                   /* the X press in progress (one "<" + log per press) */
static uint64_t g_rpt_x_t, g_rpt_back_t;    /* tick time of the last X event / of the last "<" */
static uint64_t g_rpt_grace_rel_t;          /* tick time the closing press was released in the grace (0 = not yet) */
static uint64_t g_rpt_mb_t;                 /* tick time of the last consumed event of the press in g_rpt_mb_last */
static int g_rpt_mb_last = -1;              /* the press latch: its in / hold / out events log once; -1 = none */
#define RPT_MB_PRESS_GAP_MS 1000            /* no event this long = a lost out event: the next one is a new press */

/* ---- stock-off: the stock zones under a drawn layer are switched
 * OFF in the engine, so they get no press at all; the swallow above (S_RPT_MB, the X -> "<" mapping, g_rpt_x_grace) is the
 * FALLBACK, per zone, for a zone the engine route cannot hold (byte checks failed, zone not found, bit owned by another
 * user, fault). CR15ButtonInteractCS keeps a u16 word of disable reasons per component (EComponentEnableCategory, a
 * bitmask; a zone is live only when all bits are clear). rad-menu uses its own bit 0x80 (eComponentEnableCustom):
 * Disable(h, 0x80) holds a zone off whatever the stock does with its bit 0x1 (: user_list re-enables menubutton1-4
 * by clearing 0x1 only); Enable(h, 0x80) clears only our bit, so the zone returns to the stock's CURRENT state. Both are
 * idempotent. Applied when the layer is drawn, re-asserted every RPT_SO_PERIOD_MS while drawn (a re-created component =
 * a new handle: the stale record is dropped and the new one taken), released through rpt_close on every exit; zones with
 * hold set stay off RPT_SO_HOLD_MS after the close (the press-through window), then released. A zone whose word
 * already has 0x80 before we touch it is skipped (another user's bit; logged once) and keeps the swallow.
 * With the X held off, a press on the "<" spot reaches rad-menu's own "<" zone (ffa_acr_back) instead of the X (Quest
 * delivered only the X before): device check.
 * The platform file defines RP_HAVE_ZONE_OFF and, after this header (game thread, engine calls inside invoke_set / clear):
 *   rp_zone_ready(&why) -> 1 the Enable / Disable / lookup bytes and vtable slots matched (why = the reason when 0);
 *   rp_zone_find(game, actor, zone, &z) -> 1 found (z: gs, cs, handle, current reasons; verified by rp_zone_check),
 *     0 actor / zone not loaded, -1 route unavailable or fault;
 *   rp_zone_cat(game, &z, off) -> 1 Disable(h, 0x80) (off) / Enable(h, 0x80) run, 0 not run, -1 fault.
 * Pool layout, the same on PC 6323983201049540 and Quest 6528386917200980 (disasm-only both: PC EnableComponent 0x92bd10 /
 * DisableComponent 0x92b9e0, Quest 0x1d96298 / 0x1d96354): dense slot = u16 [[cs+0xe0] + (h & 0xffff) * 4], reasons =
 * u16 [[cs+0xf8] + dense * 2], CI = [cs+0x110] + dense * 0x190 (+0 data index u16), row = [[cs+0xd0]] + index * 0x128
 * (+0 zone name, +8 owner actor name). */
#define RP_SO_BIT        0x80
#define RPT_SO_HOLD_MS   1000               /* post-close hold of the hold zones (today's grace length) */
#define RPT_SO_PERIOD_MS 100                /* re-assert period while the layer is drawn */
#define RPT_SO_MAX       24
#define RP_BCS_DATA   0xd0u
#define RP_BCS_DENSE  0xe0u
#define RP_BCS_REASON 0xf8u
#define RP_BCS_CI     0x110u
#define RP_BCS_CI_STRIDE  0x190u
#define RP_BCS_ROW_STRIDE 0x128u
#define RP_BCS_MAXSLOT    4096u
typedef struct { uintptr_t gs, cs; uint32_t h; uint16_t reasons; } rp_zref;
typedef int (*rp_rd_fn)(void *dst, uintptr_t src, size_t n);   /* 0 = read */
#ifdef RP_HAVE_ZONE_OFF
static int rp_zone_ready(const char **why);
static int rp_zone_find(void *game, uint64_t actor, uint64_t zone, rp_zref *z);
static int rp_zone_cat(void *game, const rp_zref *z, int off);
#else
static int rp_zone_ready(const char **why) { *why = "no engine route on this platform"; return 0; }
static int rp_zone_find(void *game, uint64_t actor, uint64_t zone, rp_zref *z) { (void)game; (void)actor; (void)zone; (void)z; return -1; }
static int rp_zone_cat(void *game, const rp_zref *z, int off) { (void)game; (void)z; (void)off; return -1; }
#endif
/* 1 = handle h of button system cs is zone `zone` of actor `actor`; *reasons = its disable word (reads only) */
static int rp_zone_check(rp_rd_fn rd, uintptr_t cs, uint32_t h, uint64_t actor, uint64_t zone, uint16_t *reasons)
{
    uintptr_t dt = 0, ci0 = 0, dpp = 0, rows = 0, rs = 0;
    uint32_t d32 = 0;
    uint16_t di = 0;
    uint64_t row[2];
    if ((uint16_t)h == 0xffff || !cs) return 0;
    if (rd(&dt, cs + RP_BCS_DENSE, 8) || !dt || rd(&d32, dt + (uintptr_t)(h & 0xffff) * 4, 4)) return 0;
    uint16_t d = (uint16_t)d32;
    if (d >= RP_BCS_MAXSLOT) return 0;
    if (rd(&ci0, cs + RP_BCS_CI, 8) || !ci0 || rd(&di, ci0 + (uintptr_t)d * RP_BCS_CI_STRIDE, 2)
        || rd(&dpp, cs + RP_BCS_DATA, 8) || !dpp || rd(&rows, dpp, 8) || !rows
        || rd(row, rows + (uintptr_t)di * RP_BCS_ROW_STRIDE, 16)) return 0;
    if (row[0] != zone || row[1] != actor) return 0;
    if (rd(&rs, cs + RP_BCS_REASON, 8) || !rs || rd(reasons, rs + (uintptr_t)d * 2, 2)) return 0;
    return 1;
}
/* a declarative set per surface: {actor, zone name, label, hold} */
typedef struct { uint64_t actor; const char *zone; const char *label; int hold; const uint64_t *zone_var; } rp_so_def;
typedef struct { uintptr_t gs, cs; uint32_t h; int st; } rp_so_rec;   /* st 1 = off by us (bit 0x80 set by rad-menu) */
typedef struct {
    const char *name; const rp_so_def *def; int n;
    uint64_t sym[RPT_SO_MAX]; rp_so_rec rec[RPT_SO_MAX];
    volatile int own[RPT_SO_MAX];           /* hook reads: 1 = the engine holds this zone off (no swallow for it) */
    unsigned logged[RPT_SO_MAX];            /* per-entry once-logs (bits: RP_SO_LOG_*) */
    int holding, applied_n;
    uint64_t hold_until, next;
    const char *pfx;                        /* log prefix ("stock-off" when NULL) */
} rp_so_surface;
#define RPT_SO_PFX(s) ((s)->pfx ? (s)->pfx : "stock-off")
enum { RP_SO_LOG_FOREIGN = 1, RP_SO_LOG_MISSING = 2, RP_SO_LOG_FAIL = 4, RP_SO_LOG_STALE = 8 };
static int g_rp_so_engine = -1;             /* -1 not decided, 0 fallback swallow, 1 engine (rpt_so_start) */
static int g_rp_so_fault_logged;
/* the REPORT layer (ffa_ac_report on the generic prompt), finding Entries 0..RPT_NMB-1 mirror g_rpt_mb_name (the
 * swallow list, same order: the hook maps a swallow index to its entry). */
enum { RPT_SO_SWIPE = RPT_NMB, RPT_SO_X, RPT_SO_B5, RPT_SO_CARD_REST, RPT_SO_N = RPT_SO_CARD_REST + 5 };
static const rp_so_def g_rpt_so_def[RPT_SO_N] = {
    { RP_TABLET, "menubutton1", "menubutton1", 1, NULL }, { RP_TABLET, "menubutton2", "menubutton2", 1, NULL },
    { RP_TABLET, "menubutton3", "menubutton3", 1, NULL }, { RP_TABLET, "menubutton4", "menubutton4", 1, NULL },
    { RP_TABLET, "0x5071f3ac46630320", "footer 0x5071f3ac46630320", 1, NULL },
    { RP_TABLET, "0x6567539d25efbdc0", "zone 0x6567539d25efbdc0", 1, NULL },
    { RP_TABLET, "BackButton", "BackButton", 1, NULL },
    { RP_TABLET, "0x645c2234c519a275", "dot 0x645c2234c519a275", 1, NULL },
    { RP_TABLET, "0x645c2234c519a276", "dot 0x645c2234c519a276", 1, NULL },
    { RP_TABLET, "0x645c2234c519a277", "dot 0x645c2234c519a277", 1, NULL },
    { RP_TABLET, "0x556819fa696d48c4", "swiperegion 0x556819fa696d48c4", 1, NULL },
    { RP_TABLET_COMP, "0x58cba490dc70b632", "tablet X 0x58cba490dc70b632", 1, NULL },
    /* the closed card's REPORT zone: released at close (the back path re-opens the card) */
    { RP_CARD, "button5", "card button5", 0, &g_rp_card5_uid },
    /* device: the closed card's other zones stay live too (PC: button4 = ADD FRIEND en=1 beside the panel); released at close */
    { RP_CARD, "button1", "card button1", 0, NULL }, { RP_CARD, "button2", "card button2", 0, NULL },
    { RP_CARD, "button3", "card button3", 0, NULL }, { RP_CARD, "button4", "card button4", 0, NULL },
    { RP_CARD, "close_button", "card close_button", 0, NULL },
};
static rp_so_surface g_rpt_so = { "REPORT layer", g_rpt_so_def, RPT_SO_N, { 0 }, { { 0, 0, 0, 0 } }, { 0 }, { 0 }, 0, -1, 0, 0, NULL };
#define RPT_SO_OWN(i) (g_rpt_so.own[(i)] != 0)
/* rad-menu's OWN layer zones (ffa_ac_report on the generic prompt actor) are live only while the layer is drawn: hiding
 * acr_root does not switch its zones off, so on the result prompt (layer hidden, stock prims back) the invisible "<"
 * shared the corner with the stock X and ate it. Held off (bit 0x80) whenever the layer is not drawn (the guard, every
 * close), released in the pass that draws the layer. The footer entry of g_rpt_zone_name is the stock's: not listed. */
#define RPL_SO_N RPT_Z_FOOTER
static const rp_so_def g_rpl_so_def[RPL_SO_N] = {
    { RP_GPROMPT, "ffa_acr_reason1", "ffa_acr_reason1", 0, NULL }, { RP_GPROMPT, "ffa_acr_reason2", "ffa_acr_reason2", 0, NULL },
    { RP_GPROMPT, "ffa_acr_reason3", "ffa_acr_reason3", 0, NULL }, { RP_GPROMPT, "ffa_acr_reason4", "ffa_acr_reason4", 0, NULL },
    { RP_GPROMPT, "ffa_acr_reason5", "ffa_acr_reason5", 0, NULL }, { RP_GPROMPT, "ffa_acr_reason6", "ffa_acr_reason6", 0, NULL },
    { RP_GPROMPT, "ffa_acr_mg", "ffa_acr_mg", 0, NULL }, { RP_GPROMPT, "ffa_acr_cancel", "ffa_acr_cancel", 0, NULL },
    { RP_GPROMPT, "ffa_acr_submit", "ffa_acr_submit", 0, NULL }, { RP_GPROMPT, "ffa_acr_back", "ffa_acr_back", 0, NULL },
};
static rp_so_surface g_rpl_so = { "layer zones", g_rpl_so_def, RPL_SO_N, { 0 }, { { 0, 0, 0, 0 } }, { 0 }, { 0 }, 0, -1, 0, 0, "layer-off" };
/* the five-tile card's tiles while the card is CLOSED: the stock close thread leaves button5 (and on PC button4) live with
 * nothing drawn beside the LOBBY panel, and a press there opened REPORT for the last target. Open signal: the card's
 * close_button stock-enabled (its reasons word without our bit is 0). Held off while closed, released once open; skipped
 * while the REPORT layer owns the tablet (g_rpt_so holds the card zones then). close_button itself is never held here. */
#define RPC_SO_N 5
#define RPC_POLL_MS 100
static const rp_so_def g_rpc_so_def[RPC_SO_N] = {
    { RP_CARD, "button5", "card button5", 0, &g_rp_card5_uid }, { RP_CARD, "button1", "card button1", 0, NULL },
    { RP_CARD, "button2", "card button2", 0, NULL }, { RP_CARD, "button3", "card button3", 0, NULL },
    { RP_CARD, "button4", "card button4", 0, NULL },
};
static rp_so_surface g_rpc_so = { "closed card", g_rpc_so_def, RPC_SO_N, { 0 }, { { 0, 0, 0, 0 } }, { 0 }, { 0 }, 0, -1, 0, 0, "card-off" };
/* the OPEN card's REPORT tile on a player the session reader cannot match (the card shows warning_message): the label is
 * hidden as the stock hides its own labels there, and button5 is held off so the tile no longer acts. Held from that open,
 * released once the card closes or its state is unknown (the closed-card set takes the tiles over), and by a matched open. */
static const rp_so_def g_rpw_so_def[1] = { { RP_CARD, "button5", "card button5", 0, &g_rp_card5_uid } };
static rp_so_surface g_rpw_so = { "warning card", g_rpw_so_def, 1, { 0 }, { { 0, 0, 0, 0 } }, { 0 }, { 0 }, 0, -1, 0, 0, "warn-off" };
static int rpt_so_apply(void *game, rp_so_surface *s, uint64_t now);
static void rpt_so_release(void *game, rp_so_surface *s, uint64_t now, const char *why, int hold);
static volatile int g_rp_card_open = -1;    /* hook reads: 1 the card is open, 0 closed, -1 unknown (no engine route / not loaded) */
static volatile int g_rp_ev_card_closed;    /* hook -> tick: a button5 event while the card is closed (dropped) */
static uint64_t g_rpc_next;
/* The result prompt (rp_prompt on the tablet). The stock OK pops one state, but rad-menu reaches generic_prompt by
 * transitionarmcomputertostate (the top replaced, nothing pushed under it), so the pop left an empty stack (a blank
 * tablet frame). While it is up, the OK (the stock footer) is consumed and rad-menu transitions to user_list itself. */
#define RPT_RES_SETTLE_MS 500               /* the result prompt's transition runs over frames: the top is checked after this */
static volatile int g_rpt_res, g_rpt_ok_ev; /* the result prompt is up / hook -> tick: its OK pressed */
static uint64_t g_rpt_res_next;
static uint64_t g_rp_now;                   /* the tick time (report_tick): rpt_close / rp_flush have no clock argument */
static uint64_t S_RPT_ZONE[RPT_NZ], S_RPT_STOCK[RPT_NSTOCK], S_ACR_SEL[RP_NREASON], S_ACR_ROOT, S_ACR_SENT, S_ACR_MG, S_ACR_MG_CHECK,
    S_ACR_SUB_BG, S_ACR_SUB_TEXT, S_ACR_SUB_FILL, S_FULLY_DEP, S_ST_USER_LIST, S_ACR_SENT_L[2], S_ACR_MG_TEXT, S_ST_PROMPT_T;
static volatile int g_rpt_on;               /* the tablet surface owns the prompt (hook reads it) */
static volatile int g_rpt_drawn;            /* the layer was drawn over the prompt since rpt_open (hook reads it: footer) */
static volatile int g_rpt_ev = -1;          /* hook -> tick: the zone pressed */
static int g_rpt_bound = -1, g_rpt_post, g_rpt_pass, g_rpt_last_z = -1, g_rpt_fallback_logged, g_rpt_two,
    g_rpt_fill;   /* the canvas has acr_submit_fill (w23b on; probed once per prompt load: Quest logs a missing element) */
static int g_rpt_seen, g_rpt_top_off_logged, g_rpt_orphan;   /* generic_prompt seen on top; reader-off logged; a POST whose panel left */
static uintptr_t g_rpt_key;
static uint64_t g_rpt_since, g_rpt_next, g_rpt_last_t, g_rpt_setup_at, g_rpt_setup_t, g_rpt_state_next, g_rpt_guard_next, g_rpt_guard_hide;
/*: BACK = ONE canvas step. The X in the corner is the stock close-tablet; Coffey's "<" (and the stock X under
 * the drawn layer) goes back one step: the layer closes, the tablet returns to user_list (pending 4), then the report
 * target's SOCIAL CONTROLS card is opened again. CANCEL lands on the same step by default: Coffey's tablet reasons CANCEL
 * -> #14 / #25 = the LOBBY list with the target's side card open (coffey-report-player-xd-walk-2026-11-01.md graph and
 * #16 / #28; the pause popups' CANCEL -> #03 instead, rpp_close); report_tablet_cancel=list keeps the older landing
 * (list, card closed). PRIMARY re-open (direct): the card's STRING xplatformid is cleared (rp_setvar; PC and Quest
 * CScriptCS::SetString), the card actor's ncaScript OpenLobbyMemberSocialControls p_userid=<slot recorded at the REPORT
 * press> is called, and a few ticks later the card's xplatformid must equal the string recorded at the press (success,
 * "direct"); empty / unreadable after RPT_BK_DIRECT_MS and RPT_BK_DIRECT_TICKS = no card, a different id = another player's card (closed again);
 * either falls back ONCE to the closed-loop candidate walk below. Without a setter the clear is skipped and an equal
 * read-back is accepted with a stale-value note. Unknown slot / empty recorded id = candidate walk only.
 * Candidate walk (rpt_back_tick): state_mgr.usernamepressed pressed_user_index=<k> for candidate
 * rows (the card target's users[] index, its slot, then rows 1..RP_MAXROWS-1, row 0 = the local player last; the VISIBLE
 * row is the argument), the card's xplatformid read back after each call (the call is DEFERRED: the first
 * reads may still show the value from before it), and a card showing another account is closed again (its close_button)
 * before the next candidate. After the last candidate the LOBBY list stays open, logged; no target id = no call. */
#define RPT_BK_LIST_MS  300   /* after the user_list transition ran, before the first usernamepressed */
#define RPT_BK_READ_MS  100   /* first readback after a call */
#define RPT_BK_STALE_MS 600   /* a readback still equal to the value before the call is judged only after this long */
#define RPT_BK_DIRECT_MS    2000   /* the direct open: an empty read-back is judged only after this long ... */
#define RPT_BK_DIRECT_TICKS 20     /* ... AND this many ticks (frames): a low frame rate defers the card's open thread */
#define RPT_BK_CLOSE_MS 200   /* a wrong card's close press runs first, then the next candidate */
#define RPT_BK_BUSY_MAX 60    /* busy state_mgr: retried on the next ticks, at most this many */
#define RPT_BK_NCAND    (RP_MAXROWS + 2)
enum { RPT_BK_OFF, RPT_BK_WAIT, RPT_BK_CALL, RPT_BK_READ, RPT_BK_DCALL, RPT_BK_DREAD };
static int g_rpt_bk_direct, g_rpt_bk_cleared, g_rpt_bk_dticks;   /* + ticks since the direct call */   /* the direct path is available / the card's id was cleared before its call */
static uint64_t S_OPEN_LMSC, S_P_USERID;
static char g_rpt_bk_xp[RP_NAMELEN];        /* the card's full xplatformid string at the REPORT press ("" = unknown) */
static int g_rpt_bk_xp_retry;               /* unreadable at the press: read again at the next card poll / back start */
static int g_rpt_bk;                        /* the back re-open step (RPT_BK_*) */
static int g_rpt_bk_cand[RPT_BK_NCAND], g_rpt_bk_nc, g_rpt_bk_k, g_rpt_bk_tries, g_rpt_bk_busy, g_rpt_bk_pre_ok;
static int g_rpt_bk_i = -1, g_rpt_bk_slot = -1;   /* the card target's users[] index / slot at the REPORT press (-1 unknown) */
static uint64_t g_rpt_bk_acct, g_rpt_bk_t, g_rpt_bk_call_t, S_USERNAME_PRESSED, S_PRESSED_USER_IDX, S_ST_PROFILE;
static char g_rpt_bk_pre[RP_NAMELEN];       /* the card's xplatformid read just before the current call */
static char g_rpt_bk_id[RP_IDLEN];          /* the target's user id (log: last 4) */
static const char *g_rpt_bk_by = "";        /* the press that went back: "\"<\"" / CANCEL / stock footer */
static volatile int g_rpt_bk_user;          /* hook -> tick: a press on the tablet's own zones (a LOBBY row) during the back walk */
/* fallback mode only (no engine route): our "<" pressed while no layer is drawn sits on the stock X: the X is sent for it */
static volatile int g_rpt_xfwd;
static uint64_t g_rpt_xfwd_t, S_ON_PRESS;

/* ---- hook side (any thread): flags only, plus the in-place rewrite of a consumed card press ---- */
static int report_event(const uint64_t *ev, int target, void *gs)
{
    uint64_t e0 = ev[0];
    if (g_rp_so_engine != 1 && !g_rpt_on && !g_rpt_x_grace && S_FULLY_DEP && e0 == S_FULLY_DEP && S_RPT_ZONE[RPT_Z_BACK]
        && ev[3] == S_RPT_ZONE[RPT_Z_BACK]) {   /* ahead of the report= gate: report=0 / a failed check leave our zones live too */
        RP_XCHG(&g_rpt_xfwd, 1);
        return target && S_RP_NOOP ? rp_rewrite(ev, S_RP_NOOP) : 0;
    }
    if ((g_rp_on <= 0 && g_rp_live != 1) || !g_rp_ready) return 0;   /* auto + stock tree: every event passes untouched */
    for (int i = 0; i < RP_MAXROWS; i++)
        if (e0 == S_PRESSED[i]) { RP_XCHG(&g_rp_ev_press, i); return 0; }
    if (g_rp_tile_uid && e0 == S_BTN_CLICKED && ev[3] == g_rp_tile_uid) {
        RP_XCHG(&g_rp_ev_tile, 1);
        return target ? rp_rewrite(ev, S_RP_NOOP) : 0;   /* the tile has no stock route; keep its click out of p1 */
    }
    if (g_rpp_page == RPP_LIST && g_rpp_scroll_mode != RPP_SCR_OFF) {   /* list scroll inputs (observed only, never consumed) */
        if (e0 == S_SCROLL_DN || e0 == S_SCROLL_UP) { RP_XCHG(&g_rpp_scr_ev, e0 == S_SCROLL_DN ? 1 : -1); return 0; }
        if (e0 == S_PTR_START || e0 == S_PTR_HOVER || e0 == S_PTR_END || e0 == S_PTR_IDLE) {
            uint64_t a = rp_actor(gs, ev[2]);
            int on = e0 == S_PTR_START || e0 == S_PTR_HOVER;
            int dir = a == g_rpp_actor[RPP_PEEK] ? 1 : a == g_rpp_actor[RPP_B_ROW1] ? -1 : 0;
            if (dir) { if (on) RP_XCHG(&g_rpp_hov, dir); else if (g_rpp_hov == dir) RP_XCHG(&g_rpp_hov, 0); }
            else if (on && g_rpp_hov) RP_XCHG(&g_rpp_hov, 0);   /* the pointer moved onto another actor */
            return 0;
        }
    }
    if (g_rpp_on && e0 == g_rpp_event) {                 /* popup buttons (minted actors: no stock route) */
        for (int i = 0; i < RPP_NB; i++)
            if (ev[3] == g_rpp_uid[i]) { RP_XCHG(&g_rpp_ev, i); return target ? rp_rewrite(ev, S_RP_NOOP) : 0; }
    }
    if (g_rpt_on || g_rpt_x_grace || g_rpt_res) {   /* w23e +: the stock state_mgr zones under the layer do nothing while it owns the
                                        * prompt, and  for the rest of the press that closed it: CANCEL sits over menubutton4,
                                        * and a finger pushed through CANCEL pressed the LOBBY home tab once the layer had gone */
        for (int m = 0; m < RPT_NMB; m++)
            if (ev[3] == S_RPT_MB[m]) {
                int be = 0, rel = 0;
                for (int k = 0; k < RPT_NMB_EV; k++) if (e0 == S_RPT_MB_EV[k]) { be = 1; rel |= k >= RPT_MB_EV_REL; }
                if (!be || rp_actor(gs, ev[2]) != RP_TABLET) break;
                if (RPT_SO_OWN(m)) break;   /* stock-off: the engine holds this zone off; the swallow is its fallback only */
                if (!g_rpt_on && !g_rpt_x_grace) {   /* the result prompt: only its OK (the stock footer) is taken */
                    if (m != RPT_MB_FOOTER) break;
                    if (!rel) RP_XCHG(&g_rpt_ok_ev, 1);
                    return rp_rewrite(ev, S_RP_NOOP);
                }
                if (m == RPT_MB_FOOTER && !g_rpt_drawn && !g_rpt_x_grace) break;   /* the bare stock prompt: its footer stays the exit */
                RP_XCHG(&g_rpt_mb_ev, m);
                if (rel) RP_XCHG(&g_rpt_mb_rel, m);
                return rp_rewrite(ev, S_RP_NOOP);
            }
    }
    if (ev[3] == RP_TABLET_X && !RPT_SO_OWN(RPT_SO_X) && ((g_rpt_on && g_rpt_drawn) || g_rpt_x_grace)) {
        /*: the stock X under the layer = "<" (fallback: with stock-off the X is off and "<" gets the press) */
        int xe = 0, rel = 0;
        for (int k = 0; k < RPT_NMB_EV; k++) if (e0 == S_RPT_MB_EV[k]) { xe = 1; rel = k >= RPT_MB_EV_REL; }
        if (xe && rp_actor(gs, ev[2]) == RP_TABLET_COMP) {
            RP_XCHG(rel ? &g_rpt_x_rel : &g_rpt_x_ev, 1);
            return rp_rewrite(ev, S_RP_NOOP);
        }
    }
    if (g_rpt_bk != RPT_BK_OFF && e0 == S_FULLY_DEP && rp_actor(gs, ev[2]) == RP_TABLET)
        RP_XCHG(&g_rpt_bk_user, 1);   /* the user took the LOBBY list during the back walk (a swallowed press-through returned above) */
    if (e0 == S_FULLY_DEP && (g_rpt_on || g_rpt_bound == 1)) {   /* tablet reasons zones (w23a, generic prompt actor) */
        for (int i = 0; i < RPT_NZ; i++)
            if (ev[3] == S_RPT_ZONE[i]) {
                if (i == RPT_Z_FOOTER) { if (g_rpt_on) RP_XCHG(&g_rpt_ev, i); return 0; }   /* the stock route runs too */
                RP_XCHG(&g_rpt_ev, i);
                return rp_rewrite(ev, S_RP_NOOP);   /* minted zones: no stock route */
            }
    }
    if (g_rp_five == 1) {   /* five-tile card: REPORT = button5; INVITE / ADD FRIEND keep their stock actions */
        if (e0 == g_rp_card5_event && ev[3] == g_rp_card5_uid) {
            if (g_rp_card_open == 0) { RP_XCHG(&g_rp_ev_card_closed, 1); return rp_rewrite(ev, S_RP_NOOP); }   /* nothing drawn there */
            RP_XCHG(&g_rp_ev_card, 1);
            return rp_rewrite(ev, S_RP_NOOP);
        }
        if (g_rp_addfriend && rp_actor(gs, ev[2]) == RP_CARD) {   /* a card event = the card is (still) open */
            if (ev[3] == S_CLOSE_BTN) RP_XCHG(&g_rp_f4_close, 1);
            else RP_XCHG(&g_rp_f4_act, 1);
        }
        if (g_rp_learn5_on && g_rp_learn5 < 32 && rp_actor(gs, ev[2]) == RP_CARD) {
            g_rp_learn5++;
            rp_log("report: five-tile card event %016llx uid %016llx (%s)%s", (unsigned long long)e0,
                   (unsigned long long)ev[3], target ? "target" : "core",
                   ev[3] == g_rp_card5_uid ? ": uid = report_card5_uid, set report_card5_event to this event" : "");
        }
        return 0;
    }
    if (g_rp_card_event && e0 == g_rp_card_event && (!g_rp_card_uid || ev[3] == g_rp_card_uid)
        && (g_rp_card_uid || rp_actor(gs, ev[2]) == RP_CARD)) {
        RP_XCHG(&g_rp_ev_card, 1);
        return rp_rewrite(ev, S_RP_NOOP);                /* never let the party-invite route run */
    }
    if (!g_rp_card_event && g_rp_learned < 32 && rp_actor(gs, ev[2]) == RP_CARD) {   /* ev[2] = SActor handle, not the name */
        g_rp_learned++;
        rp_log("report: card event %016llx uid %016llx (%s; set report_card_event to consume one)",
               (unsigned long long)e0, (unsigned long long)ev[3], target ? "target" : "core");
    }
    return 0;
}

/* ---- host fallback JSON (worker data, parsed on the tick) ---- */
/* Copies the JSON string starting after the opening quote at p into out (escapes kept as the char after '\\');
 * returns the position after the closing quote. */
static const char *rp_jstr(const char *p, char *out, size_t cap)
{
    size_t n = 0;
    for (; *p && *p != '"'; p++) {
        if (*p == '\\' && p[1]) p++;
        if (n + 1 < cap) out[n++] = (*p < 0x20 || *p > 0x7e) ? '?' : *p;
    }
    out[n] = 0;
    return *p ? p + 1 : p;
}

/* Walks the host's GET /reports/players JSON with a depth counter: the top-level session_id / match_type, and the
 * display_name and user_id of every object directly inside a "players" array (the reporter is already left out).
 * Fills the rows (at most RP_ROWCAP). */
static void rp_parse_host(const char *js)
{
    char key[RP_NAMELEN] = "", val[RP_NAMELEN], self[RP_NAMELEN] = "", pname[RP_NAMELEN] = "", pid[RP_IDLEN] = "";
    int depth = 0, players_depth = -1, obj_depth = -1, expect_val = 0;
    g_rp_nrows = 0;
    g_rp_sess_id[0] = g_rp_sess_type[0] = 0;
    for (const char *p = js; *p; ) {
        char c = *p;
        if (c == '"') {
            p = rp_jstr(p + 1, val, sizeof val);
            if (!expect_val) {
                const char *q = p;
                while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n') q++;
                if (*q == ':') { snprintf(key, sizeof key, "%s", val); expect_val = 1; p = q + 1; continue; }
            } else {
                if (depth == 1 && (!strcmp(key, "sessionid") || !strcmp(key, "session_id"))) snprintf(g_rp_sess_id, sizeof g_rp_sess_id, "%s", val);
                else if (depth == 1 && !strcmp(key, "match_type")) snprintf(g_rp_sess_type, sizeof g_rp_sess_type, "%s", val);
                else if (depth == 1 && !strcmp(key, "client_name")) snprintf(self, sizeof self, "%s", val);
                else if (obj_depth >= 0 && depth == obj_depth && (!strcmp(key, "name") || !strcmp(key, "display_name")))
                    snprintf(pname, sizeof pname, "%s", val);
                else if (obj_depth >= 0 && depth == obj_depth && (!strcmp(key, "userid") || !strcmp(key, "user_id")))
                    snprintf(pid, sizeof pid, "%s", val);
                expect_val = 0;
            }
            continue;
        }
        if (c == '{' || c == '[') {
            if (c == '[' && expect_val && !strcmp(key, "players")) players_depth = depth + 1;
            else if (c == '{' && players_depth >= 0 && depth == players_depth) { obj_depth = depth + 1; pname[0] = pid[0] = 0; }
            depth++;
            expect_val = 0;
        } else if (c == '}' || c == ']') {
            if (c == '}' && obj_depth >= 0 && depth == obj_depth) {
                if (pname[0] && strcmp(pname, self) && g_rp_nrows < RP_ROWCAP) {
                    snprintf(g_rp_rows[g_rp_nrows], RP_NAMELEN, "%s", pname);
                    snprintf(g_rp_rowid[g_rp_nrows], sizeof g_rp_rowid[0], "%s", pid);
                    g_rp_rowi[g_rp_nrows] = g_rp_rowslot[g_rp_nrows] = -1;
                    g_rp_rowfake[g_rp_nrows] = 0;
                    g_rp_nrows++;
                }
                obj_depth = -1;
            }
            if (c == ']' && players_depth >= 0 && depth == players_depth) players_depth = -1;
            depth--;
            expect_val = 0;
        } else if (expect_val && (c == '-' || (c >= '0' && c <= '9') || c == 't' || c == 'f' || c == 'n')) {
            size_t n = 0;
            while (*p && *p != ',' && *p != '}' && *p != ']' && n + 1 < sizeof val) val[n++] = *p++;
            val[n] = 0;
            if (obj_depth >= 0 && depth == obj_depth && !strcmp(key, "userid")) snprintf(pid, sizeof pid, "%s", val);
            expect_val = 0;
            continue;
        }
        p++;
    }
}

/* JSON string escape for the POST body */
static void rp_jesc(char *out, size_t cap, const char *s)
{
    size_t n = 0;
    for (; *s && n + 7 < cap; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { out[n++] = '\\'; out[n++] = (char)c; }
        else if (c < 0x20) n += (size_t)snprintf(out + n, cap - n, "\\u%04x", c);
        else out[n++] = (char)c;
    }
    out[n] = 0;
}

/* ---- worker (its own thread, a step every ~50 ms) ---- */
static void report_worker_step(void)
{
    int gen = g_rp_job_gen;
    int job = g_rp_job;
    if (job == RP_JOB_NONE || g_rp_job_done) return;
    int st;
    if (job == RP_JOB_HOST) st = rp_http("GET", 0, g_rp_players_path, NULL, g_rp_resp, sizeof g_rp_resp);
    else st = rp_http("POST", 0, "/reports", g_rp_post_body, g_rp_resp, sizeof g_rp_resp);
    if (gen != g_rp_job_gen) return;   /* the flow was reset (or restarted) while the request ran: this reply is not for it */
    g_rp_job_status = st;
    RP_XCHG(&g_rp_job_done, 1);
}

/* GET /reports/players[?user_id=<the logged-in user, url-escaped>]; game thread only (rp_user). */
static void rp_players_path(char *out, size_t cap)
{
    char user[96] = "";
    size_t n = (size_t)snprintf(out, cap, "/reports/players");
    if (rp_user(user, sizeof user) && user[0]) {
        n += (size_t)snprintf(out + n, cap - n, "?user_id=");
        for (const char *u = user; *u && n + 4 < cap; u++) {
            unsigned char c = (unsigned char)*u;
            if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '-' || c == '_' || c == '.')
                out[n++] = (char)c;
            else n += (size_t)snprintf(out + n, cap - n, "%%%02X", c);
        }
        out[n] = 0;
    }
}

static void rp_start_job(int job)
{
    if (job == RP_JOB_HOST)   /* rp_user on the game thread (as rp_submit); the worker only reads the path */
        rp_players_path(g_rp_players_path, sizeof g_rp_players_path);
    RP_XCHG(&g_rp_job_gen, g_rp_job_gen + 1);
    g_rp_job_done = 0;
    g_rp_job_status = 0;
    RP_XCHG(&g_rp_job, job);
}

/* ---- tablet calls (game thread) ---- */
/* fallback F's page: clearyoffset first (setupcustomuserlist keeps the scroll, and a press index includes it), then the
 * rows, their selectable flags and the transition. 0 shown, 1 busy (retry), -1 refused */
static int rp_show_list(void *game, const char *const *rows, int n)
{
    int32_t num = n;
    int rc = rp_call_on(game, RP_TABLET, RP_NCA_SCRIPT, S_CLEAR_Y, 0, NULL, NULL);
    if (rc == 1) return rc;
    if (rc) rp_log("report: clearyoffset refused: the press guard drops presses while the list is scrolled");
    uint64_t names[1 + RP_MAXROWS];
    const void *vals[1 + RP_MAXROWS];
    static char buf[RP_MAXROWS][64];
    names[0] = S_NUM_USERS; vals[0] = &num;
    for (int i = 0; i < n && i < RP_MAXROWS; i++) {
        memset(buf[i], 0, sizeof buf[i]);
        snprintf(buf[i], sizeof buf[i], "%s", rows[i]);
        names[1 + i] = S_NAME[i]; vals[1 + i] = buf[i];
    }
    rc = rp_call(game, S_SETUP_LIST, 1 + n, names, vals);
    if (rc) return rc;
    /* a custom row raises user_list_custom_user_<n>_pressed only when its selectable flag is set; the flags start false
     * and only setupcustomuserlistselectable sets them (a row has 5 press gates) */
    static uint8_t sel[RP_MAXROWS];
    const void *svals[RP_MAXROWS];
    for (int i = 0; i < RP_MAXROWS; i++) { sel[i] = i < n; svals[i] = &sel[i]; }
    rc = rp_call(game, S_SETUP_SEL, RP_MAXROWS, S_SEL, svals);
    if (rc) return rc;
    uint64_t st = S_ST_LIST;
    const void *sv = &st;
    return rp_call(game, S_TRANSITION, 1, &S_STATE, &sv);
}

static int rp_show_prompt_as(void *game, const char *msg, const char *back)
{
    static char h[64], m[64], b[64];
    uint8_t en = 1;
    memset(h, 0, sizeof h); memset(m, 0, sizeof m); memset(b, 0, sizeof b);
    snprintf(h, sizeof h, "REPORT"); snprintf(m, sizeof m, "%s", msg); snprintf(b, sizeof b, "%s", back);
    uint64_t names[4] = { S_P_HEADER, S_P_MESSAGE, S_P_BACK, S_P_BACK_EN };
    const void *vals[4] = { h, m, b, &en };
    int rc = rp_call(game, S_SETUP_PROMPT, 4, names, vals);
    if (rc) return rc;
    uint64_t st = S_ST_PROMPT;
    const void *sv = &st;
    return rp_call(game, S_TRANSITION, 1, &S_STATE, &sv);
}
static int rp_show_prompt(void *game, const char *msg) { return rp_show_prompt_as(game, msg, "OK"); }

/* The target's display name for a row (mixed case, as Coffey's card): control chars dropped, spaces trimmed, cut to
 * RP_HDRLEN-1 bytes on a UTF-8 boundary; "PLAYER" when empty. */
#define RP_HDRLEN 32                         /* bytes incl. NUL */
static void rp_target_text(char *out, size_t cap)
{
    size_t n = 0;
    const char *s = g_rp_target;
    while (*s == ' ') s++;
    for (; *s && n + 1 < cap && n + 1 < RP_HDRLEN; s++)
        if ((unsigned char)*s >= 0x20 && *s != 0x7f) out[n++] = *s;
    while (n && out[n - 1] == ' ') n--;
    if (n && (unsigned char)out[n - 1] >= 0x80) {   /* a cut multi-byte sequence: drop it whole */
        size_t i = n;
        while (i && ((unsigned char)out[i - 1] & 0xc0) == 0x80) i--;
        if (i && (unsigned char)out[i - 1] >= 0xc0) {
            unsigned char lead = (unsigned char)out[i - 1];
            size_t need = lead >= 0xf0 ? 4 : lead >= 0xe0 ? 3 : 2;
            if (n - (i - 1) < need) n = i - 1;
        }
    }
    out[n] = 0;
    if (!n) snprintf(out, cap, "PLAYER");
}

/* The pause / tablet sentence (same wording on every surface): ONE source for every surface, the name as rp_target_text gives
 * it (control characters dropped, at most 31 bytes cut on a UTF-8 boundary, "PLAYER" when empty). */
static void rp_sentence(char *out, size_t cap)
{
    char nm[RP_HDRLEN];
    rp_target_text(nm, sizeof nm);
    snprintf(out, cap, RP_SENTENCE_FMT, nm);
}

/* The pause reasons page (p23): its sentence is ONE text primitive (one line, no break) about 77 code points wide at
 * 18 px, and RP_SENTENCE_FMT's words take 60. A name longer than RPP_SENT_NAME_CP code points is cut on a code point
 * boundary and ends in "...", so the line never clips; the words stay the tablet's (parity: only the name is cut). */
#define RPP_SENT_NAME_CP 18   /* an 18-code-point name was seen to fit on the device */
static void rpp_sentence(char *out, size_t cap)
{
    char nm[RP_HDRLEN];
    size_t n = 0, cp = 0, cut = 0;
    rp_target_text(nm, sizeof nm);
    for (; nm[n]; n++)
        if (((unsigned char)nm[n] & 0xc0) != 0x80 && cp++ == RPP_SENT_NAME_CP - 3) cut = n;   /* where the 3 dots go */
    if (cp > RPP_SENT_NAME_CP) {
        while (cut && nm[cut - 1] == ' ') cut--;
        snprintf(nm + cut, sizeof nm - cut, "...");
    }
    snprintf(out, cap, RP_SENTENCE_FMT, nm);
}

static void rp_reset(const char *why)
{
    if (g_rp_state != RP_IDLE) rp_log("report: flow reset (%s)", why);
    g_rp_state = RP_IDLE;
    g_rp_pending = 0;
    g_rp_retry = 0;
    g_rp_entry = "";
    RP_XCHG(&g_rp_job_gen, g_rp_job_gen + 1);
    RP_XCHG(&g_rp_job, RP_JOB_NONE);
}

/* ---- fallback F: the reason step as ONE 4-row custom page of RP_ROWCHARS characters, Coffey's dropdown field without
 * its open list (a list of 6 reasons + BACK needs 7 press gates; the custom list has 5, indexes 0-4 only):
 *   Report <name>: · Select Reason > (the pick once made; each press steps to the next of the 6 keys, wrapping: a
 *   selector, not a page) · SUBMIT · CANCEL (report_mute_ghost=1: [check] in place of CANCEL; the tablet's back cancels)
 * Every row has a kind (g_rp_gkind) and presses dispatch by kind. SUBMIT is ignored until a pick (pause: dim SUBMIT, same
 * rule); the sentence row's press does nothing. A press is dropped while the list is scrolled (rp_press_guard). ---- */
static void rp_submit(int mg);
static void rp_prompt(const char *msg);
enum { RP_GK_NONE, RP_GK_SENT, RP_GK_MG, RP_GK_SUBMIT, RP_GK_CANCEL, RP_GK_FIELD };
static uint8_t g_rp_gkind[RP_MAXROWS];

/* cuts s to RP_ROWCHARS bytes in place, on a UTF-8 boundary (a cut multi-byte sequence is dropped whole) */
static void rp_row_cut(char *s)
{
    size_t n = strlen(s);
    if (n <= RP_ROWCHARS) return;
    n = RP_ROWCHARS;
    while (n && ((unsigned char)s[n] & 0xc0) == 0x80) n--;   /* s[n] continues a sequence: back to its lead byte */
    while (n && s[n - 1] == ' ') n--;
    s[n] = 0;
}

/* a row naming the target: fmt with the name as the card header shows it, cut so the whole row fits */
static void rp_name_row(char *out, size_t cap, const char *fmt)
{
    char nm[RP_HDRLEN];
    size_t fixed = strlen(fmt) - 2, room = RP_ROWCHARS - fixed + 1;   /* the name's bytes + NUL */
    rp_target_text(nm, room < sizeof nm ? room : sizeof nm);
    snprintf(out, cap, fmt, nm);
    rp_row_cut(out);
}

static void rp_grow_add(int *n, int kind, const char *fmt, const char *a, const char *b)
{
    if (*n >= RP_MAXROWS) return;
    g_rp_gkind[*n] = (uint8_t)kind;
    snprintf(g_rp_grow[*n], RP_NAMELEN, fmt, a, b);
    rp_row_cut(g_rp_grow[*n]);
    (*n)++;
}

static void rp_grow_name(int *n, int kind, const char *fmt)
{
    if (*n >= RP_MAXROWS) return;
    g_rp_gkind[*n] = (uint8_t)kind;
    rp_name_row(g_rp_grow[*n], RP_NAMELEN, fmt);
    (*n)++;
}

static void rp_grow_mg(int *n)
{
    if (g_rp_mgshow) rp_grow_add(n, RP_GK_MG, "%s %s", g_rp_gmg ? "[x]" : "[ ]", g_rp_mg_label);
}

static void rp_field_rows(void)
{
    int n = 0;
    rp_grow_name(&n, RP_GK_SENT, RP_SENTENCE_ROW_FMT);
    rp_grow_add(&n, RP_GK_FIELD, "%s%s", g_rp_gsel < 0 ? "Select Reason" : g_rp_reason_row[g_rp_gsel], " >");
    rp_grow_add(&n, RP_GK_SUBMIT, "%s%s", "SUBMIT", "");
    if (g_rp_mgshow) rp_grow_mg(&n);   /* 4 rows: the check or CANCEL (the tablet's back also cancels) */
    else rp_grow_add(&n, RP_GK_CANCEL, "%s%s", "CANCEL", "");
    g_rp_gn = n;
}

/* fallback F's entry: no reason picked, MUTE & GHOST off */
static void rp_enter_field(uint64_t now)
{
    g_rp_since = now;
    g_rp_retry = 0;
    g_rp_state = RP_FIELD;
    g_rp_gsel = -1;
    g_rp_gmg = 0;
    g_rp_pending = 1;
}

/* 1 = drop a fallback F press. The custom rows do not move with the scroll, but a press index includes it ( N4 + N5),
 * so a press while |list_y_offset| > 0.1 x the row pitch (list_entry_height / world_to_canvas) may name another row.
 * A REAL read not run (rp_getreal 0: bytes differ, actor not loaded) or a pitch <= 0: guard off. */
static int rp_press_guard(void *game)
{
    float y = 0, h = 0, w = 0;
    if (!rp_getreal(game, RP_TABLET, RP_NCA_SCRIPT, S_LIST_Y, &y) || !rp_getreal(game, RP_TABLET, RP_NCA_SCRIPT, S_LIST_H, &h)
        || !rp_getreal(game, RP_TABLET, RP_NCA_SCRIPT, S_W2C, &w) || !(h > 0) || !(w > 0)) return 0;
    float pitch = h / w;
    if ((y < 0 ? -y : y) <= 0.1f * pitch) return 0;
    rp_log("report: field press dropped: list scrolled (list_y_offset %.4f, row pitch %.4f)", (double)y, (double)pitch);
    return 1;
}

/* a row press on fallback F's page (game thread): by the kind of the row drawn last (rp_field_rows) */
static void rp_field_press(int press, uint64_t now)
{
    g_rp_since = now;
    if (press < 0 || press >= g_rp_gn) return;   /* a stale press of a row the page does not have */
    switch (g_rp_gkind[press]) {
    case RP_GK_FIELD:
        g_rp_gsel = (g_rp_gsel + 1) % RP_NREASON;
        rp_log("report: field reason %s", g_rp_reason_key[g_rp_gsel]);
        break;
    case RP_GK_MG: g_rp_gmg = !g_rp_gmg; rp_log("report: field mute & ghost %s", g_rp_gmg ? "on" : "off"); break;
    case RP_GK_SUBMIT:
        if (g_rp_gsel < 0) { rp_log("report: field SUBMIT ignored (no reason picked)"); return; }
        g_rp_reason = g_rp_gsel;
        rp_submit(g_rp_gmg);
        return;
    case RP_GK_CANCEL: rp_log("report: cancelled"); rp_prompt(g_rp_msg[RP_MSG_CANCELLED]); g_rp_state = RP_IDLE; return;
    default: return;   /* the sentence row */
    }
    g_rp_pending = 1;
    g_rp_retry = 0;
}

/* Runs the pending tablet write; a busy slot is retried on the next ticks (at most 60). Pending: 1 fallback F's page,
 * 2 the result prompt, 3 the tablet reasons' prompt (ac: REPORT / "" / CANCEL, the layer goes over it), 4 back to user_list. */
static void rpt_close(void *game, const char *why, int back);
static void rp_flush(void *game)
{
    if (!g_rp_pending) return;
    int rc;
    if (g_rp_pending == 3) rc = rp_show_prompt_as(game, "", "CANCEL");
    else if (g_rp_pending == 4) {
        uint64_t st = S_ST_USER_LIST;
        const void *sv = &st;
        rc = rp_call(game, S_TRANSITION, 1, &S_STATE, &sv);
    } else if (g_rp_pending == 1) {
        const char *rows[RP_MAXROWS];
        rp_field_rows();
        for (int i = 0; i < RP_MAXROWS; i++) rows[i] = g_rp_grow[i];
        rc = rp_show_list(game, rows, g_rp_gn);
    } else rc = rp_show_prompt(game, g_rp_prompt_msg);
    if (rc == 1 && ++g_rp_retry < 60) return;
    if (rc) {
        rp_log("report: tablet call %d %s", g_rp_pending, rc == 1 ? "stayed busy" : "refused (inert)");
        if (g_rp_pending == 4 && g_rpt_bk) {   /* the back step's user_list transition failed: no card re-open */
            g_rpt_bk = RPT_BK_OFF;
            rp_log("report: tablet %s: back: card not re-opened (user_list transition failed)", g_rpt_bk_by);
        }
        rpt_close(game, "tablet call failed", 0);
        rp_reset("tablet call failed");
        return;
    }
    g_rp_retry = 0;
    RP_XCHG(&g_rpt_res, g_rp_pending == 2);   /* the result prompt is up (its OK is ours); any other write ends it */
    if (g_rp_pending == 2) { RP_XCHG(&g_rpt_ok_ev, 0); g_rpt_res_next = g_rp_now + RPT_RES_SETTLE_MS; }
    g_rp_pending = 0;
}

static void rp_prompt(const char *msg) { g_rp_prompt_msg = msg; g_rp_pending = 2; g_rp_retry = 0; }

static void rp_submit(int mg)
{
    mg = mg && g_rp_mgshow;   /* no box (report_mute_ghost=0): never true */
    if (g_rp_target_fake) {   /* report_fake_rows: never POST; finish as a test row through the POST completion path */
        rp_log("report: *** TEST ROW target #%04x (report_fake_rows): reason %s mute_ghost %d, NOT POSTED ***",
               rp_tag(g_rp_target), g_rp_reason_key[g_rp_reason], mg);
        g_rp_state = RP_POSTING;
        g_rp_job_status = RP_ST_TEST;
        g_rp_job_done = 1;                 /* done before the job id: the worker skips a done job */
        RP_XCHG(&g_rp_job_gen, g_rp_job_gen + 1);
        RP_XCHG(&g_rp_job, RP_JOB_POST);
        return;
    }
    char tn[2 * RP_NAMELEN], sid[128], mt[128];
    rp_jesc(tn, sizeof tn, g_rp_target);
    rp_jesc(sid, sizeof sid, g_rp_sess_id);
    rp_jesc(mt, sizeof mt, g_rp_sess_type);
    char user[96] = "", ue[128] = "";
    rp_user(user, sizeof user);
    rp_jesc(ue, sizeof ue, user);
    snprintf(g_rp_post_body, sizeof g_rp_post_body,
             "{\"target_user_id\":\"%s\",\"target_name\":\"%s\",\"reason\":\"%s\",\"mute_ghost\":%s,"
             "\"session_id\":\"%s\",\"match_type\":\"%s\"%s%s%s%s%s%s,\"platform\":\"%s\"}",
             g_rp_target_id, tn, g_rp_reason_key[g_rp_reason], mg ? "true" : "false", sid, mt,
             ue[0] ? ",\"user_id\":\"" : "", ue, ue[0] ? "\"" : "",
             g_rp_entry[0] ? ",\"entry\":\"" : "", g_rp_entry, g_rp_entry[0] ? "\"" : "", RP_PLATFORM);
    rp_log("report: submit target #%04x id %s reason %s mute_ghost %d (recorded with the report; nothing is muted)",
           rp_tag(g_rp_target), g_rp_target_id[0] ? rp_mask(g_rp_target_id) : "-", g_rp_reason_key[g_rp_reason], mg);
    g_rp_state = RP_POSTING;
    rp_start_job(RP_JOB_POST);
}

/* The card's target id from the last host rows, by name (fallback only). */
static void rp_match_target_id(void)
{
    g_rp_target_id[0] = 0;
    for (int i = 0; i < g_rp_nrows; i++)
        if (!strcmp(g_rp_rows[i], g_rp_target)) { snprintf(g_rp_target_id, sizeof g_rp_target_id, "%s", g_rp_rowid[i]); return; }
}

static const char *rp_sess_why(int rc) { return rc == RP_SESS_UNVERIFIED ? "unverified build" : sess_err(rc); }

/* report_fake_rows (TEST ONLY): appends "TEST 1".."TEST N" after the real rows, so the popup list scroll (5 rows + the peek row)
 * can be driven with one real opponent. A synthetic row has no user id and is never POSTed (rp_submit). */
static void rp_add_fake_rows(void)
{
    int real = g_rp_nrows, n = 0;
    for (; n < g_rp_fake_rows && g_rp_nrows < RP_ROWCAP; n++, g_rp_nrows++) {
        snprintf(g_rp_rows[g_rp_nrows], RP_NAMELEN, "TEST %d", n + 1);
        g_rp_rowid[g_rp_nrows][0] = 0;
        g_rp_rowi[g_rp_nrows] = g_rp_rowslot[g_rp_nrows] = -1;
        g_rp_rowfake[g_rp_nrows] = 1;
    }
    if (g_rp_fake_rows) rp_log("report: *** TEST ROWS (report_fake_rows=%d): %d synthetic player(s) appended after %d real; "
                               "a synthetic target never POSTs ***", g_rp_fake_rows, n, real);
}

/* Game thread: the rows (other players, users[] order), session id and match type from the in-process reader.
 * 1 = the reader answered (rows may be 0); 0 = unavailable, the caller falls back to the host (logged). */
static int rp_rows_from_reader(void *game)
{
    int rc = rp_sess(game, SESS_ALL, &g_rp_snap);
    if (rc != SESS_OK) {
        rp_log("report: session reader unavailable (%s): rows from host fallback", rp_sess_why(rc));
        return 0;
    }
    const sess_player *o[SESS_MAX_USERS];
    int n = sess_others(&g_rp_snap, o, SESS_MAX_USERS);
    g_rp_nrows = 0;
    int bots = 0;
    for (int k = 0; k < n && g_rp_nrows < RP_ROWCAP; k++) {
        /* CNSLobby::EntrantIsBot = provider & 0xf == 6 with an account (sess_snapshot.h); a bot is no account, so
         * a report on it gets a 400 from the host. Host-fallback rows (rp_parse_host) are never filtered here. */
        if (g_rp_hide_bots && o[k]->has_userid && (o[k]->provider & 0xf) == 6 && o[k]->userid) { bots++; continue; }
        if (o[k]->name[0]) snprintf(g_rp_rows[g_rp_nrows], RP_NAMELEN, "%s", o[k]->name);
        else snprintf(g_rp_rows[g_rp_nrows], RP_NAMELEN, "PLAYER %u", o[k]->slot);
        if (o[k]->has_userid) snprintf(g_rp_rowid[g_rp_nrows], sizeof g_rp_rowid[0], "%llu", (unsigned long long)o[k]->userid);
        else g_rp_rowid[g_rp_nrows][0] = 0;
        g_rp_rowi[g_rp_nrows] = o[k]->i;
        g_rp_rowslot[g_rp_nrows] = o[k]->slot;
        g_rp_rowfake[g_rp_nrows] = 0;
        g_rp_nrows++;
    }
    rp_add_fake_rows();
    snprintf(g_rp_sess_id, sizeof g_rp_sess_id, "%s", g_rp_snap.has_sessionid ? g_rp_snap.sessionid : "");
    snprintf(g_rp_sess_type, sizeof g_rp_sess_type, "%s", g_rp_snap.has_lob ? g_rp_snap.match_type : "");
    rp_log("report: rows from session reader: %d other player(s)%s (%s, session id %s)", g_rp_nrows,
           bots ? " (bots hidden: report_hide_bots)" : "", g_rp_sess_type[0] ? g_rp_sess_type : "no lobby",
           g_rp_sess_id[0] ? "yes" : "none");
    return 1;
}

/* The card's target id: its script's STRING variable xplatformid ("<platform code>-<account>", e.g. DMO-…, OVR-ORG-…),
 * set by the card's p_userid setter. 1 = the account number after the last '-' parsed into *acct; xp gets the raw
 * string either way ("" when unreadable). xm gets "<code>-<last 4>" for the log. Game thread. */
static int rp_card_xpid(void *game, char *xp, size_t cap, char *xm, size_t mcap, uint64_t *acct)
{
    xp[0] = 0;
    *acct = 0;
    snprintf(xm, mcap, "-");
    if (!rp_getvar(game, RP_CARD, RP_NCA_SCRIPT, RP_XPID, xp, cap)) { snprintf(xm, mcap, "unread"); return 0; }
    if (!xp[0]) { snprintf(xm, mcap, "empty"); return 0; }
    const char *dash = strrchr(xp, '-'), *d = dash ? dash + 1 : xp;
    snprintf(xm, mcap, "%.*s-%s", dash ? (int)(dash - xp) > 12 ? 12 : (int)(dash - xp) : 0, xp, rp_mask(d));
    uint64_t v = 0;
    int n = 0;
    for (; *d >= '0' && *d <= '9' && n < 20; d++, n++) v = v * 10 + (uint64_t)(*d - '0');
    if (!n || *d) return 0;   /* not "<code>-<digits>": keep the string for the log, no id */
    *acct = v;
    return 1;
}

/* Game thread, after rp_rows_from_reader answered (g_rp_snap is fresh): the player the SOCIAL card shows.
 * 1. the card's xplatformid account == a non-local reader player's user id (any slot);
 * 2. else, only in a session with exactly one other player, that player;
 * never by name: the card's name_text is compared with the chosen player's UserName and only logged.
 * Logs one "report: card target" line and returns the player, or NULL. */
static const sess_player *rp_card_target(void *game, const char *shown, const char *why)
{
    char xp[RP_NAMELEN], xm[32];
    uint64_t acct;
    int have = rp_card_xpid(game, xp, sizeof xp, xm, sizeof xm, &acct);
    const sess_player *o[SESS_MAX_USERS], *p = NULL;
    int n = sess_others(&g_rp_snap, o, SESS_MAX_USERS), same = 0;
    for (int k = 0; have && k < n; k++)
        if (o[k]->has_userid && o[k]->userid == acct) { if (!p) p = o[k]; same++; }
    const char *how = "by card xplatformid";
    if (same > 1) how = "by card xplatformid, several players share the id: first";
    /* a readable id that matches nobody (e.g. the card still shows a player who left) never falls back to a guess */
    if (!p && n == 1 && !have) { p = o[0]; how = "only other player; card xplatformid unreadable"; }
    if (!p) {
        rp_log("report: card target none (%s): xpid=%s shown=#%04x, %d other player(s): %s", why, xm, rp_tag(shown), n,
               !n ? "no other player" : !have ? "card xplatformid unreadable, and 2+ other players" : "card xplatformid matched no user id");
        return NULL;
    }
    char uid[24];
    snprintf(uid, sizeof uid, "%llu", (unsigned long long)p->userid);
    const char *nm = !shown[0] || !p->name[0] ? "n/a" : !strcmp(p->name, shown) ? "matches" : sess__ieq(p->name, shown) ? "matches any case" : "differs";
    rp_log("report: card target idx=%u slot=%u userid=%s xpid=%s name=#%04x shown=#%04x (%s; %s; name %s; %d other player(s))",
           p->i, p->slot, p->has_userid ? rp_mask(uid) : "-", xm, rp_tag(p->name), rp_tag(shown), how, why, nm, n);
    return p;
}

/* the target fields from a reader player (as rp_rows_from_reader writes a row) */
static void rp_set_target(const sess_player *p)
{
    if (p->name[0]) snprintf(g_rp_target, sizeof g_rp_target, "%s", p->name);
    else snprintf(g_rp_target, sizeof g_rp_target, "PLAYER %u", p->slot);
    if (p->has_userid) snprintf(g_rp_target_id, sizeof g_rp_target_id, "%llu", (unsigned long long)p->userid);
    else g_rp_target_id[0] = 0;
}

/* the card tile's sprite and highlight for the configured label (party_* on PC, friend_* on Quest); 0 = unknown */
static int rp_card_tile(uint64_t *sprite, uint64_t *highlight)
{
    if (g_rp_card_el == rp_sym("party_label")) { *sprite = rp_sym("party_sprite"); *highlight = rp_sym("party_highlight"); return 1; }
    if (g_rp_card_el == rp_sym("friend_label")) { *sprite = rp_sym("friend_sprite"); *highlight = rp_sym("friend_highlight"); return 1; }
    return 0;
}

/* entry A, once per card open (the card's enable thread rewrote the label): the glyph and the target line */
static void rp_card_opened(void *game)
{
    uint64_t sp, hl;
    if (g_rp_glyph) {
        if (!rp_card_tile(&sp, &hl)) rp_log("report: card glyph skipped (label element %016llx has no known sprite)", (unsigned long long)g_rp_card_el);
        else {
            int a = rp_sprite_uv(game, RP_CARD, sp, g_rp_glyph), b = rp_show(game, RP_CARD, hl, 0);
            rp_log("report: card glyph %s (sprite %016llx region %016llx), highlight %s", a ? "set" : "not set",
                   (unsigned long long)sp, (unsigned long long)g_rp_glyph, b ? "hidden" : "untouched");
        }
    }
    char shown[RP_NAMELEN] = "";
    rp_gettext(game, RP_CARD, S_NAME_TEXT, shown, sizeof shown);
    if (g_rp_state != RP_IDLE || g_rpp_page >= 0) return;   /* a flow owns the rows: the press re-resolves */
    if (rp_rows_from_reader(game)) rp_card_target(game, shown, "card open");
}

/* ---- ADD FRIEND (PC) ---- */
static void rp_friend_apply(void *game, const char *why)
{
    if (!g_rp_addfriend) return;
    int r = rp_card_button4(game), a = 0, b = 0;
    if (r == 1) {
        a = rp_show(game, RP_CARD, S_FRIEND_LABEL, 1);
        b = rp_show(game, RP_CARD, S_FRIEND_HL, 0);
    }
    if (r != g_rp_f4_done || strcmp(why, "poll")) {
        g_rp_f4_done = r;
        rp_log("report: ADD FRIEND (%s): button4 %s, friend_label %s, friend_highlight %s", why,
               r == 1 ? "enabled" : r == 0 ? "not run (no engine hook on this platform, a prologue check failed or the card is not loaded)" : "FAULT",
               r == 1 ? (a ? "shown" : "NOT written") : "untouched", r == 1 ? (b ? "hidden" : "NOT written") : "untouched");
    }
}

/* every 1 s poll, name_text|xplatformid unchanged (a reopen of the same player, a card event, mode 2) */
static void rp_friend_poll(void *game, uint64_t now, int act, int closed)
{
    if (!g_rp_addfriend) return;
    if (closed) { g_rp_f4_closed_at = now; g_rp_f4_open = 0; g_rp_f4_re = 0; rp_log("report: ADD FRIEND: card close_button pressed: tile left to the close thread"); return; }
    if (act && !g_rp_f4_open && now - g_rp_f4_closed_at > RP_F4_QUIET_MS) {
        g_rp_f4_open = 1;
        rp_friend_apply(game, "card event");
        g_rp_f4_re = 1;                 /* and once more on the next poll */
    } else if (g_rp_f4_open && g_rp_f4_re) {
        g_rp_f4_re = 0;
        rp_friend_apply(game, "card open +1 s");
    } else if (g_rp_addfriend == 2) {   /* fallback when a same-player reopen raises no card event: fights the close thread */
        rp_friend_apply(game, "poll");
    }
}

/* ---- five-tile card (canvas has report_label; five-tile recipe) ---- */
/* 1 s poll (game thread): decides five-tile once per card gamespace (Quest FindPrimitiveID logs a missing element), and
 * treats a new name_text|xplatformid as a card open. */
static void rp_card5_check(void *game, uint64_t now)
{
    uintptr_t key = rp_actor_key(game, RP_CARD);
    if (key != g_rp_five_key || g_rp_five < 0) {
        int five = key ? rp_has_el(game, RP_CARD, S_REPORT_LABEL) : 0;
        g_rp_five_key = key;
        g_rp_card5_seen[0] = 0;
        if (five != g_rp_five) {
            if (five)
                rp_log("report: five-tile card (report_label present): no relabel, no interim glyph; REPORT = button5 "
                       "(report_card5_event %016llx uid %016llx), card events logged %s", (unsigned long long)g_rp_card5_event,
                       (unsigned long long)g_rp_card5_uid, g_rp_card5_learn ? "60 s after each card open" : "never (report_card5_learn=0)");
            else if (key || g_rp_five >= 0)
                rp_log("report: card without report_label (%s): v1 relabel %s", key ? "stock canvas" : "card not loaded",
                       g_rp_card_event ? "on" : "off (no report_card_event)");
            RP_XCHG(&g_rp_five, five);
        }
    }
    if (g_rp_learn5_on && now > g_rp_learn5_until) RP_XCHG(&g_rp_learn5_on, 0);
    if (g_rp_five != 1) return;
    char shown[RP_NAMELEN] = "", xp[RP_NAMELEN], xm[32], seen[sizeof g_rp_card5_seen];
    uint64_t acct = 0;
    rp_gettext(game, RP_CARD, S_NAME_TEXT, shown, sizeof shown);
    int have = rp_card_xpid(game, xp, sizeof xp, xm, sizeof xm, &acct);
    snprintf(seen, sizeof seen, "%s|%s", shown, xp);
    if (!shown[0] && !xp[0]) return;                       /* never opened */
    if (!strcmp(seen, g_rp_card5_seen)) {
        rp_friend_poll(game, now, RP_XCHG(&g_rp_f4_act, 0), RP_XCHG(&g_rp_f4_close, 0));
        return;
    }
    RP_XCHG(&g_rp_f4_act, 0); RP_XCHG(&g_rp_f4_close, 0);
    memcpy(g_rp_card5_seen, seen, sizeof seen);
    g_rp_learn5 = 0;
    g_rp_learn5_until = now + 60000;
    RP_XCHG(&g_rp_learn5_on, g_rp_card5_learn);
    int hl = rp_show(game, RP_CARD, S_REPORT_HL, 0);
    int label = 1;
    const char *why = "no reader check (a flow owns the rows)";
    if (g_rp_state == RP_IDLE && g_rpp_page < 0) {
        if (!rp_rows_from_reader(game)) why = "session reader unavailable: kept";
        else if (rp_card_target(game, shown, "card open")) why = "player in the session";
        else if (have) { label = 0; why = "xplatformid matches no session user: the card shows warning_message"; }
        else why = "xplatformid unreadable: kept";
    }
    int a = rp_show(game, RP_CARD, S_REPORT_LABEL, (uint32_t)label);
    int held = label ? (rpt_so_release(game, &g_rpw_so, now, "player in the session", 0), 0) : rpt_so_apply(game, &g_rpw_so, now);
    rp_log("report: five-tile card open: report_label %s%s (%s), report_highlight %s%s", label ? "shown" : "hidden",
           a ? "" : " (write failed)", why, hl ? "hidden" : "untouched", label ? "" : held ? ", button5 held off" : ", button5 NOT held (no engine route)");
    g_rp_f4_open = 1;
    rp_friend_apply(game, "card open");   /* the close thread disabled button4: enable it again on each open */
    g_rp_f4_re = 1;                       /* and once more on the next poll, in case the open thread was still running */
}

/* ---- pause report popups ---- */
/* probes the optional elements once per popup open (Quest logs an engine error for a missing element) */
static void rpp_probe(void *game)
{
    static const uint16_t want_row = 1u << RPP_E_TEXT | 1u << RPP_E_SUB | 1u << RPP_E_BG | 1u << RPP_E_BBG | 1u << RPP_E_BOUT | 1u << RPP_E_ICON,
        want_reason = 1u << RPP_E_SELC | 1u << RPP_E_SELS, want_mg = 1u << RPP_E_CHECK | 1u << RPP_E_CHECK2 | 1u << RPP_E_TEXT | 1u << RPP_E_BG | 1u << RPP_E_SELC
                                | 1u << RPP_E_BBG | 1u << RPP_E_BOUT | 1u << RPP_E_ICON, want_text = 1u << RPP_E_TEXT | 1u << RPP_E_STEXT | 1u << RPP_E_BG;
    for (int b = 0; b < RPP_NB; b++) {
        uint16_t want = b < RPP_ROWS ? (uint16_t)(want_row | 1u << RPP_E_SELC)
                     : (b >= RPP_B_REASON1 && b < RPP_B_REASON1 + RP_NREASON) ? want_reason
                     : b == RPP_B_MG ? want_mg : (b == RPP_B_SUBMIT || b == RPP_B_LIST_SUBMIT) ? want_text : 0;
        g_rpp_has[b] = 0;
        for (int e = 0; e < RPP_NE; e++)
            if ((want >> e & 1) && rp_has_el(game, g_rpp_actor[b], g_rpp_el[e])) g_rpp_has[b] |= (uint16_t)(1u << e);
    }
    for (int p = 0; p < RPP_NPAGE; p++) g_rpp_sent_ok[p] = p != RPP_LIST && rp_has_el(game, g_rpp_frame[p], S_RPP_SENTENCE);
    rp_log("report: popup elements: row1 %03x peek %03x list-submit %03x reason1 %03x mg %03x submit %03x, sentence reasons %d done %d",
           g_rpp_has[RPP_B_ROW1], g_rpp_has[RPP_PEEK], g_rpp_has[RPP_B_LIST_SUBMIT], g_rpp_has[RPP_B_REASON1], g_rpp_has[RPP_B_MG],
           g_rpp_has[RPP_B_SUBMIT], g_rpp_sent_ok[RPP_REASONS], g_rpp_sent_ok[RPP_DONE]);
}

#define RPP_HAS(b, e) (g_rpp_has[b] >> (e) & 1)

static void rpp_row_vis(void *game, int b, int on)
{
    for (int e = RPP_E_TEXT; e < RPP_NE; e++)
        if ((e <= RPP_E_BG || e >= RPP_E_STEXT) && RPP_HAS(b, e)) rp_show(game, g_rpp_actor[b], g_rpp_el[e], (uint32_t)on);
}

/* the current page's writes (on a change, then re-asserted: stock button scripts may rewrite on enable) */
static int rpp_apply(void *game)
{
    int w = 0;
    if (g_rpp_page == RPP_LIST) {
        /* rows 1-5 from off; the peek row (row 6, the clipped w22a canvas) shows the next name only, hidden at the end */
        for (int i = 0; i < RPP_ROWS; i++) {
            int b = RPP_B_ROW1 + i, k = g_rpp_off + i, on = k < g_rp_nrows;
            if (RPP_HAS(b, RPP_E_TEXT)) w += rp_settext(game, g_rpp_actor[b], g_rpp_el[RPP_E_TEXT], on ? g_rp_rows[k] : "");
            if (RPP_HAS(b, RPP_E_SUB)) w += rp_settext(game, g_rpp_actor[b], g_rpp_el[RPP_E_SUB], on && i < RPP_VIS ? "Online" : "");
            rpp_row_vis(game, b, on);
            /* the button module shows buttonhighlight on hover whatever the row's visibility: retint it to the popup body
             * on an unused row (RGB only, so the module's own show / alpha keeps working on a used row) */
            if (RPP_HAS(b, RPP_E_SELC)) w += rp_color(game, g_rpp_actor[b], g_rpp_el[RPP_E_SELC], on ? RPP_ROW_HL : RPP_BODY);
        }
        /* SUBMIT under the list: Coffey's dim, inert button (a press only logs) */
        for (int e = RPP_E_TEXT; e <= RPP_E_STEXT; e += RPP_E_STEXT - RPP_E_TEXT)
            if (RPP_HAS(RPP_B_LIST_SUBMIT, e)) w += rp_color(game, g_rpp_actor[RPP_B_LIST_SUBMIT], g_rpp_el[e], RPP_DIM);
        if (RPP_HAS(RPP_B_LIST_SUBMIT, RPP_E_BG)) w += rp_color(game, g_rpp_actor[RPP_B_LIST_SUBMIT], g_rpp_el[RPP_E_BG], RPP_DIM_BG);
    } else if (g_rpp_page == RPP_REASONS) {
        char s[RP_HDRLEN + 64];
        rpp_sentence(s, sizeof s);
        if (g_rpp_sent_ok[RPP_REASONS]) w += rp_settext(game, g_rpp_frame[RPP_REASONS], S_RPP_SENTENCE, s);
        for (int r = 0; r < RP_NREASON; r++) {
            int b = RPP_B_REASON1 + r;
            if (RPP_HAS(b, RPP_E_SELC)) w += rp_color(game, g_rpp_actor[b], g_rpp_el[RPP_E_SELC], r == g_rpp_reason ? RPP_TEAL : RPP_STOCK);
            if (RPP_HAS(b, RPP_E_SELS)) w += rp_show(game, g_rpp_actor[b], g_rpp_el[RPP_E_SELS], r == g_rpp_reason);
        }
        /* Mute & Ghost: the shared label when report_mute_ghost=1, else the whole check hidden (label, box, tick) and
         * its hover tinted to the popup body, as an unused list row. The box is the check canvas's buttonbg (with
         * buttonoutline and buttonicon from the same third-button donor), so the row hide covers it too */
        if (g_rp_mgshow && RPP_HAS(RPP_B_MG, RPP_E_TEXT)) w += rp_settext(game, g_rpp_actor[RPP_B_MG], g_rpp_el[RPP_E_TEXT], g_rp_mg_label);
        if (!g_rp_mgshow) rpp_row_vis(game, RPP_B_MG, 0);
        if (RPP_HAS(RPP_B_MG, RPP_E_SELC)) w += rp_color(game, g_rpp_actor[RPP_B_MG], g_rpp_el[RPP_E_SELC], g_rp_mgshow ? RPP_ROW_HL : RPP_BODY);
        if (RPP_HAS(RPP_B_MG, RPP_E_CHECK)) w += rp_show(game, g_rpp_actor[RPP_B_MG], g_rpp_el[RPP_E_CHECK], (uint32_t)(g_rpp_mg && g_rp_mgshow));
        if (RPP_HAS(RPP_B_MG, RPP_E_CHECK2)) w += rp_show(game, g_rpp_actor[RPP_B_MG], g_rpp_el[RPP_E_CHECK2], (uint32_t)(g_rpp_mg && g_rp_mgshow));
        for (int e = RPP_E_TEXT; e <= RPP_E_STEXT; e += RPP_E_STEXT - RPP_E_TEXT)
            if (RPP_HAS(RPP_B_SUBMIT, e))
                w += rp_color(game, g_rpp_actor[RPP_B_SUBMIT], g_rpp_el[e], g_rpp_reason >= 0 ? RPP_WHITE : RPP_DIM);
        if (RPP_HAS(RPP_B_SUBMIT, RPP_E_BG))
            w += rp_color(game, g_rpp_actor[RPP_B_SUBMIT], g_rpp_el[RPP_E_BG], g_rpp_reason >= 0 ? RPP_STOCK : RPP_DIM_BG);
    } else if (g_rpp_page == RPP_DONE) {
        if (g_rpp_sent_ok[RPP_DONE]) w += rp_settext(game, g_rpp_frame[RPP_DONE], S_RPP_SENTENCE, g_rpp_done_msg);
    }
    return w;
}

/* shows popup page p (EnablePage on its owner; the pages are one popup state each) and schedules the writes */
static void rpp_show(void *game, int p, uint64_t now)
{
    int ok = rp_show_page(game, g_rpp_owner[p]);
    g_rpp_page = p;
    g_rpp_since = now;
    g_rpp_pass = 0;
    g_rpp_next = now;
    rp_log("report: popup page %s %s", g_rpp_page_name[p], ok ? "enabled" : "NOT enabled (owner not found)");
}

static void rpp_done(void *game, const char *msg, uint64_t now)
{
    snprintf(g_rpp_done_msg, sizeof g_rpp_done_msg, "%s", msg);
    rpp_show(game, RPP_DONE, now);
}

/* back to pause p1, popup state cleared; preset mode (opened from the card): the pause menu toggled off instead */
static void rpp_close(void *game, const char *why)
{
    if (g_rpp_post) {
        g_rpp_post = 0;
        g_rpp_orphan = 1;
        rp_log("report: popup closed while the report is sent (%s): the result will be logged only", why);
    }
    if (g_rpp_preset) {
        int ok = rp_fire(game, RP_PAUSE_P1, S_TOGGLE_PAUSE) || rp_fire(game, RP_TABLET, S_TOGGLE_PAUSE);
        rp_log("report: popup closed (%s), preset: pause menu toggle %s", why, ok ? "sent" : "NOT sent");
    } else {
        int ok = rp_show_page(game, RP_PAUSE_P1);
        rp_log("report: popup closed (%s), pause p1 %s", why, ok ? "enabled" : "NOT enabled");
    }
    g_rpp_page = -1;
    g_rpp_wait = 0;
    g_rpp_preset = 0;
    g_rpp_reassert_t = 0;
}

/* the REPORT tile press when the popup pages are loaded (game thread) */
static void rpp_open(void *game, uint64_t now)
{
    rpp_probe(game);
    g_rpp_off = 0;   /* every open starts at the top */
    g_rpp_acc = 0;
    g_rpp_run = g_rpp_run_steps = 0;
    g_rpp_hov_dir = 0;
    g_rpp_scroll_t = now;
    RP_XCHG(&g_rpp_hov, 0);
    RP_XCHG(&g_rpp_scr_ev, 0);
    g_rpp_sel = -1;
    g_rp_target[0] = g_rp_target_id[0] = 0;
    g_rp_target_fake = 0;
    if (rp_rows_from_reader(game)) {
        if (!g_rp_nrows) rpp_done(game, g_rp_msg[RP_MSG_NO_PLAYERS], now);
        else rpp_show(game, RPP_LIST, now);
    } else {
        g_rpp_wait = 1;
        g_rp_state = RP_WAIT_SESSION;
        g_rp_since = now;
        rp_start_job(RP_JOB_HOST);
    }
}

/* the card's REPORT press (D-S2): p23 with the card's target preset. The popup pages load only while the pause menu
 * shows, so when p23's owner is not loaded the pause menu is toggled on (evt_dbg_toggle_menu_vis,; held by the p1
 * owner, else the tablet) and RP_PRESET waits for it in rpp_preset_tick. CANCEL and the done page's OK then toggle the
 * pause menu off (rpp_close). */
#define RPP_PRESET_SETTLE_MS 1500  /* after our toggle: p1's own enable runs first, then p23 goes over it (500 lost the race on PC, S4g);
                                    * counted from the first tick p23's owner is loaded (review M3), not from the toggle */
#define RPP_REASSERT_MS 500        /* then p23 enabled once more (EnablePage is idempotent): a late p1 enable is covered again */
#define RPP_PRESET_MS 3000         /* p23 not loaded by then: the toggle is undone and fallback F takes the reasons */
static void rp_enter_field(uint64_t now);
static void rpp_open_preset(void *game, uint64_t now)
{
    g_rp_state = RP_PRESET;
    g_rp_since = g_rpp_preset_t = now;
    g_rpp_toggled = 0;
    g_rpp_owner_t = 0;
    if (!rp_has_actor(game, g_rpp_owner[RPP_REASONS])) {
        g_rpp_toggled = rp_fire(game, RP_PAUSE_P1, S_TOGGLE_PAUSE) || rp_fire(game, RP_TABLET, S_TOGGLE_PAUSE);
        rp_log("report: card REPORT: pause menu toggle %s for %s", g_rpp_toggled ? "sent" : "NOT sent", g_rpp_page_name[RPP_REASONS]);
    }
}

static void rpp_preset_tick(void *game, uint64_t now)
{
    if (g_rp_state != RP_PRESET) return;
    if (rp_has_actor(game, g_rpp_owner[RPP_REASONS])) {
        if (!g_rpp_owner_t) g_rpp_owner_t = now;
        if (g_rpp_toggled && now - g_rpp_owner_t < RPP_PRESET_SETTLE_MS) return;
        rpp_probe(game);
        g_rp_state = RP_IDLE;   /* as the pause path: the popup owns the flow until SUBMIT */
        g_rpp_preset = 1;
        g_rpp_sel = -1;
        g_rpp_reason = -1;
        g_rpp_mg = 0;
        rpp_show(game, RPP_REASONS, now);
        /* no page-enabled read exists here: after our toggle p23 is enabled once more at +500 ms whatever happened */
        g_rpp_reassert_t = g_rpp_toggled ? now + RPP_REASSERT_MS : 0;
        return;
    }
    if (now - g_rpp_preset_t < RPP_PRESET_MS) return;
    rp_log("report: %s not loaded %d ms after the card REPORT: fallback field page", g_rpp_page_name[RPP_REASONS], RPP_PRESET_MS);
    if (g_rpp_toggled && !rp_fire(game, RP_PAUSE_P1, S_TOGGLE_PAUSE)) rp_fire(game, RP_TABLET, S_TOGGLE_PAUSE);
    g_rpp_toggled = 0;
    rp_enter_field(now);
}

/* ---- one report model, two surfaces  ----
 * The reasons step's model is ONE: target (g_rp_target / _id), pick g_rpp_reason, Mute & Ghost g_rpp_mg, the SUBMIT rule
 * and the body (rp_submit), the result strings (g_rp_msg). Both surfaces feed presses through rps_press by ROLE; each
 * surface only draws the model (apply) and leaves (close):
 *   popup  = pause p23 (ffa_rp_reasons_page, rpp_*: minted actors, evt_btn_clicked + uid),
 *   tablet = the arm tablet's stock generic_prompt state with the w23a layer acr_* of canvas ffa_ac_report on the generic
 *            prompt actor 0x8a5dcee389df5b7c (Coffey AC artboard; presses = delegate_OnFullyDepressed with ev[3] = the
 *            zone component ffa_acr_*). The layer is hidden at bind ahead of every report gate (rpt_guard; w23c also
 *            authors it disabled), shown only by rpt_open, and every exit (CANCEL, "<", the stock footer before the
 *            layer draws, SUBMIT -> result, the tablet leaving generic_prompt or closing (state-stack top, rpt_state_check), the lapse -> user_list,
 *            marker gone, a failed tablet call) runs rpt_restore: layer hidden, the stock header / message / footer / X
 *            shown again. While g_rpt_on the stock state_mgr zones under the face (menubutton1-4, w23e; the footer once the
 *            layer is drawn, 0x6567539d25efbdc0, BackButton, the three dots) are consumed on the tablet actor;
 *            clearing g_rpt_on / g_rpt_drawn on every exit ends that, nothing to restore. */
enum { RPS_MG = RP_NREASON, RPS_SUBMIT };
/* 0 ignored, 1 redraw, 2 SUBMIT started (*post set first: the surface takes the result) */
static int rps_press(int role, const char *surf, int *post)
{
    if (role >= 0 && role < RP_NREASON) {
        g_rpp_reason = role;
        rp_log("report: %s reason %s", surf, g_rp_reason_key[role]);
        return 1;
    }
    if (role == RPS_MG) {
        if (!g_rp_mgshow) { rp_log("report: %s mute & ghost press ignored (report_mute_ghost=0: hidden)", surf); return 0; }
        g_rpp_mg = !g_rpp_mg;
        rp_log("report: %s mute & ghost %s", surf, g_rpp_mg ? "on" : "off");
        return 1;
    }
    if (role == RPS_SUBMIT) {
        if (g_rpp_reason < 0) { rp_log("report: %s SUBMIT ignored (no reason picked)", surf); return 0; }
        if (g_rp_state == RP_POSTING) { rp_log("report: %s SUBMIT ignored (a report is posting)", surf); return 0; }
        g_rp_reason = g_rpp_reason;
        *post = 1;
        rp_submit(g_rpp_mg);
        return 2;
    }
    return 0;
}


static void rpt_init_syms(void)
{
    char b[32];
    for (int i = 0; i < RPT_NZ; i++) S_RPT_ZONE[i] = rp_sym_or_hex(g_rpt_zone_name[i]);
    for (int i = 0; i < RPT_NSTOCK; i++) S_RPT_STOCK[i] = rp_sym_or_hex(g_rpt_stock_name[i]);
    for (int i = 0; i < RPT_NMB; i++) S_RPT_MB[i] = rp_sym_or_hex(g_rpt_mb_name[i]);
    for (int i = 0; i < RPT_NMB_EV; i++) S_RPT_MB_EV[i] = rp_sym(g_rpt_mb_ev_name[i]);
    for (int i = 0; i < RP_NREASON; i++) { snprintf(b, sizeof b, "acr_r%d_sel", i + 1); S_ACR_SEL[i] = rp_sym(b); }
    S_ACR_ROOT = rp_sym("acr_root"); S_ACR_SENT = rp_sym("acr_sentence"); S_ACR_MG = rp_sym("acr_mg");
    S_ACR_MG_CHECK = rp_sym("acr_mg_check"); S_ACR_SUB_BG = rp_sym("acr_submit_bg"); S_ACR_SUB_TEXT = rp_sym("acr_submit_text");
    S_ACR_SUB_FILL = rp_sym("acr_submit_fill");
    S_FULLY_DEP = rp_sym("delegate_OnFullyDepressed"); S_ST_USER_LIST = rp_sym("user_list");
    S_OPEN_LMSC = rp_sym("OpenLobbyMemberSocialControls"); S_P_USERID = rp_sym("p_userid");
    S_USERNAME_PRESSED = rp_sym("usernamepressed"); S_PRESSED_USER_IDX = rp_sym("pressed_user_index");
    S_ST_PROFILE = rp_sym("player_profile");   /* Quest: the top after a usernamepressed call  */
    S_ACR_SENT_L[0] = rp_sym("acr_sent_l1"); S_ACR_SENT_L[1] = rp_sym("acr_sent_l2");
    S_ACR_MG_TEXT = rp_sym("acr_mg_text"); S_ST_PROMPT_T = rp_sym("generic_prompt");
}

/* The tablet sentence (w23b): the name as the card header shows it (rp_target_text: at most RP_HDRLEN-1 =
 * 31 bytes, cut on a UTF-8 boundary), then RP_SENTENCE_FMT's words. One centred line (acr_sentence) while it has at
 * most RPT_LINE_CHARS code points (size 22 on the 968 px row: about 12.5 px a glyph, PC shot s022_g3_ac at size 24
 * = 13.7); else two lines, "<name> will be reported for violating" / "the Echo VR Code of Conduct:" (acr_sent_l1 / l2,
 * at most 31 + 31 code points). The words equal the pause sentence (parity); only the line break differs.
 * Returns the number of lines (one[] = "" when 2). */
#define RPT_LINE_CHARS 72
#define RPT_L1_FMT "%s will be reported for violating"
#define RPT_L2_TEXT "the Echo VR Code of Conduct:"
static size_t rp_u8len(const char *s)
{
    size_t n = 0;
    for (; *s; s++) n += ((unsigned char)*s & 0xc0) != 0x80;
    return n;
}
static int rpt_sentence(char *one, size_t ocap, char *l1, char *l2, size_t lcap)
{
    char nm[RP_HDRLEN];
    rp_target_text(nm, sizeof nm);
    rp_sentence(one, ocap);
    if (rp_u8len(one) <= RPT_LINE_CHARS) { l1[0] = l2[0] = 0; return 1; }
    snprintf(l1, lcap, RPT_L1_FMT, nm);
    snprintf(l2, lcap, "%s", RPT_L2_TEXT);
    one[0] = 0;
    return 2;
}

/* 1 = the generic prompt actor draws ffa_ac_report (acr_root present). Decided once per gamespace (Quest's
 * FindPrimitiveID logs a missing element); a newly bound canvas gets its layer hidden here, so a stock prompt
 * (party join error, ...) never shows it (risk 1 of the design). Runs from rpt_guard, ahead of every report gate. */
static int rpt_bound(void *game)
{
    uintptr_t key = rp_actor_key(game, RP_GPROMPT);
    if (key == g_rpt_key && g_rpt_bound >= 0) return g_rpt_bound == 1;
    int b = key ? rp_has_el(game, RP_GPROMPT, S_ACR_ROOT) : 0;
    int hid = b && !g_rpt_on ? rp_show(game, RP_GPROMPT, S_ACR_ROOT, 0) : 0;
    g_rpt_two = b && rp_has_el(game, RP_GPROMPT, S_ACR_SENT_L[0]);   /* w23b canvas: the two-line sentence */
    g_rpt_fill = b && rp_has_el(game, RP_GPROMPT, S_ACR_SUB_FILL);
    if (b != g_rpt_bound || key != g_rpt_key)
        rp_log("report: tablet prompt %s%s%s", b ? "draws ffa_ac_report (acr_root)" : key ? "has no ffa_ac_report (stock canvas)"
               : "not loaded", b ? (hid ? ": layer hidden at load" : ": layer hide NOT run") : "",
               b ? (g_rpt_two ? " (w23b: two-line sentence)" : " (w23a: one-line sentence)") : "");
    g_rpt_key = key;
    g_rpt_bound = b;
    return b;
}

/* M1 ( review): the layer never shows unless our flow is live. First thing in every tick, ahead of the report= /
 * prologue / live gates (report=0, a failed check or a stock tree's auto mode still run it): the bind check every
 * 250 ms (rpt_bound hides a new canvas's layer at once), then the hide re-asserted once a second while no tablet report
 * owns the prompt. It needs only the canvas calls (rp_actor_key / rp_has_el / rp_show), not the report flow's checks.
 * w23c also authors acr_root disabled (T0 +0x10), so a prompt raised before the first poll stays stock. */
static int rpt_so_apply(void *game, rp_so_surface *s, uint64_t now);
static void rpt_guard(void *game, uint64_t now)
{
    if (RP_XCHG(&g_rpt_xfwd, 0) && !g_rpt_on && now - g_rpt_xfwd_t > 500) {
        /* fallback mode: the invisible "<" covers the stock X of whatever prompt is up: its press closes the tablet as the X */
        g_rpt_xfwd_t = now;
        if (!S_ON_PRESS) S_ON_PRESS = rp_sym("delegate_OnPress");
        int ok = rp_press_comp(game, RP_TABLET_COMP, S_ON_PRESS, RP_TABLET_X);
        rp_log("report: tablet: our \"<\" pressed with no layer (fallback mode): stock X %s", ok ? "sent" : "NOT sent");
    }
    if (now < g_rpt_guard_next) return;
    g_rpt_guard_next = now + 250;
    if (!S_ACR_ROOT) rpt_init_syms();   /* report_start did not run (report=0 / a failed check): the names only */
    if (!rpt_bound(game)) return;
    if (!(g_rpt_on && g_rpt_drawn)) rpt_so_apply(game, &g_rpl_so, now);   /* our own zones off while the layer is not drawn */
    if (g_rpt_on || now < g_rpt_guard_hide) return;
    g_rpt_guard_hide = now + 1000;
    rp_show(game, RP_GPROMPT, S_ACR_ROOT, 0);
}

/* ---- stock-off lifecycle (game thread; see the RP_SO note above the hook) ---- */
static void rpt_so_syms(rp_so_surface *s)
{
    for (int i = 0; i < s->n; i++) s->sym[i] = rp_sym_or_hex(s->def[i].zone);
}
static uint64_t rpt_so_zone(const rp_so_surface *s, int i)
{
    const uint64_t *v = s->def[i].zone_var;
    return v && *v ? *v : s->sym[i];
}
static void rpt_so_drop(rp_so_surface *s, int i)
{
    s->rec[i].st = 0;
    RP_XCHG(&s->own[i], 0);
}
static void rpt_so_once(rp_so_surface *s, int i, unsigned bit, const char *what)
{
    if (s->logged[i] & bit) return;
    s->logged[i] |= bit;
    rp_log("report: %s: %s %s", RPT_SO_PFX(s), s->def[i].label, what);
}
/* the engine route failed mid-run: every zone goes back to the swallow; a zone we hold may stay off until the level reloads */
static void rpt_so_fault(rp_so_surface *s0, const char *why)
{
    int held = 0;
    rp_so_surface *all[4] = { &g_rpt_so, &g_rpl_so, &g_rpc_so, &g_rpw_so };   /* one engine route: every surface falls back with it */
    (void)s0;
    for (int k = 0; k < 4; k++) {
        rp_so_surface *s = all[k];
        for (int i = 0; i < s->n; i++) if (s->rec[i].st) { held++; rpt_so_drop(s, i); }
        s->holding = 0;
        s->applied_n = -1;
    }
    g_rp_so_engine = 0;
    RP_XCHG(&g_rp_card_open, -1);   /* the hook stops dropping button5: the stock decides again */
    if (!g_rp_so_fault_logged++)
        rp_log("report: stock-off: fallback swallow (%s)%s", why, held ? ": zones held at the fault may stay off until the level reloads" : "");
}
static void rpt_so_start(void)
{
    const char *why = "unknown";
    rpt_so_syms(&g_rpt_so);
    g_rp_so_engine = rp_zone_ready(&why) ? 1 : 0;
    if (g_rp_so_engine == 1)
        rp_log("report: stock-off: engine (bit 0x%x on %d zones of the %s; the swallow stays only for a zone the engine cannot hold)",
               RP_SO_BIT, g_rpt_so.n, g_rpt_so.name);
    else rp_log("report: stock-off: fallback swallow (%s)", why);
}
/* the layer is drawn: every zone of the set off (bit 0x80), idempotent; returns the number held */
static int rpt_so_apply(void *game, rp_so_surface *s, uint64_t now)
{
    if (g_rp_so_engine != 1) return 0;
    if (!s->sym[0]) rpt_so_syms(s);
    s->next = now + RPT_SO_PERIOD_MS;
    int n = 0;
    for (int i = 0; i < s->n; i++) {
        rp_so_rec *r = &s->rec[i];
        rp_zref z;
        memset(&z, 0, sizeof z);
        int f = rp_zone_find(game, s->def[i].actor, rpt_so_zone(s, i), &z);
        if (f < 0) { rpt_so_fault(s, "zone lookup unavailable or faulted"); return 0; }
        if (!f) {
            if (r->st) { rpt_so_once(s, i, RP_SO_LOG_STALE, "zone gone while held (level reloaded?): record dropped"); rpt_so_drop(s, i); }
            else rpt_so_once(s, i, RP_SO_LOG_MISSING, "swallow fallback (zone not found)");
            continue;
        }
        if (r->st && (r->gs != z.gs || r->cs != z.cs || r->h != z.h)) {   /* a re-created component never had our bit */
            rpt_so_once(s, i, RP_SO_LOG_STALE, "handle changed while held: old record dropped, new one taken");
            rpt_so_drop(s, i);
        }
        if (!r->st) {
            if (z.reasons & RP_SO_BIT) { rpt_so_once(s, i, RP_SO_LOG_FOREIGN, "swallow fallback (bit 0x80 already set by another user: skipped)"); continue; }
            int rc = rp_zone_cat(game, &z, 1);
            if (rc < 0) { rpt_so_fault(s, "fault in DisableComponent"); return 0; }
            if (!rc) { rpt_so_once(s, i, RP_SO_LOG_FAIL, "swallow fallback (disable not run)"); continue; }
            r->gs = z.gs; r->cs = z.cs; r->h = z.h; r->st = 1;
            RP_XCHG(&s->own[i], 1);
        } else if (!(z.reasons & RP_SO_BIT) && rp_zone_cat(game, &z, 1) < 0) {   /* our bit was cleared by someone: re-assert */
            rpt_so_fault(s, "fault in DisableComponent");
            return 0;
        }
        n++;
    }
    s->holding = 0;   /* a new draw takes the zones of a post-close hold back */
    if (n != s->applied_n) rp_log("report: %s: %d zones off (engine): %s, %d/%d of the set", RPT_SO_PFX(s), n, s->name, n, s->n);
    s->applied_n = n;
    return n;
}
/* bit 0x80 cleared on one held zone after the stale-handle guard; 1 = released */
static int rpt_so_enable(void *game, rp_so_surface *s, int i)
{
    rp_so_rec *r = &s->rec[i];
    int ok = 0;
    if (!r->st) return 0;
    if (g_rp_so_engine == 1) {
        rp_zref z;
        memset(&z, 0, sizeof z);
        int f = rp_zone_find(game, s->def[i].actor, rpt_so_zone(s, i), &z);
        if (f == 1 && z.gs == r->gs && z.cs == r->cs && z.h == r->h) {
            int rc = rp_zone_cat(game, &z, 0);
            if (rc < 0) { rpt_so_fault(s, "fault in EnableComponent"); return 0; }
            ok = rc == 1;
        } else if (f >= 0) rpt_so_once(s, i, RP_SO_LOG_STALE, "handle changed or zone gone before the release: record dropped");
        else { rpt_so_fault(s, "zone lookup unavailable or faulted"); return 0; }
    }
    rpt_so_drop(s, i);
    return ok;
}
/* every exit: hold = 1 keeps the hold zones off RPT_SO_HOLD_MS (released by rpt_so_tick), the others now */
static void rpt_so_release(void *game, rp_so_surface *s, uint64_t now, const char *why, int hold)
{
    int m = 0, n = 0, held = 0;
    for (int i = 0; i < s->n; i++) {
        if (!s->rec[i].st) continue;
        m++;
        if (hold && s->def[i].hold) { held++; continue; }
        if (s == &g_rpl_so && i == RPT_Z_MG && !g_rp_mgshow) continue;   /* report_mute_ghost=0: nothing drawn there, the zone stays off */
        n += rpt_so_enable(game, s, i);
    }
    s->applied_n = -1;
    s->holding = held > 0;
    if (held) s->hold_until = now + RPT_SO_HOLD_MS;
    if (!m) return;
    if (held) rp_log("report: %s: %d zones released (%s), %d held %d ms", RPT_SO_PFX(s), n, why, held, RPT_SO_HOLD_MS);
    else rp_log("report: %s: %d zones released (%s)", RPT_SO_PFX(s), n, why);
}
/* every tick, ahead of the report gates (a flow gone inert still ends its hold) */
static void rpt_so_tick(void *game, rp_so_surface *s, uint64_t now)
{
    if (s->holding && now >= s->hold_until) rpt_so_release(game, s, now, "hold end", 0);
}
/* the five-tile card open or closed (its close_button's stock bits), every RPC_POLL_MS: the closed card's tiles held off */
static void rpc_tick(void *game, uint64_t now, int live)
{
    if (now < g_rpc_next) return;
    g_rpc_next = now + RPC_POLL_MS;
    int open = -1;
    if (live && g_rp_so_engine == 1 && g_rp_five == 1) {
        rp_zref z;
        memset(&z, 0, sizeof z);
        int f = rp_zone_find(game, RP_CARD, S_CLOSE_BTN, &z);
        if (f < 0) { rpt_so_fault(&g_rpc_so, "zone lookup unavailable or faulted"); return; }
        if (f == 1) open = !(z.reasons & (uint16_t)~RP_SO_BIT);
    }
    if (open != g_rp_card_open) {
        if (open != 1 && g_rpw_so.rec[0].st) {   /* the warning card closed: its hold ends, the next open is judged again */
            rpt_so_release(game, &g_rpw_so, now, open == 0 ? "card closed" : "card state unknown", 0);
            g_rp_card5_seen[0] = 0;
        }
        if (open == 1 && g_rp_card_open == 0) g_rp_next_card = now;   /* the open card's checks in this tick, not up to 1 s later */
        RP_XCHG(&g_rp_card_open, open);
        rp_log("report: card-off: card %s", open == 1 ? "open (close_button live)" : open == 0 ? "closed (close_button off): tiles held off"
               : "state unknown (no engine route, not a five-tile card or not loaded): tiles left to the stock");
    }
    if (g_rpt_on) return;   /* the REPORT layer's set holds the card's tiles while it owns the tablet */
    if (open == 0) rpt_so_apply(game, &g_rpc_so, now);
    else rpt_so_release(game, &g_rpc_so, now, open == 1 ? "card open" : "card state unknown", 0);
}

/* the layer hidden, the stock prims shown again: every exit */
static void rpt_restore(void *game, const char *why)
{
    int n = rp_show(game, RP_GPROMPT, S_ACR_ROOT, 0);
    for (int i = 0; i < RPT_NSTOCK; i++) n += rp_show(game, RP_GPROMPT, S_RPT_STOCK[i], 1);
    if (g_rpt_fill) rp_color(game, RP_GPROMPT, S_ACR_SUB_FILL, RPT_SUB_FILL[RPT_PLAT_QUEST][0]);   /* back to the authored fill */
    rp_log("report: tablet layer restored (%s): %d/%d writes", why, n, 1 + RPT_NSTOCK);
}

/* back = 1: transition to user_list (CANCEL, "<", the stock footer before the layer draws, the lapse); 0: the caller writes the next state
 * itself, or the tablet already left the prompt. "<" and CANCEL (report_tablet_cancel=card) then start rpt_back_start: the card. */
static void rpt_close(void *game, const char *why, int back)
{
    if (!g_rpt_on) return;
    if (g_rpt_post) {   /* left while the POST runs ("<", CANCEL, lapse): the late result is logged, never pushed over the list or card */
        g_rpt_post = 0;
        g_rpt_orphan = 1;
        rp_log("report: tablet panel closed while the report is sent (%s): the result will be logged only", why);
    }
    rpt_restore(game, why);
    rpt_so_apply(game, &g_rpl_so, g_rp_now);             /* our own zones off with the layer (the result prompt's X stays live) */
    rpt_so_release(game, &g_rpt_so, g_rp_now, why, 1);   /* stock-off: card button5 now, the rest after the hold */
    RP_XCHG(&g_rpt_on, 0);
    RP_XCHG(&g_rpt_drawn, 0);
    if (g_rp_card_open == 0) rpt_so_apply(game, &g_rpc_so, g_rp_now);   /* the closed card's tiles: no gap until the next card poll */
    g_rpt_setup_at = 0;
    if (g_rp_pending == 3) g_rp_pending = 0;   /* a prompt set-up not yet run is dropped with the panel */
    if (back) { g_rp_pending = 4; g_rp_retry = 0; }
}

/* the model drawn on the layer (on a change, then re-asserted: the prompt script may re-show its own prims) */
static int rpt_apply(void *game)
{
    int w = 0;
    char s[2 * RP_NAMELEN + 64], l1[RP_HDRLEN + 40], l2[40];
    for (int i = 0; i < RPT_NSTOCK; i++) w += rp_show(game, RP_GPROMPT, S_RPT_STOCK[i], 0);
    w += rp_show(game, RP_GPROMPT, S_ACR_ROOT, 1);
    if (g_rpt_two) {
        rpt_sentence(s, sizeof s, l1, l2, sizeof l1);
        w += rp_settext(game, RP_GPROMPT, S_ACR_SENT, s);
        w += rp_settext(game, RP_GPROMPT, S_ACR_SENT_L[0], l1);
        w += rp_settext(game, RP_GPROMPT, S_ACR_SENT_L[1], l2);
    } else {   /* a w23a canvas: one line, as before (the same sentence source) */
        rp_sentence(s, sizeof s);
        w += rp_settext(game, RP_GPROMPT, S_ACR_SENT, s);
    }
    for (int r = 0; r < RP_NREASON; r++) w += rp_show(game, RP_GPROMPT, S_ACR_SEL[r], r == g_rpp_reason);
    w += rp_show(game, RP_GPROMPT, S_ACR_MG, (uint32_t)g_rp_mgshow);
    if (g_rp_mgshow) w += rp_settext(game, RP_GPROMPT, S_ACR_MG_TEXT, g_rp_mg_label_ac);   /* Coffey's tablet label (w23e) */
    w += rp_show(game, RP_GPROMPT, S_ACR_MG_CHECK, (uint32_t)(g_rpp_mg && g_rp_mgshow));
    const float *c = g_rpp_reason >= 0 ? RPP_WHITE : RPP_DIM;   /* SetColor is RGB only: dim = grey, as the pause SUBMIT */
    w += rp_color(game, RP_GPROMPT, S_ACR_SUB_BG, c);
    w += rp_color(game, RP_GPROMPT, S_ACR_SUB_TEXT, c);
    if (g_rpt_fill) w += rp_color(game, RP_GPROMPT, S_ACR_SUB_FILL, RPT_SUB_FILL[RPT_PLAT_QUEST][g_rpp_reason >= 0]);   /* enabled = CANCEL's */
    return w;
}

/* the card's REPORT on the tablet: the stock prompt (header REPORT, empty message, CANCEL), then the layer */
static void rpt_open(void *game, uint64_t now)
{
    if (g_rpt_so.applied_n >= 0) rpt_so_release(game, &g_rpt_so, now, "new panel", 1);   /* the next draw takes them again */
    rpt_so_release(game, &g_rpc_so, now, "REPORT panel", 0);   /* the layer set holds the card's tiles from its draw on */
    RP_XCHG(&g_rpt_res, 0); RP_XCHG(&g_rpt_ok_ev, 0);
    RP_XCHG(&g_rpt_drawn, 0);
    RP_XCHG(&g_rpt_on, 1);
    RP_XCHG(&g_rpt_ev, -1);
    RP_XCHG(&g_rpt_mb_ev, -1);
    RP_XCHG(&g_rpt_mb_rel, -1); g_rpt_mb_last = -1;   /* a new panel: a new press */
    RP_XCHG(&g_rpt_x_ev, 0); RP_XCHG(&g_rpt_x_rel, 0); g_rpt_x_latch = 0;   /* the grace stays: it covers the "<" that closed the old panel */
    g_rp_state = RP_IDLE;       /* as the popup: the surface owns the flow until SUBMIT */
    g_rpp_reason = -1;
    g_rpp_mg = 0;
    g_rpt_post = 0;
    g_rpt_pass = 0;
    g_rpt_since = now;
    g_rpt_seen = 0;
    g_rpt_orphan = 0;
    g_rp_retry = 0;
    if (g_rpt_bk) { g_rpt_bk = RPT_BK_OFF; rp_log("report: tablet %s: back: card not re-opened (a new REPORT panel opened)", g_rpt_bk_by); }
    rp_log("report: tablet reasons (ac): generic_prompt + layer for #%04x", rp_tag(g_rp_target));
    /* w23b: the five-tile card stays open beside the panel otherwise. Its own close route, as a real press of its
     * close_button (a targeted delegate_OnFullyDepressed on the card's close_button). The press is an event the engine
     * runs later, so the prompt set-up waits RPT_CLOSE_MS for the close's own handler (review M2 c); a transition that
     * handler still makes afterwards is caught by the state check in rpt_tick. Nothing is restored on exit. */
    int sent = 0;
    if (g_rpt_close_card) {
        sent = rp_press_comp(game, RP_CARD, S_FULLY_DEP, S_CLOSE_BTN);
        if (sent && g_rp_addfriend) RP_XCHG(&g_rp_f4_close, 1);   /* as the hook flags a real close (the send skips it) */
        rp_log("report: tablet: five-tile card close_button %s", sent ? "sent" : "NOT sent (card not loaded)");
    }
    g_rpt_setup_at = now + (sent ? RPT_CLOSE_MS : 0);
    g_rpt_next = g_rpt_setup_at + RPT_SETTLE_MS;
}

/* M2 ( review): the arm tablet's state-stack top, read every RPT_STATE_MS while the panel is up. Once our
 * generic_prompt was on top, any other top (closed = deactivated, a tab, the card's own transition) restores the layer
 * at once; generic_prompt never reached RPT_REACH_MS after the set-up does the same. Returns 1 when the panel was
 * closed here. Reader unavailable (bytes differ, a fault): logged once, the 120 s lapse stays the only exit. */
static int rpt_state_check(void *game, uint64_t now)
{
    uint64_t top = 0;
    if (now < g_rpt_state_next) return 0;
    g_rpt_state_next = now + RPT_STATE_MS;
    if (!rp_tablet_top(game, &top)) {
        if (!g_rpt_top_off_logged++) rp_log("report: tablet state reader unavailable: a tablet closed mid-flow restores at the %d s lapse",
                                            RP_TIMEOUT_MS / 1000);
        return 0;
    }
    if (top == S_ST_PROMPT_T) { g_rpt_seen = 1; return 0; }
    if (!g_rpt_seen && now - g_rpt_setup_t < RPT_REACH_MS) return 0;   /* the queued transition has not run yet */
    rp_log("report: tablet %s (top %016llx): layer restored at once", g_rpt_seen ? "left generic_prompt" : "never reached generic_prompt",
           (unsigned long long)top);
    rpt_close(game, g_rpt_seen ? "tablet left generic_prompt" : "generic_prompt not reached", 0);
    return 1;
}

static void rpt_tick(void *game, uint64_t now)
{
    if (!g_rpt_on) return;
    if (g_rpt_setup_at) {   /* after the card close: the stock prompt (REPORT / "" / CANCEL), then the layer over it */
        if (now < g_rpt_setup_at) return;
        g_rpt_setup_at = 0;
        g_rpt_setup_t = now;
        g_rpt_state_next = now;
        g_rp_pending = 3;
        g_rp_retry = 0;
        g_rpt_next = now + RPT_SETTLE_MS;
        return;
    }
    if (g_rp_pending == 3) return;   /* the set-up still waits for a free slot */
    if (rpt_state_check(game, now)) return;
    if (now - g_rpt_since > RP_TIMEOUT_MS) {
        /* back to the list (the CANCEL route): the empty stock REPORT prompt alone may have no way out ( row 4) */
        rp_log("report: tablet reasons lapsed (no press for %d s): back to user_list", RP_TIMEOUT_MS / 1000);
        rpt_close(game, "lapsed", 1);
        return;
    }
    if (g_rpt_drawn && now >= g_rpt_so.next) rpt_so_apply(game, &g_rpt_so, now);   /* stock-off re-asserted while drawn */
    if (g_rp_pending || now < g_rpt_next) return;
    int w = rpt_apply(game);
    if (g_rpt_pass == 0) rp_log("report: tablet layer writes %d", w);
    if (w > 0 && !g_rpt_drawn) RP_XCHG(&g_rpt_drawn, 1);   /*: from now on the stock footer is consumed too */
    if (g_rpt_drawn) {
        rpt_so_apply(game, &g_rpt_so, now);                    /* stock-off: the zones under the drawn layer off */
        rpt_so_release(game, &g_rpl_so, now, "layer drawn", 0);   /* our own zones live with the layer */
    }
    g_rpt_pass++;
    g_rpt_next = now + (g_rpt_pass == 1 ? 250 : g_rpt_pass == 2 ? 750 : 2000);
}

static void rpt_back_start(void *game, const char *by);

/* the card's xplatformid for the direct re-open (rpt_back_tick's read-back compares with it). At the REPORT press any
 * non-empty string is kept (as before); unreadable / empty there = one more read at the next card poll or at the back
 * start, whichever comes first, accepted only when its account equals the target's (a row tap in between may have put
 * another player on the card). Every read logs its result. */
static void rpt_bk_xp_read(void *game, const char *when)
{
    char xp[RP_NAMELEN], xm[32];
    uint64_t acct = 0, want = 0;
    int ok = rp_card_xpid(game, xp, sizeof xp, xm, sizeof xm, &acct), n = 0;
    for (const char *d = g_rp_target_id; *d >= '0' && *d <= '9' && n < 20; d++, n++) want = want * 10 + (uint64_t)(*d - '0');
    int first = !g_rpt_bk_xp_retry;
    const char *res;
    if (first && xp[0]) { snprintf(g_rpt_bk_xp, sizeof g_rpt_bk_xp, "%s", xp); res = "ok"; }
    else if (!first && ok && want && acct == want) { snprintf(g_rpt_bk_xp, sizeof g_rpt_bk_xp, "%s", xp); res = "ok on retry"; }
    else if (!first && xp[0]) { g_rpt_bk_xp[0] = 0; res = "another player's id on the card: not used"; }
    else { g_rpt_bk_xp[0] = 0; res = first ? "unreadable, retry at the next card poll" : "still unreadable: the back re-open walks rows"; }
    g_rpt_bk_xp_retry = first && !xp[0];
    rp_log("report: card target id read (%s): %s, xpid=%s", when, res, xm);
}
static void rpt_press(void *game, int z, uint64_t now)
{
    if (z == g_rpt_last_z && now - g_rpt_last_t < (z == RPT_Z_BACK ? 500 : 250)) return;   /* one press seen on both chains; "<" also arrives as the stock X  */
    g_rpt_last_z = z;
    g_rpt_last_t = now;
    if (!g_rpt_on) { if (z != RPT_Z_FOOTER) rp_log("report: tablet zone %s pressed, no tablet report: ignored", g_rpt_zone_name[z]); return; }
    g_rpt_since = now;
    if (g_rp_state != RP_POSTING) g_rp_since = now;
    if (z == RPT_Z_CANCEL || z == RPT_Z_BACK || z == RPT_Z_FOOTER) {
        /*: "<" = back ONE step = the target's card; CANCEL (and the bare prompt's stock footer, its CANCEL)
         * the same by default, the list only with report_tablet_cancel=list */
        const char *by = z == RPT_Z_CANCEL ? "CANCEL" : z == RPT_Z_BACK ? "\"<\"" : "stock footer";
        int card = z == RPT_Z_BACK || g_rpt_cancel_card;
        g_rpt_back_t = now; g_rpt_grace_rel_t = 0; RP_XCHG(&g_rpt_x_grace, 1);   /* the press goes on: the stock zones under the layer stay consumed */
        if (card) rp_log("report: tablet %s: back one step (layer closed, user_list, then the card of %s)", by,
                         g_rp_target_id[0] ? rp_mask(g_rp_target_id) : "-");
        else rp_log("report: tablet %s: cancelled (user_list, card closed: report_tablet_cancel=list)", by);
        rpt_close(game, g_rpt_zone_name[z], 1);
        if (card) rpt_back_start(game, by);
        return;
    }
    int role = z == RPT_Z_MG ? RPS_MG : z == RPT_Z_SUBMIT ? RPS_SUBMIT : z - RPT_Z_REASON1;
    if (rps_press(role, "tablet", &g_rpt_post) == 1) { g_rpt_pass = 0; g_rpt_next = now; }
}

/* ----: back one step = the report target's card, re-opened closed-loop (see the RPT_BK_* note above) ---- */
static void rpt_back_fail(const char *why)
{
    g_rpt_bk = RPT_BK_OFF;
    rp_log("report: tablet %s: back: card not re-opened (%s)", g_rpt_bk_by, why);
}

/* after rpt_close(.., 1) queued the user_list transition: the candidates, or a logged no-op */
static void rpt_back_start(void *game, const char *by)
{
    g_rpt_bk = RPT_BK_OFF;
    g_rpt_bk_by = by;
    RP_XCHG(&g_rpt_bk_user, 0);
    if (g_rpt_bk_xp_retry) rpt_bk_xp_read(game, "back start");   /* before the direct path is armed */
    snprintf(g_rpt_bk_id, sizeof g_rpt_bk_id, "%s", g_rp_target_id);
    if (!g_rpt_close_card) { rpt_back_fail("report_tablet_close_card=0: the card was never closed"); return; }
    uint64_t v = 0;
    int n = 0;
    const char *d = g_rp_target_id;
    g_rpt_bk_direct = g_rpt_bk_slot >= 0 && g_rpt_bk_xp[0];
    for (; *d >= '0' && *d <= '9' && n < 20; d++, n++) v = v * 10 + (uint64_t)(*d - '0');
    if (!n || *d || !v) {
        if (!g_rpt_bk_direct) { rpt_back_fail("no target account id: a re-open could not be verified"); return; }
        v = 0;   /* direct only: the candidate walk needs the account */
    }
    g_rpt_bk_acct = v;
    /* the matched users[] index and slot first, then the visible rows 1..RP_MAXROWS-1, row 0 (the local player) last */
    int c[2 + RP_MAXROWS], m = 0;
    c[m++] = g_rpt_bk_i;
    c[m++] = g_rpt_bk_slot;
    for (int r = 1; r < RP_MAXROWS; r++) c[m++] = r;
    c[m++] = 0;
    g_rpt_bk_nc = 0;
    for (int i = 0; i < m; i++) {
        int dup = c[i] < 0;
        for (int j = 0; j < g_rpt_bk_nc && !dup; j++) dup = g_rpt_bk_cand[j] == c[i];
        if (!dup && g_rpt_bk_nc < RPT_BK_NCAND) g_rpt_bk_cand[g_rpt_bk_nc++] = c[i];
    }
    if (!v) g_rpt_bk_nc = 0;
    g_rpt_bk_k = g_rpt_bk_tries = g_rpt_bk_busy = 0;
    g_rpt_bk_t = 0;
    g_rpt_bk = RPT_BK_WAIT;
}

/* a card showing another account (or none readable) is closed again by its own close_button, as rpt_open does */
static void rpt_back_close_card(void *game)
{
    if (rp_press_comp(game, RP_CARD, S_FULLY_DEP, S_CLOSE_BTN) && g_rp_addfriend) RP_XCHG(&g_rp_f4_close, 1);
}

/* the direct re-open did not produce the target's card: close a wrong card, then the candidate walk ONCE (or give up) */
static void rpt_back_direct_fail(void *game, uint64_t now, const char *why, int close_card)
{
    rp_log("report: tablet %s: back: direct open (slot %d) failed (%s)%s", g_rpt_bk_by, g_rpt_bk_slot, why,
           g_rpt_bk_nc ? ": candidate walk" : "");
    if (close_card) rpt_back_close_card(game);
    if (!g_rpt_bk_nc) { rpt_back_fail("direct open failed and no account id for the candidate walk"); return; }
    g_rpt_bk_direct = 0;
    g_rpt_bk_k = g_rpt_bk_tries = g_rpt_bk_busy = 0;
    g_rpt_bk_t = close_card ? now + RPT_BK_CLOSE_MS : now;
    g_rpt_bk = RPT_BK_CALL;
}

/* game thread, after rp_flush (the user_list transition runs first) */
static void rpt_back_tick(void *game, uint64_t now)
{
    if (g_rpt_bk == RPT_BK_OFF) return;
    if (RP_XCHG(&g_rpt_bk_user, 0)) {   /* the user's own row tap wins: their card is left open, nothing more is pressed */
        rpt_back_fail("the user pressed the LOBBY list: walk stopped, no card closed");
        return;
    }
    if (g_rpt_bk == RPT_BK_WAIT) {
        if (g_rp_pending) return;                       /* the user_list transition (or another tablet write) first */
        if (!g_rpt_bk_t) { g_rpt_bk_t = now + RPT_BK_LIST_MS; return; }
        if (now < g_rpt_bk_t) return;
        g_rpt_bk = g_rpt_bk_direct ? RPT_BK_DCALL : RPT_BK_CALL;
    }
    if (g_rpt_bk == RPT_BK_DCALL) {
        char why[96];
        uint64_t top = 0;
        if (rp_tablet_top(game, &top) && top != S_ST_USER_LIST && top != S_ST_PROFILE) {
            snprintf(why, sizeof why, "the tablet left the LOBBY list, top %016llx", (unsigned long long)top);
            rpt_back_fail(why);
            return;
        }
        int32_t slot = g_rpt_bk_slot;
        const void *sv = &slot;
        if (!g_rpt_bk_busy) g_rpt_bk_cleared = rp_setvar(game, RP_CARD, RP_NCA_SCRIPT, RP_XPID, "");
        int rc = rp_call_on(game, RP_CARD, RP_NCA_SCRIPT, S_OPEN_LMSC, 1, &S_P_USERID, &sv);
        if (rc == 1) {
            if (++g_rpt_bk_busy >= RPT_BK_BUSY_MAX) rpt_back_fail("card script stayed busy");
            return;
        }
        g_rpt_bk_busy = 0;
        if (rc) { rpt_back_direct_fail(game, now, "OpenLobbyMemberSocialControls refused (card not loaded)", 0); return; }
        g_rpt_bk_call_t = now;
        g_rpt_bk_dticks = 0;
        g_rpt_bk = RPT_BK_DREAD;
        return;
    }
    if (g_rpt_bk == RPT_BK_DREAD) {
        g_rpt_bk_dticks++;
        if (now - g_rpt_bk_call_t < RPT_BK_READ_MS) return;
        char xp[RP_NAMELEN];
        int rd = rp_getvar(game, RP_CARD, RP_NCA_SCRIPT, RP_XPID, xp, sizeof xp);
        if (rd && !strcmp(xp, g_rpt_bk_xp)) {
            g_rpt_bk = RPT_BK_OFF;
            g_rp_card5_seen[0] = 0;
            rp_log("report: tablet %s: back -> card of %s (slot %d, direct)%s", g_rpt_bk_by, rp_mask(g_rpt_bk_id), g_rpt_bk_slot,
                   g_rpt_bk_cleared ? "" : " [xplatformid not cleared first: a stale value cannot be told from an open, check the card]");
            return;
        }
        if (rd && xp[0]) { rpt_back_direct_fail(game, now, "the card shows another player", 1); return; }
        if (now - g_rpt_bk_call_t < RPT_BK_DIRECT_MS || g_rpt_bk_dticks < RPT_BK_DIRECT_TICKS) return;   /* deferred: empty so far */
        char why[96];
        snprintf(why, sizeof why, "%s after %llu ms / %d ticks", rd ? "no card opened (xplatformid empty)" : "card xplatformid unreadable",
                 (unsigned long long)(now - g_rpt_bk_call_t), g_rpt_bk_dticks);
        rpt_back_direct_fail(game, now, why, 0);
        return;
    }
    if (g_rpt_bk == RPT_BK_CALL) {
        if (now < g_rpt_bk_t) return;
        char why[96];
        if (g_rpt_bk_k >= g_rpt_bk_nc) {
            snprintf(why, sizeof why, "no candidate row showed the target: %d tried, the LOBBY list left open", g_rpt_bk_tries);
            rpt_back_fail(why);
            return;
        }
        uint64_t top = 0;
        if (rp_tablet_top(game, &top) && top != S_ST_USER_LIST && top != S_ST_PROFILE) {
            snprintf(why, sizeof why, "the tablet left the LOBBY list, top %016llx", (unsigned long long)top);
            rpt_back_fail(why);
            return;
        }
        if (!g_rpt_bk_busy) g_rpt_bk_pre_ok = rp_getvar(game, RP_CARD, RP_NCA_SCRIPT, RP_XPID, g_rpt_bk_pre, sizeof g_rpt_bk_pre);
        if (!g_rpt_bk_pre_ok) g_rpt_bk_pre[0] = 0;
        int32_t idx = g_rpt_bk_cand[g_rpt_bk_k];
        const void *iv = &idx;
        int rc = rp_call(game, S_USERNAME_PRESSED, 1, &S_PRESSED_USER_IDX, &iv);
        if (rc == 1) {
            if (++g_rpt_bk_busy >= RPT_BK_BUSY_MAX) rpt_back_fail("state_mgr stayed busy");
            return;
        }
        if (rc) { rpt_back_fail("usernamepressed refused (state_mgr not loaded)"); return; }
        g_rpt_bk_busy = 0;
        g_rpt_bk_tries++;
        g_rpt_bk_call_t = now;
        g_rpt_bk = RPT_BK_READ;
        return;
    }
    /* RPT_BK_READ */
    if (now - g_rpt_bk_call_t < RPT_BK_READ_MS) return;
    char xp[RP_NAMELEN], xm[32];
    uint64_t acct = 0;
    int have = rp_card_xpid(game, xp, sizeof xp, xm, sizeof xm, &acct);
    int unread = !strcmp(xm, "unread");
    int changed = !unread && (!g_rpt_bk_pre_ok || strcmp(xp, g_rpt_bk_pre));
    /* the call is deferred: a value still equal to the one before it proves nothing yet (it may be a stale target id
     * while the call opens another player); judged only once it held RPT_BK_STALE_MS */
    if (!changed && now - g_rpt_bk_call_t < RPT_BK_STALE_MS) return;
    int idx = g_rpt_bk_cand[g_rpt_bk_k];
    if (unread) {
        rpt_back_close_card(game);
        rpt_back_fail("card xplatformid unreadable: the card closed again");
        return;
    }
    if (have && acct == g_rpt_bk_acct) {
        g_rpt_bk = RPT_BK_OFF;
        g_rp_card5_seen[0] = 0;   /* the next 1 s card poll re-applies the card-open writes (report_label, ADD FRIEND) */
        rp_log("report: tablet %s: back -> card of %s (idx %d, tries %d)%s", g_rpt_bk_by, rp_mask(g_rpt_bk_id), idx, g_rpt_bk_tries,
               changed ? "" : " [xplatformid unchanged by the call: a stale value cannot be told from an open, check the card]");
        return;
    }
    rp_log("report: tablet %s: back: idx %d shows xpid=%s, not the target %s%s: card closed, next candidate", g_rpt_bk_by, idx, xm,
           rp_mask(g_rpt_bk_id), changed ? "" : " (unchanged by the call)");
    rpt_back_close_card(game);
    g_rpt_bk_k++;
    g_rpt_bk_t = now + RPT_BK_CLOSE_MS;
    g_rpt_bk = RPT_BK_CALL;
}

/* the card's REPORT: which surface takes the reasons */
static void rpp_open_preset(void *game, uint64_t now);
static void rp_card_reasons(void *game, uint64_t now)
{
    if (g_rp_treasons == RP_TR_AC) {
        if (rpt_bound(game)) { rpt_open(game, now); return; }
        if (!g_rpt_fallback_logged++) rp_log("report: report_tablet_reasons=ac but ffa_ac_report is not bound: popup");
    }
    if (g_rpp_on && g_rp_treasons != RP_TR_FIELD) rpp_open_preset(game, now);   /* report_popup=0: p23 presses are not taken */
    else rp_enter_field(now);
}

/* a popup button press (game thread) */
static void rpp_press(void *game, int b, uint64_t now)
{
    if (b == g_rpp_last_b && now - g_rpp_last_t < 250) return;   /* one press seen on both chains */
    g_rpp_last_b = b;
    g_rpp_last_t = now;
    g_rpp_since = now;
    if (g_rpp_page < 0) {   /* m3: a page still drawn after the flow ended (lapse, a failed close): CANCEL / OK still close it */
        if (b == RPP_B_LIST_CANCEL || b == RPP_B_CANCEL || b == RPP_B_OK) {
            rp_log("report: popup button %s pressed after the flow ended: closing", g_rpp_btn_name[b]);
            rpp_close(game, g_rpp_btn_name[b]);
        } else rp_log("report: popup button %s pressed, no popup flow: ignored", g_rpp_btn_name[b]);
        return;
    }
    if (g_rp_state != RP_POSTING) g_rp_since = now;   /* a popup press keeps the report flow alive (SUBMIT 4 min after the open was reset as "timeout") */
    if (b == RPP_B_LIST_CANCEL || b == RPP_B_CANCEL || b == RPP_B_OK) { rpp_close(game, g_rpp_btn_name[b]); return; }
    if (b == RPP_B_LIST_SUBMIT) { rp_log("report: popup list SUBMIT pressed (inert: pick a player first)"); return; }
    if (g_rpp_page == RPP_LIST && b < RPP_ROWS) {
        int k = g_rpp_off + b;
        if (k >= g_rp_nrows) return;
        g_rpp_sel = k;
        snprintf(g_rp_target, sizeof g_rp_target, "%s", g_rp_rows[k]);
        snprintf(g_rp_target_id, sizeof g_rp_target_id, "%s", g_rp_rowid[k]);
        g_rp_target_fake = g_rp_rowfake[k];
        g_rpp_reason = -1;
        g_rpp_mg = 0;
        rp_log("report: popup row %d -> target #%04x id %s%s", k, rp_tag(g_rp_target), g_rp_target_id[0] ? rp_mask(g_rp_target_id) : "-",
               g_rp_target_fake ? " (TEST ROW)" : "");
        rpp_show(game, RPP_REASONS, now);
    } else if (g_rpp_page == RPP_REASONS && ((b >= RPP_B_REASON1 && b < RPP_B_REASON1 + RP_NREASON) || b == RPP_B_MG || b == RPP_B_SUBMIT)) {
        int role = b == RPP_B_MG ? RPS_MG : b == RPP_B_SUBMIT ? RPS_SUBMIT : b - RPP_B_REASON1;
        if (rps_press(role, "popup", &g_rpp_post) == 1) { g_rpp_pass = 0; g_rpp_next = now; }
    }
}

/* ---- p22 list scroll  ---- */
static int rpp_max_off(void) { return g_rp_nrows > RPP_VIS ? g_rp_nrows - RPP_VIS : 0; }

/* moves the first visible name by d rows (clamped); 1 = it moved (rows rewritten at once, one log line) */
static int rpp_scroll_by(int d, uint64_t now, const char *src)
{
    int o = g_rpp_off + d, m = rpp_max_off();
    if (o < 0) o = 0;
    if (o > m) o = m;
    if (o == g_rpp_off) return 0;
    g_rpp_off = o;
    g_rpp_since = now;   /* scrolling is activity: the list does not lapse under a reader (m3) */
    g_rpp_pass = 0;
    g_rpp_next = now;
    rp_log("report: list scroll %+d (%s) -> names %d-%d of %d, peek %s", d, src, o + 1,
           o + RPP_VIS < g_rp_nrows ? o + RPP_VIS : g_rp_nrows, g_rp_nrows, o + RPP_VIS < g_rp_nrows ? "shown" : "hidden");
    return 1;
}

/* per tick while p22 is ours: the stick / wheel amount, else the hover dwell, plus the event steps */
static void rpp_scroll_tick(void *game, uint64_t now)
{
    if (g_rpp_page != RPP_LIST || g_rpp_scroll_mode == RPP_SCR_OFF) return;
    int ev = RP_XCHG(&g_rpp_scr_ev, 0);
    if (ev) rpp_scroll_by(ev, now, "event");
    uint64_t dt = now - g_rpp_scroll_t;
    g_rpp_scroll_t = now;
    if (dt > 100) dt = 100;   /* a stalled frame does not jump the list */
    float a = 0.0f;
    int stick = g_rpp_scroll_mode != RPP_SCR_HOVER && rp_scroll_amount(game, &a);
    int src = stick ? 1 : 0;
    if (src != g_rpp_src) {
        rp_log("report: list scroll source %s (report_list_scroll=%s)", stick ? "CR15PointerCS::ScrollAmount"
               : g_rpp_scroll_mode == RPP_SCR_STICK ? "none (stick reader unavailable)" : "hover dwell", g_rpp_scr_name[g_rpp_scroll_mode]);
        g_rpp_src = src;
    }
    if (stick) {
        if (g_rpp_scroll_probe && a != 0.0f) rp_log("report: scroll probe amount %.3f", (double)a);
        a *= (float)g_rpp_scroll_sign;
        if (a > -RPP_SCROLL_DEAD && a < RPP_SCROLL_DEAD) {
            /* a run of 1-2 ticks that made no step = a wheel notch impulse: one row */
            if (g_rpp_run > 0 && g_rpp_run <= 2 && !g_rpp_run_steps) rpp_scroll_by(g_rpp_run_sign, now, "impulse");
            g_rpp_run = g_rpp_run_steps = 0;
            g_rpp_acc = 0;
            return;
        }
        int sg = a > 0 ? 1 : -1;
        if (g_rpp_run && sg != g_rpp_run_sign) { g_rpp_run = g_rpp_run_steps = 0; g_rpp_acc = 0; }
        g_rpp_run++;
        g_rpp_run_sign = sg;
        g_rpp_acc += a * RPP_SCROLL_RATE * (float)dt / 1000.0f;
        while (g_rpp_acc >= 0.999f || g_rpp_acc <= -0.999f) {
            int d = g_rpp_acc > 0 ? 1 : -1;
            g_rpp_acc -= (float)d;
            g_rpp_run_steps++;
            if (!rpp_scroll_by(d, now, "stick")) { g_rpp_acc = 0; break; }   /* clamped: nothing builds up at an end */
        }
        return;
    }
    if (g_rpp_scroll_mode == RPP_SCR_STICK) return;
    /* hover dwell: the peek row scrolls down, row 1 (while off > 0) scrolls up */
    int h = g_rpp_hov;
    if (h != g_rpp_hov_dir) { g_rpp_hov_dir = h; g_rpp_hov_next = now + RPP_HOVER_FIRST_MS; }
    if (!h || now < g_rpp_hov_next) return;
    if (h < 0 && g_rpp_off == 0) return;
    rpp_scroll_by(h, now, "hover");
    g_rpp_hov_next = now + RPP_HOVER_REPEAT_MS;
}

/* per tick while a popup page is ours: writes at once, +250 ms, +1 s, then every 2 s; the flow lapses after
 * RP_TIMEOUT_MS without a press */
static void rpp_tick(void *game, uint64_t now)
{
    if (g_rpp_page < 0) return;
    if (now - g_rpp_since > RP_TIMEOUT_MS) {   /* m3: closed as CANCEL closes it (p1 back, or the pause menu off in preset mode) */
        rp_log("report: popup flow lapsed (no press for %d s)", RP_TIMEOUT_MS / 1000);
        if (rp_has_actor(game, g_rpp_owner[g_rpp_page])) rpp_close(game, "lapsed");
        else { g_rpp_page = -1; g_rpp_preset = 0; g_rpp_wait = 0; g_rpp_reassert_t = 0; }   /* the pause menu already closed */
        return;
    }
    if (g_rpp_reassert_t && now >= g_rpp_reassert_t) {   /* M3: p1's own enable may have come after ours */
        g_rpp_reassert_t = 0;
        if (g_rpp_page == RPP_REASONS) {
            int ok = rp_show_page(game, g_rpp_owner[RPP_REASONS]);
            rp_log("report: popup page %s re-enabled +%d ms after the preset (%s)", g_rpp_page_name[RPP_REASONS], RPP_REASSERT_MS,
                   ok ? "enabled" : "NOT enabled");
            g_rpp_pass = 0;
            g_rpp_next = now;
        }
    }
    if (now < g_rpp_next) return;
    int w = rpp_apply(game);
    if (g_rpp_pass == 0) rp_log("report: popup %s writes %d", g_rpp_page_name[g_rpp_page], w);
    g_rpp_pass++;
    g_rpp_next = now + (g_rpp_pass == 1 ? 250 : g_rpp_pass == 2 ? 750 : 2000);
}

/* QUIT WARNING's test (game thread, no HTTP): 1 = warn, i.e. the reader sees a client lobby whose match type is not
 * private and not social_2.0* (any case); why = the reason either way. No reader / no lobby = no warning. */
static int rp_public_match(void *game, char *why, size_t cap)
{
    static sess_snapshot m;   /* game thread only */
    int rc = rp_sess(game, SESS_META, &m);
    if (rc != SESS_OK) { snprintf(why, cap, "session reader unavailable: %s", rp_sess_why(rc)); return 0; }
    if (!m.has_lob) { snprintf(why, cap, "session reader: no client lobby"); return 0; }
    static const char soc[] = "social_2.0";
    int social = 1;
    for (int k = 0; soc[k] && social; k++) {
        char c = m.match_type[k];
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        social = c == soc[k];
    }
    snprintf(why, cap, "match_type %s, private %s (session reader)", m.match_type, m.is_private ? "true" : "false");
    return !m.is_private && !social;
}

/* per-frame, first thing in the tick (any level) */
/* report=auto: the flow is live only while the popup marker page is loaded (our tree); once a second, logged on change.
 * Losing the marker drops any flow in progress. report=1: always live. */
static void rp_live_tick(void *game, uint64_t now)
{
    if (g_rp_on > 0) { if (g_rp_live != 1) { g_rp_live = 1; rp_log("report: report=1: flow on (any level)"); } return; }
    if (now < g_rp_next_live) return;
    g_rp_next_live = now + 1000;
    /* the popup pages load only while the pause menu shows, so the five-tile card (our report_label, loaded with
     * every MP level) is a marker too; decided once per card gamespace (Quest FindPrimitiveID logs a missing element). */
    static uintptr_t five_key;
    static int five;
    uintptr_t key = rp_actor_key(game, RP_CARD);
    if (key != five_key) { five_key = key; five = key ? rp_has_el(game, RP_CARD, S_REPORT_LABEL) : 0; }
    int pop = rp_has_actor(game, g_rpp_owner[RPP_LIST]);
    int m = pop || five;
    if (m == g_rp_live) return;
    if (m) rp_log("report: auto: %s loaded: flow on (tile uid %016llx)", pop ? "popup marker ffa_rp_list_page"
                  : "five-tile card (report_label)", (unsigned long long)g_rp_tile_uid);
    else {
        rp_log("report: auto: neither %s nor a five-tile card loaded: flow inert (stock content, no writes)",
               g_rpp_page_name[RPP_LIST]);
        rpt_close(game, "popup marker gone", 0);
        if (g_rpt_bk) rpt_back_fail("popup marker gone");
        rp_reset("popup marker gone");
        g_rpp_page = -1;
        g_rpp_preset = 0;
        g_rpp_wait = g_rpp_post = 0;
        RP_XCHG(&g_rp_ev_tile, 0); RP_XCHG(&g_rpp_ev, -1); RP_XCHG(&g_rp_ev_card, 0); RP_XCHG(&g_rp_ev_press, -1);
    }
    g_rp_live = m;
}

static void report_tick(void *game, uint64_t now)
{
    rpt_guard(game, now);   /* M1: before every gate */
    g_rp_now = now;
    rpt_so_tick(game, &g_rpt_so, now);   /* stock-off: a post-close hold ends even when the flow went inert */
    rpc_tick(game, now, g_rp_on && g_rp_ready && g_rp_live == 1);   /* the closed card's tiles (released when the flow is inert) */
    if (!g_rp_on || !g_rp_ready) return;
    rp_live_tick(game, now);
    if (g_rp_live != 1) return;
    /* entry A: the five-tile check, else keep the v1 label reading REPORT (the card's enable thread rewrites it
     * on every open). A five-tile canvas (report_label present) is never relabelled. */
    if (now >= g_rp_next_card) {
        char cur[32] = "";
        g_rp_next_card = now + 1000;
        if (g_rpt_bk_xp_retry) rpt_bk_xp_read(game, "card poll");
        rp_card5_check(game, now);
        rpt_bound(game);
        if (g_rp_five != 1 && g_rp_card_event && rp_gettext(game, RP_CARD, g_rp_card_el, cur, sizeof cur) && strcmp(cur, "REPORT")
            && rp_settext(game, RP_CARD, g_rp_card_el, "REPORT")) {
            if (!g_rp_card_logged) {
                g_rp_card_logged = 1;
                rp_log("report: card label relabelled REPORT");
            }
            rp_card_opened(game);
        }
    }
    int pb = RP_XCHG(&g_rpp_ev, -1);
    if (pb >= 0 && pb < RPP_NB) rpp_press(game, pb, now);
    int tz = RP_XCHG(&g_rpt_ev, -1);
    if (tz >= 0 && tz < RPT_NZ) rpt_press(game, tz, now);
    if (RP_XCHG(&g_rpt_x_ev, 0)) {   /*: the stock X under the layer; PC fires "<" too (rpt_press dedupes), Quest only the X */
        if (!g_rpt_x_latch || now - g_rpt_x_t >= RPT_MB_PRESS_GAP_MS) {
            g_rpt_x_latch = 1;
            /* in the grace the layer is already gone; the X event is only eaten, there is no "<" to run */
            if (g_rpt_on) { rp_log("report: tablet: stock X under the layer: press -> \"<\""); rpt_press(game, RPT_Z_BACK, now); }
            else rp_log("report: tablet: stock X under the layer (grace): press consumed, the layer already closed");
        }
        g_rpt_x_t = now;
    }
    int xr = RP_XCHG(&g_rpt_x_rel, 0);
    if (xr) g_rpt_x_latch = 0;   /* the out event: the next press acts again */
    int mb = RP_XCHG(&g_rpt_mb_ev, -1);
    int mr = RP_XCHG(&g_rpt_mb_rel, -1);
    /* the grace ends with the closing press (its X / menubutton out event, RPT_X_GRACE_REL_MS later) or by time, so a
     * deliberate X or LOBBY tab press right after "<" is not eaten for the whole RPT_X_GRACE_MS */
    if (g_rpt_x_grace && !g_rpt_on && (xr || mr >= 0) && !g_rpt_grace_rel_t) g_rpt_grace_rel_t = now ? now : 1;
    if (g_rpt_x_grace && (now - g_rpt_back_t >= RPT_X_GRACE_MS || (g_rpt_grace_rel_t && now - g_rpt_grace_rel_t >= RPT_X_GRACE_REL_MS))) {
        rp_log("report: tablet: grace ended %s (%llu ms after the close)", g_rpt_grace_rel_t ? "by the press release" : "by time",
               (unsigned long long)(now - g_rpt_back_t));
        RP_XCHG(&g_rpt_x_grace, 0); g_rpt_grace_rel_t = 0;
    }
    if (RP_XCHG(&g_rpt_ok_ev, 0) && g_rpt_res) {   /* OK on the result prompt -> the LOBBY list (card closed) */
        RP_XCHG(&g_rpt_res, 0);
        g_rpt_back_t = now; g_rpt_grace_rel_t = 0; RP_XCHG(&g_rpt_x_grace, 1);   /* the rest of the press stays consumed */
        if (g_rp_pending) rp_log("report: tablet result OK: tablet write %d pending, LOBBY list after it", g_rp_pending);
        g_rp_pending = 4; g_rp_retry = 0;
        rp_log("report: tablet result OK: LOBBY list (card closed)");
    } else if (g_rpt_res && now >= g_rpt_res_next) {   /* the result prompt left by the stock (X, a tab): nothing to take */
        uint64_t top = 0;
        g_rpt_res_next = now + RPT_STATE_MS;
        if (rp_tablet_top(game, &top) && top != S_ST_PROMPT_T) {
            RP_XCHG(&g_rpt_res, 0);
            rp_log("report: tablet result prompt left (top %016llx)", (unsigned long long)top);
        }
    }
    if (mb >= 0 && mb < RPT_NMB) {   /* one log line per press (in / hold / out, both chains), not per event */
        if (mb != g_rpt_mb_last || now - g_rpt_mb_t >= RPT_MB_PRESS_GAP_MS)
            rp_log("report: tablet: stock %s under the layer: press consumed%s", g_rpt_mb_label[mb], g_rpt_on ? "" : " (grace)");
        g_rpt_mb_last = mb; g_rpt_mb_t = now;
    }
    if (mr >= 0 && mr == g_rpt_mb_last) g_rpt_mb_last = -1;   /* the out event: the next press of this button logs again */
    if (g_rp_state != RP_IDLE && now - g_rp_since > RP_TIMEOUT_MS) {
        if (g_rpp_post) g_rpp_orphan = 1;   /* a popup POST still running: a late result is logged, never shown on the tablet */
        rp_reset("timeout");
        g_rpp_wait = g_rpp_post = 0;
    }
    /* entry B, staged (opening the tablet in the frame the pause menu closes left a blank tablet): close the pause menu,
     * open the tablet 1 s later, select the stock LOBBY tab 1.5 s after that, and stop there (D-S2: native scroll, the
     * report goes on from a row's card; no custom write). */
    int tile = RP_XCHG(&g_rp_ev_tile, 0);
    if (tile && g_rpp_on) {   /* the popup pages when their marker page is loaded */
        int m = rp_has_actor(game, g_rpp_owner[RPP_LIST]);
        if (m != g_rpp_marker) {
            g_rpp_marker = m;
            rp_log(m ? "report: popup marker %s loaded: the REPORT tile opens the popup pages"
                     : "report: popup marker %s not loaded: the REPORT tile opens the tablet", g_rpp_page_name[RPP_LIST]);
        }
        if (m) {
            tile = 0;
            if (g_rp_state == RP_POSTING || g_rpp_wait) rp_log("report: popup tile press ignored (a report is posting / waiting)");
            else if (g_rpp_page >= 0 && now - g_rpp_since < 500) {}   /* one press seen on both chains */
            else { rp_log("report: pause tile pressed (popup)"); g_rp_entry = "pause"; rpp_open(game, now); }
        }
    }
    if (tile && g_rp_state == RP_IDLE) {
        int a = rp_fire(game, RP_PAUSE_P1, S_TOGGLE_PAUSE);
        rp_log("report: pause tile pressed (pause close %s)", a ? "sent" : "not sent");
        g_rp_entry = "tablet";
        g_rp_state = RP_OPENING;
        g_rp_open_step = 0;
        g_rp_since = now;
        g_rp_target[0] = g_rp_target_id[0] = 0;
        g_rp_target_fake = 0;
    }
    if (g_rp_state == RP_OPENING) {
        if (g_rp_open_step == 0 && now - g_rp_since >= 1000) {
            g_rp_open_step = 1;
            if (!rp_fire(game, RP_TABLET, S_TABLET_OPEN)) rp_log("report: tablet open not sent");
        } else if (g_rp_open_step == 1 && now - g_rp_since >= 2500) {
            g_rp_open_step = 2;
            int a = rp_fire(game, RP_TABLET, S_TABLET_LOBBY);
            rp_log("report: tablet LOBBY tab %s: pick a player, then REPORT on the card", a ? "sent" : "not sent");
            g_rp_state = RP_IDLE;
            g_rp_entry = "";
        }
    }
    if (RP_XCHG(&g_rp_ev_card_closed, 0)) rp_log("report: card REPORT press ignored (the card is closed: nothing drawn there)");
    int card = RP_XCHG(&g_rp_ev_card, 0);
    if (card && (g_rp_state == RP_POSTING || g_rpp_wait || g_rpt_post)) {   /* as the pause tile: one report at a time (m1) */
        rp_log("report: card REPORT press ignored (a report is posting / waiting)");
        card = 0;
    }
    if (card) {
        char name[RP_NAMELEN] = "";
        g_rp_target_fake = 0;   /* the card's target is a real player */
        rp_gettext(game, RP_CARD, S_NAME_TEXT, name, sizeof name);
        const sess_player *p = NULL;
        int reader = rp_rows_from_reader(game);
        char xp[RP_NAMELEN], xm[32];
        uint64_t acct = 0;
        int have_xp = 0;
        if (reader) p = rp_card_target(game, name, "card press");
        else have_xp = rp_card_xpid(game, xp, sizeof xp, xm, sizeof xm, &acct);
        if (reader && !p) {
            rp_log("report: card press, target not resolved");
            rp_prompt(g_rp_msg[RP_MSG_NO_TARGET]);
        } else if (!reader && !name[0] && !have_xp) {
            rp_log("report: card press, no name and no xplatformid on the card");
        } else {
            if (reader) { rp_set_target(p); g_rpt_bk_i = p->i; g_rpt_bk_slot = p->slot;   /* the back re-open: direct slot + id, first candidates */
                g_rpt_bk_xp_retry = 0; rpt_bk_xp_read(game, "REPORT press"); }
            else {
                g_rpt_bk_i = g_rpt_bk_slot = -1; g_rpt_bk_xp[0] = 0; g_rpt_bk_xp_retry = 0;
                /* no reader: the id from the card itself when it parses; the host job adds the session id and match
                 * type, and fills the id by name only when the card gave none */
                snprintf(g_rp_target, sizeof g_rp_target, "%s", name[0] ? name : "PLAYER");
                if (have_xp) snprintf(g_rp_target_id, sizeof g_rp_target_id, "%llu", (unsigned long long)acct);
                else g_rp_target_id[0] = 0;
                rp_log("report: card target (no reader) xpid=%s shown=#%04x", xm, rp_tag(name));
                rp_start_job(RP_JOB_HOST);
            }
            g_rp_entry = "card";
            rp_log("report: card press, target #%04x id %s (%s)", rp_tag(g_rp_target),
                   g_rp_target_id[0] ? rp_mask(g_rp_target_id) : "-",
                   reader ? "session reader" : have_xp ? "card xplatformid, host fallback pending" : "host fallback pending");
            g_rpp_preset = 0;
            rp_card_reasons(game, now);
        }
    }
    if (g_rp_job == RP_JOB_HOST && g_rp_job_done) {
        int st = g_rp_job_status;
        RP_XCHG(&g_rp_job, RP_JOB_NONE);
        if (st == 200) { rp_parse_host(g_rp_resp); rp_add_fake_rows(); } else g_rp_nrows = 0;
        rp_log("report: rows from host fallback: /reports/players %d (%s), %d other player(s)", st,
               st == 200 ? g_rp_sess_type : st == 404 ? "not_in_session" : "failed", g_rp_nrows);
        if (g_rp_state == RP_WAIT_SESSION && g_rpp_wait) {
            g_rpp_wait = 0;
            g_rp_state = RP_IDLE;
            if (st != 200) rpp_done(game, g_rp_msg[RP_MSG_NO_SESSION], now);
            else if (!g_rp_nrows) rpp_done(game, g_rp_msg[RP_MSG_NO_PLAYERS], now);
            else rpp_show(game, RPP_LIST, now);
        } else if (g_rp_target[0] && !g_rp_target_id[0]) rp_match_target_id();
    }
    if (g_rp_job == RP_JOB_POST && g_rp_job_done) {
        int st = g_rp_job_status;
        RP_XCHG(&g_rp_job, RP_JOB_NONE);
        if (st == RP_ST_TEST) rp_log("report: test row: POST skipped%s", g_rpp_post ? " (popup)" : "");
        else rp_log("report: POST /reports %d%s", st, g_rpp_post ? " (popup)" : "");
        if (g_rpp_orphan) {   /* the popup was closed while posting: p24 is not enabled over whatever the user went to */
            g_rpp_orphan = 0;
            rp_log("report: popup result '%s' not shown (the popup was closed while posting)", g_rp_msg[rp_result_msg(st)]);
        } else if (g_rpp_post) {
            g_rpp_post = 0;
            rpp_done(game, g_rp_msg[rp_result_msg(st)], now);   /* the same string as the tablet prompt below */
        } else {
            if (g_rpt_post) { g_rpt_post = 0; rpt_close(game, "SUBMIT result", 0); }   /* the stock prompt shows the result */
            if (g_rpt_orphan) {   /* the tablet left the panel while posting: the result is not pushed onto it */
                g_rpt_orphan = 0;
                rp_log("report: tablet result '%s' not shown (the tablet left the panel while posting)", g_rp_msg[rp_result_msg(st)]);
            } else rp_prompt(g_rp_msg[rp_result_msg(st)]);
        }
        g_rp_state = RP_IDLE;
    }
    int press = RP_XCHG(&g_rp_ev_press, -1);
    /* a row press while a redraw is still pending (busy slot, retried up to 60 ticks) belongs to the rows still shown, not
     * to the row kinds already set for the redraw: dropped, the user presses again on the new rows */
    if (press >= 0 && g_rp_pending) { rp_log("report: row %d press ignored (tablet redraw %d pending)", press, g_rp_pending); press = -1; }
    if (press >= 0 && g_rp_state == RP_FIELD && !rp_press_guard(game)) rp_field_press(press, now);
    rp_flush(game);
    rpt_back_tick(game, now);   /*: after the flush, so the user_list transition runs first */
    rpp_preset_tick(game, now);
    rpp_scroll_tick(game, now);
    rpp_tick(game, now);
    rpt_tick(game, now);
}

#endif
