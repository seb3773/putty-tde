#include <tqapplication.h>
#include <locale.h>

#include "tqtcomm.h"
#include "tqtwin.h"
#include "tqtdlg.h"

#include "putty_headers.h"


static bool do_cmdline(int argc, char **argv, bool do_everything, Conf *conf)
{
    bool err = false;
    while (--argc > 0) {
        const char *p = *++argv;
        if (!strcmp(p, "-version") || !strcmp(p, "--version") || !strcmp(p, "-V")) {
            char *buildinfo_text = buildinfo("\n");
            printf("PuTTY-TDE (TQt3 edition) %s\n%s\n", ver, buildinfo_text);
            sfree(buildinfo_text);
            exit(0);
        }
        if (!strcmp(p, "-help") || !strcmp(p, "--help") || !strcmp(p, "-h")) {
            printf("PuTTY-TDE - Trinity Desktop Environment native port\n");
            printf("Usage: %s [options] [user@]host\n", appname);
            printf("Options:\n"
                   "  --version, -V      Display version information and exit\n"
                   "  --help, -h         Display this help message and exit\n"
                   "  -load SESSION      Load settings from saved session\n"
                   "  -ssh, -telnet, -rlogin, -raw, -serial\n"
                   "                     Force use of specific protocol\n"
                   "  -P PORT            Connect to specified port\n"
                   "  -l USER            Connect with specified username\n"
                   "  -i KEY             Private key file for authentication\n"
                   "  -title TITLE       Set window title\n");
            exit(0);
        }

        int ret = cmdline_process_param(p, (argc > 1 ? argv[1] : NULL),
                                        do_everything ? 1 : -1, conf);
        if (ret == -2) {
            cmdline_error("option \"%s\" requires an argument", p);
            err = true;
        } else if (ret == 2) {
            --argc, ++argv;
            continue;
        } else if (ret == 1) {
            continue;
        }

        if (!strcmp(p, "-title") || !strcmp(p, "-T")) {
            if (--argc > 0) {
                if (do_everything)
                    conf_set_str(conf, CONF_wintitle, *++argv);
            } else {
                err = true;
            }
        }
    }
    return err;
}

int main(int argc, char **argv)
{
    TQApplication app(argc, argv);
    TQObject::disconnect(&app, TQ_SIGNAL(lastWindowClosed()), &app, TQ_SLOT(quit()));
    setlocale(LC_CTYPE, "");

    setup(true);
    tqtcomm_setup();

    Conf *conf = conf_new();
    bool need_config_box = true;

    if (do_cmdline(argc, argv, false, conf) == 0) {
        do_defaults(NULL, conf);
        do_cmdline(argc, argv, true, conf);
        cmdline_run_saved(conf);

        if (cmdline_tooltype & TOOLTYPE_HOST_ARG)
            need_config_box = !cmdline_host_ok(conf);
        else
            need_config_box = false;
    }

    if (need_config_box) {
        initial_config_box(conf, NULL, NULL);
    } else {
        new_session_window(conf, NULL);
    }

    int ret = app.exec();
    cleanup_exit(ret);
    return ret;
}
