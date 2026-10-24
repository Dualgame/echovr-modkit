/* libevrmod.so — the Echo VR Quest mod loader (libr15.so of 6528386917200980, vc 4987566).
 *
 * How it gets loaded: quest_apk_build_release.py step 6 adds "libevrmod.so" to libr15.so's DT_NEEDED list. The
 * bionic linker then maps this library with libr15.so, relocates libr15.so, and runs this constructor before
 * libr15.so's own init_array. No wrap.sh, no LD_PRELOAD, and the APK stays non-debuggable.
 *
 * What it does: dlopen every "libevrmod_*.so" in its own directory (the app's extracted native-library dir), in
 * strcmp order, and log each result. The builder installs each --plugin under that prefix
 * (libradlinks.so -> libevrmod_radlinks.so), so the plugin order is alphabetical, as on PC without load_order.txt.
 * Plugins do their own work from their constructors (rad-links: wait for libr15.so, check prologues, wait for RELRO,
 * swap or chain each word). This loader exports nothing and hooks nothing: it is not a broker (that is ABI 3).
 *
 * Kill switch: if /sdcard/readyatdawn/_local/evrmod.disable exists, no plugin is loaded (a crash loop is undone by
 * creating one file, with no reinstall). Log: logcat tag "evrmod". */
#include <android/log.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TAG "evrmod"
#define PREFIX "libevrmod_"
#define DISABLE_FILE "/sdcard/readyatdawn/_local/evrmod.disable"
#define MAX_PLUGINS 32
#define LOG(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

static int is_plugin(const char *n)
{
    size_t l = strlen(n);
    return l > sizeof PREFIX - 1 + 3 && !strncmp(n, PREFIX, sizeof PREFIX - 1) && !strcmp(n + l - 3, ".so");
}

static int by_name(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

__attribute__((constructor)) static void evrmod_init(void)
{
    Dl_info self;
    char dir[512];
    if (!dladdr((void *)evrmod_init, &self) || !self.dli_fname) {
        LOG("inert: dladdr found no path for this library");
        return;
    }
    const char *sl = strrchr(self.dli_fname, '/');
    size_t dl = sl ? (size_t)(sl - self.dli_fname + 1) : 0;
    if (!dl || dl >= sizeof dir) {
        LOG("inert: unusable library path %s", self.dli_fname);
        return;
    }
    memcpy(dir, self.dli_fname, dl);
    dir[dl] = 0;
    LOG("libevrmod loaded in pid %d from %s", getpid(), dir);
    if (!access(DISABLE_FILE, F_OK)) {
        LOG("%s exists: no plugin loaded", DISABLE_FILE);
        return;
    }
    /* ENOENT = no kill switch. Any other errno (EACCES without the storage grants, EIO, ...) means the loader
     * cannot tell: logged, and the plugins still load (unchanged behaviour), so a missing grant is visible. */
    if (errno != ENOENT) LOG("kill switch %s unreadable (errno %d: %s): cannot tell, loading plugins",
                             DISABLE_FILE, errno, strerror(errno));
    DIR *d = opendir(dir);
    if (!d) {
        LOG("inert: cannot list %s (libraries not extracted?)", dir);
        return;
    }
    char *names[MAX_PLUGINS];
    int n = 0, skipped = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!is_plugin(e->d_name)) continue;
        if (n == MAX_PLUGINS) { skipped++; continue; }
        if (!(names[n] = strdup(e->d_name))) break;
        n++;
    }
    closedir(d);
    qsort(names, (size_t)n, sizeof names[0], by_name);
    LOG("load order (%d plugin(s), alphabetical%s):", n, skipped ? ", some over the limit skipped" : "");
    for (int i = 0; i < n; i++) LOG("  %d. %s", i + 1, names[i]);
    int ok = 0;
    for (int i = 0; i < n; i++) {
        char path[768];
        if ((size_t)snprintf(path, sizeof path, "%s%s", dir, names[i]) >= sizeof path) {
            LOG("skip %s: path too long", names[i]);
        } else if (dlopen(path, RTLD_NOW | RTLD_LOCAL)) {
            LOG("loaded %s", names[i]);
            ok++;
        } else {
            const char *err = dlerror();
            LOG("FAILED %s: %s", names[i], err ? err : "?");
        }
        free(names[i]);
    }
    LOG("done: %d of %d plugin(s) loaded", ok, n);
}
