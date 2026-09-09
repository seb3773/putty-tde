#ifndef PUTTY_TQTDLG_H
#define PUTTY_TQTDLG_H

#include <tqdialog.h>
#include <tqlistview.h>
#include <tqwidgetstack.h>
#include <tqlineedit.h>
#include <tqcheckbox.h>
#include <tqradiobutton.h>
#include <tqpushbutton.h>
#include <tqlistbox.h>
#include <tqcombobox.h>
#include <tqlabel.h>
#include <tqgroupbox.h>
#include <tqscrollview.h>
#include <tqmap.h>
#include <tqptrlist.h>
#include <tqvaluelist.h>
#include <tqlayout.h>
#include <tqpoint.h>
#include <tqsize.h>

#include "putty_headers.h"


struct TQtControlItem {
    union control *ctrl;
    TQWidget *widget;
    TQPtrList<TQRadioButton> radioButtons;
    TQValueList<int> listBoxIds;
};

class PuTTYConfigDialog : public TQDialog {
    TQ_OBJECT
public:
    PuTTYConfigDialog(TQWidget *parent, const char *title, Conf *conf,
                      bool midsession, int protocol,
                      post_dialog_fn_t after, void *afterctx);
    virtual ~PuTTYConfigDialog();

    Conf *conf() const { return m_conf; }
    dlgparam *dp() { return m_dlgparam; }

    void finish(int val);

    TQtControlItem *findItem(union control *ctrl);

    void refreshControl(union control *ctrl);
    void refreshAll();
    void setInitialFocus();

public slots:
    void onCategorySelected(TQListViewItem *item);
    void onOpenClicked();
    void onCancelClicked();
    void onAboutClicked();

    void onButtonClicked();
    void onCheckBoxToggled(bool);
    void onRadioButtonClicked();
    void onEditTextChanged(const TQString &);
    void onListBoxSelected(int);
    void onListBoxDoubleClicked(TQListBoxItem *);
    void onFileBrowseClicked();
    void onFontChangeClicked();
    void restorePosition();

protected:
    virtual void closeEvent(TQCloseEvent *e);
    virtual void reject();
    virtual void accept();
    virtual void moveEvent(TQMoveEvent *e);
    virtual void resizeEvent(TQResizeEvent *e);
    virtual void showEvent(TQShowEvent *e);

private:
    void buildCategoryTree();
    TQWidget *buildPanel(const char *path);
    void createControlSet(struct controlset *s, TQWidget *parentPanel, TQVBoxLayout *panelLayout);
    TQWidget *createControl(union control *ctrl, TQWidget *container);
    void saveGeometrySettings();
    void loadGeometrySettings();

    TQPoint m_savedPos;
    TQSize m_savedSize;
    bool m_hasSavedPos;
    bool m_hasSavedSize;

    Conf *m_conf;
    bool m_midsession;
    int m_protocol;
    post_dialog_fn_t m_after;
    void *m_afterctx;

    struct controlbox *m_ctrlbox;
    dlgparam *m_dlgparam;

    TQListView *m_categoryTree;
    TQWidgetStack *m_pageStack;
    TQMap<TQString, int> m_pathToPageIndex;
    TQMap<union control*, TQtControlItem*> m_items;
    TQMap<TQWidget*, union control*> m_widgetToCtrl;
};

#ifdef __cplusplus
extern "C" {
#endif

void create_config_box(const char *title, Conf *conf,
                       bool midsession, int protocol,
                       post_dialog_fn_t after, void *afterctx);

#ifdef __cplusplus
}
PuTTYConfigDialog *get_main_config_dialog();
#else
}
#endif

#endif /* PUTTY_TQTDLG_H */
