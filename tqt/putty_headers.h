#ifndef PUTTY_HEADERS_H
#define PUTTY_HEADERS_H

#ifdef __cplusplus
#define private is_private
extern "C" {
#endif

#include "putty.h"
#include "terminal.h"
#include "dialog.h"
#include "storage.h"
#include "unix.h"
#include "ldisc.h"
#include "charset.h"

#ifdef __cplusplus
}
#undef private
#endif

#ifdef def
#undef def
#endif

#endif /* PUTTY_HEADERS_H */
