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

static bool g_enableDebugLogging = false;

void SetDebugLogging( bool on )
{
   g_enableDebugLogging = on;
}

// =============================================================
// Mock Object: The only structure we need
// =============================================================
struct MockBase
{
    QWidget*     widget  = nullptr;   // QWidget* if control
    QBoxLayout*  layout  = nullptr;   // QBoxLayout* if sizer
    bool         isSizer = false;

    api_handle   moduleHandle = nullptr;
    api_handle   clientHandle = nullptr;
};

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

static std::unordered_map<void*, std::unique_ptr<MockBase>, HandleHash<control_handle>, HandleEqual<control_handle>> g_objects;
static MockBase* g_lastTopLevel = nullptr;

// =============================================================
// Utility: Lookup helper
// =============================================================
static inline MockBase* get(void* h)
{
    if (!h)
        return nullptr;
    auto it = g_objects.find(h);
    return (it == g_objects.end()) ? nullptr : it->second.get();
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
//  Generic Control Creation Helper
// =============================================================
template <typename W>
void* createControl(api_handle module, api_handle client, control_handle parent)
{
    QWidget* parentWidget = nullptr;
    if (auto* p = get(parent))
        parentWidget = p->widget;

    auto* b = new MockBase();
    b->moduleHandle = module;
    b->clientHandle = client;
    b->widget = new W(parentWidget);

    void* h = reinterpret_cast<void*>(b);
    g_objects[h] = std::unique_ptr<MockBase>(b);

    if (!parentWidget)
        g_lastTopLevel = b;

    logf("[Mock] CreateControl %s handle=%p widget=%p parent=%p",
         typeid(W).name(), h, b->widget, parentWidget);

    return h;
}

extern "C" {

control_handle API_Control_GetControlWindow(const_control_handle handle)
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
sizer_handle API_Sizer_CreateSizer(api_handle module, api_handle client, api_bool vertical)
{
    auto* b = new MockBase();
    b->isSizer = true;
    b->moduleHandle = module;
    b->clientHandle = client;

    b->layout = vertical
        ? static_cast<QBoxLayout*>(new QVBoxLayout())
        : static_cast<QBoxLayout*>(new QHBoxLayout());

    sizer_handle h = reinterpret_cast<sizer_handle>(b);
    g_objects[h] = std::unique_ptr<MockBase>(b);

    logf("[Mock] CreateSizer vertical=%d handle=%p", vertical, h);
    return h;
}

// =============================================================
//  Control Creation Wrappers
// =============================================================
control_handle API_Control_CreateControl(api_handle m)
{
    return reinterpret_cast<control_handle>(
        createControl<QWidget>(m, nullptr, nullptr));
}

label_handle API_Label_CreateLabel(api_handle m, api_handle c, control_handle parent)
{
    return reinterpret_cast<label_handle>(
        createControl<QLabel>(m, c, parent));
}

edit_handle API_Edit_CreateEdit(api_handle m, api_handle c, control_handle parent)
{
    return reinterpret_cast<edit_handle>(
        createControl<QLineEdit>(m, c, parent));
}

slider_handle API_Slider_CreateSlider(api_handle m, api_handle c, control_handle parent)
{
    return reinterpret_cast<slider_handle>(
        createControl<QSlider>(m, c, parent));
}

button_handle API_Button_CreateCheckBox(api_handle m, api_handle c, control_handle parent)
{
    return reinterpret_cast<button_handle>(
        createControl<QCheckBox>(m, c, parent));
}

combo_handle API_ComboBox_CreateComboBox(api_handle m, api_handle c, control_handle parent)
{
    return reinterpret_cast<combo_handle>(
        createControl<QComboBox>(m, c, parent));
}

spin_handle API_SpinBox_CreateSpinBox(api_handle m, api_handle c, control_handle parent)
{
    auto* b = new MockBase();
    b->moduleHandle = m;
    b->clientHandle = c;
    b->widget = new QSpinBox(get(parent) ? get(parent)->widget : nullptr);

    spin_handle h = reinterpret_cast<spin_handle>(b);
    g_objects[h] = std::unique_ptr<MockBase>(b);

    logf("[Mock] CreateSpinBox handle=%p widget=%p", h, b->widget);

    return h;
}

// =============================================================
//  Sizer Insertion
// =============================================================
api_bool API_Sizer_InsertSizerControl(sizer_handle s, api_handle, control_handle c, int index, int stretch, uint32 flags)
{
    MockBase* S = get(s);
    MockBase* C = get(c);
    if (!S || !S->isSizer || !C || !C->widget)
        return api_false;

    if (index < 0) index = S->layout->count();
    S->layout->insertWidget(index, C->widget, stretch);
    return api_true;
}

api_bool API_Sizer_InsertSizer(sizer_handle s, api_handle, sizer_handle child, int index, int stretch, uint32 flags)
{
    MockBase* S = get(s);
    MockBase* C = get(child);
    if (!S || !S->isSizer || !C || !C->isSizer)
        return api_false;

    QWidget* container = new QWidget();
    container->setLayout(C->layout);

    if (index < 0) index = S->layout->count();
    S->layout->insertWidget(index, container, stretch);
    return api_true;
}

// =============================================================
//  Attach Sizer to Control
// =============================================================
api_bool API_Control_SetControlSizer(control_handle h, api_handle, sizer_handle s)
{
    MockBase* C = get(h);
    MockBase* S = get(s);
    if (!C || !C->widget || !S || !S->isSizer)
        return api_false;

    C->widget->setLayout(S->layout);
    return api_true;
}

// =============================================================
//  Visibility
// =============================================================
api_bool API_Control_SetControlVisible(control_handle h, api_handle, uint32 flags)
{
    MockBase* C = nullptr;

    if (h)
        C = get(h);
    else
        C = g_lastTopLevel;

    if (!C || !C->widget)
        return api_false;

    C->widget->show();
    C->widget->raise();
    return api_true;
}

// =============================================================
//  Generic control property setters
// =============================================================
api_bool API_Control_SetControlFixedSize(control_handle h, api_handle, int w, int hgt)
{
    if (auto* C = get(h)) { if (C->widget) { C->widget->setFixedSize(w,hgt); return api_true; }}
    return api_false;
}

api_bool API_Control_SetControlMinSize(control_handle h, api_handle, int w, int hgt)
{
    if (auto* C = get(h)) { if (C->widget) { C->widget->setMinimumSize(w,hgt); return api_true; }}
    return api_false;
}

api_bool API_Control_SetControlBackgroundColor(control_handle h, api_handle, uint32)
{
    // Ignore for simplicity; implement if needed
    return api_true;
}

api_bool API_ComboBox_SetEditable(control_handle h, api_handle, api_bool editable)
{
    if (auto* C = get(h)) {
        if (auto* combo = qobject_cast<QComboBox*>(C->widget)) {
            combo->setEditable(editable);
            return api_true;
        }
    }
    return api_false;
}

// =============================================================
//  Edit / Slider / SpinBox Property Functions
// =============================================================
api_bool API_Slider_SetSliderValue(control_handle h, api_handle, int value)
{
    if (auto* C = get(h)) {
        if (auto* slider = qobject_cast<QSlider*>(C->widget)) {
            slider->setValue(value);
            return api_true;
        }
    }
    return api_false;
}

api_bool API_SpinBox_SetSpinBoxRange(control_handle h, api_handle, int minv, int maxv)
{
    if (auto* C = get(h)) {
        if (auto* spin = qobject_cast<QSpinBox*>(C->widget)) {
            spin->setRange(minv, maxv);
            return api_true;
        }
    }
    return api_false;
}

api_bool API_SpinBox_SetSpinBoxValue(control_handle h, api_handle, int value)
{
    if (auto* C = get(h)) {
        if (auto* spin = qobject_cast<QSpinBox*>(C->widget)) {
            spin->setValue(value);
            return api_true;
        }
    }
    return api_false;
}

// =============================================================
//  No-op stubs for unused API areas
// =============================================================
api_bool API_Control_SetChildControlToFocus(control_handle, api_handle, control_handle) { return api_true; }
api_bool API_Control_SetControlFocusStyle(control_handle, api_handle, uint32) { return api_true; }
api_bool API_Edit_SetEditCompletedEventRoutine(edit_handle, api_handle, api_handle, pcl::edit_event_routine) { return api_true; }
api_bool API_Edit_SetReturnPressedEventRoutine(edit_handle, api_handle, api_handle, pcl::edit_event_routine) { return api_true; }
api_bool API_Slider_SetSliderValueUpdatedEventRoutine(slider_handle, api_handle, api_handle, pcl::api_slider_value_event_routine) { return api_true; }
api_bool API_Button_SetButtonClickEventRoutine(button_handle, api_handle, api_handle, pcl::api_button_event_routine) { return api_true; }
api_bool API_SpinBox_SetValueUpdatedEventRoutine(spin_handle, api_handle, api_handle, pcl::api_spinbox_value_event_routine) { return api_true; }

api_bool API_ImageWindow_LoadImageWindows(const char16_t* url, const char* id, const char* hints, api_bool asACopy, api_bool allowMessages, pcl::window_enumeration_callback, void*)
{

  abort();
}

api_bool API_ImageWindow_CloseImageWindow(window_handle, api_bool force)
{

  abort();
}

window_handle API_ImageWindow_GetImageWindowById(const char*)
{

  abort();
}

window_handle API_ImageWindow_GetImageWindowByFilePath(const char16_t*)
{

  abort();
}

window_handle API_ImageWindow_GetActiveImageWindow()
{
    QWidget* w = QApplication::activeWindow();
    if (!w)
        return nullptr;

    return reinterpret_cast<window_handle>(w);
}
void API_ImageWindow_EnumerateImageWindows(pcl::window_enumeration_callback, void*, api_bool includeIconic)
{

  abort();
}
void API_ImageWindow_EnumeratePreviews(const_window_handle, pcl::view_enumeration_callback, void*)
{

  abort();
}
api_bool API_ImageWindow_GetImageWindowNewFlag(const_window_handle)
{

  abort();
}
api_bool API_ImageWindow_GetImageWindowCopyFlag(const_window_handle)
{

  abort();
}
api_bool API_ImageWindow_GetImageWindowFileURL(const_window_handle, char16_t*, size_type*)
{

  abort();
}
api_bool API_ImageWindow_GetImageWindowFilePath(const_window_handle, char16_t*, size_type*)
{

  abort();
}
api_bool API_ImageWindow_GetImageWindowFileInfo(const_window_handle, api_image_file_info*)
{

  abort();
}
size_type API_ImageWindow_GetImageWindowModifyCount(const_window_handle)
{

  abort();
}
view_handle API_ImageWindow_GetImageWindowMainView(const_window_handle)
{

  abort();
}
view_handle API_ImageWindow_GetImageWindowCurrentView(const_window_handle)
{

  abort();
}
void API_ImageWindow_SetImageWindowCurrentView(window_handle, view_handle)
{

  abort();
}
int32 API_ImageWindow_GetImageType(const_window_handle)
{

  abort();
}
api_bool API_ImageWindow_SetImageType(window_handle, int32 imageType, api_bool notify)
{

  abort();
}
void API_ImageWindow_PurgeImageWindowProperties(window_handle)
{

  abort();
}
api_bool API_ImageWindow_ValidateImageWindowView(const_window_handle, const_view_handle)
{

  abort();
}
int32 API_ImageWindow_GetPreviewCount(const_window_handle)
{

  abort();
}
view_handle API_ImageWindow_GetPreviewById(const_window_handle, const char*)
{

  abort();
}
view_handle API_ImageWindow_GetSelectedPreview(const_window_handle)
{

  abort();
}
void API_ImageWindow_SelectPreview(window_handle, view_handle)
{

  abort();
}
view_handle API_ImageWindow_CreatePreview(window_handle, int32, int32, int32, int32, const char*)
{

  abort();
}
void API_ImageWindow_ModifyPreview(window_handle, const char*, int32, int32, int32, int32, const char*)
{

  abort();
}
void API_ImageWindow_GetPreviewRect(const_window_handle, const char*, int32*, int32*, int32*, int32*)
{

  abort();
}
void API_ImageWindow_DeletePreview(window_handle, const char*)
{

  abort();
}
void API_ImageWindow_DeletePreviews(window_handle)
{

  abort();
}
window_handle API_ImageWindow_GetImageWindowMask(const_window_handle, api_bool* inverted)
{

  abort();
}
void API_ImageWindow_SetImageWindowMask(window_handle, window_handle, api_bool inverted)
{

  abort();
}
api_bool API_ImageWindow_GetImageWindowMaskEnabled(const_window_handle)
{

  abort();
}
void API_ImageWindow_SetImageWindowMaskEnabled(window_handle, api_bool)
{

  abort();
}
api_bool API_ImageWindow_GetImageWindowMaskVisible(const_window_handle)
{

  abort();
}
void API_ImageWindow_SetImageWindowMaskVisible(window_handle, api_bool)
{

  abort();
}
api_bool API_ImageWindow_ValidateImageWindowMask(const_window_handle, const_window_handle)
{

  abort();
}
int32 API_ImageWindow_GetMaskReferenceCount(const_window_handle)
{

  abort();
}
void API_ImageWindow_RemoveImageWindowMaskReferences(window_handle)
{

  abort();
}
void API_ImageWindow_UpdateImageWindowMaskReferences(window_handle)
{

  abort();
}
void API_ImageWindow_GetImageWindowSampleFormat(const_window_handle, uint32* nbits, api_bool* flt)
{

  abort();
}
void API_ImageWindow_SetImageWindowSampleFormat(window_handle, uint32 nbits, api_bool flt)
{

  abort();
}
void API_ImageWindow_GetImageWindowRGBWS(const_window_handle, api_RGBWS*)
{

  abort();
}
void API_ImageWindow_SetImageWindowRGBWS(window_handle, const api_RGBWS*)
{

  abort();
}
api_bool API_ImageWindow_GetImageWindowGlobalRGBWS(const_window_handle)
{

  abort();
}
void API_ImageWindow_SetImageWindowGlobalRGBWS(window_handle)
{

  abort();
}
void API_ImageWindow_GetGlobalRGBWS(api_RGBWS*)
{

  abort();
}
void API_ImageWindow_SetGlobalRGBWS(const api_RGBWS*)
{

  abort();
}
void API_ImageWindow_GetImageWindowCMEnabled(const_window_handle, api_bool* enableCM, api_bool* proofing, api_bool* gamutCheck)
{

  abort();
}
void API_ImageWindow_SetImageWindowCMEnabled(window_handle, api_bool enableCM, api_bool proofing, api_bool gamutCheck)
{

  abort();
}
uint32 API_ImageWindow_GetImageWindowICCProfileLength(const_window_handle)
{

  abort();
}
void API_ImageWindow_GetImageWindowICCProfile(const_window_handle, void*)
{

  abort();
}
void API_ImageWindow_SetImageWindowICCProfile(window_handle, const void*)
{

  abort();
}
void API_ImageWindow_LoadImageWindowICCProfile(window_handle, const char16_t*)
{

  abort();
}
void API_ImageWindow_DeleteImageWindowICCProfile(window_handle)
{

  abort();
}
int32 API_ImageWindow_GetImageWindowKeywordCount(const_window_handle)
{

  abort();
}
void API_ImageWindow_GetImageWindowKeyword(const_window_handle, int32, char*, size_type, char*, size_type, char*, size_type)
{

  abort();
}
void API_ImageWindow_AddImageWindowKeyword(window_handle, const char*, const char*, const char*)
{

  abort();
}
void API_ImageWindow_ResetImageWindowKeywords(window_handle)
{

  abort();
}
api_bool API_ImageWindow_GetImageWindowHasAstrometricSolution(const_window_handle)
{

  abort();
}
api_bool API_ImageWindow_RegenerateImageWindowAstrometricSolution(window_handle, api_bool, api_bool)
{

  abort();
}
api_bool API_ImageWindow_CopyImageWindowAstrometricSolution(window_handle, const_window_handle, api_bool)
{

  abort();
}
void API_ImageWindow_ClearImageWindowAstrometricSolution(window_handle, api_bool)
{

  abort();
}
void API_ImageWindow_UpdateImageWindowAstrometryMetadata(window_handle, api_bool)
{

  abort();
}
api_bool API_ImageWindow_ImageToCelestial(const_window_handle, double* x, double* y, api_bool rawRA)
{

  abort();
}
api_bool API_ImageWindow_CelestialToImage(const_window_handle, double* ra, double* dec)
{

  abort();
}
void API_ImageWindow_GetImageWindowResolution(const_window_handle, double*, double*, api_bool*)
{

  abort();
}
void API_ImageWindow_SetImageWindowResolution(window_handle, double, double, api_bool)
{

  abort();
}
void API_ImageWindow_GetDefaultResolution(double*, double*, api_bool*)
{

  abort();
}
void API_ImageWindow_GetDefaultICCProfileEmbedding(api_bool* rgb, api_bool* grayscale)
{

  abort();
}
api_bool API_ImageWindow_GetDefaultThumbnailEmbedding()
{

  abort();
}
api_bool API_ImageWindow_GetDefaultPropertiesEmbedding()
{

  abort();
}
api_bool API_ImageWindow_GetSwapDirectory(int32, char16_t*, size_type*)
{

  abort();
}
api_bool API_ImageWindow_SetSwapDirectories(const char16_t**, int32)
{

  abort();
}
int32 API_ImageWindow_GetCursorTolerance()
{

  abort();
}
int32 API_ImageWindow_GetImageWindowTransparencyMode(const_window_handle, uint32*)
{

  abort();
}
void API_ImageWindow_SetImageWindowTransparencyMode(window_handle, int32, uint32)
{

  abort();
}
int32 API_ImageWindow_GetTransparencyBackgroundBrush(uint32* fgColor, uint32* bgColor)
{

  abort();
}
void API_ImageWindow_SetTransparencyBackgroundBrush(int32 brush, uint32 fgColor, uint32 bgColor)
{

  abort();
}
int32 API_ImageWindow_GetImageWindowMode()
{

  abort();
}
void API_ImageWindow_SetImageWindowMode(int32)
{

  abort();
}
int32 API_ImageWindow_GetImageWindowDisplayChannel(const_window_handle)
{

  abort();
}
void API_ImageWindow_SetImageWindowDisplayChannel(window_handle, int32)
{

  abort();
}
int32 API_ImageWindow_GetImageWindowMaskMode(const_window_handle)
{

  abort();
}
void API_ImageWindow_SetImageWindowMaskMode(window_handle, int32)
{

  abort();
}
void API_ImageWindow_FitImageWindow(window_handle)
{

  abort();
}

void API_ImageWindow_ZoomImageWindowToFit(window_handle, api_bool, api_bool, api_bool, api_bool)
{

  //  abort();
}

int32 API_ImageWindow_GetImageWindowZoomFactor(const_window_handle)
{

  //  abort();
  return 1;
}
void API_ImageWindow_SetImageWindowZoomFactor(window_handle, int32)
{

  //  abort();
}
void API_ImageWindow_UpdateImageWindowViewport(window_handle)
{

  abort();
}
void API_ImageWindow_RegenerateImageWindowViewport(window_handle)
{

  abort();
}
void API_ImageWindow_SetImageWindowViewport(window_handle, double cx, double cy, int32 zoom)
{

  abort();
}
void API_ImageWindow_GetImageWindowViewportSize(const_window_handle, int32*, int32*)
{

  abort();
}
void API_ImageWindow_GetImageWindowViewportOrigin(const_window_handle, int32*, int32*)
{

  abort();
}
void API_ImageWindow_GetImageWindowViewportPosition(const_window_handle, int32*, int32*)
{

  abort();
}
void API_ImageWindow_SetImageWindowViewportPosition(window_handle, int32, int32)
{

  abort();
}
void API_ImageWindow_GetImageWindowVisibleViewportRect(const_window_handle, int32*, int32*, int32*, int32*)
{

  abort();
}
api_bool API_ImageWindow_GetImageWindowVisible(const_window_handle)
{

  abort();
}

void API_ImageWindow_SetImageWindowVisible( window_handle handle, api_bool visible )
{
    QWidget* w = reinterpret_cast<QWidget*>( handle );
    if ( !w )
        return;

    if ( visible )
        w->show();
    else
        w->hide();
}

api_bool API_ImageWindow_GetImageWindowIconic(const_window_handle)
{

  abort();
}
void API_ImageWindow_SetImageWindowIconic(window_handle, api_bool)
{

  abort();
}
void API_ImageWindow_BringImageWindowToFront(window_handle)
{

  abort();
}
void API_ImageWindow_SendImageWindowToBack(window_handle)
{

  abort();
}
interface_handle API_ImageWindow_GetActiveDynamicInterface()
{

  abort();
}
api_bool API_ImageWindow_TerminateDynamicSession(api_bool closeInterface)
{

  abort();
}
void API_ImageWindow_SetDynamicCursorXPM(window_handle, const char**, int32 hx, int32 hy)
{
  // ### deprecated
  abort();
}
   void           (API_ImageWindow_SetDynamicCursor)( window_handle, const_bitmap_handle, int32 hx, int32 hy )
   {

     abort();
   }

   bitmap_handle  (API_ImageWindow_GetDynamicCursorBitmap)( const_window_handle )
   {

     abort();
   }
   void           (API_ImageWindow_GetDynamicCursorHotSpot)( const_window_handle, int32* hx, int32* hy )
   {

     abort();
   }

   void           (API_ImageWindow_ViewportToImageArray)( const_window_handle, int32*, size_type n )
   {

     abort();
   }
   void           (API_ImageWindow_ViewportToImageArrayD)( const_window_handle, double*, size_type n )
   {

     abort();
   }

   void           (API_ImageWindow_ViewportToImage)( const_window_handle, int32* x, int32* y )
   {

     abort();
   }
   void           (API_ImageWindow_ViewportToImageD)( const_window_handle, double* x, double* y )
   {

     abort();
   }

   void           (API_ImageWindow_ImageToViewportArray)( const_window_handle, int32*, size_type n )
   {

     abort();
   }
   void           (API_ImageWindow_ImageToViewportArrayD)( const_window_handle, double*, size_type n )
   {

     abort();
   }

   void           (API_ImageWindow_ImageToViewport)( const_window_handle, int32* x, int32* y )
   {

     abort();
   }
   void           (API_ImageWindow_ImageToViewportD)( const_window_handle, double* x, double* y )
   {

     abort();
   }

   void           (API_ImageWindow_ViewportScalarToImageArray)( const_window_handle, int32*, size_type n )
   {

     abort();
   }
   void           (API_ImageWindow_ViewportScalarToImageArrayD)( const_window_handle, double*, size_type n )
   {

     abort();
   }

   void           (API_ImageWindow_ViewportScalarToImage)( const_window_handle, int32* )
   {

     abort();
   }
   void           (API_ImageWindow_ViewportScalarToImageD)( const_window_handle, double* )
   {

     abort();
   }

   void           (API_ImageWindow_ImageScalarToViewportArray)( const_window_handle, int32*, size_type n )
   {

     abort();
   }
   void           (API_ImageWindow_ImageScalarToViewportArrayD)( const_window_handle, double*, size_type n )
   {

     abort();
   }

   void           (API_ImageWindow_ImageScalarToViewport)( const_window_handle, int32* )
   {

     abort();
   }
   void           (API_ImageWindow_ImageScalarToViewportD)( const_window_handle, double* )
   {

     abort();
   }

   void           (API_ImageWindow_ViewportToGlobal)( const_window_handle, int32* x, int32* y )
   {

     abort();
   }
   void           (API_ImageWindow_GlobalToViewport)( const_window_handle, int32* x, int32* y )
   {

     abort();
   }

   void           (API_ImageWindow_UpdateViewportRect)( window_handle, int32, int32, int32, int32 )
   {

     abort();
   }
   void           (API_ImageWindow_UpdateImageRect)( window_handle, double, double, double, double )
   {

     abort();
   }

   void           (API_ImageWindow_RegenerateViewportRect)( window_handle, int32, int32, int32, int32 )
   {

     abort();
   }
   void           (API_ImageWindow_RegenerateImageRect)( window_handle, double, double, double, double )
   {

     abort();
   }

   void           (API_ImageWindow_CommitViewportUpdates)( window_handle )
   {

     abort();
   }

   api_bool       (API_ImageWindow_GetViewportUpdateRect)( const_window_handle, int32*, int32*, int32*, int32* )
   {

     abort();
   }

   void           (API_ImageWindow_BeginViewportSelection)( window_handle, int32 x, int32 y, uint32 flags )
   {

     abort();
   }
   void           (API_ImageWindow_ModifyViewportSelection)( window_handle, int32 x, int32 y, uint32 flags )
   {

     abort();
   }
   void           (API_ImageWindow_UpdateViewportSelection)( window_handle )
   {

     abort();
   }
   void           (API_ImageWindow_CancelViewportSelection)( window_handle )
   {

     abort();
   }
   void           (API_ImageWindow_EndViewportSelection)( window_handle )
   {

     abort();
   }
   api_bool       (API_ImageWindow_GetViewportSelection)( const_window_handle, int32* x0, int32* y0, int32* x1, int32* y1, uint32* flags )
   {

     abort();
   }

   bitmap_handle  (API_ImageWindow_GetViewportBitmap)( api_handle, const_window_handle, int32 x0, int32 y0, int32 x1, int32 y1, uint32 flags )
   {

     abort();
   }

   api_bool       (API_ImageWindow_GetImageWindowDisplayPixelRatio)( const_window_handle, double* )
   {

     abort();
   }
   api_bool       (API_ImageWindow_GetImageWindowResourcePixelRatio)( const_window_handle, double* )
   {

     abort();
   }
   api_bool       (API_ImageWindow_GetImageWindowDevicePixelRatio)( const_window_handle, double* )
   {

     abort();
   }

// ----------------------------------------------------------------------------
// ImageViewContext API
// ----------------------------------------------------------------------------

control_handle API_ImageView_CreateImageView(api_handle hModule, api_handle hClient, control_handle hParent, uint32 flags, int32 width, int32 height, int32 numberOfChannels, int32 bitsPerSample, api_bool floatSample, api_bool color)
{

  abort();
}
control_handle API_ImageView_CreateImageViewViewport(control_handle, api_handle hClient)
{

  abort();
}
image_handle API_ImageView_GetImageViewImage(const_control_handle)
{

  abort();
}
api_bool API_ImageView_IsImageViewColorImage(const_control_handle)
{

  abort();
}
api_bool API_ImageView_GetImageViewImageGeometry(const_control_handle hView, int32*, int32*, int32*)
{

  abort();
}
api_bool API_ImageView_GetImageViewSampleFormat(const_control_handle, int32* nbits, api_bool* flt)
{

  abort();
}
void API_ImageView_SetImageViewSampleFormat(control_handle, int32 nbits, api_bool flt)
{

  abort();
}
void API_ImageView_GetImageViewRGBWS(const_control_handle, api_RGBWS*)
{

  abort();
}
void API_ImageView_SetImageViewRGBWS(control_handle, const api_RGBWS*)
{

  abort();
}
void API_ImageView_GetImageViewCMEnabled(const_control_handle, api_bool* enableCM, api_bool* proofing, api_bool* gamutCheck)
{

  abort();
}
void API_ImageView_SetImageViewCMEnabled(control_handle, api_bool enableCM, api_bool proofing, api_bool gamutCheck)
{

  abort();
}
uint32 API_ImageView_GetImageViewICCProfileLength(const_control_handle)
{

  abort();
}
void API_ImageView_GetImageViewICCProfile(const_control_handle, void*)
{

  abort();
}
void API_ImageView_SetImageViewICCProfile(control_handle, const void*)
{

  abort();
}
void API_ImageView_LoadImageViewICCProfile(control_handle, const char16_t*)
{

  abort();
}
void API_ImageView_DeleteImageViewICCProfile(control_handle)
{

  abort();
}
api_bool API_ImageView_SetImageViewScrollEventRoutine(control_handle, api_handle, pcl::range_event_routine)
{

  abort();
}
int32 API_ImageView_GetImageViewMode(const_control_handle)
{

  abort();
}
void API_ImageView_SetImageViewMode(control_handle, int32)
{

  abort();
}
int32 API_ImageView_GetImageViewDisplayChannel(const_control_handle)
{

  abort();
}
void API_ImageView_SetImageViewDisplayChannel(control_handle, int32)
{

  abort();
}
int32 API_ImageView_GetImageViewZoomFactor(const_control_handle)
{

  abort();
}
void API_ImageView_SetImageViewZoomFactor(control_handle, int32)
{

  abort();
}
int32 API_ImageView_GetImageViewTransparencyMode(const_control_handle, uint32*)
{

  abort();
}
void API_ImageView_SetImageViewTransparencyMode(control_handle, int32, uint32)
{

  abort();
}
void API_ImageView_UpdateImageViewViewport(control_handle)
{

  abort();
}
void API_ImageView_RegenerateImageViewViewport(control_handle)
{

  abort();
}
void API_ImageView_SetImageViewViewport(control_handle, double cx, double cy, int32 zoom)
{

  abort();
}
void API_ImageView_GetImageViewViewportSize(const_control_handle, int32*, int32*)
{

  abort();
}
void API_ImageView_GetImageViewViewportOrigin(const_control_handle, int32*, int32*)
{

  abort();
}
void API_ImageView_GetImageViewViewportPosition(const_control_handle, int32*, int32*)
{

  abort();
}
void API_ImageView_SetImageViewViewportPosition(control_handle, int32, int32)
{

  abort();
}
void API_ImageView_GetImageViewVisibleViewportRect(const_control_handle, int32*, int32*, int32*, int32*)
{

  abort();
}
void API_ImageView_ViewportToImageArray(const_control_handle, int32*, size_type n)
{

  abort();
}
void API_ImageView_ViewportToImageArrayD(const_control_handle, double*, size_type n)
{

  abort();
}
void API_ImageView_ViewportToImage(const_control_handle, int32* x, int32* y)
{

  abort();
}
void API_ImageView_ViewportToImageD(const_control_handle, double* x, double* y)
{

  abort();
}
void API_ImageView_ImageToViewportArray(const_control_handle, int32*, size_type n)
{

  abort();
}
void API_ImageView_ImageToViewportArrayD(const_control_handle, double*, size_type n)
{

  abort();
}
void API_ImageView_ImageToViewport(const_control_handle, int32* x, int32* y)
{

  abort();
}
void API_ImageView_ImageToViewportD(const_control_handle, double* x, double* y)
{

  abort();
}
void API_ImageView_ViewportScalarToImageArray(const_control_handle, int32*, size_type n)
{

  abort();
}
void API_ImageView_ViewportScalarToImageArrayD(const_control_handle, double*, size_type n)
{

  abort();
}
void API_ImageView_ViewportScalarToImage(const_control_handle, int32*)
{

  abort();
}
void API_ImageView_ViewportScalarToImageD(const_control_handle, double*)
{

  abort();
}
void API_ImageView_ImageScalarToViewportArray(const_control_handle, int32*, size_type n)
{

  abort();
}
void API_ImageView_ImageScalarToViewportArrayD(const_control_handle, double*, size_type n)
{

  abort();
}
void API_ImageView_ImageScalarToViewport(const_control_handle, int32*)
{

  abort();
}
void API_ImageView_ImageScalarToViewportD(const_control_handle, double*)
{

  abort();
}
void API_ImageView_ViewportToGlobal(const_control_handle, int32* x, int32* y)
{

  abort();
}
void API_ImageView_GlobalToViewport(const_control_handle, int32* x, int32* y)
{

  abort();
}
void API_ImageView_UpdateViewportRect(control_handle, int32, int32, int32, int32)
{

  abort();
}
void API_ImageView_UpdateImageRect(control_handle, double, double, double, double)
{

  abort();
}
void API_ImageView_RegenerateViewportRect(control_handle, int32, int32, int32, int32)
{

  abort();
}
void API_ImageView_RegenerateImageRect(control_handle, double, double, double, double)
{

  abort();
}
void API_ImageView_CommitViewportUpdates(control_handle)
{

  abort();
}
api_bool API_ImageView_GetViewportUpdateRect(const_control_handle, int32*, int32*, int32*, int32*)
{

  abort();
}
void API_ImageView_BeginViewportSelection(control_handle, int32 x, int32 y, uint32 flags)
{

  abort();
}
void API_ImageView_ModifyViewportSelection(control_handle, int32 x, int32 y, uint32 flags)
{

  abort();
}
void API_ImageView_UpdateViewportSelection(control_handle)
{

  abort();
}
void API_ImageView_CancelViewportSelection(control_handle)
{

  abort();
}
void API_ImageView_EndViewportSelection(control_handle)
{

  abort();
}
api_bool API_ImageView_GetViewportSelection(const_control_handle, int32* x0, int32* y0, int32* x1, int32* y1, uint32* flags)
{

  abort();
}
bitmap_handle API_ImageView_GetViewportBitmap(api_handle, const_control_handle, int32 x0, int32 y0, int32 x1, int32 y1, uint32 flags)
{

  abort();
}

// ----------------------------------------------------------------------------
// CodeEditorContext API
// ----------------------------------------------------------------------------

control_handle API_CodeEditor_CreateCodeEditor(api_handle hModule, api_handle hClient, control_handle hParent, uint32 flags)
{

  abort();
}
control_handle API_CodeEditor_CreateEditorLineNumbersControl(control_handle, api_handle hClient, control_handle hParent, uint32 flags)
{

  abort();
}
api_bool API_CodeEditor_GetEditorFilePath(const_control_handle, char16_t*, size_type*)
{

  abort();
}
void API_CodeEditor_SetEditorFilePath(control_handle, const char16_t*)
{

  abort();
}
api_bool API_CodeEditor_GetEditorText(const_control_handle, char16_t*, size_type*)
{

  abort();
}
void API_CodeEditor_SetEditorText(control_handle, const char16_t*)
{

  abort();
}
api_bool API_CodeEditor_GetEditorEncodedText(const_control_handle, char*, size_type*, const char* encoding)
{

  abort();
}
api_bool API_CodeEditor_SetEditorEncodedText(control_handle, const char*, const char* encoding)
{

  abort();
}
void API_CodeEditor_ClearEditorText(control_handle)
{

  abort();
}
api_bool API_CodeEditor_GetEditorReadOnly(const_control_handle)
{

  abort();
}
void API_CodeEditor_SetEditorReadOnly(control_handle, api_bool)
{

  abort();
}
api_bool API_CodeEditor_SaveEditorText(control_handle, const char16_t* filePath, const char* encoding)
{

  abort();
}
api_bool API_CodeEditor_LoadEditorText(control_handle, const char16_t* filePath, const char* encoding)
{

  abort();
}
int32 API_CodeEditor_GetEditorLineCount(const_control_handle)
{

  abort();
}
int32 API_CodeEditor_GetEditorCharacterCount(const_control_handle)
{

  abort();
}
void API_CodeEditor_GetEditorCursorCoordinates(const_control_handle, int32* line, int32* col)
{

  abort();
}
void API_CodeEditor_SetEditorCursorCoordinates(control_handle, int32 line, int32 col)
{

  abort();
}
api_bool API_CodeEditor_GetEditorInsertMode(const_control_handle)
{

  abort();
}
void API_CodeEditor_SetEditorInsertMode(control_handle, api_bool)
{

  abort();
}
api_bool API_CodeEditor_GetEditorBlockSelectionMode(const_control_handle)
{

  abort();
}
void API_CodeEditor_SetEditorBlockSelectionMode(control_handle, api_bool)
{

  abort();
}
api_bool API_CodeEditor_GetEditorDynamicWordWrapMode(const_control_handle)
{

  abort();
}
void API_CodeEditor_SetEditorDynamicWordWrapMode(control_handle, api_bool)
{

  abort();
}
int32 API_CodeEditor_GetEditorUndoSteps(const_control_handle)
{

  abort();
}
int32 API_CodeEditor_GetEditorRedoSteps(const_control_handle)
{

  abort();
}
api_bool API_CodeEditor_GetEditorHasSelection(const_control_handle)
{

  abort();
}
void API_CodeEditor_GetEditorSelectionCoordinates(const_control_handle, int32* fromLine, int32* fromCol, int32* toLine, int32* toCol)
{

  abort();
}
void API_CodeEditor_SetEditorSelectionCoordinates(control_handle, int32 fromLine, int32 fromCol, int32 toLine, int32 toCol)
{

  abort();
}
api_bool API_CodeEditor_GetEditorSelectedText(const_control_handle, char16_t*, size_type*)
{

  abort();
}
void API_CodeEditor_InsertEditorText(control_handle, const char16_t*)
{

  abort();
}
void API_CodeEditor_EditorUndo(control_handle)
{

  abort();
}
void API_CodeEditor_EditorRedo(control_handle)
{

  abort();
}
void API_CodeEditor_EditorCut(control_handle)
{

  abort();
}
void API_CodeEditor_EditorCopy(control_handle)
{

  abort();
}
void API_CodeEditor_EditorPaste(control_handle)
{

  abort();
}
void API_CodeEditor_EditorDelete(control_handle)
{

  abort();
}
void API_CodeEditor_EditorSelectAll(control_handle)
{

  abort();
}
void API_CodeEditor_EditorUnselect(control_handle)
{

  abort();
}
api_bool API_CodeEditor_EditorGotoMatchedParenthesis(control_handle)
{

  abort();
}
int32 API_CodeEditor_EditorHighlightAllMatches(control_handle, const char16_t*, uint32 flags)
{

  abort();
}
void API_CodeEditor_EditorClearMatches(control_handle)
{

  abort();
}
api_bool API_CodeEditor_EditorFind(control_handle, const char16_t*, uint32 flags)
{

  abort();
}
api_bool API_CodeEditor_EditorReplace(control_handle, const char16_t*)
{

  abort();
}
int32 API_CodeEditor_EditorReplaceAll(control_handle, const char16_t*, const char16_t*, uint32 flags)
{

  abort();
}
api_bool API_CodeEditor_SetEditorTextUpdatedEventRoutine(control_handle, api_handle, pcl::event_routine)
{

  abort();
}
api_bool API_CodeEditor_SetEditorCursorPositionUpdatedEventRoutine(control_handle, api_handle, pcl::range_event_routine)
{

  abort();
}
api_bool API_CodeEditor_SetEditorSelectionUpdatedEventRoutine(control_handle, api_handle, pcl::rect_event_routine)
{

  abort();
}
api_bool API_CodeEditor_SetEditorOverwriteModeUpdatedEventRoutine(control_handle, api_handle, pcl::state_event_routine)
{

  abort();
}
api_bool API_CodeEditor_SetEditorSelectionModeUpdatedEventRoutine(control_handle, api_handle, pcl::state_event_routine)
{

  abort();
}
api_bool API_CodeEditor_SetEditorDynamicWordWrapModeUpdatedEventRoutine(control_handle, api_handle, pcl::state_event_routine)
{

  abort();
}

// ----------------------------------------------------------------------------
// WebViewContext API
// ----------------------------------------------------------------------------

control_handle API_WebView_CreateWebView(api_handle hModule, api_handle hClient, control_handle hParent, uint32 flags)
{

  abort();
}
api_bool API_WebView_SetWebViewContent(control_handle, const void* data, size_type size, const char* mimeType)
{

  abort();
}
api_bool API_WebView_LoadWebViewContent(control_handle, const char16_t* URI)
{

  abort();
}
api_bool API_WebView_RequestWebViewPlainText(const_control_handle)
{

  abort();
}
api_bool API_WebView_RequestWebViewHTML(const_control_handle)
{

  abort();
}
api_bool API_WebView_SaveWebViewAsPDF(control_handle, const char16_t* filePath, const double* pageWidth, const double* pageHeight, const double* marginLeft, const double* marginTop, const double* marginRight, const double* marginBottom, int32 orientation)
{

  abort();
}
api_bool API_WebView_GetWebViewHasSelection(const_control_handle)
{

  abort();
}
api_bool API_WebView_GetWebViewSelectedText(const_control_handle, char16_t*, size_type*)
{

  abort();
}
api_bool API_WebView_GetWebViewZoomFactor(const_control_handle, double*)
{

  abort();
}
api_bool API_WebView_SetWebViewZoomFactor(control_handle, const double*)
{

  abort();
}
uint32 API_WebView_GetWebViewBackgroundColor(const_control_handle)
{

  abort();
}
api_bool API_WebView_SetWebViewBackgroundColor(control_handle, uint32)
{

  abort();
}
api_bool API_WebView_ReloadWebView(control_handle)
{

  abort();
}
api_bool API_WebView_StopWebView(control_handle)
{

  abort();
}
api_bool API_WebView_EvaluateWebViewScript(control_handle, const char16_t* sourceCode, const char* language)
{

  abort();
}
api_bool API_WebView_SetWebViewLoadStartedEventRoutine(control_handle, api_handle, pcl::event_routine)
{

  abort();
}
api_bool API_WebView_SetWebViewLoadProgressEventRoutine(control_handle, api_handle, pcl::value_event_routine)
{

  abort();
}
api_bool API_WebView_SetWebViewLoadFinishedEventRoutine(control_handle, api_handle, pcl::state_event_routine)
{

  abort();
}
api_bool API_WebView_SetWebViewSelectionUpdatedEventRoutine(control_handle, api_handle, pcl::event_routine)
{

  abort();
}
api_bool API_WebView_SetWebViewPlainTextAvailableEventRoutine(control_handle, api_handle, pcl::unicode_event_routine)
{

  abort();
}
api_bool API_WebView_SetWebViewHTMLAvailableEventRoutine(control_handle, api_handle, pcl::unicode_event_routine)
{

  abort();
}
api_bool API_WebView_SetWebViewScriptResultAvailableEventRoutine(control_handle, api_handle, pcl::property_event_routine)
{

  abort();
}

// ----------------------------------------------------------------------------
// ExternalProcessContext API
// ----------------------------------------------------------------------------

   int32          (API_ExternalProcess_ExecuteProgram)( const char16_t* program, const char16_t** argv, size_type argc )
   {

     abort();
   }

   api_bool       (API_ExternalProcess_StartProgram)( const char16_t* program, const char16_t** argv, size_type argc,
                                            const char16_t* workingDirectory, uint64* pid )
   {

     abort();
   }

   external_process_handle (API_ExternalProcess_CreateExternalProcess)( api_handle hModule, api_handle hClient )
   {

     abort();
   }

   api_bool       (API_ExternalProcess_StartExternalProcess)( external_process_handle,
                                                    const char16_t* program, const char16_t** argv, size_type argc )
   {

     abort();
   }

   api_bool       (API_ExternalProcess_WaitForExternalProcessStarted)( external_process_handle, int32 ms )
   {

     abort();
   }
   api_bool       (API_ExternalProcess_WaitForExternalProcessFinished)( external_process_handle, int32 ms )
   {

     abort();
   }
   api_bool       (API_ExternalProcess_WaitForExternalProcessDataAvailable)( external_process_handle, int32 ms )
   {

     abort();
   }
   api_bool       (API_ExternalProcess_WaitForExternalProcessDataWritten)( external_process_handle, int32 ms )
   {

     abort();
   }

   api_bool       (API_ExternalProcess_TerminateExternalProcess)( external_process_handle )
   {

     abort();
   }
   api_bool       (API_ExternalProcess_KillExternalProcess)( external_process_handle )
   {

     abort();
   }

   api_bool       (API_ExternalProcess_CloseExternalProcessStream)( external_process_handle, int32 stream )
   {

     abort();
   }

   api_bool       (API_ExternalProcess_RedirectExternalProcessToFile)( external_process_handle, int32 stream, const char16_t* fileName, api_bool append )
   {

     abort();
   }
   api_bool       (API_ExternalProcess_PipeExternalProcess)( external_process_handle, int32 stream, external_process_handle toProcess )
   {

     abort();
   }

   api_bool       (API_ExternalProcess_GetExternalProcessWorkingDirectory)( const_external_process_handle, char16_t*, size_type* )
   {

     abort();
   }
   api_bool       (API_ExternalProcess_SetExternalProcessWorkingDirectory)( external_process_handle, const char16_t* )
   {

     abort();
   }

   api_bool       (API_ExternalProcess_GetExternalProcessIsRunning)( const_external_process_handle )
   {

     abort();
   }
   api_bool       (API_ExternalProcess_GetExternalProcessIsStarting)( const_external_process_handle )
   {

     abort();
   }

   uint64         (API_ExternalProcess_GetExternalProcessPID)( const_external_process_handle )
   {

     abort();
   }

   int32          (API_ExternalProcess_GetExternalProcessExitCode)( const_external_process_handle )
   {

     abort();
   }
   int32          (API_ExternalProcess_GetExternalProcessExitStatus)( const_external_process_handle )
   {

     abort();
   }
   int32          (API_ExternalProcess_GetExternalProcessErrorCode)( const_external_process_handle )
   {

     abort();
   }

   size_type      (API_ExternalProcess_GetExternalProcessBytesAvailable)( const_external_process_handle )
   {

     abort();
   }
   size_type      (API_ExternalProcess_GetExternalProcessBytesToWrite)( const_external_process_handle )
   {

     abort();
   }

   // ### The following function returns data allocated by the caller module.
   api_bool       (API_ExternalProcess_ReadFromExternalProcess)( api_handle hModule, external_process_handle, int32 stream, void**, size_type* )
   {

     abort();
   }
   api_bool       (API_ExternalProcess_WriteToExternalProcess)( external_process_handle, const void*, size_type count )
   {

     abort();
   }

   api_bool       (API_ExternalProcess_EnumerateExternalProcessEnvironment)( const_external_process_handle, pcl::environment_enumeration_callback, void* )
   {

     abort();
   }
   api_bool       (API_ExternalProcess_SetExternalProcessEnvironment)( external_process_handle, const char16_t** vars, size_type count )
   {

     abort();
   }

   api_bool       (API_ExternalProcess_SetExternalProcessStartedEventRoutine)( external_process_handle, api_handle, pcl::external_process_event_routine )
   {

     abort();
   }
   api_bool       (API_ExternalProcess_SetExternalProcessFinishedEventRoutine)( external_process_handle, api_handle, pcl::external_process_exit_status_event_routine )
   {

     abort();
   }
   api_bool       (API_ExternalProcess_SetExternalProcessStandardOutputDataAvailableEventRoutine)( external_process_handle, api_handle, pcl::external_process_event_routine )
   {

     abort();
   }
   api_bool       (API_ExternalProcess_SetExternalProcessStandardErrorDataAvailableEventRoutine)( external_process_handle, api_handle, pcl::external_process_event_routine )
   {

     abort();
   }
   api_bool       (API_ExternalProcess_SetExternalProcessErrorEventRoutine)( external_process_handle, api_handle, pcl::external_process_status_event_routine )
   {

     abort();
   }
  
/*  
   void        (API_ExternalProcess_EnterProcessDefinitionContext)()
   {

abort();
   }
   api_bool    (API_ExternalProcess_IsProcessDefinitionContextActive)()
   {

abort();
   }

   void        (API_ExternalProcess_BeginProcessDefinition)( meta_process_handle, const char* procId )
   {

abort();
   }
   api_bool    (API_ExternalProcess_GetProcessBeingDefined)( char*, size_type* )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessCategory)( const char* )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessVersion)( uint32 )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessAliasIdentifiers)( const char* )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessDescription)( const char16_t* )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessScriptComment)( const char16_t* )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessIconSVG)( const char* )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessIconSVGFile)( const char16_t* )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessIconImage)( const char** )
   {
   // ### deprecated
abort();
   }
   void        (API_ExternalProcess_SetProcessIconImageFile)( const char16_t* )
   {
   // ### deprecated
abort();
   }
   void        (API_ExternalProcess_SetProcessIconSmallImage)( const char** )
   {
   // ### deprecated
abort();
   }
   void        (API_ExternalProcess_SetProcessIconSmallImageFile)( const char16_t* )
   {
   // ### deprecated
abort();
   }

   void        (API_ExternalProcess_SetProcessClassInitializationRoutine)( pcl::process_class_initialization_routine )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessCreationRoutine)( pcl::process_creation_routine )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessDestructionRoutine)( pcl::process_destruction_routine )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessClonationRoutine)( pcl::process_clonation_routine)
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessTestClonationRoutine)( pcl::process_test_clonation_routine )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessSetServerHandleRoutine)( pcl::process_set_handle_routine )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessAssignmentRoutine)( pcl::process_assignment_routine )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessInitializationRoutine)( pcl::process_initialization_routine )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessValidationRoutine)( pcl::process_validation_routine )
   {

abort();
   }

   void        (API_ExternalProcess_SetProcessCommandLineProcessingRoutine)( pcl::process_command_line_processing_routine, uint32 flags )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessEditPreferencesRoutine)( pcl::process_edit_preferences_routine )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessBrowseDocumentationRoutine)( pcl::process_browse_documentation_routine )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessExecutionPreferencesRoutine)( pcl::process_execution_preferences_routine )
   {

abort();
   }

   void        (API_ExternalProcess_SetProcessExecutionValidationRoutine)( pcl::process_execution_validation_routine )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessMaskValidationRoutine)( pcl::process_mask_validation_routine )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessHistoryUpdateValidationRoutine)( pcl::process_history_update_validation_routine )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessUndoModeRoutine)( pcl::process_undo_mode_routine )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessPreExecutionRoutine)( pcl::process_pre_execution_routine )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessExecutionRoutine)( pcl::process_execution_routine )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessPostExecutionRoutine)( pcl::process_post_execution_routine )
   {

abort();
   }

   void        (API_ExternalProcess_SetProcessGlobalExecutionValidationRoutine)( pcl::process_global_execution_validation_routine )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessPreGlobalExecutionRoutine)( pcl::process_pre_global_execution_routine )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessGlobalExecutionRoutine)( pcl::process_global_execution_routine )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessPostGlobalExecutionRoutine)( pcl::process_post_global_execution_routine )
   {

abort();
   }

   void        (API_ExternalProcess_SetProcessImageExecutionValidationRoutine)( pcl::process_image_execution_validation_routine )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessImageExecutionRoutine)( pcl::process_image_execution_routine )
   {

abort();
   }

   void        (API_ExternalProcess_SetProcessDefaultInterfaceSelectionRoutine)( pcl::process_default_interface_selection_routine )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessInterfaceSelectionRoutine)( pcl::process_interface_selection_routine )
{

abort();
}
   void        (API_ExternalProcess_SetProcessInterfaceValidationRoutine)( pcl::process_interface_validation_routine )
{

abort();
}

   void        (API_ExternalProcess_SetProcessPreReadingRoutine)( pcl::process_pre_reading_routine )
{

abort();
}
   void        (API_ExternalProcess_SetProcessPostReadingRoutine)( pcl::process_post_reading_routine )
{

abort();
}
   void        (API_ExternalProcess_SetProcessPreWritingRoutine)( pcl::process_pre_writing_routine )
{

abort();
}
   void        (API_ExternalProcess_SetProcessPostWritingRoutine)( pcl::process_post_writing_routine )
{

abort();
}

   void        (API_ExternalProcess_SetProcessIPCStartRoutine)( pcl::process_ipc_notification_routine )
{

abort();
}
   void        (API_ExternalProcess_SetProcessIPCStopRoutine)( pcl::process_ipc_notification_routine )
{

abort();
}
   void        (API_ExternalProcess_SetProcessIPCSetParametersRoutine)( pcl::process_ipc_notification_routine )
{

abort();
}
   void        (API_ExternalProcess_SetProcessIPCGetStatusRoutine)( pcl::process_ipc_status_routine )
{

abort();
}
   void        (API_ExternalProcess_BeginParameterDefinition)( meta_parameter_handle, const char* parId, uint32 parType )
{

abort();
}
   api_bool    (API_ExternalProcess_GetParameterBeingDefined)( char*, size_type* )
{

abort();
}
   void        (API_ExternalProcess_SetParameterProcessVersionRange)( uint32, uint32 )
{

abort();
}
   void        (API_ExternalProcess_SetParameterRequired)( api_bool )
{

abort();
}
   void        (API_ExternalProcess_SetParameterReadOnly)( api_bool )
{

abort();
}
   void        (API_ExternalProcess_SetParameterAliasIdentifiers)( const char* )
{

abort();
}
   void        (API_ExternalProcess_SetParameterDescription)( const char16_t* )
{

abort();
}
   void        (API_ExternalProcess_SetParameterScriptComment)( const char16_t* )
{

abort();
}
   void        (API_ExternalProcess_SetParameterLockRoutine)( pcl::parameter_lock_routine )
{

abort();
}
   void        (API_ExternalProcess_SetParameterUnlockRoutine)( pcl::parameter_unlock_routine )
{

abort();
}
   void        (API_ExternalProcess_SetParameterValidationRoutine)( pcl::parameter_validation_routine )
{

abort();
}
   void        (API_ExternalProcess_SetParameterAllocationRoutine)( pcl::parameter_allocation_routine )
{

abort();
}
   void        (API_ExternalProcess_SetParameterLengthQueryRoutine)( pcl::parameter_length_query_routine )
{

abort();
}
   void        (API_ExternalProcess_SetDefaultNumericValue)( double )
{

abort();
}
   void        (API_ExternalProcess_SetValidNumericRange)( double, double )
{

abort();
}
   void        (API_ExternalProcess_SetPrecision)( int32 )
   {

abort();
   }
   void        (API_ExternalProcess_SetScientificNotation)( api_bool )
{

abort();
}
   void        (API_ExternalProcess_SetDefaultBooleanValue)( api_bool )
{

abort();
}
   void        (API_ExternalProcess_DefineEnumerationElement)( const char*, api_enum )
{

abort();
}
   void        (API_ExternalProcess_DefineEnumerationAlias)( const char*, const char* )
{

abort();
}
   void        (API_ExternalProcess_SetDefaultEnumerationValueIndex)( uint32 )
{

abort();
}
   void        (API_ExternalProcess_SetDefaultStringValue)( const char16_t* )
{

abort();
}
   void        (API_ExternalProcess_SetStringAllowedCharacters)( const char16_t* )
{

abort();
}
   void        (API_ExternalProcess_SetStringLengthLimits)( size_type, size_type )
{

abort();
}
   void        (API_ExternalProcess_BeginTableColumnDefinition)( meta_parameter_handle, const char* colId, uint32 colType )
{

abort();
}
   void        (API_ExternalProcess_EndTableColumnDefinition)()
{

abort();
}
   void        (API_ExternalProcess_SetTableRowLimits)( size_type, size_type )
{

abort();
}
   void        (API_ExternalProcess_SetBlockSizeLimits)( size_type, size_type )
{

abort();
}

   void        (API_ExternalProcess_EndParameterDefinition)()
{

abort();
}
   void        (API_ExternalProcess_EndProcessDefinition)()
{

abort();
}
   void        (API_ExternalProcess_ExitProcessDefinitionContext)()
{

abort();
}
*/
  
// ----------------------------------------------------------------------------
// NetworkTransferContext API
// ----------------------------------------------------------------------------

network_transfer_handle API_NetworkTransfer_CreateNetworkTransfer(api_handle hModule, api_handle hClient)
{

  abort();
}
api_bool API_NetworkTransfer_SetNetworkTransferURL(network_transfer_handle, const char16_t* url, const char16_t* userName, const char16_t* userPassword)
{

  abort();
}
api_bool API_NetworkTransfer_SetNetworkTransferProxyURL(network_transfer_handle, const char16_t* proxy, const char16_t* userName, const char16_t* userPassword)
{

  abort();
}
api_bool API_NetworkTransfer_SetNetworkTransferSSL(network_transfer_handle, api_bool useSSL, api_bool forceSSL, api_bool verifyPeer, api_bool verifyHost)
{

  abort();
}
api_bool API_NetworkTransfer_SetNetworkTransferCustomHTTPHeaders(network_transfer_handle, const char16_t* nlsHeaders)
{

  abort();
}
api_bool API_NetworkTransfer_SetNetworkTransferConnectionTimeout(network_transfer_handle, int32 seconds)
{

  abort();
}
api_bool API_NetworkTransfer_PerformNetworkTransferDownload(network_transfer_handle)
{

  abort();
}
api_bool API_NetworkTransfer_PerformNetworkTransferUpload(network_transfer_handle, fsize_type uploadSize)
{

  abort();
}
api_bool API_NetworkTransfer_PerformNetworkTransferPOST(network_transfer_handle, const char16_t* postFields)
{

  abort();
}
api_bool API_NetworkTransfer_PerformNetworkTransferSMTP(network_transfer_handle, const char16_t* mailFrom, const char16_t* mailRecipients)
{

  abort();
}
void API_NetworkTransfer_CloseNetworkTransferConnection(network_transfer_handle)
{

  abort();
}
api_bool API_NetworkTransfer_GetNetworkTransferURL(const_network_transfer_handle, char16_t*, size_type*)
{

  abort();
}
api_bool API_NetworkTransfer_GetNetworkTransferProxyURL(const_network_transfer_handle, char16_t*, size_type*)
{

  abort();
}
api_bool API_NetworkTransfer_GetNetworkTransferCustomHTTPHeaders(const_network_transfer_handle, char16_t*, size_type*)
{

  abort();
}
api_bool API_NetworkTransfer_GetNetworkTransferStatus(const_network_transfer_handle)
{

  abort();
}
api_bool API_NetworkTransfer_GetNetworkTransferIsAborted(const_network_transfer_handle)
{

  abort();
}
int32 API_NetworkTransfer_GetNetworkTransferResponseCode(const_network_transfer_handle)
{

  abort();
}
api_bool API_NetworkTransfer_GetNetworkTransferContentType(const_network_transfer_handle, char16_t*, size_type*)
{

  abort();
}
fsize_type API_NetworkTransfer_GetNetworkTransferBytesTransferred(const_network_transfer_handle)
{

  abort();
}
void API_NetworkTransfer_GetNetworkTransferTotalSpeed(const_network_transfer_handle, double*)
{
  // in KiB/s
  abort();
}
   void           (API_NetworkTransfer_GetNetworkTransferTotalTime)( const_network_transfer_handle, double* )
   {
     // in s
     abort();
   }
   api_bool       (API_NetworkTransfer_GetNetworkTransferErrorInformation)( const_network_transfer_handle, char16_t*, size_type* )
   {

     abort();
   }

   api_bool       (API_NetworkTransfer_SetNetworkTransferDownloadEventRoutine)( network_transfer_handle, api_handle, pcl::network_download_event_routine )
   {

     abort();
   }
   api_bool       (API_NetworkTransfer_SetNetworkTransferUploadEventRoutine)( network_transfer_handle, api_handle, pcl::network_upload_event_routine )
   {

     abort();
   }
   api_bool       (API_NetworkTransfer_SetNetworkTransferProgressEventRoutine)( network_transfer_handle, api_handle, pcl::network_progress_event_routine )
   {

     abort();
   }

void API_Control_AdjustControlToContents(control_handle)
{

  //  abort();
}

void API_Control_GetControlExpansionEnabled(const_control_handle, api_bool*, api_bool*)
{

  abort();
}
void API_Control_SetControlExpansionEnabled(control_handle, api_bool, api_bool)
{

  abort();
}

api_bool API_Control_GetControlUnderMouseStatus(const_control_handle)
{

  abort();
}
void API_Control_BringControlToFront(control_handle)
{

  abort();
}
void API_Control_SendControlToBack(control_handle)
{

  abort();
}
void API_Control_StackControls(control_handle stackThis, control_handle underThis)
{

  abort();
}
sizer_handle API_Control_GetControlSizer(const_control_handle)
{

  abort();
}

void API_Control_GlobalToLocal(const_control_handle, int32*, int32*)
{

  abort();
}
void API_Control_LocalToGlobal(const_control_handle, int32*, int32*)
{

  abort();
}
void API_Control_ParentToLocal(const_control_handle, int32*, int32*)
{

  abort();
}
void API_Control_LocalToParent(const_control_handle, int32*, int32*)
{

  abort();
}
void API_Control_ControlToLocal(const_control_handle, const_control_handle, int32*, int32*)
{

  abort();
}
void API_Control_LocalToControl(const_control_handle, const_control_handle, int32*, int32*)
{

  abort();
}
control_handle API_Control_GetChildByPos(const_control_handle, int32, int32)
{
  // returns client handle
  abort();
}

   void           (API_Control_GetChildrenRect)( const_control_handle, int32*, int32*, int32*, int32* )
   {

     abort();
   }

   api_bool       (API_Control_GetControlAncestry)( const_control_handle, const_control_handle )
   {

     abort();
   }


// ----------------------------------------------------------------------------
// GraphicsContext API
// ----------------------------------------------------------------------------

graphics_handle API_Graphics_CreateGraphics(api_handle)
{

  abort();
}
api_bool API_Graphics_BeginControlPaint(graphics_handle, control_handle)
{

  abort();
}
api_bool API_Graphics_BeginBitmapPaint(graphics_handle, bitmap_handle)
{

  abort();
}
api_bool API_Graphics_BeginSVGPaint(graphics_handle, svg_handle)
{

  abort();
}
void API_Graphics_EndPaint(graphics_handle)
{

  abort();
}
api_bool API_Graphics_GetGraphicsStatus(const_graphics_handle)
{

  abort();
}
api_bool API_Graphics_GetGraphicsTransformationEnabled(const_graphics_handle)
{

  abort();
}
void API_Graphics_EnableGraphicsTransformation(graphics_handle, api_bool)
{

  abort();
}
void API_Graphics_GetGraphicsTransformationMatrix(const_graphics_handle, double* m11, double* m12, double* m13, double* m21, double* m22, double* m23, double* m31, double* m32, double* m33)
{

  abort();
}
void API_Graphics_SetGraphicsTransformationMatrix(graphics_handle, double m11, double m12, double m13, double m21, double m22, double m23, double m31, double m32, double m33)
{

  abort();
}
void API_Graphics_MultiplyGraphicsTransformationMatrix(graphics_handle, double m11, double m12, double m13, double m21, double m22, double m23, double m31, double m32, double m33)
{

  abort();
}
void API_Graphics_RotateGraphicsTransformation(graphics_handle, double)
{

  abort();
}
void API_Graphics_ScaleGraphicsTransformation(graphics_handle, double, double)
{

  abort();
}
void API_Graphics_TranslateGraphicsTransformation(graphics_handle, double, double)
{

  abort();
}
void API_Graphics_ShearGraphicsTransformation(graphics_handle, double, double)
{

  abort();
}
void API_Graphics_ResetGraphicsTransformation(graphics_handle)
{

  abort();
}
void API_Graphics_TransformPoints(const_graphics_handle, double* xy, size_type n)
{

  abort();
}
api_bool API_Graphics_GetGraphicsClippingEnabled(const_graphics_handle)
{

  abort();
}
void API_Graphics_EnableGraphicsClipping(graphics_handle, api_bool)
{

  abort();
}
void API_Graphics_GetGraphicsClipRect(const_graphics_handle, int32*, int32*, int32*, int32*)
{

  abort();
}
void API_Graphics_SetGraphicsClipRect(graphics_handle, int32, int32, int32, int32)
{

  abort();
}
void API_Graphics_GetGraphicsClipRectD(const_graphics_handle, double*, double*, double*, double*)
{

  abort();
}
void API_Graphics_SetGraphicsClipRectD(graphics_handle, double, double, double, double)
{

  abort();
}
api_bool API_Graphics_GetGraphicsAntialiasingEnabled(const_graphics_handle)
{

  abort();
}
void API_Graphics_EnableGraphicsAntialiasing(graphics_handle, api_bool)
{

  abort();
}
api_bool API_Graphics_GetGraphicsTextAntialiasingEnabled(const_graphics_handle)
{

  abort();
}
void API_Graphics_EnableGraphicsTextAntialiasing(graphics_handle, api_bool)
{

  abort();
}
api_bool API_Graphics_GetGraphicsSmoothInterpolationEnabled(const_graphics_handle)
{

  abort();
}
void API_Graphics_EnableGraphicsSmoothInterpolation(graphics_handle, api_bool)
{

  abort();
}
int32 API_Graphics_GetGraphicsCompositionOperator(const_graphics_handle)
{

  abort();
}
void API_Graphics_SetGraphicsCompositionOperator(graphics_handle, int32)
{

  abort();
}
void API_Graphics_GetGraphicsOpacity(const_graphics_handle, double*)
{

  abort();
}
void API_Graphics_SetGraphicsOpacity(graphics_handle, double)
{

  abort();
}
brush_handle API_Graphics_GetGraphicsBackgroundBrush(const_graphics_handle)
{

  abort();
}
void API_Graphics_SetGraphicsBackgroundBrush(graphics_handle, const_brush_handle)
{

  abort();
}
api_bool API_Graphics_GetGraphicsTransparentBackgroundEnabled(const_graphics_handle)
{

  abort();
}
void API_Graphics_SetGraphicsTransparentBackground(graphics_handle, api_bool)
{

  abort();
}
pen_handle API_Graphics_GetGraphicsPen(const_graphics_handle)
{

  abort();
}
void API_Graphics_SetGraphicsPen(graphics_handle, const_pen_handle)
{

  abort();
}
brush_handle API_Graphics_GetGraphicsBrush(const_graphics_handle)
{

  abort();
}
void API_Graphics_SetGraphicsBrush(graphics_handle, const_brush_handle)
{

  abort();
}
void API_Graphics_GetGraphicsBrushOrigin(const_graphics_handle, int32*, int32*)
{

  abort();
}
void API_Graphics_SetGraphicsBrushOrigin(graphics_handle, int32, int32)
{

  abort();
}
void API_Graphics_GetGraphicsBrushOriginD(const_graphics_handle, double*, double*)
{

  abort();
}
void API_Graphics_SetGraphicsBrushOriginD(graphics_handle, double, double)
{

  abort();
}
font_handle API_Graphics_GetGraphicsFont(const_graphics_handle)
{

  abort();
}
void API_Graphics_SetGraphicsFont(graphics_handle, const_font_handle)
{

  abort();
}
void API_Graphics_PushGraphicsState(graphics_handle)
{

  abort();
}
void API_Graphics_PopGraphicsState(graphics_handle)
{

  abort();
}
void API_Graphics_DrawPoint(graphics_handle, int32, int32)
{

  abort();
}
void API_Graphics_DrawPointD(graphics_handle, double, double)
{

  abort();
}
void API_Graphics_DrawLine(graphics_handle, int32, int32, int32, int32)
{

  abort();
}
void API_Graphics_DrawLineD(graphics_handle, double, double, double, double)
{

  abort();
}
void API_Graphics_DrawRect(graphics_handle, int32, int32, int32, int32)
{

  abort();
}
void API_Graphics_StrokeRect(graphics_handle, int32, int32, int32, int32, const_pen_handle)
{

  abort();
}
void API_Graphics_FillRect(graphics_handle, int32, int32, int32, int32, const_brush_handle)
{

  abort();
}
void API_Graphics_DrawRectD(graphics_handle, double, double, double, double)
{

  abort();
}
void API_Graphics_StrokeRectD(graphics_handle, double, double, double, double, const_pen_handle)
{

  abort();
}
void API_Graphics_FillRectD(graphics_handle, double, double, double, double, const_brush_handle)
{

  abort();
}
void API_Graphics_DrawRoundedRect(graphics_handle, int32, int32, int32, int32, double, double)
{

  abort();
}
void API_Graphics_StrokeRoundedRect(graphics_handle, int32, int32, int32, int32, double, double, const_pen_handle)
{

  abort();
}
void API_Graphics_FillRoundedRect(graphics_handle, int32, int32, int32, int32, double, double, const_brush_handle)
{

  abort();
}
void API_Graphics_DrawRoundedRectD(graphics_handle, double, double, double, double, double, double)
{

  abort();
}
void API_Graphics_StrokeRoundedRectD(graphics_handle, double, double, double, double, double, double, const_pen_handle)
{

  abort();
}
void API_Graphics_FillRoundedRectD(graphics_handle, double, double, double, double, double, double, const_brush_handle)
{

  abort();
}
void API_Graphics_DrawEllipse(graphics_handle, int32, int32, int32, int32)
{

  abort();
}
void API_Graphics_StrokeEllipse(graphics_handle, int32, int32, int32, int32, const_pen_handle)
{

  abort();
}
void API_Graphics_FillEllipse(graphics_handle, int32, int32, int32, int32, const_brush_handle)
{

  abort();
}
void API_Graphics_DrawEllipseD(graphics_handle, double, double, double, double)
{

  abort();
}
void API_Graphics_StrokeEllipseD(graphics_handle, double, double, double, double, const_pen_handle)
{

  abort();
}
void API_Graphics_FillEllipseD(graphics_handle, double, double, double, double, const_brush_handle)
{

  abort();
}
void API_Graphics_DrawPolygon(graphics_handle, const int32*, size_type, int32)
{

  abort();
}
void API_Graphics_StrokePolygon(graphics_handle, const int32*, size_type, int32, const_pen_handle)
{

  abort();
}
void API_Graphics_FillPolygon(graphics_handle, const int32*, size_type, int32, const_brush_handle)
{

  abort();
}
void API_Graphics_DrawPolygonD(graphics_handle, const double*, size_type, int32)
{

  abort();
}
void API_Graphics_StrokePolygonD(graphics_handle, const double*, size_type, int32, const_pen_handle)
{

  abort();
}
void API_Graphics_FillPolygonD(graphics_handle, const double*, size_type, int32, const_brush_handle)
{

  abort();
}
void API_Graphics_DrawPolyline(graphics_handle, const int32*, size_type)
{

  abort();
}
void API_Graphics_DrawPolylineD(graphics_handle, const double*, size_type)
{

  abort();
}
void API_Graphics_DrawArc(graphics_handle, int32, int32, int32, int32, double, double)
{

  abort();
}
void API_Graphics_DrawArcD(graphics_handle, double, double, double, double, double, double)
{

  abort();
}
void API_Graphics_DrawChord(graphics_handle, int32, int32, int32, int32, double, double)
{

  abort();
}
void API_Graphics_StrokeChord(graphics_handle, int32, int32, int32, int32, double, double, const_pen_handle)
{

  abort();
}
void API_Graphics_FillChord(graphics_handle, int32, int32, int32, int32, double, double, const_brush_handle)
{

  abort();
}
void API_Graphics_DrawChordD(graphics_handle, double, double, double, double, double, double)
{

  abort();
}
void API_Graphics_StrokeChordD(graphics_handle, double, double, double, double, double, double, const_pen_handle)
{

  abort();
}
void API_Graphics_FillChordD(graphics_handle, double, double, double, double, double, double, const_brush_handle)
{

  abort();
}
void API_Graphics_DrawPie(graphics_handle, int32, int32, int32, int32, double, double)
{

  abort();
}
void API_Graphics_StrokePie(graphics_handle, int32, int32, int32, int32, double, double, const_pen_handle)
{

  abort();
}
void API_Graphics_FillPie(graphics_handle, int32, int32, int32, int32, double, double, const_brush_handle)
{

  abort();
}
void API_Graphics_DrawPieD(graphics_handle, double, double, double, double, double, double)
{

  abort();
}
void API_Graphics_StrokePieD(graphics_handle, double, double, double, double, double, double, const_pen_handle)
{

  abort();
}
void API_Graphics_FillPieD(graphics_handle, double, double, double, double, double, double, const_brush_handle)
{

  abort();
}
void API_Graphics_DrawBitmap(graphics_handle, int32, int32, const_bitmap_handle)
{

  abort();
}
void API_Graphics_DrawBitmapD(graphics_handle, double, double, const_bitmap_handle)
{

  abort();
}
void API_Graphics_DrawBitmapRect(graphics_handle, int32, int32, const_bitmap_handle, int32, int32, int32, int32)
{

  abort();
}
void API_Graphics_DrawBitmapRectD(graphics_handle, double, double, const_bitmap_handle, double, double, double, double)
{

  abort();
}
void API_Graphics_DrawScaledBitmap(graphics_handle, int32, int32, int32, int32, const_bitmap_handle)
{

  abort();
}
void API_Graphics_DrawScaledBitmapD(graphics_handle, double, double, double, double, const_bitmap_handle)
{

  abort();
}
void API_Graphics_DrawScaledBitmapRect(graphics_handle, int32, int32, int32, int32, const_bitmap_handle, int32, int32, int32, int32)
{

  abort();
}
void API_Graphics_DrawScaledBitmapRectD(graphics_handle, double, double, double, double, const_bitmap_handle, double, double, double, double)
{

  abort();
}
void API_Graphics_DrawTiledBitmap(graphics_handle, int32, int32, int32, int32, const_bitmap_handle, int32, int32)
{

  abort();
}
void API_Graphics_DrawTiledBitmapD(graphics_handle, double, double, double, double, const_bitmap_handle, double, double)
{

  abort();
}
void API_Graphics_DrawText(graphics_handle, int32, int32, const char16_type*)
{

  abort();
}
void API_Graphics_DrawTextD(graphics_handle, double, double, const char16_type*)
{

  abort();
}
void API_Graphics_DrawTextRect(graphics_handle, int32, int32, int32, int32, const char16_type*, int32)
{

  abort();
}
void API_Graphics_DrawTextRectD(graphics_handle, double, double, double, double, const char16_type*, int32)
{

  abort();
}
void API_Graphics_GetTextRect(graphics_handle, int32, int32, int32, int32, const char16_type*, int32, int32*, int32*, int32*, int32*)
{

  abort();
}
void API_Graphics_GetTextRectD(graphics_handle, double, double, double, double, const char16_type*, int32, double*, double*, double*, double*)
{

  abort();
}

// ----------------------------------------------------------------------------
// RealTimePreviewContext API
// ----------------------------------------------------------------------------

api_bool API_RealTimePreview_SetRealTimePreviewOwner(interface_handle, uint32 flags)
{

  abort();
}
api_bool API_RealTimePreview_IsRealTimePreviewUpdating()
{

  abort();
}
void API_RealTimePreview_UpdateRealTimePreview()
{

  abort();
}
void API_RealTimePreview_ShowRealTimePreviewProgressDialog(const char16_type* title, const char16_type* text, size_type total, uint32 flags)
{

  abort();
}
void API_RealTimePreview_CloseRealTimePreviewProgressDialog()
{

  abort();
}
api_bool API_RealTimePreview_IsRealTimePreviewProgressDialogVisible()
{

  abort();
}
void API_RealTimePreview_SetRealTimePreviewProgressCount(size_type newCount, uint32 flags)
{

  abort();
}
void API_RealTimePreview_SetRealTimePreviewProgressText(const char16_type* text, uint32 flags)
{

  abort();
}

// ----------------------------------------------------------------------------
// NumericalContext API
// ----------------------------------------------------------------------------

api_bool API_Numerical_GaussJordanInPlaceF(float** A, float** B, int32 rows, int32 cols)
{

  abort();
}
api_bool API_Numerical_GaussJordanInPlaceD(double** A, double** B, int32 rows, int32 cols)
{

  abort();
}
api_bool API_Numerical_SVDInPlaceF(float** A, float* W, float** V, int32 rows, int32 cols)
{

  abort();
}
api_bool API_Numerical_SVDInPlaceD(double** A, double* W, double** V, int32 rows, int32 cols)
{

  abort();
}
api_enum API_Numerical_LinearFitF(double* a, double* b, double* adev, const float* fx, const float* fy, size_type n, api_bool (*callback)( void* ), void*)
{

  abort();
}
api_enum API_Numerical_LinearFitD(double* a, double* b, double* adev, const double* fx, const double* fy, size_type n, api_bool (*callback)( void* ), void*)
{

  abort();
}
api_bool API_Numerical_CubicSplineGenerateF(float* dy2, const float* fx, const float* fy, float dy1, float dyn, int32 n)
{

  abort();
}
api_bool API_Numerical_CubicSplineGenerateD(double* dy2, const double* fx, const double* fy, double dy1, double dyn, int32 n)
{

  abort();
}
api_bool API_Numerical_NaturalCubicSplineGenerateF(float* dy2, const float* fx, const float* fy, int32 n)
{

  abort();
}
api_bool API_Numerical_NaturalCubicSplineGenerateD(double* dy2, const double* fx, const double* fy, int32 n)
{

  abort();
}
api_bool API_Numerical_CubicSplineInterpolateF(float* y, const float* fx, const float* fy, const float* dy2, int32 n, double x, int32* k)
{

  abort();
}
api_bool API_Numerical_CubicSplineInterpolateD(double* y, const double* fx, const double* fy, const double* dy2, int32 n, double x, int32* k)
{

  abort();
}
api_bool API_Numerical_NaturalGridCubicSplineGenerateF(float* dy2, const float* fy, int32 n)
{

  abort();
}
api_bool API_Numerical_NaturalGridCubicSplineGenerateD(double* dy2, const double* fy, int32 n)
{

  abort();
}
api_bool API_Numerical_NaturalGridCubicSplineGenerateUI8(float* dy2, const uint8* fy, int32 n)
{

  abort();
}
api_bool API_Numerical_NaturalGridCubicSplineGenerateUI16(float* dy2, const uint16* fy, int32 n)
{

  abort();
}
api_bool API_Numerical_NaturalGridCubicSplineGenerateUI32(double* dy2, const uint32* fy, int32 n)
{

  abort();
}
api_bool API_Numerical_NaturalGridCubicSplineInterpolateF(float* y, const float* fy, const float* dy2, int32 n, double x)
{

  abort();
}
api_bool API_Numerical_NaturalGridCubicSplineInterpolateD(double* y, const double* fy, const double* dy2, int32 n, double x)
{

  abort();
}
api_bool API_Numerical_NaturalGridCubicSplineInterpolateUI8(float* y, const uint8* fy, const float* dy2, int32 n, double x)
{

  abort();
}
api_bool API_Numerical_NaturalGridCubicSplineInterpolateUI16(float* y, const uint16* fy, const float* dy2, int32 n, double x)
{

  abort();
}
api_bool API_Numerical_NaturalGridCubicSplineInterpolateUI32(double* y, const uint32* fy, const double* dy2, int32 n, double x)
{

  abort();
}
api_bool API_Numerical_SurfaceSplineCreateF(sspline_handle* hSS, int32 rbf, double e2, api_bool polynomial, const float* x, const float* y, const float* z, int32 n, int32 m, float rho, const float* w)
{

  abort();
}
api_bool API_Numerical_SurfaceSplineCreateD(sspline_handle* hSS, int32 rbf, double e2, api_bool polynomial, const double* x, const double* y, const double* z, int32 n, int32 m, float rho, const float* w)
{

  abort();
}
api_bool API_Numerical_SurfaceSplineEvaluate(const_sspline_handle hSS, double* z, double x, double y)
{

  abort();
}
api_bool API_Numerical_SurfaceSplineEvaluateVectorF(const_sspline_handle hSS, float* z, const float *x, const float *y, double x0, double y0, double r, size_type n)
{

  abort();
}
api_bool API_Numerical_SurfaceSplineEvaluateVectorD(const_sspline_handle hSS, double* z, const double *x, const double *y, double x0, double y0, double r, size_type n)
{

  abort();
}
api_bool API_Numerical_SurfaceSplineDestroy(sspline_handle hSS)
{
  // ### The following function returns a null-terminated string allocated by the caller module.
  abort();
}
   char*          (API_Numerical_SurfaceSplineSerialize)( api_handle hModule, const_sspline_handle hSS, uint32 flags )
   {

     abort();
   }
   api_bool       (API_Numerical_SurfaceSplineDeserialize)( sspline_handle* hSS, const char* data, size_type len, uint32 flags )
   {

     abort();
   }

   api_bool       (API_Numerical_SurfaceSplineDuplicate)( sspline_handle* hSS1, const_sspline_handle hSS )
   {

     abort();
   }


// ----------------------------------------------------------------------------
// ThreadContext API
// ----------------------------------------------------------------------------

thread_handle API_Thread_CreateThread(api_handle handle, api_handle client, uint32 flags)
{

  return pcl_mock::CreateThread(handle, client, flags);
}

void API_Thread_StartThread(thread_handle handle, uint32 priority)
{
  pcl_mock::StartThread(handle, priority);
}

void API_Thread_KillThread(thread_handle)
{

  abort();
}

api_bool API_Thread_IsThreadActive(const_thread_handle handle)
{
  return pcl_mock::IsThreadActive(handle);
}

uint32 API_Thread_GetThreadPriority(const_thread_handle)
{

  abort();
}
void API_Thread_SetThreadPriority(thread_handle, uint32)
{

  abort();
}
uint32 API_Thread_GetThreadStackSize(const_thread_handle)
{

  abort();
}
void API_Thread_SetThreadStackSize(thread_handle, uint32)
{

  abort();
}
api_bool API_Thread_WaitThread(thread_handle, uint32 msec)
{

  abort();
}
void API_Thread_SleepThread(thread_handle, uint32 msec)
{

  abort();
}
uint32 API_Thread_GetThreadStatus(const_thread_handle)
{

  abort();
}
void API_Thread_SetThreadStatus(thread_handle, uint32)
{

  abort();
}
api_bool API_Thread_GetThreadStatusEx(const_thread_handle, uint32* status, uint32 flags)
{
  // 0x00=force_lock 0x01=try_lock
  abort();
}

   api_bool       (API_Thread_GetThreadConsoleOutputText)( const_thread_handle, char16_type* text, size_type* len )
   {

     abort();
   }
   void           (API_Thread_AppendThreadConsoleOutputText)( thread_handle, const char16_type* text, api_bool appendNewline )
   {

     abort();
   }
   void           (API_Thread_ClearThreadConsoleOutputText)( thread_handle )
   {

     abort();
   }

   thread_handle  (API_Thread_GetCurrentThread)()
   {

     return pcl_mock::GetCurrentThread();
   }

   api_bool       (API_Thread_SetThreadExecRoutine)( thread_handle handle, pcl::thread_exec_routine dispatcher)
   {

     return pcl_mock::SetThreadExecRoutine(handle, dispatcher);
   }

   int32          (API_Thread_PerformanceAnalysisValue)( int32 algorithm, size_type length,
                                                        int32 itemSize, api_bool floatingPoint, int32 kernelSize, int32 width, int32 height )
   {

     abort();
   }

// ----------------------------------------------------------------------------
// MutexContext API
// ----------------------------------------------------------------------------

mutex_handle API_Mutex_CreateMutex(api_handle, api_handle client, uint32 flags)
{
  // ### deprecated
  abort();
}
   mutex_handle   (API_Mutex_CreateReadWriteMutex)( api_handle, api_handle client, uint32 flags )
   {

     abort();
   }

   api_bool       (API_Mutex_GetLockState)( const_mutex_handle )
   {
     // ### disabled ### returns api_true if the mutex is locked
     abort();
   }

   api_bool       (API_Mutex_Lock)( mutex_handle, api_bool tryLock )
   {
     // ### deprecated
     abort();
   }
   api_bool       (API_Mutex_LockForRead)( mutex_handle, api_bool tryLock )
   {

     abort();
   }
   api_bool       (API_Mutex_LockForWrite)( mutex_handle, api_bool tryLock )
   {

     abort();
   }

   void           (API_Mutex_Unlock)( mutex_handle )
   {

     abort();
   }

// ----------------------------------------------------------------------------
// ViewListContext API
// ----------------------------------------------------------------------------

control_handle API_ViewList_CreateViewList(api_handle, api_handle client, control_handle parent, uint32 flags)
{

  abort();
}
void API_ViewList_RegenerateViewList(control_handle, api_bool mainViews, api_bool previews, api_bool realTimePreview)
{

  abort();
}
void API_ViewList_GetViewListContents(const_control_handle, api_bool* mainViews, api_bool* previews, api_bool* realTimePreview)
{

  abort();
}
const_view_handle API_ViewList_GetViewListExcludedView(const_control_handle)
{

  abort();
}
void API_ViewList_SetViewListExcludedView(control_handle, const_view_handle)
{

  abort();
}
view_handle API_ViewList_GetViewListCurrentView(const_control_handle)
{

  abort();
}
void API_ViewList_SetViewListCurrentView(control_handle, view_handle)
{

  abort();
}
api_bool API_ViewList_FindViewListView(const_control_handle, const_view_handle)
{

  abort();
}
void API_ViewList_RemoveViewListView(control_handle, const_view_handle)
{

  abort();
}
api_bool API_ViewList_SetViewListViewSelectedEventRoutine(control_handle, api_handle, pcl::view_event_routine)
{

  abort();
}
api_bool API_ViewList_SetViewListCurrentViewUpdatedEventRoutine(control_handle, api_handle, pcl::view_event_routine)
{

  abort();
}

// ----------------------------------------------------------------------------
// BitmapContext API
// ----------------------------------------------------------------------------

void API_Bitmap_OrBitmap(bitmap_handle, int32, int32, int32, int32, uint32)
{

  abort();
}
void API_Bitmap_OrBitmaps(bitmap_handle, int32, int32, const_bitmap_handle, int32, int32, int32, int32)
{

  abort();
}
void API_Bitmap_AndBitmap(bitmap_handle, int32, int32, int32, int32, uint32)
{

  abort();
}
void API_Bitmap_AndBitmaps(bitmap_handle, int32, int32, const_bitmap_handle, int32, int32, int32, int32)
{

  abort();
}
void API_Bitmap_XorBitmap(bitmap_handle, int32, int32, int32, int32, uint32)
{

  abort();
}
void API_Bitmap_XorBitmaps(bitmap_handle, int32, int32, const_bitmap_handle, int32, int32, int32, int32)
{

  abort();
}
void API_Bitmap_XorBitmapRect(bitmap_handle, int32, int32, int32, int32, uint32)
{

  abort();
}
void API_Bitmap_ReplaceBitmapColor(bitmap_handle, int32, int32, int32, int32, uint32, uint32)
{

  abort();
}
void API_Bitmap_SetBitmapAlpha(bitmap_handle, int32, int32, int32, int32, uint8)
{

  abort();
}

// ----------------------------------------------------------------------------
// SVGContext API
// ----------------------------------------------------------------------------

svg_handle API_SVG_CreateSVGFile(api_handle, const char16_type*, int32, int32, uint32)
{

  abort();
}
svg_handle API_SVG_CreateSVGBuffer(api_handle, int32, int32, uint32)
{

  abort();
}
api_bool API_SVG_GetSVGDimensions(const_svg_handle, int32*, int32*)
{

  abort();
}
api_bool API_SVG_SetSVGDimensions(svg_handle, int32, int32)
{

  abort();
}
api_bool API_SVG_GetSVGViewBox(const_svg_handle, double*, double*, double*, double*)
{

  abort();
}
api_bool API_SVG_SetSVGViewBox(svg_handle, double, double, double, double)
{

  abort();
}
int32 API_SVG_GetSVGResolution(const_svg_handle)
{

  abort();
}
void API_SVG_SetSVGResolution(svg_handle, int32)
{

  abort();
}
api_bool API_SVG_GetSVGFilePath(const_svg_handle, char16_type*, size_type*)
{

  abort();
}
api_bool API_SVG_GetSVGDataBuffer(const_svg_handle, void*, size_type*)
{

  abort();
}
api_bool API_SVG_GetSVGTitle(const_svg_handle, char16_type*, size_type*)
{

  abort();
}
void API_SVG_SetSVGTitle(svg_handle, const char16_type*)
{

  abort();
}
api_bool API_SVG_GetSVGDescription(const_svg_handle, char16_type*, size_type*)
{

  abort();
}
void API_SVG_SetSVGDescription(svg_handle, const char16_type*)
{

  abort();
}
api_bool API_SVG_IsSVGPainting(const_svg_handle)
{

  abort();
}

// ----------------------------------------------------------------------------
// BrushContext API
// ----------------------------------------------------------------------------

brush_handle API_Brush_CreateBrush(api_handle, uint32, int32)
{

  abort();
}
brush_handle API_Brush_CreateBitmapBrush(api_handle, const_bitmap_handle)
{

  abort();
}
brush_handle API_Brush_CreateLinearGradientBrush(api_handle, double x1, double y1, double x2, double y2, int32 spread, const api_gradient_stop*, size_type count)
{
  // spread: 0=pad 1=reflect 2=repeat
  abort();
}
   brush_handle   (API_Brush_CreateRadialGradientBrush)( api_handle, double cx, double cy, double r, double fx, double fy,
                                                         int32 spread, const api_gradient_stop*, size_type count )
   {

     abort();
   }
   brush_handle   (API_Brush_CreateConicalGradientBrush)( api_handle, double cx, double cy, double angle,
                                                         const api_gradient_stop*, size_type count )
   {

     abort();
   }
   brush_handle   (API_Brush_CloneBrush)( api_handle, const_brush_handle )
   {

     abort();
   }

   uint32         (API_Brush_GetBrushColor)( const_brush_handle )
   {

     abort();
   }
   void           (API_Brush_SetBrushColor)( brush_handle, uint32 )
   {

     abort();
   }

   int32          (API_Brush_GetBrushStyle)( const_brush_handle )
   {

     abort();
   }
   void           (API_Brush_SetBrushStyle)( brush_handle, int32 )
   {

     abort();
   }

   bitmap_handle  (API_Brush_GetBrushBitmap)( const_brush_handle )
   {

     abort();
   }
   void           (API_Brush_SetBrushBitmap)( brush_handle, const_bitmap_handle )
   {

     abort();
   }

   int32          (API_Brush_GetBrushGradientType)( const_brush_handle )
   {
     // 0=none 1=linear 2=radial 3=conical
     abort();
   }
   api_bool       (API_Brush_GetBrushLinearGradientParameters)( const_brush_handle, double* x1, double* y1, double* x2, double* y2 )
   {

     abort();
   }
   api_bool       (API_Brush_GetBrushRadialGradientParameters)( const_brush_handle, double* cx, double* cy, double* r, double* fx, double* fy )
   {

     abort();
   }
   api_bool       (API_Brush_GetBrushConicalGradientParameters)( const_brush_handle, double* cx, double* cy, double* angle )
   {

     abort();
   }
   int32          (API_Brush_GetBrushGradientSpread)( const_brush_handle )
   {
     // -1=error 0=pad 1=reflect 2=repeat
     abort();
   }
   api_bool       (API_Brush_GetBrushGradientStops)( const_brush_handle, api_gradient_stop*, size_type *count )
   {

     abort();
   }

// ----------------------------------------------------------------------------
// PenContext API
// ----------------------------------------------------------------------------

pen_handle API_Pen_CreatePen(api_handle, uint32, float, int32, int32, int32)
{

  abort();
}
pen_handle API_Pen_ClonePen(api_handle, const_pen_handle)
{

  abort();
}
api_bool API_Pen_GetPenWidth(const_pen_handle, float*)
{

  abort();
}
void API_Pen_SetPenWidth(pen_handle, float)
{

  abort();
}
uint32 API_Pen_GetPenColor(const_pen_handle)
{

  abort();
}
void API_Pen_SetPenColor(pen_handle, uint32)
{

  abort();
}
int32 API_Pen_GetPenStyle(const_pen_handle)
{

  abort();
}
void API_Pen_SetPenStyle(pen_handle, int32)
{

  abort();
}
int32 API_Pen_GetPenCap(const_pen_handle)
{

  abort();
}
void API_Pen_SetPenCap(pen_handle, int32)
{

  abort();
}
int32 API_Pen_GetPenJoin(const_pen_handle)
{

  abort();
}
void API_Pen_SetPenJoin(pen_handle, int32)
{

  abort();
}
brush_handle API_Pen_GetPenBrush(const_pen_handle)
{

  abort();
}
void API_Pen_SetPenBrush(pen_handle, const_brush_handle)
{

  abort();
}


// ----------------------------------------------------------------------------
// ModuleDefinitionContext API
// ----------------------------------------------------------------------------

void API_ModuleDefinition_EnterModuleDefinitionContext()
{

  abort();
}
api_bool API_ModuleDefinition_IsModuleDefinitionContextActive()
{

  abort();
}
void API_ModuleDefinition_SetModuleOnLoadRoutine(pcl::module_on_load_routine)
{

  abort();
}
void API_ModuleDefinition_SetModuleOnUnloadRoutine(pcl::module_on_unload_routine)
{

  abort();
}
void API_ModuleDefinition_SetModuleAllocationRoutine(pcl::module_allocation_routine)
{

  abort();
}
void API_ModuleDefinition_SetModuleDeallocationRoutine(pcl::module_deallocation_routine)
{

  abort();
}
void API_ModuleDefinition_ExitModuleDefinitionContext()
{

  abort();
}

// ----------------------------------------------------------------------------
// ProcessDefinitionContext API
// ----------------------------------------------------------------------------

void API_ProcessDefinition_EnterProcessDefinitionContext()
{

  abort();
}
api_bool API_ProcessDefinition_IsProcessDefinitionContextActive()
{

  abort();
}
void API_ProcessDefinition_BeginProcessDefinition(meta_process_handle, const char* procId)
{

  abort();
}
api_bool API_ProcessDefinition_GetProcessBeingDefined(char*, size_type*)
{

  abort();
}
void API_ProcessDefinition_SetProcessCategory(const char*)
{

  abort();
}
void API_ProcessDefinition_SetProcessVersion(uint32)
{

  abort();
}
void API_ProcessDefinition_SetProcessAliasIdentifiers(const char*)
{

  abort();
}
void API_ProcessDefinition_SetProcessDescription(const char16_type*)
{

  abort();
}
void API_ProcessDefinition_SetProcessScriptComment(const char16_type*)
{

  abort();
}
void API_ProcessDefinition_SetProcessIconSVG(const char*)
{

  abort();
}
void API_ProcessDefinition_SetProcessIconSVGFile(const char16_type*)
{

  abort();
}
void API_ProcessDefinition_SetProcessIconImage(const char**)
{
  // ### deprecated
  abort();
}
   void        (API_ProcessDefinition_SetProcessIconImageFile)( const char16_type* )
   {
     // ### deprecated
     abort();
   }
   void        (API_ProcessDefinition_SetProcessIconSmallImage)( const char** )
   {
     // ### deprecated
     abort();
   }
   void        (API_ProcessDefinition_SetProcessIconSmallImageFile)( const char16_type* )
   {
     // ### deprecated
     abort();
   }

   void        (API_ProcessDefinition_SetProcessClassInitializationRoutine)( pcl::process_class_initialization_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetProcessCreationRoutine)( pcl::process_creation_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetProcessDestructionRoutine)( pcl::process_destruction_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetProcessClonationRoutine)( pcl::process_clonation_routine)
   {

     abort();
   }
   void        (API_ProcessDefinition_SetProcessTestClonationRoutine)( pcl::process_test_clonation_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetProcessSetServerHandleRoutine)( pcl::process_set_handle_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetProcessAssignmentRoutine)( pcl::process_assignment_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetProcessInitializationRoutine)( pcl::process_initialization_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetProcessValidationRoutine)( pcl::process_validation_routine )
   {

     abort();
   }

   void        (API_ProcessDefinition_SetProcessCommandLineProcessingRoutine)( pcl::process_command_line_processing_routine, uint32 flags )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetProcessEditPreferencesRoutine)( pcl::process_edit_preferences_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetProcessBrowseDocumentationRoutine)( pcl::process_browse_documentation_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetProcessExecutionPreferencesRoutine)( pcl::process_execution_preferences_routine )
   {

     abort();
   }

   void        (API_ProcessDefinition_SetProcessExecutionValidationRoutine)( pcl::process_execution_validation_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetProcessMaskValidationRoutine)( pcl::process_mask_validation_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetProcessHistoryUpdateValidationRoutine)( pcl::process_history_update_validation_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetProcessUndoModeRoutine)( pcl::process_undo_mode_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetProcessPreExecutionRoutine)( pcl::process_pre_execution_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetProcessExecutionRoutine)( pcl::process_execution_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetProcessPostExecutionRoutine)( pcl::process_post_execution_routine )
   {

     abort();
   }

   void        (API_ProcessDefinition_SetProcessGlobalExecutionValidationRoutine)( pcl::process_global_execution_validation_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetProcessPreGlobalExecutionRoutine)( pcl::process_pre_global_execution_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetProcessGlobalExecutionRoutine)( pcl::process_global_execution_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetProcessPostGlobalExecutionRoutine)( pcl::process_post_global_execution_routine )
   {

     abort();
   }

   void        (API_ProcessDefinition_SetProcessImageExecutionValidationRoutine)( pcl::process_image_execution_validation_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetProcessImageExecutionRoutine)( pcl::process_image_execution_routine )
   {

     abort();
   }

   void        (API_ProcessDefinition_SetProcessDefaultInterfaceSelectionRoutine)( pcl::process_default_interface_selection_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetProcessInterfaceSelectionRoutine)( pcl::process_interface_selection_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetProcessInterfaceValidationRoutine)( pcl::process_interface_validation_routine )
   {

     abort();
   }

   void        (API_ProcessDefinition_SetProcessPreReadingRoutine)( pcl::process_pre_reading_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetProcessPostReadingRoutine)( pcl::process_post_reading_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetProcessPreWritingRoutine)( pcl::process_pre_writing_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetProcessPostWritingRoutine)( pcl::process_post_writing_routine )
   {

     abort();
   }

   void        (API_ProcessDefinition_SetProcessIPCStartRoutine)( pcl::process_ipc_notification_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetProcessIPCStopRoutine)( pcl::process_ipc_notification_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetProcessIPCSetParametersRoutine)( pcl::process_ipc_notification_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetProcessIPCGetStatusRoutine)( pcl::process_ipc_status_routine )
   {

     abort();
   }

   void        (API_ProcessDefinition_BeginParameterDefinition)( meta_parameter_handle, const char* parId, uint32 parType )
   {

     abort();
   }
   api_bool    (API_ProcessDefinition_GetParameterBeingDefined)( char*, size_type* )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetParameterProcessVersionRange)( uint32, uint32 )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetParameterRequired)( api_bool )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetParameterReadOnly)( api_bool )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetParameterAliasIdentifiers)( const char* )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetParameterDescription)( const char16_type* )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetParameterScriptComment)( const char16_type* )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetParameterLockRoutine)( pcl::parameter_lock_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetParameterUnlockRoutine)( pcl::parameter_unlock_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetParameterValidationRoutine)( pcl::parameter_validation_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetParameterAllocationRoutine)( pcl::parameter_allocation_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetParameterLengthQueryRoutine)( pcl::parameter_length_query_routine )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetDefaultNumericValue)( double )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetValidNumericRange)( double, double )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetPrecision)( int32 )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetScientificNotation)( api_bool )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetDefaultBooleanValue)( api_bool )
   {

     abort();
   }
   void        (API_ProcessDefinition_DefineEnumerationElement)( const char*, api_enum )
   {

     abort();
   }
   void        (API_ProcessDefinition_DefineEnumerationAlias)( const char*, const char* )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetDefaultEnumerationValueIndex)( uint32 )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetDefaultStringValue)( const char16_type* )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetStringAllowedCharacters)( const char16_type* )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetStringLengthLimits)( size_type, size_type )
   {

     abort();
   }
   void        (API_ProcessDefinition_BeginTableColumnDefinition)( meta_parameter_handle, const char* colId, uint32 colType )
   {

     abort();
   }
   void        (API_ProcessDefinition_EndTableColumnDefinition)()
   {

     abort();
   }
   void        (API_ProcessDefinition_SetTableRowLimits)( size_type, size_type )
   {

     abort();
   }
   void        (API_ProcessDefinition_SetBlockSizeLimits)( size_type, size_type )
   {

     abort();
   }

   void        (API_ProcessDefinition_EndParameterDefinition)()
   {

     abort();
   }
   void        (API_ProcessDefinition_EndProcessDefinition)()
   {

     abort();
   }
   void        (API_ProcessDefinition_ExitProcessDefinitionContext)()
   {

     abort();
   }

// ----------------------------------------------------------------------------
// InterfaceDefinitionContext API
// ----------------------------------------------------------------------------

void API_InterfaceDefinition_EnterInterfaceDefinitionContext()
{

  abort();
}
api_bool API_InterfaceDefinition_IsInterfaceDefinitionContextActive()
{

  abort();
}
void API_InterfaceDefinition_BeginInterfaceDefinition(meta_interface_handle, const char* ifaceId, uint32 flags)
{

  abort();
}
api_bool API_InterfaceDefinition_GetInterfaceBeingDefined(char*, size_type*)
{

  abort();
}
void API_InterfaceDefinition_SetInterfaceVersion(uint32)
{

  abort();
}
void API_InterfaceDefinition_SetInterfaceAliasIdentifiers(const char*)
{

  abort();
}
void API_InterfaceDefinition_SetInterfaceDescription(const char16_type*)
{

  abort();
}
void API_InterfaceDefinition_SetInterfaceIconSVG(const char*)
{

  abort();
}
void API_InterfaceDefinition_SetInterfaceIconSVGFile(const char16_type*)
{

  abort();
}
void API_InterfaceDefinition_SetInterfaceIconImage(const char**)
{
  // ### deprecated
  abort();
}
   void           (API_InterfaceDefinition_SetInterfaceIconImageFile)( const char16_type* )
   {
     // ### deprecated
     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceIconSmallImage)( const char** )
   {
     // ### deprecated
     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceIconSmallImageFile)( const char16_type* )
   {
     // ### deprecated
     abort();
   }

   void           (API_InterfaceDefinition_SetInterfaceFeatures)( uint32, uint32 )
   {

     abort();
   }

   void           (API_InterfaceDefinition_SetInterfaceInitializationRoutine)( pcl::interface_initialization_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceLaunchRoutine)( pcl::interface_launch_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceProcessInstantiationRoutine)( pcl::interface_process_instantiation_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceProcessTestInstantiationRoutine)( pcl::interface_process_instantiation_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceProcessValidationRoutine)( pcl::interface_process_validation_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceProcessImportRoutine)( pcl::interface_process_import_routine )
   {

     abort();
   }

   void           (API_InterfaceDefinition_SetInterfaceApplyRoutine)( pcl::interface_control_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceApplyGlobalRoutine)( pcl::interface_control_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceRealTimePreviewUpdatedRoutine)( pcl::interface_control_state_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceExecuteRoutine)( pcl::interface_control_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceCancelRoutine)( pcl::interface_control_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceBrowseDocumentationRoutine)( pcl::interface_control_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceTrackViewUpdatedRoutine)( pcl::interface_control_state_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceEditPreferencesRoutine)( pcl::interface_control_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceResetRoutine)( pcl::interface_control_routine )
   {

     abort();
   }

   void           (API_InterfaceDefinition_SetInterfaceRealTimeUpdateQueryRoutine)( pcl::interface_real_time_update_query_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceRealTimeGenerationFlagsRoutine)( pcl::interface_real_time_generation_flags_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceRealTimeGenerationRoutine)( pcl::interface_real_time_generation_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceRealTimeCancelRoutine)( pcl::interface_real_time_cancel_routine )
   {

     abort();
   }

   void           (API_InterfaceDefinition_SetInterfaceDynamicModeEnterRoutine)( pcl::interface_dynamic_mode_enter_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceDynamicModeExitRoutine)( pcl::interface_dynamic_mode_exit_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceDynamicMouseEnterRoutine)( pcl::interface_dynamic_view_event_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceDynamicMouseLeaveRoutine)( pcl::interface_dynamic_view_event_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceDynamicMouseMoveRoutine)( pcl::interface_dynamic_mouse_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceDynamicMousePressRoutine)( pcl::interface_dynamic_mouse_button_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceDynamicMouseReleaseRoutine)( pcl::interface_dynamic_mouse_button_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceDynamicMouseDoubleClickRoutine)( pcl::interface_dynamic_mouse_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceDynamicKeyPressRoutine)( pcl::interface_dynamic_keyboard_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceDynamicKeyReleaseRoutine)( pcl::interface_dynamic_keyboard_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceDynamicMouseWheelRoutine)( pcl::interface_dynamic_wheel_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceDynamicUpdateQueryRoutine)( pcl::interface_dynamic_update_query_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetInterfaceDynamicPaintRoutine)( pcl::interface_dynamic_paint_routine )
   {

     abort();
   }

   void           (API_InterfaceDefinition_SetImageCreatedNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetImageUpdatedNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetImageRenamedNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetImageDeletedNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetImageFocusedNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetImageLockedNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetImageUnlockedNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetImageSTFEnabledNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetImageSTFDisabledNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetImageSTFUpdatedNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetImageRGBWSUpdatedNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetImageCMEnabledNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetImageCMDisabledNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetImageCMUpdatedNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetImageSavedNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }

   void           (API_InterfaceDefinition_SetMaskUpdatedNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetMaskEnabledNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetMaskDisabledNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetMaskShownNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetMaskHiddenNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }

   void           (API_InterfaceDefinition_SetTransparencyHiddenNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetTransparencyModeUpdatedNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }

   void           (API_InterfaceDefinition_SetViewPropertyUpdatedNotificationRoutine)( pcl::view_property_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetViewPropertyDeletedNotificationRoutine)( pcl::view_property_notification_routine )
   {

     abort();
   }

   void           (API_InterfaceDefinition_SetBeginReadoutNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetUpdateReadoutNotificationRoutine)( pcl::readout_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetEndReadoutNotificationRoutine)( pcl::image_notification_routine )
   {

     abort();
   }

   void           (API_InterfaceDefinition_SetProcessCreatedNotificationRoutine)( pcl::process_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetProcessUpdatedNotificationRoutine)( pcl::process_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetProcessDeletedNotificationRoutine)( pcl::process_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetProcessSavedNotificationRoutine)( pcl::process_notification_routine )
   {

     abort();
   }

   void           (API_InterfaceDefinition_SetRealTimePreviewOwnerChangeNotificationRoutine)( pcl::interface_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetRealTimePreviewLUTUpdatedNotificationRoutine)( pcl::lut_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetRealTimePreviewGenerationStartNotificationRoutine)( pcl::global_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetRealTimePreviewGenerationFinishNotificationRoutine)( pcl::global_notification_routine )
   {

     abort();
   }

   void           (API_InterfaceDefinition_SetGlobalRGBWSUpdatedNotificationRoutine)( pcl::global_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetGlobalCMEnabledNotificationRoutine)( pcl::global_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetGlobalCMDisabledNotificationRoutine)( pcl::global_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetGlobalCMUpdatedNotificationRoutine)( pcl::global_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetReadoutOptionsUpdatedNotificationRoutine)( pcl::global_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetGlobalPreferencesUpdatedNotificationRoutine)( pcl::global_notification_routine )
   {

     abort();
   }
   void           (API_InterfaceDefinition_SetGlobalFiltersUpdatedNotificationRoutine)( pcl::global_notification_routine )
   {

     abort();
   }

   void           (API_InterfaceDefinition_EndInterfaceDefinition)()
   {

     abort();
   }
   void           (API_InterfaceDefinition_ExitInterfaceDefinitionContext)()
   {

     abort();
   }

// ----------------------------------------------------------------------------
// SharedImageContext API
// ----------------------------------------------------------------------------

void *API_SharedImage_GetImageOwner(const_image_handle)
{

  abort();
}
api_bool API_SharedImage_GetImageRefCount(const_image_handle, uint32*)
{

  abort();
}
api_bool API_SharedImage_IsValidImageHandle(const_image_handle)
{

  abort();
}
api_bool API_SharedImage_AttachToImage(image_handle, void*)
{

  abort();
}

api_bool API_SharedImage_GetImageFormat(const_image_handle, uint32* nbits, api_bool* flt)
{

  abort();
}

api_bool API_SharedImage_SetImageRGBWS(image_handle, const api_RGBWS*)
{
  // ### must be set through ImageWindow
  abort();
}

api_bool API_Control_GetClientRect(const_control_handle handle,
                                   int32* x, int32* y,
                                   int32* w, int32* hgt)
{
  /*    if (!w || !hgt) return api_false;
    
    MockControl* wdg = GetControlBox(handle);
    if (wdg) {
    QRect r = wdg->widget->contentsRect();

      if (x) *x = r.x();
      if (y) *y = r.y();
      *w   = r.width();
      *hgt = r.height();

      return api_true;

    }
  */
    // Return a harmless safe rect
    if (x) *x = 0;
    if (y) *y = 0;
    *w   = 0;
    *hgt = 0;
    return api_true;

}
  
void API_Bitmap_CloneBitmap() { abort(); }
void API_Bitmap_CreateBitmapFromData() { abort(); }
void API_Bitmap_CreateBitmapFromFile() { abort(); }
void API_Bitmap_CreateBitmapXPM() { abort(); }
void API_Bitmap_CreateEmptyBitmap() { abort(); }
void API_Button_SetButtonChecked() {  }
void API_Button_SetButtonText() {  }
int32 API_ComboBox_GetComboBoxLength() { return 1; }
void API_ComboBox_InsertComboBoxItem() {  }
void API_ComboBox_SetComboBoxCurrentItem() {  }
api_bool API_ComboBox_SetComboBoxItemSelectedEventRoutine() { return api_true; }
void API_Control_EnsureControlLayoutUpdated() {  }
api_bool       (API_Control_GetControlDisplayPixelRatio)( const_control_handle, double* ratio)
   {
     *ratio = 1.0;
     return api_true;
   }
void API_Control_GetControlEnabled() { abort(); }
  font_handle API_Control_GetControlFont() { return new QFont; }
void API_Control_GetControlMaxSize() { abort(); }
void API_Control_GetControlMinSize() { abort(); }
void API_Control_GetControlPosition() { abort(); }
void API_Control_SetControlEnabled() { abort(); }
void API_Control_SetControlFocus() { abort(); }
void API_Control_SetControlPosition() { abort(); }
void API_Control_SetControlSize() { abort(); }
api_bool API_Control_SetGetFocusEventRoutine() { return api_true; }
api_bool API_Control_SetKeyPressEventRoutine() { return api_true; }
api_bool API_Control_SetLoseFocusEventRoutine() { return api_true; }
api_bool API_Control_SetMousePressEventRoutine() { return api_true; }
void API_Control_SetRealTimePreviewActive() { abort(); }
void API_Control_SetWindowTitle() {  }
void API_Control_SetWindowToolTip() {  }
void API_Edit_GetEditReadOnly() { abort(); }
void API_Edit_GetEditText() { abort(); }
void API_Edit_SetEditSelected() { abort(); }
void API_Edit_SetEditText() { }
api_bool API_Edit_SetEditValidatingRegExp() { return api_true; }
void API_Font_CloneFont() { abort(); }
  int32 API_Font_GetStringPixelWidth() { return 12;  }
void API_Global_Abort() { abort(); }
void API_Global_Allocate() { abort(); }
void API_Global_BrowseProcessDocumentation() { abort(); }
void API_Global_Deallocate() { abort(); }
void API_Global_EnableAbort() { abort(); }
void API_Global_ErrorMessage() { abort(); }
void API_Global_GetConsole() { abort(); }
void API_Global_GetGlobalInteger() { abort(); }
void API_Global_GetKeyboardModifiers() { abort(); }
void API_Global_GetProcessStatus() { abort(); }
void API_Global_LastError() { abort(); }
void API_Global_LaunchProcessInstance4() { abort(); }
void API_Global_LaunchProcessInstanceOnView() { abort(); }
void API_Global_MessageBox() { abort(); }
void API_Global_ReadSettingsInteger() { abort(); }
void API_Global_ShowConsole() { abort(); }
void API_Global_WriteConsole() { abort(); }
void API_Global_WriteSettingsInteger() { abort(); }
void API_Label_SetLabelAlignment() {  }
void API_Label_SetLabelText() { }
void API_Process_CloneProcessInstance() { abort(); }
void API_SharedImage_DetachFromImage() { abort(); }
void API_SharedImage_GetImageColorSpace() { abort(); }
void API_SharedImage_GetImageGeometry() { abort(); }
void API_SharedImage_GetImagePixelData() { abort(); }
void API_SharedImage_GetImageRGBWS() { abort(); }
void API_SharedImage_SetImageColorSpace() { abort(); }
void API_SharedImage_SetImageGeometry() { abort(); }
void API_SharedImage_SetImagePixelData() { abort(); }
  api_bool API_Sizer_GetSizerDisplayPixelRatio(const_sizer_handle handle, double* ratio) { *ratio = 1.0;return api_true;}
void API_Sizer_InsertSizerSpacing() {  }
void API_Sizer_InsertSizerStretch() {  }
void API_Sizer_SetSizerMargin() { }
void API_Sizer_SetSizerSpacing() {  }
  void API_Slider_GetSliderRange(const_control_handle handle, int32* minValue, int32* maxValue) { *minValue = 0; *maxValue=100; }
void API_Slider_GetSliderValue() { abort(); }
void API_Slider_SetSliderPageSize() {  }
void API_Slider_SetSliderRange() {  }
void API_Slider_SetSliderTickInterval() {  }
void API_Slider_SetSliderTickStyle() { }
api_bool API_SpinBox_SetSpinBoxValueUpdatedEventRoutine() { return api_true; }
void API_UI_AttachToUIObject() { abort(); }
  api_bool API_UI_DetachFromUIObject() { return api_true; }

// Mock for GetUIObjectRefCount
size_type API_UI_GetUIObjectRefCount(const_api_handle ui_object) {
    
    if (!ui_object) {
        return 0; // No references for null object
    }
    
    // Return 1 to indicate the object exists and has at least one reference
    return 1;
}
void API_View_GetViewById() { abort(); }
void API_View_GetViewFullId() { abort(); }
void API_View_GetViewId() { abort(); }
void API_View_GetViewImage() { abort(); }
void API_View_GetViewLocks() { abort(); }
void API_View_LockView() { abort(); }
void API_View_UnlockView() { abort(); }

}  // extern "C"
