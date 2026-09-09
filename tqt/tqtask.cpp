#include "tqtask.h"
#include "putty_icons.h"

#include <tqdialog.h>
#include <tqlabel.h>
#include <tqlineedit.h>
#include <tqpushbutton.h>
#include <tqlayout.h>
#include <tqmessagebox.h>
#include <tqevent.h>

class FocusNextLineEdit : public TQLineEdit {
public:
    FocusNextLineEdit(TQWidget *parent, TQWidget *nextFocus = 0)
        : TQLineEdit(parent), m_nextFocus(nextFocus) {}
    void setNextFocusWidget(TQWidget *w) { m_nextFocus = w; }
protected:
    virtual void keyPressEvent(TQKeyEvent *e) {
        if (e->key() == TQt::Key_Return || e->key() == TQt::Key_Enter) {
            if (m_nextFocus) {
                m_nextFocus->setFocus();
                return;
            }
        }
        TQLineEdit::keyPressEvent(e);
    }
private:
    TQWidget *m_nextFocus;
};

static TQString s_cached_password;

void clear_cached_auth_password()
{
    s_cached_password = TQString::null;
}



int tqt_seat_verify_ssh_host_key(
    Seat *seat, const char *host, int port,
    const char *keytype, char *keystr, char *fingerprint,
    void (*callback)(void *ctx, int result), void *ctx)
{
    (void)seat;
    (void)callback;
    (void)ctx;
    int ret = verify_host_key(host, port, keytype, keystr);
    if (ret == 0) {
        return 1; // Match OK
    }

    TQString title;
    TQString text;

    if (ret == 2) {
        title = "PuTTY-TDE Security Alert - Host Key Mismatch";
        text = TQString(
            "WARNING - POTENTIAL SECURITY BREACH!\n\n"
            "The server's host key does not match the one PuTTY has cached.\n"
            "This means that either the server administrator has changed the host key,\n"
            "or you have actually connected to another computer pretending to be the server.\n\n"
            "The new %1 key fingerprint is:\n%2\n\n"
            "If you were expecting this change and trust the new key, click Accept to update PuTTY's cache.\n"
            "If you want to carry on connecting without updating the cache, click Connect Once.\n"
            "If you do not trust this host, click Cancel.").arg(keytype).arg(fingerprint);
    } else {
        title = "PuTTY-TDE Security Alert - Unknown Host Key";
        text = TQString(
            "The server's host key is not cached in the registry/store.\n"
            "You have no guarantee that the server is the computer you think it is.\n\n"
            "The server's %1 key fingerprint is:\n%2\n\n"
            "If you trust this host, click Accept to add the key to PuTTY's cache.\n"
            "If you want to carry on connecting just once, click Connect Once.\n"
            "If you do not trust this host, click Cancel.").arg(keytype).arg(fingerprint);
    }

    // Buttons: 0 = Accept, 1 = Connect Once, 2 = Cancel (escape button)
    int btn = TQMessageBox::warning(0, title, text,
                                   "&Accept", "Connect &Once", "&Cancel",
                                   0, 2);

    if (btn == 0) {
        store_host_key(host, port, keytype, keystr);
        return 1;
    } else if (btn == 1) {
        return 1; // Connect once
    } else {
        return 0; // Abort
    }
}

int tqt_seat_confirm_weak_crypto_primitive(
    Seat *seat, const char *type, const char *name,
    void (*callback)(void *ctx, int result), void *ctx)
{
    (void)seat;
    (void)callback;
    (void)ctx;
    TQString msg = TQString(
        "The first %1 supported by the server is %2, which is below the configured warning threshold.\n\n"
        "Continue with connection?").arg(type).arg(name);

    return (TQMessageBox::warning(0, "PuTTY-TDE Security Alert", msg,
                                  TQMessageBox::Yes, TQMessageBox::No) == TQMessageBox::Yes) ? 1 : 0;
}

int tqt_seat_confirm_weak_cached_hostkey(
    Seat *seat, const char *type, const char *better_type,
    void (*callback)(void *ctx, int result), void *ctx)
{
    (void)seat;
    (void)callback;
    (void)ctx;
    TQString msg = TQString(
        "The first host key type we have cached for this server is %1, but the server also supports %2.\n\n"
        "Continue with connection?").arg(type).arg(better_type);

    return (TQMessageBox::warning(0, "PuTTY-TDE Security Alert", msg,
                                  TQMessageBox::Yes, TQMessageBox::No) == TQMessageBox::Yes) ? 1 : 0;
}

int tqt_seat_get_userpass_input(Seat *seat, prompts_t *p, bufchain *input)
{
    (void)seat;
    (void)input;

    int ret = cmdline_get_passwd_input(p);
    if (ret != -1)
        return ret;

    if (p->n_prompts == 0)
        return 1;

    // Case 1: If we have a cached password and this is a single non-echo prompt (password prompt)
    if (!s_cached_password.isNull() && p->n_prompts == 1 && !p->prompts[0]->echo) {
        TQString pass = s_cached_password;
        s_cached_password = TQString::null; // consume it
        prompt_set_result(p->prompts[0], pass.utf8());
        return 1;
    }

    // Clear any stale cached password if we reach an interactive prompt
    s_cached_password = TQString::null;

    // Case 2: Check if this is the initial login / username prompt
    bool is_login_prompt = (p->n_prompts == 1 && p->prompts[0]->echo);
    if (is_login_prompt) {
        TQDialog dlg(0, "UserPassDialog", true);
        dlg.setCaption(p->name ? p->name : "PuTTY-TDE Authentication");
        dlg.setIcon(get_putty_icon());

        TQVBoxLayout *layout = new TQVBoxLayout(&dlg, 12, 8);

        if (p->instruction && strlen(p->instruction) > 0) {
            TQLabel *lblInstr = new TQLabel(p->instruction, &dlg);
            layout->addWidget(lblInstr);
        }

        // Username field
        const char *userPromptText = p->prompts[0]->prompt ? p->prompts[0]->prompt : "login as: ";
        TQLabel *lblUser = new TQLabel(userPromptText, &dlg);
        layout->addWidget(lblUser);
        FocusNextLineEdit *editUser = new FocusNextLineEdit(&dlg);
        layout->addWidget(editUser);

        // Password field
        TQLabel *lblPass = new TQLabel("Password: ", &dlg);
        layout->addWidget(lblPass);
        TQLineEdit *editPass = new TQLineEdit(&dlg);
        editPass->setEchoMode(TQLineEdit::Password);
        layout->addWidget(editPass);

        // Pressing Enter in username moves focus to password
        editUser->setNextFocusWidget(editPass);

        // Buttons
        TQHBoxLayout *btnLayout = new TQHBoxLayout(layout);
        btnLayout->addStretch(1);

        TQPushButton *btnOk = new TQPushButton("&OK", &dlg);
        btnOk->setDefault(true);
        btnOk->setMinimumWidth(85);
        TQPushButton *btnCancel = new TQPushButton("&Cancel", &dlg);
        btnCancel->setMinimumWidth(85);

        btnLayout->addWidget(btnOk);
        btnLayout->addWidget(btnCancel);

        dlg.connect(btnOk, TQ_SIGNAL(clicked()), &dlg, TQ_SLOT(accept()));
        dlg.connect(btnCancel, TQ_SIGNAL(clicked()), &dlg, TQ_SLOT(reject()));

        editUser->setFocus();

        dlg.adjustSize();
        dlg.setFixedWidth(300);

        if (dlg.exec() == TQDialog::Accepted) {
            prompt_set_result(p->prompts[0], editUser->text().utf8());
            if (!editPass->text().isEmpty()) {
                s_cached_password = editPass->text();
            }
            if (!editUser->text().isEmpty()) {
                tqt_seat_set_username(seat, editUser->text().utf8());
            }
            return 1;
        }

        return 0; // User cancelled
    }

    // Case 3: Password re-entry on failure, SSH key passphrase, 2FA, etc.
    TQDialog dlg(0, "UserPassDialog", true);
    dlg.setCaption(p->name ? p->name : "PuTTY-TDE Authentication");
    dlg.setIcon(get_putty_icon());

    TQVBoxLayout *layout = new TQVBoxLayout(&dlg, 12, 8);

    if (p->instruction && strlen(p->instruction) > 0) {
        TQLabel *lblInstr = new TQLabel(p->instruction, &dlg);
        layout->addWidget(lblInstr);
    }

    TQLineEdit **edits = new TQLineEdit*[p->n_prompts];

    for (size_t i = 0; i < p->n_prompts; ++i) {
        const char *promptText = p->prompts[i]->prompt ? p->prompts[i]->prompt : "Password: ";
        TQLabel *lbl = new TQLabel(promptText, &dlg);
        layout->addWidget(lbl);

        edits[i] = new TQLineEdit(&dlg);
        if (!p->prompts[i]->echo) {
            edits[i]->setEchoMode(TQLineEdit::Password);
        }
        layout->addWidget(edits[i]);
    }

    TQHBoxLayout *btnLayout = new TQHBoxLayout(layout);
    btnLayout->addStretch(1);

    TQPushButton *btnOk = new TQPushButton("&OK", &dlg);
    btnOk->setDefault(true);
    btnOk->setMinimumWidth(85);
    TQPushButton *btnCancel = new TQPushButton("&Cancel", &dlg);
    btnCancel->setMinimumWidth(85);

    btnLayout->addWidget(btnOk);
    btnLayout->addWidget(btnCancel);

    dlg.connect(btnOk, TQ_SIGNAL(clicked()), &dlg, TQ_SLOT(accept()));
    dlg.connect(btnCancel, TQ_SIGNAL(clicked()), &dlg, TQ_SLOT(reject()));

    if (p->n_prompts > 0) {
        edits[0]->setFocus();
    }

    dlg.adjustSize();
    dlg.setFixedWidth(300);

    if (dlg.exec() == TQDialog::Accepted) {
        for (size_t i = 0; i < p->n_prompts; ++i) {
            prompt_set_result(p->prompts[i], edits[i]->text().utf8());
            if (p->prompts[i]->echo && !edits[i]->text().isEmpty()) {
                tqt_seat_set_username(seat, edits[i]->text().utf8());
            }
        }
        delete[] edits;
        return 1;
    }

    delete[] edits;
    return 0;
}

