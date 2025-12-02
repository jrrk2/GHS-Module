// --------------------------------------------------------------
//  PCLMockAPI.cpp  —  Unified Mock PixInsight API for Testing
// --------------------------------------------------------------
//  ARCHITECTURE:
//  - control_handle is ALWAYS pcl::Control* (the client pointer)
//  - g_objects maps: pcl::Control* -> MockBase*
//  - MockBase wraps: QWidget* OR QTreeWidgetItem* OR QLayout*
//  - All handles are consistent: client pointer is the key
// --------------------------------------------------------------

#include "PCLMockAPI.h"
#include "PCLThreadMock.h"
#include <unordered_map>
#include <memory>
#include <cstdio>
#include <QSvgRenderer>
#include <QScrollArea>
#include <QFontMetrics>
#include <QFont>
#include <QTreeWidget>
#include <pcl/api/APIInterface.h>

static bool g_enableDebugLogging = false;

void SetDebugLogging(bool on)
{
   g_enableDebugLogging = on;
}

// =============================================================
// Event Filter for Qt Events
// =============================================================

class MockEventFilter : public QObject
{
public:
    explicit MockEventFilter(MockBase* b)
        : QObject(b->widget), base(b) {}

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override
    {
        if (!base || obj != base->widget)
            return QObject::eventFilter(obj, ev);

        // --- SHOW EVENT ------------------------------------------------------
        if (ev->type() == QEvent::Show)
        {
            if (base->onShow)
                base->onShow(base->pcl_handle, base->pcl_handle);
        }

        // --- MOUSE MOVE ------------------------------------------------------
        if (ev->type() == QEvent::MouseMove)
        {
            if (base->onMouseMove)
            {
                auto* e = static_cast<QMouseEvent*>(ev);
                base->onMouseMove(base->pcl_handle,
                                  base->pcl_handle,
                                  e->x(),
                                  e->y(),
                                  e->buttons(),
                                  QApplication::keyboardModifiers());
                return false;
            }
        }

        // --- MOUSE PRESS -----------------------------------------------------
        if (ev->type() == QEvent::MouseButtonPress)
        {
            if (base->onMousePress)
            {
                auto* e = static_cast<QMouseEvent*>(ev);
                base->onMousePress(base->pcl_handle,
                                   base->pcl_handle,
                                   e->x(),
                                   e->y(),
                                   e->button(),
                                   e->buttons(),
                                   QApplication::keyboardModifiers());
                return false;
            }
        }

        // --- MOUSE RELEASE ---------------------------------------------------
        if (ev->type() == QEvent::MouseButtonRelease)
        {
            if (base->onMouseRelease)
            {
                auto* e = static_cast<QMouseEvent*>(ev);
                base->onMouseRelease(base->pcl_handle,
                                     base->pcl_handle,
                                     e->x(),
                                     e->y(),
                                     e->button(),
                                     e->buttons(),
                                     QApplication::keyboardModifiers());
                return false;
            }
        }

        // --- KEY PRESS -------------------------------------------------------
        if (ev->type() == QEvent::KeyPress)
        {
            if (base->onKeyPress)
            {
                /*
                auto* e = static_cast<QKeyEvent*>(ev);
                // Signature commented out in original
                base->onKeyPress(base->pcl_handle,
                                 base->pcl_handle,
                                 e->key(),
                                 QApplication::keyboardModifiers());
                */
                return false;
            }
        }

        return QObject::eventFilter(obj, ev);
    }

private:
    MockBase* base;
};

// =============================================================
// Global Object Registry
// =============================================================

template <typename H>
struct HandleHash
{
   std::size_t operator()(const void *h) const noexcept
   {
      auto p = reinterpret_cast<std::uintptr_t>(h);
      return std::hash<std::uintptr_t>{}(p);
   }
};
 
template <typename H>
struct HandleEqual
{
   bool operator()(const void *a, const void *b) const noexcept
   {
      return a == b;
   }
};

// The global object map: client pointer -> MockBase
static std::unordered_map<const void *, std::unique_ptr<MockBase>, 
                          HandleHash<control_handle>, 
                          HandleEqual<control_handle>> g_objects;

QList<MockBase*> g_topLevelWidgets;

// =============================================================
// Accessor Functions - SIMPLIFIED
// =============================================================

// Generic accessor - returns MockBase for any handle
static inline MockBase* get(const void *h)
{
    if (!h)
        return nullptr;
    auto it = g_objects.find(h);
    return (it == g_objects.end()) ? nullptr : it->second.get();
}

// Widget accessor
static inline QWidget* widgetFromHandle(control_handle h)
{
    MockBase* b = get(h);
    return (b ? b->widget : nullptr);
}

static inline QWidget* widgetFromHandle(const_control_handle h)
{
    return widgetFromHandle(const_cast<control_handle>(h));
}

// Layout accessor - returns QBoxLayout for sizer handles
static inline QBoxLayout* layoutFromSizer(sizer_handle s)
{
    if (!s)
        return nullptr;

    MockBase* b = get(s);
    return (b && b->isSizer) ? b->layout : nullptr;
}

static inline QBoxLayout* layoutFromSizer(const_sizer_handle s)
{
    if (!s)
        return nullptr;
        
    // Cast away const from the void*, then pass to non-const version
    MockBase* b = get(const_cast<void*>(s));
    return (b && b->isSizer) ? b->layout : nullptr;
}

// Tree widget accessor
static inline QTreeWidget* treeFromHandle(control_handle h)
{
    QWidget* w = widgetFromHandle(h);
    return qobject_cast<QTreeWidget*>(w);
}

static inline QTreeWidget* treeFromHandle(const_control_handle h)
{
    return treeFromHandle(const_cast<control_handle>(h));
}

// Tree item accessor
static inline QTreeWidgetItem* treeItemFromHandle(api_handle h)
{
    MockBase* b = get(h);
    return (b && b->isTreeNode) ? b->treeItem : nullptr;
}

static inline QTreeWidgetItem* treeItemFromHandle(const_api_handle h)
{
    return treeItemFromHandle(const_cast<api_handle>(h));
}

// =============================================================
// Utility: Logging
// =============================================================

static inline void logf(const char* fmt, ...)
{
    if (!g_enableDebugLogging)
        return;
        
    va_list ap; 
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap); 
    fprintf(stderr, "\n");
    va_end(ap);
}

// =============================================================
// Helper: Determine Parent Widget for Control Creation
// =============================================================

static QWidget* determineParentWidget(control_handle parent)
{
    if (!parent)
        return nullptr;  // Top-level widget
    
    MockBase* p = get(parent);
    if (!p)
        return nullptr;
    
    // Parent is a widget
    if (!p->isSizer && p->widget)
        return p->widget;
    
    // Parent is a layout - use layout's parent widget
    if (p->isSizer && p->layout)
        return p->layout->parentWidget();
    
    return nullptr;
}

// =============================================================
// CONTROL CREATION TEMPLATE - Consistent Architecture
// =============================================================

template <class T>
control_handle createControl(api_handle module, 
                             api_handle client,      // PCL Control* pointer
                             control_handle parent)
{
    // Determine parent Qt widget
    QWidget* parentWidget = determineParentWidget(parent);
    
    // Create Qt widget
    T* qtWidget = new T(parentWidget);
    
    // Create mock wrapper
    auto* mock = new MockBase();
    mock->isSizer = false;
    mock->isTreeNode = false;
    mock->moduleHandle = module;
    mock->widget = qtWidget;
    mock->layout = nullptr;
    mock->treeItem = nullptr;
    mock->pcl_handle = reinterpret_cast<control_handle>(client);
    
    // Install event filter
    qtWidget->installEventFilter(new MockEventFilter(mock));
    
    // CRITICAL: Use CLIENT (pcl::Control*) as key
    control_handle handle = reinterpret_cast<control_handle>(client);
    g_objects[handle] = std::unique_ptr<MockBase>(mock);
    
    logf("[Mock] createControl<%s> client=%p qtWidget=%p parent=%p",
         typeid(T).name(), client, qtWidget, parentWidget);
    
    // Return the PCL Control pointer
    return handle;
}

// =============================================================
// ControlContext API
// =============================================================
control_handle ControlContext::GetControlWindow(const_control_handle handle)
{
    MockBase* b = get(handle);
    if (!b || !b->widget)
        return nullptr;

    QWidget* w = b->widget;
    QWidget* top = w->window();

    for (auto& kv : g_objects)
    {
        MockBase* obj = kv.second.get();
        if (obj->widget == top)
            return reinterpret_cast<control_handle>(const_cast<void*>(kv.first));  // FIX: reinterpret_cast
    }

    return const_cast<control_handle>(handle);
}

control_handle ControlContext::CreateControl(
    api_handle module,
    api_handle client,
    control_handle parent,
    uint32 flags)
{
    // client is pcl::Control* 
    control_handle pclCtrl = reinterpret_cast<control_handle>(client);

    MockBase* b = new MockBase();
    b->isSizer = false;
    b->isTreeNode = false;
    b->moduleHandle = module;
    b->pcl_handle = pclCtrl;
    b->widget = new QWidget(determineParentWidget(parent));
    b->layout = nullptr;
    b->treeItem = nullptr;

    // Install event filter
    b->widget->installEventFilter(new MockEventFilter(b));

    // STORE using client as key
    g_objects[pclCtrl] = std::unique_ptr<MockBase>(b);

    logf("[Mock] CreateControl client=%p widget=%p", pclCtrl, b->widget);
    
    return pclCtrl;
}

void ControlContext::SetControlVisible(control_handle h, api_bool visible)
{
    if (!h) {
        qWarning() << "[SetControlVisible] NULL handle - ignoring";
        return;
    }

    QWidget* w = widgetFromHandle(h);
    if (!w)
        return;

    if (visible) {
        w->show();
        w->raise();
    } else {
        w->hide();
    }
}

// =============================================================
// Size Sanitization Helpers
// =============================================================

inline int sanitizePCLWidth(int v, QWidget* w)
{
    int prev = w->sizeHint().width();
    if (prev <= 0) prev = 100;
    
    if (v <= 0) return prev;           // Unspecified
    if (v > 5000) return prev;         // Absurd PCL value
    
    return v;  // Reasonable value
}

inline int sanitizePCLHeight(int v, QWidget* w)
{
    int prev = w->sizeHint().height();
    if (prev <= 0) prev = 0;
    
    if (v <= 0) return prev;
    if (v > 5000) return prev;
    
    return v;
}

void ControlContext::SetControlFixedSize(control_handle h, int32 w, int32 hgt)
{
    QWidget* widget = widgetFromHandle(h);
    if (!widget) return;
    
    int pxW = sanitizePCLWidth(w, widget);
    int pxH = sanitizePCLHeight(hgt, widget);
    
    logf("[Mock] SetControlFixedSize: %d x %d", pxW, pxH);
    widget->setFixedSize(pxW, pxH);
}

void ControlContext::SetControlMinSize(control_handle h, int32 w, int32 hgt)
{
    QWidget* widget = widgetFromHandle(h);
    if (!widget) return;
    
    int pxW = sanitizePCLWidth(w, widget);
    int pxH = sanitizePCLHeight(hgt, widget);
    
    logf("[Mock] SetControlMinSize: %d x %d", pxW, pxH);
    widget->setMinimumSize(pxW, pxH);
}

void ControlContext::SetWindowToolTip(control_handle h, const char16_type* t)
{
    QWidget* w = widgetFromHandle(h);
    if (w)
        w->setToolTip(QString::fromUtf16(t));
}

void ControlContext::SetWindowTitle(control_handle h, const char16_type* t)
{
    QWidget* w = widgetFromHandle(h);
    if (w)
        w->setWindowTitle(QString::fromUtf16(t));
}

void ControlContext::EnsureControlLayoutUpdated(control_handle h)
{
    QWidget* w = widgetFromHandle(h);
    if (!w) return;
    
    if (auto* l = w->layout()) {
        l->invalidate();
        l->activate();
    }
    w->updateGeometry();
    w->update();
}

void ControlContext::SetControlBackgroundColor(control_handle h, uint32 rgba)
{
    // Stub - implement if needed
}

void ControlContext::GetClientRect(const_control_handle h,
                                   int32* x, int32* y,
                                   int32* w, int32* hgt)
{
    if (!w || !hgt)
        return;

    QWidget* widget = widgetFromHandle(h);
    if (!widget)
        return;

    const QRect r = widget->contentsRect();

    if (x)   *x   = r.x();
    if (y)   *y   = r.y();
    *w   = r.width();
    *hgt = r.height();
}

// Event routine setters
api_bool ControlContext::SetKeyPressEventRoutine(
    control_handle h,
    api_handle /*client*/,
    pcl::keyboard_event_routine r)
{
    MockBase* b = get(h);
    if (b) b->onKeyPress = r;
    return api_true;
}

api_bool ControlContext::SetMousePressEventRoutine(
    control_handle h,
    api_handle /*client*/,
    pcl::mouse_button_event_routine r)
{
    MockBase* b = get(h);
    if (b) b->onMousePress = r;
    return api_true;
}

api_bool ControlContext::SetMouseReleaseEventRoutine(
    control_handle h,
    api_handle /*client*/,
    pcl::mouse_button_event_routine r)
{
    MockBase* b = get(h);
    if (b) b->onMouseRelease = r;
    return api_true;
}

api_bool ControlContext::SetMouseMoveEventRoutine(
    control_handle h,
    api_handle /*client*/,
    pcl::mouse_event_routine r)
{
    MockBase* b = get(h);
    if (b) b->onMouseMove = r;
    return api_true;
}

api_bool ControlContext::SetShowEventRoutine(
    control_handle h,
    api_handle /*client*/,
    pcl::control_event_routine r)
{
    MockBase* b = get(h);
    if (b) b->onShow = r;
    return api_true;
}

api_bool ControlContext::SetHideEventRoutine(
    control_handle h,
    api_handle /*client*/,
    pcl::control_event_routine r)
{
    MockBase* b = get(h);
    if (b) b->onShow = r;  // Note: Original had this as onShow
    return api_true;
}

// ... Continue with rest of the implementation (sizers, labels, buttons, etc.) ...
// =============================================================
// Control Context - Stubs and Additional Functions
// =============================================================

void ControlContext::SetChildControlToFocus(control_handle, control_handle) {}
void ControlContext::SetControlFocusStyle(control_handle, int32) {}
void ControlContext::GetControlPosition(const_control_handle, int32*x, int32*y) { *x = 0; *y = 0; }
void ControlContext::SetControlPosition(control_handle, int32, int32) {}
void ControlContext::SetControlSize(control_handle, int32, int32) {}
void ControlContext::SetControlEnabled(control_handle, api_bool) {}
void ControlContext::SetRealTimePreviewActive(control_handle, api_bool) {}
api_bool ControlContext::GetControlEnabled(const_control_handle) { return api_true; }
api_bool ControlContext::GetControlVisible(const_control_handle) { return api_true; }
font_handle ControlContext::GetControlFont(const_control_handle) { return new QFont; }
void ControlContext::GetControlMaxSize(const_control_handle, int32*x, int32*y) { *x = 100; *y = 100; }
void ControlContext::GetControlMinSize(const_control_handle, int32*x, int32*y) { *x = 100; *y = 100; }
void ControlContext::SetControlFocus(control_handle, api_bool) {}
void ControlContext::SetControlMaxSize(control_handle, int32, int32) {}
void ControlContext::SetControlUpdatesEnabled(control_handle, api_bool) {}
void ControlContext::AdjustControlToContents(control_handle) {}
api_bool ControlContext::SetGetFocusEventRoutine(control_handle, api_handle, pcl::control_event_routine) { return api_true; }
api_bool ControlContext::SetLoseFocusEventRoutine(control_handle, api_handle, pcl::control_event_routine) { return api_true; }
api_bool ControlContext::SetFileDragEventRoutine(control_handle, api_handle, pcl::file_drag_event_handler) { return api_true; }
api_bool ControlContext::SetFileDropEventRoutine(control_handle, api_handle, pcl::file_drag_event_handler) { return api_true; }
control_handle ControlContext::GetControlParent(const_control_handle) { return nullptr; }
api_bool ControlContext::GetControlDisplayPixelRatio(const_control_handle, double* ratio) { *ratio = 1.0; return api_true; }
api_bool ControlContext::GetControlResourcePixelRatio(const_control_handle, double* ratio) { *ratio = 1.0; return api_true; }
void ControlContext::GetControlExpansionEnabled(const_control_handle, api_bool*, api_bool*) {}
void ControlContext::SetControlExpansionEnabled(control_handle, api_bool, api_bool) {}
api_bool ControlContext::GetControlUnderMouseStatus(const_control_handle) { return api_false; }
void ControlContext::BringControlToFront(control_handle) {}
void ControlContext::SendControlToBack(control_handle) {}
void ControlContext::StackControls(control_handle, control_handle) {}
sizer_handle ControlContext::GetControlSizer(const_control_handle) { return nullptr; }
void ControlContext::GlobalToLocal(const_control_handle, int32*, int32*) {}
void ControlContext::LocalToGlobal(const_control_handle, int32*, int32*) {}
void ControlContext::ParentToLocal(const_control_handle, int32*, int32*) {}
void ControlContext::LocalToParent(const_control_handle, int32*, int32*) {}
void ControlContext::ControlToLocal(const_control_handle, const_control_handle, int32*, int32*) {}
void ControlContext::LocalToControl(const_control_handle, const_control_handle, int32*, int32*) {}
control_handle ControlContext::GetChildByPos(const_control_handle, int32, int32) { return nullptr; }
void ControlContext::GetChildrenRect(const_control_handle, int32*, int32*, int32*, int32*) {}
api_bool ControlContext::GetControlAncestry(const_control_handle, const_control_handle) { return api_false; }

// =============================================================
// Sizer Creation
// =============================================================

sizer_handle SizerContext::CreateSizer(api_handle module, api_bool vertical)
{
    auto* b = new MockBase();
    b->isSizer = true;
    b->isTreeNode = false;
    b->vertical = vertical;
    b->moduleHandle = module;
    b->widget = nullptr;
    b->treeItem = nullptr;

    b->layout = vertical
        ? static_cast<QBoxLayout*>(new QVBoxLayout())
        : static_cast<QBoxLayout*>(new QHBoxLayout());

    // Use MockBase pointer as the handle for sizers
    // This is different from controls where we use client pointer
    sizer_handle h = reinterpret_cast<sizer_handle>(b);
    g_objects[h] = std::unique_ptr<MockBase>(b);

    logf("[Mock] CreateSizer vertical=%d handle=%p", vertical, h);
    return h;
}

// =============================================================
// Sizer Insertion
// =============================================================

void SizerContext::InsertSizerControl(
    sizer_handle s, int32 index, control_handle c, int32 stretch, int32 /*flags*/)
{
    logf("[Mock] InsertSizerControl: sizer=%p control=%p", s, c);
    
    QBoxLayout* layout = layoutFromSizer(s);
    QWidget* widget = widgetFromHandle(c);
    
    if (!layout || !widget)
        return;

    if (index < 0 || index >= layout->count())
        layout->addWidget(widget, stretch);
    else
        layout->insertWidget(index, widget, stretch);
}

void SizerContext::InsertSizer(
    sizer_handle s, int32 index, sizer_handle child, int32 stretch)
{
    logf("[Mock] InsertSizer: parent=%p child=%p", s, child);
 
    QBoxLayout* parentLayout = layoutFromSizer(s);
    QBoxLayout* childLayout = layoutFromSizer(child);
    
    if (!parentLayout || !childLayout)
        return;

    if (index < 0 || index >= parentLayout->count())
        parentLayout->addLayout(childLayout, stretch);
    else
        parentLayout->insertLayout(index, childLayout, stretch);
}

void SizerContext::InsertSizerSpacing(sizer_handle s, int32 index, int32 px)
{
    QBoxLayout* layout = layoutFromSizer(s);
    if (!layout) return;
    
    if (index < 0 || index > layout->count())
        index = layout->count();
    layout->insertSpacing(index, px);
}

void SizerContext::InsertSizerStretch(sizer_handle s, int32 index, int32 stretch)
{
    QBoxLayout* layout = layoutFromSizer(s);
    if (!layout) return;
    
    if (index < 0 || index > layout->count())
        index = layout->count();
    layout->insertStretch(index, stretch);
}

void SizerContext::SetSizerMargin(sizer_handle s, int32 px)
{
    QBoxLayout* layout = layoutFromSizer(s);
    if (layout)
        layout->setContentsMargins(px, px, px, px);
}

void SizerContext::SetSizerSpacing(sizer_handle s, int32 px)
{
    QBoxLayout* layout = layoutFromSizer(s);
    if (layout)
        layout->setSpacing(px);
}

api_bool SizerContext::GetSizerDisplayPixelRatio(const_sizer_handle s, double* ratio)
{
    if (!ratio)
        return api_false;

    QBoxLayout* l = layoutFromSizer(s);
    double r = 1.0;

    if (l && l->parentWidget())
        r = l->parentWidget()->devicePixelRatioF();

    *ratio = r;
    return api_true;
}

// =============================================================
// Attach Sizer to Control
// =============================================================

void ControlContext::SetControlSizer(control_handle ctrl, sizer_handle s)
{
    MockBase* C = get(ctrl);
    MockBase* S = get(s);

    if (!S || !S->isSizer || !S->layout) {
        logf("[Mock] SetControlSizer: invalid sizer ctrl=%p sizer=%p", ctrl, s);
        return;
    }

    // If there is no MockBase for ctrl yet, create one
    if (!C) {
        C = new MockBase();
        C->isSizer = false;
        C->isTreeNode = false;
        C->moduleHandle = nullptr;
        C->widget = nullptr;
        C->layout = nullptr;
        C->treeItem = nullptr;
        g_objects[ctrl] = std::unique_ptr<MockBase>(C);
    }

    // If this "control" has no widget, treat it as a top-level window
    if (!C->widget) {
        C->widget = new QWidget(nullptr);
        C->widget->setObjectName("MockTopLevelWindow");
        g_topLevelWidgets.append(C);

        logf("[Mock] SetControlSizer: created top-level container ctrl=%p widget=%p",
             ctrl, C->widget);
    }

    QWidget* container = C->widget;
    QLayout* layout = S->layout;

    logf("[Mock] SetControlSizer: ctrl=%p widget=%p sizer=%p layout=%p",
         ctrl, container, s, layout);

    container->setLayout(layout);
    layout->setParent(container);
}

// =============================================================
// Label Context
// =============================================================

control_handle LabelContext::CreateLabel(
    api_handle m, api_handle c, const char16_type*, control_handle parent, uint32 flags)
{
    return createControl<QLabel>(m, c, parent);
}

void LabelContext::SetLabelText(control_handle h, const char16_type* t)
{
    QLabel* label = qobject_cast<QLabel*>(widgetFromHandle(h));
    if (label)
        label->setText(QString::fromUtf16(t));
}

void LabelContext::SetLabelAlignment(control_handle h, int32 flags)
{
    QLabel* label = qobject_cast<QLabel*>(widgetFromHandle(h));
    if (!label) return;
    
    Qt::Alignment a{};
    uint32 f = uint32(flags);

    if (f & 0x01) a |= Qt::AlignLeft;
    if (f & 0x02) a |= Qt::AlignHCenter;
    if (f & 0x04) a |= Qt::AlignRight;
    if (f & 0x10) a |= Qt::AlignTop;
    if (f & 0x20) a |= Qt::AlignVCenter;
    if (f & 0x40) a |= Qt::AlignBottom;

    if (!a) a = Qt::AlignLeft | Qt::AlignVCenter;

    label->setAlignment(a);
}

// =============================================================
// Edit Context
// =============================================================

control_handle EditContext::CreateEdit(
    api_handle m, api_handle c, const char16_type*, control_handle parent, uint32 flags)
{
    return createControl<QLineEdit>(m, c, parent);
}

void EditContext::SetEditText(control_handle h, const char16_type* t)
{
    QLineEdit* edit = qobject_cast<QLineEdit*>(widgetFromHandle(h));
    if (edit)
        edit->setText(QString::fromUtf16(t));
}

api_bool EditContext::SetEditValidatingRegExp(
    control_handle h,
    const char16_type* pattern,
    api_bool caseSensitive)
{
    QLineEdit* edit = qobject_cast<QLineEdit*>(widgetFromHandle(h));
    if (!edit) return api_false;

    QString pat = QString::fromUtf16(pattern);
    QRegularExpression re(pat);

    if (!caseSensitive)
        re.setPatternOptions(QRegularExpression::CaseInsensitiveOption);

    edit->setValidator(new QRegularExpressionValidator(re, edit));
    return api_true;
}

api_bool EditContext::GetEditReadOnly(const_control_handle) { return api_false; }
api_bool EditContext::GetEditText(const_control_handle, char16_type*, size_type*) { return api_false; }
void EditContext::SetEditSelected(control_handle, api_bool) {}
api_bool EditContext::SetEditCompletedEventRoutine(control_handle, api_handle, pcl::event_routine) { return api_true; }
api_bool EditContext::SetReturnPressedEventRoutine(control_handle, api_handle, pcl::event_routine) { return api_true; }

// =============================================================
// Slider Context
// =============================================================

control_handle SliderContext::CreateSlider(
    api_handle m, api_handle c, api_bool vertical, control_handle parent, uint32 flags)
{
    return createControl<QSlider>(m, c, parent);
}

void SliderContext::SetSliderValue(control_handle h, int value)
{
    QSlider* slider = qobject_cast<QSlider*>(widgetFromHandle(h));
    if (slider)
        slider->setValue(value);
}

void SliderContext::SetSliderRange(control_handle h, int32 mn, int32 mx)
{
    QSlider* slider = qobject_cast<QSlider*>(widgetFromHandle(h));
    if (slider) {
        slider->setMinimum(mn);
        slider->setMaximum(mx);
    }
}

void SliderContext::SetSliderPageSize(control_handle h, int32 page)
{
    QSlider* slider = qobject_cast<QSlider*>(widgetFromHandle(h));
    if (slider)
        slider->setPageStep(page);
}

void SliderContext::SetSliderTickInterval(control_handle h, int32 interval)
{
    QSlider* slider = qobject_cast<QSlider*>(widgetFromHandle(h));
    if (slider)
        slider->setTickInterval(interval);
}

void SliderContext::SetSliderTickStyle(control_handle h, int32 style)
{
    QSlider* slider = qobject_cast<QSlider*>(widgetFromHandle(h));
    if (!slider) return;
    
    uint32 st = uint32(style);
    QSlider::TickPosition pos = QSlider::NoTicks;

    if (st & 0x01) pos = QSlider::TicksAbove;
    if (st & 0x02) pos = QSlider::TicksBelow;
    if (st & 0x03) pos = QSlider::TicksBothSides;

    slider->setTickPosition(pos);
}

void SliderContext::GetSliderRange(const_control_handle, int32* minValue, int32* maxValue) { *minValue = 0; *maxValue = 100; }
int32 SliderContext::GetSliderValue(const_control_handle) { return 0; }
api_bool SliderContext::SetSliderValueUpdatedEventRoutine(control_handle, api_handle, pcl::value_event_routine) { return api_true; }

// =============================================================
// SpinBox Context
// =============================================================

control_handle SpinBoxContext::CreateSpinBox(api_handle module, api_handle client, control_handle parent, uint32 flags)
{
    return createControl<QSpinBox>(module, client, parent);
}

void SpinBoxContext::SetSpinBoxRange(control_handle h, int minv, int maxv)
{
    QSpinBox* spin = qobject_cast<QSpinBox*>(widgetFromHandle(h));
    if (spin)
        spin->setRange(minv, maxv);
}

void SpinBoxContext::SetSpinBoxValue(control_handle h, int value)
{
    QSpinBox* spin = qobject_cast<QSpinBox*>(widgetFromHandle(h));
    if (spin)
        spin->setValue(value);
}

api_bool SpinBoxContext::SetSpinBoxValueUpdatedEventRoutine(control_handle, api_handle, pcl::value_event_routine) { return api_true; }

// =============================================================
// Button Context
// =============================================================

control_handle ButtonContext::CreateCheckBox(
    api_handle m, api_handle c, const char16_type*, control_handle parent, uint32 flags)
{
    return createControl<QCheckBox>(m, c, parent);
}

control_handle ButtonContext::CreatePushButton(
    api_handle m, api_handle c,
    const char16_type* text,
    const_bitmap_handle icon,
    control_handle parent,
    uint32 flags)
{
    control_handle h = createControl<QPushButton>(m, c, parent);
    
    if (text) {
        QPushButton* btn = qobject_cast<QPushButton*>(widgetFromHandle(h));
        if (btn)
            btn->setText(QString::fromUtf16(text));
    }
    
    logf("[Mock] CreatePushButton handle=%p", h);
    return h;
}

control_handle ButtonContext::CreateRadioButton(
    api_handle m, api_handle c,
    const char16_type* text,
    control_handle parent,
    uint32 flags)
{
    control_handle h = createControl<QRadioButton>(m, c, parent);
    
    if (text) {
        QRadioButton* btn = qobject_cast<QRadioButton*>(widgetFromHandle(h));
        if (btn)
            btn->setText(QString::fromUtf16(text));
    }
    
    logf("[Mock] CreateRadioButton handle=%p", h);
    return h;
}

control_handle ButtonContext::CreateToolButton(
    api_handle m, api_handle c,
    const char16_type* text,
    const_bitmap_handle icon,
    api_bool checkable,
    control_handle parent,
    uint32 flags)
{
    control_handle h = createControl<QToolButton>(m, c, parent);
    
    QToolButton* btn = qobject_cast<QToolButton*>(widgetFromHandle(h));
    if (!btn) return h;
    
    if (text)
        btn->setText(QString::fromUtf16(text));
    
    btn->setCheckable(checkable);
    
    logf("[Mock] CreateToolButton handle=%p checkable=%d", h, checkable);
    return h;
}

api_bool ButtonContext::GetButtonText(const_control_handle h,
                                  char16_type* text,
                                  size_type* len)
{
    QAbstractButton* btn = qobject_cast<QAbstractButton*>(widgetFromHandle(h));
    if (!btn) return api_false;
    
    QString qtext = btn->text();
    
    if (len) *len = qtext.length();
    
    if (text && len && *len > 0) {
        const char16_type* src = reinterpret_cast<const char16_type*>(qtext.utf16());
        size_t copyLen = std::min(*len, static_cast<size_type>(qtext.length()));
        std::memcpy(text, src, copyLen * sizeof(char16_t));
        if (copyLen < *len) text[copyLen] = 0;
    }
    
    return api_true;
}

void ButtonContext::SetButtonText(control_handle h, const char16_type* text)
{
    QAbstractButton* btn = qobject_cast<QAbstractButton*>(widgetFromHandle(h));
    if (btn && text) {
        QString qtext = QString::fromUtf16(text);
        btn->setText(qtext);
        logf("[Mock] SetButtonText: %s", qtext.toUtf8().constData());
    }
}

bitmap_handle ButtonContext::GetButtonIcon(const_control_handle h)
{
    QAbstractButton* btn = qobject_cast<QAbstractButton*>(widgetFromHandle(h));
    if (!btn) return nullptr;
    
    QIcon icon = btn->icon();
    if (!icon.isNull()) {
        return reinterpret_cast<bitmap_handle>(new QIcon(icon));
    }
    
    return nullptr;
}

void ButtonContext::SetButtonIcon(control_handle h, const_bitmap_handle icon)
{
    QAbstractButton* btn = qobject_cast<QAbstractButton*>(widgetFromHandle(h));
    if (!btn) return;
    
    if (icon) {
        QIcon* qicon = reinterpret_cast<QIcon*>(const_cast<void*>(icon));
        btn->setIcon(*qicon);
    } else {
        btn->setIcon(QIcon());
    }
    
    logf("[Mock] SetButtonIcon");
}

void ButtonContext::GetButtonIconSize(const_control_handle h, int32* w, int32* h_out)
{
    QAbstractButton* btn = qobject_cast<QAbstractButton*>(widgetFromHandle(h));
    if (!btn) return;
    
    QSize size = btn->iconSize();
    if (w) *w = size.width();
    if (h_out) *h_out = size.height();
}

void ButtonContext::SetButtonIconSize(control_handle h, int32 w, int32 h_size)
{
    QAbstractButton* btn = qobject_cast<QAbstractButton*>(widgetFromHandle(h));
    if (btn) {
        btn->setIconSize(QSize(w, h_size));
        logf("[Mock] SetButtonIconSize: %d x %d", w, h_size);
    }
}

api_bool ButtonContext::GetButtonPushed(const_control_handle h)
{
    QAbstractButton* btn = qobject_cast<QAbstractButton*>(widgetFromHandle(h));
    return (btn && btn->isDown()) ? api_true : api_false;
}

void ButtonContext::SetButtonPushed(control_handle h, api_bool pushed)
{
    QAbstractButton* btn = qobject_cast<QAbstractButton*>(widgetFromHandle(h));
    if (btn) {
        btn->setDown(pushed);
        logf("[Mock] SetButtonPushed: %d", pushed);
    }
}

uint32 ButtonContext::GetButtonChecked(const_control_handle h)
{
    // Try QCheckBox first (supports tristate)
    if (QCheckBox* checkbox = qobject_cast<QCheckBox*>(widgetFromHandle(h))) {
        Qt::CheckState state = checkbox->checkState();
        return static_cast<uint32>(state); // 0=unchecked, 1=partial, 2=checked
    }
    
    // Fallback to generic QAbstractButton (binary checked state)
    if (QAbstractButton* btn = qobject_cast<QAbstractButton*>(widgetFromHandle(h))) {
        if (btn->isCheckable()) {
            return btn->isChecked() ? 2 : 0;
        }
    }
    
    return 0;
}

void ButtonContext::SetButtonChecked(control_handle h, uint32 state)
{
    // Try QCheckBox first (supports tristate)
    if (QCheckBox* checkbox = qobject_cast<QCheckBox*>(widgetFromHandle(h))) {
        checkbox->setCheckState(static_cast<Qt::CheckState>(state));
        logf("[Mock] SetButtonChecked (QCheckBox): %u", state);
        return;
    }
    
    // Fallback to generic QAbstractButton (binary checked state)
    if (QAbstractButton* btn = qobject_cast<QAbstractButton*>(widgetFromHandle(h))) {
        btn->setChecked(state != 0);
        logf("[Mock] SetButtonChecked (QAbstractButton): %u", state);
    }
}

api_bool ButtonContext::GetButtonDefaultEnabled(const_control_handle h)
{
    QPushButton* btn = qobject_cast<QPushButton*>(widgetFromHandle(h));
    return (btn && btn->isDefault()) ? api_true : api_false;
}

void ButtonContext::SetButtonDefaultEnabled(control_handle h, api_bool enabled)
{
    QPushButton* btn = qobject_cast<QPushButton*>(widgetFromHandle(h));
    if (btn) {
        btn->setDefault(enabled);
        logf("[Mock] SetButtonDefaultEnabled: %d", enabled);
    }
}

api_bool ButtonContext::GetButtonTristateEnabled(const_control_handle h)
{
    QCheckBox* btn = qobject_cast<QCheckBox*>(widgetFromHandle(h));
    return (btn && btn->isTristate()) ? api_true : api_false;
}

void ButtonContext::SetButtonTristateEnabled(control_handle h, api_bool enabled)
{
    QCheckBox* btn = qobject_cast<QCheckBox*>(widgetFromHandle(h));
    if (btn) {
        btn->setTristate(enabled);
        logf("[Mock] SetButtonTristateEnabled: %d", enabled);
    }
}

api_bool ButtonContext::GetToolButtonCheckable(const_control_handle h)
{
    QToolButton* btn = qobject_cast<QToolButton*>(widgetFromHandle(h));
    return (btn && btn->isCheckable()) ? api_true : api_false;
}

void ButtonContext::SetToolButtonCheckable(control_handle h, api_bool checkable)
{
    QToolButton* btn = qobject_cast<QToolButton*>(widgetFromHandle(h));
    if (btn) {
        btn->setCheckable(checkable);
        logf("[Mock] SetToolButtonCheckable: %d", checkable);
    }
}

api_bool ButtonContext::SetButtonPressEventRoutine(control_handle h,
                                               api_handle receiver,
                                               pcl::event_routine routine)
{
    logf("[Mock] SetButtonPressEventRoutine");
    return api_true;
}

api_bool ButtonContext::SetButtonReleaseEventRoutine(control_handle h,
                                                 api_handle receiver, 
                                                 pcl::event_routine routine)
{
    logf("[Mock] SetButtonReleaseEventRoutine");
    return api_true;
}

api_bool ButtonContext::SetButtonClickEventRoutine(
    control_handle h,
    api_handle receiver,
    pcl::button_click_event_routine r)
{
    MockBase* b = get(h);
    if (!b) return api_false;
    
    b->eventReceiver = reinterpret_cast<control_handle>(receiver);
    b->onButtonClick = r;
    
    if (QAbstractButton* btn = qobject_cast<QAbstractButton*>(widgetFromHandle(h))) {
        QObject::connect(btn, &QAbstractButton::clicked, [b](bool checked){
            if (b->onButtonClick && b->pcl_handle) {
                b->onButtonClick(
                    b->pcl_handle,
                    b->eventReceiver,
                    checked ? api_true : api_false);
            }
        });
    }
    
    return api_true;
}

api_bool ButtonContext::SetButtonCheckEventRoutine(
    control_handle h,
    api_handle /*client*/,
    pcl::button_check_event_routine r)
{
    MockBase* b = get(h);
    if (!b) return api_false;
    
    b->onButtonCheck = r;
    
    if (QCheckBox* cb = qobject_cast<QCheckBox*>(widgetFromHandle(h))) {
        QObject::connect(cb, &QCheckBox::stateChanged, [b](int st){
            if (b->onButtonCheck && b->pcl_handle) {
                b->onButtonCheck(
                    b->pcl_handle,
                    b->pcl_handle,
                    st);
            }
        });
    }
    
    return api_true;
}

// =============================================================
// ComboBox Context
// =============================================================

control_handle ComboBoxContext::CreateComboBox(
    api_handle m, api_handle c, control_handle parent, uint32 flags)
{
    return createControl<QComboBox>(m, c, parent);
}

void ComboBoxContext::SetComboBoxEditEnabled(control_handle h, api_bool editable)
{
    QComboBox* combo = qobject_cast<QComboBox*>(widgetFromHandle(h));
    if (combo)
        combo->setEditable(editable);
}

int32 ComboBoxContext::GetComboBoxLength(const_control_handle h)
{
    QComboBox* combo = qobject_cast<QComboBox*>(widgetFromHandle(h));
    return combo ? combo->count() : 0;
}

void ComboBoxContext::InsertComboBoxItem(control_handle h, int32 idx, const char16_type* text, const_bitmap_handle icon)
{
    QComboBox* combo = qobject_cast<QComboBox*>(widgetFromHandle(h));
    if (!combo || !text) return;
    
    QString qtext = QString::fromUtf16(text);
    
    if (idx < 0 || idx >= combo->count())
        combo->addItem(qtext);
    else
        combo->insertItem(idx, qtext);
    
    logf("[Mock] InsertComboBoxItem: %s", qtext.toUtf8().constData());
}

void ComboBoxContext::SetComboBoxCurrentItem(control_handle h, int32 idx)
{
    QComboBox* combo = qobject_cast<QComboBox*>(widgetFromHandle(h));
    if (combo)
        combo->setCurrentIndex(idx);
}

api_bool ComboBoxContext::SetComboBoxItemSelectedEventRoutine(control_handle, api_handle, pcl::value_event_routine)
{
    return api_true;
}

// =============================================================
// ScrollBox Context
// =============================================================

control_handle ScrollBoxContext::CreateScrollBox(api_handle module,
                                              api_handle client,
                                              control_handle parent,
                                              uint32 /*flags*/)
{
    control_handle h = createControl<QScrollArea>(module, client, parent);
    
    QScrollArea* scrollArea = qobject_cast<QScrollArea*>(widgetFromHandle(h));
    if (scrollArea) {
        scrollArea->setWidgetResizable(true);
    }
    
    logf("[Mock] CreateScrollBox handle=%p", h);
    return h;
}

control_handle ScrollBoxContext::CreateScrollBoxViewport(control_handle scrollBox,
                                                      api_handle client)
{
    QScrollArea* scrollArea = qobject_cast<QScrollArea*>(widgetFromHandle(scrollBox));
    if (!scrollArea) return nullptr;
    
    MockBase* sb = get(scrollBox);
    if (!sb) return nullptr;
    
    // Create a viewport widget
    auto* b = new MockBase();
    b->moduleHandle = sb->moduleHandle;
    b->isSizer = false;
    b->isTreeNode = false;
    b->widget = new QWidget(scrollArea);
    b->layout = nullptr;
    b->treeItem = nullptr;
    
    // Register using client as key
    control_handle viewportHandle = reinterpret_cast<control_handle>(client);
    g_objects[viewportHandle] = std::unique_ptr<MockBase>(b);
    
    // Attach to scroll area
    scrollArea->setWidget(b->widget);
    
    logf("[Mock] CreateScrollBoxViewport handle=%p parentScroll=%p widget=%p",
          viewportHandle, scrollBox, b->widget);
    
    return viewportHandle;
}

// =============================================================
// TreeBox Context - CORRECTED
// =============================================================

control_handle TreeBoxContext::CreateTreeBox(api_handle module,
                                         api_handle client,
                                         control_handle parent,
                                         uint32 flags)
{
    control_handle h = createControl<QTreeWidget>(module, client, parent);
    
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(widgetFromHandle(h));
    if (tree) {
        tree->setColumnCount(1);
        tree->setHeaderHidden(false);
    }
    
    logf("[Mock] CreateTreeBox handle=%p", h);
    return h;
}

control_handle TreeBoxContext::CreateTreeBoxViewport(control_handle h,
                                                 api_handle client)
{
    QTreeWidget* tree = treeFromHandle(h);
    if (!tree) return nullptr;
    
    QWidget* viewport = tree->viewport();
    
    logf("[Mock] CreateTreeBoxViewport");
    return reinterpret_cast<control_handle>(viewport);
}

api_handle TreeBoxContext::CreateTreeBoxNode(api_handle module,
                                         api_handle nodeClient)
{
    // Create mock wrapper
    auto* mock = new MockBase();
    mock->moduleHandle = module;
    mock->isSizer = false;
    mock->isTreeNode = true;  // Mark as tree node
    mock->widget = nullptr;
    mock->layout = nullptr;
    mock->treeItem = new QTreeWidgetItem();
    mock->treeItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    
    // CRITICAL: Use CLIENT as key (consistent with controls)
    api_handle handle = nodeClient;
    g_objects[handle] = std::unique_ptr<MockBase>(mock);
    
    logf("[Mock] CreateTreeBoxNode client=%p item=%p", handle, mock->treeItem);
    
    return handle;
}

void TreeBoxContext::ClearTreeBox(control_handle h)
{
    QTreeWidget* tree = treeFromHandle(h);
    if (tree) {
        tree->clear();
        logf("[Mock] ClearTreeBox");
    }
}

int32 TreeBoxContext::GetTreeBoxChildCount(const_control_handle h)
{
    QTreeWidget* tree = treeFromHandle(h);
    return tree ? tree->topLevelItemCount() : 0;
}

int32 TreeBoxContext::GetTreeBoxChildIndex(const_control_handle h,
                                       const_api_handle node)
{
    QTreeWidget* tree = treeFromHandle(h);
    if (!tree || !node) return -1;
    
    QTreeWidgetItem* item = treeItemFromHandle(node);
    if (!item) return -1;
    
    return tree->indexOfTopLevelItem(item);
}

api_handle TreeBoxContext::GetTreeBoxChild(const_control_handle h, int32 idx)
{
    QTreeWidget* tree = treeFromHandle(h);
    if (!tree) return nullptr;
    
    QTreeWidgetItem* item = tree->topLevelItem(idx);
    if (!item) return nullptr;
    
    for (auto& kv : g_objects) {
        MockBase* obj = kv.second.get();
        if (obj->isTreeNode && obj->treeItem == item) {
            return const_cast<api_handle>(kv.first);
        }
    }
    
    return nullptr;
}

void TreeBoxContext::InsertTreeBoxNode(control_handle h,
                                   int32 idx,
                                   api_handle node)
{
    QTreeWidget* tree = treeFromHandle(h);
    if (!tree || !node) return;
    
    QTreeWidgetItem* item = treeItemFromHandle(node);
    if (!item) return;
    
    if (idx < 0 || idx >= tree->topLevelItemCount()) {
        tree->addTopLevelItem(item);
    } else {
        tree->insertTopLevelItem(idx, item);
    }
    
    logf("[Mock] InsertTreeBoxNode idx=%d", idx);
}

void TreeBoxContext::RemoveTreeBoxNode(control_handle h, int32 idx)
{
    QTreeWidget* tree = treeFromHandle(h);
    if (!tree) return;
    
    QTreeWidgetItem* item = tree->takeTopLevelItem(idx);
    if (item) {
        logf("[Mock] RemoveTreeBoxNode idx=%d", idx);
    }
}

api_handle TreeBoxContext::GetTreeBoxCurrentNode(const_control_handle h)
{
    QTreeWidget* tree = treeFromHandle(h);
    if (!tree) return nullptr;
    
    QTreeWidgetItem* item = tree->currentItem();
    if (!item) return nullptr;
    
    // Find the handle for this item
    for (auto& kv : g_objects) {
        MockBase* obj = kv.second.get();
        if (obj->isTreeNode && obj->treeItem == item) {
            return const_cast<void *>(kv.first);
        }
    }
    
    return nullptr;
}

void TreeBoxContext::SetTreeBoxCurrentNode(control_handle h, api_handle node)
{
    QTreeWidget* tree = treeFromHandle(h);
    if (!tree) return;
    
    if (!node) {
        tree->setCurrentItem(nullptr);
        return;
    }
    
    QTreeWidgetItem* item = treeItemFromHandle(node);
    if (item) {
        tree->setCurrentItem(item);
        logf("[Mock] SetTreeBoxCurrentNode");
    }
}

void TreeBoxContext::SelectAllTreeBoxNodes(control_handle h)
{
    QTreeWidget* tree = treeFromHandle(h);
    if (tree) {
        tree->selectAll();
        logf("[Mock] SelectAllTreeBoxNodes");
    }
}

void TreeBoxContext::SetTreeBoxMultipleNodeSelectionEnabled(control_handle h,
                                                        api_bool enabled)
{
    QTreeWidget* tree = treeFromHandle(h);
    if (tree) {
        tree->setSelectionMode(enabled ?
            QAbstractItemView::ExtendedSelection :
            QAbstractItemView::SingleSelection);
        logf("[Mock] SetTreeBoxMultipleNodeSelectionEnabled: %d", enabled);
    }
}

void TreeBoxContext::SetTreeBoxColumnCount(control_handle h, int32 count)
{
    QTreeWidget* tree = treeFromHandle(h);
    if (tree) {
        tree->setColumnCount(count);
        logf("[Mock] SetTreeBoxColumnCount: %d", count);
    }
}

void TreeBoxContext::AdjustTreeBoxColumnWidthToContents(control_handle h, int32 col)
{
    QTreeWidget* tree = treeFromHandle(h);
    if (tree) {
        tree->resizeColumnToContents(col);
        logf("[Mock] AdjustTreeBoxColumnWidthToContents col=%d", col);
    }
}

void TreeBoxContext::SetTreeBoxHeaderVisible(control_handle h, api_bool visible)
{
    QTreeWidget* tree = treeFromHandle(h);
    if (tree) {
        tree->setHeaderHidden(!visible);
        logf("[Mock] SetTreeBoxHeaderVisible: %d", visible);
    }
}

void TreeBoxContext::SetTreeBoxRootDecorationEnabled(control_handle h,
                                                 api_bool enabled)
{
    QTreeWidget* tree = treeFromHandle(h);
    if (tree) {
        tree->setRootIsDecorated(enabled);
        logf("[Mock] SetTreeBoxRootDecorationEnabled: %d", enabled);
    }
}

void TreeBoxContext::SetTreeBoxAlternateRowColorEnabled(control_handle h,
                                                    api_bool enabled)
{
    QTreeWidget* tree = treeFromHandle(h);
    if (tree) {
        tree->setAlternatingRowColors(enabled);
        logf("[Mock] SetTreeBoxAlternateRowColorEnabled: %d", enabled);
    }
}

// TreeBox Node Functions
control_handle TreeBoxContext::GetTreeBoxNodeParentBox(const_api_handle node)
{
    if (!node) return nullptr;
    
    QTreeWidgetItem* item = treeItemFromHandle(node);
    if (!item) return nullptr;
    
    QTreeWidget* tree = item->treeWidget();
    if (!tree) return nullptr;
    
    // Find the control handle for this tree widget
    for (auto& kv : g_objects) {
        MockBase* obj = kv.second.get();
        if (obj->widget == tree) {
            return reinterpret_cast<control_handle>(const_cast<void*>(kv.first));  // FIX
        }
    }
    
    return nullptr;
}

api_handle TreeBoxContext::GetTreeBoxNodeParent(const_api_handle node)
{
    if (!node) return nullptr;
    
    QTreeWidgetItem* item = treeItemFromHandle(node);
    if (!item) return nullptr;
    
    QTreeWidgetItem* parent = item->parent();
    if (!parent) return nullptr;
    
    // Find the handle for the parent item
    for (auto& kv : g_objects) {
        MockBase* obj = kv.second.get();
        if (obj->isTreeNode && obj->treeItem == parent) {
            return const_cast<api_handle>(kv.first);
        }
    }
    
    return nullptr;
}

int32 TreeBoxContext::GetTreeBoxNodeChildCount(const_api_handle node)
{
    QTreeWidgetItem* item = treeItemFromHandle(node);
    return item ? item->childCount() : 0;
}

api_handle TreeBoxContext::GetTreeBoxNodeChild(const_api_handle node, int32 idx)
{
    if (!node) return nullptr;
    
    QTreeWidgetItem* item = treeItemFromHandle(node);
    if (!item) return nullptr;
    
    QTreeWidgetItem* child = item->child(idx);
    if (!child) return nullptr;
    
    // Find the handle for the child item
    for (auto& kv : g_objects) {
        MockBase* obj = kv.second.get();
        if (obj->isTreeNode && obj->treeItem == child) {
            return const_cast<api_handle>(kv.first);
        }
    }
    
    return nullptr;
}

void TreeBoxContext::InsertTreeBoxNodeChild(api_handle parentNode,
                                        int32 idx,
                                        api_handle childNode)
{
    if (!parentNode || !childNode) return;
    
    QTreeWidgetItem* parent = treeItemFromHandle(parentNode);
    QTreeWidgetItem* child = treeItemFromHandle(childNode);
    
    if (!parent || !child) return;
    
    if (idx < 0 || idx >= parent->childCount()) {
        parent->addChild(child);
    } else {
        parent->insertChild(idx, child);
    }
    
    logf("[Mock] InsertTreeBoxNodeChild idx=%d", idx);
}

void TreeBoxContext::RemoveTreeBoxNodeChild(api_handle parentNode, int32 idx)
{
    if (!parentNode) return;
    
    QTreeWidgetItem* parent = treeItemFromHandle(parentNode);
    if (!parent) return;
    
    QTreeWidgetItem* child = parent->takeChild(idx);
    if (child) {
        logf("[Mock] RemoveTreeBoxNodeChild idx=%d", idx);
    }
}

// TreeBox Node State Functions

api_bool TreeBoxContext::GetTreeBoxNodeEnabled(const_api_handle node)
{
    QTreeWidgetItem* item = treeItemFromHandle(node);
    return (item && (item->flags() & Qt::ItemIsEnabled)) ? api_true : api_false;
}

void TreeBoxContext::SetTreeBoxNodeEnabled(api_handle node, api_bool enabled)
{
    QTreeWidgetItem* item = treeItemFromHandle(node);
    if (!item) return;
    
    Qt::ItemFlags flags = item->flags();
    
    if (enabled)
        flags |= Qt::ItemIsEnabled;
    else
        flags &= ~Qt::ItemIsEnabled;
    
    item->setFlags(flags);
    logf("[Mock] SetTreeBoxNodeEnabled: %d", enabled);
}

api_bool TreeBoxContext::GetTreeBoxNodeExpanded(const_api_handle node)
{
    QTreeWidgetItem* item = treeItemFromHandle(node);
    return (item && item->isExpanded()) ? api_true : api_false;
}

void TreeBoxContext::SetTreeBoxNodeExpanded(api_handle node, api_bool expanded)
{
    QTreeWidgetItem* item = treeItemFromHandle(node);
    if (item) {
        item->setExpanded(expanded);
        logf("[Mock] SetTreeBoxNodeExpanded: %d", expanded);
    }
}

api_bool TreeBoxContext::GetTreeBoxNodeSelectable(const_api_handle node)
{
    QTreeWidgetItem* item = treeItemFromHandle(node);
    return (item && (item->flags() & Qt::ItemIsSelectable)) ? api_true : api_false;
}

void TreeBoxContext::SetTreeBoxNodeSelectable(api_handle node, api_bool selectable)
{
    QTreeWidgetItem* item = treeItemFromHandle(node);
    if (!item) return;
    
    Qt::ItemFlags flags = item->flags();
    
    if (selectable)
        flags |= Qt::ItemIsSelectable;
    else
        flags &= ~Qt::ItemIsSelectable;
    
    item->setFlags(flags);
    logf("[Mock] SetTreeBoxNodeSelectable: %d", selectable);
}

api_bool TreeBoxContext::GetTreeBoxNodeSelected(const_api_handle node)
{
    QTreeWidgetItem* item = treeItemFromHandle(node);
    return (item && item->isSelected()) ? api_true : api_false;
}

void TreeBoxContext::SetTreeBoxNodeSelected(api_handle node, api_bool selected)
{
    QTreeWidgetItem* item = treeItemFromHandle(node);
    if (item) {
        item->setSelected(selected);
        logf("[Mock] SetTreeBoxNodeSelected: %d", selected);
    }
}

api_bool TreeBoxContext::GetTreeBoxNodeCheckable(const_api_handle node)
{
    QTreeWidgetItem* item = treeItemFromHandle(node);
    return (item && (item->flags() & Qt::ItemIsUserCheckable)) ? api_true : api_false;
}

void TreeBoxContext::SetTreeBoxNodeCheckable(api_handle node, api_bool checkable)
{
    QTreeWidgetItem* item = treeItemFromHandle(node);
    if (!item) return;
    
    Qt::ItemFlags flags = item->flags();
    
    if (checkable) {
        flags |= Qt::ItemIsUserCheckable;
        item->setCheckState(0, Qt::Unchecked);
    } else {
        flags &= ~Qt::ItemIsUserCheckable;
    }
    
    item->setFlags(flags);
    logf("[Mock] SetTreeBoxNodeCheckable: %d", checkable);
}

api_bool TreeBoxContext::GetTreeBoxNodeChecked(const_api_handle node)
{
    QTreeWidgetItem* item = treeItemFromHandle(node);
    return (item && item->checkState(0) == Qt::Checked) ? api_true : api_false;
}

void TreeBoxContext::SetTreeBoxNodeChecked(api_handle node, api_bool checked)
{
    QTreeWidgetItem* item = treeItemFromHandle(node);
    if (item) {
        item->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
        logf("[Mock] SetTreeBoxNodeChecked: %d", checked);
    }
}

api_bool TreeBoxContext::GetTreeBoxNodeEditable(const_api_handle node)
{
    QTreeWidgetItem* item = treeItemFromHandle(node);
    return (item && (item->flags() & Qt::ItemIsEditable)) ? api_true : api_false;
}

void TreeBoxContext::SetTreeBoxNodeEditable(api_handle node, api_bool editable)
{
    QTreeWidgetItem* item = treeItemFromHandle(node);
    if (!item) return;
    
    Qt::ItemFlags flags = item->flags();
    
    if (editable)
        flags |= Qt::ItemIsEditable;
    else
        flags &= ~Qt::ItemIsEditable;
    
    item->setFlags(flags);
    logf("[Mock] SetTreeBoxNodeEditable: %d", editable);
}

api_bool TreeBoxContext::GetTreeBoxNodeFirstColumnSpanned(const_api_handle node)
{
    QTreeWidgetItem* item = treeItemFromHandle(node);
    if (!item) return api_false;
    
    return item->isFirstColumnSpanned() ? api_true : api_false;
}

void TreeBoxContext::SetTreeBoxNodeFirstColumnSpanned(api_handle node, api_bool spanned)
{
    QTreeWidgetItem* item = treeItemFromHandle(node);
    if (!item) return;
    
    item->setFirstColumnSpanned(spanned);
    logf("[Mock] SetTreeBoxNodeFirstColumnSpanned: %d", spanned);
}

// TreeBox Node Column Content Functions

api_bool TreeBoxContext::GetTreeBoxNodeColText(const_api_handle node,
                                           int32 col,
                                           char16_type* text,
                                           size_type* len)
{
    QTreeWidgetItem* item = treeItemFromHandle(node);
    if (!item) return api_false;
    
    QString qtext = item->text(col);
    
    if (len) *len = qtext.length();
    
    if (text && len && *len > 0) {
        const char16_type* src = reinterpret_cast<const char16_type*>(qtext.utf16());
        size_t copyLen = std::min(*len, static_cast<size_type>(qtext.length()));
        std::memcpy(text, src, copyLen * sizeof(char16_t));
        if (copyLen < *len) text[copyLen] = 0;
    }
    
    return api_true;
}

void TreeBoxContext::SetTreeBoxNodeColText(api_handle node,
                                       int32 col,
                                       const char16_type* text)
{
    if (!node || !text) return;
    
    QTreeWidgetItem* item = treeItemFromHandle(node);
    if (!item) return;
    
    QString qtext = QString::fromUtf16(text);
    item->setText(col, qtext);
    logf("[Mock] SetTreeBoxNodeColText col=%d: %s", col, qtext.toUtf8().constData());
}

bitmap_handle TreeBoxContext::GetTreeBoxNodeColIcon(const_api_handle node, int32 col)
{
    QTreeWidgetItem* item = treeItemFromHandle(node);
    if (!item) return nullptr;
    
    QIcon icon = item->icon(col);
    
    if (!icon.isNull()) {
        QPixmap pixmap = icon.pixmap(32, 32);
        return reinterpret_cast<bitmap_handle>(new QPixmap(pixmap));
    }
    
    return nullptr;
}

void TreeBoxContext::SetTreeBoxNodeColIcon(api_handle node,
                                       int32 col,
                                       const_bitmap_handle icon)
{
    if (!node) return;
    
    QTreeWidgetItem* item = treeItemFromHandle(node);
    if (!item) return;
    
    if (icon) {
        const QPixmap* pixmap = reinterpret_cast<const QPixmap*>(icon);
        item->setIcon(col, QIcon(*pixmap));
    } else {
        item->setIcon(col, QIcon());
    }
    
    logf("[Mock] SetTreeBoxNodeColIcon col=%d", col);
}

int32 TreeBoxContext::GetTreeBoxNodeColAlignment(const_api_handle node, int32 col)
{
    QTreeWidgetItem* item = treeItemFromHandle(node);
    return item ? static_cast<int32>(item->textAlignment(col)) : 0;
}

void TreeBoxContext::SetTreeBoxNodeColAlignment(api_handle node,
                                            int32 col,
                                            int32 alignment)
{
    QTreeWidgetItem* item = treeItemFromHandle(node);
    if (item) {
        item->setTextAlignment(col, alignment);
        logf("[Mock] SetTreeBoxNodeColAlignment col=%d align=%d", col, alignment);
    }
}

api_bool TreeBoxContext::GetTreeBoxNodeColToolTip(const_api_handle node,
                                              int32 col,
                                              char16_type* text,
                                              size_type* len)
{
    QTreeWidgetItem* item = treeItemFromHandle(node);
    if (!item) return api_false;
    
    QString qtext = item->toolTip(col);
    
    if (len) *len = qtext.length();
    
    if (text && len && *len > 0) {
        const char16_type* src = reinterpret_cast<const char16_type*>(qtext.utf16());
        size_t copyLen = std::min(*len, static_cast<size_type>(qtext.length()));
        std::memcpy(text, src, copyLen * sizeof(char16_t));
        if (copyLen < *len) text[copyLen] = 0;
    }
    
    return api_true;
}

void TreeBoxContext::SetTreeBoxNodeColToolTip(api_handle node,
                                          int32 col,
                                          const char16_type* text)
{
    QTreeWidgetItem* item = treeItemFromHandle(node);
    if (!item) return;
    
    QString qtext = text ? QString::fromUtf16(text) : QString();
    item->setToolTip(col, qtext);
    logf("[Mock] SetTreeBoxNodeColToolTip col=%d", col);
}

font_handle TreeBoxContext::GetTreeBoxNodeColFont(const_api_handle node, int32 col)
{
    QTreeWidgetItem* item = treeItemFromHandle(node);
    if (!item) return nullptr;
    
    QFont font = item->font(col);
    return reinterpret_cast<font_handle>(new QFont(font));
}

void TreeBoxContext::SetTreeBoxNodeColFont(api_handle node,
                                       int32 col,
                                       const_font_handle font)
{
    QTreeWidgetItem* item = treeItemFromHandle(node);
    if (!item) return;
    
    if (font) {
        const QFont* qfont = reinterpret_cast<const QFont*>(font);
        item->setFont(col, *qfont);
    }
    
    logf("[Mock] SetTreeBoxNodeColFont col=%d", col);
}

uint32 TreeBoxContext::GetTreeBoxNodeColBackgroundColor(const_api_handle node, int32 col)
{
    QTreeWidgetItem* item = treeItemFromHandle(node);
    if (!item) return 0;
    
    QBrush brush = item->background(col);
    return brush.color().rgba();
}

void TreeBoxContext::SetTreeBoxNodeColBackgroundColor(api_handle node,
                                                  int32 col,
                                                  uint32 rgba)
{
    QTreeWidgetItem* item = treeItemFromHandle(node);
    if (!item) return;
    
    item->setBackground(col, QBrush(QColor(rgba)));
    logf("[Mock] SetTreeBoxNodeColBackgroundColor col=%d rgba=0x%08x", col, rgba);
}

uint32 TreeBoxContext::GetTreeBoxNodeColTextColor(const_api_handle node, int32 col)
{
    QTreeWidgetItem* item = treeItemFromHandle(node);
    if (!item) return 0;
    
    QBrush brush = item->foreground(col);
    return brush.color().rgba();
}

void TreeBoxContext::SetTreeBoxNodeColTextColor(api_handle node,
                                            int32 col,
                                            uint32 rgba)
{
    QTreeWidgetItem* item = treeItemFromHandle(node);
    if (!item) return;
    
    item->setForeground(col, QBrush(QColor(rgba)));
    logf("[Mock] SetTreeBoxNodeColTextColor col=%d rgba=0x%08x", col, rgba);
}

// TreeBox Event Handlers

api_bool TreeBoxContext::SetTreeBoxNodeActivatedEventRoutine(
    control_handle h,
    api_handle /*client*/,
    pcl::item_value_event_routine r)
{
    MockBase* b = get(h);
    if (!b) return api_false;
    
    b->onTreeNodeActivated = r;
    
    if (QTreeWidget* tree = treeFromHandle(h)) {
        QObject::connect(tree, &QTreeWidget::itemActivated,
                         [b](QTreeWidgetItem* item, int col){
            if (b->onTreeNodeActivated && b->pcl_handle) {
                // Find the node handle for this item
                for (auto& kv : g_objects) {
                    MockBase* obj = kv.second.get();
                    if (obj->isTreeNode && obj->treeItem == item) {
                        b->onTreeNodeActivated(
                            b->pcl_handle,
                            b->pcl_handle,
                            const_cast<api_handle>(kv.first),
                            col);
                        break;
                    }
                }
            }
        });
    }
    
    return api_true;
}

api_bool TreeBoxContext::SetTreeBoxCurrentNodeUpdatedEventRoutine(
    control_handle h,
    api_handle /*client*/,
    pcl::item_range_event_routine r)
{
    MockBase* b = get(h);
    if (!b) return api_false;
    
    b->onTreeNodeUpdated = r;
    
    if (QTreeWidget* tree = treeFromHandle(h)) {
        QObject::connect(tree, &QTreeWidget::currentItemChanged,
                         [b](QTreeWidgetItem* item, QTreeWidgetItem*){
            if (b->onTreeNodeUpdated && b->pcl_handle && item) {
                // Find the node handle for this item
                for (auto& kv : g_objects) {
                    MockBase* obj = kv.second.get();
                    if (obj->isTreeNode && obj->treeItem == item) {
                        b->onTreeNodeUpdated(
                            b->pcl_handle,
                            b->pcl_handle,
                            b->pcl_handle,
                            const_cast<api_handle>(kv.first));
                        break;
                    }
                }
            }
        });
    }
    
    return api_true;
}

api_bool TreeBoxContext::SetTreeBoxNodeSelectionUpdatedEventRoutine(
    control_handle h,
    api_handle /*client*/,
    pcl::event_routine r)
{
    MockBase* b = get(h);
    if (!b) return api_false;
    
    b->onTreeSelectionUpdated = r;
    
    if (QTreeWidget* tree = treeFromHandle(h)) {
        QObject::connect(tree, &QTreeWidget::itemSelectionChanged,
                         [b, tree](){
            if (b->onTreeSelectionUpdated && b->pcl_handle) {
                auto items = tree->selectedItems();
                QTreeWidgetItem* first = items.isEmpty() ? nullptr : items.first();
                
                control_handle firstHandle = nullptr;
                if (first) {
                    for (auto& kv : g_objects) {
                        MockBase* obj = kv.second.get();
                        if (obj->isTreeNode && obj->treeItem == first) {
                            firstHandle = reinterpret_cast<control_handle>(const_cast<void*>(kv.first));  // FIX: reinterpret_cast
                            break;
                        }
                    }
                }
                
                b->onTreeSelectionUpdated(b->pcl_handle, firstHandle);
            }
        });
    }
    
    return api_true;
}

typedef meta_format_handle const_meta_format_handle;
  
api_bool TreeBoxContext::SetTreeBoxNodeEnteredEventRoutine(
    control_handle h,
    api_handle,
    pcl::item_value_event_routine routine)
{
    logf("[Mock] SetTreeBoxNodeEnteredEventRoutine");
    return api_true;
}

api_bool TreeBoxContext::SetTreeBoxNodeClickedEventRoutine(
    control_handle h,
    api_handle,
    pcl::item_value_event_routine routine)
{
    logf("[Mock] SetTreeBoxNodeClickedEventRoutine");
    return api_true;
}

api_bool TreeBoxContext::SetTreeBoxNodeDoubleClickedEventRoutine(
    control_handle h,
    api_handle,
    pcl::item_value_event_routine routine)
{
    logf("[Mock] SetTreeBoxNodeDoubleClickedEventRoutine");
    return api_true;
}

api_bool TreeBoxContext::SetTreeBoxNodeExpandedEventRoutine(
    control_handle h,
    api_handle,
    pcl::item_event_routine routine)
{
    logf("[Mock] SetTreeBoxNodeExpandedEventRoutine");
    return api_true;
}

api_bool TreeBoxContext::SetTreeBoxNodeCollapsedEventRoutine(
    control_handle h,
    api_handle,
    pcl::item_event_routine routine)
{
    logf("[Mock] SetTreeBoxNodeCollapsedEventRoutine");
    return api_true;
}

// =============================================================
// Continue with remaining contexts...
// (BitmapBox, Bitmap, UI, Font, Global, etc.)
// These can be added as needed based on what your module uses
// =============================================================
// =============================================================
// BitmapBox Context
// =============================================================

control_handle BitmapBoxContext::CreateBitmapBox(api_handle module,
                                             api_handle client,
                                             const_bitmap_handle bitmap,
                                             control_handle parent,
                                             uint32 flags)
{
    control_handle h = createControl<QLabel>(module, client, parent);
    
    QLabel* label = qobject_cast<QLabel*>(widgetFromHandle(h));
    if (label && bitmap) {
        QPixmap* pixmap = reinterpret_cast<QPixmap*>(const_cast<void*>(bitmap));
        label->setPixmap(*pixmap);
        label->setScaledContents(false);
    }
    
    logf("[Mock] CreateBitmapBox handle=%p", h);
    return h;
}

bitmap_handle BitmapBoxContext::GetBitmapBoxBitmap(const_control_handle h)
{
    QLabel* label = qobject_cast<QLabel*>(widgetFromHandle(h));
    if (!label) return nullptr;
    
    QPixmap pm = label->pixmap(Qt::ReturnByValue);  // FIX: Use new Qt API
    if (pm.isNull())
        return nullptr;
    
    return reinterpret_cast<bitmap_handle>(new QPixmap(pm));
}

void BitmapBoxContext::SetBitmapBoxBitmap(control_handle h,
                                      const_bitmap_handle bitmap)
{
    QLabel* label = qobject_cast<QLabel*>(widgetFromHandle(h));
    if (!label) return;
    
    if (bitmap) {
        const QPixmap* pixmap = reinterpret_cast<const QPixmap*>(bitmap);
        label->setPixmap(*pixmap);
    } else {
        label->clear();
    }
    logf("[Mock] SetBitmapBoxBitmap");
}

int32 BitmapBoxContext::GetBitmapBoxMargin(const_control_handle h)
{
    QLabel* label = qobject_cast<QLabel*>(widgetFromHandle(h));
    return label ? label->margin() : 0;
}

void BitmapBoxContext::SetBitmapBoxMargin(control_handle h, int32 margin)
{
    QLabel* label = qobject_cast<QLabel*>(widgetFromHandle(h));
    if (label) {
        label->setMargin(margin);
        logf("[Mock] SetBitmapBoxMargin: %d", margin);
    }
}

api_bool BitmapBoxContext::GetBitmapBoxAutoFitEnabled(const_control_handle h)
{
    QLabel* label = qobject_cast<QLabel*>(widgetFromHandle(h));
    return (label && label->hasScaledContents()) ? api_true : api_false;
}

void BitmapBoxContext::SetBitmapBoxAutoFitEnabled(control_handle h, api_bool enabled)
{
    QLabel* label = qobject_cast<QLabel*>(widgetFromHandle(h));
    if (label) {
        label->setScaledContents(enabled);
        logf("[Mock] SetBitmapBoxAutoFitEnabled: %d", enabled);
    }
}

// =============================================================
// Bitmap Context
// =============================================================

bitmap_handle BitmapContext::CreateBitmap(api_handle module,
                                      int32 width,
                                      int32 height,
                                      void* data)
{
    QPixmap* pixmap = new QPixmap(width, height);
    
    if (data) {
        QImage img(static_cast<uchar*>(data), width, height,
                   QImage::Format_ARGB32);
        *pixmap = QPixmap::fromImage(img);
    } else {
        pixmap->fill(Qt::transparent);
    }
    
    logf("[Mock] CreateBitmap: %dx%d", width, height);
    return reinterpret_cast<bitmap_handle>(pixmap);
}

bitmap_handle BitmapContext::CreateBitmapXPM(api_handle module, const char** xpm)
{
    if (!xpm) return nullptr;
    
    QPixmap* pixmap = new QPixmap(xpm);
    
    logf("[Mock] CreateBitmapXPM");
    return reinterpret_cast<bitmap_handle>(pixmap);
}

bitmap_handle BitmapContext::CreateBitmapFromFile(api_handle module,
                                              const char16_type* filePath)
{
    if (!filePath) return nullptr;
    
    QString path = QString::fromUtf16(filePath);
    QPixmap* pixmap = new QPixmap(path);
    
    if (pixmap->isNull()) {
        delete pixmap;
        logf("[Mock] CreateBitmapFromFile FAILED: %s", path.toUtf8().constData());
        return BitmapContext::CreateBitmap(module, 100, 100, nullptr);
    }
    
    logf("[Mock] CreateBitmapFromFile: %s", path.toUtf8().constData());
    return reinterpret_cast<bitmap_handle>(pixmap);
}

bitmap_handle BitmapContext::CreateBitmapFromFile8(api_handle module,
                                               const char* filePath)
{
    if (!filePath) return nullptr;
    
    QPixmap* pixmap = new QPixmap(QString::fromUtf8(filePath));
    
    if (pixmap->isNull()) {
        delete pixmap;
        return nullptr;
    }
    
    logf("[Mock] CreateBitmapFromFile8: %s", filePath);
    return reinterpret_cast<bitmap_handle>(pixmap);
}

bitmap_handle BitmapContext::CreateBitmapFromData(api_handle module,
                                              const void* data,
                                              size_type size,
                                              const char* format,
                                              uint32 flags)
{
    if (!data || size == 0) return nullptr;
    
    QPixmap* pixmap = new QPixmap();
    QByteArray bytes(static_cast<const char*>(data), size);
    
    if (!pixmap->loadFromData(bytes, format)) {
        delete pixmap;
        logf("[Mock] CreateBitmapFromData FAILED");
        return nullptr;
    }
    
    logf("[Mock] CreateBitmapFromData: format=%s size=%zu",
         format ? format : "auto", size);
    return reinterpret_cast<bitmap_handle>(pixmap);
}

bitmap_handle BitmapContext::CreateEmptyBitmap(api_handle module)
{
    QPixmap* pixmap = new QPixmap();
    
    logf("[Mock] CreateEmptyBitmap");
    return reinterpret_cast<bitmap_handle>(pixmap);
}

bitmap_handle BitmapContext::CloneBitmap(api_handle module,
                                     const_bitmap_handle source)
{
    if (!source) return nullptr;
    
    const QPixmap* src = reinterpret_cast<const QPixmap*>(source);
    QPixmap* clone = new QPixmap(*src);
    
    logf("[Mock] CloneBitmap");
    return reinterpret_cast<bitmap_handle>(clone);
}

bitmap_handle BitmapContext::CloneBitmapRect(api_handle module,
                                         const_bitmap_handle source,
                                         int32 x, int32 y,
                                         int32 w, int32 h)
{
    if (!source) return nullptr;
    
    const QPixmap* src = reinterpret_cast<const QPixmap*>(source);
    QPixmap* clone = new QPixmap(src->copy(x, y, w, h));
    
    logf("[Mock] CloneBitmapRect: %d,%d %dx%d", x, y, w, h);
    return reinterpret_cast<bitmap_handle>(clone);
}

bitmap_handle BitmapContext::CreateBitmapFromSVG(api_handle module,
                                             const char* svgSource,
                                             int32 width,
                                             int32 height,
                                             uint32 flags)
{
    if (!svgSource) return nullptr;
    
    QByteArray svgData(svgSource);
    QSvgRenderer renderer(svgData);
    
    if (!renderer.isValid()) {
        logf("[Mock] CreateBitmapFromSVG FAILED: invalid SVG");
        return nullptr;
    }
    
    QPixmap* pixmap = new QPixmap(width, height);
    pixmap->fill(Qt::transparent);
    
    QPainter painter(pixmap);
    renderer.render(&painter);
    
    logf("[Mock] CreateBitmapFromSVG: %dx%d", width, height);
    return reinterpret_cast<bitmap_handle>(pixmap);
}

bitmap_handle BitmapContext::CreateBitmapFromSVGFile(api_handle module,
                                                 const char16_type* filePath,
                                                 int32 width,
                                                 int32 height,
                                                 uint32 flags)
{
    if (!filePath) return nullptr;
    
    QString path = QString::fromUtf16(filePath);
    QSvgRenderer renderer(path);
    
    if (!renderer.isValid()) {
        logf("[Mock] CreateBitmapFromSVGFile FAILED: %s",
             path.toUtf8().constData());
        return nullptr;
    }
    
    QPixmap* pixmap = new QPixmap(width, height);
    pixmap->fill(Qt::transparent);
    
    QPainter painter(pixmap);
    renderer.render(&painter);
    
    logf("[Mock] CreateBitmapFromSVGFile: %s", path.toUtf8().constData());
    return reinterpret_cast<bitmap_handle>(pixmap);
}

int32 BitmapContext::GetBitmapFormat(bitmap_handle h)
{
    if (!h) return 0;
    
    const QPixmap* pixmap = reinterpret_cast<const QPixmap*>(h);
    return pixmap->depth();
}

void BitmapContext::SetBitmapFormat(bitmap_handle h, int32 format)
{
    logf("[Mock] SetBitmapFormat: %d (not implemented)", format);
}

unsigned int* BitmapContext::GetBitmapScanLine(bitmap_handle h, int32 y)
{
    if (!h) return nullptr;
    
    const QPixmap* pixmap = reinterpret_cast<const QPixmap*>(h);
    QImage img = pixmap->toImage();
    
    if (y < 0 || y >= img.height()) return nullptr;
    
    return reinterpret_cast<unsigned int*>(img.scanLine(y));
}

api_bool BitmapContext::GetBitmapDimensions(const_bitmap_handle h,
                                        int32* width,
                                        int32* height)
{
    if (!h) return api_false;
    
    const QPixmap* pixmap = reinterpret_cast<const QPixmap*>(h);
    
    if (width) *width = pixmap->width();
    if (height) *height = pixmap->height();
    
    return api_true;
}

api_bool BitmapContext::IsEmptyBitmap(const_bitmap_handle h)
{
    if (!h) return api_true;
    
    const QPixmap* pixmap = reinterpret_cast<const QPixmap*>(h);
    return pixmap->isNull() ? api_true : api_false;
}

uint32 BitmapContext::GetBitmapPixel(const_bitmap_handle h, int32 x, int32 y)
{
    if (!h) return 0;
    
    const QPixmap* pixmap = reinterpret_cast<const QPixmap*>(h);
    QImage img = pixmap->toImage();
    
    if (x < 0 || x >= img.width() || y < 0 || y >= img.height())
        return 0;
    
    return img.pixel(x, y);
}

void BitmapContext::SetBitmapPixel(bitmap_handle h, int32 x, int32 y, uint32 rgba)
{
    if (!h) return;
    
    QPixmap* pixmap = reinterpret_cast<QPixmap*>(h);
    QImage img = pixmap->toImage();
    
    if (x >= 0 && x < img.width() && y >= 0 && y < img.height()) {
        img.setPixel(x, y, rgba);
        *pixmap = QPixmap::fromImage(img);
    }
}

bitmap_handle BitmapContext::MirroredBitmap(const_bitmap_handle h,
                                        api_bool horizontal,
                                        api_bool vertical)
{
    if (!h) return nullptr;
    
    const QPixmap* src = reinterpret_cast<const QPixmap*>(h);
    QTransform transform;
    
    if (horizontal) transform.scale(-1, 1);
    if (vertical) transform.scale(1, -1);
    
    QPixmap* result = new QPixmap(src->transformed(transform));
    
    logf("[Mock] MirroredBitmap: h=%d v=%d", horizontal, vertical);
    return reinterpret_cast<bitmap_handle>(result);
}

bitmap_handle BitmapContext::ScaledBitmap(const_bitmap_handle h,
                                      int32 width,
                                      int32 height,
                                      api_bool smooth)
{
    if (!h) return nullptr;
    
    const QPixmap* src = reinterpret_cast<const QPixmap*>(h);
    Qt::TransformationMode mode = smooth ?
        Qt::SmoothTransformation : Qt::FastTransformation;
    
    QPixmap* result = new QPixmap(src->scaled(width, height,
                                              Qt::IgnoreAspectRatio, mode));
    
    logf("[Mock] ScaledBitmap: %dx%d smooth=%d", width, height, smooth);
    return reinterpret_cast<bitmap_handle>(result);
}

bitmap_handle BitmapContext::RotatedBitmap(const_bitmap_handle h,
                                       double angle,
                                       api_bool smooth)
{
    if (!h) return nullptr;
    
    const QPixmap* src = reinterpret_cast<const QPixmap*>(h);
    QTransform transform;
    transform.rotate(angle);
    
    Qt::TransformationMode mode = smooth ?
        Qt::SmoothTransformation : Qt::FastTransformation;
    
    QPixmap* result = new QPixmap(src->transformed(transform, mode));
    
    logf("[Mock] RotatedBitmap: angle=%.2f smooth=%d", angle, smooth);
    return reinterpret_cast<bitmap_handle>(result);
}

api_bool BitmapContext::LoadBitmap(bitmap_handle h, const char16_type* filePath)
{
    if (!h || !filePath) return api_false;
    
    QPixmap* pixmap = reinterpret_cast<QPixmap*>(h);
    QString path = QString::fromUtf16(filePath);
    
    bool result = pixmap->load(path);
    
    logf("[Mock] LoadBitmap: %s %s",
         path.toUtf8().constData(), result ? "OK" : "FAILED");
    
    return result ? api_true : api_false;
}

api_bool BitmapContext::SaveBitmap(const_bitmap_handle h,
                               const char16_type* filePath,
                               int32 quality)
{
    if (!h || !filePath) return api_false;
    
    const QPixmap* pixmap = reinterpret_cast<const QPixmap*>(h);
    QString path = QString::fromUtf16(filePath);
    
    bool result = pixmap->save(path, nullptr, quality);
    
    logf("[Mock] SaveBitmap: %s quality=%d %s",
         path.toUtf8().constData(), quality, result ? "OK" : "FAILED");
    
    return result ? api_true : api_false;
}

api_bool BitmapContext::LoadBitmapData(bitmap_handle h,
                                   const void* data,
                                   size_type size,
                                   const char* format,
                                   uint32 flags)
{
    if (!h || !data || size == 0) return api_false;
    
    QPixmap* pixmap = reinterpret_cast<QPixmap*>(h);
    QByteArray bytes(static_cast<const char*>(data), size);
    
    bool result = pixmap->loadFromData(bytes, format);
    
    logf("[Mock] LoadBitmapData: format=%s size=%zu %s",
         format ? format : "auto", size, result ? "OK" : "FAILED");
    
    return result ? api_true : api_false;
}

void BitmapContext::CopyBitmap(bitmap_handle dest,
                           int32 xDst, int32 yDst,
                           const_bitmap_handle src,
                           int32 xSrc, int32 ySrc,
                           int32 width, int32 height)
{
    if (!dest || !src) return;
    
    QPixmap* dstPixmap = reinterpret_cast<QPixmap*>(dest);
    const QPixmap* srcPixmap = reinterpret_cast<const QPixmap*>(src);
    
    QPainter painter(dstPixmap);
    painter.drawPixmap(xDst, yDst, *srcPixmap, xSrc, ySrc, width, height);
    
    logf("[Mock] CopyBitmap: dst(%d,%d) <- src(%d,%d) %dx%d",
         xDst, yDst, xSrc, ySrc, width, height);
}

void BitmapContext::FillBitmap(bitmap_handle h,
                           int32 x, int32 y,
                           int32 width, int32 height,
                           uint32 rgba)
{
    if (!h) return;
    
    QPixmap* pixmap = reinterpret_cast<QPixmap*>(h);
    QPainter painter(pixmap);
    painter.fillRect(x, y, width, height, QColor(rgba));
    
    logf("[Mock] FillBitmap: (%d,%d) %dx%d rgba=0x%08x",
         x, y, width, height, rgba);
}

void BitmapContext::GetBitmapDevicePixelRatio(const_bitmap_handle h, double* ratio)
{
    if (!h || !ratio) return;
    
    const QPixmap* pixmap = reinterpret_cast<const QPixmap*>(h);
    *ratio = pixmap->devicePixelRatio();
}

void BitmapContext::SetBitmapDevicePixelRatio(bitmap_handle h, double ratio)
{
    if (!h) return;
    
    QPixmap* pixmap = reinterpret_cast<QPixmap*>(h);
    pixmap->setDevicePixelRatio(ratio);
    
    logf("[Mock] SetBitmapDevicePixelRatio: %.2f", ratio);
}

// Bitmap operation stubs (implement as needed)
void BitmapContext::OrBitmap(bitmap_handle, int32, int32, int32, int32, uint32) {}
void BitmapContext::OrBitmaps(bitmap_handle, int32, int32, const_bitmap_handle, int32, int32, int32, int32) {}
void BitmapContext::AndBitmap(bitmap_handle, int32, int32, int32, int32, uint32) {}
void BitmapContext::AndBitmaps(bitmap_handle, int32, int32, const_bitmap_handle, int32, int32, int32, int32) {}
void BitmapContext::XorBitmap(bitmap_handle, int32, int32, int32, int32, uint32) {}
void BitmapContext::XorBitmaps(bitmap_handle, int32, int32, const_bitmap_handle, int32, int32, int32, int32) {}
void BitmapContext::XorBitmapRect(bitmap_handle, int32, int32, int32, int32, uint32) {}
void BitmapContext::ReplaceBitmapColor(bitmap_handle, int32, int32, int32, int32, uint32, uint32) {}
void BitmapContext::SetBitmapAlpha(bitmap_handle, int32, int32, int32, int32, uint8) {}

// =============================================================
// UI Context
// =============================================================

api_bool UIContext::AttachToUIObject(api_handle object, api_handle client)
{
    if (!object) return api_false;
    
    MockBase* C = get(object);
    if (!C) return api_false;
    
    logf("[Mock] UIContext::AttachToUIObject: object=%p client=%p", object, client);
    
    return api_true;
}

api_bool UIContext::DetachFromUIObject(api_handle object, api_handle client)
{
    if (!object) return api_true;
    
    MockBase* C = get(object);
    if (!C) return api_true;
    
    logf("[Mock] UIContext::DetachFromUIObject: object=%p client=%p", object, client);
    
    return api_true;
}

api_handle UIContext::GetUIObjectModule(const_api_handle object)
{
    if (!object) return nullptr;
    
    MockBase* C = get(object);
    if (!C) return nullptr;
    
    return C->moduleHandle;
}

size_type UIContext::GetUIObjectRefCount(const_api_handle object)
{
    if (!object) return 0;
    
    MockBase* C = get(object);
    if (!C) return 0;
    
    return 1;
}

api_bool UIContext::GetUIObjectType(const_api_handle object, 
                                char* type, 
                                size_type* len)
{
    if (!object) return api_false;
    
    MockBase* C = get(object);
    if (!C) return api_false;
    
    const char* typeName = "Control";
    
    if (C->isSizer) {
        typeName = "Sizer";
    } else if (C->isTreeNode) {
        typeName = "TreeNode";
    } else if (C->widget) {
        if (qobject_cast<QLabel*>(C->widget))
            typeName = "Label";
        else if (qobject_cast<QLineEdit*>(C->widget))
            typeName = "Edit";
        else if (qobject_cast<QSlider*>(C->widget))
            typeName = "Slider";
        else if (qobject_cast<QCheckBox*>(C->widget))
            typeName = "CheckBox";
        else if (qobject_cast<QRadioButton*>(C->widget))
            typeName = "RadioButton";
        else if (qobject_cast<QPushButton*>(C->widget))
            typeName = "PushButton";
        else if (qobject_cast<QToolButton*>(C->widget))
            typeName = "ToolButton";
        else if (qobject_cast<QComboBox*>(C->widget))
            typeName = "ComboBox";
        else if (qobject_cast<QSpinBox*>(C->widget))
            typeName = "SpinBox";
        else if (qobject_cast<QTreeWidget*>(C->widget))
            typeName = "TreeBox";
    }
    
    size_t typeLen = std::strlen(typeName);
    
    if (len) *len = typeLen;
    
    if (type && len && *len > 0) {
        size_t copyLen = std::min(*len, typeLen);
        std::memcpy(type, typeName, copyLen);
        if (copyLen < *len) type[copyLen] = '\0';
    }
    
    return api_true;
}

api_bool UIContext::GetUIObjectId(const_api_handle object, 
                              char16_type* id, 
                              size_type* len)
{
    if (!object) return api_false;
    
    MockBase* C = get(object);
    if (!C || !C->widget) return api_false;
    
    QString objectName = C->widget->objectName();
    
    if (len) *len = objectName.length();
    
    if (id && len && *len > 0) {
        const char16_type* src = reinterpret_cast<const char16_type*>(
            objectName.utf16());
        size_t copyLen = std::min(*len, static_cast<size_type>(objectName.length()));
        std::memcpy(id, src, copyLen * sizeof(char16_t));
        if (copyLen < *len) id[copyLen] = 0;
    }
    
    return api_true;
}

api_bool UIContext::SetUIObjectId(api_handle object, const char16_type* id)
{
    if (!object || !id) return api_false;
    
    MockBase* C = get(object);
    if (!C || !C->widget) return api_false;
    
    QString qid = QString::fromUtf16(id);
    C->widget->setObjectName(qid);
    
    logf("[Mock] UIContext::SetUIObjectId: %s", qid.toUtf8().constData());
    
    return api_true;
}

api_bool UIContext::SetHandleDestroyedEventRoutine(api_handle object,
                                               pcl::destroy_event_routine routine)
{
    logf("[Mock] UIContext::SetHandleDestroyedEventRoutine: object=%p", object);
    return api_true;
}

// UI Control Object functions (aliases)
api_bool UIContext::AttachToUIControlObject(api_handle object, api_handle client)
{
    return AttachToUIObject(object, client);
}

api_bool UIContext::DetachFromUIControlObject(api_handle object, api_handle client)
{
    return DetachFromUIObject(object, client);
}

api_handle UIContext::GetUIControlObjectModule(const_api_handle object)
{
    return GetUIObjectModule(object);
}

size_type UIContext::GetUIControlObjectRefCount(const_api_handle object)
{
    return GetUIObjectRefCount(object);
}

api_bool UIContext::GetUIControlObjectType(const_api_handle object, 
                                char* type, 
                                size_type* len)
{
    return GetUIObjectType(object, type, len);
}

api_bool UIContext::GetUIControlObjectId(const_api_handle object, 
                              char16_type* id, 
                              size_type* len)
{
    return GetUIObjectId(object, id, len);
}

api_bool UIContext::SetUIControlObjectId(api_handle object, const char16_type* id)
{
    return SetUIObjectId(object, id);
}

// =============================================================
// Font Context
// =============================================================

font_handle FontContext::CloneFont(api_handle, const_font_handle h)
{
    if (!h) return nullptr;
    
    const QFont* src = reinterpret_cast<const QFont*>(h);
    return reinterpret_cast<font_handle>(new QFont(*src));
}

int32 FontContext::GetStringPixelWidth(const_font_handle h, const char16_type* text)
{
    QString s = QString::fromUtf16(text);
    
    QFont f;
    if (h) {
        const QFont* font = reinterpret_cast<const QFont*>(h);
        f = *font;
    }
    
    QFontMetrics fm(f);
    return fm.horizontalAdvance(s);
}

int32 FontContext::GetFontHeight(const_font_handle h)
{
    QFont f;
    if (h) {
        const QFont* font = reinterpret_cast<const QFont*>(h);
        f = *font;
    }
    
    QFontMetrics fm(f);
    return fm.height();
}

// =============================================================
// Global Context
// =============================================================

api_bool GlobalContext::Abort()
{
    logf("[Mock] GlobalContext::Abort called");
    abort();
}

void* GlobalContext::Allocate(size_type size)
{
    return malloc(size);
}

api_bool GlobalContext::Deallocate(void* ptr)
{
    free(ptr);
    return api_true;
}

api_bool GlobalContext::EnableAbort()
{
    return api_true;
}

api_bool GlobalContext::ErrorMessage(uint32, char16_type*, size_type*)
{
    return api_false;
}

console_handle GlobalContext::GetConsole()
{
    return reinterpret_cast<console_handle>(new MockBase());
}

api_bool GlobalContext::GetGlobalInteger(const char*, void*, api_bool)
{
    return api_false;
}

api_bool GlobalContext::GetGlobalString(const char*, char16_type*, size_type*)
{
    return api_false;
}

api_bool GlobalContext::GetGlobalFlag(const char*, uint32*)
{
    return api_false;
}

uint32 GlobalContext::GetKeyboardModifiers()
{
    return QApplication::keyboardModifiers();
}

uint32 GlobalContext::GetProcessStatus()
{
    return 0;
}

uint32 GlobalContext::LastError()
{
    return 0;
}

void GlobalContext::LaunchProcessInstance(meta_process_handle, const_process_handle, int32, uint32) {}
void GlobalContext::LaunchProcessInstanceOnView(meta_process_handle, const_process_handle, view_handle, uint32) {}

uint32 GlobalContext::MessageBox(const char16_type* text, 
                                 const char16_type* caption, 
                                 uint32 button0, 
                                 uint32 button1, 
                                 uint32 button2, 
                                 uint32 defButton, 
                                 uint32 escButton, 
                                 uint32 icon)
{
    QString qtext = QString::fromUtf16(text);
    QString qcaption = QString::fromUtf16(caption);
    
    QMessageBox msgBox;
    msgBox.setText(qtext);
    msgBox.setWindowTitle(qcaption);
    
    return msgBox.exec();
}

api_bool GlobalContext::ReadSettingsInteger(api_handle, int32* result, const char*, api_bool)
{
    if (result) *result = 0;
    return api_true;
}

api_bool GlobalContext::ShowConsole(console_handle, api_bool)
{
    return api_true;
}

api_bool GlobalContext::WriteConsole(console_handle, const char16_type*, api_bool)
{
    return api_true;
}

api_bool GlobalContext::WriteSettingsInteger(api_handle, int32, const char*, api_bool)
{
    return api_true;
}

void GlobalContext::ProcessEvents(uint32) {}

int32 GlobalContext::MaxProcessorsAllowedForModule(void*, uint32)
{
    return QThread::idealThreadCount();
}

const api_pixtraits_lut* GlobalContext::GetPixelTraitsLUT(uint32)
{
    static api_pixtraits_lut lut = {};
    return &lut;
}

api_bool GlobalContext::BrowseProcessDocumentation(meta_process_handle, uint32)
{
    return api_false;
}

// =============================================================
// Process Context
// =============================================================

process_handle ProcessContext::CloneProcessInstance(api_handle, const_process_handle, uint32)
{
    return nullptr;
}

// =============================================================
// View Context
// =============================================================

void ViewContext::UnlockView(void*, uint32, uint32, uint32) {}
view_handle ViewContext::GetViewById(const char*) { return nullptr; }
void ViewContext::GetViewLocks(const void*, uint32*, uint32*) {}
api_bool ViewContext::GetViewFullId(const void*, char*, size_type*) { return api_false; }
void ViewContext::LockView(void*, uint32, uint32, uint32) {}
api_bool ViewContext::GetViewId(const void*, char*, size_type*) { return api_false; }
image_handle ViewContext::GetViewImage(view_handle) { return nullptr; }

// =============================================================
// Dialog Context
// =============================================================

api_bool DialogContext::ExecuteOpenFileDialog(char16_type*, const char16_type*, const char16_type*, const char16_type*, const char16_type*)
{
    return api_false;
}

api_bool DialogContext::ExecuteOpenMultipleFilesDialog(char16_type*, uint32(*)(const char16_type*, void*), void*, const char16_type*, const char16_type*, const char16_type*, const char16_type*)
{
    return api_false;
}

// =============================================================
// ImageWindow Context - Stubs
// =============================================================

window_handle ImageWindowContext::CreateImageWindow(int, int, int, int, uint32, uint32, uint32, const char*)
{
    return nullptr;
}

api_bool ImageWindowContext::LoadImageWindows(const char16_type*, const char*, const char*, api_bool, api_bool, pcl::window_enumeration_callback, void*)
{
    return api_false;
}

api_bool ImageWindowContext::CloseImageWindow(window_handle, api_bool)
{
    return api_false;
}

window_handle ImageWindowContext::GetImageWindowById(const char*)
{
    return nullptr;
}

window_handle ImageWindowContext::GetImageWindowByFilePath(const char16_type*)
{
    return nullptr;
}

window_handle ImageWindowContext::GetActiveImageWindow()
{
    QWidget* w = QApplication::activeWindow();
    return reinterpret_cast<window_handle>(w);
}

void ImageWindowContext::SetImageWindowVisible(window_handle handle, api_bool visible)
{
    QWidget* w = reinterpret_cast<QWidget*>(handle);
    if (!w) return;
    
    if (visible)
        w->show();
    else
        w->hide();
}

void ImageWindowContext::ZoomImageWindowToFit(window_handle, api_bool, api_bool, api_bool, api_bool) {}
int32 ImageWindowContext::GetImageWindowZoomFactor(const_window_handle) { return 1; }
void ImageWindowContext::SetImageWindowZoomFactor(window_handle, int32) {}

// Remaining ImageWindow stubs
void ImageWindowContext::EnumerateImageWindows(pcl::window_enumeration_callback, void*, api_bool) {}
void ImageWindowContext::EnumeratePreviews(const_window_handle, pcl::view_enumeration_callback, void*) {}
api_bool ImageWindowContext::GetImageWindowNewFlag(const_window_handle) { return api_false; }
api_bool ImageWindowContext::GetImageWindowCopyFlag(const_window_handle) { return api_false; }
api_bool ImageWindowContext::GetImageWindowFileURL(const_window_handle, char16_type*, size_type*) { return api_false; }
api_bool ImageWindowContext::GetImageWindowFilePath(const_window_handle, char16_type*, size_type*) { return api_false; }
api_bool ImageWindowContext::GetImageWindowFileInfo(const_window_handle, api_image_file_info*) { return api_false; }
size_type ImageWindowContext::GetImageWindowModifyCount(const_window_handle) { return 0; }
view_handle ImageWindowContext::GetImageWindowMainView(const_window_handle) { return nullptr; }
view_handle ImageWindowContext::GetImageWindowCurrentView(const_window_handle) { return nullptr; }
void ImageWindowContext::SetImageWindowCurrentView(window_handle, view_handle) {}
int32 ImageWindowContext::GetImageType(const_window_handle) { return 0; }
api_bool ImageWindowContext::SetImageType(window_handle, int32, api_bool) { return api_false; }
void ImageWindowContext::PurgeImageWindowProperties(window_handle) {}
api_bool ImageWindowContext::ValidateImageWindowView(const_window_handle, const_view_handle) { return api_false; }
int32 ImageWindowContext::GetPreviewCount(const_window_handle) { return 0; }
view_handle ImageWindowContext::GetPreviewById(const_window_handle, const char*) { return nullptr; }
view_handle ImageWindowContext::GetSelectedPreview(const_window_handle) { return nullptr; }
void ImageWindowContext::SelectPreview(window_handle, view_handle) {}
view_handle ImageWindowContext::CreatePreview(window_handle, int32, int32, int32, int32, const char*) { return nullptr; }
void ImageWindowContext::ModifyPreview(window_handle, const char*, int32, int32, int32, int32, const char*) {}
void ImageWindowContext::GetPreviewRect(const_window_handle, const char*, int32*, int32*, int32*, int32*) {}
void ImageWindowContext::DeletePreview(window_handle, const char*) {}
void ImageWindowContext::DeletePreviews(window_handle) {}
window_handle ImageWindowContext::GetImageWindowMask(const_window_handle, api_bool*) { return nullptr; }
void ImageWindowContext::SetImageWindowMask(window_handle, window_handle, api_bool) {}
api_bool ImageWindowContext::GetImageWindowMaskEnabled(const_window_handle) { return api_false; }
void ImageWindowContext::SetImageWindowMaskEnabled(window_handle, api_bool) {}
api_bool ImageWindowContext::GetImageWindowMaskVisible(const_window_handle) { return api_false; }
void ImageWindowContext::SetImageWindowMaskVisible(window_handle, api_bool) {}
api_bool ImageWindowContext::ValidateImageWindowMask(const_window_handle, const_window_handle) { return api_false; }
int32 ImageWindowContext::GetMaskReferenceCount(const_window_handle) { return 0; }
void ImageWindowContext::RemoveImageWindowMaskReferences(window_handle) {}
void ImageWindowContext::UpdateImageWindowMaskReferences(window_handle) {}

// Remaining stubs continue...
void ImageWindowContext::GetImageWindowSampleFormat(const_window_handle, uint32*, api_bool*) {}
void ImageWindowContext::SetImageWindowSampleFormat(window_handle, uint32, api_bool) {}
void ImageWindowContext::GetImageWindowRGBWS(const_window_handle, api_RGBWS*) {}
void ImageWindowContext::SetImageWindowRGBWS(window_handle, const api_RGBWS*) {}
api_bool ImageWindowContext::GetImageWindowGlobalRGBWS(const_window_handle) { return api_false; }
void ImageWindowContext::SetImageWindowGlobalRGBWS(window_handle) {}
void ImageWindowContext::GetGlobalRGBWS(api_RGBWS*) {}
void ImageWindowContext::SetGlobalRGBWS(const api_RGBWS*) {}

// Graphics, SVG, Brush, Pen contexts - all stubs
api_bool GraphicsContext::GetGraphicsStatus(const_graphics_handle) { return api_false; }
void GraphicsContext::EndPaint(graphics_handle) {}

// =============================================================
// Shared Image Context
// =============================================================

void* SharedImageContext::GetImageOwner(const_image_handle) { return nullptr; }
api_bool SharedImageContext::GetImageRefCount(const_image_handle, uint32*) { return api_false; }
api_bool SharedImageContext::IsValidImageHandle(const_image_handle) { return api_false; }
api_bool SharedImageContext::AttachToImage(image_handle, void*) { return api_false; }
api_bool SharedImageContext::DetachFromImage(image_handle, void*) { return api_false; }
api_bool SharedImageContext::GetImageFormat(const_image_handle, uint32*, api_bool*) { return api_false; }
api_bool SharedImageContext::SetImageRGBWS(image_handle, const api_RGBWS*) { return api_false; }
api_bool SharedImageContext::GetImageColorSpace(const_image_handle, uint32*) { return api_false; }
api_bool SharedImageContext::SetImageColorSpace(image_handle, uint32) { return api_false; }
api_bool SharedImageContext::GetImageGeometry(const_image_handle, uint32*, uint32*, uint32*) { return api_false; }
api_bool SharedImageContext::SetImageGeometry(image_handle, uint32, uint32, uint32) { return api_false; }
api_bool SharedImageContext::GetImagePixelData(image_handle, void***) { return api_false; }
api_bool SharedImageContext::SetImagePixelData(image_handle, void**) { return api_false; }
api_bool SharedImageContext::GetImageRGBWS(const_image_handle, api_RGBWS*) { return api_false; }
image_handle SharedImageContext::CreateImage(uint32, uint32, uint32, uint32, uint32, uint32, void*) { return nullptr; }

// =============================================================
// Thread Context (uses PCLThreadMock)
// =============================================================

thread_handle ThreadContext::CreateThread(api_handle handle, api_handle client, uint32 flags)
{
    return pcl_mock::CreateThread(handle, client, flags);
}

void ThreadContext::StartThread(thread_handle handle, uint32 priority)
{
    pcl_mock::StartThread(handle, priority);
}

void ThreadContext::KillThread(thread_handle) {}

api_bool ThreadContext::IsThreadActive(const_thread_handle handle)
{
    return pcl_mock::IsThreadActive(handle);
}

uint32 ThreadContext::GetThreadPriority(const_thread_handle) { return 0; }
void ThreadContext::SetThreadPriority(thread_handle, uint32) {}
uint32 ThreadContext::GetThreadStackSize(const_thread_handle) { return 0; }
void ThreadContext::SetThreadStackSize(thread_handle, uint32) {}
api_bool ThreadContext::WaitThread(thread_handle, uint32) { return api_false; }
void ThreadContext::SleepThread(thread_handle, uint32) {}
uint32 ThreadContext::GetThreadStatus(const_thread_handle) { return 0; }
void ThreadContext::SetThreadStatus(thread_handle, uint32) {}
api_bool ThreadContext::GetThreadStatusEx(const_thread_handle, uint32*, uint32) { return api_false; }
api_bool ThreadContext::GetThreadConsoleOutputText(const_thread_handle, char16_type*, size_type*) { return api_false; }
void ThreadContext::AppendThreadConsoleOutputText(thread_handle, const char16_type*, api_bool) {}
void ThreadContext::ClearThreadConsoleOutputText(thread_handle) {}

thread_handle ThreadContext::GetCurrentThread()
{
    return pcl_mock::GetCurrentThread();
}

api_bool ThreadContext::SetThreadExecRoutine(thread_handle handle, pcl::thread_exec_routine dispatcher)
{
    return pcl_mock::SetThreadExecRoutine(handle, dispatcher);
}

int32 ThreadContext::PerformanceAnalysisValue(int32, size_type, int32, api_bool, int32, int32, int32)
{
    return 0;
}

// =============================================================
// Mutex Context - Stubs
// =============================================================

mutex_handle Mutex_CreateMutex(api_handle, uint32) { return nullptr; }
mutex_handle Mutex_CreateReadWriteMutex(api_handle, uint32) { return nullptr; }
api_bool Mutex_GetLockState(const_mutex_handle) { return api_false; }
api_bool Mutex_Lock(mutex_handle, api_bool) { return api_false; }
api_bool Mutex_LockForRead(mutex_handle, api_bool) { return api_false; }
api_bool Mutex_LockForWrite(mutex_handle, api_bool) { return api_false; }
void Mutex_Unlock(mutex_handle) {}

// =============================================================
// ViewList Context - Stubs
// =============================================================

control_handle ViewList_CreateViewList(api_handle, control_handle, uint32) { return nullptr; }
void ViewList_RegenerateViewList(control_handle, api_bool, api_bool, api_bool) {}
void ViewList_GetViewListContents(const_control_handle, api_bool*, api_bool*, api_bool*) {}
const_view_handle ViewList_GetViewListExcludedView(const_control_handle) { return nullptr; }
void ViewList_SetViewListExcludedView(control_handle, const_view_handle) {}
view_handle ViewList_GetViewListCurrentView(const_control_handle) { return nullptr; }
void ViewList_SetViewListCurrentView(control_handle, view_handle) {}
api_bool ViewList_FindViewListView(const_control_handle, const_view_handle) { return api_false; }
void ViewList_RemoveViewListView(control_handle, const_view_handle) {}
api_bool ViewList_SetViewListViewSelectedEventRoutine(control_handle, api_handle, pcl::view_event_routine) { return api_true; }
api_bool ViewList_SetViewListCurrentViewUpdatedEventRoutine(control_handle, api_handle, pcl::view_event_routine) { return api_true; }

// =============================================================
// Numerical Context - Stubs
// =============================================================

api_bool NumericalContext::GaussJordanInPlaceF(float**, float**, int32, int32) { return api_false; }
api_bool NumericalContext::GaussJordanInPlaceD(double**, double**, int32, int32) { return api_false; }
api_bool NumericalContext::SVDInPlaceF(float**, float*, float**, int32, int32) { return api_false; }
api_bool NumericalContext::SVDInPlaceD(double**, double*, double**, int32, int32) { return api_false; }
api_enum NumericalContext::LinearFitF(double*, double*, double*, const float*, const float*, size_type, api_bool(*)(void*), void*) { return 0; }
api_enum NumericalContext::LinearFitD(double*, double*, double*, const double*, const double*, size_type, api_bool(*)(void*), void*) { return 0; }
api_bool NumericalContext::FFTDestroyTransform(fft_handle) { return api_false; }
size_type NumericalContext::FFTRealOptimizedLengthF(size_type) { return 0; }
fft_handle NumericalContext::FFTCreateComplexTransformD(size_type) { return nullptr; }
api_bool NumericalContext::FFTRealTransformD(fft_handle, void*, const double*) { return api_false; }
api_bool NumericalContext::FFTComplexTransformD(fft_handle, void*, const void*) { return api_false; }
fft_handle NumericalContext::FFTCreateRealTransformD(size_type) { return nullptr; }
api_bool NumericalContext::FFTComplexInverseTransformD(fft_handle, void*, const void*) { return api_false; }
fft_handle NumericalContext::FFTCreateComplexInverseTransformD(size_type) { return nullptr; }

// =============================================================
// FileFormat Context - Stubs
// =============================================================

api_bool FileFormatContext::GetFileFormatName(meta_format_handle, char*, size_type*) { return api_false; }
file_format_handle FileFormatContext::CreateFileFormatInstance(api_handle, meta_format_handle) { return nullptr; }
api_bool FileFormatContext::ReadImage(file_format_handle, image_handle) { return api_false; }
api_bool FileFormatContext::GetImageId(const_file_format_handle, char*, size_type*, uint32) { return api_false; }
api_bool FileFormatContext::SelectImage(file_format_handle, uint32) { return api_false; }
api_bool FileFormatContext::GetImageCount(const_file_format_handle) { return api_false; }
api_bool FileFormatContext::OpenImageFileEx(file_format_handle, const char16_type*, const char*, uint32) { return api_false; }
bitmap_handle FileFormatContext::GetFileFormatIcon(const_meta_format_handle) { return nullptr; }
meta_format_handle FileFormatContext::GetFileFormatByName(api_handle, const char*) { return nullptr; }
api_bool FileFormatContext::GetFileFormatStatus(const_file_format_handle, char16_type*, size_type*, void*) { return api_false; }
api_bool FileFormatContext::GetImageDescription(const_file_format_handle, api_image_info*, api_image_options*, uint32) { return api_false; }
uint32 FileFormatContext::GetFileFormatVersion(const_meta_format_handle) { return 0; }
api_bool FileFormatContext::GetFileFormatMimeTypes(const_meta_format_handle, char**, size_type*, size_type*) { return api_false; }
bitmap_handle FileFormatContext::GetFileFormatSmallIcon(const_meta_format_handle) { return nullptr; }
meta_format_handle FileFormatContext::GetFileFormatByMimeType(api_handle, const char*, uint32, uint32) { return nullptr; }
api_bool FileFormatContext::GetFileFormatDescription(const_meta_format_handle, char16_type*, size_type*) { return api_false; }
void FileFormatContext::DisposeFormatSpecificData(const_meta_format_handle, const void*) {}
api_bool FileFormatContext::EditFileFormatPreferences(const_meta_format_handle) { return api_false; }
api_bool FileFormatContext::GetFileFormatCapabilities(const_meta_format_handle, api_format_capabilities*) { return api_false; }
api_bool FileFormatContext::ValidateFormatSpecificData(const_meta_format_handle, const void*) { return api_false; }
api_bool FileFormatContext::GetFileFormatFileExtensions(const_meta_format_handle, char16_type**, size_type*, size_type*) { return api_false; }
api_bool FileFormatContext::GetFileFormatImplementation(const_meta_format_handle, char16_type*, size_type*) { return api_false; }
meta_format_handle FileFormatContext::GetFileFormatByFileExtension(api_handle, const char16_type*, uint32, uint32) { return nullptr; }

// =============================================================
// Module/Process/Interface Definition - Stubs
// =============================================================

void ModuleDefinitionContext::SetModuleOnLoadRoutine(pcl::module_on_load_routine) {}
void ModuleDefinitionContext::SetModuleOnUnloadRoutine(pcl::module_on_unload_routine) {}
void ModuleDefinitionContext::SetModuleAllocationRoutine(pcl::module_allocation_routine) {}
void ModuleDefinitionContext::SetModuleDeallocationRoutine(pcl::module_deallocation_routine) {}
void ModuleDefinitionContext::ExitModuleDefinitionContext() {}
void ModuleDefinitionContext::EnterModuleDefinitionContext() {}

// ProcessDefinition - large stub section
void ProcessDefinitionContext::EnterProcessDefinitionContext() {}
api_bool ProcessDefinitionContext::IsProcessDefinitionContextActive() { return api_false; }
void ProcessDefinitionContext::BeginProcessDefinition(meta_process_handle, const char*) {}
api_bool ProcessDefinitionContext::GetProcessBeingDefined(char*, size_type*) { return api_false; }
void ProcessDefinitionContext::SetProcessCategory(const char*) {}
void ProcessDefinitionContext::SetProcessVersion(uint32) {}
void ProcessDefinitionContext::SetProcessAliasIdentifiers(const char*) {}
void ProcessDefinitionContext::SetProcessDescription(const char16_type*) {}
void ProcessDefinitionContext::SetProcessScriptComment(const char16_type*) {}
void ProcessDefinitionContext::SetProcessIconSVG(const char*) {}
void ProcessDefinitionContext::SetProcessIconSVGFile(const char16_type*) {}
void ProcessDefinitionContext::SetProcessIconImage(const char**) {}
void ProcessDefinitionContext::SetProcessIconImageFile(const char16_type*) {}
void ProcessDefinitionContext::SetProcessIconSmallImage(const char**) {}
void ProcessDefinitionContext::SetProcessIconSmallImageFile(const char16_type*) {}
void ProcessDefinitionContext::SetProcessClassInitializationRoutine(pcl::process_class_initialization_routine) {}
void ProcessDefinitionContext::SetProcessCreationRoutine(pcl::process_creation_routine) {}
void ProcessDefinitionContext::SetProcessDestructionRoutine(pcl::process_destruction_routine) {}
void ProcessDefinitionContext::SetProcessClonationRoutine(pcl::process_clonation_routine) {}
void ProcessDefinitionContext::SetProcessTestClonationRoutine(pcl::process_test_clonation_routine) {}
void ProcessDefinitionContext::SetProcessSetServerHandleRoutine(pcl::process_set_handle_routine) {}
void ProcessDefinitionContext::SetProcessAssignmentRoutine(pcl::process_assignment_routine) {}
void ProcessDefinitionContext::SetProcessInitializationRoutine(pcl::process_initialization_routine) {}
void ProcessDefinitionContext::SetProcessValidationRoutine(pcl::process_validation_routine) {}
void ProcessDefinitionContext::SetProcessCommandLineProcessingRoutine(pcl::process_command_line_processing_routine, uint32) {}
void ProcessDefinitionContext::SetProcessEditPreferencesRoutine(pcl::process_edit_preferences_routine) {}
void ProcessDefinitionContext::SetProcessBrowseDocumentationRoutine(pcl::process_browse_documentation_routine) {}
void ProcessDefinitionContext::SetProcessExecutionPreferencesRoutine(pcl::process_execution_preferences_routine) {}
void ProcessDefinitionContext::SetProcessExecutionValidationRoutine(pcl::process_execution_validation_routine) {}
void ProcessDefinitionContext::SetProcessMaskValidationRoutine(pcl::process_mask_validation_routine) {}
void ProcessDefinitionContext::SetProcessHistoryUpdateValidationRoutine(pcl::process_history_update_validation_routine) {}
void ProcessDefinitionContext::SetProcessUndoModeRoutine(pcl::process_undo_mode_routine) {}
void ProcessDefinitionContext::SetProcessPreExecutionRoutine(pcl::process_pre_execution_routine) {}
void ProcessDefinitionContext::SetProcessExecutionRoutine(pcl::process_execution_routine) {}
void ProcessDefinitionContext::SetProcessPostExecutionRoutine(pcl::process_post_execution_routine) {}
void ProcessDefinitionContext::SetProcessGlobalExecutionValidationRoutine(pcl::process_global_execution_validation_routine) {}
void ProcessDefinitionContext::SetProcessPreGlobalExecutionRoutine(pcl::process_pre_global_execution_routine) {}
void ProcessDefinitionContext::SetProcessGlobalExecutionRoutine(pcl::process_global_execution_routine) {}
void ProcessDefinitionContext::SetProcessPostGlobalExecutionRoutine(pcl::process_post_global_execution_routine) {}
void ProcessDefinitionContext::SetProcessImageExecutionValidationRoutine(pcl::process_image_execution_validation_routine) {}
void ProcessDefinitionContext::SetProcessImageExecutionRoutine(pcl::process_image_execution_routine) {}
void ProcessDefinitionContext::SetProcessDefaultInterfaceSelectionRoutine(pcl::process_default_interface_selection_routine) {}
void ProcessDefinitionContext::SetProcessInterfaceSelectionRoutine(pcl::process_interface_selection_routine) {}
void ProcessDefinitionContext::SetProcessInterfaceValidationRoutine(pcl::process_interface_validation_routine) {}
void ProcessDefinitionContext::SetProcessPreReadingRoutine(pcl::process_pre_reading_routine) {}
void ProcessDefinitionContext::SetProcessPostReadingRoutine(pcl::process_post_reading_routine) {}
void ProcessDefinitionContext::SetProcessPreWritingRoutine(pcl::process_pre_writing_routine) {}
void ProcessDefinitionContext::SetProcessPostWritingRoutine(pcl::process_post_writing_routine) {}
void ProcessDefinitionContext::SetProcessIPCStartRoutine(pcl::process_ipc_notification_routine) {}
void ProcessDefinitionContext::SetProcessIPCStopRoutine(pcl::process_ipc_notification_routine) {}
void ProcessDefinitionContext::SetProcessIPCSetParametersRoutine(pcl::process_ipc_notification_routine) {}
void ProcessDefinitionContext::SetProcessIPCGetStatusRoutine(pcl::process_ipc_status_routine) {}
void ProcessDefinitionContext::BeginParameterDefinition(meta_parameter_handle, const char*, uint32) {}
api_bool ProcessDefinitionContext::GetParameterBeingDefined(char*, size_type*) { return api_false; }
void ProcessDefinitionContext::SetParameterProcessVersionRange(uint32, uint32) {}
void ProcessDefinitionContext::SetParameterRequired(api_bool) {}
void ProcessDefinitionContext::SetParameterReadOnly(api_bool) {}
void ProcessDefinitionContext::SetParameterAliasIdentifiers(const char*) {}
void ProcessDefinitionContext::SetParameterDescription(const char16_type*) {}
void ProcessDefinitionContext::SetParameterScriptComment(const char16_type*) {}
void ProcessDefinitionContext::SetParameterLockRoutine(pcl::parameter_lock_routine) {}
void ProcessDefinitionContext::SetParameterUnlockRoutine(pcl::parameter_unlock_routine) {}
void ProcessDefinitionContext::SetParameterValidationRoutine(pcl::parameter_validation_routine) {}
void ProcessDefinitionContext::SetParameterAllocationRoutine(pcl::parameter_allocation_routine) {}
void ProcessDefinitionContext::SetParameterLengthQueryRoutine(pcl::parameter_length_query_routine) {}
void ProcessDefinitionContext::SetDefaultNumericValue(double) {}
void ProcessDefinitionContext::SetValidNumericRange(double, double) {}
void ProcessDefinitionContext::SetPrecision(int32) {}
void ProcessDefinitionContext::SetScientificNotation(api_bool) {}
void ProcessDefinitionContext::SetDefaultBooleanValue(api_bool) {}
void ProcessDefinitionContext::DefineEnumerationElement(const char*, api_enum) {}
void ProcessDefinitionContext::DefineEnumerationAlias(const char*, const char*) {}
void ProcessDefinitionContext::SetDefaultEnumerationValueIndex(uint32) {}
void ProcessDefinitionContext::SetDefaultStringValue(const char16_type*) {}
void ProcessDefinitionContext::SetStringAllowedCharacters(const char16_type*) {}
void ProcessDefinitionContext::SetStringLengthLimits(size_type, size_type) {}
void ProcessDefinitionContext::BeginTableColumnDefinition(meta_parameter_handle, const char*, uint32) {}
void ProcessDefinitionContext::EndTableColumnDefinition() {}
void ProcessDefinitionContext::SetTableRowLimits(size_type, size_type) {}
void ProcessDefinitionContext::SetBlockSizeLimits(size_type, size_type) {}
void ProcessDefinitionContext::EndParameterDefinition() {}
void ProcessDefinitionContext::EndProcessDefinition() {}
void ProcessDefinitionContext::ExitProcessDefinitionContext() {}

// InterfaceDefinition - stubs
void InterfaceDefinitionContext::EnterInterfaceDefinitionContext() {}
api_bool InterfaceDefinitionContext::IsInterfaceDefinitionContextActive() { return api_false; }
void InterfaceDefinitionContext::BeginInterfaceDefinition(meta_interface_handle, const char*, uint32) {}
api_bool InterfaceDefinitionContext::GetInterfaceBeingDefined(char*, size_type*) { return api_false; }
void InterfaceDefinitionContext::SetInterfaceVersion(uint32) {}
void InterfaceDefinitionContext::SetInterfaceAliasIdentifiers(const char*) {}
void InterfaceDefinitionContext::SetInterfaceDescription(const char16_type*) {}
void InterfaceDefinitionContext::SetInterfaceIconSVG(const char*) {}
void InterfaceDefinitionContext::SetInterfaceIconSVGFile(const char16_type*) {}
void InterfaceDefinitionContext::SetInterfaceIconImage(const char**) {}
void InterfaceDefinitionContext::SetInterfaceIconImageFile(const char16_type*) {}
void InterfaceDefinitionContext::SetInterfaceIconSmallImage(const char**) {}
void InterfaceDefinitionContext::SetInterfaceIconSmallImageFile(const char16_type*) {}
void InterfaceDefinitionContext::SetInterfaceFeatures(uint32, uint32) {}
// ... (many more interface definition stubs - add as needed)
void InterfaceDefinitionContext::EndInterfaceDefinition() {}
void InterfaceDefinitionContext::ExitInterfaceDefinitionContext() {}

// =============================================================
// External Process, Network Transfer, Graphics - All Stubs
// =============================================================

// ExternalProcess stubs
int32 ExternalProcess_ExecuteProgram(const char16_type*, const char16_type**, size_type) { return 0; }
api_bool ExternalProcess_StartProgram(const char16_type*, const char16_type**, size_type, const char16_type*, uint64*) { return api_false; }
external_process_handle ExternalProcess_CreateExternalProcess(api_handle, api_handle) { return nullptr; }

// NetworkTransfer stubs
network_transfer_handle NetworkTransfer_CreateNetworkTransfer(api_handle, api_handle) { return nullptr; }

// Graphics stubs
graphics_handle Graphics_CreateGraphics(api_handle) { return nullptr; }
api_bool Graphics_BeginControlPaint(graphics_handle, control_handle) { return api_false; }
api_bool Graphics_BeginBitmapPaint(graphics_handle, bitmap_handle) { return api_false; }

// SVG stubs
svg_handle SVG_CreateSVGFile(api_handle, const char16_type*, int32, int32, uint32) { return nullptr; }
svg_handle SVG_CreateSVGBuffer(api_handle, int32, int32, uint32) { return nullptr; }

// Brush stubs
brush_handle Brush_CreateBrush(api_handle, uint32, int32) { return nullptr; }
brush_handle Brush_CreateBitmapBrush(api_handle, const_bitmap_handle) { return nullptr; }

// Pen stubs
pen_handle Pen_CreatePen(api_handle, uint32, float, int32, int32, int32) { return nullptr; }
pen_handle Pen_ClonePen(api_handle, const_pen_handle) { return nullptr; }

// CodeEditor, WebView, ImageView stubs - add as needed
control_handle CodeEditor_CreateCodeEditor(api_handle, api_handle, control_handle, uint32) { return nullptr; }
control_handle WebViewContext::CreateWebView(api_handle, api_handle, control_handle, uint32) { return nullptr; }
control_handle ImageViewContext::CreateImageView(api_handle, api_handle, control_handle, uint32, int32, int32, int32, int32, api_bool, api_bool) { return nullptr; }

// =============================================================
// InterfaceDefinitionContext - Complete Stubs
// =============================================================

void InterfaceDefinitionContext::SetInterfaceInitializationRoutine(pcl::interface_initialization_routine) {}
void InterfaceDefinitionContext::SetInterfaceLaunchRoutine(pcl::interface_launch_routine) {}
void InterfaceDefinitionContext::SetInterfaceProcessInstantiationRoutine(pcl::interface_process_instantiation_routine) {}
void InterfaceDefinitionContext::SetInterfaceProcessTestInstantiationRoutine(pcl::interface_process_instantiation_routine) {}
void InterfaceDefinitionContext::SetInterfaceProcessValidationRoutine(pcl::interface_process_validation_routine) {}
void InterfaceDefinitionContext::SetInterfaceProcessImportRoutine(pcl::interface_process_import_routine) {}

void InterfaceDefinitionContext::SetInterfaceApplyRoutine(pcl::interface_control_routine) {}
void InterfaceDefinitionContext::SetInterfaceApplyGlobalRoutine(pcl::interface_control_routine) {}
void InterfaceDefinitionContext::SetInterfaceRealTimePreviewUpdatedRoutine(pcl::interface_control_state_routine) {}
void InterfaceDefinitionContext::SetInterfaceExecuteRoutine(pcl::interface_control_routine) {}
void InterfaceDefinitionContext::SetInterfaceCancelRoutine(pcl::interface_control_routine) {}
void InterfaceDefinitionContext::SetInterfaceBrowseDocumentationRoutine(pcl::interface_control_routine) {}
void InterfaceDefinitionContext::SetInterfaceTrackViewUpdatedRoutine(pcl::interface_control_state_routine) {}
void InterfaceDefinitionContext::SetInterfaceEditPreferencesRoutine(pcl::interface_control_routine) {}
void InterfaceDefinitionContext::SetInterfaceResetRoutine(pcl::interface_control_routine) {}

void InterfaceDefinitionContext::SetInterfaceRealTimeUpdateQueryRoutine(pcl::interface_real_time_update_query_routine) {}
void InterfaceDefinitionContext::SetInterfaceRealTimeGenerationFlagsRoutine(pcl::interface_real_time_generation_flags_routine) {}
void InterfaceDefinitionContext::SetInterfaceRealTimeGenerationRoutine(pcl::interface_real_time_generation_routine) {}
void InterfaceDefinitionContext::SetInterfaceRealTimeCancelRoutine(pcl::interface_real_time_cancel_routine) {}

void InterfaceDefinitionContext::SetInterfaceDynamicModeEnterRoutine(pcl::interface_dynamic_mode_enter_routine) {}
void InterfaceDefinitionContext::SetInterfaceDynamicModeExitRoutine(pcl::interface_dynamic_mode_exit_routine) {}
void InterfaceDefinitionContext::SetInterfaceDynamicMouseEnterRoutine(pcl::interface_dynamic_view_event_routine) {}
void InterfaceDefinitionContext::SetInterfaceDynamicMouseLeaveRoutine(pcl::interface_dynamic_view_event_routine) {}
void InterfaceDefinitionContext::SetInterfaceDynamicMouseMoveRoutine(pcl::interface_dynamic_mouse_routine) {}
void InterfaceDefinitionContext::SetInterfaceDynamicMousePressRoutine(pcl::interface_dynamic_mouse_button_routine) {}
void InterfaceDefinitionContext::SetInterfaceDynamicMouseReleaseRoutine(pcl::interface_dynamic_mouse_button_routine) {}
void InterfaceDefinitionContext::SetInterfaceDynamicMouseDoubleClickRoutine(pcl::interface_dynamic_mouse_routine) {}
void InterfaceDefinitionContext::SetInterfaceDynamicKeyPressRoutine(pcl::interface_dynamic_keyboard_routine) {}
void InterfaceDefinitionContext::SetInterfaceDynamicKeyReleaseRoutine(pcl::interface_dynamic_keyboard_routine) {}
void InterfaceDefinitionContext::SetInterfaceDynamicMouseWheelRoutine(pcl::interface_dynamic_wheel_routine) {}
void InterfaceDefinitionContext::SetInterfaceDynamicUpdateQueryRoutine(pcl::interface_dynamic_update_query_routine) {}
void InterfaceDefinitionContext::SetInterfaceDynamicPaintRoutine(pcl::interface_dynamic_paint_routine) {}

void InterfaceDefinitionContext::SetImageCreatedNotificationRoutine(pcl::image_notification_routine) {}
void InterfaceDefinitionContext::SetImageUpdatedNotificationRoutine(pcl::image_notification_routine) {}
void InterfaceDefinitionContext::SetImageRenamedNotificationRoutine(pcl::image_notification_routine) {}
void InterfaceDefinitionContext::SetImageDeletedNotificationRoutine(pcl::image_notification_routine) {}
void InterfaceDefinitionContext::SetImageFocusedNotificationRoutine(pcl::image_notification_routine) {}
void InterfaceDefinitionContext::SetImageLockedNotificationRoutine(pcl::image_notification_routine) {}
void InterfaceDefinitionContext::SetImageUnlockedNotificationRoutine(pcl::image_notification_routine) {}
void InterfaceDefinitionContext::SetImageSTFEnabledNotificationRoutine(pcl::image_notification_routine) {}
void InterfaceDefinitionContext::SetImageSTFDisabledNotificationRoutine(pcl::image_notification_routine) {}
void InterfaceDefinitionContext::SetImageSTFUpdatedNotificationRoutine(pcl::image_notification_routine) {}
void InterfaceDefinitionContext::SetImageRGBWSUpdatedNotificationRoutine(pcl::image_notification_routine) {}
void InterfaceDefinitionContext::SetImageCMEnabledNotificationRoutine(pcl::image_notification_routine) {}
void InterfaceDefinitionContext::SetImageCMDisabledNotificationRoutine(pcl::image_notification_routine) {}
void InterfaceDefinitionContext::SetImageCMUpdatedNotificationRoutine(pcl::image_notification_routine) {}
void InterfaceDefinitionContext::SetImageSavedNotificationRoutine(pcl::image_notification_routine) {}

void InterfaceDefinitionContext::SetMaskUpdatedNotificationRoutine(pcl::image_notification_routine) {}
void InterfaceDefinitionContext::SetMaskEnabledNotificationRoutine(pcl::image_notification_routine) {}
void InterfaceDefinitionContext::SetMaskDisabledNotificationRoutine(pcl::image_notification_routine) {}
void InterfaceDefinitionContext::SetMaskShownNotificationRoutine(pcl::image_notification_routine) {}
void InterfaceDefinitionContext::SetMaskHiddenNotificationRoutine(pcl::image_notification_routine) {}

void InterfaceDefinitionContext::SetTransparencyHiddenNotificationRoutine(pcl::image_notification_routine) {}
void InterfaceDefinitionContext::SetTransparencyModeUpdatedNotificationRoutine(pcl::image_notification_routine) {}

void InterfaceDefinitionContext::SetViewPropertyUpdatedNotificationRoutine(pcl::view_property_notification_routine) {}
void InterfaceDefinitionContext::SetViewPropertyDeletedNotificationRoutine(pcl::view_property_notification_routine) {}

void InterfaceDefinitionContext::SetBeginReadoutNotificationRoutine(pcl::image_notification_routine) {}
void InterfaceDefinitionContext::SetUpdateReadoutNotificationRoutine(pcl::readout_notification_routine) {}
void InterfaceDefinitionContext::SetEndReadoutNotificationRoutine(pcl::image_notification_routine) {}

void InterfaceDefinitionContext::SetProcessCreatedNotificationRoutine(pcl::process_notification_routine) {}
void InterfaceDefinitionContext::SetProcessUpdatedNotificationRoutine(pcl::process_notification_routine) {}
void InterfaceDefinitionContext::SetProcessDeletedNotificationRoutine(pcl::process_notification_routine) {}
void InterfaceDefinitionContext::SetProcessSavedNotificationRoutine(pcl::process_notification_routine) {}

void InterfaceDefinitionContext::SetRealTimePreviewOwnerChangeNotificationRoutine(pcl::interface_notification_routine) {}
void InterfaceDefinitionContext::SetRealTimePreviewLUTUpdatedNotificationRoutine(pcl::lut_notification_routine) {}
void InterfaceDefinitionContext::SetRealTimePreviewGenerationStartNotificationRoutine(pcl::global_notification_routine) {}
void InterfaceDefinitionContext::SetRealTimePreviewGenerationFinishNotificationRoutine(pcl::global_notification_routine) {}

void InterfaceDefinitionContext::SetGlobalRGBWSUpdatedNotificationRoutine(pcl::global_notification_routine) {}
void InterfaceDefinitionContext::SetGlobalCMEnabledNotificationRoutine(pcl::global_notification_routine) {}
void InterfaceDefinitionContext::SetGlobalCMDisabledNotificationRoutine(pcl::global_notification_routine) {}
void InterfaceDefinitionContext::SetGlobalCMUpdatedNotificationRoutine(pcl::global_notification_routine) {}
void InterfaceDefinitionContext::SetReadoutOptionsUpdatedNotificationRoutine(pcl::global_notification_routine) {}
void InterfaceDefinitionContext::SetGlobalPreferencesUpdatedNotificationRoutine(pcl::global_notification_routine) {}
void InterfaceDefinitionContext::SetGlobalFiltersUpdatedNotificationRoutine(pcl::global_notification_routine) {}

// =============================================================
// ImageWindowContext - Missing Stub
// =============================================================

api_bool ImageWindowContext::TerminateDynamicSession(uint32 flags)
{
    logf("[Mock] ImageWindowContext::TerminateDynamicSession");
    return api_true;
}

// =============================================================
// END OF IMPLEMENTATION
// =============================================================
