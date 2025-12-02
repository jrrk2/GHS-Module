// --------------------------------------------------------------
//  PCLMockAPI.cpp  —  Unified Mock PixInsight API for Testing
// --------------------------------------------------------------
//  All control types live in one registry (g_objects)
//  All handles are void*
//  All widgets/sizers use a single struct: MockBase
//  This implementation is stable & predictable for diagnostics.
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
#include <pcl/api/APIInterface.h>

static bool g_enableDebugLogging = false;

void SetDebugLogging( bool on )
{
   g_enableDebugLogging = on;
}

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

        QWidget* w = base->widget;

        // --- SHOW EVENT ------------------------------------------------------
        if (ev->type() == QEvent::Show)
        {
            if (base->onShow)
                base->onShow(base->pcl_handle,
                              base->pcl_handle);
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
                auto* e = static_cast<QKeyEvent*>(ev);
		/*
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

// Simple pointer-based hash/equality for handle types (control_handle etc.)
template <typename H>
struct HandleHash
{
   std::size_t operator()( const void *h ) const noexcept
   {
      auto p = reinterpret_cast<std::uintptr_t>( h );
      return std::hash<std::uintptr_t>{}( p );
   }
};
 
template <typename H>
struct HandleEqual
{
   bool operator()( const void *a, const void *b ) const noexcept
   {
      return a == b;
   }
};

static std::unordered_map<const void*, std::unique_ptr<MockBase>, HandleHash<control_handle>, HandleEqual<control_handle>> g_objects;

QList<MockBase*> g_topLevelWidgets;  // All candidates!

// =============================================================
// Utility: Lookup helper
// =============================================================
static inline MockBase* get(const void* h)
{
    if (!h)
        return nullptr;
    auto it = g_objects.find(h);
    return (it == g_objects.end()) ? nullptr : it->second.get();
}

static inline MockBase* get(const_control_handle h)
{
    if (!h)
        return nullptr;
    auto it = g_objects.find(h);
    return (it == g_objects.end()) ? nullptr : it->second.get();
}

//
// --- Helper functions for control/sizer/widget lookup ---
//

static QWidget* widgetFromHandle( control_handle h )
{
    if (!h)
        return nullptr;

    if (MockBase* b = get(h))
        return b->widget;

    return nullptr;
}

static QWidget* widgetFromHandle( const_control_handle h )
{
    if (!h)
        return nullptr;

    if (MockBase* b = get(h))
        return b->widget;

    return nullptr;
}

static QBoxLayout* layoutFromSizer( sizer_handle s )
{
    if (!s)
        return nullptr;

    if (MockBase* b = get(s))
        return (b->isSizer ? b->layout : nullptr);

    return nullptr;
}

static QBoxLayout* layoutFromSizer( const_sizer_handle s )
{
    return layoutFromSizer(const_cast<sizer_handle>(s));
}

static QTreeWidget* treeFromHandle( control_handle h )
{
    if (!h)
        return nullptr;

    if (MockBase* b = get(h))
        return qobject_cast<QTreeWidget*>(b->widget);

    return nullptr;
}

static QTreeWidget* treeFromHandle( const_control_handle h )
{
    return treeFromHandle(const_cast<control_handle>(h));
}

// =============================================================
// Utility: Logging
// =============================================================

static inline void logf(const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap); 
    fprintf(stderr, "\n");
    va_end(ap);
}

// =============================================================
//  NEW CONTROL CREATION TEMPLATE — FIXED
// =============================================================

QWidget* determineParentWidget(control_handle parent)
{
    if (!parent) {
        return nullptr;  // Top-level widget
    }
    
    MockBase* p = get(parent);
    if (!p) {
        return nullptr;
    }
    
    // Parent is a widget
    if (!p->isSizer && p->widget) {
        return p->widget;
    }
    
    // Parent is a layout - use layout's parent widget
    if (p->isSizer && p->layout) {
        return p->layout->parentWidget();  // May be nullptr, that's OK
    }
    
    return nullptr;
}

// Convert a MockBase sizer into a QLayout
static QLayout* toLayout(MockBase* b)
{
    if (!b) return nullptr;

    if (!b->layout)
    {
        if (b->vertical)
            b->layout = new QVBoxLayout();
        else
            b->layout = new QHBoxLayout();
    }
    return b->layout;
}

template <class T>
control_handle createControl(api_handle module, api_handle client, control_handle parent)
{
    QWidget* parentWidget = nullptr;
    if (parent)
    {
        auto* pb = reinterpret_cast<MockBase*>(parent);
        parentWidget = pb->widget;
    }

    // Create Qt widget
    T* w = new T(parentWidget);

    // Make a new MockBase object to track this
    auto* b = new MockBase();
    b->isSizer = false;
    b->moduleHandle = module;
    b->widget = w;

    // Save in map using control_handle as key
    control_handle h = reinterpret_cast<control_handle>(client);
    g_objects[h] = std::unique_ptr<MockBase>(b);

    logf("[Mock] CreateControl parent_handle=%p parentWidget=%p", parent, parentWidget);

    return h;
}

control_handle ControlContext::GetControlWindow(const_control_handle handle)
{
    MockBase* b = get((void*)handle);
    if (!b || !b->widget)
        return nullptr;

    QWidget* w = b->widget;
    QWidget* top = w->window();       // Qt's way to get the top-level owner

    // Find the MockBase corresponding to this top-level QWidget
    for (auto& kv : g_objects)
    {
        MockBase* obj = kv.second.get();
        if (obj->widget == top)
            return reinterpret_cast<control_handle>(obj);
    }

    // Fallback: return original handle if top cannot be mapped
    return (control_handle)handle;
}
  
// =============================================================
//  Sizer Creation
// =============================================================

sizer_handle SizerContext::CreateSizer( api_handle module, api_bool vertical )
{
    auto* b = new MockBase();
    b->isSizer      = true;
    b->moduleHandle = module;

    b->layout = vertical
        ? static_cast<QBoxLayout*>( new QVBoxLayout() )
        : static_cast<QBoxLayout*>( new QHBoxLayout() );

    sizer_handle h = reinterpret_cast<sizer_handle>( b );
    g_objects[h]   = std::unique_ptr<MockBase>( b );

    logf( "[Mock] CreateSizer vertical=%d handle=%p", vertical, h );
    return h;
}

// =============================================================
//  Control Creation Wrappers
// =============================================================

control_handle ControlContext::CreateControl(
    api_handle module,
    api_handle client,
    control_handle parent,
    uint32 flags )
{
    // client is actually pcl::Control* 
    control_handle pclCtrl = reinterpret_cast<control_handle>(client);

    MockBase* b = new MockBase();
    b->isSizer = false;
    b->moduleHandle = module;

    // RECORD backward mapping
    b->pcl_handle = pclCtrl;

    b->widget = new QWidget(nullptr);

    // STORE FOR LATER USE
    g_objects[pclCtrl] = std::unique_ptr<MockBase>(b);

    // Return the new PCL "handle"
    // This gets checked for non-null
    return pclCtrl;
}

control_handle LabelContext::CreateLabel( api_handle m, api_handle c, const char16_type*, control_handle parent, uint32 flags)
{
    return reinterpret_cast<label_handle>(
        createControl<QLabel>(m, c, parent));
}

control_handle (EditContext::CreateEdit)( api_handle m, api_handle c, const char16_type*, control_handle parent, uint32 flags )
{
    return reinterpret_cast<control_handle>(
        createControl<QLineEdit>(m, c, parent));
}

   control_handle (SliderContext::CreateSlider)( api_handle m, api_handle c, api_bool vertical, control_handle parent, uint32 flags )
{
    return reinterpret_cast<slider_handle>(
        createControl<QSlider>(m, c, parent));
}

control_handle (ButtonContext::CreateCheckBox)( api_handle m, api_handle c, const char16_type*, control_handle parent, uint32 flags )
{
    return reinterpret_cast<control_handle>(
        createControl<QCheckBox>(m, c, parent));
}

control_handle ( ComboBoxContext::CreateComboBox)( api_handle m, api_handle c, control_handle parent, uint32 flags )
{
    return reinterpret_cast<combo_handle>(
        createControl<QComboBox>(m, c, parent));
}

control_handle (SpinBoxContext::CreateSpinBox)( api_handle module, api_handle client, control_handle parent, uint32 flags )
{
    return (control_handle) createControl<QSpinBox>(module, client, parent);
}

// =============================================================
//  Sizer Insertion
// =============================================================

void SizerContext::InsertSizerControl(
    sizer_handle s, int32 index, control_handle c, int32 stretch, int32 /*flags*/)
{
    logf("[Mock][Sizer] InsertControl: parent=%p child=%p", s, c);
    MockBase* S = get(s);
    MockBase* C = get(c);
    if (!S || !C || !S->isSizer || !C->widget)
        return;

    QBoxLayout* layout = S->layout;   // or your existing QBoxLayout* field
    if (!layout)
        return;

    if (index < 0)
        layout->addWidget(C->widget, stretch);
    else
        layout->insertWidget(index, C->widget, stretch);
}

void SizerContext::InsertSizer(
    sizer_handle s, int32 index, sizer_handle child, int32 stretch)
{
    logf("[Mock][Sizer] InsertSizer: parent=%p child=%p", s, child);
 
    MockBase* S = get(s);
    MockBase* C = get(child);
    if (!S || !C || !S->isSizer || !C->isSizer)
        return;

    QBoxLayout* parentLayout = S->layout;
    QBoxLayout* childLayout  = C->layout;
    if (!parentLayout || !childLayout)
        return;

    if (index < 0)
        parentLayout->addLayout(childLayout, stretch);
    else
        parentLayout->insertLayout(index, childLayout, stretch);
}

// =============================================================
//  Attach Sizer to Control
// =============================================================

void (ControlContext::SetControlSizer)( control_handle ctrl, sizer_handle s )
{
    MockBase* C = get( ctrl );
    MockBase* S = get( s );

    if ( !S || !S->isSizer || !S->layout )
    {
        logf("[Mock][SetControlSizer] invalid sizer: ctrl=%p sizer=%p", ctrl, s);
        return;
    }

    // If there is no MockBase for ctrl yet, create one.
    if ( !C )
    {
        C = new MockBase();
        C->isSizer      = false;
        C->moduleHandle = nullptr;
        C->widget       = nullptr;
        C->layout       = nullptr;
        g_objects[ ctrl ] = std::unique_ptr<MockBase>( C );
    }

    // If this "control" has no widget, treat it as a top-level window.
    if ( !C->widget )
    {
        C->widget = new QWidget( nullptr );
        C->widget->setObjectName( "MockTopLevelWindow" );
        g_topLevelWidgets.append( C );

        logf("[Mock][SetControlSizer] created top-level container for ctrl=%p widget=%p",
             ctrl, C->widget );
    }

    QWidget* container = C->widget;
    QLayout* layout    = S->layout;

    logf("[Mock][SetControlSizer] ctrl=%p widget=%p sizer=%p layout=%p",
         ctrl, container, s, layout );

    container->setLayout( layout );
    layout->setParent( container );

    logf("[Mock][SetControlSizer] attached layout to container: layout parent=%p",
         layout->parent() );
}

// =============================================================
//  Visibility
// =============================================================
   void           (ControlContext::SetControlVisible)( control_handle h, api_bool visible)
{
    if (!h) {
        qWarning() << "[SetControlVisible] NULL handle - ignoring";
        return;
    }

    MockBase* C = get(h);

    if (!C || !C->widget)
        return ;

    if (visible)
      {
	C->widget->show();
	C->widget->raise();
      }
    else
      {
	C->widget->hide();
      }
}

// =============================================================
//  Generic control property setters
// =============================================================

inline int sanitizePCLWidth(int v, QWidget* w)
{
  int prev = w->sizeHint().width();
  if (prev <= 0) prev = 100;
  // case 1: unspecified
  if (v <= 0) return prev;

  // case 2: absurd PCL logical pixel or garbage
  if (v > 5000)          // PI often sends values like 152992 or 427520 or 1876937448
    return prev;

  // case 3: reasonable direct pixel size
  return v;
}

inline int sanitizePCLHeight(int v, QWidget* w)
{
  int prev = w->sizeHint().height();
  if (prev <= 0) prev = 0;
  if (v <= 0) return prev;
  return v;
}

   void           (ControlContext::SetControlFixedSize)( control_handle h, int32 w, int32 hgt)
{
    if (auto* C = get(h); C && C->widget) {
	logf("ControlContext::SetControlFixedSize (raw): %d, %d", w, hgt);
        int pxW = sanitizePCLWidth(w,   C->widget);
        int pxH = sanitizePCLHeight(hgt, C->widget);
	logf("ControlContext::SetControlFixedSize: %d, %d", pxW, pxH);
        C->widget->setFixedSize(pxW, pxH);
    }
}

void           (ControlContext::SetControlMinSize)( control_handle h, int32 w, int32 hgt)
{
    if (auto* C = get(h); C && C->widget) {
	logf("ControlContext::SetControlMinSize (raw): %d, %d", w, hgt);
        int pxW = sanitizePCLWidth(w,   C->widget);
        int pxH = sanitizePCLHeight(hgt, C->widget);
	logf("ControlContext::SetControlMinSize: %d, %d", pxW, pxH);
        C->widget->setMinimumSize(pxW, pxH);
    }
}

void ControlContext::SetWindowToolTip( control_handle h, const char16_type* t )
{
    if (auto* wdg = widgetFromHandle(h))
        wdg->setToolTip(QString::fromUtf16(t));
}

void ControlContext::SetWindowTitle( control_handle h, const char16_type* t )
{
    if (auto* wdg = widgetFromHandle(h))
        wdg->setWindowTitle(QString::fromUtf16(t));
}

void ControlContext::EnsureControlLayoutUpdated( control_handle h )
{
    if (auto* wdg = widgetFromHandle(h))
    {
        if (auto* l = wdg->layout())
        {
            l->invalidate();
            l->activate();
        }
        wdg->updateGeometry();
        wdg->update();
    }
}

void ControlContext::SetControlBackgroundColor(control_handle h, uint32)
{
    // Ignore for simplicity; implement if needed
}

api_bool ControlContext::SetKeyPressEventRoutine(
    control_handle h,
    api_handle /*client*/,
    pcl::keyboard_event_routine r)
{
    if (auto* b = get(h))
        b->onKeyPress = r;
    return api_true;
}

api_bool ControlContext::SetMousePressEventRoutine(
    control_handle h,
    api_handle /*client*/,
    pcl::mouse_button_event_routine r)
{
    if (auto* b = get(h))
        b->onMousePress = r;
    return api_true;
}

api_bool ControlContext::SetMouseReleaseEventRoutine(
    control_handle h,
    api_handle /*client*/,
    pcl::mouse_button_event_routine r)
{
    if (auto* b = get(h))
        b->onMouseRelease = r;
    return api_true;
}

api_bool ControlContext::SetMouseMoveEventRoutine(
    control_handle h,
    api_handle /*client*/,
    pcl::mouse_event_routine r)
{
    if (auto* b = get(h))
        b->onMouseMove = r;
    return api_true;
}

api_bool ControlContext::SetShowEventRoutine(
    control_handle h,
    api_handle /*client*/,
    pcl::control_event_routine r)
{
    if (auto* b = get(h))
        b->onShow = r;
    return api_true;
}

api_bool ControlContext::SetHideEventRoutine(
    control_handle h,
    api_handle /*client*/,
    pcl::control_event_routine r)
{
    if (auto* b = get(h))
        b->onShow = r;
    return api_true;
}

void           (ComboBoxContext::SetComboBoxEditEnabled)( control_handle h, api_bool editable)
{
    if (auto* C = get(h)) {
        if (auto* combo = qobject_cast<QComboBox*>(C->widget)) {
            combo->setEditable(editable);
        }
    }
}

// =============================================================
//  Edit / Slider / SpinBox Property Functions
// =============================================================
void SliderContext::SetSliderValue(control_handle h, int value)
{
    if (auto* C = get(h)) {
        if (auto* slider = qobject_cast<QSlider*>(C->widget)) {
            slider->setValue(value);
        }
    }
}

void SpinBoxContext::SetSpinBoxRange(control_handle h, int minv, int maxv)
{
    if (auto* C = get(h)) {
        if (auto* spin = qobject_cast<QSpinBox*>(C->widget)) {
            spin->setRange(minv, maxv);
        }
    }
}

void SpinBoxContext::SetSpinBoxValue(control_handle h, int value)
{
    if (auto* C = get(h)) {
        if (auto* spin = qobject_cast<QSpinBox*>(C->widget)) {
            spin->setValue(value);
        }
    }
}

// Add these implementations to PCLMockAPI.cpp

// ============================================================================
// Button Creation Functions
// ============================================================================

control_handle ButtonContext::CreatePushButton(api_handle m, api_handle c, 
                                           const char16_type* text, 
                                           const_bitmap_handle icon, 
                                           control_handle parent, 
                                           uint32 flags)
{
    auto* btn = reinterpret_cast<QPushButton*>(
        createControl<QPushButton>(m, c, parent));
    
    if (text) {
        QString qtext = QString::fromUtf16(text);
        reinterpret_cast<MockBase*>(btn)->widget->setProperty("text", qtext);
        qobject_cast<QPushButton*>(reinterpret_cast<MockBase*>(btn)->widget)
            ->setText(qtext);
    }
    
    logf("[Mock] CreatePushButton handle=%p text=%s", btn, 
         text ? QString::fromUtf16(text).toUtf8().constData() : "(null)");
    
    return reinterpret_cast<control_handle>(btn);
}

control_handle ButtonContext::CreateRadioButton(api_handle m, api_handle c, 
                                            const char16_type* text, 
                                            control_handle parent, 
                                            uint32 flags)
{
    auto* btn = reinterpret_cast<QRadioButton*>(
        createControl<QRadioButton>(m, c, parent));
    
    if (text) {
        QString qtext = QString::fromUtf16(text);
        qobject_cast<QRadioButton*>(reinterpret_cast<MockBase*>(btn)->widget)
            ->setText(qtext);
    }
    
    logf("[Mock] CreateRadioButton handle=%p", btn);
    return reinterpret_cast<control_handle>(btn);
}

control_handle ButtonContext::CreateToolButton(api_handle m, api_handle c, 
                                           const char16_type* text, 
                                           const_bitmap_handle icon, 
                                           api_bool checkable, 
                                           control_handle parent, 
                                           uint32 flags)
{
    auto* btn = reinterpret_cast<QToolButton*>(
        createControl<QToolButton>(m, c, parent));
    
    auto* mockBase = reinterpret_cast<MockBase*>(btn);
    auto* toolBtn = qobject_cast<QToolButton*>(mockBase->widget);
    
    if (text) {
        QString qtext = QString::fromUtf16(text);
        toolBtn->setText(qtext);
    }
    
    toolBtn->setCheckable(checkable);
    
    logf("[Mock] CreateToolButton handle=%p checkable=%d", btn, checkable);
    return reinterpret_cast<control_handle>(btn);
}

// ============================================================================
// Button Text Functions
// ============================================================================

api_bool ButtonContext::GetButtonText(const_control_handle h, 
                                  char16_type* text, 
                                  size_type* len)
{
    auto* C = get((h));
    if (!C || !C->widget) return api_false;
    
    QString qtext;
    if (auto* btn = qobject_cast<QAbstractButton*>(C->widget)) {
        qtext = btn->text();
    } else {
        return api_false;
    }
    
    if (len) *len = qtext.length();
    
    if (text && len && *len > 0) {
        const char16_type* src = reinterpret_cast<const char16_type*>(
            qtext.utf16());
        size_t copyLen = std::min(*len, static_cast<size_type>(qtext.length()));
        std::memcpy(text, src, copyLen * sizeof(char16_t));
        if (copyLen < *len) text[copyLen] = 0;
    }
    
    return api_true;
}

void ButtonContext::SetButtonText(control_handle h, const char16_type* text)
{
    auto* C = get(h);
    if (!C || !C->widget || !text) return;
    
    QString qtext = QString::fromUtf16(text);
    
    if (auto* btn = qobject_cast<QAbstractButton*>(C->widget)) {
        btn->setText(qtext);
        logf("[Mock] SetButtonText: %s", qtext.toUtf8().constData());
    }
}

// ============================================================================
// Button Icon Functions
// ============================================================================

bitmap_handle ButtonContext::GetButtonIcon(const_control_handle h)
{
    auto* C = get((h));
    if (!C || !C->widget) return nullptr;
    
    if (auto* btn = qobject_cast<QAbstractButton*>(C->widget)) {
        QIcon icon = btn->icon();
        if (!icon.isNull()) {
            // Return a simple handle - in real implementation would need proper management
            return reinterpret_cast<bitmap_handle>(new QIcon(icon));
        }
    }
    
    return nullptr;
}

void ButtonContext::SetButtonIcon(control_handle h, const_bitmap_handle icon)
{
    auto* C = get(h);
    if (!C || !C->widget) return;
    
    if (auto* btn = qobject_cast<QAbstractButton*>(C->widget)) {
      /*
        if (icon) {
            QIcon* qicon = reinterpret_cast<QIcon*>(
                (icon));
            btn->setIcon(*qicon);
        } else {
            btn->setIcon(QIcon());
        }
      */
        logf("[Mock] SetButtonIcon");
    }
}

void ButtonContext::GetButtonIconSize(const_control_handle h, int32* w, int32* h_out)
{
    auto* C = get((h));
    if (!C || !C->widget) return;
    
    if (auto* btn = qobject_cast<QAbstractButton*>(C->widget)) {
        QSize size = btn->iconSize();
        if (w) *w = size.width();
        if (h_out) *h_out = size.height();
    }
}

void ButtonContext::SetButtonIconSize(control_handle h, int32 w, int32 h_size)
{
    auto* C = get(h);
    if (!C || !C->widget) return;
    
    if (auto* btn = qobject_cast<QAbstractButton*>(C->widget)) {
        btn->setIconSize(QSize(w, h_size));
        logf("[Mock] SetButtonIconSize: %d x %d", w, h_size);
    }
}

// ============================================================================
// Button State Functions
// ============================================================================

api_bool ButtonContext::GetButtonPushed(const_control_handle h)
{
    auto* C = get((h));
    if (!C || !C->widget) return api_false;
    
    if (auto* btn = qobject_cast<QAbstractButton*>(C->widget)) {
        return btn->isDown() ? api_true : api_false;
    }
    
    return api_false;
}

void ButtonContext::SetButtonPushed(control_handle h, api_bool pushed)
{
    auto* C = get(h);
    if (!C || !C->widget) return;
    
    if (auto* btn = qobject_cast<QAbstractButton*>(C->widget)) {
        btn->setDown(pushed);
        logf("[Mock] SetButtonPushed: %d", pushed);
    }
}

uint32 ButtonContext::GetButtonChecked(const_control_handle h)
{
    auto* C = get((h));
    if (!C || !C->widget) return 0;
    
    // Try QCheckBox first (supports tristate)
    if (auto* checkbox = qobject_cast<QCheckBox*>(C->widget)) {
        Qt::CheckState state = checkbox->checkState();
        return static_cast<uint32>(state); // 0=unchecked, 1=partial, 2=checked
    }
    
    // Fallback to generic QAbstractButton (binary checked state)
    if (auto* btn = qobject_cast<QAbstractButton*>(C->widget)) {
        if (btn->isCheckable()) {
            return btn->isChecked() ? 2 : 0; // 0=unchecked, 2=checked
        }
    }
    
    return 0;
}

void ButtonContext::SetButtonChecked(control_handle h, uint32 state)
{
    auto* C = get(h);
    if (!C || !C->widget) return;
    
    // Try QCheckBox first (supports tristate)
    if (auto* checkbox = qobject_cast<QCheckBox*>(C->widget)) {
        checkbox->setCheckState(static_cast<Qt::CheckState>(state));
        logf("[Mock] SetButtonChecked (QCheckBox): %u", state);
        return;
    }
    
    // Fallback to generic QAbstractButton (binary checked state)
    if (auto* btn = qobject_cast<QAbstractButton*>(C->widget)) {
        btn->setChecked(state != 0);
        logf("[Mock] SetButtonChecked (QAbstractButton): %u", state);
    }
}

// ============================================================================
// Button Properties
// ============================================================================

api_bool ButtonContext::GetButtonDefaultEnabled(const_control_handle h)
{
    auto* C = get((h));
    if (!C || !C->widget) return api_false;
    
    if (auto* btn = qobject_cast<QPushButton*>(C->widget)) {
        return btn->isDefault() ? api_true : api_false;
    }
    
    return api_false;
}

void ButtonContext::SetButtonDefaultEnabled(control_handle h, api_bool enabled)
{
    auto* C = get(h);
    if (!C || !C->widget) return;
    
    if (auto* btn = qobject_cast<QPushButton*>(C->widget)) {
        btn->setDefault(enabled);
        logf("[Mock] SetButtonDefaultEnabled: %d", enabled);
    }
}

api_bool ButtonContext::GetButtonTristateEnabled(const_control_handle h)
{
    auto* C = get((h));
    if (!C || !C->widget) return api_false;
    
    if (auto* btn = qobject_cast<QCheckBox*>(C->widget)) {
        return btn->isTristate() ? api_true : api_false;
    }
    
    return api_false;
}

void ButtonContext::SetButtonTristateEnabled(control_handle h, api_bool enabled)
{
    auto* C = get(h);
    if (!C || !C->widget) return;
    
    if (auto* btn = qobject_cast<QCheckBox*>(C->widget)) {
        btn->setTristate(enabled);
        logf("[Mock] SetButtonTristateEnabled: %d", enabled);
    }
}

api_bool ButtonContext::GetToolButtonCheckable(const_control_handle h)
{
    auto* C = get((h));
    if (!C || !C->widget) return api_false;
    
    if (auto* btn = qobject_cast<QToolButton*>(C->widget)) {
        return btn->isCheckable() ? api_true : api_false;
    }
    
    return api_false;
}

void ButtonContext::SetToolButtonCheckable(control_handle h, api_bool checkable)
{
    auto* C = get(h);
    if (!C || !C->widget) return;
    
    if (auto* btn = qobject_cast<QToolButton*>(C->widget)) {
        btn->setCheckable(checkable);
        logf("[Mock] SetToolButtonCheckable: %d", checkable);
    }
}

// ============================================================================
// Button Event Handlers (Stubs)
// ============================================================================

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
    if (auto* b = get(h))
    {
      b->eventReceiver = reinterpret_cast<control_handle>(receiver);
        b->onButtonClick = r;
        if (auto* w = widgetFromHandle(h))
        {
	  if (auto* pb = qobject_cast<QAbstractButton*>(w)) if (b->pcl_handle)
            {
                QObject::connect(pb, &QAbstractButton::clicked, [b](bool checked){
                    if (b->onButtonClick)
                    {
                        b->onButtonClick(
                            b->pcl_handle,
                            b->eventReceiver,
                            checked ? api_true : api_false);
                    }
                });
            }
        }
    }
    return api_true;
}

api_bool ButtonContext::SetButtonCheckEventRoutine(
    control_handle h,
    api_handle /*client*/,
    pcl::button_check_event_routine r)
{
    if (auto* b = get(h))
    {
        b->onButtonCheck = r;
        if (auto* w = widgetFromHandle(h))
        {
            if (auto* cb = qobject_cast<QCheckBox*>(w))
            {
                QObject::connect(cb, &QCheckBox::stateChanged, [b](int st){
                    if (b->onButtonCheck)
                    {
                        b->onButtonCheck(
                            reinterpret_cast<control_handle>(b),
                            reinterpret_cast<control_handle>(b),
                            st);
                    }
                });
            }
        }
    }
    return api_true;
}

// ============================================================================
// UI Object Management Functions
// ============================================================================

api_bool UIContext::AttachToUIObject(api_handle object, api_handle client)
{
    if (!object) return api_false;
    
    auto* C = get(object);
    if (!C) return api_false;
    
    // In a real implementation, this would increment a reference count
    // For the mock, we just log the attachment
    logf("[Mock] UIContext::AttachToUIObject: object=%p client=%p", object, client);
    
    return api_true;
}

api_bool UIContext::DetachFromUIObject(api_handle object, api_handle client)
{
    if (!object) return api_true;
    
    auto* C = get(object);
    if (!C) return api_true;
    
    // In a real implementation, this would decrement reference count
    // and potentially destroy the object if count reaches zero
    logf("[Mock] UIContext::DetachFromUIObject: object=%p client=%p", object, client);
    
    return api_true;
}

api_handle UIContext::GetUIObjectModule(const_api_handle object)
{
    if (!object) return nullptr;
    
    auto* C = get((object));
    if (!C) return nullptr;
    
    return C->moduleHandle;
}

size_type UIContext::GetUIObjectRefCount(const_api_handle object)
{
    if (!object) return 0;
    
    auto* C = get((object));
    if (!C) return 0;
    
    // In a real implementation, return actual reference count
    // For the mock, we return 1 to indicate the object exists
    return 1;
}

api_bool UIContext::GetUIObjectType(const_api_handle object, 
                                char* type, 
                                size_type* len)
{
    if (!object) return api_false;
    
    auto* C = get((object));
    if (!C) return api_false;
    
    const char* typeName = "Control";
    
    if (C->isSizer) {
        typeName = "Sizer";
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
    
    auto* C = get((object));
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
    
    auto* C = get(object);
    if (!C || !C->widget) return api_false;
    
    QString qid = QString::fromUtf16(id);
    C->widget->setObjectName(qid);
    
    logf("[Mock] UIContext::SetUIObjectId: %s", qid.toUtf8().constData());
    
    return api_true;
}

api_bool UIContext::SetHandleDestroyedEventRoutine(api_handle object,
                                               pcl::destroy_event_routine routine)
{
    if (!object) return api_false;
    
    // In a real implementation, this would register a callback
    // to be invoked when the object is destroyed
    logf("[Mock] UIContext::SetHandleDestroyedEventRoutine: object=%p", object);
    
    return api_true;
}

// ============================================================================
// UI Object Management Functions
// ============================================================================

api_bool UIContext::AttachToUIControlObject(api_handle object, api_handle client)
{
    if (!object) return api_false;
    
    auto* C = get(object);
    if (!C) return api_false;
    
    // In a real implementation, this would increment a reference count
    // For the mock, we just log the attachment
    logf("[Mock] UIContext::AttachToUIControlObject: object=%p client=%p", object, client);
    
    return api_true;
}

api_bool UIContext::DetachFromUIControlObject(api_handle object, api_handle client)
{
    if (!object) return api_true;
    
    auto* C = get(object);
    if (!C) return api_true;
    
    // In a real implementation, this would decrement reference count
    // and potentially destroy the object if count reaches zero
    logf("[Mock] UIContext::DetachFromUIControlObject: object=%p client=%p", object, client);
    
    return api_true;
}

api_handle UIContext::GetUIControlObjectModule(const_api_handle object)
{
    if (!object) return nullptr;
    
    auto* C = get((object));
    if (!C) return nullptr;
    
    return C->moduleHandle;
}

size_type UIContext::GetUIControlObjectRefCount(const_api_handle object)
{
    if (!object) return 0;
    
    auto* C = get((object));
    if (!C) return 0;
    
    // In a real implementation, return actual reference count
    // For the mock, we return 1 to indicate the object exists
    return 1;
}

api_bool UIContext::GetUIControlObjectType(const_api_handle object, 
                                char* type, 
                                size_type* len)
{
    if (!object) return api_false;
    
    auto* C = get((object));
    if (!C) return api_false;
    
    const char* typeName = "Control";
    
    if (C->isSizer) {
        typeName = "Sizer";
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

api_bool UIContext::GetUIControlObjectId(const_api_handle object, 
                              char16_type* id, 
                              size_type* len)
{
    if (!object) return api_false;
    
    auto* C = get((object));
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

api_bool UIContext::SetUIControlObjectId(api_handle object, const char16_type* id)
{
    if (!object || !id) return api_false;
    
    auto* C = get(object);
    if (!C || !C->widget) return api_false;
    
    QString qid = QString::fromUtf16(id);
    C->widget->setObjectName(qid);
    
    logf("[Mock] UIContext::SetUIControlObjectId: %s", qid.toUtf8().constData());
    
    return api_true;
}

// ============================================================================
// BitmapBox API Implementation
// ============================================================================
// Add to PCLMockAPI.cpp

control_handle BitmapBoxContext::CreateBitmapBox(api_handle module,
						 api_handle client,
                                             const_bitmap_handle bitmap,
                                             control_handle parent,
                                             uint32 flags)
{
    auto* box = reinterpret_cast<QLabel*>(
        createControl<QLabel>(module, client, parent));
    
    auto* mockBase = reinterpret_cast<MockBase*>(box);
    auto* label = qobject_cast<QLabel*>(mockBase->widget);
    
    if (bitmap && label) {
        QPixmap* pixmap = reinterpret_cast<QPixmap*>(
            const_cast<void*>(bitmap));
        label->setPixmap(*pixmap);
        label->setScaledContents(false);
    }
    
    logf("[Mock] CreateBitmapBox handle=%p", box);
    return reinterpret_cast<control_handle>(box);
}

bitmap_handle BitmapBoxContext::GetBitmapBoxBitmap(const_control_handle h)
{
    auto* C = get((h));
    if (!C || !C->widget) return nullptr;
    
    if (auto* label = qobject_cast<QLabel*>(C->widget)) {
        const QPixmap* pm = label->pixmap();
        if (pm && !pm->isNull()) {
            return reinterpret_cast<bitmap_handle>(
                new QPixmap(*pm));
        }
    }
    
    return nullptr;
}

void BitmapBoxContext::SetBitmapBoxBitmap(control_handle h,
                                      const_bitmap_handle bitmap)
{
    auto* C = get(h);
    if (!C || !C->widget) return;
    
    if (auto* label = qobject_cast<QLabel*>(C->widget)) {
        if (bitmap) {
            const QPixmap* pixmap = reinterpret_cast<const QPixmap*>((bitmap));
            label->setPixmap(*pixmap);
        } else {
            label->clear();
        }
        logf("[Mock] SetBitmapBoxBitmap");
    }
}

int32 BitmapBoxContext::GetBitmapBoxMargin(const_control_handle h)
{
    auto* C = get((h));
    if (!C || !C->widget) return 0;
    
    if (auto* label = qobject_cast<QLabel*>(C->widget)) {
        return label->margin();
    }
    
    return 0;
}

void BitmapBoxContext::SetBitmapBoxMargin(control_handle h, int32 margin)
{
    auto* C = get(h);
    if (!C || !C->widget) return;
    
    if (auto* label = qobject_cast<QLabel*>(C->widget)) {
        label->setMargin(margin);
        logf("[Mock] SetBitmapBoxMargin: %d", margin);
    }
}

api_bool BitmapBoxContext::GetBitmapBoxAutoFitEnabled(const_control_handle h)
{
    auto* C = get((h));
    if (!C || !C->widget) return api_false;
    
    if (auto* label = qobject_cast<QLabel*>(C->widget)) {
        return label->hasScaledContents() ? api_true : api_false;
    }
    
    return api_false;
}

void BitmapBoxContext::SetBitmapBoxAutoFitEnabled(control_handle h, api_bool enabled)
{
    auto* C = get(h);
    if (!C || !C->widget) return;
    
    if (auto* label = qobject_cast<QLabel*>(C->widget)) {
        label->setScaledContents(enabled);
        logf("[Mock] SetBitmapBoxAutoFitEnabled: %d", enabled);
    }
}

// ============================================================================
// Bitmap Creation Functions
// ============================================================================

bitmap_handle BitmapContext::CreateBitmap(api_handle module,
                                      int32 width,
                                      int32 height,
                                      void* data)
{
    QPixmap* pixmap = new QPixmap(width, height);
    
    if (data) {
        // Assume data is ARGB32 format
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
        logf("[Mock] CreateBitmapFromFile FAILED: %s",
             path.toUtf8().constData());	
        return BitmapContext::CreateBitmap(module, 100, 100, 0);
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
    
    const QPixmap* src = reinterpret_cast<const QPixmap*>((source));
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
    
    const QPixmap* src = reinterpret_cast<const QPixmap*>((source));
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

// ============================================================================
// Bitmap Properties
// ============================================================================

int32 BitmapContext::GetBitmapFormat(bitmap_handle h)
{
    if (!h) return 0;
    
    const QPixmap* pixmap = reinterpret_cast<const QPixmap*>(h);
    return pixmap->depth();
}

void BitmapContext::SetBitmapFormat(bitmap_handle h, int32 format)
{
    // In Qt, format conversion requires creating a new image
    // For simplicity, we log but don't implement conversion
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
    
    const QPixmap* pixmap = reinterpret_cast<const QPixmap*>((h));
    
    if (width) *width = pixmap->width();
    if (height) *height = pixmap->height();
    
    return api_true;
}

api_bool BitmapContext::IsEmptyBitmap(const_bitmap_handle h)
{
    if (!h) return api_true;
    
    const QPixmap* pixmap = reinterpret_cast<const QPixmap*>((h));
    return pixmap->isNull() ? api_true : api_false;
}

uint32 BitmapContext::GetBitmapPixel(const_bitmap_handle h, int32 x, int32 y)
{
    if (!h) return 0;
    
    const QPixmap* pixmap = reinterpret_cast<const QPixmap*>((h));
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

// ============================================================================
// Bitmap Transformations
// ============================================================================

bitmap_handle BitmapContext::MirroredBitmap(const_bitmap_handle h,
                                        api_bool horizontal,
                                        api_bool vertical)
{
    if (!h) return nullptr;
    
    const QPixmap* src = reinterpret_cast<const QPixmap*>((h));
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
    
    const QPixmap* src = reinterpret_cast<const QPixmap*>((h));
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
    
    const QPixmap* src = reinterpret_cast<const QPixmap*>((h));
    QTransform transform;
    transform.rotate(angle);
    
    Qt::TransformationMode mode = smooth ?
        Qt::SmoothTransformation : Qt::FastTransformation;
    
    QPixmap* result = new QPixmap(src->transformed(transform, mode));
    
    logf("[Mock] RotatedBitmap: angle=%.2f smooth=%d", angle, smooth);
    return reinterpret_cast<bitmap_handle>(result);
}

// ============================================================================
// Bitmap I/O
// ============================================================================

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
    
    const QPixmap* pixmap = reinterpret_cast<const QPixmap*>((h));
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
    
    QPixmap* pixmap = reinterpret_cast< QPixmap*>(h);
    QByteArray bytes(static_cast<const char*>(data), size);
    
    bool result = pixmap->loadFromData(bytes, format);
    
    logf("[Mock] LoadBitmapData: format=%s size=%zu %s",
         format ? format : "auto", size, result ? "OK" : "FAILED");
    
    return result ? api_true : api_false;
}

// ============================================================================
// Bitmap Drawing Operations
// ============================================================================

void BitmapContext::CopyBitmap(bitmap_handle dest,
                           int32 xDst, int32 yDst,
                           const_bitmap_handle src,
                           int32 xSrc, int32 ySrc,
                           int32 width, int32 height)
{
    if (!dest || !src) return;
    
    QPixmap* dstPixmap = reinterpret_cast<QPixmap*>(dest);
    const QPixmap* srcPixmap = reinterpret_cast<const QPixmap*>((src));
    
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

void BitmapContext::OrBitmap(bitmap_handle h,
                         int32 x, int32 y,
                         int32 width, int32 height,
                         uint32 rgba)
{
    if (!h) return;
    
    QPixmap* pixmap = reinterpret_cast<QPixmap*>(h);
    QImage img = pixmap->toImage();
    
    for (int32 dy = 0; dy < height && (y + dy) < img.height(); ++dy) {
        for (int32 dx = 0; dx < width && (x + dx) < img.width(); ++dx) {
            int px = x + dx, py = y + dy;
            if (px >= 0 && py >= 0) {
                uint32 pixel = img.pixel(px, py);
                img.setPixel(px, py, pixel | rgba);
            }
        }
    }
    
    *pixmap = QPixmap::fromImage(img);
    logf("[Mock] OrBitmap");
}

void BitmapContext::OrBitmaps(bitmap_handle dest,
                          int32 xDst, int32 yDst,
                          const_bitmap_handle src,
                          int32 xSrc, int32 ySrc,
                          int32 width, int32 height)
{
    if (!dest || !src) return;
    
    QPixmap* dstPixmap = reinterpret_cast<QPixmap*>(dest);
    const QPixmap* srcPixmap = reinterpret_cast<const QPixmap*>((src));
    
    QImage dstImg = dstPixmap->toImage();
    QImage srcImg = srcPixmap->toImage();
    
    for (int32 dy = 0; dy < height; ++dy) {
        for (int32 dx = 0; dx < width; ++dx) {
            int sx = xSrc + dx, sy = ySrc + dy;
            int tx = xDst + dx, ty = yDst + dy;
            
            if (sx >= 0 && sx < srcImg.width() && sy >= 0 && sy < srcImg.height() &&
                tx >= 0 && tx < dstImg.width() && ty >= 0 && ty < dstImg.height()) {
                uint32 srcPixel = srcImg.pixel(sx, sy);
                uint32 dstPixel = dstImg.pixel(tx, ty);
                dstImg.setPixel(tx, ty, dstPixel | srcPixel);
            }
        }
    }
    
    *dstPixmap = QPixmap::fromImage(dstImg);
    logf("[Mock] OrBitmaps");
}

void BitmapContext::AndBitmap(bitmap_handle h,
                          int32 x, int32 y,
                          int32 width, int32 height,
                          uint32 rgba)
{
    if (!h) return;
    
    QPixmap* pixmap = reinterpret_cast<QPixmap*>(h);
    QImage img = pixmap->toImage();
    
    for (int32 dy = 0; dy < height && (y + dy) < img.height(); ++dy) {
        for (int32 dx = 0; dx < width && (x + dx) < img.width(); ++dx) {
            int px = x + dx, py = y + dy;
            if (px >= 0 && py >= 0) {
                uint32 pixel = img.pixel(px, py);
                img.setPixel(px, py, pixel & rgba);
            }
        }
    }
    
    *pixmap = QPixmap::fromImage(img);
    logf("[Mock] AndBitmap");
}

void BitmapContext::AndBitmaps(bitmap_handle dest,
                           int32 xDst, int32 yDst,
                           const_bitmap_handle src,
                           int32 xSrc, int32 ySrc,
                           int32 width, int32 height)
{
    if (!dest || !src) return;
    
    QPixmap* dstPixmap = reinterpret_cast<QPixmap*>(dest);
    const QPixmap* srcPixmap = reinterpret_cast<const QPixmap*>((src));
    
    QImage dstImg = dstPixmap->toImage();
    QImage srcImg = srcPixmap->toImage();
    
    for (int32 dy = 0; dy < height; ++dy) {
        for (int32 dx = 0; dx < width; ++dx) {
            int sx = xSrc + dx, sy = ySrc + dy;
            int tx = xDst + dx, ty = yDst + dy;
            
            if (sx >= 0 && sx < srcImg.width() && sy >= 0 && sy < srcImg.height() &&
                tx >= 0 && tx < dstImg.width() && ty >= 0 && ty < dstImg.height()) {
                uint32 srcPixel = srcImg.pixel(sx, sy);
                uint32 dstPixel = dstImg.pixel(tx, ty);
                dstImg.setPixel(tx, ty, dstPixel & srcPixel);
            }
        }
    }
    
    *dstPixmap = QPixmap::fromImage(dstImg);
    logf("[Mock] AndBitmaps");
}

void BitmapContext::XorBitmap(bitmap_handle h,
                          int32 x, int32 y,
                          int32 width, int32 height,
                          uint32 rgba)
{
    if (!h) return;
    
    QPixmap* pixmap = reinterpret_cast<QPixmap*>(h);
    QImage img = pixmap->toImage();
    
    for (int32 dy = 0; dy < height && (y + dy) < img.height(); ++dy) {
        for (int32 dx = 0; dx < width && (x + dx) < img.width(); ++dx) {
            int px = x + dx, py = y + dy;
            if (px >= 0 && py >= 0) {
                uint32 pixel = img.pixel(px, py);
                img.setPixel(px, py, pixel ^ rgba);
            }
        }
    }
    
    *pixmap = QPixmap::fromImage(img);
    logf("[Mock] XorBitmap");
}

void BitmapContext::XorBitmaps(bitmap_handle dest,
                           int32 xDst, int32 yDst,
                           const_bitmap_handle src,
                           int32 xSrc, int32 ySrc,
                           int32 width, int32 height)
{
    if (!dest || !src) return;
    
    QPixmap* dstPixmap = reinterpret_cast<QPixmap*>(dest);
    const QPixmap* srcPixmap = reinterpret_cast<const QPixmap*>((src));
    
    QImage dstImg = dstPixmap->toImage();
    QImage srcImg = srcPixmap->toImage();
    
    for (int32 dy = 0; dy < height; ++dy) {
        for (int32 dx = 0; dx < width; ++dx) {
            int sx = xSrc + dx, sy = ySrc + dy;
            int tx = xDst + dx, ty = yDst + dy;
            
            if (sx >= 0 && sx < srcImg.width() && sy >= 0 && sy < srcImg.height() &&
                tx >= 0 && tx < dstImg.width() && ty >= 0 && ty < dstImg.height()) {
                uint32 srcPixel = srcImg.pixel(sx, sy);
                uint32 dstPixel = dstImg.pixel(tx, ty);
                dstImg.setPixel(tx, ty, dstPixel ^ srcPixel);
            }
        }
    }
    
    *dstPixmap = QPixmap::fromImage(dstImg);
    logf("[Mock] XorBitmaps");
}

void BitmapContext::XorBitmapRect(bitmap_handle h,
                              int32 x, int32 y,
                              int32 width, int32 height,
                              uint32 rgba)
{
    // Same as XorBitmap
    BitmapContext::XorBitmap(h, x, y, width, height, rgba);
}

void BitmapContext::ReplaceBitmapColor(bitmap_handle h,
                                   int32 x, int32 y,
                                   int32 width, int32 height,
                                   uint32 oldColor,
                                   uint32 newColor)
{
    if (!h) return;
    
    QPixmap* pixmap = reinterpret_cast<QPixmap*>(h);
    QImage img = pixmap->toImage();
    
    for (int32 dy = 0; dy < height && (y + dy) < img.height(); ++dy) {
        for (int32 dx = 0; dx < width && (x + dx) < img.width(); ++dx) {
            int px = x + dx, py = y + dy;
            if (px >= 0 && py >= 0) {
                if (img.pixel(px, py) == oldColor) {
                    img.setPixel(px, py, newColor);
                }
            }
        }
    }
    
    *pixmap = QPixmap::fromImage(img);
    logf("[Mock] ReplaceBitmapColor: 0x%08x -> 0x%08x", oldColor, newColor);
}

void BitmapContext::SetBitmapAlpha(bitmap_handle h,
                               int32 x, int32 y,
                               int32 width, int32 height,
                               uint8 alpha)
{
    if (!h) return;
    
    QPixmap* pixmap = reinterpret_cast<QPixmap*>(h);
    QImage img = pixmap->toImage().convertToFormat(QImage::Format_ARGB32);
    
    for (int32 dy = 0; dy < height && (y + dy) < img.height(); ++dy) {
        for (int32 dx = 0; dx < width && (x + dx) < img.width(); ++dx) {
            int px = x + dx, py = y + dy;
            if (px >= 0 && py >= 0) {
                QColor color(img.pixel(px, py));
                color.setAlpha(alpha);
                img.setPixel(px, py, color.rgba());
            }
        }
    }
    
    *pixmap = QPixmap::fromImage(img);
    logf("[Mock] SetBitmapAlpha: alpha=%d", alpha);
}

// ============================================================================
// Bitmap Device Pixel Ratio
// ============================================================================

void BitmapContext::GetBitmapDevicePixelRatio(const_bitmap_handle h, double* ratio)
{
    if (!h || !ratio) return;
    
    const QPixmap* pixmap = reinterpret_cast<const QPixmap*>((h));
    *ratio = pixmap->devicePixelRatio();
}

void BitmapContext::SetBitmapDevicePixelRatio(bitmap_handle h, double ratio)
{
    if (!h) return;
    
    QPixmap* pixmap = reinterpret_cast<QPixmap*>(h);
    pixmap->setDevicePixelRatio(ratio);
    
    logf("[Mock] SetBitmapDevicePixelRatio: %.2f", ratio);
}

// =============================================================
//  No-op stubs for unused API areas
// =============================================================
void ControlContext::SetChildControlToFocus(control_handle, control_handle) { return; }
void ControlContext::SetControlFocusStyle(control_handle, int32) { return ; }
api_bool EditContext::SetEditCompletedEventRoutine(control_handle, api_handle, pcl::event_routine) { return api_true; }
api_bool EditContext::SetReturnPressedEventRoutine(control_handle, api_handle, pcl::event_routine) { return api_true; }
api_bool SliderContext::SetSliderValueUpdatedEventRoutine(control_handle, api_handle, pcl::value_event_routine) { return api_true; }
api_bool SpinBoxContext::SetSpinBoxValueUpdatedEventRoutine(control_handle, api_handle, pcl::value_event_routine) { return api_true; }

api_bool ImageWindowContext::LoadImageWindows(const char16_type* url, const char* id, const char* hints, api_bool asACopy, api_bool allowMessages, pcl::window_enumeration_callback, void*)
{

  abort();
}

api_bool ImageWindowContext::CloseImageWindow(window_handle, api_bool force)
{

  abort();
}

window_handle ImageWindowContext::GetImageWindowById(const char*)
{

  abort();
}

window_handle ImageWindowContext::GetImageWindowByFilePath(const char16_type*)
{

  abort();
}

window_handle ImageWindowContext::GetActiveImageWindow()
{
    QWidget* w = QApplication::activeWindow();
    if (!w)
        return nullptr;

    return reinterpret_cast<window_handle>(w);
}
void ImageWindowContext::EnumerateImageWindows(pcl::window_enumeration_callback, void*, api_bool includeIconic)
{

  abort();
}
void ImageWindowContext::EnumeratePreviews(const_window_handle, pcl::view_enumeration_callback, void*)
{

  abort();
}
api_bool ImageWindowContext::GetImageWindowNewFlag(const_window_handle)
{

  abort();
}
api_bool ImageWindowContext::GetImageWindowCopyFlag(const_window_handle)
{

  abort();
}
api_bool ImageWindowContext::GetImageWindowFileURL(const_window_handle, char16_type*, size_type*)
{

  abort();
}
api_bool ImageWindowContext::GetImageWindowFilePath(const_window_handle, char16_type*, size_type*)
{

  abort();
}
api_bool ImageWindowContext::GetImageWindowFileInfo(const_window_handle, api_image_file_info*)
{

  abort();
}
size_type ImageWindowContext::GetImageWindowModifyCount(const_window_handle)
{

  abort();
}
view_handle ImageWindowContext::GetImageWindowMainView(const_window_handle)
{

  abort();
}
view_handle ImageWindowContext::GetImageWindowCurrentView(const_window_handle)
{

  abort();
}
void ImageWindowContext::SetImageWindowCurrentView(window_handle, view_handle)
{

  abort();
}
int32 ImageWindowContext::GetImageType(const_window_handle)
{

  abort();
}
api_bool ImageWindowContext::SetImageType(window_handle, int32 imageType, api_bool notify)
{

  abort();
}
void ImageWindowContext::PurgeImageWindowProperties(window_handle)
{

  abort();
}
api_bool ImageWindowContext::ValidateImageWindowView(const_window_handle, const_view_handle)
{

  abort();
}
int32 ImageWindowContext::GetPreviewCount(const_window_handle)
{

  abort();
}
view_handle ImageWindowContext::GetPreviewById(const_window_handle, const char*)
{

  abort();
}
view_handle ImageWindowContext::GetSelectedPreview(const_window_handle)
{

  abort();
}
void ImageWindowContext::SelectPreview(window_handle, view_handle)
{

  abort();
}
view_handle ImageWindowContext::CreatePreview(window_handle, int32, int32, int32, int32, const char*)
{

  abort();
}
void ImageWindowContext::ModifyPreview(window_handle, const char*, int32, int32, int32, int32, const char*)
{

  abort();
}
void ImageWindowContext::GetPreviewRect(const_window_handle, const char*, int32*, int32*, int32*, int32*)
{

  abort();
}
void ImageWindowContext::DeletePreview(window_handle, const char*)
{

  abort();
}
void ImageWindowContext::DeletePreviews(window_handle)
{

  abort();
}
window_handle ImageWindowContext::GetImageWindowMask(const_window_handle, api_bool* inverted)
{

  abort();
}
void ImageWindowContext::SetImageWindowMask(window_handle, window_handle, api_bool inverted)
{

  abort();
}
api_bool ImageWindowContext::GetImageWindowMaskEnabled(const_window_handle)
{

  abort();
}
void ImageWindowContext::SetImageWindowMaskEnabled(window_handle, api_bool)
{

  abort();
}
api_bool ImageWindowContext::GetImageWindowMaskVisible(const_window_handle)
{

  abort();
}
void ImageWindowContext::SetImageWindowMaskVisible(window_handle, api_bool)
{

  abort();
}
api_bool ImageWindowContext::ValidateImageWindowMask(const_window_handle, const_window_handle)
{

  abort();
}
int32 ImageWindowContext::GetMaskReferenceCount(const_window_handle)
{

  abort();
}
void ImageWindowContext::RemoveImageWindowMaskReferences(window_handle)
{

  abort();
}
void ImageWindowContext::UpdateImageWindowMaskReferences(window_handle)
{

  abort();
}
void ImageWindowContext::GetImageWindowSampleFormat(const_window_handle, uint32* nbits, api_bool* flt)
{

  abort();
}
void ImageWindowContext::SetImageWindowSampleFormat(window_handle, uint32 nbits, api_bool flt)
{

  abort();
}
void ImageWindowContext::GetImageWindowRGBWS(const_window_handle, api_RGBWS*)
{

  abort();
}
void ImageWindowContext::SetImageWindowRGBWS(window_handle, const api_RGBWS*)
{

  abort();
}
api_bool ImageWindowContext::GetImageWindowGlobalRGBWS(const_window_handle)
{

  abort();
}
void ImageWindowContext::SetImageWindowGlobalRGBWS(window_handle)
{

  abort();
}
void ImageWindowContext::GetGlobalRGBWS(api_RGBWS*)
{

  abort();
}
void ImageWindowContext::SetGlobalRGBWS(const api_RGBWS*)
{

  abort();
}
void ImageWindowContext::GetImageWindowCMEnabled(const_window_handle, api_bool* enableCM, api_bool* proofing, api_bool* gamutCheck)
{

  abort();
}
void ImageWindowContext::SetImageWindowCMEnabled(window_handle, api_bool enableCM, api_bool proofing, api_bool gamutCheck)
{

  abort();
}
uint32 ImageWindowContext::GetImageWindowICCProfileLength(const_window_handle)
{

  abort();
}
void ImageWindowContext::GetImageWindowICCProfile(const_window_handle, void*)
{

  abort();
}
void ImageWindowContext::SetImageWindowICCProfile(window_handle, const void*)
{

  abort();
}
void ImageWindowContext::LoadImageWindowICCProfile(window_handle, const char16_type*)
{

  abort();
}
void ImageWindowContext::DeleteImageWindowICCProfile(window_handle)
{

  abort();
}
int32 ImageWindowContext::GetImageWindowKeywordCount(const_window_handle)
{

  abort();
}
void ImageWindowContext::GetImageWindowKeyword(const_window_handle, int32, char*, size_type, char*, size_type, char*, size_type)
{

  abort();
}
void ImageWindowContext::AddImageWindowKeyword(window_handle, const char*, const char*, const char*)
{

  abort();
}
void ImageWindowContext::ResetImageWindowKeywords(window_handle)
{

  abort();
}
api_bool ImageWindowContext::GetImageWindowHasAstrometricSolution(const_window_handle)
{

  abort();
}
api_bool ImageWindowContext::RegenerateImageWindowAstrometricSolution(window_handle, api_bool, api_bool)
{

  abort();
}
api_bool ImageWindowContext::CopyImageWindowAstrometricSolution(window_handle, const_window_handle, api_bool)
{

  abort();
}
void ImageWindowContext::ClearImageWindowAstrometricSolution(window_handle, api_bool)
{

  abort();
}
void ImageWindowContext::UpdateImageWindowAstrometryMetadata(window_handle, api_bool)
{

  abort();
}
api_bool ImageWindowContext::ImageToCelestial(const_window_handle, double* x, double* y, api_bool rawRA)
{

  abort();
}
api_bool ImageWindowContext::CelestialToImage(const_window_handle, double* ra, double* dec)
{

  abort();
}
void ImageWindowContext::GetImageWindowResolution(const_window_handle, double*, double*, api_bool*)
{

  abort();
}
void ImageWindowContext::SetImageWindowResolution(window_handle, double, double, api_bool)
{

  abort();
}
void ImageWindowContext::GetDefaultResolution(double*, double*, api_bool*)
{

  abort();
}
void ImageWindowContext::GetDefaultICCProfileEmbedding(api_bool* rgb, api_bool* grayscale)
{

  abort();
}
api_bool ImageWindowContext::GetDefaultThumbnailEmbedding()
{

  abort();
}
api_bool ImageWindowContext::GetDefaultPropertiesEmbedding()
{

  abort();
}
api_bool ImageWindowContext::GetSwapDirectory(int32, char16_type*, size_type*)
{

  abort();
}
api_bool ImageWindowContext::SetSwapDirectories(const char16_type**, int32)
{

  abort();
}
int32 ImageWindowContext::GetCursorTolerance()
{

  abort();
}
int32 ImageWindowContext::GetImageWindowTransparencyMode(const_window_handle, uint32*)
{

  abort();
}
void ImageWindowContext::SetImageWindowTransparencyMode(window_handle, int32, uint32)
{

  abort();
}
int32 ImageWindowContext::GetTransparencyBackgroundBrush(uint32* fgColor, uint32* bgColor)
{

  abort();
}
void ImageWindowContext::SetTransparencyBackgroundBrush(int32 brush, uint32 fgColor, uint32 bgColor)
{

  abort();
}
int32 ImageWindowContext::GetImageWindowMode()
{

  abort();
}
void ImageWindowContext::SetImageWindowMode(int32)
{

  abort();
}
int32 ImageWindowContext::GetImageWindowDisplayChannel(const_window_handle)
{

  abort();
}
void ImageWindowContext::SetImageWindowDisplayChannel(window_handle, int32)
{

  abort();
}
int32 ImageWindowContext::GetImageWindowMaskMode(const_window_handle)
{

  abort();
}
void ImageWindowContext::SetImageWindowMaskMode(window_handle, int32)
{

  abort();
}
void ImageWindowContext::FitImageWindow(window_handle)
{

  abort();
}

void ImageWindowContext::ZoomImageWindowToFit(window_handle, api_bool, api_bool, api_bool, api_bool)
{

  //  abort();
}

int32 ImageWindowContext::GetImageWindowZoomFactor(const_window_handle)
{

  //  abort();
  return 1;
}
void ImageWindowContext::SetImageWindowZoomFactor(window_handle, int32)
{

  //  abort();
}
void ImageWindowContext::UpdateImageWindowViewport(window_handle)
{

  abort();
}
void ImageWindowContext::RegenerateImageWindowViewport(window_handle)
{

  abort();
}
void ImageWindowContext::SetImageWindowViewport(window_handle, double cx, double cy, int32 zoom)
{

  abort();
}
void ImageWindowContext::GetImageWindowViewportSize(const_window_handle, int32*, int32*)
{

  abort();
}
void ImageWindowContext::GetImageWindowViewportOrigin(const_window_handle, int32*, int32*)
{

  abort();
}
void ImageWindowContext::GetImageWindowViewportPosition(const_window_handle, int32*, int32*)
{

  abort();
}
void ImageWindowContext::SetImageWindowViewportPosition(window_handle, int32, int32)
{

  abort();
}
void ImageWindowContext::GetImageWindowVisibleViewportRect(const_window_handle, int32*, int32*, int32*, int32*)
{

  abort();
}
api_bool ImageWindowContext::GetImageWindowVisible(const_window_handle)
{

  abort();
}

void ImageWindowContext::SetImageWindowVisible( window_handle handle, api_bool visible )
{
    QWidget* w = reinterpret_cast<QWidget*>( handle );
    if ( !w )
        return;

    if ( visible )
        w->show();
    else
        w->hide();
}

api_bool ImageWindowContext::GetImageWindowIconic(const_window_handle)
{

  abort();
}
void ImageWindowContext::SetImageWindowIconic(window_handle, api_bool)
{

  abort();
}
void ImageWindowContext::BringImageWindowToFront(window_handle)
{

  abort();
}
void ImageWindowContext::SendImageWindowToBack(window_handle)
{

  abort();
}
interface_handle ImageWindowContext::GetActiveDynamicInterface()
{

  abort();
}
api_bool ImageWindowContext::TerminateDynamicSession(api_bool closeInterface)
{

  abort();
}
void ImageWindowContext::SetDynamicCursorXPM(window_handle, const char**, int32 hx, int32 hy)
{
  // ### deprecated
  abort();
}
   void           (ImageWindowContext::SetDynamicCursor)( window_handle, const_bitmap_handle, int32 hx, int32 hy )
   {

     abort();
   }

   bitmap_handle  (ImageWindowContext::GetDynamicCursorBitmap)( const_window_handle )
   {

     abort();
   }
   void           (ImageWindowContext::GetDynamicCursorHotSpot)( const_window_handle, int32* hx, int32* hy )
   {

     abort();
   }

   void           (ImageWindowContext::ViewportToImageArray)( const_window_handle, int32*, size_type n )
   {

     abort();
   }
   void           (ImageWindowContext::ViewportToImageArrayD)( const_window_handle, double*, size_type n )
   {

     abort();
   }

   void           (ImageWindowContext::ViewportToImage)( const_window_handle, int32* x, int32* y )
   {

     abort();
   }
   void           (ImageWindowContext::ViewportToImageD)( const_window_handle, double* x, double* y )
   {

     abort();
   }

   void           (ImageWindowContext::ImageToViewportArray)( const_window_handle, int32*, size_type n )
   {

     abort();
   }
   void           (ImageWindowContext::ImageToViewportArrayD)( const_window_handle, double*, size_type n )
   {

     abort();
   }

   void           (ImageWindowContext::ImageToViewport)( const_window_handle, int32* x, int32* y )
   {

     abort();
   }
   void           (ImageWindowContext::ImageToViewportD)( const_window_handle, double* x, double* y )
   {

     abort();
   }

   void           (ImageWindowContext::ViewportScalarToImageArray)( const_window_handle, int32*, size_type n )
   {

     abort();
   }
   void           (ImageWindowContext::ViewportScalarToImageArrayD)( const_window_handle, double*, size_type n )
   {

     abort();
   }

   void           (ImageWindowContext::ViewportScalarToImage)( const_window_handle, int32* )
   {

     abort();
   }
   void           (ImageWindowContext::ViewportScalarToImageD)( const_window_handle, double* )
   {

     abort();
   }

   void           (ImageWindowContext::ImageScalarToViewportArray)( const_window_handle, int32*, size_type n )
   {

     abort();
   }
   void           (ImageWindowContext::ImageScalarToViewportArrayD)( const_window_handle, double*, size_type n )
   {

     abort();
   }

   void           (ImageWindowContext::ImageScalarToViewport)( const_window_handle, int32* )
   {

     abort();
   }
   void           (ImageWindowContext::ImageScalarToViewportD)( const_window_handle, double* )
   {

     abort();
   }

   void           (ImageWindowContext::ViewportToGlobal)( const_window_handle, int32* x, int32* y )
   {

     abort();
   }
   void           (ImageWindowContext::GlobalToViewport)( const_window_handle, int32* x, int32* y )
   {

     abort();
   }

   void           (ImageWindowContext::UpdateViewportRect)( window_handle, int32, int32, int32, int32 )
   {

     abort();
   }
   void           (ImageWindowContext::UpdateImageRect)( window_handle, double, double, double, double )
   {

     abort();
   }

   void           (ImageWindowContext::RegenerateViewportRect)( window_handle, int32, int32, int32, int32 )
   {

     abort();
   }
   void           (ImageWindowContext::RegenerateImageRect)( window_handle, double, double, double, double )
   {

     abort();
   }

   void           (ImageWindowContext::CommitViewportUpdates)( window_handle )
   {

     abort();
   }

   api_bool       (ImageWindowContext::GetViewportUpdateRect)( const_window_handle, int32*, int32*, int32*, int32* )
   {

     abort();
   }

   void           (ImageWindowContext::BeginViewportSelection)( window_handle, int32 x, int32 y, uint32 flags )
   {

     abort();
   }
   void           (ImageWindowContext::ModifyViewportSelection)( window_handle, int32 x, int32 y, uint32 flags )
   {

     abort();
   }
   void           (ImageWindowContext::UpdateViewportSelection)( window_handle )
   {

     abort();
   }
   void           (ImageWindowContext::CancelViewportSelection)( window_handle )
   {

     abort();
   }
   void           (ImageWindowContext::EndViewportSelection)( window_handle )
   {

     abort();
   }
   api_bool       (ImageWindowContext::GetViewportSelection)( const_window_handle, int32* x0, int32* y0, int32* x1, int32* y1, uint32* flags )
   {

     abort();
   }

   bitmap_handle  (ImageWindowContext::GetViewportBitmap)( api_handle, const_window_handle, int32 x0, int32 y0, int32 x1, int32 y1, uint32 flags )
   {

     abort();
   }

   api_bool       (ImageWindowContext::GetImageWindowDisplayPixelRatio)( const_window_handle, double* )
   {

     abort();
   }
   api_bool       (ImageWindowContext::GetImageWindowResourcePixelRatio)( const_window_handle, double* )
   {

     abort();
   }
   api_bool       (ImageWindowContext::GetImageWindowDevicePixelRatio)( const_window_handle, double* )
   {

     abort();
   }

// ----------------------------------------------------------------------------
// ImageViewContext API
// ----------------------------------------------------------------------------

control_handle ImageViewContext::CreateImageView(api_handle hModule, api_handle hClient, control_handle hParent, uint32 flags, int32 width, int32 height, int32 numberOfChannels, int32 bitsPerSample, api_bool floatSample, api_bool color)
{

  abort();
}
control_handle ImageViewContext::CreateImageViewViewport(control_handle, api_handle hClient)
{

  abort();
}
image_handle ImageViewContext::GetImageViewImage(const_control_handle)
{

  abort();
}
api_bool ImageViewContext::IsImageViewColorImage(const_control_handle)
{

  abort();
}
api_bool ImageViewContext::GetImageViewImageGeometry(const_control_handle hView, int32*, int32*, int32*)
{

  abort();
}
api_bool ImageViewContext::GetImageViewSampleFormat(const_control_handle, int32* nbits, api_bool* flt)
{

  abort();
}
void ImageViewContext::SetImageViewSampleFormat(control_handle, int32 nbits, api_bool flt)
{

  abort();
}
void ImageViewContext::GetImageViewRGBWS(const_control_handle, api_RGBWS*)
{

  abort();
}
void ImageViewContext::SetImageViewRGBWS(control_handle, const api_RGBWS*)
{

  abort();
}
void ImageViewContext::GetImageViewCMEnabled(const_control_handle, api_bool* enableCM, api_bool* proofing, api_bool* gamutCheck)
{

  abort();
}
void ImageViewContext::SetImageViewCMEnabled(control_handle, api_bool enableCM, api_bool proofing, api_bool gamutCheck)
{

  abort();
}
uint32 ImageViewContext::GetImageViewICCProfileLength(const_control_handle)
{

  abort();
}
void ImageViewContext::GetImageViewICCProfile(const_control_handle, void*)
{

  abort();
}
void ImageViewContext::SetImageViewICCProfile(control_handle, const void*)
{

  abort();
}
void ImageViewContext::LoadImageViewICCProfile(control_handle, const char16_type*)
{

  abort();
}
void ImageViewContext::DeleteImageViewICCProfile(control_handle)
{

  abort();
}
api_bool ImageViewContext::SetImageViewScrollEventRoutine(control_handle, api_handle, pcl::range_event_routine)
{

  abort();
}
int32 ImageViewContext::GetImageViewMode(const_control_handle)
{

  abort();
}
void ImageViewContext::SetImageViewMode(control_handle, int32)
{

  abort();
}
int32 ImageViewContext::GetImageViewDisplayChannel(const_control_handle)
{

  abort();
}
void ImageViewContext::SetImageViewDisplayChannel(control_handle, int32)
{

  abort();
}
int32 ImageViewContext::GetImageViewZoomFactor(const_control_handle)
{

  abort();
}
void ImageViewContext::SetImageViewZoomFactor(control_handle, int32)
{

  abort();
}
int32 ImageViewContext::GetImageViewTransparencyMode(const_control_handle, uint32*)
{

  abort();
}
void ImageViewContext::SetImageViewTransparencyMode(control_handle, int32, uint32)
{

  abort();
}
void ImageViewContext::UpdateImageViewViewport(control_handle)
{

  abort();
}
void ImageViewContext::RegenerateImageViewViewport(control_handle)
{

  abort();
}
void ImageViewContext::SetImageViewViewport(control_handle, double cx, double cy, int32 zoom)
{

  abort();
}
void ImageViewContext::GetImageViewViewportSize(const_control_handle, int32*, int32*)
{

  abort();
}
void ImageViewContext::GetImageViewViewportOrigin(const_control_handle, int32*, int32*)
{

  abort();
}
void ImageViewContext::GetImageViewViewportPosition(const_control_handle, int32*, int32*)
{

  abort();
}
void ImageViewContext::SetImageViewViewportPosition(control_handle, int32, int32)
{

  abort();
}
void ImageViewContext::GetImageViewVisibleViewportRect(const_control_handle, int32*, int32*, int32*, int32*)
{

  abort();
}
void ImageViewContext::ViewportToImageArray(const_control_handle, int32*, size_type n)
{

  abort();
}
void ImageViewContext::ViewportToImageArrayD(const_control_handle, double*, size_type n)
{

  abort();
}
void ImageViewContext::ViewportToImage(const_control_handle, int32* x, int32* y)
{

  abort();
}
void ImageViewContext::ViewportToImageD(const_control_handle, double* x, double* y)
{

  abort();
}
void ImageViewContext::ImageToViewportArray(const_control_handle, int32*, size_type n)
{

  abort();
}
void ImageViewContext::ImageToViewportArrayD(const_control_handle, double*, size_type n)
{

  abort();
}
void ImageViewContext::ImageToViewport(const_control_handle, int32* x, int32* y)
{

  abort();
}
void ImageViewContext::ImageToViewportD(const_control_handle, double* x, double* y)
{

  abort();
}
void ImageViewContext::ViewportScalarToImageArray(const_control_handle, int32*, size_type n)
{

  abort();
}
void ImageViewContext::ViewportScalarToImageArrayD(const_control_handle, double*, size_type n)
{

  abort();
}
void ImageViewContext::ViewportScalarToImage(const_control_handle, int32*)
{

  abort();
}
void ImageViewContext::ViewportScalarToImageD(const_control_handle, double*)
{

  abort();
}
void ImageViewContext::ImageScalarToViewportArray(const_control_handle, int32*, size_type n)
{

  abort();
}
void ImageViewContext::ImageScalarToViewportArrayD(const_control_handle, double*, size_type n)
{

  abort();
}
void ImageViewContext::ImageScalarToViewport(const_control_handle, int32*)
{

  abort();
}
void ImageViewContext::ImageScalarToViewportD(const_control_handle, double*)
{

  abort();
}
void ImageViewContext::ViewportToGlobal(const_control_handle, int32* x, int32* y)
{

  abort();
}
void ImageViewContext::GlobalToViewport(const_control_handle, int32* x, int32* y)
{

  abort();
}
void ImageViewContext::UpdateViewportRect(control_handle, int32, int32, int32, int32)
{

  abort();
}
void ImageViewContext::UpdateImageRect(control_handle, double, double, double, double)
{

  abort();
}
void ImageViewContext::RegenerateViewportRect(control_handle, int32, int32, int32, int32)
{

  abort();
}
void ImageViewContext::RegenerateImageRect(control_handle, double, double, double, double)
{

  abort();
}
void ImageViewContext::CommitViewportUpdates(control_handle)
{

  abort();
}
api_bool ImageViewContext::GetViewportUpdateRect(const_control_handle, int32*, int32*, int32*, int32*)
{

  abort();
}
void ImageViewContext::BeginViewportSelection(control_handle, int32 x, int32 y, uint32 flags)
{

  abort();
}
void ImageViewContext::ModifyViewportSelection(control_handle, int32 x, int32 y, uint32 flags)
{

  abort();
}
void ImageViewContext::UpdateViewportSelection(control_handle)
{

  abort();
}
void ImageViewContext::CancelViewportSelection(control_handle)
{

  abort();
}
void ImageViewContext::EndViewportSelection(control_handle)
{

  abort();
}
api_bool ImageViewContext::GetViewportSelection(const_control_handle, int32* x0, int32* y0, int32* x1, int32* y1, uint32* flags)
{

  abort();
}
bitmap_handle ImageViewContext::GetViewportBitmap(api_handle, const_control_handle, int32 x0, int32 y0, int32 x1, int32 y1, uint32 flags)
{

  abort();
}

// ----------------------------------------------------------------------------
// CodeEditorContext API
// ----------------------------------------------------------------------------

control_handle CodeEditor_CreateCodeEditor(api_handle hModule, api_handle hClient, control_handle hParent, uint32 flags)
{

  abort();
}
control_handle CodeEditor_CreateEditorLineNumbersControl(control_handle, api_handle hClient, control_handle hParent, uint32 flags)
{

  abort();
}
api_bool CodeEditor_GetEditorFilePath(const_control_handle, char16_type*, size_type*)
{

  abort();
}
void CodeEditor_SetEditorFilePath(control_handle, const char16_type*)
{

  abort();
}
api_bool CodeEditor_GetEditorText(const_control_handle, char16_type*, size_type*)
{

  abort();
}
void CodeEditor_SetEditorText(control_handle, const char16_type*)
{

  abort();
}
api_bool CodeEditor_GetEditorEncodedText(const_control_handle, char*, size_type*, const char* encoding)
{

  abort();
}
api_bool CodeEditor_SetEditorEncodedText(control_handle, const char*, const char* encoding)
{

  abort();
}
void CodeEditor_ClearEditorText(control_handle)
{

  abort();
}
api_bool CodeEditor_GetEditorReadOnly(const_control_handle)
{

  abort();
}
void CodeEditor_SetEditorReadOnly(control_handle, api_bool)
{

  abort();
}
api_bool CodeEditor_SaveEditorText(control_handle, const char16_type* filePath, const char* encoding)
{

  abort();
}
api_bool CodeEditor_LoadEditorText(control_handle, const char16_type* filePath, const char* encoding)
{

  abort();
}
int32 CodeEditor_GetEditorLineCount(const_control_handle)
{

  abort();
}
int32 CodeEditor_GetEditorCharacterCount(const_control_handle)
{

  abort();
}
void CodeEditor_GetEditorCursorCoordinates(const_control_handle, int32* line, int32* col)
{

  abort();
}
void CodeEditor_SetEditorCursorCoordinates(control_handle, int32 line, int32 col)
{

  abort();
}
api_bool CodeEditor_GetEditorInsertMode(const_control_handle)
{

  abort();
}
void CodeEditor_SetEditorInsertMode(control_handle, api_bool)
{

  abort();
}
api_bool CodeEditor_GetEditorBlockSelectionMode(const_control_handle)
{

  abort();
}
void CodeEditor_SetEditorBlockSelectionMode(control_handle, api_bool)
{

  abort();
}
api_bool CodeEditor_GetEditorDynamicWordWrapMode(const_control_handle)
{

  abort();
}
void CodeEditor_SetEditorDynamicWordWrapMode(control_handle, api_bool)
{

  abort();
}
int32 CodeEditor_GetEditorUndoSteps(const_control_handle)
{

  abort();
}
int32 CodeEditor_GetEditorRedoSteps(const_control_handle)
{

  abort();
}
api_bool CodeEditor_GetEditorHasSelection(const_control_handle)
{

  abort();
}
void CodeEditor_GetEditorSelectionCoordinates(const_control_handle, int32* fromLine, int32* fromCol, int32* toLine, int32* toCol)
{

  abort();
}
void CodeEditor_SetEditorSelectionCoordinates(control_handle, int32 fromLine, int32 fromCol, int32 toLine, int32 toCol)
{

  abort();
}
api_bool CodeEditor_GetEditorSelectedText(const_control_handle, char16_type*, size_type*)
{

  abort();
}
void CodeEditor_InsertEditorText(control_handle, const char16_type*)
{

  abort();
}
void CodeEditor_EditorUndo(control_handle)
{

  abort();
}
void CodeEditor_EditorRedo(control_handle)
{

  abort();
}
void CodeEditor_EditorCut(control_handle)
{

  abort();
}
void CodeEditor_EditorCopy(control_handle)
{

  abort();
}
void CodeEditor_EditorPaste(control_handle)
{

  abort();
}
void CodeEditor_EditorDelete(control_handle)
{

  abort();
}
void CodeEditor_EditorSelectAll(control_handle)
{

  abort();
}
void CodeEditor_EditorUnselect(control_handle)
{

  abort();
}
api_bool CodeEditor_EditorGotoMatchedParenthesis(control_handle)
{

  abort();
}
int32 CodeEditor_EditorHighlightAllMatches(control_handle, const char16_type*, uint32 flags)
{

  abort();
}
void CodeEditor_EditorClearMatches(control_handle)
{

  abort();
}
api_bool CodeEditor_EditorFind(control_handle, const char16_type*, uint32 flags)
{

  abort();
}
api_bool CodeEditor_EditorReplace(control_handle, const char16_type*)
{

  abort();
}
int32 CodeEditor_EditorReplaceAll(control_handle, const char16_type*, const char16_type*, uint32 flags)
{

  abort();
}
api_bool CodeEditor_SetEditorTextUpdatedEventRoutine(control_handle, api_handle, pcl::event_routine)
{

  abort();
}
api_bool CodeEditor_SetEditorCursorPositionUpdatedEventRoutine(control_handle, api_handle, pcl::range_event_routine)
{

  abort();
}
api_bool CodeEditor_SetEditorSelectionUpdatedEventRoutine(control_handle, api_handle, pcl::rect_event_routine)
{

  abort();
}
api_bool CodeEditor_SetEditorOverwriteModeUpdatedEventRoutine(control_handle, api_handle, pcl::state_event_routine)
{

  abort();
}
api_bool CodeEditor_SetEditorSelectionModeUpdatedEventRoutine(control_handle, api_handle, pcl::state_event_routine)
{

  abort();
}
api_bool CodeEditor_SetEditorDynamicWordWrapModeUpdatedEventRoutine(control_handle, api_handle, pcl::state_event_routine)
{

  abort();
}

// ----------------------------------------------------------------------------
// WebViewContext API
// ----------------------------------------------------------------------------

control_handle WebViewContext::CreateWebView(api_handle hModule, api_handle hClient, control_handle hParent, uint32 flags)
{

  abort();
}
api_bool WebViewContext::SetWebViewContent(control_handle, const void* data, size_type size, const char* mimeType)
{

  abort();
}
api_bool WebViewContext::LoadWebViewContent(control_handle, const char16_type* URI)
{

  abort();
}
api_bool WebViewContext::RequestWebViewPlainText(const_control_handle)
{

  abort();
}
api_bool WebViewContext::RequestWebViewHTML(const_control_handle)
{

  abort();
}
api_bool WebViewContext::SaveWebViewAsPDF(control_handle, const char16_type* filePath, const double* pageWidth, const double* pageHeight, const double* marginLeft, const double* marginTop, const double* marginRight, const double* marginBottom, int32 orientation)
{

  abort();
}
api_bool WebViewContext::GetWebViewHasSelection(const_control_handle)
{

  abort();
}
api_bool WebViewContext::GetWebViewSelectedText(const_control_handle, char16_type*, size_type*)
{

  abort();
}
api_bool WebViewContext::GetWebViewZoomFactor(const_control_handle, double*)
{

  abort();
}
api_bool WebViewContext::SetWebViewZoomFactor(control_handle, const double*)
{

  abort();
}
uint32 WebViewContext::GetWebViewBackgroundColor(const_control_handle)
{

  abort();
}
api_bool WebViewContext::SetWebViewBackgroundColor(control_handle, uint32)
{

  abort();
}
api_bool WebViewContext::ReloadWebView(control_handle)
{

  abort();
}
api_bool WebViewContext::StopWebView(control_handle)
{

  abort();
}
api_bool WebViewContext::EvaluateWebViewScript(control_handle, const char16_type* sourceCode, const char* language)
{

  abort();
}
api_bool WebViewContext::SetWebViewLoadStartedEventRoutine(control_handle, api_handle, pcl::event_routine)
{

  abort();
}
api_bool WebViewContext::SetWebViewLoadProgressEventRoutine(control_handle, api_handle, pcl::value_event_routine)
{

  abort();
}
api_bool WebViewContext::SetWebViewLoadFinishedEventRoutine(control_handle, api_handle, pcl::state_event_routine)
{

  abort();
}
api_bool WebViewContext::SetWebViewSelectionUpdatedEventRoutine(control_handle, api_handle, pcl::event_routine)
{

  abort();
}
api_bool WebViewContext::SetWebViewPlainTextAvailableEventRoutine(control_handle, api_handle, pcl::unicode_event_routine)
{

  abort();
}
api_bool WebViewContext::SetWebViewHTMLAvailableEventRoutine(control_handle, api_handle, pcl::unicode_event_routine)
{

  abort();
}
api_bool WebViewContext::SetWebViewScriptResultAvailableEventRoutine(control_handle, api_handle, pcl::property_event_routine)
{

  abort();
}

// ----------------------------------------------------------------------------
// ExternalProcessContext API
// ----------------------------------------------------------------------------

   int32          (ExternalProcess_ExecuteProgram)( const char16_type* program, const char16_type** argv, size_type argc )
   {

     abort();
   }

   api_bool       (ExternalProcess_StartProgram)( const char16_type* program, const char16_type** argv, size_type argc,
                                            const char16_type* workingDirectory, uint64* pid )
   {

     abort();
   }

   external_process_handle (ExternalProcess_CreateExternalProcess)( api_handle hModule, api_handle hClient )
   {

     abort();
   }

   api_bool       (ExternalProcess_StartExternalProcess)( external_process_handle,
                                                    const char16_type* program, const char16_type** argv, size_type argc )
   {

     abort();
   }

   api_bool       (ExternalProcess_WaitForExternalProcessStarted)( external_process_handle, int32 ms )
   {

     abort();
   }
   api_bool       (ExternalProcess_WaitForExternalProcessFinished)( external_process_handle, int32 ms )
   {

     abort();
   }
   api_bool       (ExternalProcess_WaitForExternalProcessDataAvailable)( external_process_handle, int32 ms )
   {

     abort();
   }
   api_bool       (ExternalProcess_WaitForExternalProcessDataWritten)( external_process_handle, int32 ms )
   {

     abort();
   }

   api_bool       (ExternalProcess_TerminateExternalProcess)( external_process_handle )
   {

     abort();
   }
   api_bool       (ExternalProcess_KillExternalProcess)( external_process_handle )
   {

     abort();
   }

   api_bool       (ExternalProcess_CloseExternalProcessStream)( external_process_handle, int32 stream )
   {

     abort();
   }

   api_bool       (ExternalProcess_RedirectExternalProcessToFile)( external_process_handle, int32 stream, const char16_type* fileName, api_bool append )
   {

     abort();
   }
   api_bool       (ExternalProcess_PipeExternalProcess)( external_process_handle, int32 stream, external_process_handle toProcess )
   {

     abort();
   }

   api_bool       (ExternalProcess_GetExternalProcessWorkingDirectory)( const_external_process_handle, char16_type*, size_type* )
   {

     abort();
   }
   api_bool       (ExternalProcess_SetExternalProcessWorkingDirectory)( external_process_handle, const char16_type* )
   {

     abort();
   }

   api_bool       (ExternalProcess_GetExternalProcessIsRunning)( const_external_process_handle )
   {

     abort();
   }
   api_bool       (ExternalProcess_GetExternalProcessIsStarting)( const_external_process_handle )
   {

     abort();
   }

   uint64         (ExternalProcess_GetExternalProcessPID)( const_external_process_handle )
   {

     abort();
   }

   int32          (ExternalProcess_GetExternalProcessExitCode)( const_external_process_handle )
   {

     abort();
   }
   int32          (ExternalProcess_GetExternalProcessExitStatus)( const_external_process_handle )
   {

     abort();
   }
   int32          (ExternalProcess_GetExternalProcessErrorCode)( const_external_process_handle )
   {

     abort();
   }

   size_type      (ExternalProcess_GetExternalProcessBytesAvailable)( const_external_process_handle )
   {

     abort();
   }
   size_type      (ExternalProcess_GetExternalProcessBytesToWrite)( const_external_process_handle )
   {

     abort();
   }

   // ### The following function returns data allocated by the caller module.
   api_bool       (ExternalProcess_ReadFromExternalProcess)( api_handle hModule, external_process_handle, int32 stream, void**, size_type* )
   {

     abort();
   }
   api_bool       (ExternalProcess_WriteToExternalProcess)( external_process_handle, const void*, size_type count )
   {

     abort();
   }

   api_bool       (ExternalProcess_EnumerateExternalProcessEnvironment)( const_external_process_handle, pcl::environment_enumeration_callback, void* )
   {

     abort();
   }
   api_bool       (ExternalProcess_SetExternalProcessEnvironment)( external_process_handle, const char16_type** vars, size_type count )
   {

     abort();
   }

   api_bool       (ExternalProcess_SetExternalProcessStartedEventRoutine)( external_process_handle, api_handle, pcl::external_process_event_routine )
   {

     abort();
   }
   api_bool       (ExternalProcess_SetExternalProcessFinishedEventRoutine)( external_process_handle, api_handle, pcl::external_process_exit_status_event_routine )
   {

     abort();
   }
   api_bool       (ExternalProcess_SetExternalProcessStandardOutputDataAvailableEventRoutine)( external_process_handle, api_handle, pcl::external_process_event_routine )
   {

     abort();
   }
   api_bool       (ExternalProcess_SetExternalProcessStandardErrorDataAvailableEventRoutine)( external_process_handle, api_handle, pcl::external_process_event_routine )
   {

     abort();
   }
   api_bool       (ExternalProcess_SetExternalProcessErrorEventRoutine)( external_process_handle, api_handle, pcl::external_process_status_event_routine )
   {

     abort();
   }
  
/*  
   void        (ExternalProcess_EnterProcessDefinitionContext)()
   {

abort();
   }
   api_bool    (ExternalProcess_IsProcessDefinitionContextActive)()
   {

abort();
   }

   void        (ExternalProcess_BeginProcessDefinition)( meta_process_handle, const char* procId )
   {

abort();
   }
   api_bool    (ExternalProcess_GetProcessBeingDefined)( char*, size_type* )
   {

abort();
   }
   void        (ExternalProcess_SetProcessCategory)( const char* )
   {

abort();
   }
   void        (ExternalProcess_SetProcessVersion)( uint32 )
   {

abort();
   }
   void        (ExternalProcess_SetProcessAliasIdentifiers)( const char* )
   {

abort();
   }
   void        (ExternalProcess_SetProcessDescription)( const char16_type* )
   {

abort();
   }
   void        (ExternalProcess_SetProcessScriptComment)( const char16_type* )
   {

abort();
   }
   void        (ExternalProcess_SetProcessIconSVG)( const char* )
   {

abort();
   }
   void        (ExternalProcess_SetProcessIconSVGFile)( const char16_type* )
   {

abort();
   }
   void        (ExternalProcess_SetProcessIconImage)( const char** )
   {
   // ### deprecated
abort();
   }
   void        (ExternalProcess_SetProcessIconImageFile)( const char16_type* )
   {
   // ### deprecated
abort();
   }
   void        (ExternalProcess_SetProcessIconSmallImage)( const char** )
   {
   // ### deprecated
abort();
   }
   void        (ExternalProcess_SetProcessIconSmallImageFile)( const char16_type* )
   {
   // ### deprecated
abort();
   }

   void        (ExternalProcess_SetProcessClassInitializationRoutine)( pcl::process_class_initialization_routine )
   {

abort();
   }
   void        (ExternalProcess_SetProcessCreationRoutine)( pcl::process_creation_routine )
   {

abort();
   }
   void        (ExternalProcess_SetProcessDestructionRoutine)( pcl::process_destruction_routine )
   {

abort();
   }
   void        (ExternalProcess_SetProcessClonationRoutine)( pcl::process_clonation_routine)
   {

abort();
   }
   void        (ExternalProcess_SetProcessTestClonationRoutine)( pcl::process_test_clonation_routine )
   {

abort();
   }
   void        (ExternalProcess_SetProcessSetServerHandleRoutine)( pcl::process_set_handle_routine )
   {

abort();
   }
   void        (ExternalProcess_SetProcessAssignmentRoutine)( pcl::process_assignment_routine )
   {

abort();
   }
   void        (ExternalProcess_SetProcessInitializationRoutine)( pcl::process_initialization_routine )
   {

abort();
   }
   void        (ExternalProcess_SetProcessValidationRoutine)( pcl::process_validation_routine )
   {

abort();
   }

   void        (ExternalProcess_SetProcessCommandLineProcessingRoutine)( pcl::process_command_line_processing_routine, uint32 flags )
   {

abort();
   }
   void        (ExternalProcess_SetProcessEditPreferencesRoutine)( pcl::process_edit_preferences_routine )
   {

abort();
   }
   void        (ExternalProcess_SetProcessBrowseDocumentationRoutine)( pcl::process_browse_documentation_routine )
   {

abort();
   }
   void        (ExternalProcess_SetProcessExecutionPreferencesRoutine)( pcl::process_execution_preferences_routine )
   {

abort();
   }

   void        (ExternalProcess_SetProcessExecutionValidationRoutine)( pcl::process_execution_validation_routine )
   {

abort();
   }
   void        (ExternalProcess_SetProcessMaskValidationRoutine)( pcl::process_mask_validation_routine )
   {

abort();
   }
   void        (ExternalProcess_SetProcessHistoryUpdateValidationRoutine)( pcl::process_history_update_validation_routine )
   {

abort();
   }
   void        (ExternalProcess_SetProcessUndoModeRoutine)( pcl::process_undo_mode_routine )
   {

abort();
   }
   void        (ExternalProcess_SetProcessPreExecutionRoutine)( pcl::process_pre_execution_routine )
   {

abort();
   }
   void        (ExternalProcess_SetProcessExecutionRoutine)( pcl::process_execution_routine )
   {

abort();
   }
   void        (ExternalProcess_SetProcessPostExecutionRoutine)( pcl::process_post_execution_routine )
   {

abort();
   }

   void        (ExternalProcess_SetProcessGlobalExecutionValidationRoutine)( pcl::process_global_execution_validation_routine )
   {

abort();
   }
   void        (ExternalProcess_SetProcessPreGlobalExecutionRoutine)( pcl::process_pre_global_execution_routine )
   {

abort();
   }
   void        (ExternalProcess_SetProcessGlobalExecutionRoutine)( pcl::process_global_execution_routine )
   {

abort();
   }
   void        (ExternalProcess_SetProcessPostGlobalExecutionRoutine)( pcl::process_post_global_execution_routine )
   {

abort();
   }

   void        (ExternalProcess_SetProcessImageExecutionValidationRoutine)( pcl::process_image_execution_validation_routine )
   {

abort();
   }
   void        (ExternalProcess_SetProcessImageExecutionRoutine)( pcl::process_image_execution_routine )
   {

abort();
   }

   void        (ExternalProcess_SetProcessDefaultInterfaceSelectionRoutine)( pcl::process_default_interface_selection_routine )
   {

abort();
   }
   void        (ExternalProcess_SetProcessInterfaceSelectionRoutine)( pcl::process_interface_selection_routine )
{

abort();
}
   void        (ExternalProcess_SetProcessInterfaceValidationRoutine)( pcl::process_interface_validation_routine )
{

abort();
}

   void        (ExternalProcess_SetProcessPreReadingRoutine)( pcl::process_pre_reading_routine )
{

abort();
}
   void        (ExternalProcess_SetProcessPostReadingRoutine)( pcl::process_post_reading_routine )
{

abort();
}
   void        (ExternalProcess_SetProcessPreWritingRoutine)( pcl::process_pre_writing_routine )
{

abort();
}
   void        (ExternalProcess_SetProcessPostWritingRoutine)( pcl::process_post_writing_routine )
{

abort();
}

   void        (ExternalProcess_SetProcessIPCStartRoutine)( pcl::process_ipc_notification_routine )
{

abort();
}
   void        (ExternalProcess_SetProcessIPCStopRoutine)( pcl::process_ipc_notification_routine )
{

abort();
}
   void        (ExternalProcess_SetProcessIPCSetParametersRoutine)( pcl::process_ipc_notification_routine )
{

abort();
}
   void        (ExternalProcess_SetProcessIPCGetStatusRoutine)( pcl::process_ipc_status_routine )
{

abort();
}
   void        (ExternalProcess_BeginParameterDefinition)( meta_parameter_handle, const char* parId, uint32 parType )
{

abort();
}
   api_bool    (ExternalProcess_GetParameterBeingDefined)( char*, size_type* )
{

abort();
}
   void        (ExternalProcess_SetParameterProcessVersionRange)( uint32, uint32 )
{

abort();
}
   void        (ExternalProcess_SetParameterRequired)( api_bool )
{

abort();
}
   void        (ExternalProcess_SetParameterReadOnly)( api_bool )
{

abort();
}
   void        (ExternalProcess_SetParameterAliasIdentifiers)( const char* )
{

abort();
}
   void        (ExternalProcess_SetParameterDescription)( const char16_type* )
{

abort();
}
   void        (ExternalProcess_SetParameterScriptComment)( const char16_type* )
{

abort();
}
   void        (ExternalProcess_SetParameterLockRoutine)( pcl::parameter_lock_routine )
{

abort();
}
   void        (ExternalProcess_SetParameterUnlockRoutine)( pcl::parameter_unlock_routine )
{

abort();
}
   void        (ExternalProcess_SetParameterValidationRoutine)( pcl::parameter_validation_routine )
{

abort();
}
   void        (ExternalProcess_SetParameterAllocationRoutine)( pcl::parameter_allocation_routine )
{

abort();
}
   void        (ExternalProcess_SetParameterLengthQueryRoutine)( pcl::parameter_length_query_routine )
{

abort();
}
   void        (ExternalProcess_SetDefaultNumericValue)( double )
{

abort();
}
   void        (ExternalProcess_SetValidNumericRange)( double, double )
{

abort();
}
   void        (ExternalProcess_SetPrecision)( int32 )
   {

abort();
   }
   void        (ExternalProcess_SetScientificNotation)( api_bool )
{

abort();
}
   void        (ExternalProcess_SetDefaultBooleanValue)( api_bool )
{

abort();
}
   void        (ExternalProcess_DefineEnumerationElement)( const char*, api_enum )
{

abort();
}
   void        (ExternalProcess_DefineEnumerationAlias)( const char*, const char* )
{

abort();
}
   void        (ExternalProcess_SetDefaultEnumerationValueIndex)( uint32 )
{

abort();
}
   void        (ExternalProcess_SetDefaultStringValue)( const char16_type* )
{

abort();
}
   void        (ExternalProcess_SetStringAllowedCharacters)( const char16_type* )
{

abort();
}
   void        (ExternalProcess_SetStringLengthLimits)( size_type, size_type )
{

abort();
}
   void        (ExternalProcess_BeginTableColumnDefinition)( meta_parameter_handle, const char* colId, uint32 colType )
{

abort();
}
   void        (ExternalProcess_EndTableColumnDefinition)()
{

abort();
}
   void        (ExternalProcess_SetTableRowLimits)( size_type, size_type )
{

abort();
}
   void        (ExternalProcess_SetBlockSizeLimits)( size_type, size_type )
{

abort();
}

   void        (ExternalProcess_EndParameterDefinition)()
{

abort();
}
   void        (ExternalProcess_EndProcessDefinition)()
{

abort();
}
   void        (ExternalProcess_ExitProcessDefinitionContext)()
{

abort();
}
*/
  
// ----------------------------------------------------------------------------
// NetworkTransferContext API
// ----------------------------------------------------------------------------

network_transfer_handle NetworkTransfer_CreateNetworkTransfer(api_handle hModule, api_handle hClient)
{

  abort();
}
api_bool NetworkTransfer_SetNetworkTransferURL(network_transfer_handle, const char16_type* url, const char16_type* userName, const char16_type* userPassword)
{

  abort();
}
api_bool NetworkTransfer_SetNetworkTransferProxyURL(network_transfer_handle, const char16_type* proxy, const char16_type* userName, const char16_type* userPassword)
{

  abort();
}
api_bool NetworkTransfer_SetNetworkTransferSSL(network_transfer_handle, api_bool useSSL, api_bool forceSSL, api_bool verifyPeer, api_bool verifyHost)
{

  abort();
}
api_bool NetworkTransfer_SetNetworkTransferCustomHTTPHeaders(network_transfer_handle, const char16_type* nlsHeaders)
{

  abort();
}
api_bool NetworkTransfer_SetNetworkTransferConnectionTimeout(network_transfer_handle, int32 seconds)
{

  abort();
}
api_bool NetworkTransfer_PerformNetworkTransferDownload(network_transfer_handle)
{

  abort();
}
api_bool NetworkTransfer_PerformNetworkTransferUpload(network_transfer_handle, fsize_type uploadSize)
{

  abort();
}
api_bool NetworkTransfer_PerformNetworkTransferPOST(network_transfer_handle, const char16_type* postFields)
{

  abort();
}
api_bool NetworkTransfer_PerformNetworkTransferSMTP(network_transfer_handle, const char16_type* mailFrom, const char16_type* mailRecipients)
{

  abort();
}
void NetworkTransfer_CloseNetworkTransferConnection(network_transfer_handle)
{

  abort();
}
api_bool NetworkTransfer_GetNetworkTransferURL(const_network_transfer_handle, char16_type*, size_type*)
{

  abort();
}
api_bool NetworkTransfer_GetNetworkTransferProxyURL(const_network_transfer_handle, char16_type*, size_type*)
{

  abort();
}
api_bool NetworkTransfer_GetNetworkTransferCustomHTTPHeaders(const_network_transfer_handle, char16_type*, size_type*)
{

  abort();
}
api_bool NetworkTransfer_GetNetworkTransferStatus(const_network_transfer_handle)
{

  abort();
}
api_bool NetworkTransfer_GetNetworkTransferIsAborted(const_network_transfer_handle)
{

  abort();
}
int32 NetworkTransfer_GetNetworkTransferResponseCode(const_network_transfer_handle)
{

  abort();
}
api_bool NetworkTransfer_GetNetworkTransferContentType(const_network_transfer_handle, char16_type*, size_type*)
{

  abort();
}
fsize_type NetworkTransfer_GetNetworkTransferBytesTransferred(const_network_transfer_handle)
{

  abort();
}
void NetworkTransfer_GetNetworkTransferTotalSpeed(const_network_transfer_handle, double*)
{
  // in KiB/s
  abort();
}
   void           (NetworkTransfer_GetNetworkTransferTotalTime)( const_network_transfer_handle, double* )
   {
     // in s
     abort();
   }
   api_bool       (NetworkTransfer_GetNetworkTransferErrorInformation)( const_network_transfer_handle, char16_type*, size_type* )
   {

     abort();
   }

   api_bool       (NetworkTransfer_SetNetworkTransferDownloadEventRoutine)( network_transfer_handle, api_handle, pcl::network_download_event_routine )
   {

     abort();
   }
   api_bool       (NetworkTransfer_SetNetworkTransferUploadEventRoutine)( network_transfer_handle, api_handle, pcl::network_upload_event_routine )
   {

     abort();
   }
   api_bool       (NetworkTransfer_SetNetworkTransferProgressEventRoutine)( network_transfer_handle, api_handle, pcl::network_progress_event_routine )
   {

     abort();
   }

void ControlContext::AdjustControlToContents(control_handle)
{

  //  abort();
}

void ControlContext::GetControlExpansionEnabled(const_control_handle, api_bool*, api_bool*)
{

  abort();
}
void ControlContext::SetControlExpansionEnabled(control_handle, api_bool, api_bool)
{

  abort();
}

api_bool ControlContext::GetControlUnderMouseStatus(const_control_handle)
{

  abort();
}
void ControlContext::BringControlToFront(control_handle)
{

  abort();
}
void ControlContext::SendControlToBack(control_handle)
{

  abort();
}
void ControlContext::StackControls(control_handle stackThis, control_handle underThis)
{

  abort();
}
sizer_handle ControlContext::GetControlSizer(const_control_handle)
{

  abort();
}

void ControlContext::GlobalToLocal(const_control_handle, int32*, int32*)
{

  abort();
}
void ControlContext::LocalToGlobal(const_control_handle, int32*, int32*)
{

  abort();
}
void ControlContext::ParentToLocal(const_control_handle, int32*, int32*)
{

  abort();
}
void ControlContext::LocalToParent(const_control_handle, int32*, int32*)
{

  abort();
}
void ControlContext::ControlToLocal(const_control_handle, const_control_handle, int32*, int32*)
{

  abort();
}
void ControlContext::LocalToControl(const_control_handle, const_control_handle, int32*, int32*)
{

  abort();
}
control_handle ControlContext::GetChildByPos(const_control_handle, int32, int32)
{
  // returns client handle
  abort();
}

   void           (ControlContext::GetChildrenRect)( const_control_handle, int32*, int32*, int32*, int32* )
   {

     abort();
   }

   api_bool       (ControlContext::GetControlAncestry)( const_control_handle, const_control_handle )
   {

     abort();
   }


// ----------------------------------------------------------------------------
// GraphicsContext API
// ----------------------------------------------------------------------------

graphics_handle Graphics_CreateGraphics(api_handle)
{

  abort();
}
api_bool Graphics_BeginControlPaint(graphics_handle, control_handle)
{

  abort();
}
api_bool Graphics_BeginBitmapPaint(graphics_handle, bitmap_handle)
{

  abort();
}
api_bool Graphics_BeginSVGPaint(graphics_handle, svg_handle)
{

  abort();
}
void Graphics_EndPaint(graphics_handle)
{

  abort();
}
api_bool Graphics_GetGraphicsStatus(const_graphics_handle)
{

  abort();
}
api_bool Graphics_GetGraphicsTransformationEnabled(const_graphics_handle)
{

  abort();
}
void Graphics_EnableGraphicsTransformation(graphics_handle, api_bool)
{

  abort();
}
void Graphics_GetGraphicsTransformationMatrix(const_graphics_handle, double* m11, double* m12, double* m13, double* m21, double* m22, double* m23, double* m31, double* m32, double* m33)
{

  abort();
}
void Graphics_SetGraphicsTransformationMatrix(graphics_handle, double m11, double m12, double m13, double m21, double m22, double m23, double m31, double m32, double m33)
{

  abort();
}
void Graphics_MultiplyGraphicsTransformationMatrix(graphics_handle, double m11, double m12, double m13, double m21, double m22, double m23, double m31, double m32, double m33)
{

  abort();
}
void Graphics_RotateGraphicsTransformation(graphics_handle, double)
{

  abort();
}
void Graphics_ScaleGraphicsTransformation(graphics_handle, double, double)
{

  abort();
}
void Graphics_TranslateGraphicsTransformation(graphics_handle, double, double)
{

  abort();
}
void Graphics_ShearGraphicsTransformation(graphics_handle, double, double)
{

  abort();
}
void Graphics_ResetGraphicsTransformation(graphics_handle)
{

  abort();
}
void Graphics_TransformPoints(const_graphics_handle, double* xy, size_type n)
{

  abort();
}
api_bool Graphics_GetGraphicsClippingEnabled(const_graphics_handle)
{

  abort();
}
void Graphics_EnableGraphicsClipping(graphics_handle, api_bool)
{

  abort();
}
void Graphics_GetGraphicsClipRect(const_graphics_handle, int32*, int32*, int32*, int32*)
{

  abort();
}
void Graphics_SetGraphicsClipRect(graphics_handle, int32, int32, int32, int32)
{

  abort();
}
void Graphics_GetGraphicsClipRectD(const_graphics_handle, double*, double*, double*, double*)
{

  abort();
}
void Graphics_SetGraphicsClipRectD(graphics_handle, double, double, double, double)
{

  abort();
}
api_bool Graphics_GetGraphicsAntialiasingEnabled(const_graphics_handle)
{

  abort();
}
void Graphics_EnableGraphicsAntialiasing(graphics_handle, api_bool)
{

  abort();
}
api_bool Graphics_GetGraphicsTextAntialiasingEnabled(const_graphics_handle)
{

  abort();
}
void Graphics_EnableGraphicsTextAntialiasing(graphics_handle, api_bool)
{

  abort();
}
api_bool Graphics_GetGraphicsSmoothInterpolationEnabled(const_graphics_handle)
{

  abort();
}
void Graphics_EnableGraphicsSmoothInterpolation(graphics_handle, api_bool)
{

  abort();
}
int32 Graphics_GetGraphicsCompositionOperator(const_graphics_handle)
{

  abort();
}
void Graphics_SetGraphicsCompositionOperator(graphics_handle, int32)
{

  abort();
}
void Graphics_GetGraphicsOpacity(const_graphics_handle, double*)
{

  abort();
}
void Graphics_SetGraphicsOpacity(graphics_handle, double)
{

  abort();
}
brush_handle Graphics_GetGraphicsBackgroundBrush(const_graphics_handle)
{

  abort();
}
void Graphics_SetGraphicsBackgroundBrush(graphics_handle, const_brush_handle)
{

  abort();
}
api_bool Graphics_GetGraphicsTransparentBackgroundEnabled(const_graphics_handle)
{

  abort();
}
void Graphics_SetGraphicsTransparentBackground(graphics_handle, api_bool)
{

  abort();
}
pen_handle Graphics_GetGraphicsPen(const_graphics_handle)
{

  abort();
}
void Graphics_SetGraphicsPen(graphics_handle, const_pen_handle)
{

  abort();
}
brush_handle Graphics_GetGraphicsBrush(const_graphics_handle)
{

  abort();
}
void Graphics_SetGraphicsBrush(graphics_handle, const_brush_handle)
{

  abort();
}
void Graphics_GetGraphicsBrushOrigin(const_graphics_handle, int32*, int32*)
{

  abort();
}
void Graphics_SetGraphicsBrushOrigin(graphics_handle, int32, int32)
{

  abort();
}
void Graphics_GetGraphicsBrushOriginD(const_graphics_handle, double*, double*)
{

  abort();
}
void Graphics_SetGraphicsBrushOriginD(graphics_handle, double, double)
{

  abort();
}
font_handle Graphics_GetGraphicsFont(const_graphics_handle)
{

  abort();
}
void Graphics_SetGraphicsFont(graphics_handle, const_font_handle)
{

  abort();
}
void Graphics_PushGraphicsState(graphics_handle)
{

  abort();
}
void Graphics_PopGraphicsState(graphics_handle)
{

  abort();
}
void Graphics_DrawPoint(graphics_handle, int32, int32)
{

  abort();
}
void Graphics_DrawPointD(graphics_handle, double, double)
{

  abort();
}
void Graphics_DrawLine(graphics_handle, int32, int32, int32, int32)
{

  abort();
}
void Graphics_DrawLineD(graphics_handle, double, double, double, double)
{

  abort();
}
void Graphics_DrawRect(graphics_handle, int32, int32, int32, int32)
{

  abort();
}
void Graphics_StrokeRect(graphics_handle, int32, int32, int32, int32, const_pen_handle)
{

  abort();
}
void Graphics_FillRect(graphics_handle, int32, int32, int32, int32, const_brush_handle)
{

  abort();
}
void Graphics_DrawRectD(graphics_handle, double, double, double, double)
{

  abort();
}
void Graphics_StrokeRectD(graphics_handle, double, double, double, double, const_pen_handle)
{

  abort();
}
void Graphics_FillRectD(graphics_handle, double, double, double, double, const_brush_handle)
{

  abort();
}
void Graphics_DrawRoundedRect(graphics_handle, int32, int32, int32, int32, double, double)
{

  abort();
}
void Graphics_StrokeRoundedRect(graphics_handle, int32, int32, int32, int32, double, double, const_pen_handle)
{

  abort();
}
void Graphics_FillRoundedRect(graphics_handle, int32, int32, int32, int32, double, double, const_brush_handle)
{

  abort();
}
void Graphics_DrawRoundedRectD(graphics_handle, double, double, double, double, double, double)
{

  abort();
}
void Graphics_StrokeRoundedRectD(graphics_handle, double, double, double, double, double, double, const_pen_handle)
{

  abort();
}
void Graphics_FillRoundedRectD(graphics_handle, double, double, double, double, double, double, const_brush_handle)
{

  abort();
}
void Graphics_DrawEllipse(graphics_handle, int32, int32, int32, int32)
{

  abort();
}
void Graphics_StrokeEllipse(graphics_handle, int32, int32, int32, int32, const_pen_handle)
{

  abort();
}
void Graphics_FillEllipse(graphics_handle, int32, int32, int32, int32, const_brush_handle)
{

  abort();
}
void Graphics_DrawEllipseD(graphics_handle, double, double, double, double)
{

  abort();
}
void Graphics_StrokeEllipseD(graphics_handle, double, double, double, double, const_pen_handle)
{

  abort();
}
void Graphics_FillEllipseD(graphics_handle, double, double, double, double, const_brush_handle)
{

  abort();
}
void Graphics_DrawPolygon(graphics_handle, const int32*, size_type, int32)
{

  abort();
}
void Graphics_StrokePolygon(graphics_handle, const int32*, size_type, int32, const_pen_handle)
{

  abort();
}
void Graphics_FillPolygon(graphics_handle, const int32*, size_type, int32, const_brush_handle)
{

  abort();
}
void Graphics_DrawPolygonD(graphics_handle, const double*, size_type, int32)
{

  abort();
}
void Graphics_StrokePolygonD(graphics_handle, const double*, size_type, int32, const_pen_handle)
{

  abort();
}
void Graphics_FillPolygonD(graphics_handle, const double*, size_type, int32, const_brush_handle)
{

  abort();
}
void Graphics_DrawPolyline(graphics_handle, const int32*, size_type)
{

  abort();
}
void Graphics_DrawPolylineD(graphics_handle, const double*, size_type)
{

  abort();
}
void Graphics_DrawArc(graphics_handle, int32, int32, int32, int32, double, double)
{

  abort();
}
void Graphics_DrawArcD(graphics_handle, double, double, double, double, double, double)
{

  abort();
}
void Graphics_DrawChord(graphics_handle, int32, int32, int32, int32, double, double)
{

  abort();
}
void Graphics_StrokeChord(graphics_handle, int32, int32, int32, int32, double, double, const_pen_handle)
{

  abort();
}
void Graphics_FillChord(graphics_handle, int32, int32, int32, int32, double, double, const_brush_handle)
{

  abort();
}
void Graphics_DrawChordD(graphics_handle, double, double, double, double, double, double)
{

  abort();
}
void Graphics_StrokeChordD(graphics_handle, double, double, double, double, double, double, const_pen_handle)
{

  abort();
}
void Graphics_FillChordD(graphics_handle, double, double, double, double, double, double, const_brush_handle)
{

  abort();
}
void Graphics_DrawPie(graphics_handle, int32, int32, int32, int32, double, double)
{

  abort();
}
void Graphics_StrokePie(graphics_handle, int32, int32, int32, int32, double, double, const_pen_handle)
{

  abort();
}
void Graphics_FillPie(graphics_handle, int32, int32, int32, int32, double, double, const_brush_handle)
{

  abort();
}
void Graphics_DrawPieD(graphics_handle, double, double, double, double, double, double)
{

  abort();
}
void Graphics_StrokePieD(graphics_handle, double, double, double, double, double, double, const_pen_handle)
{

  abort();
}
void Graphics_FillPieD(graphics_handle, double, double, double, double, double, double, const_brush_handle)
{

  abort();
}
void Graphics_DrawBitmap(graphics_handle, int32, int32, const_bitmap_handle)
{

  abort();
}
void Graphics_DrawBitmapD(graphics_handle, double, double, const_bitmap_handle)
{

  abort();
}
void Graphics_DrawBitmapRect(graphics_handle, int32, int32, const_bitmap_handle, int32, int32, int32, int32)
{

  abort();
}
void Graphics_DrawBitmapRectD(graphics_handle, double, double, const_bitmap_handle, double, double, double, double)
{

  abort();
}
void Graphics_DrawScaledBitmap(graphics_handle, int32, int32, int32, int32, const_bitmap_handle)
{

  abort();
}
void Graphics_DrawScaledBitmapD(graphics_handle, double, double, double, double, const_bitmap_handle)
{

  abort();
}
void Graphics_DrawScaledBitmapRect(graphics_handle, int32, int32, int32, int32, const_bitmap_handle, int32, int32, int32, int32)
{

  abort();
}
void Graphics_DrawScaledBitmapRectD(graphics_handle, double, double, double, double, const_bitmap_handle, double, double, double, double)
{

  abort();
}
void Graphics_DrawTiledBitmap(graphics_handle, int32, int32, int32, int32, const_bitmap_handle, int32, int32)
{

  abort();
}
void Graphics_DrawTiledBitmapD(graphics_handle, double, double, double, double, const_bitmap_handle, double, double)
{

  abort();
}
void Graphics_DrawText(graphics_handle, int32, int32, const char16_type*)
{

  abort();
}
void Graphics_DrawTextD(graphics_handle, double, double, const char16_type*)
{

  abort();
}
void Graphics_DrawTextRect(graphics_handle, int32, int32, int32, int32, const char16_type*, int32)
{

  abort();
}
void Graphics_DrawTextRectD(graphics_handle, double, double, double, double, const char16_type*, int32)
{

  abort();
}
void Graphics_GetTextRect(graphics_handle, int32, int32, int32, int32, const char16_type*, int32, int32*, int32*, int32*, int32*)
{

  abort();
}
void Graphics_GetTextRectD(graphics_handle, double, double, double, double, const char16_type*, int32, double*, double*, double*, double*)
{

  abort();
}

// ----------------------------------------------------------------------------
// RealTimePreviewContext API
// ----------------------------------------------------------------------------

api_bool RealTimePreview_SetRealTimePreviewOwner(interface_handle, uint32 flags)
{

  abort();
}
api_bool RealTimePreview_IsRealTimePreviewUpdating()
{

  abort();
}
void RealTimePreview_UpdateRealTimePreview()
{

  abort();
}
void RealTimePreview_ShowRealTimePreviewProgressDialog(const char16_type* title, const char16_type* text, size_type total, uint32 flags)
{

  abort();
}
void RealTimePreview_CloseRealTimePreviewProgressDialog()
{

  abort();
}
api_bool RealTimePreview_IsRealTimePreviewProgressDialogVisible()
{

  abort();
}
void RealTimePreview_SetRealTimePreviewProgressCount(size_type newCount, uint32 flags)
{

  abort();
}
void RealTimePreview_SetRealTimePreviewProgressText(const char16_type* text, uint32 flags)
{

  abort();
}

// ----------------------------------------------------------------------------
// NumericalContext API
// ----------------------------------------------------------------------------

api_bool NumericalContext::GaussJordanInPlaceF(float** A, float** B, int32 rows, int32 cols)
{

  abort();
}
api_bool NumericalContext::GaussJordanInPlaceD(double** A, double** B, int32 rows, int32 cols)
{

  abort();
}
api_bool NumericalContext::SVDInPlaceF(float** A, float* W, float** V, int32 rows, int32 cols)
{

  abort();
}
api_bool NumericalContext::SVDInPlaceD(double** A, double* W, double** V, int32 rows, int32 cols)
{

  abort();
}
api_enum NumericalContext::LinearFitF(double* a, double* b, double* adev, const float* fx, const float* fy, size_type n, api_bool (*callback)( void* ), void*)
{

  abort();
}
api_enum NumericalContext::LinearFitD(double* a, double* b, double* adev, const double* fx, const double* fy, size_type n, api_bool (*callback)( void* ), void*)
{

  abort();
}
api_bool NumericalContext::CubicSplineGenerateF(float* dy2, const float* fx, const float* fy, float dy1, float dyn, int32 n)
{

  abort();
}
api_bool NumericalContext::CubicSplineGenerateD(double* dy2, const double* fx, const double* fy, double dy1, double dyn, int32 n)
{

  abort();
}
api_bool NumericalContext::NaturalCubicSplineGenerateF(float* dy2, const float* fx, const float* fy, int32 n)
{

  abort();
}
api_bool NumericalContext::NaturalCubicSplineGenerateD(double* dy2, const double* fx, const double* fy, int32 n)
{

  abort();
}
api_bool NumericalContext::CubicSplineInterpolateF(float* y, const float* fx, const float* fy, const float* dy2, int32 n, double x, int32* k)
{

  abort();
}
api_bool NumericalContext::CubicSplineInterpolateD(double* y, const double* fx, const double* fy, const double* dy2, int32 n, double x, int32* k)
{

  abort();
}
api_bool NumericalContext::NaturalGridCubicSplineGenerateF(float* dy2, const float* fy, int32 n)
{

  abort();
}
api_bool NumericalContext::NaturalGridCubicSplineGenerateD(double* dy2, const double* fy, int32 n)
{

  abort();
}
api_bool NumericalContext::NaturalGridCubicSplineGenerateUI8(float* dy2, const uint8* fy, int32 n)
{

  abort();
}
api_bool NumericalContext::NaturalGridCubicSplineGenerateUI16(float* dy2, const uint16* fy, int32 n)
{

  abort();
}
api_bool NumericalContext::NaturalGridCubicSplineGenerateUI32(double* dy2, const uint32* fy, int32 n)
{

  abort();
}
api_bool NumericalContext::NaturalGridCubicSplineInterpolateF(float* y, const float* fy, const float* dy2, int32 n, double x)
{

  abort();
}
api_bool NumericalContext::NaturalGridCubicSplineInterpolateD(double* y, const double* fy, const double* dy2, int32 n, double x)
{

  abort();
}
api_bool NumericalContext::NaturalGridCubicSplineInterpolateUI8(float* y, const uint8* fy, const float* dy2, int32 n, double x)
{

  abort();
}
api_bool NumericalContext::NaturalGridCubicSplineInterpolateUI16(float* y, const uint16* fy, const float* dy2, int32 n, double x)
{

  abort();
}
api_bool NumericalContext::NaturalGridCubicSplineInterpolateUI32(double* y, const uint32* fy, const double* dy2, int32 n, double x)
{

  abort();
}
api_bool NumericalContext::SurfaceSplineCreateF(sspline_handle* hSS, int32 rbf, double e2, api_bool polynomial, const float* x, const float* y, const float* z, int32 n, int32 m, float rho, const float* w)
{

  abort();
}
api_bool NumericalContext::SurfaceSplineCreateD(sspline_handle* hSS, int32 rbf, double e2, api_bool polynomial, const double* x, const double* y, const double* z, int32 n, int32 m, float rho, const float* w)
{

  abort();
}
api_bool NumericalContext::SurfaceSplineEvaluate(const_sspline_handle hSS, double* z, double x, double y)
{

  abort();
}
api_bool NumericalContext::SurfaceSplineEvaluateVectorF(const_sspline_handle hSS, float* z, const float *x, const float *y, double x0, double y0, double r, size_type n)
{

  abort();
}
api_bool NumericalContext::SurfaceSplineEvaluateVectorD(const_sspline_handle hSS, double* z, const double *x, const double *y, double x0, double y0, double r, size_type n)
{

  abort();
}
api_bool NumericalContext::SurfaceSplineDestroy(sspline_handle hSS)
{
  // ### The following function returns a null-terminated string allocated by the caller module.
  abort();
}
   char*          (NumericalContext::SurfaceSplineSerialize)( api_handle hModule, const_sspline_handle hSS, uint32 flags )
   {

     abort();
   }
   api_bool       (NumericalContext::SurfaceSplineDeserialize)( sspline_handle* hSS, const char* data, size_type len, uint32 flags )
   {

     abort();
   }

   api_bool       (NumericalContext::SurfaceSplineDuplicate)( sspline_handle* hSS1, const_sspline_handle hSS )
   {

     abort();
   }


// ----------------------------------------------------------------------------
// ThreadContext API
// ----------------------------------------------------------------------------

thread_handle ThreadContext::CreateThread(api_handle handle, api_handle client, uint32 flags)
{
  return pcl_mock::CreateThread(handle, client, flags);
}

void ThreadContext::StartThread(thread_handle handle, uint32 priority)
{
  pcl_mock::StartThread(handle, priority);
}

void ThreadContext::KillThread(thread_handle)
{

  abort();
}

api_bool ThreadContext::IsThreadActive(const_thread_handle handle)
{
  return pcl_mock::IsThreadActive(handle);
}

uint32 ThreadContext::GetThreadPriority(const_thread_handle)
{

  abort();
}
void ThreadContext::SetThreadPriority(thread_handle, uint32)
{

  abort();
}
uint32 ThreadContext::GetThreadStackSize(const_thread_handle)
{

  abort();
}
void ThreadContext::SetThreadStackSize(thread_handle, uint32)
{

  abort();
}
api_bool ThreadContext::WaitThread(thread_handle, uint32 msec)
{

  abort();
}
void ThreadContext::SleepThread(thread_handle, uint32 msec)
{

  abort();
}
uint32 ThreadContext::GetThreadStatus(const_thread_handle)
{

  abort();
}
void ThreadContext::SetThreadStatus(thread_handle, uint32)
{

  abort();
}
api_bool ThreadContext::GetThreadStatusEx(const_thread_handle, uint32* status, uint32 flags)
{
  // 0x00=force_lock 0x01=try_lock
  abort();
}

   api_bool       (ThreadContext::GetThreadConsoleOutputText)( const_thread_handle, char16_type* text, size_type* len )
   {

     abort();
   }
   void           (ThreadContext::AppendThreadConsoleOutputText)( thread_handle, const char16_type* text, api_bool appendNewline )
   {

     abort();
   }
   void           (ThreadContext::ClearThreadConsoleOutputText)( thread_handle )
   {

     abort();
   }

   thread_handle  (ThreadContext::GetCurrentThread)()
   {

     return pcl_mock::GetCurrentThread();
   }

   api_bool       (ThreadContext::SetThreadExecRoutine)( thread_handle handle, pcl::thread_exec_routine dispatcher)
   {

     return pcl_mock::SetThreadExecRoutine(handle, dispatcher);
   }

   int32          (ThreadContext::PerformanceAnalysisValue)( int32 algorithm, size_type length,
                                                        int32 itemSize, api_bool floatingPoint, int32 kernelSize, int32 width, int32 height )
   {

     abort();
   }

// ----------------------------------------------------------------------------
// MutexContext API
// ----------------------------------------------------------------------------

mutex_handle Mutex_CreateMutex(api_handle,  uint32 flags)
{
  // ### deprecated
  abort();
}
   mutex_handle   (Mutex_CreateReadWriteMutex)( api_handle,  uint32 flags )
   {

     abort();
   }

   api_bool       (Mutex_GetLockState)( const_mutex_handle )
   {
     // ### disabled ### returns api_true if the mutex is locked
     abort();
   }

   api_bool       (Mutex_Lock)( mutex_handle, api_bool tryLock )
   {
     // ### deprecated
     abort();
   }
   api_bool       (Mutex_LockForRead)( mutex_handle, api_bool tryLock )
   {

     abort();
   }
   api_bool       (Mutex_LockForWrite)( mutex_handle, api_bool tryLock )
   {

     abort();
   }

   void           (Mutex_Unlock)( mutex_handle )
   {

     abort();
   }

// ----------------------------------------------------------------------------
// ViewListContext API
// ----------------------------------------------------------------------------

control_handle ViewList_CreateViewList(api_handle,  control_handle parent, uint32 flags)
{

  abort();
}
void ViewList_RegenerateViewList(control_handle, api_bool mainViews, api_bool previews, api_bool realTimePreview)
{

  abort();
}
void ViewList_GetViewListContents(const_control_handle, api_bool* mainViews, api_bool* previews, api_bool* realTimePreview)
{

  abort();
}
const_view_handle ViewList_GetViewListExcludedView(const_control_handle)
{

  abort();
}
void ViewList_SetViewListExcludedView(control_handle, const_view_handle)
{

  abort();
}
view_handle ViewList_GetViewListCurrentView(const_control_handle)
{

  abort();
}
void ViewList_SetViewListCurrentView(control_handle, view_handle)
{

  abort();
}
api_bool ViewList_FindViewListView(const_control_handle, const_view_handle)
{

  abort();
}
void ViewList_RemoveViewListView(control_handle, const_view_handle)
{

  abort();
}
api_bool ViewList_SetViewListViewSelectedEventRoutine(control_handle, api_handle, pcl::view_event_routine)
{

  abort();
}
api_bool ViewList_SetViewListCurrentViewUpdatedEventRoutine(control_handle, api_handle, pcl::view_event_routine)
{

  abort();
}

// ----------------------------------------------------------------------------
// SVGContext API
// ----------------------------------------------------------------------------

svg_handle SVG_CreateSVGFile(api_handle, const char16_type*, int32, int32, uint32)
{

  abort();
}
svg_handle SVG_CreateSVGBuffer(api_handle, int32, int32, uint32)
{

  abort();
}
api_bool SVG_GetSVGDimensions(const_svg_handle, int32*, int32*)
{

  abort();
}
api_bool SVG_SetSVGDimensions(svg_handle, int32, int32)
{

  abort();
}
api_bool SVG_GetSVGViewBox(const_svg_handle, double*, double*, double*, double*)
{

  abort();
}
api_bool SVG_SetSVGViewBox(svg_handle, double, double, double, double)
{

  abort();
}
int32 SVG_GetSVGResolution(const_svg_handle)
{

  abort();
}
void SVG_SetSVGResolution(svg_handle, int32)
{

  abort();
}
api_bool SVG_GetSVGFilePath(const_svg_handle, char16_type*, size_type*)
{

  abort();
}
api_bool SVG_GetSVGDataBuffer(const_svg_handle, void*, size_type*)
{

  abort();
}
api_bool SVG_GetSVGTitle(const_svg_handle, char16_type*, size_type*)
{

  abort();
}
void SVG_SetSVGTitle(svg_handle, const char16_type*)
{

  abort();
}
api_bool SVG_GetSVGDescription(const_svg_handle, char16_type*, size_type*)
{

  abort();
}
void SVG_SetSVGDescription(svg_handle, const char16_type*)
{

  abort();
}
api_bool SVG_IsSVGPainting(const_svg_handle)
{

  abort();
}

// ----------------------------------------------------------------------------
// BrushContext API
// ----------------------------------------------------------------------------

brush_handle Brush_CreateBrush(api_handle, uint32, int32)
{

  abort();
}
brush_handle Brush_CreateBitmapBrush(api_handle, const_bitmap_handle)
{

  abort();
}
brush_handle Brush_CreateLinearGradientBrush(api_handle, double x1, double y1, double x2, double y2, int32 spread, const api_gradient_stop*, size_type count)
{
  // spread: 0=pad 1=reflect 2=repeat
  abort();
}
   brush_handle   (Brush_CreateRadialGradientBrush)( api_handle, double cx, double cy, double r, double fx, double fy,
                                                         int32 spread, const api_gradient_stop*, size_type count )
   {

     abort();
   }
   brush_handle   (Brush_CreateConicalGradientBrush)( api_handle, double cx, double cy, double angle,
                                                         const api_gradient_stop*, size_type count )
   {

     abort();
   }
   brush_handle   (Brush_CloneBrush)( api_handle, const_brush_handle )
   {

     abort();
   }

   uint32         (Brush_GetBrushColor)( const_brush_handle )
   {

     abort();
   }
   void           (Brush_SetBrushColor)( brush_handle, uint32 )
   {

     abort();
   }

   int32          (Brush_GetBrushStyle)( const_brush_handle )
   {

     abort();
   }
   void           (Brush_SetBrushStyle)( brush_handle, int32 )
   {

     abort();
   }

   bitmap_handle  (Brush_GetBrushBitmap)( const_brush_handle )
   {

     abort();
   }
   void           (Brush_SetBrushBitmap)( brush_handle, const_bitmap_handle )
   {

     abort();
   }

   int32          (Brush_GetBrushGradientType)( const_brush_handle )
   {
     // 0=none 1=linear 2=radial 3=conical
     abort();
   }
   api_bool       (Brush_GetBrushLinearGradientParameters)( const_brush_handle, double* x1, double* y1, double* x2, double* y2 )
   {

     abort();
   }
   api_bool       (Brush_GetBrushRadialGradientParameters)( const_brush_handle, double* cx, double* cy, double* r, double* fx, double* fy )
   {

     abort();
   }
   api_bool       (Brush_GetBrushConicalGradientParameters)( const_brush_handle, double* cx, double* cy, double* angle )
   {

     abort();
   }
   int32          (Brush_GetBrushGradientSpread)( const_brush_handle )
   {
     // -1=error 0=pad 1=reflect 2=repeat
     abort();
   }
   api_bool       (Brush_GetBrushGradientStops)( const_brush_handle, api_gradient_stop*, size_type *count )
   {

     abort();
   }

// ----------------------------------------------------------------------------
// PenContext API
// ----------------------------------------------------------------------------

pen_handle Pen_CreatePen(api_handle, uint32, float, int32, int32, int32)
{

  abort();
}
pen_handle Pen_ClonePen(api_handle, const_pen_handle)
{

  abort();
}
api_bool Pen_GetPenWidth(const_pen_handle, float*)
{

  abort();
}
void Pen_SetPenWidth(pen_handle, float)
{

  abort();
}
uint32 Pen_GetPenColor(const_pen_handle)
{

  abort();
}
void Pen_SetPenColor(pen_handle, uint32)
{

  abort();
}
int32 Pen_GetPenStyle(const_pen_handle)
{

  abort();
}
void Pen_SetPenStyle(pen_handle, int32)
{

  abort();
}
int32 Pen_GetPenCap(const_pen_handle)
{

  abort();
}
void Pen_SetPenCap(pen_handle, int32)
{

  abort();
}
int32 Pen_GetPenJoin(const_pen_handle)
{

  abort();
}
void Pen_SetPenJoin(pen_handle, int32)
{

  abort();
}
brush_handle Pen_GetPenBrush(const_pen_handle)
{

  abort();
}
void Pen_SetPenBrush(pen_handle, const_brush_handle)
{

  abort();
}


// ----------------------------------------------------------------------------
// ModuleDefinitionContext API
// ----------------------------------------------------------------------------

void ModuleDefinition_EnterModuleDefinitionContext()
{

  abort();
}
api_bool ModuleDefinition_IsModuleDefinitionContextActive()
{

  abort();
}
void ModuleDefinition_SetModuleOnLoadRoutine(pcl::module_on_load_routine)
{

  abort();
}
void ModuleDefinition_SetModuleOnUnloadRoutine(pcl::module_on_unload_routine)
{

  abort();
}
void ModuleDefinition_SetModuleAllocationRoutine(pcl::module_allocation_routine)
{

  abort();
}
void ModuleDefinition_SetModuleDeallocationRoutine(pcl::module_deallocation_routine)
{

  abort();
}
void ModuleDefinition_ExitModuleDefinitionContext()
{

  abort();
}

// ----------------------------------------------------------------------------
// ProcessDefinitionContext API
// ----------------------------------------------------------------------------

void ProcessDefinition_EnterProcessDefinitionContext()
{

  abort();
}
api_bool ProcessDefinition_IsProcessDefinitionContextActive()
{

  abort();
}
void ProcessDefinition_BeginProcessDefinition(meta_process_handle, const char* procId)
{

  abort();
}
api_bool ProcessDefinition_GetProcessBeingDefined(char*, size_type*)
{

  abort();
}
void ProcessDefinition_SetProcessCategory(const char*)
{

  abort();
}
void ProcessDefinition_SetProcessVersion(uint32)
{

  abort();
}
void ProcessDefinition_SetProcessAliasIdentifiers(const char*)
{

  abort();
}
void ProcessDefinition_SetProcessDescription(const char16_type*)
{

  abort();
}
void ProcessDefinition_SetProcessScriptComment(const char16_type*)
{

  abort();
}
void ProcessDefinition_SetProcessIconSVG(const char*)
{

  abort();
}
void ProcessDefinition_SetProcessIconSVGFile(const char16_type*)
{

  abort();
}
void ProcessDefinition_SetProcessIconImage(const char**)
{
  // ### deprecated
  abort();
}
   void        (ProcessDefinition_SetProcessIconImageFile)( const char16_type* )
   {
     // ### deprecated
     abort();
   }
   void        (ProcessDefinition_SetProcessIconSmallImage)( const char** )
   {
     // ### deprecated
     abort();
   }
   void        (ProcessDefinition_SetProcessIconSmallImageFile)( const char16_type* )
   {
     // ### deprecated
     abort();
   }

   void        (ProcessDefinition_SetProcessClassInitializationRoutine)( pcl::process_class_initialization_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetProcessCreationRoutine)( pcl::process_creation_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetProcessDestructionRoutine)( pcl::process_destruction_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetProcessClonationRoutine)( pcl::process_clonation_routine)
   {

     abort();
   }
   void        (ProcessDefinition_SetProcessTestClonationRoutine)( pcl::process_test_clonation_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetProcessSetServerHandleRoutine)( pcl::process_set_handle_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetProcessAssignmentRoutine)( pcl::process_assignment_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetProcessInitializationRoutine)( pcl::process_initialization_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetProcessValidationRoutine)( pcl::process_validation_routine )
   {

     abort();
   }

   void        (ProcessDefinition_SetProcessCommandLineProcessingRoutine)( pcl::process_command_line_processing_routine, uint32 flags )
   {

     abort();
   }
   void        (ProcessDefinition_SetProcessEditPreferencesRoutine)( pcl::process_edit_preferences_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetProcessBrowseDocumentationRoutine)( pcl::process_browse_documentation_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetProcessExecutionPreferencesRoutine)( pcl::process_execution_preferences_routine )
   {

     abort();
   }

   void        (ProcessDefinition_SetProcessExecutionValidationRoutine)( pcl::process_execution_validation_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetProcessMaskValidationRoutine)( pcl::process_mask_validation_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetProcessHistoryUpdateValidationRoutine)( pcl::process_history_update_validation_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetProcessUndoModeRoutine)( pcl::process_undo_mode_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetProcessPreExecutionRoutine)( pcl::process_pre_execution_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetProcessExecutionRoutine)( pcl::process_execution_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetProcessPostExecutionRoutine)( pcl::process_post_execution_routine )
   {

     abort();
   }

   void        (ProcessDefinition_SetProcessGlobalExecutionValidationRoutine)( pcl::process_global_execution_validation_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetProcessPreGlobalExecutionRoutine)( pcl::process_pre_global_execution_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetProcessGlobalExecutionRoutine)( pcl::process_global_execution_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetProcessPostGlobalExecutionRoutine)( pcl::process_post_global_execution_routine )
   {

     abort();
   }

   void        (ProcessDefinition_SetProcessImageExecutionValidationRoutine)( pcl::process_image_execution_validation_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetProcessImageExecutionRoutine)( pcl::process_image_execution_routine )
   {

     abort();
   }

   void        (ProcessDefinition_SetProcessDefaultInterfaceSelectionRoutine)( pcl::process_default_interface_selection_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetProcessInterfaceSelectionRoutine)( pcl::process_interface_selection_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetProcessInterfaceValidationRoutine)( pcl::process_interface_validation_routine )
   {

     abort();
   }

   void        (ProcessDefinition_SetProcessPreReadingRoutine)( pcl::process_pre_reading_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetProcessPostReadingRoutine)( pcl::process_post_reading_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetProcessPreWritingRoutine)( pcl::process_pre_writing_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetProcessPostWritingRoutine)( pcl::process_post_writing_routine )
   {

     abort();
   }

   void        (ProcessDefinition_SetProcessIPCStartRoutine)( pcl::process_ipc_notification_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetProcessIPCStopRoutine)( pcl::process_ipc_notification_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetProcessIPCSetParametersRoutine)( pcl::process_ipc_notification_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetProcessIPCGetStatusRoutine)( pcl::process_ipc_status_routine )
   {

     abort();
   }

   void        (ProcessDefinition_BeginParameterDefinition)( meta_parameter_handle, const char* parId, uint32 parType )
   {

     abort();
   }
   api_bool    (ProcessDefinition_GetParameterBeingDefined)( char*, size_type* )
   {

     abort();
   }
   void        (ProcessDefinition_SetParameterProcessVersionRange)( uint32, uint32 )
   {

     abort();
   }
   void        (ProcessDefinition_SetParameterRequired)( api_bool )
   {

     abort();
   }
   void        (ProcessDefinition_SetParameterReadOnly)( api_bool )
   {

     abort();
   }
   void        (ProcessDefinition_SetParameterAliasIdentifiers)( const char* )
   {

     abort();
   }
   void        (ProcessDefinition_SetParameterDescription)( const char16_type* )
   {

     abort();
   }
   void        (ProcessDefinition_SetParameterScriptComment)( const char16_type* )
   {

     abort();
   }
   void        (ProcessDefinition_SetParameterLockRoutine)( pcl::parameter_lock_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetParameterUnlockRoutine)( pcl::parameter_unlock_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetParameterValidationRoutine)( pcl::parameter_validation_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetParameterAllocationRoutine)( pcl::parameter_allocation_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetParameterLengthQueryRoutine)( pcl::parameter_length_query_routine )
   {

     abort();
   }
   void        (ProcessDefinition_SetDefaultNumericValue)( double )
   {

     abort();
   }
   void        (ProcessDefinition_SetValidNumericRange)( double, double )
   {

     abort();
   }
   void        (ProcessDefinition_SetPrecision)( int32 )
   {

     abort();
   }
   void        (ProcessDefinition_SetScientificNotation)( api_bool )
   {

     abort();
   }
   void        (ProcessDefinition_SetDefaultBooleanValue)( api_bool )
   {

     abort();
   }
   void        (ProcessDefinition_DefineEnumerationElement)( const char*, api_enum )
   {

     abort();
   }
   void        (ProcessDefinition_DefineEnumerationAlias)( const char*, const char* )
   {

     abort();
   }
   void        (ProcessDefinition_SetDefaultEnumerationValueIndex)( uint32 )
   {

     abort();
   }
   void        (ProcessDefinition_SetDefaultStringValue)( const char16_type* )
   {

     abort();
   }
   void        (ProcessDefinition_SetStringAllowedCharacters)( const char16_type* )
   {

     abort();
   }
   void        (ProcessDefinition_SetStringLengthLimits)( size_type, size_type )
   {

     abort();
   }
   void        (ProcessDefinition_BeginTableColumnDefinition)( meta_parameter_handle, const char* colId, uint32 colType )
   {

     abort();
   }
   void        (ProcessDefinition_EndTableColumnDefinition)()
   {

     abort();
   }
   void        (ProcessDefinition_SetTableRowLimits)( size_type, size_type )
   {

     abort();
   }
   void        (ProcessDefinition_SetBlockSizeLimits)( size_type, size_type )
   {

     abort();
   }

   void        (ProcessDefinition_EndParameterDefinition)()
   {

     abort();
   }
   void        (ProcessDefinition_EndProcessDefinition)()
   {

     abort();
   }
   void        (ProcessDefinition_ExitProcessDefinitionContext)()
   {

     abort();
   }

// ----------------------------------------------------------------------------
// InterfaceDefinitionContext API
// ----------------------------------------------------------------------------

void InterfaceDefinitionContext::EnterInterfaceDefinitionContext()
{

  abort();
}
api_bool InterfaceDefinitionContext::IsInterfaceDefinitionContextActive()
{

  abort();
}
void InterfaceDefinitionContext::BeginInterfaceDefinition(meta_interface_handle, const char* ifaceId, uint32 flags)
{

  abort();
}
api_bool InterfaceDefinitionContext::GetInterfaceBeingDefined(char*, size_type*)
{

  abort();
}
void InterfaceDefinitionContext::SetInterfaceVersion(uint32)
{

  abort();
}
void InterfaceDefinitionContext::SetInterfaceAliasIdentifiers(const char*)
{

  abort();
}
void InterfaceDefinitionContext::SetInterfaceDescription(const char16_type*)
{

  abort();
}
void InterfaceDefinitionContext::SetInterfaceIconSVG(const char*)
{

  abort();
}
void InterfaceDefinitionContext::SetInterfaceIconSVGFile(const char16_type*)
{

  abort();
}
void InterfaceDefinitionContext::SetInterfaceIconImage(const char**)
{
  // ### deprecated
  abort();
}
   void           (InterfaceDefinitionContext::SetInterfaceIconImageFile)( const char16_type* )
   {
     // ### deprecated
     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceIconSmallImage)( const char** )
   {
     // ### deprecated
     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceIconSmallImageFile)( const char16_type* )
   {
     // ### deprecated
     abort();
   }

   void           (InterfaceDefinitionContext::SetInterfaceFeatures)( uint32, uint32 )
   {

     abort();
   }

   void           (InterfaceDefinitionContext::SetInterfaceInitializationRoutine)( pcl::interface_initialization_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceLaunchRoutine)( pcl::interface_launch_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceProcessInstantiationRoutine)( pcl::interface_process_instantiation_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceProcessTestInstantiationRoutine)( pcl::interface_process_instantiation_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceProcessValidationRoutine)( pcl::interface_process_validation_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceProcessImportRoutine)( pcl::interface_process_import_routine )
   {

     abort();
   }

   void           (InterfaceDefinitionContext::SetInterfaceApplyRoutine)( pcl::interface_control_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceApplyGlobalRoutine)( pcl::interface_control_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceRealTimePreviewUpdatedRoutine)( pcl::interface_control_state_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceExecuteRoutine)( pcl::interface_control_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceCancelRoutine)( pcl::interface_control_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceBrowseDocumentationRoutine)( pcl::interface_control_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceTrackViewUpdatedRoutine)( pcl::interface_control_state_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceEditPreferencesRoutine)( pcl::interface_control_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceResetRoutine)( pcl::interface_control_routine )
   {

     abort();
   }

   void           (InterfaceDefinitionContext::SetInterfaceRealTimeUpdateQueryRoutine)( pcl::interface_real_time_update_query_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceRealTimeGenerationFlagsRoutine)( pcl::interface_real_time_generation_flags_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceRealTimeGenerationRoutine)( pcl::interface_real_time_generation_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceRealTimeCancelRoutine)( pcl::interface_real_time_cancel_routine )
   {

     abort();
   }

   void           (InterfaceDefinitionContext::SetInterfaceDynamicModeEnterRoutine)( pcl::interface_dynamic_mode_enter_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceDynamicModeExitRoutine)( pcl::interface_dynamic_mode_exit_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceDynamicMouseEnterRoutine)( pcl::interface_dynamic_view_event_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceDynamicMouseLeaveRoutine)( pcl::interface_dynamic_view_event_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceDynamicMouseMoveRoutine)( pcl::interface_dynamic_mouse_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceDynamicMousePressRoutine)( pcl::interface_dynamic_mouse_button_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceDynamicMouseReleaseRoutine)( pcl::interface_dynamic_mouse_button_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceDynamicMouseDoubleClickRoutine)( pcl::interface_dynamic_mouse_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceDynamicKeyPressRoutine)( pcl::interface_dynamic_keyboard_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceDynamicKeyReleaseRoutine)( pcl::interface_dynamic_keyboard_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceDynamicMouseWheelRoutine)( pcl::interface_dynamic_wheel_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceDynamicUpdateQueryRoutine)( pcl::interface_dynamic_update_query_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetInterfaceDynamicPaintRoutine)( pcl::interface_dynamic_paint_routine )
   {

     abort();
   }

   void           (InterfaceDefinitionContext::SetImageCreatedNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetImageUpdatedNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetImageRenamedNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetImageDeletedNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetImageFocusedNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetImageLockedNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetImageUnlockedNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetImageSTFEnabledNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetImageSTFDisabledNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetImageSTFUpdatedNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetImageRGBWSUpdatedNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetImageCMEnabledNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetImageCMDisabledNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetImageCMUpdatedNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetImageSavedNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }

   void           (InterfaceDefinitionContext::SetMaskUpdatedNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetMaskEnabledNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetMaskDisabledNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetMaskShownNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetMaskHiddenNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }

   void           (InterfaceDefinitionContext::SetTransparencyHiddenNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetTransparencyModeUpdatedNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }

   void           (InterfaceDefinitionContext::SetViewPropertyUpdatedNotificationRoutine)( pcl::view_property_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetViewPropertyDeletedNotificationRoutine)( pcl::view_property_notification_routine )
   {

     abort();
   }

   void           (InterfaceDefinitionContext::SetBeginReadoutNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetUpdateReadoutNotificationRoutine)( pcl::readout_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetEndReadoutNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }

   void           (InterfaceDefinitionContext::SetProcessCreatedNotificationRoutine)( pcl::process_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetProcessUpdatedNotificationRoutine)( pcl::process_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetProcessDeletedNotificationRoutine)( pcl::process_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetProcessSavedNotificationRoutine)( pcl::process_notification_routine )
   {

     abort();
   }

   void           (InterfaceDefinitionContext::SetRealTimePreviewOwnerChangeNotificationRoutine)( pcl::interface_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetRealTimePreviewLUTUpdatedNotificationRoutine)( pcl::lut_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetRealTimePreviewGenerationStartNotificationRoutine)( pcl::global_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetRealTimePreviewGenerationFinishNotificationRoutine)( pcl::global_notification_routine )
   {

     abort();
   }

   void           (InterfaceDefinitionContext::SetGlobalRGBWSUpdatedNotificationRoutine)( pcl::global_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetGlobalCMEnabledNotificationRoutine)( pcl::global_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetGlobalCMDisabledNotificationRoutine)( pcl::global_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetGlobalCMUpdatedNotificationRoutine)( pcl::global_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetReadoutOptionsUpdatedNotificationRoutine)( pcl::global_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetGlobalPreferencesUpdatedNotificationRoutine)( pcl::global_notification_routine )
   {

     abort();
   }
   void           (InterfaceDefinitionContext::SetGlobalFiltersUpdatedNotificationRoutine)( pcl::global_notification_routine )
   {

     abort();
   }

   void           (InterfaceDefinitionContext::EndInterfaceDefinition)()
   {

     abort();
   }
   void           (InterfaceDefinitionContext::ExitInterfaceDefinitionContext)()
   {

     abort();
   }

// ----------------------------------------------------------------------------
// SharedImageContext API
// ----------------------------------------------------------------------------

void *SharedImageContext::GetImageOwner(const_image_handle)
{

  abort();
}
api_bool SharedImageContext::GetImageRefCount(const_image_handle, uint32*)
{

  abort();
}
api_bool SharedImageContext::IsValidImageHandle(const_image_handle)
{

  abort();
}
api_bool SharedImageContext::AttachToImage(image_handle, void*)
{

  abort();
}

api_bool SharedImageContext::GetImageFormat(const_image_handle, uint32* nbits, api_bool* flt)
{

  abort();
}

api_bool SharedImageContext::SetImageRGBWS(image_handle, const api_RGBWS*)
{
  // ### must be set through ImageWindow
  abort();
}

void ControlContext::GetClientRect(const_control_handle h,
                                   int32* x, int32* y,
                                   int32* w, int32* hgt)
{
    if (!w || !hgt)
        return;

    auto* C = get(h);              // whatever you use elsewhere: get(control_handle)
    if (!C || !C->widget)
        return;

    const QRect r = C->widget->contentsRect();

    if (x)   *x   = r.x();
    if (y)   *y   = r.y();
    *w   = r.width();
    *hgt = r.height();
}

int32 ComboBoxContext::GetComboBoxLength(const_control_handle) { return 1; }
void ComboBoxContext::InsertComboBoxItem(control_handle, int32, const char16_type*, const_bitmap_handle) { logf("ComboBoxContext::InsertComboBoxItem");  }
void ComboBoxContext::SetComboBoxCurrentItem(control_handle, int32) { logf("ComboBoxContext::SetComboBoxCurrentItem");  }
api_bool ComboBoxContext::SetComboBoxItemSelectedEventRoutine(control_handle, api_handle, pcl::value_event_routine ) { return api_true; }
api_bool       (ControlContext::GetControlDisplayPixelRatio)( const_control_handle, double* ratio)
   {
     *ratio = 1.0;
     return api_true;
   }
api_bool       (ControlContext::GetControlResourcePixelRatio)( const_control_handle, double* ratio)
   {
     *ratio = 1.0;
     return api_true;
   }
api_bool ControlContext::GetControlEnabled(const_control_handle) { abort(); }
  font_handle ControlContext::GetControlFont(const_control_handle) { return new QFont; }
void ControlContext::GetControlMaxSize(const_control_handle, int32*x, int32*y) { *x = 100; *y = 100; }
void ControlContext::GetControlMinSize(const_control_handle, int32*x, int32*y) { *x = 100; *y = 100; }
void ControlContext::GetControlPosition(const_control_handle, int32*x, int32*y)
{
  *x = 0;
  *y = 0;
}
void ControlContext::SetControlEnabled( control_handle, api_bool) {  }
void ControlContext::SetControlFocus(control_handle, api_bool ) { abort(); }
void ControlContext::SetControlPosition(control_handle, int32, int32) { abort(); }
void ControlContext::SetControlSize(control_handle, int32, int32) { abort(); }
api_bool ControlContext::SetGetFocusEventRoutine(control_handle, api_handle, pcl::control_event_routine) { logf("ControlContext::SetGetFocusEventRoutine"); return api_true; }
api_bool ControlContext::SetLoseFocusEventRoutine(control_handle, api_handle, pcl::control_event_routine) { logf("ControlContext::SetLoseFocusEventRoutine"); return api_true; }
void ControlContext::SetRealTimePreviewActive(control_handle, api_bool) { abort(); }
api_bool EditContext::GetEditReadOnly(const_control_handle) { abort(); }
api_bool EditContext::GetEditText(const_control_handle, char16_type*, size_type*) { abort(); }
void EditContext::SetEditSelected(control_handle, api_bool) { abort(); }

api_bool EditContext::SetEditValidatingRegExp(
    control_handle h,
    const char16_type* pattern,
    api_bool caseSensitive )
{
  logf("Edit_SetEditValidatingRegExp");
    auto* edit = qobject_cast<QLineEdit*>(widgetFromHandle(h));
    if (!edit) return api_false;

    QString pat = QString::fromUtf16(pattern);
    QRegularExpression re(pat);

    if (!caseSensitive)
        re.setPatternOptions(QRegularExpression::CaseInsensitiveOption);

    edit->setValidator(new QRegularExpressionValidator(re, edit));
    return api_true;
}

void EditContext::SetEditText( control_handle h, const char16_type* t )
{
  logf("Edit_SetEditText"); 
    if (auto* edit = qobject_cast<QLineEdit*>(widgetFromHandle(h)))
        edit->setText(QString::fromUtf16(t));
}

font_handle FontContext::CloneFont(api_handle, const_font_handle) { abort(); }

int32 FontContext::GetStringPixelWidth( const_font_handle, const char16_type* text )
{
    QString s = QString::fromUtf16(text);

    QFont f;                 // default font
    QFontMetrics fm(f);      // <-- no vexing parse

    return fm.horizontalAdvance(s);
}

api_bool GlobalContext::Abort() { abort(); }
void *GlobalContext::Allocate(size_type) { abort(); }
api_bool GlobalContext::BrowseProcessDocumentation(meta_process_handle, uint32 flags) { abort(); }
api_bool GlobalContext::Deallocate(void *) { abort(); }
api_bool GlobalContext::EnableAbort() { abort(); }
api_bool GlobalContext::ErrorMessage(uint32, char16_type*, size_type*) { abort(); }
console_handle GlobalContext::GetConsole() { return (new MockBase()); }
api_bool GlobalContext::GetGlobalInteger(const char*, void*, api_bool isSigned) { abort(); }
uint32 GlobalContext::GetKeyboardModifiers() { abort(); }
uint32 GlobalContext::GetProcessStatus() { abort(); }
uint32 GlobalContext::LastError() { return 0; }
void GlobalContext::LaunchProcessInstance(meta_process_handle, const_process_handle, int32 mode, uint32 flags) { abort(); }
void GlobalContext::LaunchProcessInstanceOnView(meta_process_handle, const_process_handle, view_handle, uint32 flags) { abort(); }
uint32 GlobalContext::MessageBox(const char16_type* text, const char16_type* caption, uint32 button0, uint32 button1, uint32 button2, uint32 defButton, uint32 escButton, uint32 icon) { abort(); }

api_bool    (GlobalContext::ReadSettingsInteger)( api_handle, int32*rslt, const char* key, api_bool global )
{
  *rslt = 0;
  return api_true;
}

api_bool GlobalContext::ShowConsole(console_handle, api_bool ) { abort(); }
api_bool GlobalContext::WriteConsole(console_handle, const char16_type*, api_bool appendNewline ) { return api_true; }
api_bool GlobalContext::WriteSettingsInteger( api_handle, int32, const char* key, api_bool global ) { abort(); }

void LabelContext::SetLabelText( control_handle h, const char16_type* t )
{
    if (auto* w = qobject_cast<QLabel*>(widgetFromHandle(h)))
        w->setText(QString::fromUtf16(t));
}

void LabelContext::SetLabelAlignment( control_handle h, int32 flags )
{
    if (auto* w = qobject_cast<QLabel*>(widgetFromHandle(h)))
    {
        Qt::Alignment a{};
        uint32 f = uint32(flags);

        if (f & 0x01) a |= Qt::AlignLeft;
        if (f & 0x02) a |= Qt::AlignHCenter;
        if (f & 0x04) a |= Qt::AlignRight;
        if (f & 0x10) a |= Qt::AlignTop;
        if (f & 0x20) a |= Qt::AlignVCenter;
        if (f & 0x40) a |= Qt::AlignBottom;

        if (!a) a = Qt::AlignLeft | Qt::AlignVCenter;

        w->setAlignment(a);
    }
}

api_bool SizerContext::GetSizerDisplayPixelRatio( const_sizer_handle s, double* ratio )
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

void SizerContext::InsertSizerSpacing( sizer_handle s, int32 index, int32 px )
{
    if (QBoxLayout* l = layoutFromSizer(s))
    {
        if (index < 0 || index > l->count())
            index = l->count();
        l->insertSpacing(index, px);
    }
}

void SizerContext::InsertSizerStretch( sizer_handle s, int32 index, int32 stretch )
{
    if (QBoxLayout* l = layoutFromSizer(s))
    {
        if (index < 0 || index > l->count())
            index = l->count();
        l->insertStretch(index, stretch);
    }
}

void SizerContext::SetSizerMargin( sizer_handle s, int32 px )
{
    if (QBoxLayout* l = layoutFromSizer(s))
        l->setContentsMargins(px,px,px,px);
}

void SizerContext::SetSizerSpacing( sizer_handle s, int32 px )
{
    if (QBoxLayout* l = layoutFromSizer(s))
        l->setSpacing(px);
}

process_handle ProcessContext::CloneProcessInstance(api_handle, const_process_handle, uint32 flags ) { abort(); }
api_bool SharedImageContext::DetachFromImage(image_handle, void*) { abort(); }
api_bool SharedImageContext::GetImageColorSpace(const_image_handle, uint32* cs ) { abort(); }
api_bool SharedImageContext::GetImageGeometry(const_image_handle, uint32* w, uint32* h, uint32* n) { abort(); }
api_bool SharedImageContext::GetImagePixelData( image_handle, void*** ) { abort(); }
api_bool SharedImageContext::GetImageRGBWS(const_image_handle, api_RGBWS*) { abort(); }
api_bool SharedImageContext::SetImageColorSpace(image_handle, uint32 cs ) { abort(); }
api_bool SharedImageContext::SetImageGeometry(image_handle, uint32 w, uint32 h, uint32 n ) { abort(); }
api_bool SharedImageContext::SetImagePixelData(image_handle, void**) { abort(); }
void SliderContext::GetSliderRange(const_control_handle handle, int32* minValue, int32* maxValue) { *minValue = 0; *maxValue=100; }
int32 SliderContext::GetSliderValue(const_control_handle) { abort(); }

void SliderContext::SetSliderRange( control_handle h, int32 mn, int32 mx )
{
    if (auto* s = qobject_cast<QSlider*>(widgetFromHandle(h)))
    {
        s->setMinimum(mn);
        s->setMaximum(mx);
    }
}

void SliderContext::SetSliderPageSize( control_handle h, int32 page )
{
    if (auto* s = qobject_cast<QSlider*>(widgetFromHandle(h)))
        s->setPageStep(page);
}

void SliderContext::SetSliderTickInterval( control_handle h, int32 interval )
{
    if (auto* s = qobject_cast<QSlider*>(widgetFromHandle(h)))
        s->setTickInterval(interval);
}

void SliderContext::SetSliderTickStyle( control_handle h, int32 style )
{
    if (auto* s = qobject_cast<QSlider*>(widgetFromHandle(h)))
    {
        uint32 st = uint32(style);
        QSlider::TickPosition pos = QSlider::NoTicks;

        if (st & 0x01) pos = QSlider::TicksAbove;
        if (st & 0x02) pos = QSlider::TicksBelow;
        if (st & 0x03) pos = QSlider::TicksBothSides;

        s->setTickPosition(pos);
    }
}

    image_handle   (ViewContext::GetViewImage)( view_handle ) { abort(); }
    api_bool       (DialogContext::ExecuteOpenFileDialog)( unsigned short*, unsigned short const*, unsigned short const*, unsigned short const*, unsigned short const*) { }
    api_bool    (GlobalContext::GetGlobalString)( const char*, char16_type*, size_type* ) { abort(); }
    api_bool       (ControlContext::GetControlVisible)( const_control_handle ) { return api_true; }
    api_bool       (NumericalContext::FFTDestroyTransform)( fft_handle hFFT ) { abort(); }
    size_type      (NumericalContext::FFTRealOptimizedLengthF)( size_type n ) { abort(); }
    fft_handle     (NumericalContext::FFTCreateComplexTransformD)( size_type n ) { abort(); }
    api_bool             (FileFormatContext::GetFileFormatName)( meta_format_handle, char*, size_type* ) { abort(); }
    file_format_handle   (FileFormatContext::CreateFileFormatInstance)( api_handle, meta_format_handle ) { abort(); }
    api_bool             (FileFormatContext::ReadImage)( file_format_handle, image_handle ) { abort(); }

int32 FontContext::GetFontHeight(void const*) { return 12; }
api_bool DialogContext::ExecuteOpenMultipleFilesDialog(unsigned short*, unsigned int (*)(unsigned short const*, void*), void*, unsigned short const*, unsigned short const*, unsigned short const*, unsigned short const*) { abort(); }
api_bool GlobalContext::GetGlobalFlag(char const*, unsigned int*) { abort(); }
void GlobalContext::ProcessEvents(unsigned int) { abort(); }
int32 GlobalContext::MaxProcessorsAllowedForModule(void*, unsigned int) { abort(); }
control_handle ControlContext::GetControlParent(const_control_handle) { abort(); }
void ControlContext::SetControlMaxSize(control_handle, int, int) { abort(); }
api_bool ControlContext::SetFileDragEventRoutine(control_handle, api_handle, pcl::file_drag_event_handler) { return api_true; }
api_bool ControlContext::SetFileDropEventRoutine(control_handle, api_handle, pcl::file_drag_event_handler) { return api_true; }
void ControlContext::SetControlUpdatesEnabled(control_handle, unsigned int) {  }
api_bool GraphicsContext::GetGraphicsStatus(void const*) { abort(); }
void GraphicsContext::EndPaint(void*) { abort(); }
api_bool NumericalContext::FFTRealTransformD(void*, void*, double const*) { abort(); }
api_bool NumericalContext::FFTComplexTransformD(void*, void*, void const*) { abort(); }
fft_handle NumericalContext::FFTCreateRealTransformD(unsigned long) { abort(); }
api_bool NumericalContext::FFTComplexInverseTransformD(void*, void*, void const*) { abort(); }
fft_handle NumericalContext::FFTCreateComplexInverseTransformD(unsigned long) { abort(); }
api_bool FileFormatContext::GetImageId(void const*, char*, unsigned long*, unsigned int) { abort(); }
api_bool FileFormatContext::SelectImage(void*, unsigned int) { abort(); }
api_bool FileFormatContext::GetImageCount(void const*) { abort(); }
api_bool FileFormatContext::OpenImageFileEx(void*, unsigned short const*, char const*, unsigned int) { abort(); }
bitmap_handle FileFormatContext::GetFileFormatIcon(void const*) { abort(); }
meta_format_handle FileFormatContext::GetFileFormatByName(void*, char const*) { abort(); }
api_bool FileFormatContext::GetFileFormatStatus(void const*, unsigned short*, unsigned long*, void*) { abort(); }
api_bool FileFormatContext::GetImageDescription(void const*, api_image_info*, api_image_options*, unsigned int) { abort(); }
uint32 FileFormatContext::GetFileFormatVersion(void const*) { abort(); }
api_bool FileFormatContext::GetFileFormatMimeTypes(void const*, char**, unsigned long*, unsigned long*) { abort(); }
bitmap_handle FileFormatContext::GetFileFormatSmallIcon(void const*) { abort(); }
meta_format_handle FileFormatContext::GetFileFormatByMimeType(void*, char const*, unsigned int, unsigned int) { abort(); }
api_bool FileFormatContext::GetFileFormatDescription(void const*, unsigned short*, unsigned long*) { abort(); }
void FileFormatContext::DisposeFormatSpecificData(void const*, void const*) { abort(); }
api_bool FileFormatContext::EditFileFormatPreferences(void const*) { abort(); }
api_bool FileFormatContext::GetFileFormatCapabilities(void const*, api_format_capabilities*) { abort(); }
api_bool FileFormatContext::ValidateFormatSpecificData(void const*, void const*) { abort(); }
api_bool FileFormatContext::GetFileFormatFileExtensions(void const*, unsigned short**, unsigned long*, unsigned long*) { abort(); }
api_bool FileFormatContext::GetFileFormatImplementation(void const*, unsigned short*, unsigned long*) { abort(); }
meta_format_handle FileFormatContext::GetFileFormatByFileExtension(void*, unsigned short const*, unsigned int, unsigned int) { abort(); }
window_handle ImageWindowContext::CreateImageWindow(int, int, int, int, unsigned int, unsigned int, unsigned int, char const*) { abort(); }
image_handle SharedImageContext::CreateImage(unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, void*) { abort(); }
void ModuleDefinitionContext::SetModuleOnLoadRoutine(void (*)()) { abort(); }
void ModuleDefinitionContext::SetModuleOnUnloadRoutine(void (*)()) { abort(); }
void ModuleDefinitionContext::SetModuleAllocationRoutine(void* (*)(unsigned long)) { abort(); }
void ModuleDefinitionContext::ExitModuleDefinitionContext() { abort(); }
void ModuleDefinitionContext::EnterModuleDefinitionContext() { abort(); }
void ModuleDefinitionContext::SetModuleDeallocationRoutine(void (*)(void*)) { abort(); }
void ProcessDefinitionContext::SetPrecision(int) { abort(); }
void ProcessDefinitionContext::SetProcessIconSVG(char const*) { abort(); }
void ProcessDefinitionContext::SetProcessVersion(unsigned int) { abort(); }
void ProcessDefinitionContext::SetTableRowLimits(unsigned long, unsigned long) { abort(); }
void ProcessDefinitionContext::SetProcessCategory(char const*) { abort(); }
void ProcessDefinitionContext::SetProcessIconImage(char const**) { abort(); }
void ProcessDefinitionContext::EndProcessDefinition() { abort(); }
void ProcessDefinitionContext::SetParameterReadOnly(unsigned int) { abort(); }
void ProcessDefinitionContext::SetParameterRequired(unsigned int) { abort(); }
void ProcessDefinitionContext::SetValidNumericRange(double, double) { abort(); }
void ProcessDefinitionContext::SetDefaultStringValue(unsigned short const*) { abort(); }
void ProcessDefinitionContext::SetProcessDescription(unsigned short const*) { abort(); }
void ProcessDefinitionContext::SetProcessIconSVGFile(unsigned short const*) { abort(); }
void ProcessDefinitionContext::SetScientificNotation(unsigned int) { abort(); }
void ProcessDefinitionContext::SetStringLengthLimits(unsigned long, unsigned long) { abort(); }
void ProcessDefinitionContext::BeginProcessDefinition(void const*, char const*) { abort(); }
void ProcessDefinitionContext::EndParameterDefinition() { abort(); }
void ProcessDefinitionContext::SetDefaultBooleanValue(unsigned int) { abort(); }
void ProcessDefinitionContext::SetDefaultNumericValue(double) { abort(); }
void ProcessDefinitionContext::SetParameterDescription(unsigned short const*) { abort(); }
void ProcessDefinitionContext::SetParameterLockRoutine(void* (*)(void*, void const*, unsigned long)) { abort(); }
void ProcessDefinitionContext::SetProcessIconImageFile(unsigned short const*) { abort(); }
void ProcessDefinitionContext::SetProcessScriptComment(unsigned short const*) { abort(); }
void ProcessDefinitionContext::BeginParameterDefinition(void const*, char const*, unsigned int) { abort(); }
void ProcessDefinitionContext::EndTableColumnDefinition() { abort(); }
void ProcessDefinitionContext::SetProcessIPCStopRoutine(void (*)(void const*, int, char const*, unsigned short const*)) { abort(); }
void ProcessDefinitionContext::SetProcessIconSmallImage(char const**) { abort(); }
void ProcessDefinitionContext::SetParameterScriptComment(unsigned short const*) { abort(); }
void ProcessDefinitionContext::SetParameterUnlockRoutine(void (*)(void*, void const*, unsigned long)) { abort(); }
void ProcessDefinitionContext::SetProcessCreationRoutine(void* (*)(void const*)) { abort(); }
void ProcessDefinitionContext::SetProcessIPCStartRoutine(void (*)(void const*, int, char const*, unsigned short const*)) { abort(); }
void ProcessDefinitionContext::SetProcessUndoModeRoutine(unsigned int (*)(void const*, void const*)) { abort(); }
void ProcessDefinitionContext::BeginTableColumnDefinition(void const*, char const*, unsigned int) { abort(); }
void ProcessDefinitionContext::SetProcessAliasIdentifiers(char const*) { abort(); }
void ProcessDefinitionContext::SetProcessClonationRoutine(void* (*)(void const*)) { abort(); }
void ProcessDefinitionContext::SetProcessExecutionRoutine(unsigned int (*)(void*, void*)) { abort(); }
void ProcessDefinitionContext::SetStringAllowedCharacters(unsigned short const*) { abort(); }
void ProcessDefinitionContext::SetProcessAssignmentRoutine(unsigned int (*)(void*, void const*)) { abort(); }
void ProcessDefinitionContext::SetProcessPreReadingRoutine(unsigned int (*)(void*)) { abort(); }
void ProcessDefinitionContext::SetProcessPreWritingRoutine(unsigned int (*)(void const*)) { abort(); }
void ProcessDefinitionContext::SetProcessValidationRoutine(unsigned int (*)(void*, unsigned short*, unsigned int)) { abort(); }
void ProcessDefinitionContext::ExitProcessDefinitionContext() { abort(); }
void ProcessDefinitionContext::SetParameterAliasIdentifiers(char const*) { abort(); }
void ProcessDefinitionContext::SetProcessDestructionRoutine(void (*)(void*)) { abort(); }
void ProcessDefinitionContext::SetProcessIconSmallImageFile(unsigned short const*) { abort(); }
void ProcessDefinitionContext::SetProcessPostReadingRoutine(void (*)(void*)) { abort(); }
void ProcessDefinitionContext::SetProcessPostWritingRoutine(void (*)(void const*)) { abort(); }
void ProcessDefinitionContext::EnterProcessDefinitionContext() { abort(); }
void ProcessDefinitionContext::SetParameterAllocationRoutine(unsigned int (*)(unsigned long, void*, void const*, unsigned long)) { abort(); }
void ProcessDefinitionContext::SetParameterValidationRoutine(unsigned int (*)(void*, void const*, void const*, unsigned long)) { abort(); }
void ProcessDefinitionContext::SetProcessIPCGetStatusRoutine(int (*)(void const*, int, char const*)) { abort(); }
void ProcessDefinitionContext::SetProcessPreExecutionRoutine(unsigned int (*)(void*, void*)) { abort(); }
void ProcessDefinitionContext::SetParameterLengthQueryRoutine(unsigned long (*)(void const*, void const*, unsigned long)) { abort(); }
void ProcessDefinitionContext::SetProcessPostExecutionRoutine(void (*)(void*, void*)) { abort(); }
void ProcessDefinitionContext::SetProcessTestClonationRoutine(void* (*)(void const*)) { abort(); }
void ProcessDefinitionContext::SetParameterProcessVersionRange(unsigned int, unsigned int) { abort(); }
void ProcessDefinitionContext::SetProcessImageExecutionRoutine(unsigned int (*)(void*, void*, char const*, unsigned int)) { abort(); }
void ProcessDefinitionContext::SetProcessInitializationRoutine(unsigned int (*)(void*)) { abort(); }
void ProcessDefinitionContext::SetProcessMaskValidationRoutine(unsigned int (*)(void const*, void const*, void const*)) { abort(); }
void ProcessDefinitionContext::SetProcessEditPreferencesRoutine(unsigned int (*)(void const*)) { abort(); }
void ProcessDefinitionContext::SetProcessGlobalExecutionRoutine(unsigned int (*)(void*)) { abort(); }
void ProcessDefinitionContext::SetProcessSetServerHandleRoutine(unsigned int (*)(void*, void const*)) { abort(); }
void ProcessDefinitionContext::SetProcessIPCSetParametersRoutine(void (*)(void const*, int, char const*, unsigned short const*)) { abort(); }
void ProcessDefinitionContext::SetProcessInterfaceSelectionRoutine(void const* (*)(void const*)) { abort(); }
void ProcessDefinitionContext::SetProcessPreGlobalExecutionRoutine(unsigned int (*)(void*)) { abort(); }
void ProcessDefinitionContext::SetProcessBrowseDocumentationRoutine(unsigned int (*)(void const*)) { abort(); }
void ProcessDefinitionContext::SetProcessClassInitializationRoutine(void (*)(void const*)) { abort(); }
void ProcessDefinitionContext::SetProcessExecutionValidationRoutine(unsigned int (*)(void const*, void const*, unsigned short*, unsigned int)) { abort(); }
void ProcessDefinitionContext::SetProcessInterfaceValidationRoutine(unsigned int (*)(void const*, void const*)) { abort(); }
void ProcessDefinitionContext::SetProcessPostGlobalExecutionRoutine(void (*)(void*)) { abort(); }
void ProcessDefinitionContext::SetProcessExecutionPreferencesRoutine(unsigned int (*)(void const*)) { abort(); }
void ProcessDefinitionContext::SetProcessCommandLineProcessingRoutine(int (*)(void const*, int, unsigned short const**), unsigned int) { abort(); }
void ProcessDefinitionContext::SetProcessHistoryUpdateValidationRoutine(unsigned int (*)(void const*, void const*)) { abort(); }
void ProcessDefinitionContext::SetProcessImageExecutionValidationRoutine(unsigned int (*)(void const*, void const*, unsigned short*, unsigned int)) { abort(); }
void ProcessDefinitionContext::SetProcessDefaultInterfaceSelectionRoutine(void const* (*)(void const*)) { abort(); }
void ProcessDefinitionContext::SetProcessGlobalExecutionValidationRoutine(unsigned int (*)(void const*, unsigned short*, unsigned int)) { abort(); }
const ::api_pixtraits_lut *GlobalContext::GetPixelTraitsLUT(unsigned int)  { return new api_pixtraits_lut; }

void ViewContext::UnlockView(void*, unsigned int, unsigned int, unsigned int) { abort(); }
view_handle ViewContext::GetViewById(char const*) { abort(); }
void ViewContext::GetViewLocks(void const*, unsigned int*, unsigned int*) { abort(); }
api_bool ViewContext::GetViewFullId(void const*, char*, unsigned long*) { abort(); }
void ViewContext::LockView(void*, unsigned int, unsigned int, unsigned int) { abort(); }
api_bool ViewContext::GetViewId(void const*, char*, unsigned long*) { abort(); }
void ProcessDefinitionContext::DefineEnumerationAlias(char const*, char const*) { abort(); }
void ProcessDefinitionContext::DefineEnumerationElement(char const*, int) { abort(); }
void ProcessDefinitionContext::SetDefaultEnumerationValueIndex(unsigned int) { abort(); }

/*
void ViewContext::GetViewById() { abort(); }
void ViewContext::GetViewFullId() { abort(); }
void ViewContext::GetViewId() { abort(); }
void ViewContext::GetViewImage() { abort(); }
void ViewContext::GetViewLocks() { abort(); }
void ViewContext::LockView() { abort(); }
void ViewContext::UnlockView() { abort(); }
void ControlContext::GetControlParent() { abort(); }
void ControlContext::SetControlMaxSize() { abort(); }
void ControlContext::SetControlUpdatesEnabled() {  }
api_bool ControlContext::SetHideEventRoutine() { return api_true; }
void DialogContext::ExecuteOpenFileDialog() { abort(); }
void DialogContext::ExecuteOpenMultipleFilesDialog() { abort(); }
void FileFormatContext::CreateFileFormatInstance() { abort(); }
void FileFormatContext::DisposeFormatSpecificData() { abort(); }
void FileFormatContext::EditFileFormatPreferences() { abort(); }
void FileFormatContext::GetFileFormatByFileExtension() { abort(); }
void FileFormatContext::GetFileFormatByMimeType() { abort(); }
void FileFormatContext::GetFileFormatByName() { abort(); }
void FileFormatContext::GetFileFormatCapabilities() { abort(); }
void FileFormatContext::GetFileFormatDescription() { abort(); }
void FileFormatContext::GetFileFormatFileExtensions() { abort(); }
void FileFormatContext::GetFileFormatIcon() { abort(); }
void FileFormatContext::GetFileFormatImplementation() { abort(); }
void FileFormatContext::GetFileFormatMimeTypes() { abort(); }
void FileFormatContext::GetFileFormatName() { abort(); }
void FileFormatContext::GetFileFormatSmallIcon() { abort(); }
void FileFormatContext::GetFileFormatStatus() { abort(); }
void FileFormatContext::GetFileFormatVersion() { abort(); }
void FileFormatContext::GetImageCount() { abort(); }
void FileFormatContext::GetImageDescription() { abort(); }
void FileFormatContext::GetImageId() { abort(); }
void FileFormatContext::OpenImageFileEx() { abort(); }
void FileFormatContext::ReadImage() { abort(); }
void FileFormatContext::SelectImage() { abort(); }
void FileFormatContext::ValidateFormatSpecificData() { abort(); }
void GlobalContext::GetGlobalFlag() { abort(); }
void GlobalContext::GetGlobalString() { abort(); }
void GlobalContext::MaxProcessorsAllowedForModule() { abort(); }
void GlobalContext::ProcessEvents() { abort(); }
void ImageWindowContext::CreateImageWindow() { abort(); }
void NumericalContext::FFTComplexInverseTransformD() { abort(); }
void NumericalContext::FFTComplexTransformD() { abort(); }
void NumericalContext::FFTCreateComplexInverseTransformD() { abort(); }
void NumericalContext::FFTCreateComplexTransformD() { abort(); }
void NumericalContext::FFTCreateRealTransformD() { abort(); }
void NumericalContext::FFTDestroyTransform() { abort(); }
void NumericalContext::FFTRealOptimizedLengthF() { abort(); }
void NumericalContext::FFTRealTransformD() { abort(); }
void SharedImageContext::CreateImage() { abort(); }
*/

// ============================================================================
// ScrollBox creation
// ============================================================================

control_handle ScrollBoxContext::CreateScrollBox( api_handle module,
                                              api_handle client,
                                              control_handle parent,
                                              uint32 /*flags*/ )
{
    // Reuse the generic helper that already wires parents correctly.
    control_handle h = reinterpret_cast<control_handle>(
        createControl<QScrollArea>( module, client, parent ) );

    MockBase* b = get( h );
    if ( !b || !b->widget )
        return nullptr;

    auto* scrollArea = qobject_cast<QScrollArea*>( b->widget );
    if ( scrollArea )
    {
        // For the mock we keep it simple and resizable,  
        // PCL’s flags are ignored here.
        scrollArea->setWidgetResizable( true );
    }

    logf( "[Mock] CreateScrollBox handle=%p widget=%p parent=%p",
          h, b->widget, parent );

    return h;
}

control_handle ScrollBoxContext::CreateScrollBoxViewport( control_handle scrollBox,
                                                      api_handle client )
{
    MockBase* sb = get( scrollBox );
    if ( !sb || !sb->widget )
        return nullptr;

    auto* scrollArea = qobject_cast<QScrollArea*>( sb->widget );
    if ( !scrollArea )
        return nullptr;

    // Create a simple QWidget as the viewport contents
    auto* b = new MockBase();
    b->moduleHandle = sb->moduleHandle;
    b->isSizer      = false;
    b->layout       = nullptr;
    b->widget       = new QWidget( scrollArea );

    // Register it in the global object map
    control_handle viewportHandle = reinterpret_cast<control_handle>( b );
    g_objects[ viewportHandle ] = std::unique_ptr<MockBase>( b );

    // Attach to the scroll area
    scrollArea->setWidget( b->widget );

    logf( "[Mock] CreateScrollBoxViewport handle=%p parentScroll=%p widget=%p",
          viewportHandle, scrollBox, b->widget );

    return viewportHandle;
}

// ============================================================================
// TreeBox Node State Functions
// ============================================================================

api_bool TreeBoxContext::GetTreeBoxNodeEnabled(const_api_handle node)
{
    if (!node) return api_false;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return api_false;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    return !(item->flags() & Qt::ItemIsEnabled) ? api_false : api_true;
}

void TreeBoxContext::SetTreeBoxNodeEnabled(api_handle node, api_bool enabled)
{
    if (!node) return;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
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
    if (!node) return api_false;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return api_false;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    return item->isExpanded() ? api_true : api_false;
}

void TreeBoxContext::SetTreeBoxNodeExpanded(api_handle node, api_bool expanded)
{
    if (!node) return;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    item->setExpanded(expanded);
    logf("[Mock] SetTreeBoxNodeExpanded: %d", expanded);
}

api_bool TreeBoxContext::GetTreeBoxNodeSelectable(const_api_handle node)
{
    if (!node) return api_false;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return api_false;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    return (item->flags() & Qt::ItemIsSelectable) ? api_true : api_false;
}

void TreeBoxContext::SetTreeBoxNodeSelectable(api_handle node, api_bool selectable)
{
    if (!node) return;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
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
    if (!node) return api_false;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return api_false;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    return item->isSelected() ? api_true : api_false;
}

void TreeBoxContext::SetTreeBoxNodeSelected(api_handle node, api_bool selected)
{
    if (!node) return;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    item->setSelected(selected);
    logf("[Mock] SetTreeBoxNodeSelected: %d", selected);
}

api_bool TreeBoxContext::GetTreeBoxNodeCheckable(const_api_handle node)
{
    if (!node) return api_false;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return api_false;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    return (item->flags() & Qt::ItemIsUserCheckable) ? api_true : api_false;
}

void TreeBoxContext::SetTreeBoxNodeCheckable(api_handle node, api_bool checkable)
{
    if (!node) return;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
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
    if (!node) return api_false;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return api_false;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    return (item->checkState(0) == Qt::Checked) ? api_true : api_false;
}

void TreeBoxContext::SetTreeBoxNodeChecked(api_handle node, api_bool checked)
{
    if (!node) return;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    item->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
    logf("[Mock] SetTreeBoxNodeChecked: %d", checked);
}

api_bool TreeBoxContext::GetTreeBoxNodeEditable(const_api_handle node)
{
    if (!node) return api_false;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return api_false;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    return (item->flags() & Qt::ItemIsEditable) ? api_true : api_false;
}

void TreeBoxContext::SetTreeBoxNodeEditable(api_handle node, api_bool editable)
{
    if (!node) return;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
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
    if (!node) return api_false;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return api_false;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    if (QTreeWidget* tree = item->treeWidget()) {
        return tree->isFirstItemColumnSpanned(item) ? api_true : api_false;
    }
    
    return api_false;
}

void TreeBoxContext::SetTreeBoxNodeFirstColumnSpanned(api_handle node, api_bool spanned)
{
    if (!node) return;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    if (QTreeWidget* tree = item->treeWidget()) {
        tree->setFirstItemColumnSpanned(item, spanned);
        logf("[Mock] SetTreeBoxNodeFirstColumnSpanned: %d", spanned);
    }
}

// ============================================================================
// TreeBox Node Column Content Functions
// ============================================================================

api_bool TreeBoxContext::GetTreeBoxNodeColText(const_api_handle node,
                                           int32 col,
                                           char16_type* text,
                                           size_type* len)
{
    if (!node) return api_false;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return api_false;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
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
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    QString qtext = QString::fromUtf16(text);
    item->setText(col, qtext);
    logf("[Mock] SetTreeBoxNodeColText col=%d: %s", col, qtext.toUtf8().constData());
}

bitmap_handle TreeBoxContext::GetTreeBoxNodeColIcon(const_api_handle node, int32 col)
{
    if (!node) return nullptr;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return nullptr;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    QIcon icon = item->icon(col);
    
    if (!icon.isNull()) {
        QPixmap pixmap = icon.pixmap(32, 32); // Default size
        return reinterpret_cast<bitmap_handle>(new QPixmap(pixmap));
    }
    
    return nullptr;
}

void TreeBoxContext::SetTreeBoxNodeColIcon(api_handle node,
                                       int32 col,
                                       const_bitmap_handle icon)
{
    if (!node) return;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    
    if (icon) {
        const QPixmap* pixmap = reinterpret_cast<const QPixmap*>((icon));
        item->setIcon(col, QIcon(*pixmap));
    } else {
        item->setIcon(col, QIcon());
    }
    
    logf("[Mock] SetTreeBoxNodeColIcon col=%d", col);
}

int32 TreeBoxContext::GetTreeBoxNodeColAlignment(const_api_handle node, int32 col)
{
    if (!node) return 0;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return 0;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    return static_cast<int32>(item->textAlignment(col));
}

void TreeBoxContext::SetTreeBoxNodeColAlignment(api_handle node,
                                            int32 col,
                                            int32 alignment)
{
    if (!node) return;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    item->setTextAlignment(col, alignment);
    logf("[Mock] SetTreeBoxNodeColAlignment col=%d align=%d", col, alignment);
}

api_bool TreeBoxContext::GetTreeBoxNodeColToolTip(const_api_handle node,
                                              int32 col,
                                              char16_type* text,
                                              size_type* len)
{
    if (!node) return api_false;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return api_false;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
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
    if (!node) return;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    QString qtext = text ? QString::fromUtf16(text) : QString();
    item->setToolTip(col, qtext);
    logf("[Mock] SetTreeBoxNodeColToolTip col=%d", col);
}

font_handle TreeBoxContext::GetTreeBoxNodeColFont(const_api_handle node, int32 col)
{
    if (!node) return nullptr;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return nullptr;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    QFont font = item->font(col);
    return reinterpret_cast<font_handle>(new QFont(font));
}

void TreeBoxContext::SetTreeBoxNodeColFont(api_handle node,
                                       int32 col,
                                       const_font_handle font)
{
    if (!node) return;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    
    if (font) {
        const QFont* qfont = reinterpret_cast<const QFont*>((font));
        item->setFont(col, *qfont);
    }
    
    logf("[Mock] SetTreeBoxNodeColFont col=%d", col);
}

uint32 TreeBoxContext::GetTreeBoxNodeColBackgroundColor(const_api_handle node, int32 col)
{
    if (!node) return 0;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return 0;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    QBrush brush = item->background(col);
    return brush.color().rgba();
}

void TreeBoxContext::SetTreeBoxNodeColBackgroundColor(api_handle node,
                                                  int32 col,
                                                  uint32 rgba)
{
    if (!node) return;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    item->setBackground(col, QBrush(QColor(rgba)));
    logf("[Mock] SetTreeBoxNodeColBackgroundColor col=%d rgba=0x%08x", col, rgba);
}

uint32 TreeBoxContext::GetTreeBoxNodeColTextColor(const_api_handle node, int32 col)
{
    if (!node) return 0;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return 0;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    QBrush brush = item->foreground(col);
    return brush.color().rgba();
}

void TreeBoxContext::SetTreeBoxNodeColTextColor(api_handle node,
                                            int32 col,
                                            uint32 rgba)
{
    if (!node) return;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    item->setForeground(col, QBrush(QColor(rgba)));
    logf("[Mock] SetTreeBoxNodeColTextColor col=%d rgba=0x%08x", col, rgba);
}

api_bool TreeBoxContext::SetTreeBoxNodeActivatedEventRoutine(
    control_handle h,
    api_handle /*client*/,
    pcl::item_value_event_routine r)
{
    if (auto* b = get(h))
    {
        b->onTreeNodeActivated = r;

        if (auto* t = treeFromHandle(h))
        {
            QObject::connect(t, &QTreeWidget::itemActivated,
                             [b](QTreeWidgetItem* item, int){
                if (b->onTreeNodeActivated)
                {
                    b->onTreeNodeActivated(
                        reinterpret_cast<control_handle>(b),
                        reinterpret_cast<control_handle>(b),
                        reinterpret_cast<api_handle>(item),
					   0);
                }
            });
        }
    }
    return api_true;
}

api_bool TreeBoxContext::SetTreeBoxCurrentNodeUpdatedEventRoutine(
    control_handle h,
    api_handle /*client*/,
    pcl::item_range_event_routine r)
{
    if (auto* b = get(h))
    {
        b->onTreeNodeUpdated = r;

        if (auto* t = treeFromHandle(h))
        {
            QObject::connect(t, &QTreeWidget::currentItemChanged,
                             [b](QTreeWidgetItem* item, QTreeWidgetItem*){
                if (b->onTreeNodeUpdated)
                {
                    b->onTreeNodeUpdated(
                        reinterpret_cast<control_handle>(b),
                        reinterpret_cast<control_handle>(b),
                        reinterpret_cast<control_handle>(b),
                        reinterpret_cast<api_handle>(item));
                }
            });
        }
    }
    return api_true;
}

api_bool TreeBoxContext::SetTreeBoxNodeSelectionUpdatedEventRoutine(
    control_handle h,
    api_handle /*client*/,
    pcl::event_routine r)
{
    if (auto* b = get(h))
    {
        b->onTreeSelectionUpdated = r;

        if (auto* t = treeFromHandle(h))
        {
            QObject::connect(t, &QTreeWidget::itemSelectionChanged,
                             [b,t](){
                if (b->onTreeSelectionUpdated)
                {
                    auto items = t->selectedItems();
                    QTreeWidgetItem* first =
                        items.isEmpty() ? nullptr : items.first();

                    b->onTreeSelectionUpdated(
                        reinterpret_cast<control_handle>(b),
                        reinterpret_cast<control_handle>(first));
                }
            });
        }
    }
    return api_true;
}

// ============================================================================
// TreeBox Event Handlers (Stubs)
// ============================================================================

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

// updates
// ============================================================================
// TreeBox Missing Stub Implementations - Replace abort() calls
// ============================================================================

control_handle TreeBoxContext::CreateTreeBox(api_handle module,
                                         api_handle client,
                                         control_handle parent,
                                         uint32 flags)
{
    auto* treeHandle = createControl<QTreeWidget>(module, client, parent);
    
    auto* C = get(treeHandle);
    if (!C || !C->widget) return nullptr;
    
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(C->widget);
    if (tree) {
        tree->setColumnCount(1); // Default to 1 column
        tree->setHeaderHidden(false);
    }
    
    logf("[Mock] CreateTreeBox handle=%p", treeHandle);
    return reinterpret_cast<control_handle>(treeHandle);
}

control_handle TreeBoxContext::CreateTreeBoxViewport(control_handle h,
                                                 api_handle client)
{
    auto* C = get(h);
    if (!C || !C->widget) return nullptr;
    
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(C->widget);
    if (!tree) return nullptr;
    
    QWidget* viewport = tree->viewport();
    
    logf("[Mock] CreateTreeBoxViewport");
    return reinterpret_cast<control_handle>(viewport);
}

api_handle TreeBoxContext::CreateTreeBoxNode(api_handle module,
                                         api_handle nodeClient)
{
    auto* b = new MockBase();
    b->moduleHandle = module;
    
    // Create QTreeWidgetItem and store as widget (even though it's not a QWidget)
    QTreeWidgetItem* item = new QTreeWidgetItem();
    b->widget = reinterpret_cast<QWidget*>(item);
    
    // Set default flags
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    
    void* h = reinterpret_cast<void*>(b);
    g_objects[h] = std::unique_ptr<MockBase>(b);
    
    logf("[Mock] CreateTreeBoxNode handle=%p client=%p", h, nodeClient);
    return h;
}

void TreeBoxContext::ClearTreeBox(control_handle h)
{
    auto* C = get(h);
    if (!C || !C->widget) return;
    
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(C->widget);
    if (!tree) return;
    
    tree->clear();
    
    logf("[Mock] ClearTreeBox");
}

int32 TreeBoxContext::GetTreeBoxChildCount(const_control_handle h)
{
    auto* C = get((h));
    if (!C || !C->widget) return 0;
    
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(C->widget);
    if (!tree) return 0;
    
    return tree->topLevelItemCount();
}

api_handle TreeBoxContext::GetTreeBoxChild(const_control_handle h, int32 idx)
{
    auto* C = get((h));
    if (!C || !C->widget) return nullptr;
    
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(C->widget);
    if (!tree) return nullptr;
    
    QTreeWidgetItem* item = tree->topLevelItem(idx);
    if (!item) return nullptr;
    
    // Find the handle for this item in g_objects
    for (auto& kv : g_objects) {
        MockBase* obj = kv.second.get();
        if (reinterpret_cast<QTreeWidgetItem*>(obj->widget) == item) {
	  return (api_handle)(kv.first);
        }
    }
    
    return nullptr;
}

int32 TreeBoxContext::GetTreeBoxChildIndex(const_control_handle h,
                                       const_api_handle node)
{
    auto* C = get((h));
    if (!C || !C->widget || !node) return -1;
    
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(C->widget);
    if (!tree) return -1;
    
    auto it = g_objects.find((node));
    if (it == g_objects.end()) return -1;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    return tree->indexOfTopLevelItem(item);
}

void TreeBoxContext::InsertTreeBoxNode(control_handle h,
                                   int32 idx,
                                   api_handle node)
{
    auto* C = get(h);
    if (!C || !C->widget || !node) return;
    
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(C->widget);
    if (!tree) return;
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    
    if (idx < 0 || idx >= tree->topLevelItemCount()) {
        tree->addTopLevelItem(item);
    } else {
        tree->insertTopLevelItem(idx, item);
    }
    
    logf("[Mock] InsertTreeBoxNode idx=%d", idx);
}

void TreeBoxContext::RemoveTreeBoxNode(control_handle h, int32 idx)
{
    auto* C = get(h);
    if (!C || !C->widget) return;
    
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(C->widget);
    if (!tree) return;
    
    QTreeWidgetItem* item = tree->takeTopLevelItem(idx);
    if (item) {
        logf("[Mock] RemoveTreeBoxNode idx=%d", idx);
    }
}

api_handle TreeBoxContext::GetTreeBoxCurrentNode(const_control_handle h)
{
    auto* C = get((h));
    if (!C || !C->widget) return nullptr;
    
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(C->widget);
    if (!tree) return nullptr;
    
    QTreeWidgetItem* item = tree->currentItem();
    if (!item) return nullptr;
    
    // Find the handle for this item
    for (auto& kv : g_objects) {
        MockBase* obj = kv.second.get();
        if (reinterpret_cast<QTreeWidgetItem*>(obj->widget) == item) {
	  return (api_handle)(kv.first);
        }
    }
    
    return nullptr;
}

void TreeBoxContext::SetTreeBoxCurrentNode(control_handle h, api_handle node)
{
    auto* C = get(h);
    if (!C || !C->widget) return;
    
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(C->widget);
    if (!tree) return;
    
    if (!node) {
        tree->setCurrentItem(nullptr);
        return;
    }
    
    auto it = g_objects.find(node);
    if (it == g_objects.end()) return;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    tree->setCurrentItem(item);
    
    logf("[Mock] SetTreeBoxCurrentNode");
}

void TreeBoxContext::SelectAllTreeBoxNodes(control_handle h)
{
    auto* C = get(h);
    if (!C || !C->widget) return;
    
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(C->widget);
    if (!tree) return;
    
    tree->selectAll();
    
    logf("[Mock] SelectAllTreeBoxNodes");
}

void TreeBoxContext::SetTreeBoxMultipleNodeSelectionEnabled(control_handle h,
                                                        api_bool enabled)
{
    auto* C = get(h);
    if (!C || !C->widget) return;
    
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(C->widget);
    if (!tree) return;
    
    tree->setSelectionMode(enabled ?
        QAbstractItemView::ExtendedSelection :
        QAbstractItemView::SingleSelection);
    
    logf("[Mock] SetTreeBoxMultipleNodeSelectionEnabled: %d", enabled);
}

void TreeBoxContext::SetTreeBoxColumnCount(control_handle h, int32 count)
{
    auto* C = get(h);
    if (!C || !C->widget) return;
    
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(C->widget);
    if (!tree) return;
    
    tree->setColumnCount(count);
    
    logf("[Mock] SetTreeBoxColumnCount: %d", count);
}

void TreeBoxContext::AdjustTreeBoxColumnWidthToContents(control_handle h, int32 col)
{
    auto* C = get(h);
    if (!C || !C->widget) return;
    
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(C->widget);
    if (!tree) return;
    
    tree->resizeColumnToContents(col);
    
    logf("[Mock] AdjustTreeBoxColumnWidthToContents col=%d", col);
}

void TreeBoxContext::SetTreeBoxHeaderVisible(control_handle h, api_bool visible)
{
    auto* C = get(h);
    if (!C || !C->widget) return;
    
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(C->widget);
    if (!tree) return;
    
    tree->setHeaderHidden(!visible);
    
    logf("[Mock] SetTreeBoxHeaderVisible: %d", visible);
}

void TreeBoxContext::SetTreeBoxRootDecorationEnabled(control_handle h,
                                                 api_bool enabled)
{
    auto* C = get(h);
    if (!C || !C->widget) return;
    
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(C->widget);
    if (!tree) return;
    
    tree->setRootIsDecorated(enabled);
    
    logf("[Mock] SetTreeBoxRootDecorationEnabled: %d", enabled);
}

void TreeBoxContext::SetTreeBoxAlternateRowColorEnabled(control_handle h,
                                                    api_bool enabled)
{
    auto* C = get(h);
    if (!C || !C->widget) return;
    
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(C->widget);
    if (!tree) return;
    
    tree->setAlternatingRowColors(enabled);
    
    logf("[Mock] SetTreeBoxAlternateRowColorEnabled: %d", enabled);
}

control_handle TreeBoxContext::GetTreeBoxNodeParentBox(const_api_handle node)
{
    if (!node) return nullptr;
    
    auto it = g_objects.find((node));
    if (it == g_objects.end()) return nullptr;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    QTreeWidget* tree = item->treeWidget();
    
    if (!tree) return nullptr;
    
    // Find the control handle for this tree widget
    for (auto& kv : g_objects) {
        MockBase* obj = kv.second.get();
        if (obj->widget == tree) {
	  return (control_handle)(kv.first);
        }
    }
    
    return nullptr;
}

api_handle TreeBoxContext::GetTreeBoxNodeParent(const_api_handle node)
{
    if (!node) return nullptr;
    
    auto it = g_objects.find((node));
    if (it == g_objects.end()) return nullptr;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    QTreeWidgetItem* parent = item->parent();
    
    if (!parent) return nullptr;
    
    // Find the handle for the parent item
    for (auto& kv : g_objects) {
        MockBase* obj = kv.second.get();
        if (reinterpret_cast<QTreeWidgetItem*>(obj->widget) == parent) {
	  return (api_handle)kv.first;
        }
    }
    
    return nullptr;
}

int32 TreeBoxContext::GetTreeBoxNodeChildCount(const_api_handle node)
{
    if (!node) return 0;
    
    auto it = g_objects.find((node));
    if (it == g_objects.end()) return 0;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    return item->childCount();
}

api_handle TreeBoxContext::GetTreeBoxNodeChild(const_api_handle node, int32 idx)
{
    if (!node) return nullptr;
    
    auto it = g_objects.find((node));
    if (it == g_objects.end()) return nullptr;
    
    QTreeWidgetItem* item = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    QTreeWidgetItem* child = item->child(idx);
    
    if (!child) return nullptr;
    
    // Find the handle for the child item
    for (auto& kv : g_objects) {
        MockBase* obj = kv.second.get();
        if (reinterpret_cast<QTreeWidgetItem*>(obj->widget) == child) {
	  return (api_handle)(kv.first);
        }
    }
    
    return nullptr;
}

void TreeBoxContext::InsertTreeBoxNodeChild(api_handle parentNode,
                                        int32 idx,
                                        api_handle childNode)
{
    if (!parentNode || !childNode) return;
    
    auto parentIt = g_objects.find(parentNode);
    if (parentIt == g_objects.end()) return;
    
    auto childIt = g_objects.find(childNode);
    if (childIt == g_objects.end()) return;
    
    QTreeWidgetItem* parent = reinterpret_cast<QTreeWidgetItem*>(parentIt->second.get()->widget);
    QTreeWidgetItem* child = reinterpret_cast<QTreeWidgetItem*>(childIt->second.get()->widget);
    
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
    
    auto it = g_objects.find(parentNode);
    if (it == g_objects.end()) return;
    
    QTreeWidgetItem* parent = reinterpret_cast<QTreeWidgetItem*>(it->second.get()->widget);
    QTreeWidgetItem* child = parent->takeChild(idx);
    
    if (child) {
        logf("[Mock] RemoveTreeBoxNodeChild idx=%d", idx);
    }
}
