/* sess_snapshot.h — in-process read of the net game's session (who is here, which lobby), PC and Quest.
 *
 * Header-only C. No JSON (unless SESS_SNAPSHOT_JSON is defined before the include), no allocation, no HTTP, no
 * state across calls. One reader for rad-menu (Report Player list, QUIT WARNING) on PC and Quest, called from the GAME THREAD (the name
 * accessor reads the server-profile JSON that the net tick rewrites).
 *
 * 6323983201049540 (echovr.exe), Quest final 6528386917200980 (libr15.so). Every offset below is the same on both
 * (disasm-only, cross-build confirmed P 6323983201049540 <-> Q 6528386917200980) unless the profile says otherwise.
 *
 *   ng      = [game + o_game_netgame]       PC +0x8518 (device-observed P1 2026-10-31: [ng] = 9, [ng+8] == game);
 *                                           Quest +0x8510 (CR15Game::NetGame 0x12027c8 = ldr x0,[x0,#0x8510]).
 *   [ng]    u32 state (9 = in a level);     [ng+8] = the CR15Game* (sanity check: must equal `game`)
 *   counts  u16 ng+0xdc users, +0xde local users (users[0..local) are this client's), +0xe0, +0xe2 non-replay
 *   users   ng+0x178 + i*0x250 (SR15NetUser): u16 head slot = &0x1f, teamslot = bits 5-9 (inferred: the bits
 *           between), teamid = bits 10-12 (>= 5 = empty; the lobby uses 3, device-observed P3 PC); +0x8 SActorGUID
 *           {u64 gamespace sym, u64 actor} (raw: NOT the `nav` self handle, device-observed P2 PC)
 *   index   ng+0x138 + slot*4 = {u16 index, u16 gen}
 *   lob     [ng+0x40] (null outside a lobby); match_type char* [lob+0x460] (case varies: "Social_2.0_Private",
 *           device-observed P4 PC), map_name char* [lob+0x470]
 *   nsl     lob+0x18 (CNSLobby, an address, not loaded); entrants [nsl+0x360], count [nsl+0x368], stride 0xd8:
 *           +0x0 SNSUserID {u64 provider, u64 account}, +0x8a u16 ping. This is the whole body of
 *           CNSLobby::EntrantUserID / EntrantPing (P 0x6093c0 / 0x6084d0, Q 0x192d1b0 / 0x192d3b4, disasm-only),
 *           read here instead of called; the callers' prologue checks pin those bodies.
 *   lobbyid nsl+0x1cc, 16 B SUuid {u32, u16, u16, u8[8]}: CNSLobby::LobbyId is `lea rax,[rcx+0x1cc]; ret` (P 0x60bf50)
 *           / `add x0,x0,#0x1cc; ret` (Q 0x1918c74), disasm-only both. sessionid = the SUuid::ToString format found in
 *           both binaries, "%08X-%04hX-%04hX-%02hhX%02hhX-%02hhX x6" (Q SUuid::ToString 0x11f1650 reads u32 +0, u16 +4,
 *           +6, u8 +8..+15), formatted here; that /session passes that same format is inferred (probe P6 diff).
 *   userid  lobby entrant (nsl, slot) for every user. Local users (i < n_local) take CNSUser::UserID([ng+0x28d0])
 *           when the profile has `local_userid` (Quest dynsym 0x1919c9c: {[u+0x90], vcall [[u]+0x70](u)}; the PC
 *           function is not pinned, so PC stays on the entrant row, which carries the local user too: inferred).
 *   name    CR15NetGame::UserName(ng, u16 index) -> const char* (P RVA 0x1c98c0, Q 0x124f73c, dynsym
 *           _ZNK10NRadEngine8NRadGame11CR15NetGame8UserNameEt): serverprofile displayname -> lobby entrant name ->
 *           "Unknown"; "Replayer %u" at index >= [ng+0xe2]. The only engine call; NULL in the profile = no names.
 *   private / tournament: derived from match_type (case-insensitive), not from the lob vtable gametype call
 *           (Q slot +0x28 / P +0x20 not pinned for a safe call): private = social_2.0_private, echo_arena_private,
 *           echo_combat_private, echo_demo, *_private_ai, *_npe; tournament = echo_arena_tournament,
 *           echo_combat_tournament (inferred: match_type is the gametype's name).
 *
 * TODO (not read in v1, no pinned address yet): head transform
 * (ResolveActorAlias -> CTransformCS chain), teamname, number / level, packet loss, the meaning of +0xe0.
 *
 * Memory access goes through the profile's `read` (0 = copied, nonzero = fault): PC wraps memcpy in __try, Quest
 * uses safe_copy (pipe probe). The name call is not fault-guarded by this header: PC callers run sess_read inside
 * __try; Quest callers must only pass `username` once its prologue check passed. */
#ifndef SESS_SNAPSHOT_H
#define SESS_SNAPSHOT_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef _MSC_VER
#define SESS_API static __inline
#else
#define SESS_API static inline
#endif

typedef int (*sess_read_fn)(void *dst, uintptr_t src, size_t n);       /* 0 = ok */
typedef const char *(*sess_username_fn)(void *ng, uint16_t index);    /* CR15NetGame::UserName(u16) const */
typedef struct { uint64_t provider, account; } sess_userid;           /* SNSUserID, 16 B */
typedef sess_userid (*sess_local_userid_fn)(void *cnsuser);           /* CNSUser::UserID const: AArch64 x0:x1 ONLY
                                                                       * (MSVC x64 would return it through a hidden
                                                                       * pointer: never set this on PC) */

typedef struct {
    const char *build;          /* "6323983201049540" / "6528386917200980" (goes into the snapshot) */
    uint32_t o_game_netgame;    /* PC 0x8518, Quest 0x8510 */
    sess_read_fn read;          /* required */
    sess_username_fn username;  /* optional: NULL = names stay "" */
    sess_local_userid_fn local_userid;   /* optional (Quest): local users' id from CNSUser, else the entrant row */
} sess_profile;

#define SESS_O_STATE       0x0
#define SESS_O_GAME        0x8
#define SESS_O_LOB         0x40
#define SESS_O_N_USERS     0xdc
#define SESS_O_N_LOCAL     0xde
#define SESS_O_N_E0        0xe0
#define SESS_O_N_LIVE      0xe2
#define SESS_O_INDEX       0x138
#define SESS_O_USERS       0x178
#define SESS_USER_STRIDE   0x250
#define SESS_MAX_USERS     16          /* the index table ng+0x138..0x178 holds 16 slots */
#define SESS_O_MATCH_TYPE  0x460
#define SESS_O_MAP_NAME    0x470
#define SESS_O_NSL         0x18
#define SESS_O_LOBBYID     0x1cc       /* in the CNSLobby */
#define SESS_O_CNSUSER     0x28d0      /* in the net game: the local CNSUser* */
#define SESS_O_ENTRANTS    0x360
#define SESS_O_N_ENTRANTS  0x368
#define SESS_ENTRANT_STRIDE 0xd8
#define SESS_O_ENT_PING    0x8a
#define SESS_TEAM_EMPTY    5

enum { SESS_OK = 0, SESS_NO_GAME = -1, SESS_NO_NETGAME = -2, SESS_BAD_NETGAME = -3, SESS_FAULT = -4 };
enum { SESS_META = 1, SESS_PLAYERS = 2, SESS_ALL = 3 };

typedef struct {
    uint16_t i, slot, gen, team, teamslot;
    uint8_t local;              /* i < n_local */
    uint8_t has_entrant;        /* slot < the lobby's entrant count: ping (and userid, unless from CNSUser) valid */
    uint8_t has_userid;         /* userid / provider valid (entrant row or CNSUser) */
    uint8_t userid_cnsuser;     /* 1 = userid came from CNSUser::UserID (local user, Quest) */
    uint16_t ping;
    uint64_t provider, userid;  /* SNSUserID {providerid, accountid} */
    uint64_t gs, actor;         /* SActorGUID, raw */
    char name[40];
} sess_player;

typedef struct {
    const char *build;
    uint64_t ng, lob;
    uint32_t state;
    uint16_t n_users, n_local, n_e0, n_live;
    int has_lob;                /* 0: match_type / map_name / private / tournament unknown */
    char match_type[48], map_name[48];
    int has_sessionid;
    uint8_t lobbyid[16];        /* raw SUuid at nsl+0x1cc */
    char sessionid[40];         /* "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX" */
    int is_private, is_tournament;   /* -1 = unknown (no lob) */
    int local_index, local_slot;     /* -1 = no local user */
    int n_players;
    sess_player players[SESS_MAX_USERS];
} sess_snapshot;

/* a NUL-ended string at `src` into out[cap], read in chunks that never cross a page; "" on fault */
SESS_API int sess__str(const sess_profile *p, uintptr_t src, char *out, size_t cap)
{
    size_t n = 0;
    out[0] = 0;
    if (!src || cap < 2) return -1;
    while (n < cap - 1) {
        size_t want = cap - 1 - n, page = 4096 - ((src + n) & 4095);
        if (want > page) want = page;
        if (p->read(out + n, src + n, want)) { out[n] = 0; return -1; }
        for (size_t k = 0; k < want; k++)
            if (!out[n + k]) return 0;
        n += want;
    }
    out[cap - 1] = 0;
    return 0;
}

SESS_API int sess__ieq(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        char x = *a >= 'A' && *a <= 'Z' ? *a + 32 : *a, y = *b >= 'A' && *b <= 'Z' ? *b + 32 : *b;
        if (x != y) return 0;
    }
    return *a == *b;
}

SESS_API int sess__iends(const char *s, const char *suf)
{
    size_t a = strlen(s), b = strlen(suf);
    return a >= b && sess__ieq(s + a - b, suf);
}

SESS_API int sess_is_private(const char *mt)
{
    return sess__ieq(mt, "social_2.0_private") || sess__ieq(mt, "echo_arena_private")
        || sess__ieq(mt, "echo_combat_private") || sess__ieq(mt, "echo_demo")
        || sess__iends(mt, "_private_ai") || sess__iends(mt, "_npe");
}

SESS_API int sess_is_tournament(const char *mt)
{
    return sess__ieq(mt, "echo_arena_tournament") || sess__ieq(mt, "echo_combat_tournament");
}

/* Fill *s. want = SESS_META (no per-user loop, no engine call), SESS_PLAYERS or SESS_ALL. Returns SESS_*. */
SESS_API int sess_read(void *game, const sess_profile *p, int want, sess_snapshot *s)
{
    memset(s, 0, sizeof *s);
    s->build = p->build;
    s->is_private = s->is_tournament = -1;
    s->local_index = s->local_slot = -1;
    if (!game) return SESS_NO_GAME;
    uint64_t ng = 0, g = 0, lob = 0;
    if (p->read(&ng, (uintptr_t)game + p->o_game_netgame, 8)) return SESS_FAULT;
    if (!ng) return SESS_NO_NETGAME;
    s->ng = ng;
    uint8_t hdr[0x10];
    if (p->read(hdr, ng, sizeof hdr)) return SESS_FAULT;
    memcpy(&s->state, hdr + SESS_O_STATE, 4);
    memcpy(&g, hdr + SESS_O_GAME, 8);
    if (g != (uint64_t)(uintptr_t)game) return SESS_BAD_NETGAME;
    uint16_t c[4];
    if (p->read(c, ng + SESS_O_N_USERS, sizeof c)) return SESS_FAULT;
    s->n_users = c[0]; s->n_local = c[1]; s->n_e0 = c[2]; s->n_live = c[3];
    if (p->read(&lob, ng + SESS_O_LOB, 8)) return SESS_FAULT;
    s->lob = lob;
    uint64_t ents = 0, nents = 0;
    if (lob) {
        uint64_t sp[3];   /* match_type, (u64 symbol), map_name */
        if (!p->read(sp, lob + SESS_O_MATCH_TYPE, sizeof sp)) {
            s->has_lob = 1;
            sess__str(p, sp[0], s->match_type, sizeof s->match_type);
            sess__str(p, sp[2], s->map_name, sizeof s->map_name);
            s->is_private = sess_is_private(s->match_type);
            s->is_tournament = sess_is_tournament(s->match_type);
        }
        if (!p->read(s->lobbyid, lob + SESS_O_NSL + SESS_O_LOBBYID, 16)) {
            const uint8_t *b = s->lobbyid;
            uint32_t d1; uint16_t d2, d3;
            memcpy(&d1, b, 4); memcpy(&d2, b + 4, 2); memcpy(&d3, b + 6, 2);
            static const char hx[] = "0123456789ABCDEF";
            char *o = s->sessionid;
            for (int k = 7; k >= 0; k--) *o++ = hx[(d1 >> (4 * k)) & 15];
            *o++ = '-';
            for (int k = 3; k >= 0; k--) *o++ = hx[(d2 >> (4 * k)) & 15];
            *o++ = '-';
            for (int k = 3; k >= 0; k--) *o++ = hx[(d3 >> (4 * k)) & 15];
            for (int k = 8; k < 16; k++) {
                if (k == 10 || k == 8) *o++ = '-';
                *o++ = hx[b[k] >> 4]; *o++ = hx[b[k] & 15];
            }
            *o = 0;
            s->has_sessionid = 1;
        }
        if (p->read(&ents, lob + SESS_O_NSL + SESS_O_ENTRANTS, 8)
            || p->read(&nents, lob + SESS_O_NSL + SESS_O_N_ENTRANTS, 8) || nents > 64)
            ents = nents = 0;
    }
    uint16_t n = s->n_users > SESS_MAX_USERS ? SESS_MAX_USERS : s->n_users;
    if (s->n_local && n) {
        uint16_t h0;
        if (!p->read(&h0, ng + SESS_O_USERS, 2)) { s->local_index = 0; s->local_slot = h0 & 0x1f; }
    }
    if (!(want & SESS_PLAYERS)) return SESS_OK;
    for (uint16_t i = 0; i < n; i++) {
        uintptr_t u = ng + SESS_O_USERS + (uintptr_t)i * SESS_USER_STRIDE;
        uint8_t ub[0x18];
        if (p->read(ub, u, sizeof ub)) return SESS_FAULT;
        uint16_t head;
        memcpy(&head, ub, 2);
        uint16_t team = (head >> 10) & 7;
        if (team >= SESS_TEAM_EMPTY) continue;
        sess_player *pl = &s->players[s->n_players++];
        pl->i = i;
        pl->slot = head & 0x1f;
        pl->teamslot = (head >> 5) & 0x1f;
        pl->team = team;
        pl->local = i < s->n_local;
        memcpy(&pl->gs, ub + 8, 8);
        memcpy(&pl->actor, ub + 16, 8);
        uint16_t ig[2];
        if (pl->slot < SESS_MAX_USERS && !p->read(ig, ng + SESS_O_INDEX + pl->slot * 4u, 4)) pl->gen = ig[1];
        if (ents && pl->slot < nents) {
            uint8_t eb[0x10];
            uintptr_t e = ents + (uintptr_t)pl->slot * SESS_ENTRANT_STRIDE;
            if (!p->read(eb, e, 16) && !p->read(&pl->ping, e + SESS_O_ENT_PING, 2)) {
                pl->has_entrant = pl->has_userid = 1;
                memcpy(&pl->provider, eb, 8);
                memcpy(&pl->userid, eb + 8, 8);
            }
        }
        if (pl->local && p->local_userid) {
            uint64_t cu = 0;
            if (!p->read(&cu, ng + SESS_O_CNSUSER, 8) && cu) {
                sess_userid id = p->local_userid((void *)(uintptr_t)cu);
                pl->provider = id.provider; pl->userid = id.account;
                pl->has_userid = pl->userid_cnsuser = 1;
            }
        }
        if (p->username) sess__str(p, (uintptr_t)p->username((void *)(uintptr_t)ng, i), pl->name, sizeof pl->name);
    }
    return SESS_OK;
}

/* rad-menu lookups (: the SOCIAL card's p_userid is an index into the net game user list). by_index: the row
 * of users[i] (the `i` key); by_slot: the row whose lobby slot is `slot` (the `slot` key). NULL = not present. Which
 * of the two the card's p_userid is stays to be confirmed on device (compare it with both keys of `session players`). */
SESS_API const sess_player *sess_by_index(const sess_snapshot *s, int i)
{
    for (int k = 0; k < s->n_players; k++)
        if (s->players[k].i == i) return &s->players[k];
    return NULL;
}
SESS_API const sess_player *sess_by_slot(const sess_snapshot *s, int slot)
{
    for (int k = 0; k < s->n_players; k++)
        if (s->players[k].slot == slot) return &s->players[k];
    return NULL;
}
/* the other players, in users[] order (the Report Player custom list); returns how many were written */
SESS_API int sess_others(const sess_snapshot *s, const sess_player **out, int max)
{
    int n = 0;
    for (int k = 0; k < s->n_players && n < max; k++)
        if (!s->players[k].local) out[n++] = &s->players[k];
    return n;
}

SESS_API const char *sess_err(int rc)
{
    return rc == SESS_NO_GAME ? "no game tick yet" : rc == SESS_NO_NETGAME ? "no netgame ([game+o] is null)"
         : rc == SESS_BAD_NETGAME ? "netgame mismatch ([ng+8] != game)" : rc == SESS_FAULT ? "fault reading the netgame" : "ok";
}


#endif /* SESS_SNAPSHOT_H */
