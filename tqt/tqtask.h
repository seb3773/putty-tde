#ifndef PUTTY_TQTASK_H
#define PUTTY_TQTASK_H

#include "putty_headers.h"
 
#ifdef __cplusplus
extern "C" {
#endif

int tqt_seat_verify_ssh_host_key(
    Seat *seat, const char *host, int port,
    const char *keytype, char *keystr, char *key_fingerprint,
    void (*callback)(void *ctx, int result), void *ctx);

int tqt_seat_confirm_weak_crypto_primitive(
    Seat *seat, const char *type, const char *name,
    void (*callback)(void *ctx, int result), void *ctx);

int tqt_seat_confirm_weak_cached_hostkey(
    Seat *seat, const char *type, const char *better_type,
    void (*callback)(void *ctx, int result), void *ctx);

int tqt_seat_get_userpass_input(Seat *seat, prompts_t *p, bufchain *input);
void clear_cached_auth_password();
void tqt_seat_set_username(Seat *seat, const char *user);

#ifdef __cplusplus
}
#endif

#endif /* PUTTY_TQTASK_H */
