// libwenuy – WenuyOS Kassaskåp: krypterad förvaring av lösenord, nycklar och annat känsligt.
//
// Allt ligger i en enda krypterad fil (~/.local/share/wenuyos/kassaskap.wsafe) som bara går att
// öppna med användarens lösenord – eller med återställningsnyckeln om lösenordet glöms.
//
// Kryptering (libsodium, välprövad – ingen egen kryptografi):
//   - lösenordet blir en nyckel med Argon2id (minneskrävande – svårt att gissa med grafikkort)
//   - innehållet krypteras med XChaCha20-Poly1305 med en slumpad datanyckel; ändras ett enda tecken
//     i filen går den inte att öppna (ingen tyst förvanskning)
//   - datanyckeln lagras inlindad två gånger: med lösenordet och med återställningsnyckeln
//   - nycklar ligger i låst minne (hamnar inte i växlingsfilen) och skrivs över när skåpet låses
//
// Varje post har ett namn, en sort ("password", "key", "note", "card", "other") och fält
// (t.ex. "value", "user", "url", "note"). Fältet "value" är själva hemligheten.
#pragma once
#include <glib-object.h>

G_BEGIN_DECLS

#define WENUY_TYPE_SAFE (wenuy_safe_get_type())
G_DECLARE_FINAL_TYPE(WenuySafe, wenuy_safe, WENUY, SAFE, GObject)

#define WENUY_SAFE_ERROR (wenuy_safe_error_quark())
GQuark wenuy_safe_error_quark(void);
typedef enum {
    WENUY_SAFE_ERROR_WRONG_PASSWORD,   /* fel lösenord eller återställningsnyckel */
    WENUY_SAFE_ERROR_DAMAGED,          /* filen är skadad eller ändrad */
    WENUY_SAFE_ERROR_IO,               /* gick inte att läsa/skriva */
    WENUY_SAFE_ERROR_LOCKED,           /* skåpet är låst */
    WENUY_SAFE_ERROR_WEAK              /* lösenordet är för kort */
} WenuySafeError;

/**
 * wenuy_safe_default_path:
 *
 * Returns: (transfer full): sökvägen till användarens kassaskåp
 */
char* wenuy_safe_default_path(void);
/**
 * wenuy_safe_exists:
 * @path: (nullable): fil (%NULL = användarens kassaskåp)
 */
gboolean wenuy_safe_exists(const char* path);
/**
 * wenuy_safe_create:
 * @path: (nullable): fil (%NULL = användarens kassaskåp)
 * @password: lösenordet (minst 8 tecken)
 * @recovery_key: (out) (transfer full): återställningsnyckeln – visas EN gång, ska skrivas upp
 * @error: fel
 *
 * Returns: (transfer full) (nullable): ett nytt, öppet kassaskåp
 */
WenuySafe* wenuy_safe_create(const char* path, const char* password, char** recovery_key, GError** error);
/**
 * wenuy_safe_open:
 * @path: (nullable): fil (%NULL = användarens kassaskåp)
 * @password: lösenordet
 * @error: fel
 *
 * Returns: (transfer full) (nullable): det öppna kassaskåpet
 */
WenuySafe* wenuy_safe_open(const char* path, const char* password, GError** error);
/**
 * wenuy_safe_open_with_recovery:
 * @path: (nullable): fil (%NULL = användarens kassaskåp)
 * @recovery_key: återställningsnyckeln (mellanslag och bindestreck spelar ingen roll)
 * @error: fel
 *
 * Returns: (transfer full) (nullable): det öppna kassaskåpet – byt sedan lösenord
 */
WenuySafe* wenuy_safe_open_with_recovery(const char* path, const char* recovery_key, GError** error);

/**
 * wenuy_safe_list:
 * @self: kassaskåpet
 *
 * Returns: (transfer full) (array zero-terminated=1): postnamnen i bokstavsordning
 */
char** wenuy_safe_list(WenuySafe* self);
/**
 * wenuy_safe_get:
 * @self: kassaskåpet
 * @name: postens namn
 * @field: (nullable): fältet (%NULL = "value")
 *
 * Returns: (transfer full) (nullable): värdet – frigör med wenuy_safe_free_secret()
 */
char* wenuy_safe_get(WenuySafe* self, const char* name, const char* field);
/**
 * wenuy_safe_get_kind:
 * @self: kassaskåpet
 * @name: postens namn
 *
 * Returns: (transfer full) (nullable): sorten ("password", "key", "note", "card", "other")
 */
char* wenuy_safe_get_kind(WenuySafe* self, const char* name);
/**
 * wenuy_safe_get_fields:
 * @self: kassaskåpet
 * @name: postens namn
 *
 * Returns: (transfer full) (array zero-terminated=1): fältens namn
 */
char** wenuy_safe_get_fields(WenuySafe* self, const char* name);
/**
 * wenuy_safe_get_modified:
 * @self: kassaskåpet
 * @name: postens namn
 *
 * Returns: när posten ändrades (unix-tid), 0 om den inte finns
 */
gint64 wenuy_safe_get_modified(WenuySafe* self, const char* name);
/**
 * wenuy_safe_set:
 * @self: kassaskåpet
 * @name: postens namn (skapas om den inte finns)
 * @kind: (nullable): sorten (%NULL = behåll / "other")
 * @field: (nullable): fältet (%NULL = "value")
 * @value: (nullable): värdet (%NULL = ta bort fältet)
 *
 * Ändringen sparas först med wenuy_safe_save().
 */
void wenuy_safe_set(WenuySafe* self, const char* name, const char* kind, const char* field, const char* value);
/* Byt namn på en post. */
gboolean wenuy_safe_rename(WenuySafe* self, const char* name, const char* new_name);
/* Ta bort en post. */
gboolean wenuy_safe_remove(WenuySafe* self, const char* name);
/* Spara (krypterat, atomiskt; förra versionen sparas som .bak). */
gboolean wenuy_safe_save(WenuySafe* self, GError** error);
/* Byt lösenord (minst 8 tecken). Sparas direkt. */
gboolean wenuy_safe_change_password(WenuySafe* self, const char* new_password, GError** error);
/**
 * wenuy_safe_new_recovery_key:
 * @self: kassaskåpet
 * @error: fel
 *
 * Gör en ny återställningsnyckel (den gamla slutar fungera). Sparas direkt.
 *
 * Returns: (transfer full) (nullable): den nya nyckeln
 */
char* wenuy_safe_new_recovery_key(WenuySafe* self, GError** error);
/* Lås: nycklar och innehåll skrivs över i minnet; objektet går inte att använda efteråt. */
void wenuy_safe_lock(WenuySafe* self);
gboolean wenuy_safe_is_locked(WenuySafe* self);
/* Skriv över och frigör en hemlighet från wenuy_safe_get(). */
void wenuy_safe_free_secret(char* secret);

/**
 * wenuy_safe_password_strength:
 * @password: lösenordet
 *
 * Returns: 0 (svagt) … 4 (mycket starkt) – en enkel uppskattning för att visa
 */
int wenuy_safe_password_strength(const char* password);
/**
 * wenuy_safe_generate_password:
 * @length: antal tecken (12–64)
 *
 * Returns: (transfer full): ett slumpat lösenord (bokstäver, siffror och några tecken, inga
 *   förväxlingsbara som l/1/O/0) – frigör med wenuy_safe_free_secret()
 */
char* wenuy_safe_generate_password(int length);

G_END_DECLS
