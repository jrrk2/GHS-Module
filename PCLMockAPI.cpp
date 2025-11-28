// PCLMockAPI.cpp
//
// Mock implementation of the PCL C API used by PixInsight modules.
//
// Design:
//   - Every handle value is a pointer to a mock C++ object that *we* own.
//   - All Qt widgets live inside those mocks.
//   - No API function ever treats a handle as a raw QWidget*.
//   - All widget access goes through helper lookup functions.
//
// This file is focused on what the GradientHDRComposition module
// actually exercises (based on your logs). You can extend it with more
// APIs as needed.

#include "PCLMockAPI.h"

#include <pcl/String.h>
#include <pcl/Console.h>

#include <QApplication>
#include <QWidget>
#include <QRadioButton>
#include <QSpinBox>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QToolButton>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QSlider>
#include <QScrollArea>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QBoxLayout>
#include <QFont>
#include <QFontMetrics>
#include <QPixmap>
#include <QIcon>
#include <QTimer>
#include <QDebug>

#include <map>
#include <unordered_map>
#include <mutex>

#define button_handle control_handle
#define edit_handle control_handle
#define label_handle control_handle
#define slider_handle control_handle
#define treebox_handle control_handle
#define combo_handle control_handle
#define spin_handle control_handle

typedef void *ui_handle;

//---------------------------------------------------------------------
// Logging helpers
//---------------------------------------------------------------------

static bool g_enableDebugLogging = true;

void SetDebugLogging( bool on )
{
   g_enableDebugLogging = on;
}

static void LogDebug( const QString& msg )
{
   if ( g_enableDebugLogging )
      qDebug().noquote() << msg;
}

static void LogInfo( const QString& msg )
{
   qDebug().noquote() << msg;
}

static void LogWarning( const QString& msg )
{
   qWarning().noquote() << msg;
}

static QString PtrToHex( const void* p )
{
   return QString( "0x%1" ).arg( reinterpret_cast<quintptr>( p ), 0, 16 );
}

//---------------------------------------------------------------------
// Base mock object
//---------------------------------------------------------------------

//---------------------------------------------------------------------
// Mock widgets
//---------------------------------------------------------------------
// revised createXXX

struct MockBase
{
   api_handle moduleHandle = nullptr;
   api_handle clientHandle = nullptr;
   virtual ~MockBase() = default;
};

struct MockControl : public MockBase
{
   QWidget* widget = nullptr;
   QString  uiObjectId;
   uint32   flags = 0;
};

struct MockLabel : public MockBase
{
   QLabel* widget = nullptr;
};

struct MockEdit : public MockBase
{
   QLineEdit* widget = nullptr;
};

struct MockSlider : public MockBase
{
   QSlider* widget = nullptr;
};

struct MockCheckBox : public MockBase
{
   QCheckBox* widget = nullptr;
};

struct MockCombo : public MockBase
{
   QComboBox* widget = nullptr;
};

struct MockScrollBox : public MockBase
{
   QScrollArea* scroll   = nullptr;
   QWidget*     viewport = nullptr;
   void*        scrollArea = nullptr;
};

struct MockSpin : public MockBase
{
   QSpinBox* widget = nullptr;
};

// NOTE: layout is QBoxLayout*, not QLayout*
struct MockSizer : public MockBase
{
   bool                   vertical = true;
   QBoxLayout*            layout   = nullptr;
   std::vector<api_handle> items;
};

// Represent any “button-like” thing as a QAbstractButton
struct MockButton : public MockBase
{
   QAbstractButton* button = nullptr;
   bool             isCheckBox = false;
};

struct MockRadio : public MockBase
{
   QRadioButton* button = nullptr;
   bool          checkable    = false;
   bool          isToolButton = false;
};

struct MockTreeBox : public MockBase
{
   QTreeWidget* tree = nullptr;
};

struct MockBitmap : public MockBase
{
   QPixmap pixmap;
};

struct MockFont : public MockBase
{
   QFont font;
};

//---------------------------------------------------------------------
// Global maps: handle -> mock objects
//---------------------------------------------------------------------

// Simple pointer-based hash/equality for handle types (control_handle etc.)
template <typename H>
struct HandleHash
{
   std::size_t operator()( H h ) const noexcept
   {
      auto p = reinterpret_cast<std::uintptr_t>( h );
      return std::hash<std::uintptr_t>{}( p );
   }
};

template <typename H>
struct HandleEqual
{
   bool operator()( H a, H b ) const noexcept
   {
      return a == b;
   }
};

// --- GLOBAL REGISTRY OF ALL MOCK OBJECT TYPES ------------------------------

static std::unordered_map<control_handle,
                          std::unique_ptr<MockControl>,
                          HandleHash<control_handle>,
                          HandleEqual<control_handle>> g_controls;

static std::unordered_map<label_handle,
                          std::unique_ptr<MockLabel>,
                          HandleHash<label_handle>,
                          HandleEqual<label_handle>>   g_labels;

static std::unordered_map<edit_handle,
                          std::unique_ptr<MockEdit>,
                          HandleHash<edit_handle>,
                          HandleEqual<edit_handle>>    g_edits;

static std::unordered_map<button_handle,
                          std::unique_ptr<MockButton>,
                          HandleHash<button_handle>,
                          HandleEqual<button_handle>>  g_buttons;

static std::unordered_map<slider_handle,
                          std::unique_ptr<MockSlider>,
                          HandleHash<slider_handle>,
                          HandleEqual<slider_handle>>  g_sliders;

static std::unordered_map<combo_handle,
                          std::unique_ptr<MockCombo>,
                          HandleHash<combo_handle>,
                          HandleEqual<combo_handle>>   g_combos;

static std::unordered_map<spin_handle,
                          std::unique_ptr<MockSpin>,
                          HandleHash<spin_handle>,
                          HandleEqual<spin_handle>>    g_spins;

static std::unordered_map<sizer_handle,
                          std::unique_ptr<MockSizer>,
                          HandleHash<sizer_handle>,
                          HandleEqual<sizer_handle>>   g_sizers;

static std::unordered_map<void*,
                          QWidget*,
                          HandleHash<void*>,
                          HandleEqual<void*>> g_widgets;

static std::unordered_map<bitmap_handle,
                          std::unique_ptr<MockBitmap>,
                          HandleHash<bitmap_handle>,
                          HandleEqual<bitmap_handle>> g_bitmaps;

static std::unordered_map<font_handle,
                          std::unique_ptr<MockFont>,
                          HandleHash<font_handle>,
                          HandleEqual<font_handle>>   g_fonts;


static std::unordered_map<treebox_handle,
                          std::unique_ptr<MockTreeBox>,
                          HandleHash<treebox_handle>,
                          HandleEqual<treebox_handle>>   g_treeboxs;

// Remember last exposed top-level widget
static QWidget* g_lastTopLevelWidget = nullptr;

static QWidget* lookupWidget( ui_handle h )
{
   if ( !h )
      return nullptr;

   if ( auto it = g_controls.find( h ); it != g_controls.end() )
      return it->second->widget;

   if ( auto it = g_labels.find( h ); it != g_labels.end() )
      return it->second->widget;

   if ( auto it = g_edits.find( h ); it != g_edits.end() )
      return it->second->widget;

   if ( auto it = g_buttons.find( h ); it != g_buttons.end() )
      return it->second->button;

   if ( auto it = g_sliders.find( h ); it != g_sliders.end() )
      return it->second->widget;

   if ( auto it = g_combos.find( h ); it != g_combos.end() )
      return it->second->widget;

   if ( auto it = g_spins.find( h ); it != g_spins.end() )
      return it->second->widget;

   if ( auto it = g_treeboxs.find( h ); it != g_treeboxs.end() )
      return it->second->tree;

   // fall back to direct registry if present
   if ( auto it = g_widgets.find( h ); it != g_widgets.end() )
      return it->second;

   return nullptr;
}

extern "C" {

 control_handle API_Control_CreateControl(api_handle h)
{
    auto* c = new MockControl();
    c->widget = new QWidget();

    control_handle handle = (control_handle)c;

    g_controls[handle] = std::unique_ptr<MockControl>(c);

    g_lastTopLevelWidget = c->widget;

    fprintf(stderr, "[PCLMockAPI] API_Control_CreateControl: handle=%p widget=%p\n",
            handle, c->widget);

    return handle;
}

 label_handle API_Label_CreateLabel(api_handle h, control_handle parent)
{
    auto* l = new MockLabel();
    QWidget* parentWidget = lookupWidget(parent);

    l->widget = new QLabel(parentWidget);

    label_handle handle = (label_handle)l;

    g_labels[handle] = std::unique_ptr<MockLabel>(l);

    fprintf(stderr, "[PCLMockAPI] API_Label_CreateLabel: handle=%p widget=%p parent=%p\n",
            handle, l->widget, parentWidget);

    return handle;
}

 edit_handle API_Edit_CreateEdit(api_handle h, control_handle parent)
{
    auto* e = new MockEdit();
    QWidget* p = lookupWidget(parent);
    e->widget = new QLineEdit(p);

    edit_handle handle = (edit_handle)e;

    g_edits[handle] = std::unique_ptr<MockEdit>(e);

    fprintf(stderr, "[PCLMockAPI] API_Edit_CreateEdit: handle=%p widget=%p parent=%p\n",
            handle, e->widget, p);

    return handle;
}

 slider_handle API_Slider_CreateSlider(api_handle h, control_handle parent)
{
    auto* s = new MockSlider();
    QWidget* p = lookupWidget(parent);
    s->widget = new QSlider(Qt::Horizontal, p);

    slider_handle handle = (slider_handle)s;
    g_sliders[handle] = std::unique_ptr<MockSlider>(s);

    fprintf(stderr, "[PCLMockAPI] API_Slider_CreateSlider: handle=%p widget=%p parent=%p\n",
            handle, s->widget, p);

    return handle;
}

button_handle API_Button_CreateCheckBox( api_handle h, control_handle parent )
{
   Q_UNUSED( h );

   auto* btn = new MockButton();
   QWidget* p = lookupWidget( parent );

   // Represent the checkbox as a QAbstractButton so we can reuse button logic.
   btn->button = new QCheckBox( p );
   btn->button->setCheckable( true );

   button_handle handle = reinterpret_cast<button_handle>( btn );
   g_buttons[ handle ] = std::unique_ptr<MockButton>( btn );

   fprintf( stderr,
            "[PCLMockAPI] API_Button_CreateCheckBox: handle=%p widget=%p\n",
            handle, btn->button );

   return handle;
}

 combo_handle API_ComboBox_CreateComboBox(api_handle h, control_handle parent)
{
    auto* c = new MockCombo();
    QWidget* p = lookupWidget(parent);
    c->widget = new QComboBox(p);

    combo_handle handle = (combo_handle)c;
    g_combos[handle] = std::unique_ptr<MockCombo>(c);

    fprintf(stderr, "[PCLMockAPI] API_ComboBox_CreateComboBox: handle=%p widget=%p\n",
            handle, c->widget);

    return handle;
}

 spin_handle API_SpinBox_CreateSpinBox(api_handle h, control_handle parent)
{
    auto* s = new MockSpin();
    QWidget* p = lookupWidget(parent);
    s->widget = new QSpinBox(p);

    spin_handle handle = (spin_handle)s;
    g_spins[handle] = std::unique_ptr<MockSpin>(s);

    fprintf(stderr, "[PCLMockAPI] API_SpinBox_CreateSpinBox: handle=%p widget=%p\n",
            handle, s->widget);

    return handle;
}
 int API_UI_GetUIObjectRefCount(api_handle h)
{
    int n = g_controls.size() + g_labels.size() + g_edits.size() +
            g_buttons.size() + g_sliders.size() + g_combos.size() +
            g_spins.size() + g_sizers.size();

    fprintf(stderr, "[PCLMockAPI] API_UI_GetUIObjectRefCount = %d\n", n);
    return n;
}

};

// Last created top-level control for null-handle Show()
static MockControl* g_lastTopLevelControl = nullptr;

// Simple error code storage
static int g_last_error = 0;

// Runtime value of PCL's "null control"
static api_handle g_pcl_null_control = nullptr;

// Called from main() before any GUI creation
void PCLMockAPI_SetNullControlHandle( void )
{
  g_pcl_null_control = reinterpret_cast<api_handle>( pcl::Control::Null().handle );
}

static inline bool IsNullOrInvalidControl(api_handle h)
{
    if (h == nullptr)
        return true;

    if (!g_pcl_null_control)  PCLMockAPI_SetNullControlHandle();
    
    if (h == g_pcl_null_control)
        return true;

    // If the handle does not exist in our map, treat it as null
    if (g_controls.find(h) == g_controls.end())
        return true;

    return false;
}

// Return the QWidget associated with a control_handle (MockControl*)
// or nullptr if invalid.
static QWidget* GetWidgetForControl( control_handle h )
{
    auto it = g_controls.find( h );
    if ( it == g_controls.end() )
        return nullptr;

    MockControl *mc = it->second.get();
    if ( mc == nullptr )
        return nullptr;

    return mc->widget; // this is the real QWidget*
}

//---------------------------------------------------------------------
// Generic helpers
//---------------------------------------------------------------------

template <class T, class Handle, class Map>
static T* Lookup( Map& m, Handle h, const char* context )
{
   if ( h == nullptr )
   {
      LogWarning( QString( "[PCLMockAPI] %1: null handle" ).arg( context ) );
      return nullptr;
   }

   auto it = m.find( h );
   if ( it == m.end() )
   {
      LogWarning( QString( "[PCLMockAPI] %1: unknown handle=%2" )
                  .arg( context )
                  .arg( PtrToHex( h ) ) );
      return nullptr;
   }
   return it->second.get();
}

static void RegisterWidget( ui_handle handle, QWidget* w, const char* /*context*/ )
{
   if ( handle && w )
      g_widgets[ handle ] = w;
}

static void UnregisterWidget( ui_handle handle )
{
   g_widgets.erase( handle );
}

// Existing helper – you should already have something like this:
static MockControl* GetControlFromHandle( control_handle handle, const char* where )
{
    if ( handle == nullptr )
    {
      LogWarning( QString("%1: null control handle").arg(where) );
        return nullptr;
    }

    auto it = g_controls.find( handle );
    if ( it == g_controls.end() )
    {
      LogWarning( QString("%1: unknown control handle %2").arg(where).arg(PtrToHex( handle )) );
        return nullptr;
    }
    return it->second.get();
}

// New helper just for combo boxes:
static QComboBox* GetComboBoxFromHandle( combo_handle handle, const char* where )
{
    MockControl* ctrl = GetControlFromHandle( handle, where );
    if ( !ctrl )
        return nullptr;

    QComboBox* combo = qobject_cast<QComboBox*>( ctrl->widget );
    if ( !combo )
    {
      LogWarning( QString("%1: handle %2 is not a QComboBox").arg(where).arg(PtrToHex( handle )) );
    }
    return combo;
}

// Convert UTF-16 string to UTF-8
std::string Utf16ToUtf8(const char16_type* utf16Str) {
    if (!utf16Str) return "";
    
    // Simple direct conversion
    std::string result;
    const char16_t* src = reinterpret_cast<const char16_t*>(utf16Str);
    while (*src) {
        char16_t ch = *src++;
        if (ch < 128) {
            result += static_cast<char>(ch);
        } else if (ch < 0x800) {
            result += static_cast<char>(0xC0 | (ch >> 6));
            result += static_cast<char>(0x80 | (ch & 0x3F));
        } else {
            result += static_cast<char>(0xE0 | (ch >> 12));
            result += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (ch & 0x3F));
        }
    }
    return result;
}

//---------------------------------------------------------------------
// Control API
//---------------------------------------------------------------------

extern "C" {

void API_Control_DestroyControl( control_handle handle, api_handle client )
{
   Q_UNUSED( client );

   LogDebug( QString( "[PCLMockAPI] DestroyControl called, handle=%1" )
             .arg( PtrToHex( handle ) ) );

   MockControl* ctrl = nullptr;
   {
      auto it = g_controls.find( handle );
      if ( it != g_controls.end() )
      {
	ctrl = it->second.get();
         g_controls.erase( it );
      }
   }

   if ( ctrl )
   {
      QWidget* w = ctrl->widget;
      if ( w )
         w->deleteLater();
      delete ctrl;
   }

   {
      g_widgets.erase( handle );
   }
}

api_bool API_Control_SetControlVisible( control_handle handle,
                                        api_handle client,
                                        uint32 flags )
{
   Q_UNUSED( client );

   LogDebug( QString( "[PCLMockAPI] SetControlVisible called, handle=%1 flags=%2" )
             .arg( PtrToHex( handle ) )
             .arg( flags ) );

   MockControl* ctrl = nullptr;

   if ( handle != nullptr )
   {
      ctrl = Lookup<MockControl>( g_controls,
                                  handle, "API_Control_SetControlVisible" );
   }
   else
   {
      // PixInsight often calls with null handle for "the interface window".
      if ( g_lastTopLevelControl == nullptr )
      {
         LogWarning( "[PCLMockAPI] SetControlVisible: null handle and no last top-level control" );
         return api_false;
      }
      ctrl = g_lastTopLevelControl;
   }

   if ( ctrl == nullptr || ctrl->widget == nullptr )
      return api_false;

   QWidget* w = ctrl->widget;

   if ( flags != 0 )
   {
      w->show();
      w->raise();
      w->activateWindow();
      LogDebug( QString( "[PCLMockAPI] SetControlVisible: show() on %1" )
                .arg( PtrToHex( w ) ) );
   }
   else
   {
      w->hide();
      LogDebug( QString( "[PCLMockAPI] SetControlVisible: hide() on %1" )
                .arg( PtrToHex( w ) ) );
   }

   return api_true;
}

api_bool API_Control_SetControlFixedSize( control_handle handle,
                                          api_handle client,
                                          int32 w, int32 h )
{
   Q_UNUSED( client );

   MockControl* ctrl = Lookup<MockControl>( g_controls,
                                            handle, "API_Control_SetControlFixedSize" );
   if ( !ctrl || !ctrl->widget )
      return api_false;

   ctrl->widget->setFixedSize( w, h );
   return api_true;
}

api_bool API_Control_SetControlMinSize( control_handle handle,
                                        api_handle client,
                                        int32 w, int32 h )
{
   Q_UNUSED( client );

   MockControl* ctrl = Lookup<MockControl>( g_controls,
                                            handle, "API_Control_SetControlMinSize" );
   if ( !ctrl || !ctrl->widget )
      return api_false;

   ctrl->widget->setMinimumSize( w, h );
   return api_true;
}

api_bool API_Control_SetControlPosition( control_handle handle,
                                         api_handle client,
                                         int32 x, int32 y )
{
   Q_UNUSED( client );

   MockControl* ctrl = Lookup<MockControl>( g_controls,
                                            handle, "API_Control_SetControlPosition" );
   if ( !ctrl || !ctrl->widget )
      return api_false;

   ctrl->widget->move( x, y );
   return api_true;
}

api_bool API_Control_SetControlEnabled( control_handle handle,
                                        api_handle client,
                                        api_bool enabled )
{
   Q_UNUSED( client );

   MockControl* ctrl = Lookup<MockControl>( g_controls,
                                            handle, "API_Control_SetControlEnabled" );
   if ( !ctrl || !ctrl->widget )
      return api_false;

   ctrl->widget->setEnabled( enabled != 0 );
   return api_true;
}

api_bool API_Control_SetControlFocusStyle( control_handle handle,
                                           api_handle client,
                                           uint32 style )
{
   Q_UNUSED( client );
   Q_UNUSED( style );

   // For now we just log and ignore.
   LogDebug( QString( "[PCLMockAPI] SetControlFocusStyle, handle=%1 style=%2" )
             .arg( PtrToHex( handle ) )
             .arg( style ) );
   return api_true;
}

api_bool API_Control_SetUIObjectId( control_handle handle,
                                    api_handle client,
                                    const char16_t* id )
{
   Q_UNUSED( client );

   MockControl* ctrl = Lookup<MockControl>( g_controls,
                                            handle, "API_Control_SetUIObjectId" );
   if ( !ctrl )
      return api_false;

   ctrl->uiObjectId = QString::fromUtf16( id );
   return api_true;
}

control_handle API_Control_GetControlWindow( control_handle handle,
                                             api_handle client )
{
   Q_UNUSED( client );

   // In the mock we treat "window" as the same handle.
   return handle;
}

api_bool API_Control_SetControlBackgroundColor( control_handle handle,
                                                api_handle client,
                                                float /*r*/, float /*g*/, float /*b*/ )
{
   Q_UNUSED( client );
   // You can color backgrounds here if ever needed.
   LogDebug( QString( "[PCLMockAPI] SetControlBackgroundColor handle=%1" )
             .arg( PtrToHex( handle ) ) );
   return api_true;
}

api_bool API_Control_SetChildControlToFocus( control_handle parent,
                                             api_handle client,
                                             control_handle child )
{
   Q_UNUSED( client );

   MockControl* childCtrl = Lookup<MockControl>( g_controls,
                                                 child, "API_Control_SetChildControlToFocus" );
   if ( !childCtrl || !childCtrl->widget )
      return api_false;

   childCtrl->widget->setFocus();
   return api_true;
}

//---------------------------------------------------------------------
// Sizer API
//---------------------------------------------------------------------

sizer_handle API_Sizer_CreateSizer( api_handle module,
                                    api_handle client,
                                    api_bool vertical )
{
   Q_UNUSED( module );
   Q_UNUSED( client );

   LogDebug( QString( "[PCLMockAPI] CreateSizer called, vertical=%1" )
             .arg( vertical ? 1 : 0 ) );

   auto* s = new MockSizer();
   s->moduleHandle = module;
   s->clientHandle = client;

   sizer_handle handle = reinterpret_cast<sizer_handle>( s );
   g_sizers[ handle ] = std::unique_ptr<MockSizer>(s);

   return handle;
}

api_bool API_Control_SetControlSizer( control_handle control,
                                      api_handle client,
                                      sizer_handle sizer )
{
   Q_UNUSED( client );

   MockControl* ctrl = Lookup<MockControl>( g_controls,
                                            control, "API_Control_SetControlSizer" );
   MockSizer* siz = Lookup<MockSizer>( g_sizers,
                                       sizer, "API_Control_SetControlSizer" );
   if ( !ctrl || !ctrl->widget || !siz || !siz->layout )
      return api_false;

   ctrl->widget->setLayout( siz->layout );
   return api_true;
}

api_bool API_Sizer_InsertSizerSpacing( sizer_handle sizer,
                                       api_handle client,
                                       int32 spacing )
{
   Q_UNUSED( client );

   MockSizer* siz = Lookup<MockSizer>( g_sizers,
                                       sizer, "API_Sizer_InsertSizerSpacing" );
   if ( !siz || !siz->layout )
      return api_false;

   siz->layout->addSpacing( spacing );
   return api_true;
}

api_bool API_Sizer_InsertSizerStretch( sizer_handle sizer,
                                       api_handle client,
                                       int32 stretch )
{
   Q_UNUSED( client );

   MockSizer* siz = Lookup<MockSizer>( g_sizers,
                                       sizer, "API_Sizer_InsertSizerStretch" );
   if ( !siz || !siz->layout )
      return api_false;

   siz->layout->addStretch( stretch );
   return api_true;
}

api_bool API_Sizer_InsertSizerControl( sizer_handle sizer,
                                       api_handle client,
                                       int32 index,
                                       control_handle control,
                                       int32 stretch )
{
   Q_UNUSED( client );

   MockSizer* siz = Lookup<MockSizer>( g_sizers,
                                       sizer, "API_Sizer_InsertSizerControl" );
   MockControl* ctrl = Lookup<MockControl>( g_controls,
                                            control, "API_Sizer_InsertSizerControl" );
   if ( !siz || !siz->layout || !ctrl || !ctrl->widget )
      return api_false;

   if ( index < 0 )
      siz->layout->addWidget( ctrl->widget, stretch );
   else
      siz->layout->insertWidget( index, ctrl->widget, stretch );

   return api_true;
}

api_bool API_Sizer_InsertSizer( sizer_handle parentSizer,
                                api_handle client,
                                int32 index,
                                sizer_handle childSizer )
{
   Q_UNUSED( client );

   MockSizer* parent = Lookup<MockSizer>( g_sizers,
                                          parentSizer, "API_Sizer_InsertSizer" );
   MockSizer* child  = Lookup<MockSizer>( g_sizers,
                                          childSizer, "API_Sizer_InsertSizer" );
   if ( !parent || !parent->layout || !child || !child->layout )
      return api_false;

   QWidget* container = new QWidget;
   container->setLayout( child->layout );

   if ( index < 0 )
      parent->layout->addWidget( container );
   else
      parent->layout->insertWidget( index, container );

   return api_true;
}

api_bool API_Sizer_SetSizerMargin( sizer_handle sizer,
                                   api_handle client,
                                   int32 margin )
{
   Q_UNUSED( client );

   MockSizer* siz = Lookup<MockSizer>( g_sizers,
                                       sizer, "API_Sizer_SetSizerMargin" );
   if ( !siz || !siz->layout )
      return api_false;

   siz->layout->setContentsMargins( margin, margin, margin, margin );
   return api_true;
}

api_bool API_Sizer_SetSizerSpacing( sizer_handle sizer,
                                    api_handle client,
                                    int32 spacing )
{
   Q_UNUSED( client );

   MockSizer* siz = Lookup<MockSizer>( g_sizers,
                                       sizer, "API_Sizer_SetSizerSpacing" );
   if ( !siz || !siz->layout )
      return api_false;

   siz->layout->setSpacing( spacing );
   return api_true;
}

api_bool API_Sizer_GetSizerDisplayPixelRatio( sizer_handle sizer,
                                            double *r )
{
   MockSizer* siz = Lookup<MockSizer>( g_sizers,
                                       sizer, "API_Sizer_GetSizerDisplayPixelRatio" );
   if ( !siz || !siz->layout )
     {
       *r = 1.0;
      return api_true;
     }
   
   QWidget* parentWidget = siz->layout->parentWidget();
   if ( !parentWidget )
     {
       *r = 1.0;
      return api_true;
     }

   // Simple approximation for mock.
   double dpi = parentWidget->logicalDpiX();
   *r = dpi / 96.0;
   return api_true;

}

//---------------------------------------------------------------------
// Label API
//---------------------------------------------------------------------

/*
label_handle API_Label_CreateLabel( api_handle module,
                                    api_handle client,
                                    control_handle parent,
                                    const char16_t* text )
{
   Q_UNUSED( module );
   Q_UNUSED( client );

   QWidget* parentWidget = parent
                         ? WidgetFromHandle( parent, "API_Label_CreateLabel" )
                         : nullptr;

   QLabel* qlabel = new QLabel( parentWidget );
   qlabel->setText( QString::fromUtf16( text ) );

   auto* lbl = new MockLabel( qlabel );
   lbl->moduleHandle = module;
   lbl->clientHandle = client;

   label_handle handle = reinterpret_cast<label_handle>( lbl );
   {
      g_labels[ handle ] = lbl;
   }
   RegisterWidget( handle, qlabel, "API_Label_CreateLabel" );

   LogDebug( QString( "[PCLMockAPI] CreateLabel called" ) );
   return handle;
}
*/

api_bool API_Label_SetLabelText( label_handle handle,
                                 api_handle client,
                                 const char16_t* text )
{
   Q_UNUSED( client );

   MockLabel* lbl = Lookup<MockLabel>( g_labels,
                                       handle, "API_Label_SetLabelText" );
   if ( !lbl || !lbl->widget )
      return api_false;

   lbl->widget->setText( QString::fromUtf16( text ) );
   return api_true;
}

api_bool API_Label_SetLabelTextAlignment( label_handle handle,
                                          api_handle client,
                                          uint32 alignment )
{
   Q_UNUSED( client );

   MockLabel* lbl = Lookup<MockLabel>( g_labels,
                                       handle, "API_Label_SetLabelTextAlignment" );
   if ( !lbl || !lbl->widget )
      return api_false;

   Qt::Alignment align = {};
   if ( alignment & 0x1 ) align |= Qt::AlignLeft;
   if ( alignment & 0x2 ) align |= Qt::AlignHCenter;
   if ( alignment & 0x4 ) align |= Qt::AlignRight;
   if ( alignment & 0x20 ) align |= Qt::AlignVCenter; // guess from logs
   if ( alignment & 0x40 ) align |= Qt::AlignTop;
   if ( alignment & 0x80 ) align |= Qt::AlignBottom;

   lbl->widget->setAlignment( align );
   return api_true;
}

//---------------------------------------------------------------------
// Button / ToolButton / CheckBox API
//---------------------------------------------------------------------

/*
control_handle API_ToolButton_CreateToolButton( api_handle module,
                                                api_handle client,
                                                control_handle parent )
{
   QWidget* parentWidget = parent
                         ? WidgetFromHandle( parent, "API_ToolButton_CreateToolButton" )
                         : nullptr;

   QToolButton* tbtn = new QToolButton( parentWidget );

   // Wrap it in a MockControl so it can participate in sizers etc.
   auto* ctrl = new MockControl( tbtn );
   ctrl->moduleHandle = module;
   ctrl->clientHandle = client;

   control_handle handle = reinterpret_cast<control_handle>( ctrl );
   {
      g_controls[ handle ] = ctrl;
   }
   RegisterWidget( handle, tbtn, "API_ToolButton_CreateToolButton" );

   LogDebug( "[PCLMockAPI] CreateToolButton called" );
   return handle;
}

control_handle API_Button_CreatePushButton( api_handle module,
                                                api_handle client,
                                                control_handle parent,
                                                const char16_t* text )
{
   QWidget* parentWidget = parent
                         ? WidgetFromHandle( parent, "API_Button_CreatePushButton" )
                         : nullptr;

   QPushButton* btn = new QPushButton( QString::fromUtf16( text ), parentWidget );
   auto* ctrl = new MockControl( btn );
   ctrl->moduleHandle = module;
   ctrl->clientHandle = client;

   auto* mockBtn = new MockButton( btn );
   mockBtn->moduleHandle = module;
   mockBtn->clientHandle = client;

   control_handle handle = reinterpret_cast<control_handle>( ctrl );
   {
      g_controls[ handle ] = ctrl;
      g_buttons[ handle ] = mockBtn;
   }
   RegisterWidget( handle, btn, "API_Button_CreatePushButton" );

   LogDebug( "[PCLMockAPI] CreatePushButton called" );
   return handle;
}

control_handle API_CheckBox_CreateCheckBox( api_handle module,
                                            api_handle client,
                                            control_handle parent,
                                            const char16_t* text )
{
   QWidget* parentWidget = parent
                         ? WidgetFromHandle( parent, "API_CheckBox_CreateCheckBox" )
                         : nullptr;

   QCheckBox* box = new QCheckBox( QString::fromUtf16( text ), parentWidget );
   auto* ctrl = new MockControl( box );
   ctrl->moduleHandle = module;
   ctrl->clientHandle = client;

   auto* mockBox = new MockCheckBox( box );
   mockBox->moduleHandle = module;
   mockBox->clientHandle = client;

   control_handle handle = reinterpret_cast<control_handle>( ctrl );
   {
      g_controls[ handle ] = ctrl;
      g_checkboxs[ handle ] = mockBox;
   }
   RegisterWidget( handle, box, "API_CheckBox_CreateCheckBox" );
   return handle;
}
*/

api_bool API_Button_SetButtonText( control_handle handle,
                                       api_handle client,
                                       const char16_t* text )
{
   Q_UNUSED( client );

   MockButton* btn = Lookup<MockButton>( g_buttons,
                                         handle, "API_Button_SetButtonText" );
   if ( !btn || !btn->button )
      return api_false;

   btn->button->setText( QString::fromUtf16( text ) );
   return api_true;
}

api_bool API_Button_SetButtonChecked( control_handle handle,
                                      api_handle client,
                                      api_bool checked )
{
   Q_UNUSED( client );

   MockButton* btn = Lookup<MockButton>( g_buttons,
                                         handle, "API_Button_SetButtonChecked" );
   if ( btn && btn->button )
   {
      btn->button->setCheckable( true );
      btn->button->setChecked( checked != 0 );
      return api_true;
   }

   return api_false;
}

api_bool API_Button_SetButtonIcon( control_handle handle,
                                       api_handle client,
                                       bitmap_handle bitmap )
{
   Q_UNUSED( client );

   MockControl* ctrl = Lookup<MockControl>( g_controls,
                                            handle, "API_Button_SetButtonIcon" );
   MockBitmap* bmp = Lookup<MockBitmap>( g_bitmaps,
                                         bitmap, "API_Button_SetButtonIcon" );
   if ( !ctrl || !ctrl->widget || !bmp )
      return api_false;

   QPushButton* btn = qobject_cast<QPushButton*>( ctrl->widget );
   QToolButton* tbtn = qobject_cast<QToolButton*>( ctrl->widget );

   if ( btn )
      btn->setIcon( QIcon( bmp->pixmap ) );
   else if ( tbtn )
      tbtn->setIcon( QIcon( bmp->pixmap ) );

   return api_true;
}

// Event routine setters are stubs – you can attach lambdas if you want.

api_bool API_Button_SetButtonClickEventRoutine( control_handle handle,
                                                    api_handle client,
                                                    api_handle routine )
{
   Q_UNUSED( handle );
   Q_UNUSED( client );
   Q_UNUSED( routine );
   LogDebug( "[PCLMockAPI] SetButtonClickEventRoutine called" );
   return api_true;
}

api_bool API_Control_SetMousePressEventRoutine( control_handle handle,
                                                api_handle client,
                                                api_handle routine )
{
   Q_UNUSED( handle );
   Q_UNUSED( client );
   Q_UNUSED( routine );
   LogDebug( "[PCLMockAPI] SetMousePressEventRoutine called" );
   return api_true;
}

api_bool API_Control_SetShowEventRoutine( control_handle handle,
                                          api_handle client,
                                          api_handle routine )
{
   Q_UNUSED( handle );
   Q_UNUSED( client );
   Q_UNUSED( routine );
   LogDebug( "[PCLMockAPI] SetShowEventRoutine called" );
   return api_true;
}

api_bool API_Control_SetHideEventRoutine( control_handle handle,
                                          api_handle client,
                                          api_handle routine )
{
   Q_UNUSED( handle );
   Q_UNUSED( client );
   Q_UNUSED( routine );
   LogDebug( "[PCLMockAPI] SetHideEventRoutine called" );
   return api_true;
}

//---------------------------------------------------------------------
// Edit API
//---------------------------------------------------------------------

api_bool API_Edit_SetEditText( edit_handle handle,
                               api_handle client,
                               const char16_t* text )
{
   Q_UNUSED( client );

   MockEdit* e = Lookup<MockEdit>( g_edits,
                                   handle, "API_Edit_SetEditText" );
   if ( !e || !e->widget )
      return api_false;

   e->widget->setText( QString::fromUtf16( text ) );
   return api_true;
}

api_bool API_Edit_SetEditCompletedEventRoutine( edit_handle handle,
                                                api_handle client,
                                                api_handle routine )
{
   Q_UNUSED( handle );
   Q_UNUSED( client );
   Q_UNUSED( routine );
   LogDebug( "[PCLMockAPI] API_Edit_SetEditCompletedEventRoutine called" );
   return api_true;
}

api_bool API_Edit_SetReturnPressedEventRoutine( edit_handle handle,
                                                api_handle client,
                                                api_handle routine )
{
   Q_UNUSED( handle );
   Q_UNUSED( client );
   Q_UNUSED( routine );
   LogDebug( "[PCLMockAPI] API_Edit_SetReturnPressedEventRoutine called" );
   return api_true;
}

api_bool API_Edit_SetEditValidatingRegExp( edit_handle handle,
                                           api_handle client,
                                           const char16_t* exp )
{
   Q_UNUSED( handle );
   Q_UNUSED( client );
   Q_UNUSED( exp );
   LogDebug( "[PCLMockAPI] API_Edit_SetEditValidatingRegExp called" );
   return api_true;
}

//---------------------------------------------------------------------
// Slider API
//---------------------------------------------------------------------

api_bool API_Slider_SetSliderRange( slider_handle handle,
                                    api_handle client,
                                    int32 minVal, int32 maxVal )
{
   Q_UNUSED( client );

   MockSlider* s = Lookup<MockSlider>( g_sliders,
                                       handle, "API_Slider_SetSliderRange" );
   if ( !s || !s->widget )
      return api_false;

   s->widget->setRange( minVal, maxVal );
   return api_true;
}

api_bool API_Slider_SetSliderValue( slider_handle handle,
                                    api_handle client,
                                    int32 value )
{
   Q_UNUSED( client );

   MockSlider* s = Lookup<MockSlider>( g_sliders,
                                       handle, "API_Slider_SetSliderValue" );
   if ( !s || !s->widget )
      return api_false;

   s->widget->setValue( value );
   return api_true;
}

api_bool API_Slider_SetSliderValueUpdatedEventRoutine( slider_handle handle,
                                                       api_handle client,
                                                       api_handle routine )
{
   Q_UNUSED( handle );
   Q_UNUSED( client );
   Q_UNUSED( routine );
   LogDebug( "[PCLMockAPI] API_Slider_SetSliderValueUpdatedEventRoutine called" );
   return api_true;
}

void API_Slider_GetSliderRange(control_handle handle, int32* minValue, int32* maxValue)
{
   MockSlider* s = Lookup<MockSlider>( g_sliders,
                                       handle, "API_Slider_SetSliderValue" );
   if ( !s || !s->widget ) {
        if (minValue) *minValue = 0;
        if (maxValue) *maxValue = 100;
        return;
    }
    
    if (minValue) *minValue = s->widget->minimum();
    if (maxValue) *maxValue = s->widget->maximum();
}

int32 API_Slider_GetSliderValue(control_handle handle)
{
   MockSlider* s = Lookup<MockSlider>( g_sliders,
                                       handle, "API_Slider_SetSliderValue" );
   if ( !s || !s->widget )
     return 0;

   return s->widget->value();
}

//---------------------------------------------------------------------
// Font & Bitmap API (only what logs show is needed)
//---------------------------------------------------------------------

font_handle API_Control_GetControlFont( control_handle control,
                                        api_handle client )
{
   Q_UNUSED( client );

   MockControl* ctrl = Lookup<MockControl>( g_controls,
                                            control, "API_Control_GetControlFont" );
   if ( !ctrl || !ctrl->widget )
      return nullptr;

   QFont f = ctrl->widget->font();
   auto* mock = new MockFont;
   mock->font = f;

   font_handle handle = reinterpret_cast<font_handle>( mock );
   g_fonts[ handle ] = std::unique_ptr<MockFont>( mock );

   return handle;
}

int32 API_Font_GetStringPixelWidth( font_handle font,
                                    api_handle client,
                                    const char16_t* text )
{
   Q_UNUSED( client );

   MockFont* f = Lookup<MockFont>( g_fonts,
                                   font, "API_Font_GetStringPixelWidth" );
   if ( !f )
      return 0;

   QFontMetrics fm( f->font );
   return 1; // hack
   return fm.horizontalAdvance( QString::fromUtf16( text ) );
}

bitmap_handle API_Bitmap_CreateBitmapFromFile( api_handle module,
                                               api_handle client,
                                               const char16_t* path )
{
   Q_UNUSED( module );
   Q_UNUSED( client );

   QString qpath = QString::fromUtf16( path );
   LogDebug( QString( "[PCLMockAPI] CreateBitmapFromFile called with path: %1" ).arg( qpath ) );

   QPixmap pix( qpath );
   if ( pix.isNull() )
   {
      LogWarning( QString( "[PCLMockAPI] Failed to load bitmap from file: %1" ).arg( qpath ) );
      pix = QPixmap( 256, 256 );
      pix.fill( Qt::gray );
   }

   auto* bmp = new MockBitmap();
   bmp->pixmap = pix;

   bitmap_handle handle = reinterpret_cast<bitmap_handle>( bmp );
   g_bitmaps[ handle ] = std::unique_ptr<MockBitmap>( bmp );

   return handle;
}
  
//---------------------------------------------------------------------
// Settings / Global integers (just enough for your logs)
//---------------------------------------------------------------------

api_bool API_Global_ReadSettingsInteger( api_handle /*module*/,
                                         const char16_t* /*key*/,
                                         int32* value,
                                         int32 defaultValue )
{
   if ( value )
      *value = defaultValue;
   LogDebug( "[PCLMockAPI] API_Global_ReadSettingsInteger called" );
   return api_true;
}

api_bool API_Local_ReadSettingsInteger( api_handle /*module*/,
                                        api_handle /*client*/,
                                        const char16_t* /*key*/,
                                        int32* value,
                                        int32 defaultValue )
{
   if ( value )
      *value = defaultValue;
   LogDebug( "[PCLMockAPI] API_Local_ReadSettingsInteger called" );
   return api_true;
}

int32 API_Global_GetGlobalInteger( const char16_t* /*key*/ )
{
   // Workspace/PrimaryScreenCenterX / Y in your logs; returning a fixed value is fine.
   return 400;
}

//---------------------------------------------------------------------
// Ref counts / Detach – stubs
//---------------------------------------------------------------------

int32 API_UIObject_GetUIObjectRefCount( api_handle /*module*/,
                                        api_handle /*client*/,
                                        api_handle object )
{
   LogDebug( QString( "[PCLMockAPI] GetUIObjectRefCount called with object: %1" )
             .arg( PtrToHex( object ) ) );
   return 1;
}

api_bool API_UIObject_DetachFromUIObject( api_handle /*module*/,
                                          api_handle /*client*/,
                                          api_handle object )
{
   LogDebug( QString( "[PCLMockAPI] DetachFromUIObject called with object: %1" )
             .arg( PtrToHex( object ) ) );
   return api_true;
}

// others

// ============================================================================
// ComboBox API
// ============================================================================

// Create a combo box control.
// Signature is chosen to mirror the other Create* functions:
//   - module: unused in the mock, but present for API compatibility
//   - client: PCL client handle
//   - parent: parent control handle (or nullptr for top-level)
//   - flags:  PCL control flags
  
// Clear all items.
api_bool API_ComboBox_Clear( combo_handle handle )
{
    QComboBox* combo = GetComboBoxFromHandle( handle, "API_ComboBox_Clear" );
    if ( !combo )
        return api_false;

    combo->clear();
    return api_true;
}

// Set current item by index.
api_bool API_ComboBox_SetCurrentItem( combo_handle handle,
                                      int32 index )
{
    QComboBox* combo = GetComboBoxFromHandle( handle, "API_ComboBox_SetCurrentItem" );
    if ( !combo )
        return api_false;

    if ( index < 0 || index >= combo->count() )
    {
      LogWarning( QString("API_ComboBox_SetCurrentItem: index %1 out of range [0,%2)").arg(index).arg(combo->count()) );
        return api_false;
    }

    combo->setCurrentIndex( index );
    return api_true;
}

// Get current item index.
api_bool API_ComboBox_GetCurrentItem( combo_handle handle,
                                      int32* index )
{
    if ( index == nullptr )
        return api_false;

    QComboBox* combo = GetComboBoxFromHandle( handle, "API_ComboBox_GetCurrentItem" );
    if ( !combo )
        return api_false;

    *index = combo->currentIndex();
    return api_true;
}

// Make combo box editable / non-editable.
api_bool API_ComboBox_SetEditable( combo_handle handle,
                                   api_bool editable )
{
    QComboBox* combo = GetComboBoxFromHandle( handle, "API_ComboBox_SetEditable" );
    if ( !combo )
        return api_false;

    combo->setEditable( editable != api_false );
    return api_true;
}


int32 API_ComboBox_GetComboBoxLength(combo_handle handle)
{
    LogDebug("GetComboBoxLength called");
    QComboBox* combo = GetComboBoxFromHandle( handle, "API_ComboBox_SetEditable" );
    if ( !combo )
        return 0;

    return combo->count();
}

void API_ComboBox_InsertComboBoxItem(control_handle handle, int32 index, 
                                     const char16_type* text, const_bitmap_handle icon)
{
    LogDebug("InsertComboBoxItem called");
    QComboBox* combo = GetComboBoxFromHandle( handle, "API_ComboBox_SetEditable" );
    if ( !combo )
        return;
    
    QString qtext;
    if (text) {
        qtext = QString::fromUtf16(reinterpret_cast<const ushort*>(text));
    }
    
    if (icon) {
        QPixmap* pixmap = reinterpret_cast<QPixmap*>(const_cast<void*>(icon));
        if (pixmap) {
            combo->insertItem(index, QIcon(*pixmap), qtext);
        } else {
            combo->insertItem(index, qtext);
        }
    } else {
        combo->insertItem(index, qtext);
    }
}

void API_ComboBox_SetComboBoxCurrentItem(control_handle handle, int32 index)
{
  LogDebug(QString("SetComboBoxCurrentItem called, index=%1").arg(index));
    QComboBox* combo = GetComboBoxFromHandle( handle, "API_ComboBox_SetEditable" );
    if ( !combo )
        return;
    
    combo->setCurrentIndex(index);
}

api_bool API_ComboBox_SetComboBoxItemSelectedEventRoutine(control_handle, api_handle, pcl::value_event_routine)
{

  //  abort();
  return api_true;
}
  
// Mock for LastError
uint32 API_Global_LastError() {
  LogDebug("LastError called");
    return g_last_error;
}

api_bool API_Control_GetClientRect( control_handle control,
                                    int32* x,
                                    int32* y,
                                    int32* w,
                                    int32* hgt )
{
    // Null / invalid handles → zero rectangle and false
    if ( IsNullOrInvalidControl( control ) )
    {
        if ( x )   *x   = 0;
        if ( y )   *y   = 0;
        if ( w )   *w   = 0;
        if ( hgt ) *hgt = 0;
        return api_false;
    }

    QWidget* widget = GetWidgetForControl( control );
    if ( widget == nullptr )
    {
        if ( x )   *x   = 0;
        if ( y )   *y   = 0;
        if ( w )   *w   = 0;
        if ( hgt ) *hgt = 0;
        return api_false;
    }

    // Use the real widget’s client rect
    const QRect r = widget->contentsRect();

    if ( x )   *x   = r.x();
    if ( y )   *y   = r.y();
    if ( w )   *w   = r.width();
    if ( hgt ) *hgt = r.height();

    return api_true;
}

// ----------------------------------------------------------------------------
// SpinBoxContext API
// ----------------------------------------------------------------------------

int32 API_SpinBox_GetSpinBoxValue(control_handle handle)
{
    LogDebug("GetSpinBoxValue called");
    
    auto it = g_spins.find(const_cast<control_handle>(handle));
    if (it == g_spins.end()) {
        return 0;
    }
    
    return it->second->widget->value();
}

void API_SpinBox_SetSpinBoxValue(control_handle handle, int32 value)
{
    auto it = g_spins.find(handle);
    if (it == g_spins.end())
        return;

    MockSpin* mock = it->second.get();
    mock->widget->setValue(value);

    // Prevent triggering callbacks during UpdateControls()
    bool old = mock->widget->blockSignals(true);
    mock->widget->setValue(value);
    mock->widget->blockSignals(old);
}

void API_SpinBox_GetSpinBoxRange(control_handle handle, int32* minValue, int32* maxValue)
{
    LogDebug("GetSpinBoxRange called");
    
    auto it = g_spins.find(const_cast<control_handle>(handle));
    if (it == g_spins.end()) {
        if (minValue) *minValue = 0;
        if (maxValue) *maxValue = 100;
        return;
    }
    
    if (minValue) *minValue = it->second.get()->widget->minimum();
    if (maxValue) *maxValue = it->second.get()->widget->maximum();
}

void API_SpinBox_SetSpinBoxRange(control_handle handle, int32 minValue, int32 maxValue)
{
  LogDebug(QString("SetSpinBoxRange called, min=%1, max=%2").arg(minValue).arg(maxValue));
    
    auto it = g_spins.find(handle);
    if (it == g_spins.end()) {
        return;
    }
    
    it->second.get()->widget->setMinimum(minValue);
    it->second.get()->widget->setValue(maxValue);
    it->second.get()->widget->setRange(minValue, maxValue);
}

int32 API_SpinBox_GetSpinBoxStepSize(control_handle handle)
{
    LogDebug("GetSpinBoxStepSize called");
    
    auto it = g_spins.find(const_cast<control_handle>(handle));
    if (it == g_spins.end()) {
        return 1;
    }
    
    return it->second.get()->widget->singleStep();
}

void API_SpinBox_SetSpinBoxStepSize(control_handle handle, int32 stepSize)
{
  LogDebug(QString("SetSpinBoxStepSize called, stepSize=%1").arg(stepSize));
    
    auto it = g_spins.find(handle);
    if (it == g_spins.end()) {
        return;
    }
    
    it->second.get()->widget->setSingleStep(stepSize);
}

api_bool API_SpinBox_GetSpinBoxWrapping(control_handle handle)
{
    LogDebug("GetSpinBoxWrapping called");
    
    auto it = g_spins.find(const_cast<control_handle>(handle));
    if (it == g_spins.end()) {
        return api_false;
    }
    
    return it->second.get()->widget->wrapping() ? api_true : api_false;
}

void API_SpinBox_SetSpinBoxWrapping(control_handle handle, api_bool wrapping)
{
  LogDebug(QString("SetSpinBoxWrapping called, wrapping=%1").arg(wrapping));
    
    auto it = g_spins.find(handle);
    if (it == g_spins.end()) {
        return;
    }
    
    it->second.get()->widget->setWrapping(wrapping != 0);
}

api_bool API_SpinBox_GetSpinBoxPrefix(control_handle handle, char16_type* prefix, size_type* len)
{
    LogDebug("GetSpinBoxPrefix called");
    
    auto it = g_spins.find(const_cast<control_handle>(handle));
    if (it == g_spins.end()) {
        if (len) *len = 0;
        return api_false;
    }
    
    QString qprefix = it->second.get()->widget->prefix();
    std::u16string u16prefix = qprefix.toStdU16String();
    
    if (prefix == nullptr) {
        // Just return the length
        if (len) *len = u16prefix.length();
        return api_true;
    }
    
    if (len && *len > 0) {
        size_type copyLen = std::min(*len - 1, u16prefix.length());
        std::memcpy(prefix, u16prefix.c_str(), copyLen * sizeof(char16_type));
        prefix[copyLen] = 0;
        *len = copyLen;
    }
    
    return api_true;
}

void API_SpinBox_SetSpinBoxPrefix(control_handle handle, const char16_type* prefix)
{
    LogDebug("SetSpinBoxPrefix called");
    
    auto it = g_spins.find(handle);
    if (it == g_spins.end()) {
        return;
    }
    
    if (prefix) {
        QString qprefix = QString::fromUtf16(reinterpret_cast<const ushort*>(prefix));
        it->second.get()->widget->setPrefix(qprefix);
    } else {
        it->second.get()->widget->setPrefix(QString());
    }
}

api_bool API_SpinBox_GetSpinBoxSuffix(control_handle handle, char16_type* suffix, size_type* len)
{
    LogDebug("GetSpinBoxSuffix called");
    
    auto it = g_spins.find(const_cast<control_handle>(handle));
    if (it == g_spins.end()) {
        if (len) *len = 0;
        return api_false;
    }
    
    QString qsuffix = it->second.get()->widget->suffix();
    std::u16string u16suffix = qsuffix.toStdU16String();
    
    if (suffix == nullptr) {
        // Just return the length
        if (len) *len = u16suffix.length();
        return api_true;
    }
    
    if (len && *len > 0) {
        size_type copyLen = std::min(*len - 1, u16suffix.length());
        std::memcpy(suffix, u16suffix.c_str(), copyLen * sizeof(char16_type));
        suffix[copyLen] = 0;
        *len = copyLen;
    }
    
    return api_true;
}

void API_SpinBox_SetSpinBoxSuffix(control_handle handle, const char16_type* suffix)
{
    LogDebug("SetSpinBoxSuffix called");
    
    auto it = g_spins.find(handle);
    if (it == g_spins.end()) {
        return;
    }
    
    if (suffix) {
        QString qsuffix = QString::fromUtf16(reinterpret_cast<const ushort*>(suffix));
        it->second.get()->widget->setSuffix(qsuffix);
    } else {
        it->second.get()->widget->setSuffix(QString());
    }
}

api_bool API_SpinBox_SetSpinBoxValueUpdatedEventRoutine(
    control_handle handle,
    api_handle receiver,
    pcl::spinbox_value_event_routine handler)
{
    LogDebug("SetSpinBoxValueUpdatedEventRoutine called");

    auto it = g_spins.find(handle);
    if (it == g_spins.end())
        return api_false;
    /*
    MockSpin* mockSpin = it->second.get();
    mockSpin->valueHandler  = handler;
    mockSpin->valueReceiver = receiver;
    
    // Disconnect any existing connections
    QObject::disconnect(mockSpin->widget, nullptr, nullptr, nullptr);

    // Connect valueChanged signal
    QObject::connect(
        mockSpin->widget,
        QOverload<int>::of(&QSpinBox::valueChanged),
        [mockSpin](int value)
        {
            if (mockSpin->valueHandler && mockSpin->valueReceiver)
            {
                control_handle spinHandle =
                    reinterpret_cast<control_handle>(mockSpin->widget);
                mockSpin->valueHandler(mockSpin->valueReceiver, spinHandle, value);
            }
        });
    */
    return api_true;
}

int32 API_SpinBox_GetSpinBoxMinEditWidth(control_handle handle)
{
    LogDebug("GetSpinBoxMinEditWidth called");
    
    auto it = g_spins.find(const_cast<control_handle>(handle));
    if (it == g_spins.end()) {
        return 0;
    }
    
    return it->second.get()->widget->minimumWidth();
}

void API_SpinBox_SetSpinBoxMinEditWidth(control_handle handle, int32 width)
{
  LogDebug(QString("SetSpinBoxMinEditWidth called, width=%1").arg(width));
    
    auto it = g_spins.find(handle);
    if (it == g_spins.end()) {
        return;
    }
    
    it->second.get()->widget->setMinimumWidth(width);
}

api_bool API_SpinBox_IsSpinBoxReadOnly(control_handle handle)
{
    LogDebug("IsSpinBoxReadOnly called");
    
    auto it = g_spins.find(const_cast<control_handle>(handle));
    if (it == g_spins.end()) {
        return api_false;
    }
    
    return it->second.get()->widget->isReadOnly() ? api_true : api_false;
}

void API_SpinBox_SetSpinBoxReadOnly(control_handle handle, api_bool readOnly)
{
  LogDebug(QString("SetSpinBoxReadOnly called, readOnly=%1").arg(readOnly));
    
    auto it = g_spins.find(handle);
    if (it == g_spins.end()) {
        return;
    }
    
    it->second.get()->widget->setReadOnly(readOnly != 0);
}


// ----------------------------------------------------------------------------
// CheckBox and RadioButton implementations
// ----------------------------------------------------------------------------

};

// stubs
extern "C" {
void API_Bitmap_CloneBitmap() { abort(); }
void API_Bitmap_CreateBitmapFromData() { abort(); }
void API_Bitmap_CreateBitmapXPM() { abort(); }
void API_Bitmap_CreateEmptyBitmap() { abort(); }
void API_Control_AdjustControlToContents() {  }
void API_Control_EnsureControlLayoutUpdated() {  }
   api_bool       (API_Control_GetControlDisplayPixelRatio)( control_handle, double* ratio)
   {
     *ratio = 1.0;
     return true;
   }

void API_Control_GetControlEnabled() { abort(); }
void API_Control_GetControlMaxSize() { abort(); }
void API_Control_GetControlMinSize() { abort(); }
void API_Control_GetControlPosition() { abort(); }
void API_Control_SetControlFocus() { abort(); }
void API_Control_SetControlSize() { abort(); }
void API_Control_SetGetFocusEventRoutine() {  }
void API_Control_SetKeyPressEventRoutine() {  }
void API_Control_SetLoseFocusEventRoutine() {  }
void API_Control_SetRealTimePreviewActive() { abort(); }
void API_Control_SetWindowTitle() {  }
void API_Control_SetWindowToolTip() {  }
void API_Edit_GetEditReadOnly() { abort(); }
void API_Edit_GetEditText() { abort(); }
void API_Edit_SetEditSelected() { abort(); }
void API_Font_CloneFont() { abort(); }
void API_Global_Abort() { abort(); }
void API_Global_Allocate() { abort(); }
void API_Global_BrowseProcessDocumentation() { abort(); }
void API_Global_Deallocate() { abort(); }
void API_Global_EnableAbort() { abort(); }
void API_Global_ErrorMessage() { abort(); }
void API_Global_GetConsole() { abort(); }
void API_Global_GetKeyboardModifiers() { abort(); }
void API_Global_GetProcessStatus() { abort(); }
void API_Global_LaunchProcessInstance4() { abort(); }
void API_Global_LaunchProcessInstanceOnView() { abort(); }
void API_Global_MessageBox() { abort(); }
void API_Global_ShowConsole() { abort(); }
void API_Global_WriteConsole() { abort(); }
void API_Global_WriteSettingsInteger() { abort(); }
void API_Graphics_EndPaint() { abort(); }
void API_Graphics_GetGraphicsStatus() { abort(); }
void API_ImageWindow_EnumerateImageWindows() { abort(); }
void API_ImageWindow_EnumeratePreviews() { abort(); }
void API_ImageWindow_GetActiveImageWindow() { abort(); }
void API_ImageWindow_GetImageWindowById() { abort(); }
void API_ImageWindow_GetImageWindowMainView() { abort(); }
void API_ImageWindow_GetPreviewById() { abort(); }
void API_ImageWindow_TerminateDynamicSession() { abort(); }
void API_InterfaceDefinition_BeginInterfaceDefinition() { abort(); }
void API_InterfaceDefinition_EndInterfaceDefinition() { abort(); }
void API_InterfaceDefinition_EnterInterfaceDefinitionContext() { abort(); }
void API_InterfaceDefinition_ExitInterfaceDefinitionContext() { abort(); }
void API_InterfaceDefinition_SetBeginReadoutNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetEndReadoutNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetGlobalCMDisabledNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetGlobalCMEnabledNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetGlobalCMUpdatedNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetGlobalFiltersUpdatedNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetGlobalPreferencesUpdatedNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetGlobalRGBWSUpdatedNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetImageCMDisabledNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetImageCMEnabledNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetImageCMUpdatedNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetImageCreatedNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetImageDeletedNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetImageFocusedNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetImageLockedNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetImageRenamedNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetImageRGBWSUpdatedNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetImageSavedNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetImageSTFDisabledNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetImageSTFEnabledNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetImageSTFUpdatedNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetImageUnlockedNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetImageUpdatedNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceAliasIdentifiers() { abort(); }
void API_InterfaceDefinition_SetInterfaceApplyGlobalRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceApplyRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceBrowseDocumentationRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceCancelRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceDescription() { abort(); }
void API_InterfaceDefinition_SetInterfaceDynamicKeyPressRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceDynamicKeyReleaseRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceDynamicModeEnterRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceDynamicModeExitRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceDynamicMouseDoubleClickRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceDynamicMouseEnterRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceDynamicMouseLeaveRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceDynamicMouseMoveRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceDynamicMousePressRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceDynamicMouseReleaseRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceDynamicMouseWheelRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceDynamicPaintRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceDynamicUpdateQueryRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceEditPreferencesRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceExecuteRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceFeatures() { abort(); }
void API_InterfaceDefinition_SetInterfaceIconImage() { abort(); }
void API_InterfaceDefinition_SetInterfaceIconImageFile() { abort(); }
void API_InterfaceDefinition_SetInterfaceIconSmallImage() { abort(); }
void API_InterfaceDefinition_SetInterfaceIconSmallImageFile() { abort(); }
void API_InterfaceDefinition_SetInterfaceIconSVG() { abort(); }
void API_InterfaceDefinition_SetInterfaceIconSVGFile() { abort(); }
void API_InterfaceDefinition_SetInterfaceInitializationRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceLaunchRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceProcessImportRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceProcessInstantiationRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceProcessTestInstantiationRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceProcessValidationRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceRealTimeCancelRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceRealTimeGenerationFlagsRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceRealTimeGenerationRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceRealTimePreviewUpdatedRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceRealTimeUpdateQueryRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceResetRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceTrackViewUpdatedRoutine() { abort(); }
void API_InterfaceDefinition_SetInterfaceVersion() { abort(); }
void API_InterfaceDefinition_SetMaskDisabledNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetMaskEnabledNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetMaskHiddenNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetMaskShownNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetMaskUpdatedNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetProcessCreatedNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetProcessDeletedNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetProcessSavedNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetProcessUpdatedNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetReadoutOptionsUpdatedNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetRealTimePreviewGenerationFinishNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetRealTimePreviewGenerationStartNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetRealTimePreviewLUTUpdatedNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetRealTimePreviewOwnerChangeNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetTransparencyHiddenNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetTransparencyModeUpdatedNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetUpdateReadoutNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetViewPropertyDeletedNotificationRoutine() { abort(); }
void API_InterfaceDefinition_SetViewPropertyUpdatedNotificationRoutine() { abort(); }
void API_Label_SetLabelAlignment() {  }
void API_ModuleDefinition_EnterModuleDefinitionContext() { abort(); }
void API_ModuleDefinition_ExitModuleDefinitionContext() { abort(); }
void API_ModuleDefinition_SetModuleAllocationRoutine() { abort(); }
void API_ModuleDefinition_SetModuleDeallocationRoutine() { abort(); }
void API_ModuleDefinition_SetModuleOnLoadRoutine() { abort(); }
void API_ModuleDefinition_SetModuleOnUnloadRoutine() { abort(); }
void API_Process_CloneProcessInstance() { abort(); }
void API_ProcessDefinition_BeginParameterDefinition() { abort(); }
void API_ProcessDefinition_BeginProcessDefinition() { abort(); }
void API_ProcessDefinition_BeginTableColumnDefinition() { abort(); }
void API_ProcessDefinition_DefineEnumerationAlias() { abort(); }
void API_ProcessDefinition_DefineEnumerationElement() { abort(); }
void API_ProcessDefinition_EndParameterDefinition() { abort(); }
void API_ProcessDefinition_EndProcessDefinition() { abort(); }
void API_ProcessDefinition_EndTableColumnDefinition() { abort(); }
void API_ProcessDefinition_EnterProcessDefinitionContext() { abort(); }
void API_ProcessDefinition_ExitProcessDefinitionContext() { abort(); }
void API_ProcessDefinition_SetDefaultBooleanValue() { abort(); }
void API_ProcessDefinition_SetDefaultEnumerationValueIndex() { abort(); }
void API_ProcessDefinition_SetDefaultNumericValue() { abort(); }
void API_ProcessDefinition_SetDefaultStringValue() { abort(); }
void API_ProcessDefinition_SetParameterAliasIdentifiers() { abort(); }
void API_ProcessDefinition_SetParameterAllocationRoutine() { abort(); }
void API_ProcessDefinition_SetParameterDescription() { abort(); }
void API_ProcessDefinition_SetParameterLengthQueryRoutine() { abort(); }
void API_ProcessDefinition_SetParameterLockRoutine() { abort(); }
void API_ProcessDefinition_SetParameterProcessVersionRange() { abort(); }
void API_ProcessDefinition_SetParameterReadOnly() { abort(); }
void API_ProcessDefinition_SetParameterRequired() { abort(); }
void API_ProcessDefinition_SetParameterScriptComment() { abort(); }
void API_ProcessDefinition_SetParameterUnlockRoutine() { abort(); }
void API_ProcessDefinition_SetParameterValidationRoutine() { abort(); }
void API_ProcessDefinition_SetPrecision() { abort(); }
void API_ProcessDefinition_SetProcessAliasIdentifiers() { abort(); }
void API_ProcessDefinition_SetProcessAssignmentRoutine() { abort(); }
void API_ProcessDefinition_SetProcessBrowseDocumentationRoutine() { abort(); }
void API_ProcessDefinition_SetProcessCategory() { abort(); }
void API_ProcessDefinition_SetProcessClassInitializationRoutine() { abort(); }
void API_ProcessDefinition_SetProcessClonationRoutine() { abort(); }
void API_ProcessDefinition_SetProcessCommandLineProcessingRoutine() { abort(); }
void API_ProcessDefinition_SetProcessCreationRoutine() { abort(); }
void API_ProcessDefinition_SetProcessDefaultInterfaceSelectionRoutine() { abort(); }
void API_ProcessDefinition_SetProcessDescription() { abort(); }
void API_ProcessDefinition_SetProcessDestructionRoutine() { abort(); }
void API_ProcessDefinition_SetProcessEditPreferencesRoutine() { abort(); }
void API_ProcessDefinition_SetProcessExecutionPreferencesRoutine() { abort(); }
void API_ProcessDefinition_SetProcessExecutionRoutine() { abort(); }
void API_ProcessDefinition_SetProcessExecutionValidationRoutine() { abort(); }
void API_ProcessDefinition_SetProcessGlobalExecutionRoutine() { abort(); }
void API_ProcessDefinition_SetProcessGlobalExecutionValidationRoutine() { abort(); }
void API_ProcessDefinition_SetProcessHistoryUpdateValidationRoutine() { abort(); }
void API_ProcessDefinition_SetProcessIconImage() { abort(); }
void API_ProcessDefinition_SetProcessIconImageFile() { abort(); }
void API_ProcessDefinition_SetProcessIconSmallImage() { abort(); }
void API_ProcessDefinition_SetProcessIconSmallImageFile() { abort(); }
void API_ProcessDefinition_SetProcessIconSVG() { abort(); }
void API_ProcessDefinition_SetProcessIconSVGFile() { abort(); }
void API_ProcessDefinition_SetProcessImageExecutionRoutine() { abort(); }
void API_ProcessDefinition_SetProcessImageExecutionValidationRoutine() { abort(); }
void API_ProcessDefinition_SetProcessInitializationRoutine() { abort(); }
void API_ProcessDefinition_SetProcessInterfaceSelectionRoutine() { abort(); }
void API_ProcessDefinition_SetProcessInterfaceValidationRoutine() { abort(); }
void API_ProcessDefinition_SetProcessIPCGetStatusRoutine() { abort(); }
void API_ProcessDefinition_SetProcessIPCSetParametersRoutine() { abort(); }
void API_ProcessDefinition_SetProcessIPCStartRoutine() { abort(); }
void API_ProcessDefinition_SetProcessIPCStopRoutine() { abort(); }
void API_ProcessDefinition_SetProcessMaskValidationRoutine() { abort(); }
void API_ProcessDefinition_SetProcessPostExecutionRoutine() { abort(); }
void API_ProcessDefinition_SetProcessPostGlobalExecutionRoutine() { abort(); }
void API_ProcessDefinition_SetProcessPostReadingRoutine() { abort(); }
void API_ProcessDefinition_SetProcessPostWritingRoutine() { abort(); }
void API_ProcessDefinition_SetProcessPreExecutionRoutine() { abort(); }
void API_ProcessDefinition_SetProcessPreGlobalExecutionRoutine() { abort(); }
void API_ProcessDefinition_SetProcessPreReadingRoutine() { abort(); }
void API_ProcessDefinition_SetProcessPreWritingRoutine() { abort(); }
void API_ProcessDefinition_SetProcessScriptComment() { abort(); }
void API_ProcessDefinition_SetProcessSetServerHandleRoutine() { abort(); }
void API_ProcessDefinition_SetProcessTestClonationRoutine() { abort(); }
void API_ProcessDefinition_SetProcessUndoModeRoutine() { abort(); }
void API_ProcessDefinition_SetProcessValidationRoutine() { abort(); }
void API_ProcessDefinition_SetProcessVersion() { abort(); }
void API_ProcessDefinition_SetScientificNotation() { abort(); }
void API_ProcessDefinition_SetStringAllowedCharacters() { abort(); }
void API_ProcessDefinition_SetStringLengthLimits() { abort(); }
void API_ProcessDefinition_SetValidNumericRange() { abort(); }
void API_SharedImage_AttachToImage() { abort(); }
void API_SharedImage_DetachFromImage() { abort(); }
void API_SharedImage_GetImageColorSpace() { abort(); }
void API_SharedImage_GetImageFormat() { abort(); }
void API_SharedImage_GetImageGeometry() { abort(); }
void API_SharedImage_GetImagePixelData() { abort(); }
void API_SharedImage_GetImageRGBWS() { abort(); }
void API_SharedImage_SetImageColorSpace() { abort(); }
void API_SharedImage_SetImageGeometry() { abort(); }
void API_SharedImage_SetImagePixelData() { abort(); }
void API_Slider_SetSliderPageSize() {  }
void API_Slider_SetSliderTickInterval() {  }
void API_Slider_SetSliderTickStyle() { }
void API_Thread_AppendThreadConsoleOutputText() { abort(); }
void API_Thread_GetCurrentThread() { abort(); }
void API_Thread_GetThreadStatusEx() { abort(); }
void API_UI_AttachToUIObject() { abort(); }
void API_UI_DetachFromUIObject() { abort(); }
void API_View_GetViewById() { abort(); }
void API_View_GetViewFullId() { abort(); }
void API_View_GetViewId() { abort(); }
void API_View_GetViewImage() { abort(); }
void API_View_GetViewLocks() { abort(); }
void API_View_LockView() { abort(); }
void API_View_UnlockView() { abort(); }
};
