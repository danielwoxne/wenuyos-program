// libwenuy – WenuyOS Kassaskåp (se WenuySafe.h).
//
// Filformat (alla tal little-endian):
//   "WSAFE" 0x01                       magi + version
//   u64 opslimit, u64 memlimit, 16 B salt   Argon2id-parametrar för lösenordet
//   u8 antal inlindningar, per inlindning: u8 sort (1 lösenord, 2 återställning), 24 B nonce, 48 B
//     inlindad datanyckel (32 B + 16 B tagg) – AD = magi+version+sort
//   24 B nonce, chiffertext (GVariant a(ssxa{ss}) + 16 B tagg) – AD = hela huvudet ovanför
#include "WenuySafe.h"

#include <fcntl.h>
#include <glib/gstdio.h>
#include <sodium.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

const unsigned char kMagic[6] = {'W', 'S', 'A', 'F', 'E', 1};
// Argon2id: 3 varv, 96 MiB – tar ~0,5 s på en ThinkPad A275 och fungerar på 2 GB-golvet
const unsigned long long kOps = 3;
const size_t kMem = 96ull * 1024 * 1024;
const char* kRecoveryContext = "wenuyos-safe-recovery-v1";
const char* kAlphabet = "ABCDEFGHJKMNPQRSTVWXYZ0123456789";   // utan I, L, O, U – lätt att skriva av

struct Secret {   // sträng i låst minne som skrivs över när den frigörs
    char* p = nullptr;
    size_t n = 0;
    Secret() = default;
    explicit Secret(const std::string& s) { set(s.data(), s.size()); }
    Secret(const Secret& o) { set(o.p, o.n); }
    Secret& operator=(const Secret& o) { if (this != &o) { clear(); set(o.p, o.n); } return *this; }
    ~Secret() { clear(); }
    void set(const char* d, size_t len) {
        p = static_cast<char*>(sodium_malloc(len + 1));
        if (!p) { n = 0; return; }
        memcpy(p, d, len); p[len] = 0; n = len;
    }
    void clear() { if (p) { sodium_free(p); p = nullptr; n = 0; } }
    const char* c_str() const { return p ? p : ""; }
};

struct Entry {
    std::string kind = "other";
    gint64 modified = 0;
    std::map<std::string, Secret> fields;
};

struct Wrap { unsigned char type; unsigned char nonce[24]; unsigned char data[48]; };

bool sodiumReady() {
    static int r = sodium_init();
    return r >= 0;
}

std::string ad(unsigned char type) {
    std::string a(reinterpret_cast<const char*>(kMagic), 6);
    a.push_back(static_cast<char>(type));
    return a;
}

void put64(std::string& s, guint64 v) { for (int i = 0; i < 8; ++i) s.push_back(static_cast<char>((v >> (8 * i)) & 0xff)); }
guint64 get64(const unsigned char* p) { guint64 v = 0; for (int i = 7; i >= 0; --i) v = (v << 8) | p[i]; return v; }

std::string recoveryText(const unsigned char* key) {   // 32 B → 52 tecken i grupper om 4
    std::string bits, out;
    int acc = 0, nb = 0;
    for (int i = 0; i < 32; ++i) {
        acc = (acc << 8) | key[i]; nb += 8;
        while (nb >= 5) { bits.push_back(kAlphabet[(acc >> (nb - 5)) & 31]); nb -= 5; }
    }
    if (nb > 0) bits.push_back(kAlphabet[(acc << (5 - nb)) & 31]);
    for (size_t i = 0; i < bits.size(); ++i) { if (i && i % 4 == 0) out.push_back('-'); out.push_back(bits[i]); }
    sodium_memzero(&acc, sizeof acc);
    return out;
}
bool recoveryParse(const char* text, unsigned char* key) {
    int acc = 0, nb = 0, n = 0;
    for (const char* c = text; *c && n < 32; ++c) {
        char ch = g_ascii_toupper(*c);
        if (ch == '-' || ch == ' ') continue;
        if (ch == 'O') ch = '0';
        if (ch == 'I' || ch == 'L') ch = '1';
        const char* pos = strchr(kAlphabet, ch);
        if (!pos || !ch) return false;
        acc = (acc << 5) | int(pos - kAlphabet); nb += 5;
        if (nb >= 8) { key[n++] = static_cast<unsigned char>((acc >> (nb - 8)) & 0xff); nb -= 8; }
    }
    sodium_memzero(&acc, sizeof acc);
    return n == 32;
}
void recoveryKek(const unsigned char* rk, unsigned char* kek) {
    crypto_generichash(kek, 32, rk, 32, reinterpret_cast<const unsigned char*>(kRecoveryContext), strlen(kRecoveryContext));
}

}  // namespace

struct _WenuySafe {
    GObject parent;
    std::string* path;
    unsigned char* dataKey;       // 32 B, låst minne
    unsigned char* pwKek;         // 32 B, låst minne (behövs inte för att spara – bara om parametrar byts)
    unsigned char salt[16];
    guint64 ops, mem;
    std::vector<Wrap>* wraps;
    std::map<std::string, Entry>* entries;
    gboolean locked;
};

G_DEFINE_FINAL_TYPE(WenuySafe, wenuy_safe, G_TYPE_OBJECT)
G_DEFINE_QUARK(wenuy-safe-error-quark, wenuy_safe_error)

static void wenuy_safe_finalize(GObject* o) {
    auto* s = WENUY_SAFE(o);
    wenuy_safe_lock(s);
    delete s->path;
    delete s->wraps;
    delete s->entries;
    G_OBJECT_CLASS(wenuy_safe_parent_class)->finalize(o);
}
static void wenuy_safe_class_init(WenuySafeClass* k) { G_OBJECT_CLASS(k)->finalize = wenuy_safe_finalize; }
static void wenuy_safe_init(WenuySafe* s) {
    s->path = new std::string;
    s->wraps = new std::vector<Wrap>;
    s->entries = new std::map<std::string, Entry>;
    s->dataKey = static_cast<unsigned char*>(sodium_malloc(32));
    s->pwKek = static_cast<unsigned char*>(sodium_malloc(32));
    s->ops = kOps; s->mem = kMem;
    s->locked = FALSE;
}

namespace {

std::string pathOr(const char* p) {
    if (p && *p) return p;
    gchar* d = wenuy_safe_default_path();
    std::string r = d;
    g_free(d);
    return r;
}

bool deriveKek(const char* password, const unsigned char* salt, guint64 ops, guint64 mem, unsigned char* out) {
    return crypto_pwhash(out, 32, password, strlen(password), salt, ops, (size_t)mem, crypto_pwhash_ALG_ARGON2ID13) == 0;
}

void wrapWith(WenuySafe* s, unsigned char type, const unsigned char* kek) {
    Wrap w{};
    w.type = type;
    randombytes_buf(w.nonce, sizeof w.nonce);
    const std::string a = ad(type);
    unsigned long long clen = 0;
    crypto_aead_xchacha20poly1305_ietf_encrypt(w.data, &clen, s->dataKey, 32, reinterpret_cast<const unsigned char*>(a.data()), a.size(),
                                               nullptr, w.nonce, kek);
    s->wraps->erase(std::remove_if(s->wraps->begin(), s->wraps->end(), [type](const Wrap& x) { return x.type == type; }), s->wraps->end());
    s->wraps->push_back(w);
}

std::string header(WenuySafe* s) {
    std::string h(reinterpret_cast<const char*>(kMagic), 6);
    put64(h, s->ops); put64(h, s->mem);
    h.append(reinterpret_cast<const char*>(s->salt), 16);
    h.push_back(static_cast<char>(s->wraps->size()));
    for (auto& w : *s->wraps) {
        h.push_back(static_cast<char>(w.type));
        h.append(reinterpret_cast<const char*>(w.nonce), 24);
        h.append(reinterpret_cast<const char*>(w.data), 48);
    }
    return h;
}

GVariant* serialize(WenuySafe* s) {
    GVariantBuilder b;
    g_variant_builder_init(&b, G_VARIANT_TYPE("a(ssxa{ss})"));
    for (auto& [name, e] : *s->entries) {
        GVariantBuilder f;
        g_variant_builder_init(&f, G_VARIANT_TYPE("a{ss}"));
        for (auto& [k, v] : e.fields) g_variant_builder_add(&f, "{ss}", k.c_str(), v.c_str());
        g_variant_builder_add(&b, "(ssxa{ss})", name.c_str(), e.kind.c_str(), e.modified, &f);
    }
    return g_variant_ref_sink(g_variant_builder_end(&b));
}

bool load(WenuySafe* s, const std::string& path, const char* password, const char* recovery, GError** err) {
    *s->path = path;
    gchar* raw = nullptr;
    gsize len = 0;
    if (!g_file_get_contents(path.c_str(), &raw, &len, nullptr)) {
        g_set_error(err, WENUY_SAFE_ERROR, WENUY_SAFE_ERROR_IO, "Kassaskåpet finns inte eller går inte att läsa");
        return false;
    }
    auto* d = reinterpret_cast<const unsigned char*>(raw);
    auto fail = [&](WenuySafeError code, const char* msg) { g_set_error_literal(err, WENUY_SAFE_ERROR, code, msg); g_free(raw); return false; };
    if (len < 6 + 16 + 16 + 1 + 24 + 16 || memcmp(d, kMagic, 6) != 0) return fail(WENUY_SAFE_ERROR_DAMAGED, "Inte ett WenuyOS-kassaskåp (eller fel version)");
    size_t pos = 6;
    s->ops = get64(d + pos); pos += 8;
    s->mem = get64(d + pos); pos += 8;
    if (s->ops < 1 || s->ops > 20 || s->mem < 8u * 1024 * 1024 || s->mem > 2048ull * 1024 * 1024) return fail(WENUY_SAFE_ERROR_DAMAGED, "Kassaskåpet är skadat");
    memcpy(s->salt, d + pos, 16); pos += 16;
    const unsigned nw = d[pos++];
    if (len < pos + nw * 73u + 24 + 16) return fail(WENUY_SAFE_ERROR_DAMAGED, "Kassaskåpet är skadat");
    s->wraps->clear();
    for (unsigned i = 0; i < nw; ++i) {
        Wrap w{};
        w.type = d[pos++];
        memcpy(w.nonce, d + pos, 24); pos += 24;
        memcpy(w.data, d + pos, 48); pos += 48;
        s->wraps->push_back(w);
    }
    const size_t headerLen = pos;
    // lås upp datanyckeln
    unsigned char* kek = static_cast<unsigned char*>(sodium_malloc(32));
    bool have = false;
    unsigned char want = password ? 1 : 2;
    if (password) {
        have = deriveKek(password, s->salt, s->ops, s->mem, kek);
        if (have) memcpy(s->pwKek, kek, 32);
    } else {
        unsigned char* rk = static_cast<unsigned char*>(sodium_malloc(32));
        have = recoveryParse(recovery, rk);
        if (have) recoveryKek(rk, kek);
        sodium_free(rk);
    }
    bool opened = false;
    for (auto& w : *s->wraps) {
        if (!have || w.type != want) continue;
        const std::string a = ad(w.type);
        unsigned long long mlen = 0;
        if (crypto_aead_xchacha20poly1305_ietf_decrypt(s->dataKey, &mlen, nullptr, w.data, 48,
                                                       reinterpret_cast<const unsigned char*>(a.data()), a.size(), w.nonce, kek) == 0 && mlen == 32) {
            opened = true;
            break;
        }
    }
    sodium_free(kek);
    if (!opened) return fail(WENUY_SAFE_ERROR_WRONG_PASSWORD, password ? "Fel lösenord" : "Fel återställningsnyckel");
    // innehållet
    const unsigned char* nonce = d + pos; pos += 24;
    const size_t clen = len - pos;
    auto* plain = static_cast<unsigned char*>(sodium_malloc(clen));
    unsigned long long plen = 0;
    const int rc = crypto_aead_xchacha20poly1305_ietf_decrypt(plain, &plen, nullptr, d + pos, clen, d, headerLen, nonce, s->dataKey);
    g_free(raw);
    if (rc != 0) {
        sodium_free(plain);
        g_set_error_literal(err, WENUY_SAFE_ERROR, WENUY_SAFE_ERROR_DAMAGED, "Kassaskåpet är skadat eller ändrat – prova säkerhetskopian (.bak)");
        return false;
    }
    GVariant* v = g_variant_new_from_data(G_VARIANT_TYPE("a(ssxa{ss})"), plain, plen, FALSE, nullptr, nullptr);
    g_variant_ref_sink(v);
    GVariantIter it;
    g_variant_iter_init(&it, v);
    const char *name, *kind;
    gint64 mod;
    GVariantIter* fields;
    while (g_variant_iter_next(&it, "(&s&sxa{ss})", &name, &kind, &mod, &fields)) {
        Entry e;
        e.kind = kind; e.modified = mod;
        const char *k, *val;
        while (g_variant_iter_next(fields, "{&s&s}", &k, &val)) e.fields[k] = Secret(std::string(val));
        g_variant_iter_free(fields);
        (*s->entries)[name] = e;
    }
    g_variant_unref(v);
    sodium_memzero(plain, plen);
    sodium_free(plain);
    return true;
}

bool weak(const char* pw, GError** err) {
    if (pw && g_utf8_strlen(pw, -1) >= 8) return false;
    g_set_error_literal(err, WENUY_SAFE_ERROR, WENUY_SAFE_ERROR_WEAK, "Lösenordet måste ha minst 8 tecken");
    return true;
}

}  // namespace

char* wenuy_safe_default_path(void) { return g_build_filename(g_get_user_data_dir(), "wenuyos", "kassaskap.wsafe", nullptr); }
gboolean wenuy_safe_exists(const char* path) { return g_file_test(pathOr(path).c_str(), G_FILE_TEST_IS_REGULAR); }

WenuySafe* wenuy_safe_create(const char* path, const char* password, char** recovery_key, GError** error) {
    if (!sodiumReady()) { g_set_error_literal(error, WENUY_SAFE_ERROR, WENUY_SAFE_ERROR_IO, "libsodium kunde inte starta"); return nullptr; }
    if (weak(password, error)) return nullptr;
    auto* s = WENUY_SAFE(g_object_new(WENUY_TYPE_SAFE, nullptr));
    *s->path = pathOr(path);
    randombytes_buf(s->salt, 16);
    crypto_aead_xchacha20poly1305_ietf_keygen(s->dataKey);
    if (!deriveKek(password, s->salt, s->ops, s->mem, s->pwKek)) {
        g_set_error_literal(error, WENUY_SAFE_ERROR, WENUY_SAFE_ERROR_IO, "För lite minne för att skydda lösenordet");
        g_object_unref(s);
        return nullptr;
    }
    wrapWith(s, 1, s->pwKek);
    unsigned char* rk = static_cast<unsigned char*>(sodium_malloc(32));
    unsigned char* kek = static_cast<unsigned char*>(sodium_malloc(32));
    randombytes_buf(rk, 32);
    recoveryKek(rk, kek);
    wrapWith(s, 2, kek);
    if (recovery_key) *recovery_key = g_strdup(recoveryText(rk).c_str());
    sodium_free(rk); sodium_free(kek);
    if (!wenuy_safe_save(s, error)) { g_object_unref(s); return nullptr; }
    return s;
}

WenuySafe* wenuy_safe_open(const char* path, const char* password, GError** error) {
    if (!sodiumReady() || !password) { g_set_error_literal(error, WENUY_SAFE_ERROR, WENUY_SAFE_ERROR_WRONG_PASSWORD, "Fel lösenord"); return nullptr; }
    auto* s = WENUY_SAFE(g_object_new(WENUY_TYPE_SAFE, nullptr));
    if (!load(s, pathOr(path), password, nullptr, error)) { g_object_unref(s); return nullptr; }
    return s;
}

WenuySafe* wenuy_safe_open_with_recovery(const char* path, const char* recovery_key, GError** error) {
    if (!sodiumReady() || !recovery_key) { g_set_error_literal(error, WENUY_SAFE_ERROR, WENUY_SAFE_ERROR_WRONG_PASSWORD, "Fel återställningsnyckel"); return nullptr; }
    auto* s = WENUY_SAFE(g_object_new(WENUY_TYPE_SAFE, nullptr));
    if (!load(s, pathOr(path), nullptr, recovery_key, error)) { g_object_unref(s); return nullptr; }
    return s;
}

char** wenuy_safe_list(WenuySafe* self) {
    GPtrArray* a = g_ptr_array_new();
    if (WENUY_IS_SAFE(self) && !self->locked)
        for (auto& [n, e] : *self->entries) g_ptr_array_add(a, g_strdup(n.c_str()));
    g_ptr_array_sort(a, +[](gconstpointer x, gconstpointer y) { return g_utf8_collate(*(const char* const*)x, *(const char* const*)y); });
    g_ptr_array_add(a, nullptr);
    return reinterpret_cast<char**>(g_ptr_array_free(a, FALSE));
}

char* wenuy_safe_get(WenuySafe* self, const char* name, const char* field) {
    g_return_val_if_fail(WENUY_IS_SAFE(self) && name, nullptr);
    if (self->locked) return nullptr;
    auto e = self->entries->find(name);
    if (e == self->entries->end()) return nullptr;
    auto f = e->second.fields.find(field ? field : "value");
    return f == e->second.fields.end() ? nullptr : g_strdup(f->second.c_str());
}
char* wenuy_safe_get_kind(WenuySafe* self, const char* name) {
    g_return_val_if_fail(WENUY_IS_SAFE(self) && name, nullptr);
    if (self->locked) return nullptr;
    auto e = self->entries->find(name);
    return e == self->entries->end() ? nullptr : g_strdup(e->second.kind.c_str());
}
char** wenuy_safe_get_fields(WenuySafe* self, const char* name) {
    GPtrArray* a = g_ptr_array_new();
    if (WENUY_IS_SAFE(self) && !self->locked && name) {
        auto e = self->entries->find(name);
        if (e != self->entries->end())
            for (auto& [k, v] : e->second.fields) g_ptr_array_add(a, g_strdup(k.c_str()));
    }
    g_ptr_array_add(a, nullptr);
    return reinterpret_cast<char**>(g_ptr_array_free(a, FALSE));
}
gint64 wenuy_safe_get_modified(WenuySafe* self, const char* name) {
    if (!WENUY_IS_SAFE(self) || self->locked || !name) return 0;
    auto e = self->entries->find(name);
    return e == self->entries->end() ? 0 : e->second.modified;
}

void wenuy_safe_set(WenuySafe* self, const char* name, const char* kind, const char* field, const char* value) {
    g_return_if_fail(WENUY_IS_SAFE(self) && name && *name);
    if (self->locked) return;
    Entry& e = (*self->entries)[name];
    if (kind && *kind) e.kind = kind;
    const std::string f = field ? field : "value";
    if (value) e.fields[f] = Secret(std::string(value));
    else e.fields.erase(f);
    e.modified = g_get_real_time() / G_USEC_PER_SEC;
}
gboolean wenuy_safe_rename(WenuySafe* self, const char* name, const char* new_name) {
    g_return_val_if_fail(WENUY_IS_SAFE(self) && name && new_name && *new_name, FALSE);
    if (self->locked || self->entries->count(new_name)) return FALSE;
    auto e = self->entries->find(name);
    if (e == self->entries->end()) return FALSE;
    Entry copy = e->second;
    self->entries->erase(e);
    (*self->entries)[new_name] = copy;
    return TRUE;
}
gboolean wenuy_safe_remove(WenuySafe* self, const char* name) {
    g_return_val_if_fail(WENUY_IS_SAFE(self) && name, FALSE);
    return !self->locked && self->entries->erase(name) > 0;
}

gboolean wenuy_safe_save(WenuySafe* self, GError** error) {
    g_return_val_if_fail(WENUY_IS_SAFE(self), FALSE);
    if (self->locked) { g_set_error_literal(error, WENUY_SAFE_ERROR, WENUY_SAFE_ERROR_LOCKED, "Kassaskåpet är låst"); return FALSE; }
    const std::string h = header(self);
    GVariant* v = serialize(self);
    GBytes* vb = g_variant_get_data_as_bytes(v);   // serialiseras en gång
    gsize plen = 0;
    gconstpointer vd = g_bytes_get_data(vb, &plen);
    auto* plain = static_cast<unsigned char*>(sodium_malloc(plen ? plen : 1));
    if (plen) memcpy(plain, vd, plen);
    // GVariant har en egen kopia av texterna – skriv över den innan den frigörs
    if (plen) sodium_memzero(const_cast<gpointer>(vd), plen);
    g_bytes_unref(vb);
    g_variant_unref(v);
    std::string out = h;
    unsigned char nonce[24];
    randombytes_buf(nonce, 24);
    out.append(reinterpret_cast<const char*>(nonce), 24);
    std::string ct(plen + 16, '\0');
    unsigned long long clen = 0;
    crypto_aead_xchacha20poly1305_ietf_encrypt(reinterpret_cast<unsigned char*>(&ct[0]), &clen, plain, plen,
                                               reinterpret_cast<const unsigned char*>(h.data()), h.size(), nullptr, nonce, self->dataKey);
    sodium_memzero(plain, plen);
    sodium_free(plain);
    out.append(ct.data(), clen);
    // atomiskt: skriv ny fil, behåll förra som .bak, byt namn (rättigheter: bara ägaren)
    const std::string& p = *self->path;
    gchar* dir = g_path_get_dirname(p.c_str());
    g_mkdir_with_parents(dir, 0700);
    g_free(dir);
    const std::string tmp = p + ".tmp";
    int fd = g_open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    bool ok = fd >= 0;
    if (ok) {
        size_t off = 0;
        while (ok && off < out.size()) { ssize_t n = write(fd, out.data() + off, out.size() - off); if (n <= 0) ok = false; else off += n; }
        ok = ok && fsync(fd) == 0;
        close(fd);
    }
    if (ok && g_file_test(p.c_str(), G_FILE_TEST_EXISTS)) {
        const std::string bak = p + ".bak";
        g_unlink(bak.c_str());
        if (link(p.c_str(), bak.c_str()) != 0) { /* ingen säkerhetskopia – inte ett fel */ }
    }
    ok = ok && g_rename(tmp.c_str(), p.c_str()) == 0;
    if (!ok) {
        g_unlink(tmp.c_str());
        g_set_error_literal(error, WENUY_SAFE_ERROR, WENUY_SAFE_ERROR_IO, "Kunde inte spara kassaskåpet");
        return FALSE;
    }
    return TRUE;
}

gboolean wenuy_safe_change_password(WenuySafe* self, const char* new_password, GError** error) {
    g_return_val_if_fail(WENUY_IS_SAFE(self), FALSE);
    if (self->locked) { g_set_error_literal(error, WENUY_SAFE_ERROR, WENUY_SAFE_ERROR_LOCKED, "Kassaskåpet är låst"); return FALSE; }
    if (weak(new_password, error)) return FALSE;
    randombytes_buf(self->salt, 16);
    self->ops = kOps; self->mem = kMem;
    if (!deriveKek(new_password, self->salt, self->ops, self->mem, self->pwKek)) {
        g_set_error_literal(error, WENUY_SAFE_ERROR, WENUY_SAFE_ERROR_IO, "För lite minne för att skydda lösenordet");
        return FALSE;
    }
    wrapWith(self, 1, self->pwKek);
    return wenuy_safe_save(self, error);
}

char* wenuy_safe_new_recovery_key(WenuySafe* self, GError** error) {
    g_return_val_if_fail(WENUY_IS_SAFE(self), nullptr);
    if (self->locked) { g_set_error_literal(error, WENUY_SAFE_ERROR, WENUY_SAFE_ERROR_LOCKED, "Kassaskåpet är låst"); return nullptr; }
    unsigned char* rk = static_cast<unsigned char*>(sodium_malloc(32));
    unsigned char* kek = static_cast<unsigned char*>(sodium_malloc(32));
    randombytes_buf(rk, 32);
    recoveryKek(rk, kek);
    wrapWith(self, 2, kek);
    char* text = g_strdup(recoveryText(rk).c_str());
    sodium_free(rk); sodium_free(kek);
    if (!wenuy_safe_save(self, error)) { wenuy_safe_free_secret(text); return nullptr; }
    return text;
}

void wenuy_safe_lock(WenuySafe* self) {
    if (!WENUY_IS_SAFE(self) || self->locked) return;
    self->entries->clear();   // Secret skriver över sig själv
    if (self->dataKey) { sodium_free(self->dataKey); self->dataKey = nullptr; }
    if (self->pwKek) { sodium_free(self->pwKek); self->pwKek = nullptr; }
    self->locked = TRUE;
}
gboolean wenuy_safe_is_locked(WenuySafe* self) { return !WENUY_IS_SAFE(self) || self->locked; }

void wenuy_safe_free_secret(char* secret) {
    if (!secret) return;
    sodium_memzero(secret, strlen(secret));
    g_free(secret);
}

int wenuy_safe_password_strength(const char* pw) {
    if (!pw) return 0;
    const glong n = g_utf8_strlen(pw, -1);
    bool lower = false, upper = false, digit = false, other = false;
    for (const char* c = pw; *c; c = g_utf8_next_char(c)) {
        gunichar u = g_utf8_get_char(c);
        if (g_unichar_islower(u)) lower = true;
        else if (g_unichar_isupper(u)) upper = true;
        else if (g_unichar_isdigit(u)) digit = true;
        else other = true;
    }
    const int kinds = lower + upper + digit + other;
    if (n < 8) return 0;
    if (n >= 20 || (n >= 14 && kinds >= 3)) return 4;   // långa lösenfraser är starkast
    if (n >= 12 && kinds >= 2) return 3;
    if (kinds >= 3) return 2;
    return 1;
}

char* wenuy_safe_generate_password(int length) {
    if (!sodiumReady()) return nullptr;
    length = std::clamp(length, 12, 64);
    static const char set[] = "abcdefghijkmnpqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789-_.!?";
    char* out = static_cast<char*>(g_malloc(length + 1));
    for (int i = 0; i < length; ++i) out[i] = set[randombytes_uniform(sizeof(set) - 1)];
    out[length] = 0;
    return out;
}
