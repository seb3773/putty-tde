#include "tqtdlg.h"
#include "tqtwin.h"
#include "putty_icons.h"

#include <tqheader.h>
#include <tqsplitter.h>
#include <tqcolordialog.h>
#include <tqfiledialog.h>
#include <tqfontdialog.h>
#include <tqmessagebox.h>
#include <tqapplication.h>
#include <tqbuttongroup.h>
#include <tqobjectlist.h>
#include <tqsettings.h>
#include <tqdesktopwidget.h>
#include <tqtimer.h>



struct dlgparam {
    PuTTYConfigDialog *dialog;
    void *data;
    struct {
        int r, g, b;
        bool ok;
    } coloursel;
};

static inline bool is_combobox(union control *ctrl)
{
    if (!ctrl) return false;
    if (ctrl->generic.type == CTRL_EDITBOX)
        return ctrl->editbox.has_list;
    if (ctrl->generic.type == CTRL_LISTBOX)
        return ctrl->listbox.height == 0;
    return false;
}

struct GridPlacement {
    union control *ctrl;
    TQWidget *w;
    int col;
    int span;
    int row;
    int rowspan;
};

// --------------------------------------------------------------------------
// PuTTYConfigDialog Implementation
// --------------------------------------------------------------------------

static PuTTYConfigDialog *s_mainConfigDialog = NULL;

PuTTYConfigDialog *get_main_config_dialog()
{
    return s_mainConfigDialog;
}

PuTTYConfigDialog::PuTTYConfigDialog(TQWidget *parent, const char *title, Conf *conf,
                                     bool midsession, int protocol,
                                     post_dialog_fn_t after, void *afterctx)
    : TQDialog(parent, "PuTTYConfigDialog", false),
      m_conf(conf),
      m_midsession(midsession),
      m_protocol(protocol),
      m_after(after),
      m_afterctx(afterctx),
      m_ctrlbox(0),
      m_hasSavedPos(false),
      m_hasSavedSize(false)
{
    if (m_midsession) {
        setWFlags(getWFlags() | TQt::WDestructiveClose);
    } else {
        s_mainConfigDialog = this;
    }
    setCaption(title ? title : "PuTTY-TDE Configuration");
    setIcon(get_puttycfg_icon());
    resize(720, 600);
    loadGeometrySettings();

    m_dlgparam = new dlgparam;
    m_dlgparam->dialog = this;
    m_dlgparam->data = m_conf;
    m_dlgparam->coloursel.ok = false;

    TQVBoxLayout *mainLayout = new TQVBoxLayout(this, 6, 6);

    TQSplitter *splitter = new TQSplitter(this);
    mainLayout->addWidget(splitter, 1);

    m_categoryTree = new TQListView(splitter);
    m_categoryTree->addColumn("Category");
    m_categoryTree->header()->hide();
    m_categoryTree->setRootIsDecorated(true);
    m_categoryTree->setMinimumWidth(160);

    m_pageStack = new TQWidgetStack(splitter);

    TQValueList<int> splitSizes;
    splitSizes << 180 << 500;
    splitter->setSizes(splitSizes);

    // Bottom action buttons
    TQHBoxLayout *btnLayout = new TQHBoxLayout(mainLayout);
    btnLayout->addStretch(1);

    TQPushButton *btnOpen = new TQPushButton(m_midsession ? "&Apply" : "&Open", this);
    btnOpen->setDefault(true);
    btnOpen->setMinimumWidth(80);
    TQPushButton *btnCancel = new TQPushButton("&Cancel", this);
    btnCancel->setMinimumWidth(80);

    btnLayout->addWidget(btnOpen);
    btnLayout->addWidget(btnCancel);

    connect(btnOpen, TQ_SIGNAL(clicked()), this, TQ_SLOT(onOpenClicked()));
    connect(btnCancel, TQ_SIGNAL(clicked()), this, TQ_SLOT(onCancelClicked()));
    connect(m_categoryTree, TQ_SIGNAL(selectionChanged(TQListViewItem *)),
            this, TQ_SLOT(onCategorySelected(TQListViewItem *)));

    buildCategoryTree();
    refreshAll();
    setInitialFocus();
}

void PuTTYConfigDialog::setInitialFocus()
{
    TQWidget *w = m_pageStack ? m_pageStack->visibleWidget() : 0;
    if (w) {
        TQObjectList *list = w->queryList("TQLineEdit");
        if (list) {
            if (!list->isEmpty()) {
                ((TQWidget*)list->first())->setFocus();
            }
            delete list;
        }
    }
}

PuTTYConfigDialog::~PuTTYConfigDialog()
{
    if (s_mainConfigDialog == this)
        s_mainConfigDialog = NULL;
    if (m_ctrlbox) {
        ctrl_free_box(m_ctrlbox);
        m_ctrlbox = 0;
    }
    delete m_dlgparam;
    if (!m_midsession && m_conf) {
        conf_free(m_conf);
        m_conf = 0;
    }
}

void PuTTYConfigDialog::finish(int val)
{
    if (val > 0)
        accept();
    else
        reject();
}

TQtControlItem *PuTTYConfigDialog::findItem(union control *ctrl)
{
    if (m_items.contains(ctrl))
        return m_items[ctrl];
    return 0;
}

void PuTTYConfigDialog::buildCategoryTree()
{
    m_ctrlbox = ctrl_new_box();
    setup_config_box(m_ctrlbox, m_midsession, m_protocol, 0);
    unix_setup_config_box(m_ctrlbox, m_midsession, m_protocol);

    TQMap<TQString, TQListViewItem*> treeNodes;
    TQListViewItem *firstItem = 0;

    for (size_t i = 0; i < m_ctrlbox->nctrlsets; ++i) {
        struct controlset *s = m_ctrlbox->ctrlsets[i];
        TQString path = s->pathname;

        if (!m_pathToPageIndex.contains(path)) {
            // Build tree node
            TQStringList parts = TQStringList::split('/', path);
            TQListViewItem *parentItem = 0;
            TQString curPath = "";

            for (TQStringList::Iterator it = parts.begin(); it != parts.end(); ++it) {
                if (!curPath.isEmpty()) curPath += "/";
                curPath += *it;

                if (treeNodes.contains(curPath)) {
                    parentItem = treeNodes[curPath];
                } else {
                    TQListViewItem *newItem = parentItem ?
                        new TQListViewItem(parentItem, *it) :
                        new TQListViewItem(m_categoryTree, *it);
                    newItem->setOpen(true);
                    treeNodes[curPath] = newItem;
                    parentItem = newItem;
                    if (!firstItem) firstItem = newItem;
                }
            }

            TQWidget *panel = buildPanel(path.utf8());
            int pageIdx = m_pageStack->addWidget(panel);
            m_pathToPageIndex[path] = pageIdx;
        }
    }

    if (treeNodes.contains("Session")) {
        m_categoryTree->setSelected(treeNodes["Session"], true);
        onCategorySelected(treeNodes["Session"]);
    } else if (firstItem) {
        m_categoryTree->setSelected(firstItem, true);
        onCategorySelected(firstItem);
    }
}

TQWidget *PuTTYConfigDialog::buildPanel(const char *path)
{
    TQScrollView *sv = new TQScrollView(m_pageStack);
    sv->setResizePolicy(TQScrollView::AutoOneFit);
    sv->setVScrollBarMode(TQScrollView::Auto);
    sv->setHScrollBarMode(TQScrollView::AlwaysOff);
    sv->setFrameShape(TQFrame::NoFrame);

    TQWidget *panel = new TQWidget(sv->viewport());
    TQVBoxLayout *panelLayout = new TQVBoxLayout(panel, 4, 4);

    for (size_t i = 0; i < m_ctrlbox->nctrlsets; ++i) {
        struct controlset *s = m_ctrlbox->ctrlsets[i];
        if (strcmp(s->pathname, path) == 0) {
            createControlSet(s, panel, panelLayout);
        }
    }

    panelLayout->addStretch(1);
    sv->addChild(panel);
    return sv;
}

void PuTTYConfigDialog::createControlSet(struct controlset *s, TQWidget *parentPanel, TQVBoxLayout *panelLayout)
{
    if (!s->boxname) {
        // Panel title header label
        if (s->boxtitle && strlen(s->boxtitle) > 0) {
            TQLabel *titleLbl = new TQLabel(s->boxtitle, parentPanel);
            TQFont f = titleLbl->font();
            f.setBold(true);
            titleLbl->setFont(f);
            panelLayout->addWidget(titleLbl);
            panelLayout->addSpacing(2);
        }
        return;
    }

    TQWidget *container = 0;
    TQVBoxLayout *containerLayout = 0;

    if (*s->boxname != '\0') {
        TQGroupBox *box = new TQGroupBox(s->boxtitle ? s->boxtitle : "", parentPanel);
        box->setColumnLayout(0, TQt::Vertical);
        box->layout()->setSpacing(4);
        box->layout()->setMargin(6);
        container = box;
        containerLayout = new TQVBoxLayout(box->layout());
        containerLayout->setSpacing(4);
        containerLayout->setMargin(0);
        panelLayout->addWidget(box);
    } else {
        container = parentPanel;
        containerLayout = panelLayout;
    }

    int cur_ncols = 1;
    const int *cur_percentages = NULL;

    size_t i = 0;
    while (i < s->ncontrols) {
        union control *ctrl = s->ctrls[i];

        if (ctrl->generic.type == CTRL_COLUMNS) {
            cur_ncols = ctrl->columns.ncols;
            cur_percentages = ctrl->columns.percentages;
            i++;
            continue;
        }

        if (ctrl->generic.type == CTRL_TABDELAY) {
            i++;
            continue;
        }

        if (cur_ncols <= 1) {
            TQWidget *w = createControl(ctrl, container);
            if (w) {
                containerLayout->addWidget(w);
            }
            i++;
        } else {
            TQValueList<GridPlacement> group;

            while (i < s->ncontrols) {
                union control *c = s->ctrls[i];
                if (c->generic.type == CTRL_COLUMNS)
                    break;
                if (c->generic.type == CTRL_TABDELAY) {
                    i++;
                    continue;
                }

                GridPlacement gp;
                gp.ctrl = c;
                gp.col = COLUMN_START(c->generic.column);
                gp.span = COLUMN_SPAN(c->generic.column);
                if (gp.col >= cur_ncols) {
                    gp.col = 0;
                    gp.span = 1;
                }
                if (gp.col + gp.span > cur_ncols) {
                    gp.span = cur_ncols - gp.col;
                }
                gp.w = createControl(c, container);
                gp.row = 0;
                gp.rowspan = 1;
                group.append(gp);
                i++;
            }

            if (group.isEmpty())
                continue;

            int next_row_in_col[16] = {0};
            for (uint g = 0; g < group.count(); ++g) {
                GridPlacement &gp = group[g];
                int r = 0;
                for (int c = gp.col; c < gp.col + gp.span; ++c) {
                    if (next_row_in_col[c] > r)
                        r = next_row_in_col[c];
                }
                gp.row = r;
                for (int c = gp.col; c < gp.col + gp.span; ++c) {
                    next_row_in_col[c] = r + 1;
                }
            }

            int total_rows = 0;
            for (int c = 0; c < cur_ncols; ++c) {
                if (next_row_in_col[c] > total_rows)
                    total_rows = next_row_in_col[c];
            }

            for (uint g = 0; g < group.count(); ++g) {
                GridPlacement &gp = group[g];
                bool has_subsequent = false;
                for (uint h = g + 1; h < group.count(); ++h) {
                    GridPlacement &other = group[h];
                    if (other.col < gp.col + gp.span && other.col + other.span > gp.col) {
                        has_subsequent = true;
                        break;
                    }
                }
                if (!has_subsequent && total_rows > gp.row) {
                    gp.rowspan = total_rows - gp.row;
                }
            }

            TQGridLayout *grid = new TQGridLayout(total_rows, cur_ncols, 6);
            for (int c = 0; c < cur_ncols; ++c) {
                int pct = cur_percentages ? cur_percentages[c] : (100 / cur_ncols);
                grid->setColStretch(c, pct);
            }

            for (uint g = 0; g < group.count(); ++g) {
                GridPlacement &gp = group[g];
                if (gp.w) {
                    grid->addMultiCellWidget(gp.w,
                                             gp.row,
                                             gp.row + gp.rowspan - 1,
                                             gp.col,
                                             gp.col + gp.span - 1);
                }
            }
            containerLayout->addLayout(grid);
        }
    }
}

TQWidget *PuTTYConfigDialog::createControl(union control *ctrl, TQWidget *container)
{
    TQtControlItem *item = new TQtControlItem;
    item->ctrl = ctrl;
    item->widget = 0;
    TQWidget *outerWidget = 0;

    switch (ctrl->generic.type) {
    case CTRL_TEXT: {
        TQLabel *lbl = new TQLabel(ctrl->generic.label ? ctrl->generic.label : "", container);
        lbl->setAlignment(TQt::AlignLeft | TQt::AlignVCenter);
        item->widget = lbl;
        outerWidget = lbl;
        break;
    }
    case CTRL_EDITBOX: {
        TQWidget *w = new TQWidget(container);
        if (ctrl->editbox.percentwidth == 100) {
            TQVBoxLayout *vl = new TQVBoxLayout(w, 0, 2);
            if (ctrl->generic.label && strlen(ctrl->generic.label) > 0) {
                TQLabel *lbl = new TQLabel(ctrl->generic.label, w);
                vl->addWidget(lbl);
            }
            if (ctrl->editbox.has_list) {
                TQComboBox *cb = new TQComboBox(true, w);
                vl->addWidget(cb);
                item->widget = cb;
                m_widgetToCtrl[cb] = ctrl;
                connect(cb, TQ_SIGNAL(textChanged(const TQString &)),
                        this, TQ_SLOT(onEditTextChanged(const TQString &)));
                connect(cb, TQ_SIGNAL(activated(int)),
                        this, TQ_SLOT(onListBoxSelected(int)));
            } else {
                TQLineEdit *edit = new TQLineEdit(w);
                if (ctrl->editbox.password)
                    edit->setEchoMode(TQLineEdit::Password);
                vl->addWidget(edit);
                item->widget = edit;
                m_widgetToCtrl[edit] = ctrl;
                connect(edit, TQ_SIGNAL(textChanged(const TQString &)),
                        this, TQ_SLOT(onEditTextChanged(const TQString &)));
            }
        } else {
            TQHBoxLayout *hl = new TQHBoxLayout(w, 0, 6);
            if (ctrl->generic.label && strlen(ctrl->generic.label) > 0) {
                TQLabel *lbl = new TQLabel(ctrl->generic.label, w);
                hl->addWidget(lbl);
            }
            if (ctrl->editbox.has_list) {
                TQComboBox *cb = new TQComboBox(true, w);
                hl->addWidget(cb, 1);
                item->widget = cb;
                m_widgetToCtrl[cb] = ctrl;
                connect(cb, TQ_SIGNAL(textChanged(const TQString &)),
                        this, TQ_SLOT(onEditTextChanged(const TQString &)));
                connect(cb, TQ_SIGNAL(activated(int)),
                        this, TQ_SLOT(onListBoxSelected(int)));
            } else {
                TQLineEdit *edit = new TQLineEdit(w);
                if (ctrl->editbox.password)
                    edit->setEchoMode(TQLineEdit::Password);
                if (ctrl->editbox.percentwidth <= 30) {
                    edit->setMaximumWidth(100);
                    hl->addWidget(edit);
                    hl->addStretch(1);
                } else {
                    hl->addWidget(edit, 1);
                }
                item->widget = edit;
                m_widgetToCtrl[edit] = ctrl;
                connect(edit, TQ_SIGNAL(textChanged(const TQString &)),
                        this, TQ_SLOT(onEditTextChanged(const TQString &)));
            }
        }
        outerWidget = w;
        break;
    }
    case CTRL_RADIO: {
        TQWidget *w = new TQWidget(container);
        TQVBoxLayout *vl = new TQVBoxLayout(w, 0, 2);
        if (ctrl->generic.label && strlen(ctrl->generic.label) > 0) {
            TQLabel *lbl = new TQLabel(ctrl->generic.label, w);
            vl->addWidget(lbl);
        }
        TQButtonGroup *grp = new TQButtonGroup(w);
        grp->setFrameShape(TQFrame::NoFrame);
        int ncols = ctrl->radio.ncolumns > 0 ? ctrl->radio.ncolumns : 1;
        int nrows = (ctrl->radio.nbuttons + ncols - 1) / ncols;
        TQGridLayout *grid = new TQGridLayout(grp, nrows, ncols, 2, 6);

        for (int b = 0; b < ctrl->radio.nbuttons; ++b) {
            TQRadioButton *rb = new TQRadioButton(ctrl->radio.buttons[b], grp);
            grid->addWidget(rb, b / ncols, b % ncols);
            item->radioButtons.append(rb);
            m_widgetToCtrl[rb] = ctrl;
            connect(rb, TQ_SIGNAL(clicked()), this, TQ_SLOT(onRadioButtonClicked()));
        }
        vl->addWidget(grp);
        item->widget = grp;
        outerWidget = w;
        break;
    }
    case CTRL_CHECKBOX: {
        TQCheckBox *cb = new TQCheckBox(ctrl->generic.label ? ctrl->generic.label : "", container);
        item->widget = cb;
        m_widgetToCtrl[cb] = ctrl;
        connect(cb, TQ_SIGNAL(toggled(bool)), this, TQ_SLOT(onCheckBoxToggled(bool)));
        outerWidget = cb;
        break;
    }
    case CTRL_BUTTON: {
        TQPushButton *btn = new TQPushButton(ctrl->generic.label ? ctrl->generic.label : "", container);
        item->widget = btn;
        m_widgetToCtrl[btn] = ctrl;
        connect(btn, TQ_SIGNAL(clicked()), this, TQ_SLOT(onButtonClicked()));
        outerWidget = btn;
        break;
    }
    case CTRL_LISTBOX: {
        TQWidget *w = new TQWidget(container);
        TQVBoxLayout *vl = new TQVBoxLayout(w, 0, 2);
        if (ctrl->generic.label && strlen(ctrl->generic.label) > 0) {
            TQLabel *lbl = new TQLabel(ctrl->generic.label, w);
            vl->addWidget(lbl);
        }
        if (ctrl->listbox.height == 0) {
            TQComboBox *cb = new TQComboBox(w);
            item->widget = cb;
            m_widgetToCtrl[cb] = ctrl;
            connect(cb, TQ_SIGNAL(activated(int)), this, TQ_SLOT(onListBoxSelected(int)));
            vl->addWidget(cb);
        } else {
            TQListBox *lb = new TQListBox(w);
            int rowHeight = lb->fontMetrics().lineSpacing();
            int minH = rowHeight * (ctrl->listbox.height > 0 ? ctrl->listbox.height : 5) + 8;
            lb->setMinimumHeight(minH);
            item->widget = lb;
            m_widgetToCtrl[lb] = ctrl;
            connect(lb, TQ_SIGNAL(selected(int)), this, TQ_SLOT(onListBoxSelected(int)));
            connect(lb, TQ_SIGNAL(doubleClicked(TQListBoxItem *)),
                    this, TQ_SLOT(onListBoxDoubleClicked(TQListBoxItem *)));
            vl->addWidget(lb, 1);
        }
        outerWidget = w;
        break;
    }
    case CTRL_FILESELECT: {
        TQWidget *w = new TQWidget(container);
        TQVBoxLayout *vl = new TQVBoxLayout(w, 0, 2);
        if (ctrl->generic.label && strlen(ctrl->generic.label) > 0) {
            TQLabel *lbl = new TQLabel(ctrl->generic.label, w);
            vl->addWidget(lbl);
        }
        TQHBoxLayout *hl = new TQHBoxLayout(vl, 4);
        TQLineEdit *edit = new TQLineEdit(w);
        TQPushButton *btnBrowse = new TQPushButton("Browse...", w);
        btnBrowse->setMaximumWidth(80);
        hl->addWidget(edit, 1);
        hl->addWidget(btnBrowse);
        item->widget = edit;
        m_widgetToCtrl[btnBrowse] = ctrl;
        m_widgetToCtrl[edit] = ctrl;
        connect(btnBrowse, TQ_SIGNAL(clicked()), this, TQ_SLOT(onFileBrowseClicked()));
        connect(edit, TQ_SIGNAL(textChanged(const TQString &)),
                this, TQ_SLOT(onEditTextChanged(const TQString &)));
        outerWidget = w;
        break;
    }
    case CTRL_FONTSELECT: {
        TQWidget *w = new TQWidget(container);
        TQVBoxLayout *vl = new TQVBoxLayout(w, 0, 2);
        if (ctrl->generic.label && strlen(ctrl->generic.label) > 0) {
            TQLabel *lbl = new TQLabel(ctrl->generic.label, w);
            vl->addWidget(lbl);
        }
        TQHBoxLayout *hl = new TQHBoxLayout(vl, 4);
        TQLabel *fontLbl = new TQLabel("Monospace 10", w);
        fontLbl->setFrameShape(TQFrame::StyledPanel);
        fontLbl->setFrameShadow(TQFrame::Sunken);
        fontLbl->setMargin(4);
        TQPushButton *btnFont = new TQPushButton("Change...", w);
        btnFont->setMaximumWidth(80);
        hl->addWidget(fontLbl, 1);
        hl->addWidget(btnFont);
        item->widget = fontLbl;
        m_widgetToCtrl[btnFont] = ctrl;
        connect(btnFont, TQ_SIGNAL(clicked()), this, TQ_SLOT(onFontChangeClicked()));
        outerWidget = w;
        break;
    }
    default:
        break;
    }

    m_items[ctrl] = item;
    return outerWidget;
}

void PuTTYConfigDialog::refreshControl(union control *ctrl)
{
    if (ctrl && ctrl->generic.handler) {
        ctrl->generic.handler(ctrl, m_dlgparam, m_conf, EVENT_REFRESH);
    }
}

void PuTTYConfigDialog::refreshAll()
{
    for (size_t i = 0; i < m_ctrlbox->nctrlsets; ++i) {
        struct controlset *s = m_ctrlbox->ctrlsets[i];
        for (size_t j = 0; j < s->ncontrols; ++j) {
            refreshControl(s->ctrls[j]);
        }
    }
}

void PuTTYConfigDialog::onCategorySelected(TQListViewItem *item)
{
    if (!item) return;

    TQString path = item->text(0);
    TQListViewItem *p = item->parent();
    while (p) {
        path = p->text(0) + "/" + path;
        p = p->parent();
    }

    if (m_pathToPageIndex.contains(path)) {
        m_pageStack->raiseWidget(m_pathToPageIndex[path]);
    }
}

void PuTTYConfigDialog::onOpenClicked()
{
    accept();
}

void PuTTYConfigDialog::onCancelClicked()
{
    reject();
}

void PuTTYConfigDialog::accept()
{
    if (m_midsession) {
        if (m_after) {
            post_dialog_fn_t after = m_after;
            void *ctx = m_afterctx;
            m_after = 0;
            m_afterctx = 0;
            after(ctx, 1);
        }
        TQDialog::accept();
    } else {
        m_savedPos = pos();
        m_hasSavedPos = true;
        m_savedSize = size();
        m_hasSavedSize = true;
        saveGeometrySettings();
        hide();
        Conf *sessconf = conf_copy(m_conf);
        new_session_window(sessconf, NULL);
    }
}

void PuTTYConfigDialog::reject()
{
    if (m_midsession) {
        if (m_after) {
            post_dialog_fn_t after = m_after;
            void *ctx = m_afterctx;
            m_after = 0;
            m_afterctx = 0;
            after(ctx, 0);
        }
        TQDialog::reject();
    } else {
        if (get_active_session_count() <= 0) {
            cleanup_exit(0);
        } else {
            hide();
        }
    }
}

void PuTTYConfigDialog::closeEvent(TQCloseEvent *e)
{
    reject();
    e->accept();
}

void PuTTYConfigDialog::moveEvent(TQMoveEvent *e)
{
    TQDialog::moveEvent(e);
    if (isVisible()) {
        m_savedPos = pos();
        m_hasSavedPos = true;
        saveGeometrySettings();
    }
}

void PuTTYConfigDialog::resizeEvent(TQResizeEvent *e)
{
    TQDialog::resizeEvent(e);
    if (isVisible()) {
        m_savedSize = size();
        m_hasSavedSize = true;
        saveGeometrySettings();
    }
}

void PuTTYConfigDialog::showEvent(TQShowEvent *e)
{
    TQDialog::showEvent(e);
    if (m_hasSavedPos) {
        move(m_savedPos);
        TQTimer::singleShot(50, this, TQ_SLOT(restorePosition()));
    }
}

void PuTTYConfigDialog::restorePosition()
{
    if (m_hasSavedPos) {
        move(m_savedPos);
    }
    if (m_hasSavedSize) {
        resize(m_savedSize);
    }
}

void PuTTYConfigDialog::saveGeometrySettings()
{
    if (m_midsession) return;
    TQSettings settings;
    if (m_hasSavedPos) {
        settings.writeEntry("/putty/config_window/x", m_savedPos.x());
        settings.writeEntry("/putty/config_window/y", m_savedPos.y());
    }
    if (m_hasSavedSize) {
        settings.writeEntry("/putty/config_window/width", m_savedSize.width());
        settings.writeEntry("/putty/config_window/height", m_savedSize.height());
    }
}

void PuTTYConfigDialog::loadGeometrySettings()
{
    if (m_midsession) return;
    TQSettings settings;
    bool okW = false, okH = false;
    int w = settings.readNumEntry("/putty/config_window/width", 0, &okW);
    int h = settings.readNumEntry("/putty/config_window/height", 0, &okH);
    if (okW && okH && w >= 640 && h >= 480) {
        m_savedSize = TQSize(w, h);
        m_hasSavedSize = true;
        resize(m_savedSize);
    }

    bool okX = false, okY = false;
    int x = settings.readNumEntry("/putty/config_window/x", -10000, &okX);
    int y = settings.readNumEntry("/putty/config_window/y", -10000, &okY);
    if (okX && okY && x > -5000 && y > -5000) {
        TQRect screen = TQApplication::desktop()->availableGeometry();
        if (x >= screen.left() - 100 && x < screen.right() - 100 &&
            y >= screen.top() - 50 && y < screen.bottom() - 100) {
            m_savedPos = TQPoint(x, y);
            m_hasSavedPos = true;
            move(m_savedPos);
        }
    }
}

void PuTTYConfigDialog::onButtonClicked()
{
    TQWidget *senderWidget = (TQWidget*)sender();
    if (m_widgetToCtrl.contains(senderWidget)) {
        union control *ctrl = m_widgetToCtrl[senderWidget];
        if (ctrl->generic.handler)
            ctrl->generic.handler(ctrl, m_dlgparam, m_conf, EVENT_ACTION);
    }
}

void PuTTYConfigDialog::onCheckBoxToggled(bool)
{
    TQWidget *senderWidget = (TQWidget*)sender();
    if (m_widgetToCtrl.contains(senderWidget)) {
        union control *ctrl = m_widgetToCtrl[senderWidget];
        if (ctrl->generic.handler)
            ctrl->generic.handler(ctrl, m_dlgparam, m_conf, EVENT_VALCHANGE);
    }
}

void PuTTYConfigDialog::onRadioButtonClicked()
{
    TQWidget *senderWidget = (TQWidget*)sender();
    if (m_widgetToCtrl.contains(senderWidget)) {
        union control *ctrl = m_widgetToCtrl[senderWidget];
        if (ctrl->generic.handler)
            ctrl->generic.handler(ctrl, m_dlgparam, m_conf, EVENT_VALCHANGE);
    }
}

void PuTTYConfigDialog::onEditTextChanged(const TQString &)
{
    TQWidget *senderWidget = (TQWidget*)sender();
    if (m_widgetToCtrl.contains(senderWidget)) {
        union control *ctrl = m_widgetToCtrl[senderWidget];
        if (ctrl->generic.handler)
            ctrl->generic.handler(ctrl, m_dlgparam, m_conf, EVENT_VALCHANGE);
    }
}

void PuTTYConfigDialog::onListBoxSelected(int)
{
    TQWidget *senderWidget = (TQWidget*)sender();
    if (m_widgetToCtrl.contains(senderWidget)) {
        union control *ctrl = m_widgetToCtrl[senderWidget];
        if (ctrl->generic.handler)
            ctrl->generic.handler(ctrl, m_dlgparam, m_conf, EVENT_SELCHANGE);
    }
}

void PuTTYConfigDialog::onListBoxDoubleClicked(TQListBoxItem *)
{
    TQWidget *senderWidget = (TQWidget*)sender();
    if (m_widgetToCtrl.contains(senderWidget)) {
        union control *ctrl = m_widgetToCtrl[senderWidget];
        if (ctrl->generic.handler)
            ctrl->generic.handler(ctrl, m_dlgparam, m_conf, EVENT_ACTION);
    }
}

void PuTTYConfigDialog::onFileBrowseClicked()
{
    TQWidget *senderWidget = (TQWidget*)sender();
    if (m_widgetToCtrl.contains(senderWidget)) {
        union control *ctrl = m_widgetToCtrl[senderWidget];
        TQtControlItem *item = findItem(ctrl);
        if (item && item->widget) {
            TQLineEdit *edit = (TQLineEdit*)item->widget;
            TQString path = TQFileDialog::getOpenFileName(edit->text(), TQString::null, this);
            if (!path.isEmpty()) {
                edit->setText(path);
                if (ctrl->generic.handler)
                    ctrl->generic.handler(ctrl, m_dlgparam, m_conf, EVENT_VALCHANGE);
            }
        }
    }
}

void PuTTYConfigDialog::onFontChangeClicked()
{
    TQWidget *senderWidget = (TQWidget*)sender();
    if (m_widgetToCtrl.contains(senderWidget)) {
        union control *ctrl = m_widgetToCtrl[senderWidget];
        bool ok = false;
        TQFont font = TQFontDialog::getFont(&ok, TQFont("Monospace", 10), this);
        if (ok) {
            TQtControlItem *item = findItem(ctrl);
            if (item && item->widget) {
                TQLabel *lbl = (TQLabel*)item->widget;
                lbl->setText(font.family() + " " + TQString::number(font.pointSize()));
                if (ctrl->generic.handler)
                    ctrl->generic.handler(ctrl, m_dlgparam, m_conf, EVENT_CALLBACK);
            }
        }
    }
}

// --------------------------------------------------------------------------
// dlg_* API implementation
// --------------------------------------------------------------------------

extern "C" {

void dlg_radiobutton_set(union control *ctrl, dlgparam *dp, int whichbutton)
{
    TQtControlItem *item = dp->dialog->findItem(ctrl);
    if (item && whichbutton >= 0 && whichbutton < (int)item->radioButtons.count()) {
        item->radioButtons.at(whichbutton)->setChecked(true);
    }
}

int dlg_radiobutton_get(union control *ctrl, dlgparam *dp)
{
    TQtControlItem *item = dp->dialog->findItem(ctrl);
    if (item) {
        for (uint i = 0; i < item->radioButtons.count(); ++i) {
            if (item->radioButtons.at(i)->isChecked())
                return i;
        }
    }
    return 0;
}

void dlg_checkbox_set(union control *ctrl, dlgparam *dp, bool checked)
{
    TQtControlItem *item = dp->dialog->findItem(ctrl);
    if (item && item->widget) {
        ((TQCheckBox*)item->widget)->setChecked(checked);
    }
}

bool dlg_checkbox_get(union control *ctrl, dlgparam *dp)
{
    TQtControlItem *item = dp->dialog->findItem(ctrl);
    if (item && item->widget) {
        return ((TQCheckBox*)item->widget)->isChecked();
    }
    return false;
}

void dlg_editbox_set(union control *ctrl, dlgparam *dp, char const *text)
{
    TQtControlItem *item = dp->dialog->findItem(ctrl);
    if (item && item->widget) {
        if (is_combobox(ctrl))
            ((TQComboBox*)item->widget)->setEditText(TQString::fromUtf8(text));
        else
            ((TQLineEdit*)item->widget)->setText(TQString::fromUtf8(text));
    }
}

char *dlg_editbox_get(union control *ctrl, dlgparam *dp)
{
    TQtControlItem *item = dp->dialog->findItem(ctrl);
    if (item && item->widget) {
        TQString text;
        if (is_combobox(ctrl))
            text = ((TQComboBox*)item->widget)->currentText();
        else
            text = ((TQLineEdit*)item->widget)->text();
        return dupstr(text.utf8().data());
    }
    return dupstr("");
}

void dlg_listbox_clear(union control *ctrl, dlgparam *dp)
{
    TQtControlItem *item = dp->dialog->findItem(ctrl);
    if (item && item->widget) {
        if (is_combobox(ctrl))
            ((TQComboBox*)item->widget)->clear();
        else
            ((TQListBox*)item->widget)->clear();
        item->listBoxIds.clear();
    }
}

void dlg_listbox_del(union control *ctrl, dlgparam *dp, int index)
{
    TQtControlItem *item = dp->dialog->findItem(ctrl);
    if (item && item->widget) {
        if (is_combobox(ctrl))
            ((TQComboBox*)item->widget)->removeItem(index);
        else
            ((TQListBox*)item->widget)->removeItem(index);
        if (index >= 0 && index < (int)item->listBoxIds.size())
            item->listBoxIds.remove(item->listBoxIds.at(index));
    }
}

void dlg_listbox_add(union control *ctrl, dlgparam *dp, char const *text)
{
    dlg_listbox_addwithid(ctrl, dp, text, 0);
}

void dlg_listbox_addwithid(union control *ctrl, dlgparam *dp, char const *text, int id)
{
    TQtControlItem *item = dp->dialog->findItem(ctrl);
    if (item && item->widget) {
        if (is_combobox(ctrl))
            ((TQComboBox*)item->widget)->insertItem(TQString::fromUtf8(text));
        else
            ((TQListBox*)item->widget)->insertItem(TQString::fromUtf8(text));
        item->listBoxIds.append(id);
    }
}

int dlg_listbox_getid(union control *ctrl, dlgparam *dp, int index)
{
    TQtControlItem *item = dp->dialog->findItem(ctrl);
    if (item && index >= 0 && index < (int)item->listBoxIds.size()) {
        return item->listBoxIds[index];
    }
    return -1;
}

int dlg_listbox_index(union control *ctrl, dlgparam *dp)
{
    TQtControlItem *item = dp->dialog->findItem(ctrl);
    if (item && item->widget) {
        if (is_combobox(ctrl))
            return ((TQComboBox*)item->widget)->currentItem();
        else
            return ((TQListBox*)item->widget)->currentItem();
    }
    return -1;
}

bool dlg_listbox_issel(union control *ctrl, dlgparam *dp, int index)
{
    return dlg_listbox_index(ctrl, dp) == index;
}

void dlg_listbox_select(union control *ctrl, dlgparam *dp, int index)
{
    TQtControlItem *item = dp->dialog->findItem(ctrl);
    if (item && item->widget) {
        if (is_combobox(ctrl))
            ((TQComboBox*)item->widget)->setCurrentItem(index);
        else
            ((TQListBox*)item->widget)->setSelected(index, true);
    }
}

void dlg_text_set(union control *ctrl, dlgparam *dp, char const *text)
{
    TQtControlItem *item = dp->dialog->findItem(ctrl);
    if (item && item->widget) {
        ((TQLabel*)item->widget)->setText(TQString::fromUtf8(text));
    }
}

void dlg_filesel_set(union control *ctrl, dlgparam *dp, Filename *fn)
{
    TQtControlItem *item = dp->dialog->findItem(ctrl);
    if (item && item->widget) {
        ((TQLineEdit*)item->widget)->setText(TQString::fromUtf8(fn->path));
    }
}

Filename *dlg_filesel_get(union control *ctrl, dlgparam *dp)
{
    TQtControlItem *item = dp->dialog->findItem(ctrl);
    if (item && item->widget) {
        return filename_from_str(((TQLineEdit*)item->widget)->text().utf8());
    }
    return filename_from_str("");
}

void dlg_fontsel_set(union control *ctrl, dlgparam *dp, FontSpec *fs)
{
    TQtControlItem *item = dp->dialog->findItem(ctrl);
    if (item && item->widget) {
        ((TQLabel*)item->widget)->setText(TQString::fromUtf8(fs->name));
    }
}

FontSpec *dlg_fontsel_get(union control *ctrl, dlgparam *dp)
{
    TQtControlItem *item = dp->dialog->findItem(ctrl);
    if (item && item->widget) {
        return fontspec_new(((TQLabel*)item->widget)->text().utf8());
    }
    return fontspec_new("Monospace 10");
}

void dlg_update_start(union control *ctrl, dlgparam *dp) { (void)ctrl; (void)dp; }
void dlg_update_done(union control *ctrl, dlgparam *dp) { (void)ctrl; (void)dp; }
void dlg_set_focus(union control *ctrl, dlgparam *dp)
{
    TQtControlItem *item = dp->dialog->findItem(ctrl);
    if (item && item->widget) {
        item->widget->setFocus();
    }
}

void dlg_label_change(union control *ctrl, dlgparam *dp, char const *text)
{
    dlg_text_set(ctrl, dp, text);
}

void dlg_beep(dlgparam *dp)
{
    (void)dp;
    TQApplication::beep();
}

void dlg_error_msg(dlgparam *dp, const char *msg)
{
    TQMessageBox::critical(dp->dialog, "PuTTY-TDE Error", msg);
}

void dlg_end(dlgparam *dp, int value)
{
    dp->dialog->finish(value);
}

void dlg_refresh(union control *ctrl, dlgparam *dp)
{
    if (ctrl)
        dp->dialog->refreshControl(ctrl);
    else
        dp->dialog->refreshAll();
}

void dlg_coloursel_start(union control *ctrl, dlgparam *dp, int r, int g, int b)
{
    TQColor initial(r, g, b);
    TQColor res = TQColorDialog::getColor(initial, dp->dialog);
    if (res.isValid()) {
        dp->coloursel.r = res.red();
        dp->coloursel.g = res.green();
        dp->coloursel.b = res.blue();
        dp->coloursel.ok = true;
    } else {
        dp->coloursel.ok = false;
    }

    if (ctrl && ctrl->generic.handler)
        ctrl->generic.handler(ctrl, dp, dp->data, EVENT_CALLBACK);
}

bool dlg_coloursel_results(union control *ctrl, dlgparam *dp, int *r, int *g, int *b)
{
    (void)ctrl;
    if (dp->coloursel.ok) {
        *r = dp->coloursel.r;
        *g = dp->coloursel.g;
        *b = dp->coloursel.b;
        return true;
    }
    return false;
}

union control *dlg_last_focused(union control *ctrl, dlgparam *dp)
{
    (void)ctrl;
    (void)dp;
    return NULL;
}

bool dlg_is_visible(union control *ctrl, dlgparam *dp)
{
    if (!dp || !dp->dialog) return false;
    TQtControlItem *item = dp->dialog->findItem(ctrl);
    if (item && item->widget)
        return item->widget->isVisible();
    return true;
}

void create_config_box(const char *title, Conf *conf,
                       bool midsession, int protocol,
                       post_dialog_fn_t after, void *afterctx)
{
    PuTTYConfigDialog *dlg = new PuTTYConfigDialog(
        0, title, conf, midsession, protocol, after, afterctx);
    dlg->show();
}

} // extern "C"
