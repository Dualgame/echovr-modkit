/* broker_selftest.c - offline check of the live evrmod broker. No game: the broker source is
 * compiled into this exe and pointed at a FAKE image (a VirtualAlloc'd region of echovr.exe's SizeOfImage) that
 * holds the four stock prologues (bytes of the stock echovr.exe, PC 6323983201049540) at their RVAs, each
 * followed by an epilogue that undoes it, and a tick vftable slot pointing at a `ret`. The send_event site is
 * pre-hooked the way a standalone plugin hooks (jmp [rip+0] into a loaded module = this exe) to exercise chaining.
 * Checks: each site installed once (two registrations), callbacks run in registration order, a faulting callback
 * is contained and the next one and the original still run, chaining reaches the standalone hook.
 * Plugin load order (plan_load_order, pure): mods\load_order.txt first in its order, missing / non-plugin
 * entries logged and skipped, then the rest alphabetically, and no name (zeta.dll) is special.
 * Build + run: build_mods.cmd (last step). Exit code 0 = PASS. */
#include "../src/winmm_proxy.c"

static uint8_t *F;            /* fake image base */
static char g_order[64]; static int g_no;
static int g_standalone_calls;
static void mark(char c) { if (g_no < 63) g_order[g_no++] = c; g_order[g_no] = 0; }

static void cb_a(void *g, void *u) { (void)g; (void)u; mark('A'); }
static void cb_crash(void *g, void *u) { (void)g; (void)u; mark('F'); *(volatile int *)0 = 1; }
static void cb_b(void *g, void *u) { (void)g; mark(*(char *)u); }
static void ev_a(void *gs, const uint64_t *ev, uint32_t fl, void *u) { (void)gs; (void)fl; (void)u; mark(ev[0] == 0x1234 ? 'e' : '?'); }
static void ev_g(void *gs, const uint64_t *ev, uint32_t fl, void *u) { (void)gs; (void)ev; (void)fl; (void)u; mark('g'); }
static void me_a(const char *j, size_t n, void *u) { (void)u; mark(n == 2 && j[0] == '{' ? 'm' : '?'); }
static void __fastcall standalone_target(void *gs, const uint64_t *ev, uint32_t fl)
{ (void)gs; (void)ev; (void)fl; g_standalone_calls++; mark('S'); }

static void put(uint32_t rva, const char *hexs)
{
    for (uint8_t *p = F + rva; hexs[0] && hexs[1]; hexs += 2) { unsigned v; sscanf_s(hexs, "%2x", &v); *p++ = (uint8_t)v; }
}

static int fails;
static void expect(const char *what, int ok) { printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) fails++; }

/* Load order. `want` = the expected names joined by '|'. */
static void expect_order(const char *what, const char *lo, const wchar_t *const *disk, int nd, const wchar_t *want, int want_skip)
{
    static ModName found[MAXMODS], out[MAXMODS];
    wchar_t got[1024] = L"";
    int skipped = -1;
    for (int i = 0; i < nd; i++) wcscpy_s(found[i], MAX_PATH, disk[i]);
    int n = plan_load_order(lo, found, nd, out, MAXMODS, &skipped);
    for (int i = 0; i < n; i++) { if (i) wcscat_s(got, 1024, L"|"); wcscat_s(got, 1024, out[i]); }
    int ok = !wcscmp(got, want) && skipped == want_skip;
    expect(what, ok);
    if (!ok) printf("     got %ls (skipped %d), want %ls (skipped %d)\n", got, skipped, want, want_skip);
}

static void test_order(void)
{
    static const wchar_t *const three[] = { L"zeta.dll", L"Rad-Links.dll", L"evrmod_earlyquit.dll" };
    static const wchar_t *const alone[] = { L"rad-links.dll" };
    expect_order("order: no load_order.txt -> alphabetical, no name forced first", NULL, three, 3,
                 L"evrmod_earlyquit.dll|Rad-Links.dll|zeta.dll", 0);
    expect_order("order: rad-links.dll alone (no other plugin)", NULL, alone, 1, L"rad-links.dll", 0);
    expect_order("order: no plugins at all", NULL, alone, 0, L"", 0);
    expect_order("order: empty load_order.txt -> alphabetical", "", three, 3,
                 L"evrmod_earlyquit.dll|Rad-Links.dll|zeta.dll", 0);
    expect_order("order: list first (BOM, comments, blanks, trim, case, dup), missing + non-plugin skipped, rest alpha",
                 "\xEF\xBB\xBF# my order\r\n\r\n  rad-links.dll \t\r\n   # indented comment\r\n" "missing.dll\r\n" "notes.txt\r\n"
                 "RAD-LINKS.DLL\r\n" "zeta.dll\n", three, 3, L"Rad-Links.dll|zeta.dll|evrmod_earlyquit.dll", 2);
    expect_order("order: LF-only list, no trailing newline", "zeta.dll\n" "evrmod_earlyquit.dll", three, 3,
                 L"zeta.dll|evrmod_earlyquit.dll|Rad-Links.dll", 0);
    expect_order("order: a path in the list cannot reach outside mods\\", "..\\evil.dll\r\nC:\\x\\zeta.dll\r\n", three, 3,
                 L"evrmod_earlyquit.dll|Rad-Links.dll|zeta.dll", 2);
    expect_order("order: winmm.dll is never a plugin", "winmm.dll\r\n", alone, 1, L"rad-links.dll", 1);
}

int main(void)
{
    g_log = GetStdHandle(STD_OUTPUT_HANDLE);
    test_order();
    F = VirtualAlloc(NULL, 0x2231000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!F) return 2;
    /* global core: three home stores, then ret */
    put(0x50fed0, "48895c240848896c24104889742418" "c3");
    /* targeted: the same prologue, but pre-hooked by a standalone plugin: jmp [rip+0] -> standalone_target, nop */
    put(0x50fb60, "48895c240848896c24104889742418" "c3");
    put_jmp(F + 0x50fb60, (void *)standalone_target); F[0x50fb60 + 14] = 0x90;
    /* remote log: mov [rsp+8],rbx; push rdi; sub rsp,30h; mov rbx,rdx; mov rdi,rcx | add rsp,30h; pop rdi;
     * mov rbx,[rsp+8]; ret */
    put(0x113960, "48895c2408574883ec30488bda488bf9" "4883c4305f488b5c2408c3");
    /* tick: the vftable slot points at the stock function, a ret */
    F[0x5194f0] = 0xc3;
    *(uint64_t *)(F + 0x16d1100) = (uint64_t)(F + 0x5194f0);
    g_build.image_base = F; g_build.time_date_stamp = 0x6452dff6; g_build.size_of_image = 0x2231000;
    g_build.is_known = 1; g_build.name = "FAKE image (selftest)";

    static char ub = 'B';
    expect("register tick A", g_api.register_tick(cb_a, NULL) == 0);
    expect("register tick F (faults)", g_api.register_tick(cb_crash, NULL) == 0);
    expect("register tick B", g_api.register_tick(cb_b, &ub) == 0);
    expect("register send_event", g_api.register_send_event(ev_a, NULL) == 0);
    expect("register send_global", g_api.register_send_global(ev_g, NULL) == 0);
    expect("register metric", g_api.register_metric_log(me_a, NULL) == 0);
    expect("register metric again (no second install)", g_api.register_metric_log(me_a, NULL) == 0);
    expect("NULL fn refused", g_api.register_tick(NULL, NULL) == -1);
    broker_summary();

    /* Call through the patched sites exactly as the game would. */
    TickFn tick = *(TickFn *)(F + 0x16d1100);
    expect("tick slot now points at the broker", tick == hk_tick);
    g_no = 0; tick(F);
    expect("tick order A F B (fault contained, B still runs)", !strcmp(g_order, "AFB"));
    expect("fault counted once", g_cb[H_TICK][1].faults == 1);
    uint64_t ev[5] = { 0x1234, 0, 0, 0, ~0ull };
    g_no = 0; ((SendCoreFn)(F + 0x50fb60))(F, ev, 1);
    expect("send_event: broker callback, then the chained standalone hook", !strcmp(g_order, "eS") && g_standalone_calls == 1);
    g_no = 0; ((SendCoreFn)(F + 0x50fed0))(F, ev, 1);
    expect("send_global: callback then the trampoline back into the stock body", !strcmp(g_order, "g"));
    g_no = 0; ((RemoteLogFn)(F + 0x113960))("{}", 2);
    expect("metric: two callbacks then the stock body (16-byte steal)", !strcmp(g_order, "mm"));
    g_no = 0; tick(F); tick(F);
    expect("second / third frame", !strcmp(g_order, "AFBAFB") && g_cb[H_TICK][1].faults == 3);
    expect("each site installed once", g_installed[0] == 1 && g_installed[1] == 1 && g_installed[2] == 1 && g_installed[3] == 1);
    printf(fails ? "SELFTEST FAIL (%d)\n" : "SELFTEST PASS\n", fails);
    return fails ? 1 : 0;
}
