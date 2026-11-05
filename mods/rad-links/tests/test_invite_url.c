/* Offline tests for src/invite_url.h:  cc -std=c11 -Wall -Wextra -Werror -I../src test_invite_url.c -o /tmp/t && /tmp/t */
#include "invite_url.h"
#include <assert.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static int run(char *o, const char *host, const char *ov, const char *svc, const char *code, int *src)
{
    return invite_view_url(o, INVITE_VIEW_CAP, host, ov, svc, code, src);
}

int main(void)
{
    char o[INVITE_VIEW_CAP];
    int s;
    /* the host's link_url wins over the apiservice_host fallback */
    CHECK(run(o, "http://host.example:777/deeplinks/launch?code=AB12C", "", "http://svc.example:777", "AB12C", &s) && s == INVITE_SRC_HOST);
    CHECK(!strcmp(o, "http://host.example:777/deeplinks/launch?code=AB12C"));
    /* no link_url (an older host): built from apiservice_host */
    CHECK(run(o, "", NULL, "http://host.example:777", "AB12C", &s) && s == INVITE_SRC_FALLBACK);
    CHECK(!strcmp(o, "http://host.example:777/deeplinks/launch?code=AB12C"));
    /* the client override wins over both; trailing slashes dropped */
    CHECK(run(o, "http://host.example:777/deeplinks/launch?code=AB12C", "https://play.example.org/", "http://x.example:1", "AB12C", &s) && s == INVITE_SRC_OVERRIDE);
    CHECK(!strcmp(o, "https://play.example.org/deeplinks/launch?code=AB12C"));
    CHECK(run(o, NULL, "http://other.example:8080///", NULL, "zz-9", &s) && !strcmp(o, "http://other.example:8080/deeplinks/launch?code=zz-9"));
    /* a bad override is ignored, not shown: falls through to the host link */
    CHECK(run(o, "http://host.example:777/deeplinks/launch?code=AB12C", "ftp://nope", "", "AB12C", &s) && s == INVITE_SRC_HOST);
    CHECK(run(o, "", "host.example:777", "http://host.example:777", "AB12C", &s) && s == INVITE_SRC_FALLBACK);
    /* missing / unusable code: no fallback or override URL */
    CHECK(!run(o, "", "", "http://host.example:777", "", &s) && s == INVITE_SRC_NONE && !o[0]);
    CHECK(!run(o, "", "http://host.example:777", "http://host.example:777", NULL, &s) && s == INVITE_SRC_NONE);
    CHECK(!run(o, "", "", "http://host.example:777", "A&B", &s));
    CHECK(!run(o, "", "", "http://host.example:777", "ABCDEFGHIJKLMNOP", &s));   /* 16 chars: over the code cap */
    /* the host link must hold the code: a link for another code (or no code known) is not shown */
    CHECK(!run(o, "http://host.example:777/deeplinks/launch?code=Q", "", "", "", &s) && s == INVITE_SRC_NONE);
    CHECK(run(o, "http://host.example:777/deeplinks/launch?code=OTHER", "", "http://svc.example:777", "AB12C", &s) && s == INVITE_SRC_FALLBACK);
    CHECK(!strcmp(o, "http://svc.example:777/deeplinks/launch?code=AB12C"));
    CHECK(!run(o, "http://host.example:777/deeplinks/launch?code=OTHER", "", "", "AB12C", &s) && s == INVITE_SRC_NONE);
    /* the p35 note is written only beside a web link, never beside the -lobbyid fallback */
    CHECK(invite_url_ok("http://host.example:777/deeplinks/launch?code=AB12C") && !invite_url_ok("-lobbyid 8f4e1d2a-0000-0000-0000-000000000000"));
    /* nothing at all */
    CHECK(!run(o, "", "", "", "AB12C", &s) && s == INVITE_SRC_NONE);
    /* hostile host link: spaces, quotes, backslash, non-http, no host */
    CHECK(!run(o, "http://a b/", "", "", "AB12C", &s));
    CHECK(!run(o, "javascript:alert(1)", "", "", "AB12C", &s));
    CHECK(!run(o, "http://", "", "", "AB12C", &s));
    CHECK(!run(o, "http://h\\x/", "", "", "AB12C", &s));
    CHECK(!run(o, "http://h\"x/", "", "", "AB12C", &s));
    /* over-long: never truncated into a wrong URL */
    {
        char longbase[300] = "http://";
        memset(longbase + 7, 'a', 200);
        longbase[207] = 0;
        CHECK(!run(o, "", longbase, "", "AB12C", &s) && !o[0]);
        CHECK(!run(o, longbase, "", "", "AB12C", &s));
        /* 127 chars exactly fits; 128 does not */
        char b[200] = "http://";
        memset(b + 7, 'a', 127 - 7 - (int)strlen("/deeplinks/launch?code=AB12C"));
        b[127 - (int)strlen("/deeplinks/launch?code=AB12C")] = 0;
        CHECK(run(o, "", b, "", "AB12C", &s) && strlen(o) == 127);
        strcat(b, "a");
        CHECK(!run(o, "", b, "", "AB12C", &s));
    }
    /* the p35 note: replaces "...can only be used on PC.", fits the cap, plain ASCII, no PC-only claim */
    CHECK(strlen(INVITE_NOTE_TEXT) < INVITE_NOTE_CAP && strlen(INVITE_NOTE_TEXT) <= 113);
    CHECK(!strstr(INVITE_NOTE_TEXT, "only be used on PC") && strstr(INVITE_NOTE_TEXT, "JOIN VIA CODE"));
    for (const char *c = INVITE_NOTE_TEXT; *c; c++) CHECK((unsigned char)*c >= ' ' && (unsigned char)*c < 0x7f && *c != '"' && *c != '\\');
    printf(fails ? "%d FAILED\n" : "all invite_url tests passed\n", fails);
    return fails != 0;
}
