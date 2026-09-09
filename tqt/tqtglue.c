/*
 * tqtglue.c - Platform glue definitions for PuTTY-TQt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "putty.h"
#include "ssh.h"
#include "storage.h"

void create_config_box(const char *title, Conf *conf,
                       bool midsession, int protocol,
                       post_dialog_fn_t after, void *afterctx);

const bool use_pty_argv = false;
char **pty_argv = NULL;
char *pty_osx_envrestore_prefix = NULL;

const bool use_event_log = true;
const bool new_session = true;
const bool saved_sessions = true;
const bool dup_check_launchable = true;
const bool share_can_be_downstream = true;
const bool share_can_be_upstream = true;

const bool buildinfo_gtk_relevant = false;

char *buildinfo_gtk_version(void)
{
    return dupstr("tqt3 version by seb3773");
}

void cleanup_exit(int code)
{
    sk_cleanup();
    random_save_seed();
    exit(code);
}

const struct BackendVtable *select_backend(Conf *conf)
{
    const struct BackendVtable *vt =
        backend_vt_from_proto(conf_get_int(conf, CONF_protocol));
    assert(vt != NULL);
    return vt;
}

char *make_default_wintitle(char *hostname)
{
    return dupcat(hostname, " - PuTTY-TDE");
}

char *platform_get_x_display(void)
{
    const char *display = getenv("DISPLAY");
    return dupstr(display ? display : ":0");
}

const unsigned cmdline_tooltype =
    TOOLTYPE_HOST_ARG |
    TOOLTYPE_PORT_ARG |
    TOOLTYPE_NO_VERBOSE_OPTION;

void setup(bool single)
{
    sk_init();
    settings_set_default_protocol(be_default_protocol);
    {
        const struct BackendVtable *vt =
            backend_vt_from_proto(be_default_protocol);
        settings_set_default_port(0);
        if (vt)
            settings_set_default_port(vt->default_port);
    }
}

void initial_config_box(Conf *conf, post_dialog_fn_t after, void *afterctx)
{
    create_config_box("PuTTY-TDE Configuration", conf, false, 0, after, afterctx);
}

void modalfatalbox(const char *p, ...)
{
    va_list ap;
    fprintf(stderr, "FATAL ERROR: ");
    va_start(ap, p);
    vfprintf(stderr, p, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

void nonfatal(const char *p, ...)
{
    va_list ap;
    char *msg;
    va_start(ap, p);
    msg = dupvprintf(p, ap);
    va_end(ap);
    fprintf(stderr, "ERROR: %s\n", msg);
    sfree(msg);
}

void cmdline_error(const char *p, ...)
{
    va_list ap;
    fprintf(stderr, "%s: ", appname);
    va_start(ap, p);
    vfprintf(stderr, p, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

void old_keyfile_warning(void)
{
    static const char message[] =
        "You are loading an SSH-2 private key which has an\n"
        "old version of the file format. This means your key\n"
        "file is not fully tamperproof. Future versions of\n"
        "PuTTY may stop supporting this private key format,\n"
        "so we recommend you convert your key to the new\n"
        "format.\n"
        "\n"
        "You can perform this conversion by loading the key\n"
        "into PuTTYgen and then saving it again.";

    static bool warned = false;

    if (!warned) {
        warned = true;
        nonfatal("%s", message);
    }
}

char *x_get_default(const char *key)
{
    (void)key;
    return NULL;
}

FontSpec *platform_default_fontspec(const char *name)
{
    if (!strcmp(name, "Font"))
        return fontspec_new("Monospace 10");
    else
        return fontspec_new("");
}

Filename *platform_default_filename(const char *name)
{
    if (!strcmp(name, "LogFileName"))
        return filename_from_str("putty.log");
    else
        return filename_from_str("");
}

char *platform_default_s(const char *name)
{
    if (!strcmp(name, "SerialLine"))
        return dupstr("/dev/ttyS0");
    return NULL;
}

bool platform_default_b(const char *name, bool def)
{
    if (!strcmp(name, "WinNameAlways"))
        return false;
    return def;
}

int platform_default_i(const char *name, int def)
{
    if (!strcmp(name, "CloseOnExit"))
        return 2;
    return def;
}

