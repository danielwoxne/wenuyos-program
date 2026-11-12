// WenuyOS: egen kraschlogg för program där systemets kraschspår inte räcker (0.4.0).
//
// När ett program hoppar till en ogiltig adress (t.ex. en återanropsfunktion i ett bibliotek som redan tagits bort)
// kan coredumpctl inte följa stacken bakåt – spåret slutar på "#0 n/a". Den här hanteraren skriver då ner
// programräknaren, stackpekaren och de adresser på stacken som pekar in i kod, med bibliotek och funktionsnamn
// (dladdr). Den översta sådana är oftast den som gjorde hoppet. Sedan kraschar programmet som vanligt (core dump
// och autostartens omstart fungerar som förut).
//
// Filen: ~/.local/state/wenuyos/crash/<program>-<tid>.txt (högst 5 sparas). wenuyos-measure tar med de senaste.
#pragma once
#include <dlfcn.h>
#include <fcntl.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ucontext.h>
#include <time.h>
#include <unistd.h>

#include <string>

namespace wenuy {
namespace crash {

inline char gDir[512];
inline char gProg[64];

inline void put(int fd, const char* s) {
    if (s) { ssize_t r = write(fd, s, strlen(s)); (void)r; }
}
inline void hex(int fd, unsigned long v) {
    char b[19] = "0x";
    for (int i = 0; i < 16; ++i) { int d = (v >> ((15 - i) * 4)) & 0xF; b[2 + i] = d < 10 ? '0' + d : 'a' + d - 10; }
    b[18] = 0;
    put(fd, b);
}
inline void num(int fd, long v) {
    char b[24]; int i = 23; b[i] = 0;
    bool neg = v < 0; unsigned long u = neg ? -v : v;
    do { b[--i] = '0' + u % 10; u /= 10; } while (u && i > 1);
    if (neg) b[--i] = '-';
    put(fd, b + i);
}
// en adress: bibliotek+förskjutning och funktion, om den pekar in i ett laddat bibliotek
inline bool where(int fd, unsigned long a) {
    Dl_info di;
    if (!dladdr(reinterpret_cast<void*>(a), &di) || !di.dli_fname) return false;
    const char* base = strrchr(di.dli_fname, '/');
    put(fd, base ? base + 1 : di.dli_fname);
    put(fd, " + "); hex(fd, a - reinterpret_cast<unsigned long>(di.dli_fbase));
    if (di.dli_sname) { put(fd, "  "); put(fd, di.dli_sname); put(fd, " + "); hex(fd, a - reinterpret_cast<unsigned long>(di.dli_saddr)); }
    return true;
}

inline void handler(int sig, siginfo_t* si, void* ctx) {
    char path[640];
    time_t t = time(nullptr);
    struct tm tmv;
    localtime_r(&t, &tmv);
    char stamp[32];
    strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", &tmv);
    strcpy(path, gDir); strcat(path, "/"); strcat(path, gProg); strcat(path, "-"); strcat(path, stamp); strcat(path, ".txt");
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd >= 0) {
        put(fd, gProg); put(fd, ": signal "); num(fd, sig); put(fd, " ("); put(fd, strsignal(sig)); put(fd, ")\n");
        put(fd, "fel adress: "); hex(fd, reinterpret_cast<unsigned long>(si->si_addr)); put(fd, "\n");
#if defined(__x86_64__)
        auto* uc = static_cast<ucontext_t*>(ctx);
        const unsigned long pc = uc->uc_mcontext.gregs[REG_RIP], sp = uc->uc_mcontext.gregs[REG_RSP];
        put(fd, "pc: "); hex(fd, pc); put(fd, "  "); if (!where(fd, pc)) put(fd, "(inte i något laddat bibliotek)"); put(fd, "\n");
        put(fd, "sp: "); hex(fd, sp); put(fd, "\n");
        put(fd, "adresser på stacken som pekar in i kod (den översta är oftast anroparen):\n");
        int found = 0;
        auto* s = reinterpret_cast<unsigned long*>(sp);
        for (int i = 0; i < 512 && found < 40; ++i) {   // högst 4 KB av stacken
            const unsigned long v = s[i];
            if (v < 0x10000) continue;
            Dl_info di;
            if (!dladdr(reinterpret_cast<void*>(v), &di) || !di.dli_fname) continue;
            put(fd, "  [sp+"); num(fd, i * 8); put(fd, "] "); where(fd, v); put(fd, "\n");
            ++found;
        }
#else
        (void)ctx;
#endif
        close(fd);
    }
    signal(sig, SIG_DFL);   // krascha som vanligt (core dump)
    raise(sig);
}

// Anropas tidigt i main(). Rensar gamla loggar (högst 5 per program).
inline void install(const char* prog) {
    const gchar* state = g_get_user_state_dir();
    g_snprintf(gDir, sizeof gDir, "%s/wenuyos/crash", state);
    g_mkdir_with_parents(gDir, 0700);
    g_strlcpy(gProg, prog, sizeof gProg);
    if (GDir* d = g_dir_open(gDir, 0, nullptr)) {   // de äldsta bort
        GPtrArray* names = g_ptr_array_new_with_free_func(g_free);
        const char* n;
        const std::string pre = std::string(prog) + "-";
        while ((n = g_dir_read_name(d))) if (g_str_has_prefix(n, pre.c_str())) g_ptr_array_add(names, g_strdup(n));
        g_dir_close(d);
        g_ptr_array_sort(names, +[](gconstpointer a, gconstpointer b) { return strcmp(*(char* const*)a, *(char* const*)b); });
        for (guint i = 0; names->len > 5 && i + 5 < names->len; ++i) {
            gchar* p = g_build_filename(gDir, static_cast<char*>(names->pdata[i]), nullptr);
            g_unlink(p);
            g_free(p);
        }
        g_ptr_array_free(names, TRUE);
    }
    static char altstack[64 * 1024];   // egen stack: hanteraren fungerar även om stacken är förstörd
    stack_t ss{};
    ss.ss_sp = altstack; ss.ss_size = sizeof altstack;
    sigaltstack(&ss, nullptr);
    struct sigaction sa{};
    sa.sa_sigaction = handler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
    sigemptyset(&sa.sa_mask);
    for (int s : {SIGSEGV, SIGBUS, SIGILL, SIGFPE}) sigaction(s, &sa, nullptr);
}

}  // namespace crash
}  // namespace wenuy
