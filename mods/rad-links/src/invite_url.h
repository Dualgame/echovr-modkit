/* The text the INVITE LINK page (p35) shows for an invite. Pure C, no game or OS calls: radlinks.c,
 * radlinks_quest.c and tests/test_invite_url.c all include it.
 *
 * Precedence: the client's invite_link_base (when set) > the host's link_url (from the invite reply) > the base
 * built from apiservice_host. Every choice ends in <base>/deeplinks/launch?code=<CODE> (the host's own link_url
 * has that shape, and must hold the code, else it is skipped). 0 = nothing usable: the caller keeps its previous text. */
#ifndef RADLINKS_INVITE_URL_H
#define RADLINKS_INVITE_URL_H
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define INVITE_VIEW_CAP 128   /* bytes, with the NUL: the longest text p35's link element is ever given */

/* The note under the link on p35 (element info_text_02, shipped "Note:  This link can only be used on PC.", 40 chars).
 * Ours replaces it only while p35 shows our web link (invite_url_ok of the text), never beside the -lobbyid fallback. Same SetText path as link_text (a 52-char shipped string takes up
 * to 127), and shorter than p35's own info_text_01 (113 chars) in the same font, so it fits the same way. */
#define INVITE_NOTE_TEXT "Note: open the link to see the code, or enter it in JOIN VIA CODE."
#define INVITE_NOTE_CAP 128

enum { INVITE_SRC_NONE, INVITE_SRC_OVERRIDE, INVITE_SRC_HOST, INVITE_SRC_FALLBACK };

static inline const char *invite_src_name(int s)
{
    return s == INVITE_SRC_OVERRIDE ? "invite_link_base" : s == INVITE_SRC_HOST ? "the host's link_url"
         : s == INVITE_SRC_FALLBACK ? "apiservice_host" : "none";
}

/* "http://" or "https://" plus at least one host character, and nothing a text field or a log line should not carry */
static inline int invite_url_ok(const char *u)
{
    size_t n = u ? strlen(u) : 0, h = !n ? 0 : !strncmp(u, "http://", 7) ? 7 : !strncmp(u, "https://", 8) ? 8 : 0;
    if (!h || n <= h) return 0;
    for (size_t i = 0; i < n; i++)
        if ((unsigned char)u[i] <= ' ' || (unsigned char)u[i] >= 0x7f || u[i] == '"' || u[i] == '\\' || u[i] == '<' || u[i] == '>')
            return 0;
    return 1;
}

/* A code the page can put in a query string as is */
static inline int invite_code_ok(const char *c)
{
    size_t n = c ? strlen(c) : 0;
    if (!n || n > 15) return 0;
    for (size_t i = 0; i < n; i++)
        if (!((c[i] >= '0' && c[i] <= '9') || (c[i] >= 'A' && c[i] <= 'Z') || (c[i] >= 'a' && c[i] <= 'z') || c[i] == '-' || c[i] == '_'))
            return 0;
    return 1;
}

/* base + "/deeplinks/launch?code=" + code; base = scheme://host[:port] with any trailing '/' dropped */
static inline int invite_url_join(char *out, size_t cap, const char *base, const char *code)
{
    size_t n = strlen(base);
    while (n > 8 && base[n - 1] == '/') n--;
    if (!invite_url_ok(base) || !invite_code_ok(code)) return 0;
    int w = snprintf(out, cap, "%.*s/deeplinks/launch?code=%s", (int)n, base, code);
    return w > 0 && (size_t)w < cap;
}

/* out (>= INVITE_VIEW_CAP): the text for p35. host_url / override_base / svc_base may be NULL or "". */
static inline int invite_view_url(char *out, size_t cap, const char *host_url, const char *override_base,
                                  const char *svc_base, const char *code, int *src)
{
    int s = INVITE_SRC_NONE;
    if (cap) out[0] = 0;
    if (override_base && *override_base && invite_url_join(out, cap, override_base, code)) s = INVITE_SRC_OVERRIDE;
    else if (host_url && *host_url && invite_url_ok(host_url) && strlen(host_url) < cap && code && *code && strstr(host_url, code)) {
        memcpy(out, host_url, strlen(host_url) + 1);
        s = INVITE_SRC_HOST;
    } else if (svc_base && *svc_base && invite_url_join(out, cap, svc_base, code)) s = INVITE_SRC_FALLBACK;
    if (src) *src = s;
    if (!s && cap) out[0] = 0;
    return s != INVITE_SRC_NONE;
}
#endif
