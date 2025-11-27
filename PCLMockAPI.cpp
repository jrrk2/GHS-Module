/**
 * PCLMockAPI.cpp - Implementation of the PCL mock API
 */

#include "PCLMockAPI.h"
#include "PCLThreadMock.h"
#include "FITS/FITS.h"
#include <fitsio.h>

#include <QWidget>
#include <QTreeWidgetItem>
#include <QBoxLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QPushButton>
#include <QSpinBox>
#include <QLabel>
#include <QScreen>
#include <QApplication>
#include <QGuiApplication>
#include <QPixmap>
#include <QImage>
#include <QSvgRenderer>
#include <QPainter>
#include <QFile>
#include <QBuffer>
#include <QImageReader>
#include <QImageWriter>
#include <QScrollArea>
#include <QComboBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QGroupBox>
#include <QTimer>
#include <QBitmap>
/*
#include <>
#include <>
*/
#include <map>
#include <mutex>
#include <string>
#include <iostream>
#include <fstream>
#include <memory.h>
#include <fftw3.h>
#include <mutex>
#include <map>
#include <pcl/StandardAllocator.h>
#include <pcl/Complex.h> // For dcomplex
#include <cstring> // For memcpy
#include <pcl/XISF.h>
#include <pcl/Edit.h>
#include <pcl/TreeBox.h>

// #include <pcl/XISFReader.h>

#ifdef __PCL_WINDOWS
#include <pcl/AutoLock.h>
static pcl::Mutex s_cfitsio_mutex;
#define CFITSIO_LOCK volatile pcl::AutoLock lock( s_cfitsio_mutex );
#else
#define CFITSIO_LOCK
#endif

// Add these to your PCLMockAPI.cpp

#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QMoveEvent>
#include <QEnterEvent>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QMoveEvent>
// ----------------------------------------------------------------------------
// TreeBox mock support
// ----------------------------------------------------------------------------

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>

// ----------------------------------------------------------------------------
// Event Handler Storage (updated MockControl structure)
// ----------------------------------------------------------------------------


struct TreeNode
{
    std::string text[3];
    control_handle icon[3];
    bool selected = false;
};

struct MockTreeBox ;

// Update your existing MockControl structure to include these event handler fields:

struct MockControl {
    QWidget* widget;
    api_handle clientHandle;
    QLayout* layout;
    
    // Event handlers - add these to your existing structure
    pcl::paint_event_routine paintHandler;
    void* paintReceiver;
    
    pcl::resize_event_routine resizeHandler;
    void* resizeReceiver;
    
    pcl::move_event_routine moveHandler;
    void* moveReceiver;
    
    pcl::control_event_routine enterHandler;
    void* enterReceiver;
    
    pcl::control_event_routine leaveHandler;
    void* leaveReceiver;
    
    pcl::mouse_event_routine mouseMoveHandler;
    void* mouseMoveReceiver;
    
    pcl::mouse_button_event_routine mousePressHandler;
    void* mousePressReceiver;
    
    pcl::mouse_button_event_routine mouseReleaseHandler;
    void* mouseReleaseReceiver;
    
    pcl::keyboard_event_routine keyPressHandler;
    void* keyPressReceiver;
    
    pcl::keyboard_event_routine keyReleaseHandler;
    void* keyReleaseReceiver;
    
    pcl::wheel_event_routine wheelHandler;
    void* wheelReceiver;
    
    pcl::control_event_routine destroyHandler;
    void* destroyReceiver;
    
    pcl::control_event_routine showHandler;
    void* showReceiver;
    
    pcl::control_event_routine hideHandler;
    void* hideReceiver;
    
    pcl::control_event_routine closeHandler;
    void* closeReceiver;
    
    pcl::control_event_routine getFocusHandler;
    void* getFocusReceiver;
    
    pcl::control_event_routine loseFocusHandler;
    void* loseFocusReceiver;

    pcl::event_routine editCompletedHandler;
    void* editCompletedReceiver;

    MockTreeBox *parent;
   int flags;
    MockControl(QWidget* w = nullptr) 
        : widget(w ? w : new QWidget()),
          clientHandle(nullptr),
          layout(nullptr),
          paintHandler(nullptr), paintReceiver(nullptr),
          resizeHandler(nullptr), resizeReceiver(nullptr),
          moveHandler(nullptr), moveReceiver(nullptr),
          enterHandler(nullptr), enterReceiver(nullptr),
          leaveHandler(nullptr), leaveReceiver(nullptr),
          mouseMoveHandler(nullptr), mouseMoveReceiver(nullptr),
          mousePressHandler(nullptr), mousePressReceiver(nullptr),
          mouseReleaseHandler(nullptr), mouseReleaseReceiver(nullptr),
          keyPressHandler(nullptr), keyPressReceiver(nullptr),
          keyReleaseHandler(nullptr), keyReleaseReceiver(nullptr),
          wheelHandler(nullptr), wheelReceiver(nullptr),
          destroyHandler(nullptr), destroyReceiver(nullptr),
          showHandler(nullptr), showReceiver(nullptr),
          hideHandler(nullptr), hideReceiver(nullptr),
          closeHandler(nullptr), closeReceiver(nullptr),
          getFocusHandler(nullptr), getFocusReceiver(nullptr),
          loseFocusHandler(nullptr), loseFocusReceiver(nullptr),
	  editCompletedHandler(nullptr), editCompletedReceiver(nullptr)
    {
    }
    
    ~MockControl() {
        // Qt handles widget cleanup
    }
};

struct MockTreeNode
{
    std::vector<String>          text;
    std::vector<control_handle>  icon;
    std::vector<String>          tooltip;

    bool selected = false;

    explicit MockTreeNode( int columns )
    {
        text.resize( columns );
        icon.resize( columns, nullptr );
        tooltip.resize( columns );
    }
};

struct MockTreeBox : MockControl
{
    int columns = 1;
    std::vector<MockTreeNode*> nodes;

    control_handle viewport = nullptr;
    QTreeWidget* tree = nullptr;

    // Behavior flags we care about
    bool multipleSelection = false;
    bool uniformRowHeight = false;

    bool multipleSelections = false;
    bool rootDecoration     = false;
    bool alternateRowColor  = false;

    MockTreeBox() = default;

    ~MockTreeBox()
    {
        for ( MockTreeNode* n : nodes )
            delete n;
    }
};

static std::map<control_handle, MockTreeBox*> g_treebox_map;
static std::map<pcl::TreeBox::Node*, QTreeWidgetItem*> g_node_to_item;
static std::map<QTreeWidgetItem*, pcl::TreeBox::Node*> g_item_to_node;
static std::mutex g_treebox_mutex;

// Helpers
static MockTreeBox* GetMockTreeBox(const_control_handle hTree)
{
    if (!hTree)
        return nullptr;

    auto it = g_treebox_map.find(const_cast<control_handle>(hTree));
    if (it == g_treebox_map.end())
        return nullptr;
    return it->second;
}

static QTreeWidgetItem* ItemFromNodeHandle(const_api_handle hNode)
{
    if (!hNode)
        return nullptr;

    auto* node = reinterpret_cast<pcl::TreeBox::Node*>(
        const_cast<api_handle>(hNode));

    auto it = g_node_to_item.find(node);
    if (it == g_node_to_item.end())
        return nullptr;

    return it->second;
}

static pcl::TreeBox::Node* NodeFromItem(QTreeWidgetItem* item)
{
    if (!item)
        return nullptr;

    auto it = g_item_to_node.find(item);
    if (it == g_item_to_node.end())
        return nullptr;

    return it->second;
}

// Remove a subtree from the node maps.
static void RemoveItemSubtreeFromMaps(QTreeWidgetItem* item)
{
    if (!item)
        return;

    auto it = g_item_to_node.find(item);
    if (it != g_item_to_node.end())
    {
        pcl::TreeBox::Node* node = it->second;
        g_item_to_node.erase(it);
        g_node_to_item.erase(node);
    }

    const int childCount = item->childCount();
    for (int i = 0; i < childCount; ++i)
        RemoveItemSubtreeFromMaps(item->child(i));
}

// Logging settings
static bool g_debug_logging = false;
static std::ofstream g_log_file;

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

// Helper function to extract file extension
std::string GetFileExtension(const std::string& path) {
    size_t pos = path.find_last_of('.');
    if (pos != std::string::npos) {
        std::string ext = path.substr(pos);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext;
    }
    return "";
}

extern "C" {
// Log a debug message
void LogDebug(const std::string& message) {
    if (!g_debug_logging) return;
    
    if (g_log_file.is_open()) {
        g_log_file << message << std::endl;
    } else {
        std::cout << "[PCLMockAPI] " << message << std::endl;
    }
    
};

static inline void LogDbg(const std::string& msg) {
    LogDebug(msg.c_str());
}

static inline void LogDbg(const pcl::String& msg) {
    LogDebug(msg.ToUTF8().c_str());
}

static inline void LogDbg(const QString& msg) {
  //    LogDebug(msg);
}

inline void LogDbg(const char* msg) {
    LogDebug(msg);
}

// ----------------------------------------------------------------------------
// Custom Event-Aware Widget
// ----------------------------------------------------------------------------

class PCLWidget : public QWidget {
  Q_OBJECT
public:
    control_handle controlHandle;
    MockControl* mockControl;
    
    PCLWidget(control_handle handle, MockControl* ctrl, QWidget* parent = nullptr)
        : QWidget(parent), controlHandle(handle), mockControl(ctrl)
    {
    }
    
protected:
    void paintEvent(QPaintEvent* event) override {
        QWidget::paintEvent(event);
        
        if (mockControl && mockControl->paintHandler && mockControl->paintReceiver) {
            mockControl->paintHandler(mockControl->paintReceiver, controlHandle,
                event->rect().x(), event->rect().y(),
                event->rect().width(), event->rect().height());
        }
    }
    
    void resizeEvent(QResizeEvent* event) override {
        QWidget::resizeEvent(event);
        
        if (mockControl && mockControl->resizeHandler && mockControl->resizeReceiver) {
            mockControl->resizeHandler(mockControl->resizeReceiver, controlHandle,
                event->size().width(), event->size().height(),
                event->oldSize().width(), event->oldSize().height());
        }
    }
    
    void moveEvent(QMoveEvent* event) override {
        QWidget::moveEvent(event);
        
        if (mockControl && mockControl->moveHandler && mockControl->moveReceiver) {
            mockControl->moveHandler(mockControl->moveReceiver, controlHandle,
                event->pos().x(), event->pos().y(),
                event->oldPos().x(), event->oldPos().y());
        }
    }
    
    void enterEvent(QEvent* event) override {
        QWidget::enterEvent(event);
        
        if (mockControl && mockControl->enterHandler && mockControl->enterReceiver) {
            mockControl->enterHandler(mockControl->enterReceiver, controlHandle);
        }
    }
    
    void leaveEvent(QEvent* event) override {
        QWidget::leaveEvent(event);
        
        if (mockControl && mockControl->leaveHandler && mockControl->leaveReceiver) {
            mockControl->leaveHandler(mockControl->leaveReceiver, controlHandle);
        }
    }
    
    void mouseMoveEvent(QMouseEvent* event) override {
        QWidget::mouseMoveEvent(event);
        
        if (mockControl && mockControl->mouseMoveHandler && mockControl->mouseMoveReceiver) {
            mockControl->mouseMoveHandler(mockControl->mouseMoveReceiver, controlHandle,
                x(), y(),
                static_cast<uint32_t>(event->buttons()),
                static_cast<uint32_t>(event->modifiers()));
        }
    }
    
    void mousePressEvent(QMouseEvent* event) override {
        QWidget::mousePressEvent(event);
        
        if (mockControl && mockControl->mousePressHandler && mockControl->mousePressReceiver) {
            mockControl->mousePressHandler(mockControl->mousePressReceiver, controlHandle,
                event->x(), event->y(),
                static_cast<uint32_t>(event->button()),
                static_cast<uint32_t>(event->buttons()),
                static_cast<uint32_t>(event->modifiers()));
        }
    }
    
    void mouseReleaseEvent(QMouseEvent* event) override {
        QWidget::mouseReleaseEvent(event);
        
        if (mockControl && mockControl->mouseReleaseHandler && mockControl->mouseReleaseReceiver) {
            mockControl->mouseReleaseHandler(mockControl->mouseReleaseReceiver, controlHandle,
                event->x(), event->y(),
                static_cast<uint32_t>(event->button()),
                static_cast<uint32_t>(event->buttons()),
                static_cast<uint32_t>(event->modifiers()));
        }
    }
    
    void keyPressEvent(QKeyEvent* event) override {
        QWidget::keyPressEvent(event);
        
        if (mockControl && mockControl->keyPressHandler && mockControl->keyPressReceiver) {
            mockControl->keyPressHandler(mockControl->keyPressReceiver, controlHandle,
                event->key(),
                static_cast<uint32_t>(event->modifiers()));
        }
    }
    
    void keyReleaseEvent(QKeyEvent* event) override {
        QWidget::keyReleaseEvent(event);
        
        if (mockControl && mockControl->keyReleaseHandler && mockControl->keyReleaseReceiver) {
            mockControl->keyReleaseHandler(mockControl->keyReleaseReceiver, controlHandle,
                event->key(),
                static_cast<uint32_t>(event->modifiers()));
        }
    }
    
    void wheelEvent(QWheelEvent* event) override {
        QWidget::wheelEvent(event);
        
        if (mockControl && mockControl->wheelHandler && mockControl->wheelReceiver) {
            mockControl->wheelHandler(mockControl->wheelReceiver, controlHandle,
		x(), y(),
                event->angleDelta().y(),  // Single delta value		      
                static_cast<uint32_t>(event->buttons()),
                static_cast<uint32_t>(event->modifiers()));
        }
    }
    
    void showEvent(QShowEvent* event) override {
        QWidget::showEvent(event);
        
        if (mockControl && mockControl->showHandler && mockControl->showReceiver) {
            mockControl->showHandler(mockControl->showReceiver, controlHandle);
        }
    }
    
    void hideEvent(QHideEvent* event) override {
        QWidget::hideEvent(event);
        
        if (mockControl && mockControl->hideHandler && mockControl->hideReceiver) {
            mockControl->hideHandler(mockControl->hideReceiver, controlHandle);
        }
    }
    
    void closeEvent(QCloseEvent* event) override {
        if (mockControl && mockControl->closeHandler && mockControl->closeReceiver) {
            mockControl->closeHandler(mockControl->closeReceiver, controlHandle);
        }
        
        QWidget::closeEvent(event);
    }
    
    void focusInEvent(QFocusEvent* event) override {
        QWidget::focusInEvent(event);
        
        if (mockControl && mockControl->getFocusHandler && mockControl->getFocusReceiver) {
            mockControl->getFocusHandler(mockControl->getFocusReceiver, controlHandle);
        }
    }
    
    void focusOutEvent(QFocusEvent* event) override {
        QWidget::focusOutEvent(event);
        
        if (mockControl && mockControl->loseFocusHandler && mockControl->loseFocusReceiver) {
            mockControl->loseFocusHandler(mockControl->loseFocusReceiver, controlHandle);
        }
    }
};

class ControlEventFilter : public QObject {
    Q_OBJECT
    
public:
    control_handle controlHandle;
    MockControl* mockControl;
    
    ControlEventFilter(control_handle handle, MockControl* ctrl, QObject* parent = nullptr)
        : QObject(parent), controlHandle(handle), mockControl(ctrl)
    {
    }
    
protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
	if (!mockControl) {
	    // No mock control, just pass through
	    return QObject::eventFilter(obj, event);
	}

        switch (event->type()) {
            case QEvent::Paint:
                if (mockControl->paintHandler && mockControl->paintReceiver) {
                    QPaintEvent* pe = static_cast<QPaintEvent*>(event);
                    mockControl->paintHandler(mockControl->paintReceiver, controlHandle,
                        pe->rect().x(), pe->rect().y(), 
                        pe->rect().width(), pe->rect().height());
                    return false; // Let Qt handle default painting too
                }
                break;
                
            case QEvent::Resize:
                if (mockControl->resizeHandler && mockControl->resizeReceiver) {
                    QResizeEvent* re = static_cast<QResizeEvent*>(event);
                    mockControl->resizeHandler(mockControl->resizeReceiver, controlHandle,
                        re->size().width(), re->size().height(),
                        re->oldSize().width(), re->oldSize().height());
                }
                break;
                
            case QEvent::Enter:
                if (mockControl->enterHandler && mockControl->enterReceiver) {
                    mockControl->enterHandler(mockControl->enterReceiver, controlHandle);
                }
                break;
                
            case QEvent::Leave:
                if (mockControl->leaveHandler && mockControl->leaveReceiver) {
                    mockControl->leaveHandler(mockControl->leaveReceiver, controlHandle);
                }
                break;
                
            case QEvent::MouseMove:
                if (mockControl->mouseMoveHandler && mockControl->mouseMoveReceiver) {
                    QMouseEvent* me = static_cast<QMouseEvent*>(event);
                    mockControl->mouseMoveHandler(mockControl->mouseMoveReceiver, controlHandle,
                        me->x(), me->y(),
                        static_cast<uint32>(me->buttons()),
                        static_cast<uint32>(me->modifiers()));
                }
                break;
                
            case QEvent::MouseButtonPress:
                if (mockControl->mousePressHandler && mockControl->mousePressReceiver) {
                    QMouseEvent* me = static_cast<QMouseEvent*>(event);
                    mockControl->mousePressHandler(mockControl->mousePressReceiver, controlHandle,
                        me->x(), me->y(),
                        static_cast<uint32>(me->button()),
                        static_cast<uint32>(me->buttons()),
                        static_cast<uint32>(me->modifiers()));
                }
                break;
                
            case QEvent::MouseButtonRelease:
                if (mockControl->mouseReleaseHandler && mockControl->mouseReleaseReceiver) {
                    QMouseEvent* me = static_cast<QMouseEvent*>(event);
                    mockControl->mouseReleaseHandler(mockControl->mouseReleaseReceiver, controlHandle,
                        me->x(), me->y(),
                        static_cast<uint32>(me->button()),
                        static_cast<uint32>(me->buttons()),
                        static_cast<uint32>(me->modifiers()));
                }
                break;
                
            case QEvent::KeyPress:
                if (mockControl->keyPressHandler && mockControl->keyPressReceiver) {
                    QKeyEvent* ke = static_cast<QKeyEvent*>(event);
                    mockControl->keyPressHandler(mockControl->keyPressReceiver, controlHandle,
                        ke->key(),
			static_cast<uint32>(ke->modifiers()));
                }
                break;
                
            case QEvent::KeyRelease:
                if (mockControl->keyReleaseHandler && mockControl->keyReleaseReceiver) {
                    QKeyEvent* ke = static_cast<QKeyEvent*>(event);
                    mockControl->keyReleaseHandler(mockControl->keyReleaseReceiver, controlHandle,
                        ke->key(),
		        static_cast<uint32>(ke->modifiers()));
                }
                break;
                
            case QEvent::Wheel:
                if (mockControl->wheelHandler && mockControl->wheelReceiver) {
                    QWheelEvent* we = static_cast<QWheelEvent*>(event);
                    mockControl->wheelHandler(mockControl->wheelReceiver, controlHandle,
                        we->position().x(), we->position().y(),
                        we->angleDelta().y(),
                        static_cast<uint32>(we->buttons()),
                        static_cast<uint32>(we->modifiers()));
                }
                break;
                
            default:
                break;
        }
        
        return QObject::eventFilter(obj, event);
    }
};

// ----------------------------------------------------------------------------
// Helper to replace standard widget with event-aware widget
// ----------------------------------------------------------------------------

static void EnableEvents(control_handle handle, MockControl* ctrl) {
    if (!ctrl) {
        LogDebug("EnableEvents: null ctrl");
        return;
    }
    
    if (!ctrl->widget) {
        LogDebug("EnableEvents: null widget - this shouldn't happen");
        return;
    }
    
    // Check if we already have an event filter installed
    QObjectList children = ctrl->widget->children();
    for (QObject* child : children) {
        if (ControlEventFilter* existingFilter = dynamic_cast<ControlEventFilter*>(child)) {
            LogDebug("EnableEvents: event filter already exists, updating");
            existingFilter->mockControl = ctrl;
            existingFilter->controlHandle = handle;
            return;
        }
    }
    
    LogDebug("EnableEvents: installing new event filter on widget");
    
    // Create and install an event filter
    // Set the widget as parent so it gets cleaned up automatically
    ControlEventFilter* filter = new ControlEventFilter(handle, ctrl, ctrl->widget);
    ctrl->widget->installEventFilter(filter);
    
    LogDebug("EnableEvents: event filter installed successfully");
}

// ----------------------------------------------------------------------------
// Event Handler Registration Functions
// ----------------------------------------------------------------------------

// Global map to track all controls
static std::map<const_control_handle, MockControl*> g_control_map;
static std::mutex g_control_map_mutex;

QWidget* FindInterfaceGuiRoot()
{
    QWidget* root = nullptr;

    for (auto& pair : g_control_map)
    {
        MockControl* mc = pair.second;
        QWidget* w = mc->widget;
        QWidget* parent = w->parentWidget();

        bool parentIsControl = false;

        // Check whether the parent is one of our mock controls.
        for (auto& other : g_control_map)
        {
            if (other.second->widget == parent)
            {
                parentIsControl = true;
                break;
            }
        }

        if (!parentIsControl)
        {
            // This one is the root
            root = w;
            break;
        }
    }

    return root;
}
  
api_bool API_Control_SetDestroyEventRoutine(control_handle handle, api_handle receiver,
                                            pcl::control_event_routine handler)
{
    LogDbg("SetDestroyEventRoutine called");
    
    std::lock_guard<std::mutex> lock(g_control_map_mutex);
    auto it = g_control_map.find(handle);
    if (it == g_control_map.end()) return api_false;
    
    MockControl* ctrl = it->second;
    ctrl->destroyHandler = handler;
    ctrl->destroyReceiver = receiver;
    
    if (handler) {
        QObject::connect(ctrl->widget, &QObject::destroyed, [ctrl, handle]() {
            if (ctrl->destroyHandler && ctrl->destroyReceiver) {
                ctrl->destroyHandler(ctrl->destroyReceiver, handle);
            }
        });
    }
    
    return api_true;
}

api_bool API_Control_SetShowEventRoutine(control_handle handle, api_handle receiver,
                                         pcl::control_event_routine handler)
{
    LogDbg("SetShowEventRoutine called");
    
    std::lock_guard<std::mutex> lock(g_control_map_mutex);
    auto it = g_control_map.find(handle);
    if (it == g_control_map.end()) return api_false;
    
    MockControl* ctrl = it->second;
    ctrl->showHandler = handler;
    ctrl->showReceiver = receiver;
    
    EnableEvents(handle, ctrl);
    return api_true;
}

api_bool API_Control_SetHideEventRoutine(control_handle handle, api_handle receiver,
                                         pcl::control_event_routine handler)
{
    LogDbg("SetHideEventRoutine called");
    
    std::lock_guard<std::mutex> lock(g_control_map_mutex);
    auto it = g_control_map.find(handle);
    if (it == g_control_map.end()) return api_false;
    
    MockControl* ctrl = it->second;
    ctrl->hideHandler = handler;
    ctrl->hideReceiver = receiver;
    
    EnableEvents(handle, ctrl);
    return api_true;
}

api_bool API_Control_SetCloseEventRoutine(control_handle handle, api_handle receiver,
                                          pcl::control_event_routine handler)
{
    LogDbg("SetCloseEventRoutine called");
    
    std::lock_guard<std::mutex> lock(g_control_map_mutex);
    auto it = g_control_map.find(handle);
    if (it == g_control_map.end()) return api_false;
    
    MockControl* ctrl = it->second;
    ctrl->closeHandler = handler;
    ctrl->closeReceiver = receiver;
    
    EnableEvents(handle, ctrl);
    return api_true;
}

api_bool API_Control_SetGetFocusEventRoutine(
        control_handle handle,
        api_handle receiver,
        pcl::control_event_routine handler)
{
    LogDbg("SetGetFocusEventRoutine called");

    if (!handle)
        return api_false;

    std::lock_guard<std::mutex> lock(g_control_map_mutex);

    MockControl* ctrl = nullptr;
    auto it = g_control_map.find(handle);

    if (it == g_control_map.end()) {
        // Create new MockControl for existing QWidget*
        QWidget* widget = reinterpret_cast<QWidget*>(handle);
        ctrl = new MockControl(widget);
        g_control_map[handle] = ctrl;
        LogDebug("SetGetFocusEventRoutine: created new MockControl");
    } else {
        ctrl = it->second;
    }

    ctrl->getFocusHandler  = handler;
    ctrl->getFocusReceiver = receiver;

    EnableEvents(handle, ctrl);
    return api_true;
}

api_bool API_Control_SetLoseFocusEventRoutine(control_handle handle, api_handle receiver,
                                              pcl::control_event_routine handler)
{
    LogDbg("SetLoseFocusEventRoutine called");
    
    std::lock_guard<std::mutex> lock(g_control_map_mutex);
    auto it = g_control_map.find(handle);
    if (it == g_control_map.end()) return api_false;
    
    MockControl* ctrl = it->second;
    ctrl->loseFocusHandler = handler;
    ctrl->loseFocusReceiver = receiver;
    
    EnableEvents(handle, ctrl);
    return api_true;
}

api_bool API_Control_SetEnterEventRoutine(control_handle handle, api_handle receiver,
                                          pcl::control_event_routine handler)
{
    LogDbg("SetEnterEventRoutine called");
    /*    
    std::lock_guard<std::mutex> lock(g_control_map_mutex);
    auto it = g_control_map.find(handle);
    if (it == g_control_map.end()) return api_false;
    
    MockControl* ctrl = it->second;
    ctrl->enterHandler = handler;
    ctrl->enterReceiver = receiver;
    
    EnableEvents(handle, ctrl);
    */
    return api_true;
}

api_bool API_Control_SetLeaveEventRoutine(control_handle handle, api_handle receiver,
                                          pcl::control_event_routine handler)
{
    LogDbg("SetLeaveEventRoutine called");
    /*    
    std::lock_guard<std::mutex> lock(g_control_map_mutex);
    auto it = g_control_map.find(handle);
    if (it == g_control_map.end()) return api_false;
    
    MockControl* ctrl = it->second;
    ctrl->leaveHandler = handler;
    ctrl->leaveReceiver = receiver;
    
    EnableEvents(handle, ctrl);
    */
    return api_true;
}

api_bool API_Control_SetMoveEventRoutine(control_handle handle, api_handle receiver,
                                         pcl::move_event_routine handler)
{
    LogDbg("SetMoveEventRoutine called");
    
    std::lock_guard<std::mutex> lock(g_control_map_mutex);
    auto it = g_control_map.find(handle);
    if (it == g_control_map.end()) return api_false;
    
    MockControl* ctrl = it->second;
    ctrl->moveHandler = handler;
    ctrl->moveReceiver = receiver;
    
    EnableEvents(handle, ctrl);
    return api_true;
}

api_bool API_Control_SetResizeEventRoutine(control_handle handle, api_handle receiver,
                                           pcl::resize_event_routine handler)
{
    LogDbg("SetResizeEventRoutine called");
    
    std::lock_guard<std::mutex> lock(g_control_map_mutex);
    auto it = g_control_map.find(handle);
    if (it == g_control_map.end()) return api_false;
    
    MockControl* ctrl = it->second;
    ctrl->resizeHandler = handler;
    ctrl->resizeReceiver = receiver;
    
    EnableEvents(handle, ctrl);
    return api_true;
}

api_bool API_Control_SetPaintEventRoutine(control_handle handle, api_handle receiver,
                                          pcl::paint_event_routine handler)
{
    LogDbg("SetPaintEventRoutine called");
    
    std::lock_guard<std::mutex> lock(g_control_map_mutex);
    auto it = g_control_map.find(handle);
    if (it == g_control_map.end()) return api_false;
    
    MockControl* ctrl = it->second;
    ctrl->paintHandler = handler;
    ctrl->paintReceiver = receiver;
    
    EnableEvents(handle, ctrl);
    return api_true;
}

api_bool API_Control_SetKeyPressEventRoutine(control_handle handle, api_handle receiver,
                                             pcl::keyboard_event_routine handler)
{
    LogDebug("SetKeyPressEventRoutine called");
    
    if (!handle) {
        LogDebug("SetKeyPressEventRoutine: null handle");
        return api_false;
    }
    
    std::lock_guard<std::mutex> lock(g_control_map_mutex);
    
    // Get or create MockControl
    MockControl* ctrl = nullptr;
    auto it = g_control_map.find(handle);
    if (it == g_control_map.end()) {
        // Create new MockControl for existing widget
        QWidget* widget = reinterpret_cast<QWidget*>(handle);
        ctrl = new MockControl(widget);
        g_control_map[handle] = ctrl;
        LogDebug("SetKeyPressEventRoutine: created new MockControl");
    } else {
        ctrl = it->second;
    }
    
    ctrl->keyPressHandler = handler;
    ctrl->keyPressReceiver = receiver;
    
    EnableEvents(handle, ctrl);
    return api_true;
}

api_bool API_Control_SetKeyReleaseEventRoutine(control_handle handle, api_handle receiver,
                                               pcl::keyboard_event_routine handler)
{
    LogDebug("SetKeyReleaseEventRoutine called");
    
    if (!handle) {
        LogDebug("SetKeyReleaseEventRoutine: null handle");
        return api_false;
    }
    
    std::lock_guard<std::mutex> lock(g_control_map_mutex);
    
    // Get or create MockControl
    MockControl* ctrl = nullptr;
    auto it = g_control_map.find(handle);
    if (it == g_control_map.end()) {
        // Create new MockControl for existing widget
        QWidget* widget = reinterpret_cast<QWidget*>(handle);
        ctrl = new MockControl(widget);
        g_control_map[handle] = ctrl;
        LogDebug("SetKeyReleaseEventRoutine: created new MockControl");
    } else {
        ctrl = it->second;
    }
    
    ctrl->keyReleaseHandler = handler;
    ctrl->keyReleaseReceiver = receiver;
    
    EnableEvents(handle, ctrl);
    return api_true;
}

api_bool API_Control_SetMouseMoveEventRoutine(control_handle handle, api_handle receiver,
                                              pcl::mouse_event_routine handler)
{
    LogDbg("SetMouseMoveEventRoutine called");
    
    std::lock_guard<std::mutex> lock(g_control_map_mutex);
    auto it = g_control_map.find(handle);
    if (it == g_control_map.end()) return api_false;
    
    MockControl* ctrl = it->second;
    ctrl->mouseMoveHandler = handler;
    ctrl->mouseMoveReceiver = receiver;
    
    EnableEvents(handle, ctrl);
    return api_true;
}

api_bool API_Control_SetMouseDoubleClickEventRoutine(control_handle handle, api_handle receiver,
                                                     pcl::mouse_event_routine handler)
{
    LogDbg("SetMouseDoubleClickEventRoutine called");
    // TODO: Add double-click handler support
    return api_true;
}

api_bool API_Control_SetMousePressEventRoutine(control_handle handle, api_handle receiver,
                                               pcl::mouse_button_event_routine handler)
{
    LogDbg("SetMousePressEventRoutine called");
    
    if (!handle) {
        LogDebug("SetMousePressEventRoutine: null handle");
        return api_false;
    }
    
    std::lock_guard<std::mutex> lock(g_control_map_mutex);
    
    // Get or create MockControl
    MockControl* ctrl = nullptr;
    auto it = g_control_map.find(handle);
    if (it == g_control_map.end()) {
        // Create new MockControl for existing widget
        QWidget* widget = reinterpret_cast<QWidget*>(handle);
        ctrl = new MockControl(widget);
        g_control_map[handle] = ctrl;
        LogDebug("SetMousePressEventRoutine: created new MockControl");
    } else {
        ctrl = it->second;
    }
    
    ctrl->mousePressHandler = handler;
    ctrl->mousePressReceiver = receiver;
    
    EnableEvents(handle, ctrl);
    return api_true;
}

api_bool API_Control_SetMouseReleaseEventRoutine(control_handle handle, api_handle receiver,
                                                 pcl::mouse_button_event_routine handler)
{
    LogDbg("SetMouseReleaseEventRoutine called");
    
    std::lock_guard<std::mutex> lock(g_control_map_mutex);
    auto it = g_control_map.find(handle);
    if (it == g_control_map.end()) return api_false;
    
    MockControl* ctrl = it->second;
    ctrl->mouseReleaseHandler = handler;
    ctrl->mouseReleaseReceiver = receiver;
    
    EnableEvents(handle, ctrl);
    return api_true;
}

api_bool API_Control_SetWheelEventRoutine(control_handle handle, api_handle receiver,
                                          pcl::wheel_event_routine handler)
{
    LogDbg("SetWheelEventRoutine called");
    
    std::lock_guard<std::mutex> lock(g_control_map_mutex);
    auto it = g_control_map.find(handle);
    if (it == g_control_map.end()) return api_false;
    
    MockControl* ctrl = it->second;
    ctrl->wheelHandler = handler;
    ctrl->wheelReceiver = receiver;
    
    EnableEvents(handle, ctrl);
    return api_true;
}

// Stub implementations for drag/drop
api_bool API_Control_SetFileDragEventRoutine(control_handle, api_handle, pcl::file_drag_event_handler)
{
    return api_true;
}

api_bool API_Control_SetFileDropEventRoutine(control_handle, api_handle, pcl::file_drag_event_handler)
{
    return api_true;
}

api_bool API_Control_SetViewDragEventRoutine(control_handle, api_handle, pcl::view_drag_event_handler)
{
    return api_true;
}

api_bool API_Control_SetViewDropEventRoutine(control_handle, api_handle, pcl::view_drag_event_handler)
{
    return api_true;
}

api_bool API_Control_SetChildCreateEventRoutine(control_handle, api_handle, pcl::child_event_routine)
{
    return api_true;
}

api_bool API_Control_SetChildDestroyEventRoutine(control_handle, api_handle, pcl::child_event_routine)
{
    return api_true;
}

// ----------------------------------------------------------------------------
// Event Filter for Control Events
// ----------------------------------------------------------------------------

// Image handling structure to keep track of created images
struct MockImage {
    uint32_t width;
    uint32_t height;
    uint32_t channels;
    uint32_t bitsPerSample;
    bool isFloat;
    uint32_t colorSpace;
    void** pixelData;    // Array of channel pointers
    double* stats;       // Min/max values per channel
};

// Simple structure for tracking open files
struct MockFileInstance {
    std::string path;
    std::string extension;
    uint32_t selectedImage; // index of selected image
    std::vector<MockImage*> images; // images in the file
};

// Mock PixInsight version information
static uint32_t s_major = 1;
static uint32_t s_minor = 8;
static uint32_t s_release = 9;
static uint32_t s_revision = 1425;
static uint32_t s_beta = 0;
static uint32_t s_confidential = 0;
static uint32_t s_le = 0;
static char16_type s_language[64] = { 'e', 'n', 'g', 0 };
static char16_type s_codename[64] = { 'C', 'l', 'o', 'u', 'd', ' ', 'N', 'i', 'n', 'e', 0 };
  
// Map to store file format instances
static std::map<file_format_handle, MockFileInstance*> g_file_instances;
static std::mutex g_file_instances_mutex;

// Global module handle
static void* g_module_handle = nullptr;
// At top of PCLMockAPI.cpp
static control_handle g_lastTopLevelControl = nullptr;

// Function mapping
static std::map<std::string, void*> g_function_map;
static std::mutex g_function_map_mutex;
static std::map<image_handle, MockImage*> g_image_map;
static std::mutex g_image_map_mutex;
static thread_local bool g_constructing_gui = true;

// Create a stub function for missing functions
// We'll use a simple global function that just returns nullptr
static void* unimplemented_function(void) {
        LogDebug("Called unimplemented function");
	abort();
        return nullptr;
}
      
// Function resolver implementation
void* mock_function_resolver(const char* name) {
    if (!name) return nullptr;
    
    std::string func_name = name;
    LogDebug("Resolving function: " + func_name);
    
    std::lock_guard<std::mutex> lock(g_function_map_mutex);
    auto it = g_function_map.find(func_name);
    if (it != g_function_map.end()) {
        LogDebug("Found implementation for: " + func_name);
        return it->second;
    }
    
    // Create a default handler for missing functions
    LogDebug("No implementation found for: " + func_name);
    
    // Store this handler so we don't create a new one each time
    g_function_map[func_name] = (void*)unimplemented_function;
    
    return g_function_map[func_name];
}

// Structure to store FFT transform information
struct FFTTransform {
    int size;
    bool isReal;
    bool isDouble;
    void* buffer;  // To simulate memory allocation
    
    FFTTransform(int n, bool real, bool dbl) : 
        size(n), isReal(real), isDouble(dbl) {
        // Allocate some memory to simulate FFT buffer
        size_t bufferSize = n * (isReal ? 1 : 2) * (isDouble ? sizeof(double) : sizeof(float));
        buffer = malloc(bufferSize);
    }
    
    ~FFTTransform() {
        if (buffer) {
            free(buffer);
            buffer = nullptr;
        }
    }
};

// Map to keep track of created FFT transforms
static std::map<void*, FFTTransform*> g_fft_transforms;
static std::mutex g_fft_mutex;

// Typedef for the FFT transform function signature
typedef api_bool (*fft_real_transform_d_func)(void* handle, dcomplex* y, const double* x);

// Structure to store FFTW plan information
struct FFTWPlanWrapper {
    int size;
    bool isReal;
    bool forward;
    
    // FFTW plans
    fftw_plan forwardPlan;
    fftw_plan inversePlan;
    
    // Buffers for real transforms
    double* realIn;
    double* realOut;
    fftw_complex* complexIn;
    fftw_complex* complexOut;
    
    FFTWPlanWrapper(int n, bool real) : 
        size(n), isReal(real), forward(true) {
        
        if (isReal) {
            // Real-to-complex and complex-to-real transforms
            realIn = (double*)fftw_malloc(sizeof(double) * n);
            realOut = (double*)fftw_malloc(sizeof(double) * n);
            complexOut = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * (n/2 + 1));
            complexIn = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * (n/2 + 1));
            
            // Create plans
            forwardPlan = fftw_plan_dft_r2c_1d(n, realIn, complexOut, FFTW_MEASURE);
            inversePlan = fftw_plan_dft_c2r_1d(n, complexIn, realOut, FFTW_MEASURE);
        } else {
            // Complex-to-complex transforms
            complexIn = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * n);
            complexOut = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * n);
            realIn = nullptr;
            realOut = nullptr;
            
            // Create plans
            forwardPlan = fftw_plan_dft_1d(n, complexIn, complexOut, FFTW_FORWARD, FFTW_MEASURE);
            inversePlan = fftw_plan_dft_1d(n, complexIn, complexOut, FFTW_BACKWARD, FFTW_MEASURE);
        }
    }
    
    ~FFTWPlanWrapper() {
        // Destroy plans
        fftw_destroy_plan(forwardPlan);
        fftw_destroy_plan(inversePlan);
        
        // Free buffers
        if (realIn) fftw_free(realIn);
        if (realOut) fftw_free(realOut);
        if (complexIn) fftw_free(complexIn);
        if (complexOut) fftw_free(complexOut);
    }
};

// Map to keep track of created FFT transforms
static std::map<void*, FFTWPlanWrapper*> g_fftw_plans;
static std::mutex g_fftw_mutex;

// Initialize FFTW library
void InitializeFFTW() {
    static bool initialized = false;
    if (!initialized) {
        fftw_init_threads();
        fftw_plan_with_nthreads(4); // Use multiple threads for better performance
        initialized = true;
    }
}

// Clean up FFTW resources
void CleanupFFTW() {
    std::lock_guard<std::mutex> lock(g_fftw_mutex);
    for (auto& pair : g_fftw_plans) {
        delete pair.second;
    }
    g_fftw_plans.clear();
    fftw_cleanup_threads();
    fftw_cleanup();
}

//======================= PCL API FUNCTIONS =======================

// Returns the next power of 2 greater than or equal to n
int API_Numerical_FFTRealOptimizedLengthF(int n) {
    LogDebug("FFTRealOptimizedLengthF called with n=" + std::to_string(n));
    
    // Ensure we never return a value smaller than the input
    if (n <= 0) return 1;
    
    // FFTW works best with sizes that are products of small primes
    // For simplicity, we'll use powers of 2, but FFTW actually works well with many sizes
    int result = 1;
    while (result < n) {
        result *= 2;
    }
    
    LogDebug("FFTRealOptimizedLengthF returning " + std::to_string(result));
    return result;
}

// Complex FFT optimized length (similar implementation)
int API_Numerical_FFTComplexOptimizedLengthF(int n) {
    return API_Numerical_FFTRealOptimizedLengthF(n);
}

// Create a real transform
void* API_Numerical_FFTCreateRealTransformD(int n) {
    LogDebug("FFTCreateRealTransformD called with n=" + std::to_string(n));
    
    InitializeFFTW();
    
    // Create a new transform wrapper
    FFTWPlanWrapper* wrapper = new FFTWPlanWrapper(n, true);
    void* handle = wrapper;  // Use the pointer as the handle
    
    // Store in our map
    std::lock_guard<std::mutex> lock(g_fftw_mutex);
    g_fftw_plans[handle] = wrapper;
    
    LogDebug("FFTCreateRealTransformD returning handle " + std::to_string((uintptr_t)handle));
    return handle;
}

// Create a complex transform
void* API_Numerical_FFTCreateComplexTransformD(int n) {
    LogDebug("FFTCreateComplexTransformD called with n=" + std::to_string(n));
    
    InitializeFFTW();
    
    // Create a new transform wrapper
    FFTWPlanWrapper* wrapper = new FFTWPlanWrapper(n, false);
    void* handle = wrapper;  // Use the pointer as the handle
    
    // Store in our map
    std::lock_guard<std::mutex> lock(g_fftw_mutex);
    g_fftw_plans[handle] = wrapper;
    
    LogDebug("FFTCreateComplexTransformD returning handle " + std::to_string((uintptr_t)handle));
    return handle;
}

// Create a complex inverse transform
void* API_Numerical_FFTCreateComplexInverseTransformD(int n) {
    LogDebug("FFTCreateComplexInverseTransformD called with n=" + std::to_string(n));
    
    InitializeFFTW();
    
    // Create a new transform wrapper
    FFTWPlanWrapper* wrapper = new FFTWPlanWrapper(n, false);
    wrapper->forward = false;  // Mark this as an inverse transform
    void* handle = wrapper;    // Use the pointer as the handle
    
    // Store in our map
    std::lock_guard<std::mutex> lock(g_fftw_mutex);
    g_fftw_plans[handle] = wrapper;
    
    LogDebug("FFTCreateComplexInverseTransformD returning handle " + std::to_string((uintptr_t)handle));
    return handle;
}

// Destroy a transform
void API_Numerical_FFTDestroyTransform(void* handle) {
    if (!handle) {
        return;
    }
    
    LogDebug("FFTDestroyTransform called with handle " + std::to_string((uintptr_t)handle));
    
    std::lock_guard<std::mutex> lock(g_fftw_mutex);
    auto it = g_fftw_plans.find(handle);
    if (it != g_fftw_plans.end()) {
        delete it->second;
        g_fftw_plans.erase(it);
    }
}

// Forward real transform (real to complex)
api_bool API_Numerical_FFTRealTransformD(void* handle, dcomplex* y, const double* x) {
    if (!handle || !y || !x) return api_false;
    
    LogDebug("FFTRealTransformD called with handle " + std::to_string((uintptr_t)handle));
    
    std::lock_guard<std::mutex> lock(g_fftw_mutex);
    auto it = g_fftw_plans.find(handle);
    if (it == g_fftw_plans.end() || !it->second->isReal) {
        LogDebug("FFTRealTransformD: Invalid handle or not real transform");
        return api_false;
    }
    
    FFTWPlanWrapper* wrapper = it->second;
    int n = wrapper->size;
    
    // Copy input data to FFTW buffer
    memcpy(wrapper->realIn, x, sizeof(double) * n);
    
    // Execute forward plan
    fftw_execute(wrapper->forwardPlan);
    
    // Copy results to output array in PCL dcomplex format
    for (int i = 0; i < n/2 + 1; i++) {
        y[i] = dcomplex(wrapper->complexOut[i][0], wrapper->complexOut[i][1]);
    }
    
    return api_true;
}

// Inverse real transform (complex to real)
api_bool API_Numerical_FFTInverseRealTransformD(void* handle, double* y, const dcomplex* x) {
    if (!handle || !y || !x) return api_false;
    
    LogDebug("FFTInverseRealTransformD called with handle " + std::to_string((uintptr_t)handle));
    
    std::lock_guard<std::mutex> lock(g_fftw_mutex);
    auto it = g_fftw_plans.find(handle);
    if (it == g_fftw_plans.end() || !it->second->isReal) {
        LogDebug("FFTInverseRealTransformD: Invalid handle or not real transform");
        return api_false;
    }
    
    FFTWPlanWrapper* wrapper = it->second;
    int n = wrapper->size;
    
    // Copy input data to FFTW buffer
    for (int i = 0; i < n/2 + 1; i++) {
        wrapper->complexIn[i][0] = x[i].Real();
        wrapper->complexIn[i][1] = x[i].Imag();
    }
    
    // Execute inverse plan
    fftw_execute(wrapper->inversePlan);
    
    // Copy results to output array
    // FFTW unnormalized results need to be divided by n
    memcpy(y, wrapper->realOut, sizeof(double) * n);
    
    // Normalize (FFTW doesn't normalize automatically)
    for (int i = 0; i < n; i++) {
        y[i] /= n;
    }
    
    return api_true;
}

// Forward complex transform (complex to complex)
api_bool API_Numerical_FFTComplexTransformD(void* handle, dcomplex* y, const dcomplex* x) {
    if (!handle || !y || !x) return api_false;
    
    LogDebug("FFTComplexTransformD called with handle " + std::to_string((uintptr_t)handle));
    
    std::lock_guard<std::mutex> lock(g_fftw_mutex);
    auto it = g_fftw_plans.find(handle);
    if (it == g_fftw_plans.end() || it->second->isReal) {
        LogDebug("FFTComplexTransformD: Invalid handle or not complex transform");
        return api_false;
    }
    
    FFTWPlanWrapper* wrapper = it->second;
    int n = wrapper->size;
    
    // Copy input data to FFTW buffer
    for (int i = 0; i < n; i++) {
        wrapper->complexIn[i][0] = x[i].Real();
        wrapper->complexIn[i][1] = x[i].Imag();
    }
    
    // Execute forward plan
    fftw_execute(wrapper->forwardPlan);
    
    // Copy results to output array
    for (int i = 0; i < n; i++) {
        y[i] = dcomplex(wrapper->complexOut[i][0], wrapper->complexOut[i][1]);
    }
    
    return api_true;
}

// Inverse complex transform (complex to complex)
api_bool API_Numerical_FFTInverseComplexTransformD(void* handle, dcomplex* y, const dcomplex* x) {
    if (!handle || !y || !x) return api_false;
    
    LogDebug("FFTInverseComplexTransformD called with handle " + std::to_string((uintptr_t)handle));
    
    std::lock_guard<std::mutex> lock(g_fftw_mutex);
    auto it = g_fftw_plans.find(handle);
    if (it == g_fftw_plans.end() || it->second->isReal) {
        LogDebug("FFTInverseComplexTransformD: Invalid handle or not complex transform");
        return api_false;
    }
    
    FFTWPlanWrapper* wrapper = it->second;
    int n = wrapper->size;
    
    // Copy input data to FFTW buffer
    for (int i = 0; i < n; i++) {
        wrapper->complexIn[i][0] = x[i].Real();
        wrapper->complexIn[i][1] = x[i].Imag();
    }
    
    // Execute inverse plan
    fftw_execute(wrapper->inversePlan);
    
    // Copy results to output array and normalize
    for (int i = 0; i < n; i++) {
        y[i] = dcomplex(wrapper->complexOut[i][0] / n, wrapper->complexOut[i][1] / n);
    }
    
    return api_true;
}

// Another naming convention for inverse complex transform (alias for FFTInverseComplexTransformD)
api_bool API_Numerical_FFTComplexInverseTransformD(void* handle, dcomplex* y, const dcomplex* x) {
    LogDebug("FFTComplexInverseTransformD called (alias for FFTInverseComplexTransformD)");
    // Simply delegate to the existing implementation
    return API_Numerical_FFTInverseComplexTransformD(handle, y, x);
}  

// Don't forget to call this at program exit
void ShutdownFFTFunctions() {
    CleanupFFTW();
}

// Call this in your InitializeMockAPI function
void InitializeMockAPI() {
    
    LogDebug("Mock API initialized");
}  

// Get the function resolver
function_resolver GetMockFunctionResolver() {
    return mock_function_resolver;
}

// Set the module handle
void SetModuleHandle(void* handle) {
    g_module_handle = handle;
    LogDebug("Module handle set to: " + std::to_string((uintptr_t)handle));
}

// Get the module handle
void* GetModuleHandle() {
    return g_module_handle;
}

// Enable or disable debug logging
void SetDebugLogging(bool enabled) {
    g_debug_logging = enabled;
}

// Set the log file
void SetLogFile(const std::string& filename) {
    if (g_log_file.is_open()) {
        g_log_file.close();
    }
    
    if (!filename.empty()) {
        g_log_file.open(filename);
        if (!g_log_file.is_open()) {
            std::cerr << "Failed to open log file: " << filename << std::endl;
        }
    }
}

// ----------------------------------------------------------------------------
// Sizer Mock Implementation
// ----------------------------------------------------------------------------

// Helper to get or create MockControl for any widget
static MockControl* GetOrCreateMockControl(control_handle handle) {
    std::lock_guard<std::mutex> lock(g_control_map_mutex);
    auto it = g_control_map.find(handle);
    if (it != g_control_map.end()) {
        return it->second;
    }
    
    // Create new mock control for existing widget
    QWidget* widget = reinterpret_cast<QWidget*>(handle);
    MockControl* ctrl = new MockControl(widget);
    g_control_map[handle] = ctrl;
    return ctrl;
}

struct MockSizer {
    QBoxLayout* layout;
    bool vertical;
    std::vector<QWidget*> widgets;
    
    MockSizer(bool vert) : vertical(vert) {
        if (vert) {
            layout = new QVBoxLayout();
        } else {
            layout = new QHBoxLayout();
        }
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
    }
    
    ~MockSizer() {
        // Don't delete layout - Qt parent ownership handles it
        // Don't delete widgets - they're owned by their parents
    }
};

// Global map to track sizers
static std::map<sizer_handle, MockSizer*> g_sizer_map;
static std::mutex g_sizer_map_mutex;

// ----------------------------------------------------------------------------
// SizerContext API
// ----------------------------------------------------------------------------

api_bool API_Sizer_GetSizerDisplayPixelRatio(const_sizer_handle handle, double* ratio)
{
    LogDebug("GetSizerDisplayPixelRatio called");
    
    if (!ratio) {
        return api_false;
    }
    
    std::lock_guard<std::mutex> lock(g_sizer_map_mutex);
    auto it = g_sizer_map.find(const_cast<sizer_handle>(handle));
    if (it == g_sizer_map.end()) {
        *ratio = 1.0;
        return api_false;
    }
    
    // Get the parent widget of the layout
    QWidget* parentWidget = it->second->layout->parentWidget();
    if (!parentWidget) {
        // No parent widget, return default ratio
        *ratio = 1.0;
        return api_true;
    }
    
    // Get the device pixel ratio from the widget's screen
    QScreen* screen = parentWidget->screen();
    if (screen) {
        *ratio = screen->devicePixelRatio();
    } else {
        // Fallback to primary screen
        QScreen* primaryScreen = QGuiApplication::primaryScreen();
        if (primaryScreen) {
            *ratio = primaryScreen->devicePixelRatio();
        } else {
            *ratio = 1.0;
        }
    }
    
    return api_true;
}
  
sizer_handle API_Sizer_CreateSizer(api_handle, api_bool vertical)
{
    LogDebug("CreateSizer called, vertical=" + std::to_string(vertical));
    
    MockSizer* sizer = new MockSizer(vertical != 0);
    sizer_handle handle = reinterpret_cast<sizer_handle>(sizer);
    
    std::lock_guard<std::mutex> lock(g_sizer_map_mutex);
    g_sizer_map[handle] = sizer;
    
    return handle;
}

control_handle API_Sizer_GetSizerParentControl(const_sizer_handle handle)
{
    LogDebug("GetSizerParentControl called");
    
    std::lock_guard<std::mutex> lock(g_sizer_map_mutex);
    auto it = g_sizer_map.find(const_cast<sizer_handle>(handle));
    if (it == g_sizer_map.end()) {
        return nullptr;
    }
    
    // Return the widget that owns this layout
    QWidget* parent = it->second->layout->parentWidget();
    return reinterpret_cast<control_handle>(parent);
}

api_bool API_Sizer_GetSizerOrientation(const_sizer_handle handle)
{
    LogDebug("GetSizerOrientation called");
    
    std::lock_guard<std::mutex> lock(g_sizer_map_mutex);
    auto it = g_sizer_map.find(const_cast<sizer_handle>(handle));
    if (it == g_sizer_map.end()) {
        return api_false;
    }
    
    return it->second->vertical ? api_true : api_false;
}

int32 API_Sizer_GetSizerCount(const_sizer_handle handle)
{
    LogDebug("GetSizerCount called");
    
    std::lock_guard<std::mutex> lock(g_sizer_map_mutex);
    auto it = g_sizer_map.find(const_cast<sizer_handle>(handle));
    if (it == g_sizer_map.end()) {
        return 0;
    }
    
    return it->second->layout->count();
}

void API_Sizer_InsertSizerControl(sizer_handle handle, int32 index, 
                                   control_handle control, int32 stretch, int32 alignment)
{
    LogDebug("InsertSizerControl called, index=" + std::to_string(index) + 
             ", stretch=" + std::to_string(stretch));
    
    if (!control) {
        LogDebug("InsertSizerControl: null control");
        return;
    }
    
    std::lock_guard<std::mutex> lock(g_sizer_map_mutex);
    auto it = g_sizer_map.find(handle);
    if (it == g_sizer_map.end()) {
        LogDebug("InsertSizerControl: sizer not found");
        return;
    }
    
    MockControl* ctrl = GetOrCreateMockControl(control);
    QWidget* widget = ctrl->widget;
    
    // Convert PCL alignment to Qt alignment
    Qt::Alignment qtAlign = Qt::AlignLeft | Qt::AlignTop;
    if (alignment & 0x01) qtAlign |= Qt::AlignLeft;
    if (alignment & 0x02) qtAlign |= Qt::AlignRight;
    if (alignment & 0x04) qtAlign |= Qt::AlignHCenter;
    if (alignment & 0x08) qtAlign |= Qt::AlignTop;
    if (alignment & 0x10) qtAlign |= Qt::AlignBottom;
    if (alignment & 0x20) qtAlign |= Qt::AlignVCenter;
    
    // Insert widget at the specified index
    it->second->layout->insertWidget(index, widget, stretch, qtAlign);
    it->second->widgets.push_back(widget);
}

void API_Sizer_InsertSizer(sizer_handle handle, int32 index, 
                           sizer_handle childSizer, int32 stretch)
{
    LogDebug("InsertSizer called, index=" + std::to_string(index));
    
    std::lock_guard<std::mutex> lock(g_sizer_map_mutex);
    auto it = g_sizer_map.find(handle);
    auto childIt = g_sizer_map.find(childSizer);
    
    if (it == g_sizer_map.end() || childIt == g_sizer_map.end()) {
        LogDebug("InsertSizer: sizer not found");
        return;
    }
    
    it->second->layout->insertLayout(index, childIt->second->layout, stretch);
}

void API_Sizer_InsertSizerSpacing(sizer_handle handle, int32 index, int32 spacing)
{
    LogDebug("InsertSizerSpacing called, spacing=" + std::to_string(spacing));
    
    std::lock_guard<std::mutex> lock(g_sizer_map_mutex);
    auto it = g_sizer_map.find(handle);
    if (it == g_sizer_map.end()) {
        return;
    }
    
    it->second->layout->insertSpacing(index, spacing);
}

void API_Sizer_InsertSizerStretch(sizer_handle handle, int32 index, int32 stretch)
{
    LogDebug("InsertSizerStretch called, stretch=" + std::to_string(stretch));
    
    std::lock_guard<std::mutex> lock(g_sizer_map_mutex);
    auto it = g_sizer_map.find(handle);
    if (it == g_sizer_map.end()) {
        return;
    }
    
    it->second->layout->insertStretch(index, stretch);
}

void API_Sizer_RemoveSizerControl(sizer_handle handle, control_handle control)
{
    LogDebug("RemoveSizerControl called");
    
    std::lock_guard<std::mutex> lock(g_sizer_map_mutex);
    auto it = g_sizer_map.find(handle);
    if (it == g_sizer_map.end()) {
        return;
    }
    
    QWidget* widget = reinterpret_cast<QWidget*>(control);
    it->second->layout->removeWidget(widget);
}

void API_Sizer_SetSizerMargin(sizer_handle handle, int32 margin)
{
    LogDebug("SetSizerMargin called, margin=" + std::to_string(margin));
    
    std::lock_guard<std::mutex> lock(g_sizer_map_mutex);
    auto it = g_sizer_map.find(handle);
    if (it == g_sizer_map.end()) {
        return;
    }
    
    it->second->layout->setContentsMargins(margin, margin, margin, margin);
}

int32 API_Sizer_GetSizerMargin(const_sizer_handle handle)
{
    LogDebug("GetSizerMargin called");
    
    std::lock_guard<std::mutex> lock(g_sizer_map_mutex);
    auto it = g_sizer_map.find(const_cast<sizer_handle>(handle));
    if (it == g_sizer_map.end()) {
        return 0;
    }
    
    QMargins margins = it->second->layout->contentsMargins();
    return margins.left(); // Return one margin value
}

void API_Sizer_SetSizerSpacing(sizer_handle handle, int32 spacing)
{
    LogDebug("SetSizerSpacing called, spacing=" + std::to_string(spacing));
    
    std::lock_guard<std::mutex> lock(g_sizer_map_mutex);
    auto it = g_sizer_map.find(handle);
    if (it == g_sizer_map.end()) {
        return;
    }
    
    it->second->layout->setSpacing(spacing);
}

int32 API_Sizer_GetSizerSpacing(const_sizer_handle handle)
{
    LogDebug("GetSizerSpacing called");
    
    std::lock_guard<std::mutex> lock(g_sizer_map_mutex);
    auto it = g_sizer_map.find(const_cast<sizer_handle>(handle));
    if (it == g_sizer_map.end()) {
        return 0;
    }
    
    return it->second->layout->spacing();
}

// ----------------------------------------------------------------------------
// Button Mock Implementation
// ----------------------------------------------------------------------------

struct MockButton {
    QWidget* button;  // Could be QPushButton or QToolButton
    bool isToolButton;
    bool checkable;
    bool checked;
    
    // Event handlers
    api_handle clientHandle;
    pcl::button_click_event_routine clickHandler;
    void* clickReceiver;
    
    MockButton(bool toolBtn = false) 
        : isToolButton(toolBtn), checkable(false), checked(false),
          clientHandle(nullptr), clickHandler(nullptr), clickReceiver(nullptr)
    {
        if (toolBtn) {
            button = new QToolButton();
        } else {
            button = new QPushButton();
        }
    }
    
    ~MockButton() {
        // Qt parent ownership handles deletion
    }
};

// Global map to track buttons
static std::map<control_handle, MockButton*> g_button_map;
static std::mutex g_button_map_mutex;

// ----------------------------------------------------------------------------
// ButtonContext API
// ----------------------------------------------------------------------------

control_handle API_Button_CreateToolButton(api_handle hModule, api_handle hClient, 
                                           const char16_type* text, const_bitmap_handle icon,
                                           api_bool checkable, control_handle parent, uint32 flags)
{
    auto textStr = text ? Utf16ToUtf8(text) : "";
    LogDbg("CreateToolButton called, text=" + textStr);
    
    MockButton* btn = new MockButton(true);
    btn->clientHandle = hClient;
    btn->checkable = (checkable != 0);
    
    QToolButton* toolBtn = static_cast<QToolButton*>(btn->button);
    
    // Set text if provided
    if (text && *text) {
        toolBtn->setText(QString::fromStdU16String(reinterpret_cast<const char16_t*>(text)));
    }
    
    // Set icon if provided
    if (icon) {
        QPixmap* pixmap = reinterpret_cast<QPixmap*>(const_cast<void*>(icon));
        if (pixmap) {
            toolBtn->setIcon(QIcon(*pixmap));
        }
    }
    
    // Set checkable
    if (checkable) {
        toolBtn->setCheckable(true);
    }
    
    // Set parent if provided
    if (parent) {
        QWidget* parentWidget = reinterpret_cast<QWidget*>(parent);
        toolBtn->setParent(parentWidget);
    }
    
    control_handle handle = reinterpret_cast<control_handle>(btn->button);
    
    std::lock_guard<std::mutex> lock(g_button_map_mutex);
    g_button_map[handle] = btn;
    
    return handle;
}

control_handle API_Button_CreatePushButton(api_handle hModule, api_handle hClient,
                                           const char16_type* text, const_bitmap_handle icon,
                                           control_handle parent, uint32 flags)
{
    auto textStr = text ? Utf16ToUtf8(text) : "";
    LogDbg("CreatePushButton called, text=" + textStr);
    
    MockButton* btn = new MockButton(false);
    btn->clientHandle = hClient;
    
    QPushButton* pushBtn = static_cast<QPushButton*>(btn->button);
    
    // Set text if provided
    if (text && *text) {
        pushBtn->setText(QString::fromStdU16String(reinterpret_cast<const char16_t*>(text)));
    }
    
    // Set icon if provided
    if (icon) {
        QPixmap* pixmap = reinterpret_cast<QPixmap*>(const_cast<void*>(icon));
        if (pixmap) {
            pushBtn->setIcon(QIcon(*pixmap));
        }
    }
    
    // Set parent if provided
    if (parent) {
        QWidget* parentWidget = reinterpret_cast<QWidget*>(parent);
        pushBtn->setParent(parentWidget);
    }
    
    control_handle handle = reinterpret_cast<control_handle>(btn->button);
    
    std::lock_guard<std::mutex> lock(g_button_map_mutex);
    g_button_map[handle] = btn;
    
    return handle;
}

api_bool API_Button_GetButtonText(const_control_handle handle, char16_type* text, size_type* len)
{
    LogDbg("GetButtonText called");
    
    std::lock_guard<std::mutex> lock(g_button_map_mutex);
    auto it = g_button_map.find(const_cast<control_handle>(handle));
    if (it == g_button_map.end()) {
        if (len) *len = 0;
        return api_false;
    }
    
    QAbstractButton* btn = qobject_cast<QAbstractButton*>(it->second->button);
    if (!btn) {
        if (len) *len = 0;
        return api_false;
    }
    
    QString qtext = btn->text();
    std::u16string u16text = qtext.toStdU16String();
    
    if (text == nullptr) {
        // Just return the length
        if (len) *len = u16text.length();
        return api_true;
    }
    
    if (len && *len > 0) {
        size_type copyLen = std::min(*len - 1, u16text.length());
        std::memcpy(text, u16text.c_str(), copyLen * sizeof(char16_type));
        text[copyLen] = 0;
        *len = copyLen;
    }
    
    return api_true;
}

void API_Button_SetButtonText(control_handle handle, const char16_type* text)
{
    auto textStr = text ? Utf16ToUtf8(text) : "";
    LogDbg("SetButtonText called, text=" + textStr);
    
    std::lock_guard<std::mutex> lock(g_button_map_mutex);
    auto it = g_button_map.find(handle);
    if (it == g_button_map.end()) {
        return;
    }
    
    QAbstractButton* btn = qobject_cast<QAbstractButton*>(it->second->button);
    if (btn && text) {
        btn->setText(QString::fromStdU16String(reinterpret_cast<const char16_t*>(text)));
    }
}

bitmap_handle API_Button_GetButtonIcon(const_control_handle handle)
{
    LogDbg("GetButtonIcon called");
    
    std::lock_guard<std::mutex> lock(g_button_map_mutex);
    auto it = g_button_map.find(const_cast<control_handle>(handle));
    if (it == g_button_map.end()) {
        return nullptr;
    }
    
    QAbstractButton* btn = qobject_cast<QAbstractButton*>(it->second->button);
    if (!btn) {
        return nullptr;
    }
    
    // Return a pixmap handle (simplified - would need proper icon->pixmap conversion)
    return nullptr;
}

void API_Button_SetButtonIcon(control_handle handle, const_bitmap_handle icon)
{
    LogDbg("SetButtonIcon called");
    
    std::lock_guard<std::mutex> lock(g_button_map_mutex);
    auto it = g_button_map.find(handle);
    if (it == g_button_map.end()) {
        return;
    }
    
    QAbstractButton* btn = qobject_cast<QAbstractButton*>(it->second->button);
    if (btn && icon) {
        QPixmap* pixmap = reinterpret_cast<QPixmap*>(const_cast<void*>(icon));
        if (pixmap) {
            btn->setIcon(QIcon(*pixmap));
        }
    }
}

void API_Button_GetButtonIconSize(const_control_handle handle, int32* w, int32* h)
{
    LogDbg("GetButtonIconSize called");
    
    std::lock_guard<std::mutex> lock(g_button_map_mutex);
    auto it = g_button_map.find(const_cast<control_handle>(handle));
    if (it == g_button_map.end()) {
        if (w) *w = 0;
        if (h) *h = 0;
        return;
    }
    
    QAbstractButton* btn = qobject_cast<QAbstractButton*>(it->second->button);
    if (btn) {
        QSize size = btn->iconSize();
        if (w) *w = size.width();
        if (h) *h = size.height();
    }
}

void API_Button_SetButtonIconSize(control_handle handle, int32 w, int32 h)
{
    LogDbg("SetButtonIconSize called, w=" + std::to_string(w) + ", h=" + std::to_string(h));
    
    std::lock_guard<std::mutex> lock(g_button_map_mutex);
    auto it = g_button_map.find(handle);
    if (it == g_button_map.end()) {
        return;
    }
    
    QAbstractButton* btn = qobject_cast<QAbstractButton*>(it->second->button);
    if (btn) {
        btn->setIconSize(QSize(w, h));
    }
}

api_bool API_Button_GetToolButtonCheckable(const_control_handle handle)
{
    LogDbg("GetToolButtonCheckable called");
    
    std::lock_guard<std::mutex> lock(g_button_map_mutex);
    auto it = g_button_map.find(const_cast<control_handle>(handle));
    if (it == g_button_map.end()) {
        return api_false;
    }
    
    return it->second->checkable ? api_true : api_false;
}

void API_Button_SetToolButtonCheckable(control_handle handle, api_bool checkable)
{
    LogDbg("SetToolButtonCheckable called, checkable=" + std::to_string(checkable));
    
    std::lock_guard<std::mutex> lock(g_button_map_mutex);
    auto it = g_button_map.find(handle);
    if (it == g_button_map.end()) {
        return;
    }
    
    it->second->checkable = (checkable != 0);
    
    QAbstractButton* btn = qobject_cast<QAbstractButton*>(it->second->button);
    if (btn) {
        btn->setCheckable(checkable != 0);
    }
}

uint32 API_Button_GetButtonChecked(const_control_handle handle)
{
    LogDbg("GetButtonChecked called");
    
    std::lock_guard<std::mutex> lock(g_button_map_mutex);
    auto it = g_button_map.find(const_cast<control_handle>(handle));
    if (it == g_button_map.end()) {
        return 0; // unchecked
    }
    
    QAbstractButton* btn = qobject_cast<QAbstractButton*>(it->second->button);
    if (btn && btn->isChecked()) {
        return 1; // checked
    }
    
    return 0; // unchecked
}

void API_Button_SetButtonChecked(control_handle handle, uint32 checked)
{
    LogDbg("SetButtonChecked called, checked=" + std::to_string(checked));
    
    std::lock_guard<std::mutex> lock(g_button_map_mutex);
    auto it = g_button_map.find(handle);
    if (it == g_button_map.end()) {
        return;
    }
    
    it->second->checked = (checked != 0);
    
    QAbstractButton* btn = qobject_cast<QAbstractButton*>(it->second->button);
    if (btn) {
        btn->setChecked(checked != 0);
    }
}

api_bool API_Button_SetButtonClickEventRoutine(control_handle handle, api_handle receiver,
                                                pcl::button_click_event_routine handler)
{
    LogDbg("SetButtonClickEventRoutine called");
    
    std::lock_guard<std::mutex> lock(g_button_map_mutex);
    auto it = g_button_map.find(handle);
    if (it == g_button_map.end()) {
        return api_false;
    }
    
    MockButton* mockBtn = it->second;
    mockBtn->clickHandler = handler;
    mockBtn->clickReceiver = receiver;
    
    QAbstractButton* btn = qobject_cast<QAbstractButton*>(mockBtn->button);
    if (btn) {
        // Disconnect any existing connections
        QObject::disconnect(btn, nullptr, nullptr, nullptr);
        
        // Connect clicked signal
        QObject::connect(btn, &QAbstractButton::clicked, [mockBtn, btn](bool checked) {
            if (mockBtn->clickHandler && mockBtn->clickReceiver) {
                control_handle btnHandle = reinterpret_cast<control_handle>(btn);
                mockBtn->clickHandler(mockBtn->clickReceiver, btnHandle, checked ? api_true : api_false);
            }
        });
    }
    
    return api_true;
}

// ----------------------------------------------------------------------------
// SpinBox Mock Implementation
// ----------------------------------------------------------------------------

struct MockSpinBox {
    QSpinBox* spinBox;
    
    // Event handlers
    api_handle clientHandle;
    pcl::spinbox_value_event_routine valueHandler;
    void* valueReceiver;
    
    // Range and value
    int minValue;
    int maxValue;
    int currentValue;
    
    MockSpinBox() 
        : spinBox(new QSpinBox()),
          clientHandle(nullptr),
          valueHandler(nullptr),
          valueReceiver(nullptr),
          minValue(0),
          maxValue(100),
          currentValue(0)
    {
        spinBox->setRange(minValue, maxValue);
        spinBox->setValue(currentValue);
    }
    
    ~MockSpinBox() {
        // Qt parent ownership handles deletion
    }
};

// Global map to track spinboxes
static std::map<const_control_handle, MockSpinBox*> g_spinbox_map;
static std::mutex g_spinbox_map_mutex;

// ----------------------------------------------------------------------------
// SpinBoxContext API
// ----------------------------------------------------------------------------

control_handle API_SpinBox_CreateSpinBox(api_handle hModule, api_handle client, 
                                         control_handle parent, uint32 flags)
{
    LogDebug("CreateSpinBox called");
    
    MockSpinBox* spin = new MockSpinBox();
    spin->clientHandle = client;
    
    // Set parent if provided
    if (parent) {
        QWidget* parentWidget = reinterpret_cast<QWidget*>(parent);
        spin->spinBox->setParent(parentWidget);
    }
    
    control_handle handle = reinterpret_cast<control_handle>(spin->spinBox);
    
    std::lock_guard<std::mutex> lock(g_spinbox_map_mutex);
    g_spinbox_map[handle] = spin;
    
    return handle;
}

int32 API_SpinBox_GetSpinBoxValue(const_control_handle handle)
{
    LogDebug("GetSpinBoxValue called");
    
    std::lock_guard<std::mutex> lock(g_spinbox_map_mutex);
    auto it = g_spinbox_map.find(const_cast<control_handle>(handle));
    if (it == g_spinbox_map.end()) {
        return 0;
    }
    
    return it->second->spinBox->value();
}

void API_SpinBox_SetSpinBoxValue(const_control_handle handle, int32 value)
{
    std::lock_guard<std::mutex> lock(g_spinbox_map_mutex);
    auto it = g_spinbox_map.find(handle);
    if (it == g_spinbox_map.end())
        return;

    MockSpinBox* mock = it->second;
    mock->currentValue = value;

    if (g_constructing_gui) return;
			      
    // Prevent triggering callbacks during UpdateControls()
    bool old = mock->spinBox->blockSignals(true);
    mock->spinBox->setValue(value);
    mock->spinBox->blockSignals(old);
}

void API_SpinBox_GetSpinBoxRange(const_control_handle handle, int32* minValue, int32* maxValue)
{
    LogDebug("GetSpinBoxRange called");
    
    std::lock_guard<std::mutex> lock(g_spinbox_map_mutex);
    auto it = g_spinbox_map.find(const_cast<control_handle>(handle));
    if (it == g_spinbox_map.end()) {
        if (minValue) *minValue = 0;
        if (maxValue) *maxValue = 100;
        return;
    }
    
    if (minValue) *minValue = it->second->spinBox->minimum();
    if (maxValue) *maxValue = it->second->spinBox->maximum();
}

void API_SpinBox_SetSpinBoxRange(control_handle handle, int32 minValue, int32 maxValue)
{
    LogDebug("SetSpinBoxRange called, min=" + std::to_string(minValue) + 
             ", max=" + std::to_string(maxValue));
    
    std::lock_guard<std::mutex> lock(g_spinbox_map_mutex);
    auto it = g_spinbox_map.find(handle);
    if (it == g_spinbox_map.end()) {
        return;
    }
    
    it->second->minValue = minValue;
    it->second->maxValue = maxValue;
    it->second->spinBox->setRange(minValue, maxValue);
}

int32 API_SpinBox_GetSpinBoxStepSize(const_control_handle handle)
{
    LogDebug("GetSpinBoxStepSize called");
    
    std::lock_guard<std::mutex> lock(g_spinbox_map_mutex);
    auto it = g_spinbox_map.find(const_cast<control_handle>(handle));
    if (it == g_spinbox_map.end()) {
        return 1;
    }
    
    return it->second->spinBox->singleStep();
}

void API_SpinBox_SetSpinBoxStepSize(control_handle handle, int32 stepSize)
{
    LogDebug("SetSpinBoxStepSize called, stepSize=" + std::to_string(stepSize));
    
    std::lock_guard<std::mutex> lock(g_spinbox_map_mutex);
    auto it = g_spinbox_map.find(handle);
    if (it == g_spinbox_map.end()) {
        return;
    }
    
    it->second->spinBox->setSingleStep(stepSize);
}

api_bool API_SpinBox_GetSpinBoxWrapping(const_control_handle handle)
{
    LogDebug("GetSpinBoxWrapping called");
    
    std::lock_guard<std::mutex> lock(g_spinbox_map_mutex);
    auto it = g_spinbox_map.find(const_cast<control_handle>(handle));
    if (it == g_spinbox_map.end()) {
        return api_false;
    }
    
    return it->second->spinBox->wrapping() ? api_true : api_false;
}

void API_SpinBox_SetSpinBoxWrapping(control_handle handle, api_bool wrapping)
{
    LogDebug("SetSpinBoxWrapping called, wrapping=" + std::to_string(wrapping));
    
    std::lock_guard<std::mutex> lock(g_spinbox_map_mutex);
    auto it = g_spinbox_map.find(handle);
    if (it == g_spinbox_map.end()) {
        return;
    }
    
    it->second->spinBox->setWrapping(wrapping != 0);
}

api_bool API_SpinBox_GetSpinBoxPrefix(const_control_handle handle, char16_type* prefix, size_type* len)
{
    LogDebug("GetSpinBoxPrefix called");
    
    std::lock_guard<std::mutex> lock(g_spinbox_map_mutex);
    auto it = g_spinbox_map.find(const_cast<control_handle>(handle));
    if (it == g_spinbox_map.end()) {
        if (len) *len = 0;
        return api_false;
    }
    
    QString qprefix = it->second->spinBox->prefix();
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
    
    std::lock_guard<std::mutex> lock(g_spinbox_map_mutex);
    auto it = g_spinbox_map.find(handle);
    if (it == g_spinbox_map.end()) {
        return;
    }
    
    if (prefix) {
        QString qprefix = QString::fromUtf16(reinterpret_cast<const ushort*>(prefix));
        it->second->spinBox->setPrefix(qprefix);
    } else {
        it->second->spinBox->setPrefix(QString());
    }
}

api_bool API_SpinBox_GetSpinBoxSuffix(const_control_handle handle, char16_type* suffix, size_type* len)
{
    LogDebug("GetSpinBoxSuffix called");
    
    std::lock_guard<std::mutex> lock(g_spinbox_map_mutex);
    auto it = g_spinbox_map.find(const_cast<control_handle>(handle));
    if (it == g_spinbox_map.end()) {
        if (len) *len = 0;
        return api_false;
    }
    
    QString qsuffix = it->second->spinBox->suffix();
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
    
    std::lock_guard<std::mutex> lock(g_spinbox_map_mutex);
    auto it = g_spinbox_map.find(handle);
    if (it == g_spinbox_map.end()) {
        return;
    }
    
    if (suffix) {
        QString qsuffix = QString::fromUtf16(reinterpret_cast<const ushort*>(suffix));
        it->second->spinBox->setSuffix(qsuffix);
    } else {
        it->second->spinBox->setSuffix(QString());
    }
}

api_bool API_SpinBox_SetSpinBoxValueUpdatedEventRoutine(
    control_handle handle,
    api_handle receiver,
    pcl::spinbox_value_event_routine handler)
{
    LogDebug("SetSpinBoxValueUpdatedEventRoutine called");

    std::lock_guard<std::mutex> lock(g_spinbox_map_mutex);
    auto it = g_spinbox_map.find(handle);
    if (it == g_spinbox_map.end())
        return api_false;

    MockSpinBox* mockSpin = it->second;
    mockSpin->valueHandler  = handler;
    mockSpin->valueReceiver = receiver;
    
    // Disconnect any existing connections
    QObject::disconnect(mockSpin->spinBox, nullptr, nullptr, nullptr);

    // Connect valueChanged signal
    QObject::connect(
        mockSpin->spinBox,
        QOverload<int>::of(&QSpinBox::valueChanged),
        [mockSpin](int value)
        {
            if (mockSpin->valueHandler && mockSpin->valueReceiver)
            {
                control_handle spinHandle =
                    reinterpret_cast<control_handle>(mockSpin->spinBox);
                mockSpin->valueHandler(reinterpret_cast<api_handle>(g_activeInterface), spinHandle, value);
            }
        });
    
    return api_true;
}

int32 API_SpinBox_GetSpinBoxMinEditWidth(const_control_handle handle)
{
    LogDebug("GetSpinBoxMinEditWidth called");
    
    std::lock_guard<std::mutex> lock(g_spinbox_map_mutex);
    auto it = g_spinbox_map.find(const_cast<control_handle>(handle));
    if (it == g_spinbox_map.end()) {
        return 0;
    }
    
    return it->second->spinBox->minimumWidth();
}

void API_SpinBox_SetSpinBoxMinEditWidth(control_handle handle, int32 width)
{
    LogDebug("SetSpinBoxMinEditWidth called, width=" + std::to_string(width));
    
    std::lock_guard<std::mutex> lock(g_spinbox_map_mutex);
    auto it = g_spinbox_map.find(handle);
    if (it == g_spinbox_map.end()) {
        return;
    }
    
    it->second->spinBox->setMinimumWidth(width);
}

api_bool API_SpinBox_IsSpinBoxReadOnly(const_control_handle handle)
{
    LogDebug("IsSpinBoxReadOnly called");
    
    std::lock_guard<std::mutex> lock(g_spinbox_map_mutex);
    auto it = g_spinbox_map.find(const_cast<control_handle>(handle));
    if (it == g_spinbox_map.end()) {
        return api_false;
    }
    
    return it->second->spinBox->isReadOnly() ? api_true : api_false;
}

void API_SpinBox_SetSpinBoxReadOnly(control_handle handle, api_bool readOnly)
{
    LogDebug("SetSpinBoxReadOnly called, readOnly=" + std::to_string(readOnly));
    
    std::lock_guard<std::mutex> lock(g_spinbox_map_mutex);
    auto it = g_spinbox_map.find(handle);
    if (it == g_spinbox_map.end()) {
        return;
    }
    
    it->second->spinBox->setReadOnly(readOnly != 0);
}

// ----------------------------------------------------------------------------
// ControlContext API
// ----------------------------------------------------------------------------

// Small helper – avoids dynamic_cast on non-polymorphic MockControl.
/*
static MockControl *GetControlBox( const_control_handle h )
{
    if (!h) return nullptr;
    std::lock_guard<std::mutex> lock(g_control_map_mutex);
    auto it = g_control_map.find( h );
    if ( it == g_control_map.end() )
        return nullptr;
    return static_cast<MockControl *>( it->second );
}
*/

MockControl* GetControlBox(const_control_handle h)
{
    if (!h)
        return nullptr;

    std::lock_guard<std::mutex> lock(g_control_map_mutex);
    auto it = g_control_map.find(h);
    if (it == g_control_map.end())
        return nullptr;

    return it->second;
}

api_bool API_Control_GetControlResourcePixelRatio(const_control_handle handle, double* ratio)
{
    LogDebug("GetControlResourcePixelRatio called");
    
    if (!handle) {
        *ratio = 1.0;
        return api_false;
    }

    MockControl* wdg = GetControlBox(handle);
    
    if (!ratio || !wdg) {
        return api_false;
    }
    
    QWidget* widget = wdg->widget;
    
    // Get the device pixel ratio from the widget's screen
    QScreen* screen = widget->screen();
    if (screen) {
        *ratio = screen->devicePixelRatio();
    } else {
        // Fallback to primary screen
        QScreen* primaryScreen = QGuiApplication::primaryScreen();
        if (primaryScreen) {
            *ratio = primaryScreen->devicePixelRatio();
        } else {
            *ratio = 1.0;
        }
    }

    *ratio = 1.0;
    return api_true;
}

static inline int SanitizeSize(int v)
{
    return (v < 0) ? 0 : v;   // or 1, but 0 is accepted and means "no min"
}

control_handle API_Control_CreateControl(api_handle hModule,
                                         api_handle client,
                                         control_handle parent,
                                         uint32 flags)
{
    LogDebug("CreateControl called");

    // Allocate a new MockControl object
    MockControl* ctrl = new MockControl();
    ctrl->clientHandle = client;
    ctrl->flags = flags;

    // Parent relationship
    if (parent) {
        MockControl* parentCtrl = reinterpret_cast<MockControl*>(parent);
        ctrl->widget->setParent(parentCtrl->widget);   // <- QWidget parent
        ctrl->parent = nullptr;                        // <- TreeBox only
    } else {
        //
        // Important: This becomes a top-level window.
        //
        ctrl->widget->setWindowFlags(Qt::Window);
    }

    // Create stable handle
    control_handle handle = reinterpret_cast<control_handle>(ctrl);

    {
        std::lock_guard<std::mutex> lock(g_control_map_mutex);
        g_control_map[handle] = ctrl;
    }

    // Remember the last created top-level control
    if (!parent)
        g_lastTopLevelControl = handle;
    
    return handle;
}

void API_Control_DestroyControl(control_handle handle)
{
    LogDebug("DestroyControl called");
    
    std::lock_guard<std::mutex> lock(g_control_map_mutex);
    auto it = g_control_map.find(handle);
    if (it != g_control_map.end()) {
        delete it->second->widget;
        delete it->second;
        g_control_map.erase(it);
    }
}

control_handle API_Control_GetControlParent(const_control_handle handle)
{
    LogDebug("GetControlParent called");
    
    MockControl* wdg = GetControlBox(handle);
    
    if (!wdg) {
        return api_false;
    }
    
    QWidget* widget = wdg->widget;
    QWidget* parent = widget->parentWidget();
    
    return reinterpret_cast<control_handle>(parent);
}

void API_Control_SetControlParent(control_handle handle, control_handle parent)
{
    LogDebug("SetControlParent called");
    
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return;
    QWidget* widget = wdg->widget;
    QWidget* parentWidget = parent ? reinterpret_cast<QWidget*>(parent) : nullptr;
    
    widget->setParent(parentWidget);
}

void API_Control_GetControlPosition(const_control_handle handle, int32* x, int32* y)
{
    if (!handle) {
        if (x) *x = 0;
        if (y) *y = 0;
        return;
    }
    
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return;
    QWidget* widget = wdg->widget;
    QPoint pos = widget->pos();
    
    if (x) *x = pos.x();
    if (y) *y = pos.y();
}

void API_Control_SetControlPosition(control_handle handle, int32 x, int32 y)
{
    LogDebug("SetControlPosition called, x=" + std::to_string(x) + ", y=" + std::to_string(y));
    
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return;
    QWidget* widget = wdg->widget;
    widget->move(x, y);
}

void API_Control_GetControlSize(const_control_handle handle, int32* w, int32* h)
{
    if (!handle) {
        if (w) *w = 0;
        if (h) *h = 0;
        return;
    }
    
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return;
    QWidget* widget = wdg->widget;
    QSize size = widget->size();
    
    if (w) *w = size.width();
    if (h) *h = size.height();
}

void API_Control_SetControlSize(control_handle handle, int32 w, int32 h)
{
    LogDebug("SetControlSize called, w=" + std::to_string(w) + ", h=" + std::to_string(h));
    
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return;
    QWidget* widget = wdg->widget;
    widget->resize(w, h);
}

void API_Control_GetControlMinSize(const_control_handle handle, int32* w, int32* h)
{
    if (!handle) {
        if (w) *w = 0;
        if (h) *h = 0;
        return;
    }
    
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return;
    QWidget* widget = wdg->widget;
    QSize size = widget->minimumSize();
    
    if (w) *w = size.width();
    if (h) *h = size.height();
}

void API_Control_SetControlMinSize(control_handle handle, int32 w, int32 h)
{
    LogDebug("SetControlMinSize called, w=" + std::to_string(w) + ", h=" + std::to_string(h));
    
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return;
    QWidget* widget = wdg->widget;
    int W = SanitizeSize(w);
    int H = SanitizeSize(h);
    
    widget->setMinimumSize(W, H);
}

void API_Control_GetControlMaxSize(const_control_handle handle, int32* w, int32* h)
{
    if (!handle) {
        if (w) *w = 16777215; // Qt's default QWIDGETSIZE_MAX
        if (h) *h = 16777215;
        return;
    }
    
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return;
    QWidget* widget = wdg->widget;
    QSize size = widget->maximumSize();
    
    if (w) *w = size.width();
    if (h) *h = size.height();
}

void API_Control_SetControlMaxSize(control_handle handle, int32 w, int32 h)
{
    LogDebug("SetControlMaxSize called, w=" + std::to_string(w) + ", h=" + std::to_string(h));
    
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return;
    QWidget* widget = wdg->widget;
    widget->setMaximumSize(w, h);
}

void API_Control_SetControlFixedSize(control_handle handle, int32 w, int32 h)
{
    LogDebug("SetControlFixedSize called, w=" + std::to_string(w) + ", h=" + std::to_string(h));
    
    int W = SanitizeSize(w);
    int H = SanitizeSize(h);
    
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return;
    QWidget* widget = wdg->widget;
    
    widget->setFixedSize(W, H);
}

void API_Control_SetControlScaledMinSize(control_handle handle, int32 w, int32 h)
{
    LogDebug("SetControlScaledMinSize called, w=" + std::to_string(w) + ", h=" + std::to_string(h));
    
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return;
    QWidget* widget = wdg->widget;
    // In a real implementation, you'd scale by DPI
    // For now, just set minimum size directly
    int W = SanitizeSize(w);
    int H = SanitizeSize(h);
    
    widget->setMinimumSize(W, H);
}

void API_Control_SetControlScaledMaxSize(control_handle handle, int32 w, int32 h)
{
    LogDebug("SetControlScaledMaxSize called, w=" + std::to_string(w) + ", h=" + std::to_string(h));
    
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return;
    QWidget* widget = wdg->widget;
    widget->setMaximumSize(w, h);
}

void API_Control_SetControlScaledFixedSize(control_handle handle, int32 w, int32 h)
{
    LogDebug("SetControlScaledFixedSize called, w=" + std::to_string(w) + ", h=" + std::to_string(h));
    
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return;
    QWidget* widget = wdg->widget;
    widget->setFixedSize(w, h);
}

void API_Control_SetControlScaledMinWidth(control_handle handle, int32 w)
{
    LogDebug("SetControlScaledMinWidth called, w=" + std::to_string(w));
    
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return;
    QWidget* widget = wdg->widget;
    widget->setMinimumWidth(w);
}

void API_Control_SetControlScaledMinHeight(control_handle handle, int32 h)
{
    LogDebug("SetControlScaledMinHeight called, h=" + std::to_string(h));
    
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return;
    QWidget* widget = wdg->widget;
    widget->setMinimumHeight(h);
}

api_bool API_Control_GetControlVisible(const_control_handle handle)
{
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return api_false;
    QWidget* widget = wdg->widget;
    return widget->isVisible() ? api_true : api_false;
}

void API_Control_SetControlVisible(control_handle handle, api_bool visibleFlags)
{
    LogDebug("SetControlVisible called, flags=" + std::to_string(visibleFlags));

    // If the PixInsight side uses a null handle for the interface "frame",
    // substitute our last known top-level control.
    if (!handle && g_lastTopLevelControl) {
        LogDebug("SetControlVisible: null handle, using last top-level control");
        handle = g_lastTopLevelControl;
    }

    MockControl* wdg = GetControlBox(handle);
    if (!wdg)
        return;

    QWidget* widget = wdg->widget;
    if (!widget)
        return;

    if (visibleFlags)
        widget->show();
    else
        widget->hide();
}

void API_Control_ShowControl(control_handle handle)
{
    LogDebug("ShowControl called");
    
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return;
    QWidget* widget = wdg->widget;
    widget->show();
}

void API_Control_HideControl(control_handle handle)
{
    LogDebug("HideControl called");
    
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return;
    QWidget* widget = wdg->widget;
    widget->hide();
}

api_bool API_Control_GetControlEnabled(const_control_handle handle)
{
    if (!handle) return api_false;
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return api_false;
    QWidget* widget = wdg->widget;
    return widget->isEnabled() ? api_true : api_false;
}

void API_Control_SetControlEnabled(control_handle handle, api_bool enabled)
{
    LogDebug("SetControlEnabled called, enabled=" + std::to_string(enabled));
    
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return;
    QWidget* widget = wdg->widget;
    widget->setEnabled(enabled != 0);
}

void API_Control_EnableControl(control_handle handle)
{
    LogDebug("EnableControl called");
    
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return;
    QWidget* widget = wdg->widget;
    widget->setEnabled(true);
}

void API_Control_DisableControl(control_handle handle)
{
    LogDebug("DisableControl called");
    
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return;
    QWidget* widget = wdg->widget;
    widget->setEnabled(false);
}

void API_Control_SetControlToolTip(control_handle handle, const char16_type* tooltip)
{
    if (!handle || !tooltip) return;
    
    std::string tipStr = Utf16ToUtf8(tooltip);
    LogDebug("SetControlToolTip called");
    
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return;
    QWidget* widget = wdg->widget;
    widget->setToolTip(QString::fromUtf16(reinterpret_cast<const ushort*>(tooltip)));
}

void API_Control_SetControlFocusStyle(control_handle handle, int32 style)
{
    LogDebug("SetControlFocusStyle called, style=" + std::to_string(style));
    
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return;
    QWidget* widget = wdg->widget;
    
    // Map PCL focus styles to Qt
    // 0 = NoFocus, 1 = TabFocus, 2 = ClickFocus, 3 = StrongFocus
    switch (style) {
        case 0:
            widget->setFocusPolicy(Qt::NoFocus);
            break;
        case 1:
            widget->setFocusPolicy(Qt::TabFocus);
            break;
        case 2:
            widget->setFocusPolicy(Qt::ClickFocus);
            break;
        case 3:
            widget->setFocusPolicy(Qt::StrongFocus);
            break;
        default:
            widget->setFocusPolicy(Qt::StrongFocus);
    }
}

void API_Control_SetControlSizer(control_handle handle, sizer_handle sizer)
{
    LogDebug("SetControlSizer called");
    
    if (!handle) return;
    
    std::lock_guard<std::mutex> lock(g_sizer_map_mutex);
    auto sizerIt = g_sizer_map.find(sizer);
    if (sizerIt == g_sizer_map.end()) {
        LogDebug("SetControlSizer: sizer not found");
        return;
    }
    
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return;
    QWidget* widget = wdg->widget;
    widget->setLayout(sizerIt->second->layout);
    
    // Store sizer reference in control
    MockControl* ctrl = GetOrCreateMockControl(handle);
    ctrl->layout = sizerIt->second->layout;
}

void API_Control_UpdateControl(control_handle handle)
{
    LogDebug("UpdateControl called");
    
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return;
    QWidget* widget = wdg->widget;
    widget->update();
}

void API_Control_RepaintControl(control_handle handle)
{
    LogDebug("RepaintControl called");
    
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return;
    QWidget* widget = wdg->widget;
    widget->repaint();
}

void API_Control_EnableMouseTracking(control_handle handle)
{
    LogDebug("EnableMouseTracking called");
    
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return;
    QWidget* widget = wdg->widget;
    widget->setMouseTracking(true);
}

void API_Control_DisableMouseTracking(control_handle handle)
{
    LogDebug("DisableMouseTracking called");
    
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return;
    QWidget* widget = wdg->widget;
    widget->setMouseTracking(false);
}

api_bool API_Control_GetControlCursor(const_control_handle handle, int32* cursorShape)
{
    if (!handle || !cursorShape) return api_false;
    
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return api_false;
    QWidget* widget = wdg->widget;
    
    // Map Qt cursor to PCL cursor shape
    Qt::CursorShape shape = widget->cursor().shape();
    *cursorShape = static_cast<int32>(shape);
    
    return api_true;
}

void API_Control_SetControlCursor(control_handle handle, int32 cursorShape)
{
    LogDebug("SetControlCursor called, cursorShape=" + std::to_string(cursorShape));
    
    MockControl* wdg = GetControlBox(handle);
    if (!wdg) return;
    QWidget* widget = wdg->widget;
    
    // Map PCL cursor shape to Qt (assuming similar values)
    Qt::CursorShape qtShape = static_cast<Qt::CursorShape>(cursorShape);
    widget->setCursor(QCursor(qtShape));
}

// ----------------------------------------------------------------------------
// Label Mock Implementation
// ----------------------------------------------------------------------------

struct MockLabel {
    QLabel* label;
    api_handle clientHandle;
    
    MockLabel(const char16_type* text = nullptr) 
        : label(new QLabel()),
          clientHandle(nullptr)
    {
        if (text && *text) {
            label->setText(QString::fromUtf16(reinterpret_cast<const ushort*>(text)));
        }
    }
    
    ~MockLabel() {
        // Qt parent ownership handles deletion
    }
};

// Global map to track labels
static std::map<control_handle, MockLabel*> g_label_map;
static std::mutex g_label_map_mutex;

// ----------------------------------------------------------------------------
// LabelContext API
// ----------------------------------------------------------------------------

control_handle API_Label_CreateLabel(api_handle hModule, api_handle client, 
                                     const char16_type* text, control_handle parent, uint32 flags)
{
    LogDebug("CreateLabel called");
    
    MockLabel* lbl = new MockLabel(text);
    lbl->clientHandle = client;
    
    // Set parent if provided
    if (parent) {
        QWidget* parentWidget = reinterpret_cast<QWidget*>(parent);
        lbl->label->setParent(parentWidget);
    }
    
    control_handle handle = reinterpret_cast<control_handle>(lbl->label);
    
    std::lock_guard<std::mutex> lock(g_label_map_mutex);
    g_label_map[handle] = lbl;
    
    return handle;
}

api_bool API_Label_GetLabelText(const_control_handle handle, char16_type* text, size_type* len)
{
    LogDebug("GetLabelText called");
    
    std::lock_guard<std::mutex> lock(g_label_map_mutex);
    auto it = g_label_map.find(const_cast<control_handle>(handle));
    if (it == g_label_map.end()) {
        if (len) *len = 0;
        return api_false;
    }
    
    QString qtext = it->second->label->text();
    std::u16string u16text = qtext.toStdU16String();
    
    if (text == nullptr) {
        // Just return the length
        if (len) *len = u16text.length();
        return api_true;
    }
    
    if (len && *len > 0) {
        size_type copyLen = std::min(*len - 1, u16text.length());
        std::memcpy(text, u16text.c_str(), copyLen * sizeof(char16_type));
        text[copyLen] = 0;
        *len = copyLen;
    }
    
    return api_true;
}

void API_Label_SetLabelText(control_handle handle, const char16_type* text)
{
    LogDebug("SetLabelText called");
    
    std::lock_guard<std::mutex> lock(g_label_map_mutex);
    auto it = g_label_map.find(handle);
    if (it == g_label_map.end()) {
        return;
    }
    
    if (text) {
        it->second->label->setText(QString::fromUtf16(reinterpret_cast<const ushort*>(text)));
    } else {
        it->second->label->setText(QString());
    }
}

void API_Label_ClearLabelText(control_handle handle)
{
    LogDebug("ClearLabelText called");
    
    std::lock_guard<std::mutex> lock(g_label_map_mutex);
    auto it = g_label_map.find(handle);
    if (it == g_label_map.end()) {
        return;
    }
    
    it->second->label->clear();
}

int32 API_Label_GetLabelTextAlignment(const_control_handle handle)
{
    LogDebug("GetLabelTextAlignment called");
    
    std::lock_guard<std::mutex> lock(g_label_map_mutex);
    auto it = g_label_map.find(const_cast<control_handle>(handle));
    if (it == g_label_map.end()) {
        return 0;
    }
    
    Qt::Alignment align = it->second->label->alignment();
    
    // Convert Qt alignment to PCL alignment flags
    int32 pclAlign = 0;
    if (align & Qt::AlignLeft) pclAlign |= 0x01;
    if (align & Qt::AlignRight) pclAlign |= 0x02;
    if (align & Qt::AlignHCenter) pclAlign |= 0x04;
    if (align & Qt::AlignTop) pclAlign |= 0x08;
    if (align & Qt::AlignBottom) pclAlign |= 0x10;
    if (align & Qt::AlignVCenter) pclAlign |= 0x20;
    
    return pclAlign;
}

void API_Label_SetLabelTextAlignment(control_handle handle, int32 alignment)
{
    LogDebug("SetLabelTextAlignment called, alignment=" + std::to_string(alignment));
    
    std::lock_guard<std::mutex> lock(g_label_map_mutex);
    auto it = g_label_map.find(handle);
    if (it == g_label_map.end()) {
        return;
    }
    
    // Convert PCL alignment flags to Qt alignment
    Qt::Alignment qtAlign = Qt::AlignLeft | Qt::AlignTop;
    
    if (alignment & 0x01) qtAlign |= Qt::AlignLeft;
    if (alignment & 0x02) qtAlign |= Qt::AlignRight;
    if (alignment & 0x04) qtAlign |= Qt::AlignHCenter;
    if (alignment & 0x08) qtAlign |= Qt::AlignTop;
    if (alignment & 0x10) qtAlign |= Qt::AlignBottom;
    if (alignment & 0x20) qtAlign |= Qt::AlignVCenter;
    
    it->second->label->setAlignment(qtAlign);
}

api_bool API_Label_GetLabelWordWrapping(const_control_handle handle)
{
    LogDebug("GetLabelWordWrapping called");
    
    std::lock_guard<std::mutex> lock(g_label_map_mutex);
    auto it = g_label_map.find(const_cast<control_handle>(handle));
    if (it == g_label_map.end()) {
        return api_false;
    }
    
    return it->second->label->wordWrap() ? api_true : api_false;
}

void API_Label_SetLabelWordWrapping(control_handle handle, api_bool wordWrap)
{
    LogDebug("SetLabelWordWrapping called, wordWrap=" + std::to_string(wordWrap));
    
    std::lock_guard<std::mutex> lock(g_label_map_mutex);
    auto it = g_label_map.find(handle);
    if (it == g_label_map.end()) {
        return;
    }
    
    it->second->label->setWordWrap(wordWrap != 0);
}

int32 API_Label_GetLabelMargin(const_control_handle handle)
{
    LogDebug("GetLabelMargin called");
    
    std::lock_guard<std::mutex> lock(g_label_map_mutex);
    auto it = g_label_map.find(const_cast<control_handle>(handle));
    if (it == g_label_map.end()) {
        return 0;
    }
    
    return it->second->label->margin();
}

void API_Label_SetLabelMargin(control_handle handle, int32 margin)
{
    LogDebug("SetLabelMargin called, margin=" + std::to_string(margin));
    
    std::lock_guard<std::mutex> lock(g_label_map_mutex);
    auto it = g_label_map.find(handle);
    if (it == g_label_map.end()) {
        return;
    }
    
    it->second->label->setMargin(margin);
}

int32 API_Label_GetLabelIndent(const_control_handle handle)
{
    LogDebug("GetLabelIndent called");
    
    std::lock_guard<std::mutex> lock(g_label_map_mutex);
    auto it = g_label_map.find(const_cast<control_handle>(handle));
    if (it == g_label_map.end()) {
        return 0;
    }
    
    return it->second->label->indent();
}

void API_Label_SetLabelIndent(control_handle handle, int32 indent)
{
    LogDebug("SetLabelIndent called, indent=" + std::to_string(indent));
    
    std::lock_guard<std::mutex> lock(g_label_map_mutex);
    auto it = g_label_map.find(handle);
    if (it == g_label_map.end()) {
        return;
    }
    
    it->second->label->setIndent(indent);
}

int32 API_Label_GetLabelFrameStyle(const_control_handle handle)
{
    LogDebug("GetLabelFrameStyle called");
    
    std::lock_guard<std::mutex> lock(g_label_map_mutex);
    auto it = g_label_map.find(const_cast<control_handle>(handle));
    if (it == g_label_map.end()) {
        return 0;
    }
    
    return it->second->label->frameStyle();
}

void API_Label_SetLabelFrameStyle(control_handle handle, int32 style)
{
    LogDebug("SetLabelFrameStyle called, style=" + std::to_string(style));
    
    std::lock_guard<std::mutex> lock(g_label_map_mutex);
    auto it = g_label_map.find(handle);
    if (it == g_label_map.end()) {
        return;
    }
    
    it->second->label->setFrameStyle(style);
}

int32 API_Label_GetLabelLineWidth(const_control_handle handle)
{
    LogDebug("GetLabelLineWidth called");
    
    std::lock_guard<std::mutex> lock(g_label_map_mutex);
    auto it = g_label_map.find(const_cast<control_handle>(handle));
    if (it == g_label_map.end()) {
        return 0;
    }
    
    return it->second->label->lineWidth();
}

void API_Label_SetLabelLineWidth(control_handle handle, int32 width)
{
    LogDebug("SetLabelLineWidth called, width=" + std::to_string(width));
    
    std::lock_guard<std::mutex> lock(g_label_map_mutex);
    auto it = g_label_map.find(handle);
    if (it == g_label_map.end()) {
        return;
    }
    
    it->second->label->setLineWidth(width);
}

int32 API_Label_GetLabelMinWidth(const_control_handle handle)
{
    LogDebug("GetLabelMinWidth called");
    
    std::lock_guard<std::mutex> lock(g_label_map_mutex);
    auto it = g_label_map.find(const_cast<control_handle>(handle));
    if (it == g_label_map.end()) {
        return 0;
    }
    
    return it->second->label->minimumWidth();
}

void API_Label_SetLabelMinWidth(control_handle handle, int32 width)
{
    LogDebug("SetLabelMinWidth called, width=" + std::to_string(width));
    
    std::lock_guard<std::mutex> lock(g_label_map_mutex);
    auto it = g_label_map.find(handle);
    if (it == g_label_map.end()) {
        return;
    }
    
    it->second->label->setMinimumWidth(width);
}

// ----------------------------------------------------------------------------
// UI Object ID Management
// ----------------------------------------------------------------------------

// Map to store object IDs
static std::map<api_handle, std::u16string> g_object_id_map;
static std::mutex g_object_id_map_mutex;

// ----------------------------------------------------------------------------
// UIContext API
// ----------------------------------------------------------------------------

api_bool API_UI_SetUIObjectId(api_handle handle, const char16_type* id)
{
    if (!handle) {
        return api_false;
    }
    
    std::lock_guard<std::mutex> lock(g_object_id_map_mutex);
    
    if (id && *id) {
        std::u16string idStr(reinterpret_cast<const char16_t*>(id));
        g_object_id_map[handle] = idStr;
        
        // Also set Qt object name if it's a QWidget
        MockControl* wdg = GetControlBox(handle);
        if (wdg) {
	    QWidget* widget = wdg->widget;
            QString qid = QString::fromUtf16(reinterpret_cast<const ushort*>(id));
            widget->setObjectName(qid);
        }
        
        LogDebug("SetUIObjectId called, id=" + Utf16ToUtf8(id));
    } else {
        // Clear the ID
        g_object_id_map.erase(handle);
        
        MockControl* wdg = GetControlBox(handle);
        if (wdg) {
	    QWidget* widget = wdg->widget;
            widget->setObjectName(QString());
        }
        
        LogDebug("SetUIObjectId called, id cleared");
    }
    
    return api_true;
}

api_bool API_UI_GetUIObjectId(api_handle handle, char16_type* id, size_type* len)
{
    if (!handle) {
        if (len) *len = 0;
        return api_false;
    }
    
    std::lock_guard<std::mutex> lock(g_object_id_map_mutex);
    
    auto it = g_object_id_map.find(handle);
    if (it == g_object_id_map.end()) {
        if (len) *len = 0;
        return api_false;
    }
    
    const std::u16string& idStr = it->second;
    
    if (id == nullptr) {
        // Just return the length
        if (len) *len = idStr.length();
        return api_true;
    }
    
    if (len && *len > 0) {
        size_type copyLen = std::min(*len - 1, idStr.length());
        std::memcpy(id, idStr.c_str(), copyLen * sizeof(char16_type));
        id[copyLen] = 0;
        *len = copyLen;
    }
    
    return api_true;
}

api_handle API_UI_FindUIObjectById(const char16_type* id)
{
    if (!id || !*id) {
        return nullptr;
    }
    
    LogDebug("FindUIObjectById called");
    
    std::u16string searchId(reinterpret_cast<const char16_t*>(id));
    
    std::lock_guard<std::mutex> lock(g_object_id_map_mutex);
    
    for (const auto& pair : g_object_id_map) {
        if (pair.second == searchId) {
            return pair.first;
        }
    }
    
    return nullptr;
}

api_bool API_UI_IsUIObjectValid(api_handle handle)
{
    if (!handle) {
        return api_false;
    }
    
    // Check if it's a valid QWidget
    QWidget* widget = reinterpret_cast<QWidget*>(handle);
    
    // Simple validation - in a real implementation you might want more checks
    // For now, just check if the pointer seems reasonable and the widget isn't deleted
    try {
        // Try to access a Qt property to see if it's valid
        widget->isVisible();
        return api_true;
    } catch (...) {
        return api_false;
    }
}

void API_UI_DeleteUIObject(api_handle handle)
{
    if (!handle) {
        return;
    }
    
    LogDebug("DeleteUIObject called");
    
    // Remove from ID map
    {
        std::lock_guard<std::mutex> lock(g_object_id_map_mutex);
        g_object_id_map.erase(handle);
    }
    
    // Try to delete the widget
    QWidget* widget = reinterpret_cast<QWidget*>(handle);
    if (widget) {
        widget->deleteLater(); // Use deleteLater() for Qt safety
    }
}

const char* API_UI_GetUIObjectType(api_handle handle)
{
    if (!handle) {
        return "null";
    }
    
    QWidget* widget = reinterpret_cast<QWidget*>(handle);
    if (!widget) {
        return "invalid";
    }
    
    // Return the Qt metaobject class name
    return widget->metaObject()->className();
}

// ----------------------------------------------------------------------------
// Bitmap Mock Implementation
// ----------------------------------------------------------------------------

struct MockBitmap {
    QPixmap pixmap;
    api_handle moduleHandle;
    double devicePixelRatio;
    
    MockBitmap(api_handle hModule) 
        : moduleHandle(hModule),
          devicePixelRatio(1.0)
    {
    }
    
    MockBitmap(api_handle hModule, int width, int height)
        : pixmap(width, height),
          moduleHandle(hModule),
          devicePixelRatio(1.0)
    {
        pixmap.fill(Qt::transparent);
    }
    
    MockBitmap(api_handle hModule, const QPixmap& pm)
        : pixmap(pm),
          moduleHandle(hModule),
          devicePixelRatio(1.0)
    {
    }
    
    ~MockBitmap() {
        // QPixmap handles its own memory
    }
};

// Global map to track bitmaps
static std::map<bitmap_handle, MockBitmap*> g_bitmap_map;
static std::mutex g_bitmap_map_mutex;

// Helper to get bitmap from handle
static MockBitmap* GetBitmap(bitmap_handle handle) {
    if (!handle) return nullptr;
    
    std::lock_guard<std::mutex> lock(g_bitmap_map_mutex);
    auto it = g_bitmap_map.find(handle);
    if (it != g_bitmap_map.end()) {
        return it->second;
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// BitmapContext API Implementation
// ----------------------------------------------------------------------------

bitmap_handle API_Bitmap_CreateBitmap(api_handle hModule, int32 width, int32 height, void* data)
{
    LogDbg("CreateBitmap called with dimensions: " + std::to_string(width) + "x" + std::to_string(height));
    
    if (width <= 0 || height <= 0) {
        return nullptr;
    }
    
    MockBitmap* bitmap = new MockBitmap(hModule, width, height);
    
    // If data is provided, copy it into the pixmap
    if (data) {
        QImage image(static_cast<const uchar*>(data), width, height, 
                    width * 4, QImage::Format_ARGB32);
        bitmap->pixmap = QPixmap::fromImage(image);
    }
    
    bitmap_handle handle = reinterpret_cast<bitmap_handle>(bitmap);
    
    std::lock_guard<std::mutex> lock(g_bitmap_map_mutex);
    g_bitmap_map[handle] = bitmap;
    
    return handle;
}

bitmap_handle API_Bitmap_CreateBitmapXPM(api_handle hModule, const char** xpm)
{
    LogDbg("CreateBitmapXPM called");
    
    if (!xpm) {
        return nullptr;
    }
    
    QPixmap pixmap(xpm);
    if (pixmap.isNull()) {
        LogDbg("Failed to create bitmap from XPM data");
        return nullptr;
    }
    
    MockBitmap* bitmap = new MockBitmap(hModule, pixmap);
    bitmap_handle handle = reinterpret_cast<bitmap_handle>(bitmap);
    
    std::lock_guard<std::mutex> lock(g_bitmap_map_mutex);
    g_bitmap_map[handle] = bitmap;
    
    return handle;
}

bitmap_handle API_Bitmap_CreateBitmapFromFile(api_handle hModule, const char16_type* filePath)
{
    if (!filePath) {
        LogDbg("CreateBitmapFromFile: null file path");
        return nullptr;
    }
    
    QString qFilePath = QString::fromUtf16(reinterpret_cast<const ushort*>(filePath));
    LogDbg("CreateBitmapFromFile called with path: " + qFilePath.toStdString());
    
    QPixmap pixmap;
    
    // Check if it's an SVG file
    QString lowerPath = qFilePath.toLower();
    if (lowerPath.endsWith(".svg") || lowerPath.endsWith(".svgz")) {
        // Load SVG - render at a reasonable default size
        QSvgRenderer renderer(qFilePath);
        if (!renderer.isValid()) {
            LogDbg("Failed to load SVG file: " + qFilePath.toStdString());
            return nullptr;
        }
        
        // Use default size from SVG, or 256x256 if not specified
        QSize size = renderer.defaultSize();
        if (!size.isValid() || size.width() <= 0 || size.height() <= 0) {
            size = QSize(256, 256);
        }
        
        pixmap = QPixmap(size);
        pixmap.fill(Qt::transparent);
        
        QPainter painter(&pixmap);
        renderer.render(&painter);
    } else {
        // Load regular image file
        pixmap.load(qFilePath);
    }
    
    if (pixmap.isNull()) {
        LogDbg("Failed to load bitmap from file: " + qFilePath.toStdString());
	return API_Bitmap_CreateBitmap(hModule, 256, 256, nullptr);
    }
    
    MockBitmap* bitmap = new MockBitmap(hModule, pixmap);
    bitmap_handle handle = reinterpret_cast<bitmap_handle>(bitmap);
    
    std::lock_guard<std::mutex> lock(g_bitmap_map_mutex);
    g_bitmap_map[handle] = bitmap;
    
    LogDbg("Successfully loaded bitmap: " + std::to_string(pixmap.width()) + "x" + 
             std::to_string(pixmap.height()));
    
    return handle;
}

bitmap_handle API_Bitmap_CreateBitmapFromFile8(api_handle hModule, const char* filePath)
{
    if (!filePath) {
        return nullptr;
    }
    
    // Convert UTF-8 to UTF-16
    QString qFilePath = QString::fromUtf8(filePath);
    std::u16string u16path = qFilePath.toStdU16String();
    
    return API_Bitmap_CreateBitmapFromFile(hModule, 
        reinterpret_cast<const char16_type*>(u16path.c_str()));
}

bitmap_handle API_Bitmap_CreateBitmapFromData(api_handle hModule, const void* data, 
                                              size_type size, const char* format, uint32 flags)
{
    LogDbg("CreateBitmapFromData called");
    
    if (!data || size == 0) {
        return nullptr;
    }
    
    QByteArray byteArray(static_cast<const char*>(data), size);
    QPixmap pixmap;
    
    if (format && format[0]) {
        pixmap.loadFromData(byteArray, format);
    } else {
        // Auto-detect format
        pixmap.loadFromData(byteArray);
    }
    
    if (pixmap.isNull()) {
        LogDbg("Failed to create bitmap from data");
        return nullptr;
    }
    
    MockBitmap* bitmap = new MockBitmap(hModule, pixmap);
    bitmap_handle handle = reinterpret_cast<bitmap_handle>(bitmap);
    
    std::lock_guard<std::mutex> lock(g_bitmap_map_mutex);
    g_bitmap_map[handle] = bitmap;
    
    return handle;
}

bitmap_handle API_Bitmap_CreateEmptyBitmap(api_handle hModule)
{
    LogDbg("CreateEmptyBitmap called");
    
    MockBitmap* bitmap = new MockBitmap(hModule);
    bitmap_handle handle = reinterpret_cast<bitmap_handle>(bitmap);
    
    std::lock_guard<std::mutex> lock(g_bitmap_map_mutex);
    g_bitmap_map[handle] = bitmap;
    
    return handle;
}

bitmap_handle API_Bitmap_CloneBitmap(api_handle hModule, const_bitmap_handle source)
{
    LogDbg("CloneBitmap called");
    
    MockBitmap* srcBitmap = GetBitmap(const_cast<bitmap_handle>(source));
    if (!srcBitmap) {
        return nullptr;
    }
    
    MockBitmap* bitmap = new MockBitmap(hModule, srcBitmap->pixmap.copy());
    bitmap->devicePixelRatio = srcBitmap->devicePixelRatio;
    
    bitmap_handle handle = reinterpret_cast<bitmap_handle>(bitmap);
    
    std::lock_guard<std::mutex> lock(g_bitmap_map_mutex);
    g_bitmap_map[handle] = bitmap;
    
    return handle;
}

bitmap_handle API_Bitmap_CloneBitmapRect(api_handle hModule, const_bitmap_handle source,
                                         int32 x, int32 y, int32 width, int32 height)
{
    LogDbg("CloneBitmapRect called");
    
    MockBitmap* srcBitmap = GetBitmap(const_cast<bitmap_handle>(source));
    if (!srcBitmap) {
        return nullptr;
    }
    
    QPixmap cropped = srcBitmap->pixmap.copy(x, y, width, height);
    if (cropped.isNull()) {
        return nullptr;
    }
    
    MockBitmap* bitmap = new MockBitmap(hModule, cropped);
    bitmap->devicePixelRatio = srcBitmap->devicePixelRatio;
    
    bitmap_handle handle = reinterpret_cast<bitmap_handle>(bitmap);
    
    std::lock_guard<std::mutex> lock(g_bitmap_map_mutex);
    g_bitmap_map[handle] = bitmap;
    
    return handle;
}

bitmap_handle API_Bitmap_CreateBitmapFromSVG(api_handle hModule, const char* svgSource,
                                             int32 width, int32 height, uint32 flags)
{
    LogDbg("CreateBitmapFromSVG called");
    
    if (!svgSource || width <= 0 || height <= 0) {
        return nullptr;
    }
    
    QByteArray svgData(svgSource);
    QSvgRenderer renderer(svgData);
    
    if (!renderer.isValid()) {
        LogDbg("Failed to parse SVG data");
        return nullptr;
    }
    
    QPixmap pixmap(width, height);
    pixmap.fill(Qt::transparent);
    
    QPainter painter(&pixmap);
    renderer.render(&painter);
    
    MockBitmap* bitmap = new MockBitmap(hModule, pixmap);
    bitmap_handle handle = reinterpret_cast<bitmap_handle>(bitmap);
    
    std::lock_guard<std::mutex> lock(g_bitmap_map_mutex);
    g_bitmap_map[handle] = bitmap;
    
    return handle;
}

bitmap_handle API_Bitmap_CreateBitmapFromSVGFile(api_handle hModule, const char16_type* filePath,
                                                 int32 width, int32 height, uint32 flags)
{
    if (!filePath) {
        return nullptr;
    }
    
    QString qFilePath = QString::fromUtf16(reinterpret_cast<const ushort*>(filePath));
    LogDbg("CreateBitmapFromSVGFile called with path: " + qFilePath.toStdString());
    
    QSvgRenderer renderer(qFilePath);
    if (!renderer.isValid()) {
        LogDbg("Failed to load SVG file: " + qFilePath.toStdString());
        return nullptr;
    }
    
    // Use provided size or default size from SVG
    QSize size(width, height);
    if (width <= 0 || height <= 0) {
        size = renderer.defaultSize();
        if (!size.isValid() || size.width() <= 0 || size.height() <= 0) {
            size = QSize(256, 256);
        }
    }
    
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);
    
    QPainter painter(&pixmap);
    renderer.render(&painter);
    
    MockBitmap* bitmap = new MockBitmap(hModule, pixmap);
    bitmap_handle handle = reinterpret_cast<bitmap_handle>(bitmap);
    
    std::lock_guard<std::mutex> lock(g_bitmap_map_mutex);
    g_bitmap_map[handle] = bitmap;
    
    return handle;
}

int32 API_Bitmap_GetBitmapFormat(bitmap_handle handle)
{
    MockBitmap* bitmap = GetBitmap(handle);
    if (!bitmap) {
        return -1;
    }
    
    // Return Qt image format
    // 0 = Invalid, 1 = Mono, 2 = MonoLSB, 3 = Indexed8, 4 = RGB32, 5 = ARGB32, etc.
    QImage image = bitmap->pixmap.toImage();
    return static_cast<int32>(image.format());
}

void API_Bitmap_SetBitmapFormat(bitmap_handle handle, int32 format)
{
    MockBitmap* bitmap = GetBitmap(handle);
    if (!bitmap) {
        return;
    }
    
    QImage image = bitmap->pixmap.toImage();
    QImage converted = image.convertToFormat(static_cast<QImage::Format>(format));
    bitmap->pixmap = QPixmap::fromImage(converted);
}

unsigned int* API_Bitmap_GetBitmapScanLine(bitmap_handle handle, int32 y)
{
    MockBitmap* bitmap = GetBitmap(handle);
    if (!bitmap) {
        return nullptr;
    }
    
    QImage image = bitmap->pixmap.toImage();
    if (y < 0 || y >= image.height()) {
        return nullptr;
    }
    
    // Return pointer to scan line
    // Note: This is dangerous - the pointer becomes invalid if the bitmap is modified
    return reinterpret_cast<unsigned int*>(image.scanLine(y));
}

api_bool API_Bitmap_GetBitmapDimensions(const_bitmap_handle handle, int32* width, int32* height)
{
    MockBitmap* bitmap = GetBitmap(const_cast<bitmap_handle>(handle));
    if (!bitmap) {
        if (width) *width = 0;
        if (height) *height = 0;
        return api_false;
    }
    
    if (width) *width = bitmap->pixmap.width();
    if (height) *height = bitmap->pixmap.height();
    
    return api_true;
}

api_bool API_Bitmap_IsEmptyBitmap(const_bitmap_handle handle)
{
    MockBitmap* bitmap = GetBitmap(const_cast<bitmap_handle>(handle));
    if (!bitmap) {
        return api_true;
    }
    
    return bitmap->pixmap.isNull() ? api_true : api_false;
}

uint32 API_Bitmap_GetBitmapPixel(const_bitmap_handle handle, int32 x, int32 y)
{
    MockBitmap* bitmap = GetBitmap(const_cast<bitmap_handle>(handle));
    if (!bitmap) {
        return 0;
    }
    
    QImage image = bitmap->pixmap.toImage();
    if (x < 0 || x >= image.width() || y < 0 || y >= image.height()) {
        return 0;
    }
    
    QRgb pixel = image.pixel(x, y);
    return static_cast<uint32>(pixel);
}

void API_Bitmap_SetBitmapPixel(bitmap_handle handle, int32 x, int32 y, uint32 color)
{
    MockBitmap* bitmap = GetBitmap(handle);
    if (!bitmap) {
        return;
    }
    
    QImage image = bitmap->pixmap.toImage();
    if (x < 0 || x >= image.width() || y < 0 || y >= image.height()) {
        return;
    }
    
    image.setPixel(x, y, static_cast<QRgb>(color));
    bitmap->pixmap = QPixmap::fromImage(image);
}

bitmap_handle API_Bitmap_MirroredBitmap(const_bitmap_handle handle, api_bool horizontal, api_bool vertical)
{
    MockBitmap* srcBitmap = GetBitmap(const_cast<bitmap_handle>(handle));
    if (!srcBitmap) {
        return nullptr;
    }
    
    QImage image = srcBitmap->pixmap.toImage();
    QImage mirrored = image.mirrored(horizontal != 0, vertical != 0);
    
    MockBitmap* bitmap = new MockBitmap(srcBitmap->moduleHandle, QPixmap::fromImage(mirrored));
    bitmap->devicePixelRatio = srcBitmap->devicePixelRatio;
    
    bitmap_handle newHandle = reinterpret_cast<bitmap_handle>(bitmap);
    
    std::lock_guard<std::mutex> lock(g_bitmap_map_mutex);
    g_bitmap_map[newHandle] = bitmap;
    
    return newHandle;
}

bitmap_handle API_Bitmap_ScaledBitmap(const_bitmap_handle handle, int32 width, int32 height, 
                                      api_bool smoothScaling)
{
    MockBitmap* srcBitmap = GetBitmap(const_cast<bitmap_handle>(handle));
    if (!srcBitmap || width <= 0 || height <= 0) {
        return nullptr;
    }
    
    Qt::TransformationMode mode = smoothScaling ? Qt::SmoothTransformation : Qt::FastTransformation;
    QPixmap scaled = srcBitmap->pixmap.scaled(width, height, Qt::IgnoreAspectRatio, mode);
    
    MockBitmap* bitmap = new MockBitmap(srcBitmap->moduleHandle, scaled);
    bitmap->devicePixelRatio = srcBitmap->devicePixelRatio;
    
    bitmap_handle newHandle = reinterpret_cast<bitmap_handle>(bitmap);
    
    std::lock_guard<std::mutex> lock(g_bitmap_map_mutex);
    g_bitmap_map[newHandle] = bitmap;
    
    return newHandle;
}

bitmap_handle API_Bitmap_RotatedBitmap(const_bitmap_handle handle, double angleDegrees, 
                                       api_bool smoothRotation)
{
    MockBitmap* srcBitmap = GetBitmap(const_cast<bitmap_handle>(handle));
    if (!srcBitmap) {
        return nullptr;
    }
    
    QTransform transform;
    transform.rotate(angleDegrees);
    
    Qt::TransformationMode mode = smoothRotation ? Qt::SmoothTransformation : Qt::FastTransformation;
    QPixmap rotated = srcBitmap->pixmap.transformed(transform, mode);
    
    MockBitmap* bitmap = new MockBitmap(srcBitmap->moduleHandle, rotated);
    bitmap->devicePixelRatio = srcBitmap->devicePixelRatio;
    
    bitmap_handle newHandle = reinterpret_cast<bitmap_handle>(bitmap);
    
    std::lock_guard<std::mutex> lock(g_bitmap_map_mutex);
    g_bitmap_map[newHandle] = bitmap;
    
    return newHandle;
}

api_bool API_Bitmap_LoadBitmap(bitmap_handle handle, const char16_type* filePath)
{
    MockBitmap* bitmap = GetBitmap(handle);
    if (!bitmap || !filePath) {
        return api_false;
    }
    
    QString qFilePath = QString::fromUtf16(reinterpret_cast<const ushort*>(filePath));
    
    bool success = bitmap->pixmap.load(qFilePath);
    
    LogDbg(success ? "Successfully loaded bitmap from: " : "Failed to load bitmap from: " + 
             qFilePath.toStdString());
    
    return success ? api_true : api_false;
}

api_bool API_Bitmap_SaveBitmap(const_bitmap_handle handle, const char16_type* filePath, int32 quality)
{
    MockBitmap* bitmap = GetBitmap(const_cast<bitmap_handle>(handle));
    if (!bitmap || !filePath) {
        return api_false;
    }
    
    QString qFilePath = QString::fromUtf16(reinterpret_cast<const ushort*>(filePath));
    
    bool success = bitmap->pixmap.save(qFilePath, nullptr, quality);
    
    LogDbg(success ? "Successfully saved bitmap to: " : "Failed to save bitmap to: " + 
             qFilePath.toStdString());
    
    return success ? api_true : api_false;
}

api_bool API_Bitmap_LoadBitmapData(bitmap_handle handle, const void* data, size_type size,
                                   const char* format, uint32 flags)
{
    MockBitmap* bitmap = GetBitmap(handle);
    if (!bitmap || !data || size == 0) {
        return api_false;
    }
    
    QByteArray byteArray(static_cast<const char*>(data), size);
    
    bool success;
    if (format && format[0]) {
        success = bitmap->pixmap.loadFromData(byteArray, format);
    } else {
        success = bitmap->pixmap.loadFromData(byteArray);
    }
    
    return success ? api_true : api_false;
}

void API_Bitmap_CopyBitmap(bitmap_handle dest, int32 destX, int32 destY,
                           const_bitmap_handle source, int32 srcX, int32 srcY,
                           int32 width, int32 height)
{
    MockBitmap* destBitmap = GetBitmap(dest);
    MockBitmap* srcBitmap = GetBitmap(const_cast<bitmap_handle>(source));
    
    if (!destBitmap || !srcBitmap) {
        return;
    }
    
    QImage destImage = destBitmap->pixmap.toImage();
    QImage srcImage = srcBitmap->pixmap.toImage();
    
    QPainter painter(&destImage);
    painter.drawImage(destX, destY, srcImage, srcX, srcY, width, height);
    painter.end();
    
    destBitmap->pixmap = QPixmap::fromImage(destImage);
}

void API_Bitmap_FillBitmap(bitmap_handle handle, int32 x, int32 y, int32 width, int32 height,
                           uint32 color)
{
    MockBitmap* bitmap = GetBitmap(handle);
    if (!bitmap) {
        return;
    }
    
    QImage image = bitmap->pixmap.toImage();
    QPainter painter(&image);
    painter.fillRect(x, y, width, height, QColor::fromRgba(color));
    painter.end();
    
    bitmap->pixmap = QPixmap::fromImage(image);
}

void API_Bitmap_GetBitmapDevicePixelRatio(const_bitmap_handle handle, double* ratio)
{
    MockBitmap* bitmap = GetBitmap(const_cast<bitmap_handle>(handle));
    if (!bitmap || !ratio) {
        if (ratio) *ratio = 1.0;
        return;
    }
    
    *ratio = bitmap->pixmap.devicePixelRatio();
}

void API_Bitmap_SetBitmapDevicePixelRatio(bitmap_handle handle, double ratio)
{
    MockBitmap* bitmap = GetBitmap(handle);
    if (!bitmap || ratio <= 0.0) {
        return;
    }
    
    bitmap->pixmap.setDevicePixelRatio(ratio);
    bitmap->devicePixelRatio = ratio;
}


// ----------------------------------------------------------------------------
// ComboBox Context Implementation
// ----------------------------------------------------------------------------

struct MockComboBox {
    QComboBox* comboBox;
    api_handle clientHandle;
    
    MockComboBox() 
        : comboBox(new QComboBox()),
          clientHandle(nullptr)
    {
    }
    
    ~MockComboBox() {
        // Qt parent ownership handles deletion
    }
};

// Global map to track comboboxes
static std::map<control_handle, MockComboBox*> g_combobox_map;
static std::mutex g_combobox_map_mutex;

control_handle API_ComboBox_CreateComboBox(api_handle, api_handle client, 
                                           control_handle parent, uint32 flags)
{
    LogDbg("CreateComboBox called");
    
    MockComboBox* combo = new MockComboBox();
    combo->clientHandle = client;
    
    if (parent) {
        QWidget* parentWidget = reinterpret_cast<QWidget*>(parent);
        combo->comboBox->setParent(parentWidget);
    }
    
    control_handle handle = reinterpret_cast<control_handle>(combo->comboBox);
    
    std::lock_guard<std::mutex> lock(g_combobox_map_mutex);
    g_combobox_map[handle] = combo;
    
    return handle;
}

int32 API_ComboBox_GetComboBoxLength(const_control_handle handle)
{
    std::lock_guard<std::mutex> lock(g_combobox_map_mutex);
    auto it = g_combobox_map.find(const_cast<control_handle>(handle));
    if (it == g_combobox_map.end()) {
        return 0;
    }
    
    return it->second->comboBox->count();
}

int32 API_ComboBox_GetComboBoxCurrentItem(const_control_handle handle)
{
    std::lock_guard<std::mutex> lock(g_combobox_map_mutex);
    auto it = g_combobox_map.find(const_cast<control_handle>(handle));
    if (it == g_combobox_map.end()) {
        return -1;
    }
    
    return it->second->comboBox->currentIndex();
}

void API_ComboBox_SetComboBoxCurrentItem(control_handle handle, int32 index)
{
    LogDbg("SetComboBoxCurrentItem called, index=" + std::to_string(index));
    
    std::lock_guard<std::mutex> lock(g_combobox_map_mutex);
    auto it = g_combobox_map.find(handle);
    if (it == g_combobox_map.end()) {
        return;
    }
    
    it->second->comboBox->setCurrentIndex(index);
}

void API_ComboBox_InsertComboBoxItem(control_handle handle, int32 index, 
                                     const char16_type* text, const_bitmap_handle icon)
{
    LogDbg("InsertComboBoxItem called");
    
    std::lock_guard<std::mutex> lock(g_combobox_map_mutex);
    auto it = g_combobox_map.find(handle);
    if (it == g_combobox_map.end()) {
        return;
    }
    
    QString qtext;
    if (text) {
        qtext = QString::fromUtf16(reinterpret_cast<const ushort*>(text));
    }
    
    if (icon) {
        QPixmap* pixmap = reinterpret_cast<QPixmap*>(const_cast<void*>(icon));
        if (pixmap) {
            it->second->comboBox->insertItem(index, QIcon(*pixmap), qtext);
        } else {
            it->second->comboBox->insertItem(index, qtext);
        }
    } else {
        it->second->comboBox->insertItem(index, qtext);
    }
}

void API_ComboBox_ClearComboBox(control_handle handle)
{
    LogDbg("ClearComboBox called");
    
    std::lock_guard<std::mutex> lock(g_combobox_map_mutex);
    auto it = g_combobox_map.find(handle);
    if (it == g_combobox_map.end()) {
        return;
    }
    
    it->second->comboBox->clear();
}


// ----------------------------------------------------------------------------
// CheckBox and RadioButton implementations
// ----------------------------------------------------------------------------

control_handle API_Button_CreateCheckBox(api_handle hModule, api_handle client, 
                                         const char16_type* text, control_handle parent, uint32 flags)
{
    auto textStr = text ? Utf16ToUtf8(text) : "";
    LogDbg("CreateCheckBox called, text=" + textStr);
    
    MockButton* btn = new MockButton(false);
    btn->clientHandle = client;
    btn->checkable = true;
    
    // Replace the button with a QCheckBox
    delete btn->button;
    btn->button = new QCheckBox();
    btn->isToolButton = false;
    
    QCheckBox* checkBox = static_cast<QCheckBox*>(btn->button);
    checkBox->setCheckable(true);
    
    if (text && *text) {
        checkBox->setText(QString::fromUtf16(reinterpret_cast<const ushort*>(text)));
    }
    
    if (parent) {
        QWidget* parentWidget = reinterpret_cast<QWidget*>(parent);
        checkBox->setParent(parentWidget);
    }
    
    control_handle handle = reinterpret_cast<control_handle>(btn->button);
    
    std::lock_guard<std::mutex> lock(g_button_map_mutex);
    g_button_map[handle] = btn;
    
    return handle;
}

control_handle API_Button_CreateRadioButton(api_handle hModule, api_handle client,
                                            const char16_type* text, control_handle parent, uint32 flags)
{
    auto textStr = text ? Utf16ToUtf8(text) : "";
    LogDbg("CreateRadioButton called, text=" + textStr);
    
    MockButton* btn = new MockButton(false);
    btn->clientHandle = client;
    btn->checkable = true;
    
    // Replace the button with a QRadioButton
    delete btn->button;
    btn->button = new QRadioButton();
    btn->isToolButton = false;
    
    QRadioButton* radioButton = static_cast<QRadioButton*>(btn->button);
    radioButton->setCheckable(true);
    
    if (text && *text) {
        radioButton->setText(QString::fromUtf16(reinterpret_cast<const ushort*>(text)));
    }
    
    if (parent) {
        QWidget* parentWidget = reinterpret_cast<QWidget*>(parent);
        radioButton->setParent(parentWidget);
    }
    
    control_handle handle = reinterpret_cast<control_handle>(btn->button);
    
    std::lock_guard<std::mutex> lock(g_button_map_mutex);
    g_button_map[handle] = btn;
    
    return handle;
}


// ----------------------------------------------------------------------------
// Edit Context Implementation
// ----------------------------------------------------------------------------
struct MockEdit {
    QLineEdit* edit;

    // PCL objects:
    void* pclEdit;               // pcl::Edit* (used as hSender)
    void* editCompletedReceiver; // pcl::Control* (NumericControl etc.)

    pcl::event_routine editCompletedHandler;

    // Other handlers, as you already have:
    pcl::event_routine returnPressedHandler;
    void* returnPressedReceiver;

    pcl::unicode_event_routine textUpdatedHandler;
    void* textUpdatedReceiver;

    pcl::range_event_routine caretPositionUpdatedHandler;
    void* caretPositionUpdatedReceiver;

    pcl::range_event_routine selectionUpdatedHandler;
    void* selectionUpdatedReceiver;

    QRegularExpressionValidator* validator;

    MockEdit(const char16_type* text = nullptr)
        : edit(new QLineEdit())
        , pclEdit(nullptr)
        , editCompletedReceiver(nullptr)
        , editCompletedHandler(nullptr)
        , returnPressedHandler(nullptr)
        , returnPressedReceiver(nullptr)
        , textUpdatedHandler(nullptr)
        , textUpdatedReceiver(nullptr)
        , caretPositionUpdatedHandler(nullptr)
        , caretPositionUpdatedReceiver(nullptr)
        , selectionUpdatedHandler(nullptr)
        , selectionUpdatedReceiver(nullptr)
        , validator(nullptr)
    {
        if (text && *text)
            edit->setText(QString::fromUtf16(
                reinterpret_cast<const ushort*>(text)));
    }

    ~MockEdit()
    {
        delete validator;
    }
};
  
// Global map to track edits
static std::map<control_handle, MockEdit*> g_edit_map;
static std::mutex g_edit_map_mutex;

control_handle API_Edit_CreateEdit(api_handle /*module*/,
                                   api_handle client,
                                   const char16_type* text,
                                   control_handle parent,
                                   uint32 /*flags*/)
{
    LogDbg("CreateEdit called");

    MockEdit* mock = new MockEdit(text);

    // 1) store the PCL Edit* (sender for EditCompleted)
    mock->pclEdit = client; // this is exactly 'this' from pcl::Edit ctor

    // 2) parent the Qt widget if needed
    if (parent) {
        QWidget* parentWidget = reinterpret_cast<QWidget*>(parent);
        mock->edit->setParent(parentWidget);
    }

    // 3) This is the handle PCL will use for this edit control
    control_handle handle = reinterpret_cast<control_handle>(mock->edit);

    {
        std::lock_guard<std::mutex> lock(g_edit_map_mutex);
        g_edit_map[handle] = mock;
    }

    return handle;
}
  
api_bool API_Edit_GetEditText(const_control_handle handle, char16_type* text, size_type* len)
{
    LogDbg("GetEditText called");
    
    std::lock_guard<std::mutex> lock(g_edit_map_mutex);
    auto it = g_edit_map.find(const_cast<control_handle>(handle));
    if (it == g_edit_map.end()) {
        if (len) *len = 0;
        return api_false;
    }
    
    QString qtext = it->second->edit->text();
    std::u16string u16text = qtext.toStdU16String();
    
    if (text == nullptr) {
        if (len) *len = u16text.length();
        return api_true;
    }
    
    if (len && *len > 0) {
        size_type copyLen = std::min(*len - 1, u16text.length());
        std::memcpy(text, u16text.c_str(), copyLen * sizeof(char16_type));
        text[copyLen] = 0;
        *len = copyLen;
    }
    
    return api_true;
}

void API_Edit_SetEditText(control_handle handle, const char16_type* text)
{
    LogDbg("SetEditText called");
    
    std::lock_guard<std::mutex> lock(g_edit_map_mutex);
    auto it = g_edit_map.find(handle);
    if (it == g_edit_map.end()) {
        return;
    }
    
    if (text) {
        it->second->edit->setText(QString::fromUtf16(reinterpret_cast<const ushort*>(text)));
    } else {
        it->second->edit->clear();
    }
}

api_bool API_Edit_GetEditReadOnly(const_control_handle handle)
{
    std::lock_guard<std::mutex> lock(g_edit_map_mutex);
    auto it = g_edit_map.find(const_cast<control_handle>(handle));
    if (it == g_edit_map.end()) {
        return api_false;
    }
    
    return it->second->edit->isReadOnly() ? api_true : api_false;
}

void API_Edit_SetEditReadOnly(control_handle handle, api_bool readOnly)
{
    LogDbg("SetEditReadOnly called");
    
    std::lock_guard<std::mutex> lock(g_edit_map_mutex);
    auto it = g_edit_map.find(handle);
    if (it == g_edit_map.end()) {
        return;
    }
    
    it->second->edit->setReadOnly(readOnly != 0);
}

// ----------------------------------------------------------------------------
// TextBox Context Implementation (QTextEdit)
// ----------------------------------------------------------------------------

struct MockTextBox {
    QTextEdit* textEdit;
    api_handle clientHandle;
    
    MockTextBox(const char16_type* text = nullptr) 
        : textEdit(new QTextEdit()),
          clientHandle(nullptr)
    {
        if (text && *text) {
            textEdit->setText(QString::fromUtf16(reinterpret_cast<const ushort*>(text)));
        }
    }
    
    ~MockTextBox() {
        // Qt parent ownership handles deletion
    }
};

// Global map to track textboxes
static std::map<control_handle, MockTextBox*> g_textbox_map;
static std::mutex g_textbox_map_mutex;

control_handle API_TextBox_CreateTextBox(api_handle, api_handle client, const char16_type* text,
                                         control_handle parent, uint32 flags)
{
    LogDbg("CreateTextBox called");
    
    MockTextBox* textbox = new MockTextBox(text);
    textbox->clientHandle = client;
    
    if (parent) {
        QWidget* parentWidget = reinterpret_cast<QWidget*>(parent);
        textbox->textEdit->setParent(parentWidget);
    }
    
    control_handle handle = reinterpret_cast<control_handle>(textbox->textEdit);
    
    std::lock_guard<std::mutex> lock(g_textbox_map_mutex);
    g_textbox_map[handle] = textbox;
    
    return handle;
}

api_bool API_TextBox_GetTextBoxText(const_control_handle handle, char16_type* text, size_type* len)
{
    std::lock_guard<std::mutex> lock(g_textbox_map_mutex);
    auto it = g_textbox_map.find(const_cast<control_handle>(handle));
    if (it == g_textbox_map.end()) {
        if (len) *len = 0;
        return api_false;
    }
    
    QString qtext = it->second->textEdit->toPlainText();
    std::u16string u16text = qtext.toStdU16String();
    
    if (text == nullptr) {
        if (len) *len = u16text.length();
        return api_true;
    }
    
    if (len && *len > 0) {
        size_type copyLen = std::min(*len - 1, u16text.length());
        std::memcpy(text, u16text.c_str(), copyLen * sizeof(char16_type));
        text[copyLen] = 0;
        *len = copyLen;
    }
    
    return api_true;
}

void API_TextBox_SetTextBoxText(control_handle handle, const char16_type* text)
{
    std::lock_guard<std::mutex> lock(g_textbox_map_mutex);
    auto it = g_textbox_map.find(handle);
    if (it == g_textbox_map.end()) {
        return;
    }
    
    if (text) {
        it->second->textEdit->setText(QString::fromUtf16(reinterpret_cast<const ushort*>(text)));
    } else {
        it->second->textEdit->clear();
    }
}

// ----------------------------------------------------------------------------
// Slider Context Implementation
// ----------------------------------------------------------------------------

struct MockSlider {
    QSlider* slider;
    api_handle clientHandle;
    
    MockSlider(bool vertical) 
        : slider(new QSlider(vertical ? Qt::Vertical : Qt::Horizontal)),
          clientHandle(nullptr)
    {
    }
    
    ~MockSlider() {
        // Qt parent ownership handles deletion
    }
};

// Global map to track sliders
static std::map<control_handle, MockSlider*> g_slider_map;
static std::mutex g_slider_map_mutex;

control_handle API_Slider_CreateSlider(api_handle, api_handle client, api_bool vertical,
                                       control_handle parent, uint32 flags)
{
    LogDbg("CreateSlider called, vertical=" + std::to_string(vertical));
    
    MockSlider* slider = new MockSlider(vertical != 0);
    slider->clientHandle = client;
    
    if (parent) {
        QWidget* parentWidget = reinterpret_cast<QWidget*>(parent);
        slider->slider->setParent(parentWidget);
    }
    
    control_handle handle = reinterpret_cast<control_handle>(slider->slider);
    
    std::lock_guard<std::mutex> lock(g_slider_map_mutex);
    g_slider_map[handle] = slider;
    
    return handle;
}

int32 API_Slider_GetSliderValue(const_control_handle handle)
{
    std::lock_guard<std::mutex> lock(g_slider_map_mutex);
    auto it = g_slider_map.find(const_cast<control_handle>(handle));
    if (it == g_slider_map.end()) {
        return 0;
    }
    
    return it->second->slider->value();
}

void API_Slider_SetSliderValue(control_handle handle, int32 value)
{
    LogDbg("SetSliderValue called, value=" + std::to_string(value));
    
    std::lock_guard<std::mutex> lock(g_slider_map_mutex);
    auto it = g_slider_map.find(handle);
    if (it == g_slider_map.end()) {
        return;
    }
    
    it->second->slider->setValue(value);
}

void API_Slider_GetSliderRange(const_control_handle handle, int32* minValue, int32* maxValue)
{
    std::lock_guard<std::mutex> lock(g_slider_map_mutex);
    auto it = g_slider_map.find(const_cast<control_handle>(handle));
    if (it == g_slider_map.end()) {
        if (minValue) *minValue = 0;
        if (maxValue) *maxValue = 100;
        return;
    }
    
    if (minValue) *minValue = it->second->slider->minimum();
    if (maxValue) *maxValue = it->second->slider->maximum();
}

void API_Slider_SetSliderRange(control_handle handle, int32 minValue, int32 maxValue)
{
    LogDbg("SetSliderRange called");
    
    std::lock_guard<std::mutex> lock(g_slider_map_mutex);
    auto it = g_slider_map.find(handle);
    if (it == g_slider_map.end()) {
        return;
    }
    
    it->second->slider->setRange(minValue, maxValue);
}

struct MockGroupBox {
    QGroupBox* box;
    api_handle clientHandle;

    MockGroupBox() :
        box(new QGroupBox()),
        clientHandle(nullptr)
    {
    }

    ~MockGroupBox() {
        // QGroupBox is cleaned up by Qt parent ownership
    }
};

static std::map<control_handle, MockGroupBox*> g_groupbox_map;
static std::mutex g_groupbox_map_mutex;

struct MockTimer {
    QTimer* timer;
    api_handle clientHandle;
    pcl::timer_event_routine timeoutHandler;

    MockTimer()
        : timer(new QTimer()),
          clientHandle(nullptr),
          timeoutHandler(nullptr)
    {
        timer->setSingleShot(false); // PixInsight timers are repeating by default
    }

    ~MockTimer() {
        // QTimer deleted by Qt parent hierarchy if parented
    }
};

static std::map<control_handle, MockTimer*> g_timer_map;
static std::mutex g_timer_map_mutex;

struct MockFont {
    QFont font;
    api_handle clientHandle;

    MockFont(const QFont& f, api_handle client)
        : font(f)
        , clientHandle(client)
    {
    }
};

static std::map<const_font_handle, MockFont*> g_font_map;
static std::mutex g_font_map_mutex;

font_handle API_Font_CreateFontByFamily(api_handle client, int32 weight, double sizePt)
{
    QString family = QFont().defaultFamily();  // default family
    QFont f(family, sizePt);
    f.setPointSizeF(sizePt);
    f.setWeight(weight);

    MockFont* mf = new MockFont(f, client);

    font_handle handle = reinterpret_cast<font_handle>(mf);
    {
        std::lock_guard<std::mutex> lock(g_font_map_mutex);
        g_font_map[handle] = mf;
    }
    return handle;
}

font_handle API_Font_CreateFontByFace(api_handle client, const char16_type* face, double ptSize)
{
    QString family = QString::fromUtf16(reinterpret_cast<const ushort*>(face));
    QFont f(family, ptSize);
    f.setPointSizeF(ptSize);

    MockFont* mf = new MockFont(f, client);

    font_handle h = reinterpret_cast<font_handle>(mf);
    {
        std::lock_guard<std::mutex> lock(g_font_map_mutex);
        g_font_map[h] = mf;
    }
    return h;
}

font_handle API_Control_GetControlFont(const_control_handle handle)
{
    LogDbg("API_Control_GetControlFont called");

    const QWidget* w = reinterpret_cast<const QWidget*>(handle);
    if (!w)
        return nullptr;

    QFont f = w->font();

    MockFont* mf = new MockFont(f, nullptr);

    font_handle h = reinterpret_cast<font_handle>(mf);
    {
        std::lock_guard<std::mutex> lock(g_font_map_mutex);
        g_font_map[h] = mf;
    }

    return h;
}

struct MockCursor {
    QCursor cursor;
    api_handle clientHandle;

    MockCursor(const QCursor& c, api_handle client)
        : cursor(c)
        , clientHandle(client)
    {
    }
};

static std::map<cursor_handle, MockCursor*> g_cursor_map;
static std::mutex g_cursor_map_mutex;

cursor_handle API_Cursor_CreateCursor(api_handle client,
                                      int32 hot)
{
    LogDbg("API_Cursor_CreateCursor called");

    QString bitmapFile;
    QString maskFile;
    QCursor qc;

    if (!bitmapFile.isEmpty())
    {
        QPixmap pm(bitmapFile);
        if (!pm.isNull())
        {
            if (!maskFile.isEmpty()) {
                QBitmap mask(maskFile);
                if (!mask.isNull())
                    pm.setMask(mask);
            }
            qc = QCursor(pm, hot, hot);
        }
        else {
            LogDbg("[Cursor] Failed to load bitmap: " + bitmapFile);
            qc = QCursor(Qt::ArrowCursor);
        }
    }
    else {
        // No-file case: default to Arrow cursor
        qc = QCursor(Qt::ArrowCursor);
    }

    MockCursor* mc = new MockCursor(qc, client);

    cursor_handle h = reinterpret_cast<cursor_handle>(mc);

    {
        std::lock_guard<std::mutex> lock(g_cursor_map_mutex);
        g_cursor_map[h] = mc;
    }

    return h;
}

struct MockImageWindow {
    QWidget* window;       // or QMainWindow / QDialog
    api_handle clientHandle;
    image_handle currentImage;
    // plus whatever else you store for image data, views, etc.
};

static std::map<window_handle, MockImageWindow*> g_image_window_map;
static std::mutex g_image_window_map_mutex;

struct MockView {
    MockImage* image;
    QString identifier;
    api_handle clientHandle;
};

// View handling
static std::map<const_view_handle, MockView*> g_view_map;
static std::mutex g_view_map_mutex;

// -----------------------------------------------------------------------------
// TreeBox Mock API
// -----------------------------------------------------------------------------

// Small helper – avoids dynamic_cast on non-polymorphic MockControl.
static MockTreeBox* GetTreeBox( control_handle h )
{
    std::lock_guard<std::mutex> lock(g_control_map_mutex);
    auto it = g_control_map.find( h );
    if ( it == g_control_map.end() )
        return nullptr;
    return static_cast<MockTreeBox*>( it->second );
}

// -----------------------------------------------------------------------------
// Column management
// -----------------------------------------------------------------------------

api_bool API_TreeBox_SetNumberOfColumns( control_handle h, int32 n )
{
    MockTreeBox* tb = GetTreeBox( h );
    if ( !tb )
        return api_false;

    tb->columns = (n > 0) ? n : 1;
    return api_true;
}

api_bool API_TreeBox_GetNumberOfColumns( control_handle h, int32* n )
{
    MockTreeBox* tb = GetTreeBox( h );
    if ( !tb || n == nullptr )
        return api_false;

    *n = tb->columns;
    return api_true;
}

api_bool API_TreeBox_AdjustColumnWidthToContents( control_handle, int32 )
{
    // No-op in mock
    return api_true;
}

// -----------------------------------------------------------------------------
// Node management
// -----------------------------------------------------------------------------

api_bool API_TreeBox_Clear( control_handle h )
{
    MockTreeBox* tb = GetTreeBox( h );
    if ( !tb )
        return api_false;

    for ( MockTreeNode* n : tb->nodes )
        delete n;
    tb->nodes.clear();
    return api_true;
}

api_bool API_TreeBox_GetNumberOfNodes( control_handle h, int32* n )
{
    MockTreeBox* tb = GetTreeBox( h );
    if ( !tb || n == nullptr )
        return api_false;

    *n = int32( tb->nodes.size() );
    return api_true;
}

control_handle API_TreeBox_InsertNode( control_handle h, int32 index )
{
    MockTreeBox* tb = GetTreeBox( h );
    if ( !tb )
        return nullptr;

    if ( index < 0 || index > int32( tb->nodes.size() ) )
        index = int32( tb->nodes.size() );

    MockTreeNode* node = new MockTreeNode( tb->columns );
    tb->nodes.insert( tb->nodes.begin() + index, node );

    return reinterpret_cast<control_handle>( node );
}

api_bool API_TreeBox_RemoveNode( control_handle h, int32 index )
{
    MockTreeBox* tb = GetTreeBox( h );
    if ( !tb )
        return api_false;

    if ( index < 0 || index >= int32( tb->nodes.size() ) )
        return api_false;

    delete tb->nodes[index];
    tb->nodes.erase( tb->nodes.begin() + index );
    return api_true;
}

control_handle API_TreeBox_GetNode( control_handle h, int32 index )
{
    MockTreeBox* tb = GetTreeBox( h );
    if ( !tb )
        return nullptr;

    if ( index < 0 || index >= int32( tb->nodes.size() ) )
        return nullptr;

    return reinterpret_cast<control_handle>( tb->nodes[index] );
}

api_bool API_TreeBox_GetNodeIndex( control_handle h,
                                   control_handle nodeHandle,
                                   int32* index )
{
    MockTreeBox* tb      = GetTreeBox( h );
    MockTreeNode* target = reinterpret_cast<MockTreeNode*>( nodeHandle );

    if ( !tb || !index || !target )
        return api_false;

    for ( size_t i = 0; i < tb->nodes.size(); ++i )
        if ( tb->nodes[i] == target )
        {
            *index = int32( i );
            return api_true;
        }

    return api_false;
}

// -----------------------------------------------------------------------------
// Node text & icon & tooltip
// -----------------------------------------------------------------------------

api_bool API_TreeBox_SetNodeText( control_handle hNode, int32 col, const char* text )
{
    MockTreeNode* node = reinterpret_cast<MockTreeNode*>( hNode );
    if ( !node )
        return api_false;

    if ( col < 0 || col >= int32( node->text.size() ) )
        return api_false;

    node->text[col] = text ? String( text ) : String();
    return api_true;
}

api_bool API_TreeBox_GetNodeText( control_handle hNode, int32 col,
                                  char* buffer, size_type* len )
{
    MockTreeNode* node = reinterpret_cast<MockTreeNode*>( hNode );
    if ( !node || !len )
        return api_false;

    if ( col < 0 || col >= int32( node->text.size() ) )
        return api_false;

    const String& t = node->text[col];

    if ( buffer == nullptr )
    {
        *len = t.Length();
        return api_true;
    }

    // PCL strings are UTF-16 (char16_type). We just copy the raw data.
    ::memcpy( buffer, t.c_str(), t.Length() * sizeof( char16_type ) );
    return api_true;
}

api_bool API_TreeBox_SetNodeIcon( control_handle hNode, int32 col, control_handle icon )
{
    MockTreeNode* node = reinterpret_cast<MockTreeNode*>( hNode );
    if ( !node )
        return api_false;

    if ( col < 0 || col >= int32( node->icon.size() ) )
        return api_false;

    node->icon[col] = icon;
    return api_true;
}

api_bool API_TreeBox_SetNodeToolTip( control_handle hNode, int32 col, const char* text )
{
    MockTreeNode* node = reinterpret_cast<MockTreeNode*>( hNode );
    if ( !node )
        return api_false;

    if ( col < 0 || col >= int32( node->tooltip.size() ) )
        return api_false;

    node->tooltip[col] = text ? String( text ) : String();
    return api_true;
}

// -----------------------------------------------------------------------------
// Selection
// -----------------------------------------------------------------------------

api_bool API_TreeBox_SelectNode( control_handle h,
                                 control_handle hNode,
                                 api_bool selected )
{
    MockTreeBox*  tb   = GetTreeBox( h );
    MockTreeNode* node = reinterpret_cast<MockTreeNode*>( hNode );

    if ( !tb || !node )
        return api_false;

    if ( !tb->multipleSelections )
        for ( MockTreeNode* n : tb->nodes )
            n->selected = false;

    node->selected = (selected != api_false);
    return api_true;
}

api_bool API_TreeBox_IsNodeSelected( control_handle hNode, api_bool* result )
{
    MockTreeNode* node = reinterpret_cast<MockTreeNode*>( hNode );
    if ( !node || !result )
        return api_false;

    *result = node->selected ? api_true : api_false;
    return api_true;
}

api_bool API_TreeBox_SelectAllNodes( control_handle h )
{
    MockTreeBox* tb = GetTreeBox( h );
    if ( !tb )
        return api_false;

    if ( !tb->multipleSelections )
        return api_false;

    for ( MockTreeNode* n : tb->nodes )
        n->selected = true;
    return api_true;
}

api_bool API_TreeBox_HasSelectedNodes( control_handle h, api_bool* r )
{
    MockTreeBox* tb = GetTreeBox( h );
    if ( !tb || !r )
        return api_false;

    *r = api_false;
    for ( MockTreeNode* n : tb->nodes )
        if ( n->selected )
        {
            *r = api_true;
            break;
        }

    return api_true;
}

// -----------------------------------------------------------------------------
// Navigation
// -----------------------------------------------------------------------------

api_bool API_TreeBox_GetCurrentNode( control_handle h, control_handle* node )
{
    MockTreeBox* tb = GetTreeBox( h );
    if ( !tb || !node )
        return api_false;

    for ( MockTreeNode* n : tb->nodes )
        if ( n->selected )
        {
            *node = reinterpret_cast<control_handle>( n );
            return api_true;
        }

    *node = nullptr;
    return api_true;
}

api_bool API_TreeBox_SetCurrentNode( control_handle h, control_handle hNode )
{
    MockTreeBox*  tb   = GetTreeBox( h );
    MockTreeNode* node = reinterpret_cast<MockTreeNode*>( hNode );

    if ( !tb || !node )
        return api_false;

    for ( MockTreeNode* n : tb->nodes )
        n->selected = false;

    node->selected = true;
    return api_true;
}

api_bool API_TreeBox_GetNextNode( control_handle h,
                                  control_handle hNode,
                                  control_handle* next )
{
    MockTreeBox*  tb   = GetTreeBox( h );
    MockTreeNode* node = reinterpret_cast<MockTreeNode*>( hNode );

    if ( !tb || !node || !next )
        return api_false;

    for ( size_t i = 0; i < tb->nodes.size(); ++i )
        if ( tb->nodes[i] == node )
        {
            if ( i + 1 < tb->nodes.size() )
                *next = reinterpret_cast<control_handle>( tb->nodes[i+1] );
            else
                *next = nullptr;
            return api_true;
        }

    return api_false;
}

api_bool API_TreeBox_GetPrevNode( control_handle h,
                                  control_handle hNode,
                                  control_handle* prev )
{
    MockTreeBox*  tb   = GetTreeBox( h );
    MockTreeNode* node = reinterpret_cast<MockTreeNode*>( hNode );

    if ( !tb || !node || !prev )
        return api_false;

    for ( size_t i = 0; i < tb->nodes.size(); ++i )
        if ( tb->nodes[i] == node )
        {
            if ( i > 0 )
                *prev = reinterpret_cast<control_handle>( tb->nodes[i-1] );
            else
                *prev = nullptr;
            return api_true;
        }

    return api_false;
}

// -----------------------------------------------------------------------------
// Settings
// -----------------------------------------------------------------------------

api_bool API_TreeBox_EnableMultipleSelections( control_handle h, api_bool e )
{
    MockTreeBox* tb = GetTreeBox( h );
    if ( !tb )
        return api_false;

    tb->multipleSelections = (e != api_false);
    return api_true;
}

api_bool API_TreeBox_DisableRootDecoration( control_handle h, api_bool )
{
    MockTreeBox* tb = GetTreeBox( h );
    if ( !tb )
        return api_false;

    tb->rootDecoration = false;
    return api_true;
}

api_bool API_TreeBox_EnableAlternateRowColor( control_handle h, api_bool e )
{
    MockTreeBox* tb = GetTreeBox( h );
    if ( !tb )
        return api_false;

    tb->alternateRowColor = (e != api_false);
    return api_true;
}

// -----------------------------------------------------------------------------
// Height management
// -----------------------------------------------------------------------------

api_bool API_TreeBox_SetFixedHeight( control_handle, int32 )
{
    return api_true; // no-op
}

api_bool API_TreeBox_SetMinHeight( control_handle, int32 )
{
    return api_true;
}

api_bool API_TreeBox_SetMaxHeight( control_handle, int32 )
{
    return api_true;
}

// -----------------------------------------------------------------------------
// Viewport
// -----------------------------------------------------------------------------

control_handle API_TreeBox_GetViewportHandle( control_handle h )
{
    MockTreeBox* tb = GetTreeBox( h );
    return tb ? tb->viewport : nullptr;
}

// -----------------------------------------------------------------------------
// Event routines (stored but not auto-invoked in the mock)
// -----------------------------------------------------------------------------

typedef void *treebox_currentnodeupdated_event;
typedef void * treebox_node_event ;
typedef void * treebox_tree_event ;

struct TreeBoxEventRoutines
{
    treebox_currentnodeupdated_event currentNodeUpdated = nullptr;
    treebox_node_event               nodeActivated      = nullptr;
    treebox_tree_event               selectionUpdated   = nullptr;
};

static std::map<control_handle, TreeBoxEventRoutines> g_tree_events;

api_bool API_TreeBox_SetCurrentNodeUpdatedEventRoutine( control_handle h,
                                                        control_handle /*receiver*/,
                                                        treebox_currentnodeupdated_event f )
{
    g_tree_events[h].currentNodeUpdated = f;
    return api_true;
}

api_bool API_TreeBox_SetNodeActivatedEventRoutine( control_handle h,
                                                   control_handle /*receiver*/,
                                                   treebox_node_event f )
{
    g_tree_events[h].nodeActivated = f;
    return api_true;
}

api_bool API_TreeBox_SetNodeSelectionUpdatedEventRoutine( control_handle h,
                                                          control_handle /*receiver*/,
                                                          treebox_tree_event f )
{
    g_tree_events[h].selectionUpdated = f;
    return api_true;
}

// Mock implementations for Global API functions

// Simple error code storage
static int g_last_error = 0;

// Mock console handle
static void* g_console_handle = (void*)0xDEADBEEF;

// Mock pixel traits LUT - create a simple lookup table
struct MockPixelTraitsLUT {
    // These would normally be function pointers, but we'll make them simple values
    int sample_format;
    int bytes_per_sample;
    int bits_per_sample;
    double min_sample_value;
    double max_sample_value;
};

// Create mock LUTs for different pixel formats
static MockPixelTraitsLUT g_pixel_luts[16] = {
    // Format 0: 8-bit unsigned integer
    { 0, 1, 8, 0.0, 255.0 },
    // Format 1: 16-bit unsigned integer  
    { 1, 2, 16, 0.0, 65535.0 },
    // Format 2: 32-bit unsigned integer
    { 2, 4, 32, 0.0, 4294967295.0 },
    // Format 3: 32-bit IEEE 754 floating point
    { 3, 4, 32, 0.0, 1.0 },
    // Format 4: 64-bit IEEE 754 floating point
    { 4, 8, 64, 0.0, 1.0 },
    // Add more formats as needed...
};

// ----------------------------------------------------------------------------
// Global Settings mock
// ----------------------------------------------------------------------------

static std::mutex g_settings_mutex;

// settings[module][key] = int
static std::map<api_handle, std::map<std::string, int32>> g_settings_local;
static std::map<std::string, int32> g_settings_global;

static void preload_default_global_settings()
{
    std::lock_guard<std::mutex> lock(g_settings_mutex);

    if (!g_settings_global.count("Workspace/PrimaryScreenCenterX"))
        g_settings_global["Workspace/PrimaryScreenCenterX"] = 400;

    if (!g_settings_global.count("Workspace/PrimaryScreenCenterY"))
        g_settings_global["Workspace/PrimaryScreenCenterY"] = 300;
}

api_bool API_Global_ReadSettingsInteger( api_handle module,
                                         int32*     outValue,
                                         const char* key,
                                         api_bool   global )
{
    LogDbg("API_Global_ReadSettingsInteger called");
    preload_default_global_settings();
 
    if (!outValue || !key)
        return api_false;

    std::string skey(key);

    std::lock_guard<std::mutex> lock(g_settings_mutex);

    if (global)
    {
        auto it = g_settings_global.find(skey);
        if (it == g_settings_global.end())
	  {
	    LogDbg("API_Global_ReadSettingsInteger missing: " + skey);
            return api_false;
	  }
        *outValue = it->second;
        return api_true;
    }
    else
    {
        auto modIt = g_settings_local.find(module);
        if (modIt == g_settings_local.end())
	  {
	    LogDbg("API_Local_ReadSettingsInteger missing: " + skey);
            return api_false;
	  }
	
        auto& m = modIt->second;
        auto it = m.find(skey);
        if (it == m.end())
            return api_false;

        *outValue = it->second;
        return api_true;
    }
}

// Get the PixInsight version
void API_Global_GetPixInsightVersion(uint32_t* major, uint32_t* minor, uint32_t* release, uint32_t* revision, uint32_t* beta, uint32_t* conf, uint32_t* le, char16_type* lang) {
    LogDbg("GetPixInsightVersion called");
    
    if (major) *major = s_major;
    if (minor) *minor = s_minor;
    if (release) *release = s_release;
    if (revision) *revision = s_revision;
    if (beta) *beta = s_beta;
    if (conf) *conf = s_confidential;
    if (le) *le = s_le;
    
    if (lang) {
        // Copy the language string
        int i = 0;
        while (s_language[i] != 0) {
            lang[i] = s_language[i];
            i++;
        }
        lang[i] = 0;
    }
}

// Get the PixInsight codename
char16_type* API_Global_GetPixInsightCodename(void* moduleHandle) {
    LogDbg("GetPixInsightCodename called with module: " + std::to_string((uintptr_t)moduleHandle));
    
    // Static codename to return
    static const char16_type codename[] = { 'C', 'l', 'o', 'u', 'd', ' ', 'N', 'i', 'n', 'e', 0 };
    
    // Calculate required size including null terminator
    size_t len = 0;
    while (codename[len] != 0)
        len++;
    len++; // Include the null terminator
    
    // Allocate memory using our Allocate function
    char16_type* result = (char16_type*)malloc(len * sizeof(char16_type));
    
    if (result) {
        // Copy the string
        for (size_t i = 0; i < len; i++)
            result[i] = codename[i];
    }
    
    return result;
}  

// Mock for GetPixelTraitsLUT
void* GetPixelTraitsLUT(int format) {
    LogDbg("GetPixelTraitsLUT called with format: " + std::to_string(format));
    
    if (format >= 0 && format < 16) {
        return &g_pixel_luts[format];
    }
    return &g_pixel_luts[0]; // Default to format 0
}

// Mock for GetConsole
void* GetConsole() {
    LogDbg("GetConsole called");
    return g_console_handle;
}

// Mock for LastError
uint32 API_Global_LastError() {
    LogDbg("LastError called, returning: " + std::to_string(g_last_error));
    return g_last_error;
}

// Mock for setting error
void SetLastError(int error_code) {
    g_last_error = error_code;
    LogDbg("SetLastError called with: " + std::to_string(error_code));
}

// Mock for ClearError
void ClearError() {
    g_last_error = 0;
    LogDbg("ClearError called");
}

// Mock for ProcessEvents
void API_Global_ProcessEvents(api_bool excludeUserInputEvents) {
    LogDbg("ProcessEvents called");
}

// Mock for GetApplicationInstanceSlot
int GetApplicationInstanceSlot() {
    LogDbg("GetApplicationInstanceSlot called");
    return 0; // Root slot
}

// Mock for GetProcessStatus
uint32_t API_Global_GetProcessStatus() {
    LogDbg("GetProcessStatus called");
    // Return a status that indicates not aborted (bit 31 clear)
    // PCL checks if bit 31 (0x80000000) is set to determine if process should abort
    return 0x00000000; // Normal status, not aborted
}

  api_bool WriteConsole(console_handle handle, const char16_type *text16, api_bool appendNewline )
  {
    if (!text16) {
      if (appendNewline) {
        putchar('\n');
	return api_true;
      }
      return api_false;
    }
    
    // Just output to stdout for now
    while (*text16)
      {
	uint16_t ch;
	ch = *text16++;
	putchar(ch);
      }
    if (appendNewline) {
        putchar('\n');
    }
    
    return api_true; // success
}

// Mock for EnableAbort
int EnableAbort() {
  LogDbg("EnableAbort called");
  return 1; // api_true - success
}

// Mock for GetGlobalFlag
api_bool API_Global_GetGlobalFlag(const char* flag_name, api_bool* value) {
    LogDbg("GetGlobalFlag called with: " + std::string(flag_name ? flag_name : "(null)"));
    
    if (!value) {
        return 0; // api_false
    }
    
    // Return default values for common flags
    if (flag_name) {
        std::string name = flag_name;
        if (name.find("Abort") != std::string::npos) {
            *value = 0; // Not aborted
        } else if (name.find("Debug") != std::string::npos) {
            *value = 0; // Debug off
        } else {
            *value = 0; // Default to false/off
        }
    } else {
        *value = 0;
    }
    
    return 1; // api_true - success
}

// Mock for GetGlobalInteger
int API_Global_GetGlobalInteger(const char* int_name, void* value, api_bool isSigned) {
    std::string skey(int_name ? int_name : "(null)");
    LogDbg("GetGlobalInteger called with: " + skey);
    preload_default_global_settings();
    
    if (!value) {
        return 0; // api_false
    }

    auto it = g_settings_global.find(skey);
    if (it == g_settings_global.end())
	  {
	    LogDbg("API_Global_GetGlobalInteger missing: " + skey);
            return api_false;
	  }
    *(int *)value = it->second;
    return api_true;
}

// Mock for GetUIObjectRefCount
size_type API_UI_GetUIObjectRefCount(const_api_handle ui_object) {
    LogDbg("GetUIObjectRefCount called with object: " + std::to_string((uintptr_t)ui_object));
    
    if (!ui_object) {
        return 0; // No references for null object
    }
    
    // Return 1 to indicate the object exists and has at least one reference
    return 1;
}

// Mock for DetachFromUIObject
api_bool API_UI_DetachFromUIObject(api_handle module_handle, api_handle ui_object) {
    LogDbg("DetachFromUIObject called with module: " + std::to_string((uintptr_t)module_handle) + 
             ", object: " + std::to_string((uintptr_t)ui_object));
    
    if (!ui_object) {
        return 0; // api_false - can't detach from null object
    }
    
    // Always succeed in detaching (in a real implementation, this would decrease reference count)
    return 1; // api_true - success
}

// Memory allocation function
void* API_Global_Allocate(size_type size) {
    LogDbg("Allocate called for size: " + std::to_string(size));
    
    if (size == 0) {
        return nullptr;
    }
    
    void* ptr = malloc(size);
    
    if (!ptr)
        LogDbg("Allocate failed: out of memory");
    
    return ptr;
}

// Memory deallocation function
api_bool API_Global_Deallocate(void* ptr) {
    if (!ptr) {
        LogDbg("Deallocate called with nullptr");
        return api_false;
    }
    
    free(ptr);
    return api_true;
}
  
// Structure for file format capabilities
struct MockFormatCapabilities {
    std::string name;          // ADD THIS - format name like "XISF", "FITS"
    std::string extension;     // ADD THIS - extension like ".xisf", ".fits"
    bool canRead;
    bool canWrite;
    bool canReadMultipleImages;
    bool canWriteMultipleImages;
    bool canStore8Bit;
    bool canStore16Bit;
    bool canStore32Bit;
    bool canStore64Bit;
    bool canStoreFloat;
    bool canStoreDouble;
    bool canStoreRGBColor;
};

// Map of supported formats - UPDATE WITH NAME AND EXTENSION
static std::map<std::string, MockFormatCapabilities> g_format_capabilities = {
    {".fits", {"FITS", ".fits", true, true, true, true, true, true, true, true, true, true, true}},
    {".fit",  {"FITS", ".fit",  true, true, true, true, true, true, true, true, true, true, true}},
    {".fts",  {"FITS", ".fts",  true, true, true, true, true, true, true, true, true, true, true}},
    {".xisf", {"XISF", ".xisf", true, true, true, true, true, true, true, true, true, true, true}},
};  

meta_format_handle API_FileFormat_GetFileFormatByFileExtension(api_handle handle, const char16_type* filename, api_bool toRead, api_bool toWrite) {
    if (!filename) {
        return nullptr;
    }
    
    std::string filenameUtf8 = Utf16ToUtf8(filename);
    LogDbg("GetFileFormatByFileExtension called with filename: " + filenameUtf8);
    
    std::string extension = GetFileExtension(filenameUtf8);
    if (extension.empty()) {
        return nullptr;
    }
    
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    auto it = g_format_capabilities.find(extension);
    if (it != g_format_capabilities.end()) {
        if ((toRead && !it->second.canRead) || (toWrite && !it->second.canWrite)) {
            return nullptr;
        }
        
        // FIXED: Return pointer to the struct, not a strdup'd string
        return reinterpret_cast<meta_format_handle>(
            const_cast<MockFormatCapabilities*>(&(it->second))
        );
    }
    
    return nullptr;
}

api_bool API_FileFormat_GetFileFormatCapabilities(meta_format_handle handle, api_format_capabilities* capabilities) {
    if (!handle || !capabilities) {
        return api_false;
    }
    
    // FIXED: Cast to MockFormatCapabilities, not string
    const MockFormatCapabilities* caps = static_cast<const MockFormatCapabilities*>(handle);
    
    LogDbg("GetFileFormatCapabilities called for: " + caps->extension);
    
    capabilities->canRead = caps->canRead ? api_true : api_false;
    capabilities->canWrite = caps->canWrite ? api_true : api_false;
    capabilities->canStore8bit = caps->canStore8Bit ? api_true : api_false;
    capabilities->canStore16bit = caps->canStore16Bit ? api_true : api_false;
    capabilities->canStore32bit = caps->canStore32Bit ? api_true : api_false;
    capabilities->canStore64bit = caps->canStore64Bit ? api_true : api_false;
    capabilities->canStoreFloat = caps->canStoreFloat ? api_true : api_false;
    capabilities->canStoreDouble = caps->canStoreDouble ? api_true : api_false;
    capabilities->canStoreRGBColor = caps->canStoreRGBColor ? api_true : api_false;
    capabilities->supportsMultipleImages = caps->canReadMultipleImages ? api_true : api_false;
    
    return api_true;
}  

// Update API_FileFormat_OpenImageFileEx to handle XISF files
api_bool API_FileFormat_OpenImageFileEx(file_format_handle handle, const char16_type *filePath, 
                                        const char *hints, uint32 flags) {
    if (!handle || !filePath) {
        return api_false;
    }
    
    std::string path = Utf16ToUtf8(filePath);
    LogDbg("OpenImageFileEx called for: " + path);
    
    std::lock_guard<std::mutex> lock(g_file_instances_mutex);
    auto it = g_file_instances.find(handle);
    if (it == g_file_instances.end()) {
        return api_false;
    }
    
    // Store file path and clear any existing images
    it->second->path = path;
    
    // Clean up any existing images
    for (MockImage* img : it->second->images) {
        if (img->pixelData) {
            for (uint32_t i = 0; i < img->channels; i++) {
                if (img->pixelData[i]) {
                    free(img->pixelData[i]);
                }
            }
            delete[] img->pixelData;
        }
        if (img->stats) {
            delete[] img->stats;
        }
        delete img;
    }
    it->second->images.clear();
    it->second->selectedImage = 0;
    
    // Determine file type
    std::string extension = GetFileExtension(path);
    bool isXISF = (extension == ".xisf");
    bool isFITS = (extension == ".fits" || extension == ".fit" || extension == ".fts");
    
    // Handle XISF files
    if (isXISF) {
        LogDbg("OpenImageFileEx: Opening XISF file");
        
        try {
            pcl::XISFReader xisfReader;
            
            // Set up options
            pcl::XISFOptions xisfOptions;
            xisfOptions.verbosity = 1;
            if (hints) {
                // Parse hints if needed
                LogDbg("OpenImageFileEx: Hints provided: " + std::string(hints));
            }
            xisfReader.SetOptions(xisfOptions);
            
            // Open the file
            xisfReader.Open(pcl::String(path.c_str()));
            
            // Get number of images
            int numImages = xisfReader.NumberOfImages();
            LogDbg("OpenImageFileEx: Found " + std::to_string(numImages) + " images in XISF file");
            
            // Create mock images for each image in the file
            for (int i = 0; i < numImages; i++) {
                xisfReader.SelectImage(i);
                
                pcl::ImageInfo imgInfo = xisfReader.ImageInfo();
                pcl::ImageOptions imgOptions = xisfReader.ImageOptions();
                
                MockImage* img = new MockImage();
                img->width = imgInfo.width;
                img->height = imgInfo.height;
                img->channels = imgInfo.numberOfChannels;
                img->bitsPerSample = imgOptions.bitsPerSample;
                img->isFloat = imgOptions.ieeefpSampleFormat;
                img->colorSpace = imgInfo.colorSpace;
                
                // Don't load pixel data yet - that happens in ReadImage
                img->pixelData = nullptr;
                
                // Create stats array
                img->stats = new double[img->channels * 2];
                for (uint32_t c = 0; c < img->channels; c++) {
                    img->stats[c*2] = 0.0;      // min
                    img->stats[c*2+1] = 1.0;    // max (default for float)
                }
                
                it->second->images.push_back(img);
                
                LogDbg("OpenImageFileEx: Image " + std::to_string(i) + " - " +
                         std::to_string(img->width) + "x" + std::to_string(img->height) + 
                         ", " + std::to_string(img->channels) + " channels, " +
                         std::to_string(img->bitsPerSample) + " bits, " +
                         (img->isFloat ? "float" : "integer"));
            }
            
            xisfReader.Close();
            
            if (it->second->images.empty()) {
                LogDbg("OpenImageFileEx: No images found in XISF file");
                return api_false;
            }
            
            return api_true;
            
        } catch (const std::exception& e) {
            LogDbg("OpenImageFileEx: Exception opening XISF file: " + std::string(e.what()));
            return api_false;
        } catch (...) {
            LogDbg("OpenImageFileEx: Unknown exception opening XISF file");
            return api_false;
        }
    }
    
    // Handle FITS files (existing CFITSIO code)
    if (isFITS) {
        LogDbg("OpenImageFileEx: Opening FITS file");
        
        try {
            fitsfile *fptr;
            int status = 0;
            
            CFITSIO_LOCK
            
            // Open the FITS file
            if (fits_open_file(&fptr, path.c_str(), READONLY, &status)) {
                LogDbg("OpenImageFileEx: Failed to open FITS file, status = " + std::to_string(status));
                return api_false;
            }
            
            // Get number of HDUs (Header Data Units)
            int numHDUs = 0;
            fits_get_num_hdus(fptr, &numHDUs, &status);
            
            LogDbg("OpenImageFileEx: Found " + std::to_string(numHDUs) + " HDUs in FITS file");
            
            // Read each image HDU
            for (int hdu = 1; hdu <= numHDUs; hdu++) {
                fits_movabs_hdu(fptr, hdu, NULL, &status);
                
                int hduType;
                fits_get_hdu_type(fptr, &hduType, &status);
                
                // Skip non-image HDUs
                if (hduType != IMAGE_HDU) {
                    continue;
                }
                
                // Get image dimensions
                int naxis;
                long naxes[3] = {0, 0, 0};
                fits_get_img_dim(fptr, &naxis, &status);
                fits_get_img_size(fptr, 3, naxes, &status);
                
                if (naxis < 2 || naxes[0] == 0 || naxes[1] == 0) {
                    continue; // Skip empty or invalid images
                }
                
                // Get bit depth
                int bitpix;
                fits_get_img_type(fptr, &bitpix, &status);
                
                MockImage* img = new MockImage();
                img->width = naxes[0];
                img->height = naxes[1];
                img->channels = (naxis >= 3 && naxes[2] > 0) ? naxes[2] : 1;
                img->colorSpace = (img->channels > 1) ? 0 : 1; // 0=RGB, 1=Grayscale
                
                // Determine sample format from bitpix
                switch (bitpix) {
                    case BYTE_IMG:
                        img->bitsPerSample = 8;
                        img->isFloat = false;
                        break;
                    case SHORT_IMG:
                        img->bitsPerSample = 16;
                        img->isFloat = false;
                        break;
                    case LONG_IMG:
                        img->bitsPerSample = 32;
                        img->isFloat = false;
                        break;
                    case FLOAT_IMG:
                        img->bitsPerSample = 32;
                        img->isFloat = true;
                        break;
                    case DOUBLE_IMG:
                        img->bitsPerSample = 64;
                        img->isFloat = true;
                        break;
                    default:
                        img->bitsPerSample = 32;
                        img->isFloat = true;
                }
                
                // Don't load pixel data yet - that happens in ReadImage
                img->pixelData = nullptr;
                
                // Create stats array
                img->stats = new double[img->channels * 2];
                for (uint32_t c = 0; c < img->channels; c++) {
                    img->stats[c*2] = 0.0;      // min
                    img->stats[c*2+1] = 1.0;    // max
                }
                
                it->second->images.push_back(img);
                
                LogDbg("OpenImageFileEx: HDU " + std::to_string(hdu) + " - " +
                         std::to_string(img->width) + "x" + std::to_string(img->height) + 
                         ", " + std::to_string(img->channels) + " channels, " +
                         std::to_string(img->bitsPerSample) + " bits, " +
                         (img->isFloat ? "float" : "integer"));
            }
            
            fits_close_file(fptr, &status);
            
            if (it->second->images.empty()) {
                LogDbg("OpenImageFileEx: No image HDUs found in FITS file");
                return api_false;
            }
            
            return api_true;
            
        } catch (const std::exception& e) {
            LogDbg("OpenImageFileEx: Exception opening FITS file: " + std::string(e.what()));
            return api_false;
        } catch (...) {
            LogDbg("OpenImageFileEx: Unknown exception opening FITS file");
            return api_false;
        }
    }
    
    // For other formats or testing, create a single mock image
    MockImage* img = new MockImage();
    img->width = 1024;
    img->height = 1024;
    img->channels = 3;
    img->bitsPerSample = 32;
    img->isFloat = true;
    img->colorSpace = 0; // RGB
    
    // Don't allocate pixel data yet
    img->pixelData = nullptr;
    
    // Create stats
    img->stats = new double[img->channels * 2];
    for (uint32_t i = 0; i < img->channels; i++) {
        img->stats[i*2] = 0.0;
        img->stats[i*2+1] = 1.0;
    }
    
    it->second->images.push_back(img);
    
    return api_true;
}

api_bool API_FileFormat_ReadImage(file_format_handle handle, image_handle image) {
    if (!handle || !image) {
        LogDbg("ReadImage: Invalid handle or image");
        return api_false;
    }
    
    LogDbg("ReadImage: Starting image read operation");
    
    std::lock_guard<std::mutex> lock(g_file_instances_mutex);
    auto it = g_file_instances.find(handle);
    if (it == g_file_instances.end() || 
        it->second->selectedImage >= it->second->images.size()) {
        LogDbg("ReadImage: File instance not found or invalid image selection");
        return api_false;
    }
    
    MockFileInstance* instance = it->second;
    bool isXISF = (instance->extension == "XISF");
    
    // Handle XISF files
    if (isXISF && !instance->path.empty()) {
        LogDbg("ReadImage: Using XISF reader for: " + instance->path);
        
        try {
            pcl::XISFReader xisfReader;
            pcl::XISFOptions xisfOptions;
            xisfOptions.verbosity = 2;
            xisfReader.SetOptions(xisfOptions);
            
            // Keep the path string alive
            std::string filePath = instance->path;
            xisfReader.Open(pcl::String(filePath.c_str()));
            
            if (instance->selectedImage < xisfReader.NumberOfImages()) {
                xisfReader.SelectImage(instance->selectedImage);
            } else {
                LogDbg("ReadImage: Invalid image index");
                xisfReader.Close();
                return api_false;
            }
            
            pcl::ImageInfo imgInfo = xisfReader.ImageInfo();
            pcl::ImageOptions imgOptions = xisfReader.ImageOptions();
            
            LogDbg("ReadImage: XISF image dimensions: " + 
                     std::to_string(imgInfo.width) + "x" + 
                     std::to_string(imgInfo.height) + ", " +
                     std::to_string(imgInfo.numberOfChannels) + " channels");
            
            // Get the MockImage
            std::lock_guard<std::mutex> img_lock(g_image_map_mutex);
            auto img_it = g_image_map.find(image);
            if (img_it == g_image_map.end()) {
                xisfReader.Close();
                return api_false;
            }
            
            MockImage* mockImg = img_it->second;

	    // Check if we need to reallocate pixelData array for different channel count
	    if (mockImg->channels != imgInfo.numberOfChannels) {
		LogDbg("ReadImage: Reallocating pixelData array from " + 
			 std::to_string(mockImg->channels) + " to " + 
			 std::to_string(imgInfo.numberOfChannels) + " channels");

		if (mockImg->pixelData) {
		    delete[] mockImg->pixelData;
		}
		mockImg->pixelData = new void*[imgInfo.numberOfChannels];
		for (uint32_t c = 0; c < imgInfo.numberOfChannels; c++) {
		    mockImg->pixelData[c] = nullptr;
		}
	    }
	    
            // Update metadata
            mockImg->width = imgInfo.width;
            mockImg->height = imgInfo.height;
            mockImg->channels = imgInfo.numberOfChannels;
            mockImg->colorSpace = imgInfo.colorSpace;
            mockImg->bitsPerSample = imgOptions.bitsPerSample;
            mockImg->isFloat = imgOptions.ieeefpSampleFormat;
            
            // Allocate pixelData array
            if (!mockImg->pixelData) {
                mockImg->pixelData = new void*[mockImg->channels];
                for (uint32_t c = 0; c < mockImg->channels; c++) {
                    mockImg->pixelData[c] = nullptr;
                }
            }
            
            // Read into a REAL PCL image based on format
            if (imgOptions.ieeefpSampleFormat) {
                if (imgOptions.bitsPerSample == 64) {
                    pcl::DImage tempImage;
                    xisfReader.ReadImage(tempImage);
                    
                    // Copy data to MockImage
                    for (uint32_t c = 0; c < mockImg->channels; c++) {
                        size_t size = mockImg->width * mockImg->height * sizeof(double);
                        mockImg->pixelData[c] = malloc(size);
                        memcpy(mockImg->pixelData[c], tempImage.PixelData(c), size);
                    }
                } else {
                    pcl::FImage tempImage;
                    xisfReader.ReadImage(tempImage);
                    
                    // Copy data to MockImage
                    for (uint32_t c = 0; c < mockImg->channels; c++) {
                        size_t size = mockImg->width * mockImg->height * sizeof(float);
                        mockImg->pixelData[c] = malloc(size);
                        memcpy(mockImg->pixelData[c], tempImage.PixelData(c), size);
                    }
                }
            } else {
                if (imgOptions.bitsPerSample <= 8) {
                    pcl::UInt8Image tempImage;
                    xisfReader.ReadImage(tempImage);
                    
                    for (uint32_t c = 0; c < mockImg->channels; c++) {
                        size_t size = mockImg->width * mockImg->height * sizeof(uint8_t);
                        mockImg->pixelData[c] = malloc(size);
                        memcpy(mockImg->pixelData[c], tempImage.PixelData(c), size);
                    }
                } else if (imgOptions.bitsPerSample <= 16) {
                    pcl::UInt16Image tempImage;
                    xisfReader.ReadImage(tempImage);
                    
                    for (uint32_t c = 0; c < mockImg->channels; c++) {
                        size_t size = mockImg->width * mockImg->height * sizeof(uint16_t);
                        mockImg->pixelData[c] = malloc(size);
                        memcpy(mockImg->pixelData[c], tempImage.PixelData(c), size);
                    }
                } else {
                    pcl::UInt32Image tempImage;
                    xisfReader.ReadImage(tempImage);
                    
                    for (uint32_t c = 0; c < mockImg->channels; c++) {
                        size_t size = mockImg->width * mockImg->height * sizeof(uint32_t);
                        mockImg->pixelData[c] = malloc(size);
                        memcpy(mockImg->pixelData[c], tempImage.PixelData(c), size);
                    }
                }
            }
            
            xisfReader.Close();
            LogDbg("ReadImage: Successfully read XISF file");
            
            return api_true;
            
        } catch (const std::exception& e) {
            LogDbg("ReadImage: Exception reading XISF file: " + std::string(e.what()));
            return api_false;
        }
    }
    
    return api_false;
}

// Create a file format instance
file_format_handle CreateFileFormatInstance(api_handle handle, meta_format_handle meta_handle) {
    if (!meta_handle) {
        return nullptr;
    }
    
    std::string extension = (const char*)meta_handle;
    LogDbg("CreateFileFormatInstance called for: " + extension);
    
    // Create a new instance
    MockFileInstance* instance = new MockFileInstance();
    instance->extension = extension;
    
    file_format_handle file_handle = (file_format_handle)instance;
    
    // Store in our map
    std::lock_guard<std::mutex> lock(g_file_instances_mutex);
    g_file_instances[file_handle] = instance;
    
    return file_handle;
}

// Get number of images in the file
uint32 GetImageCount(const_file_format_handle handle) {
    if (!handle) {
        LogDbg("GetImageCount: Invalid handle");
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(g_file_instances_mutex);
    auto it = g_file_instances.find((file_format_handle)handle);
    if (it == g_file_instances.end()) {
        LogDbg("GetImageCount: File instance not found");
        return 0;
    }
    
    uint32 count = static_cast<uint32>(it->second->images.size());
    LogDbg("GetImageCount: Found " + std::to_string(count) + " images");
    
    return count;
}
  
// Get image ID
api_bool GetImageId(const_file_format_handle handle, char* id, size_type* len, uint32 index) {
    if (!handle || !len) {
        return api_false;
    }
    
    std::lock_guard<std::mutex> lock(g_file_instances_mutex);
    auto it = g_file_instances.find((file_format_handle)handle);
    if (it == g_file_instances.end() || index >= it->second->images.size()) {
        if (id && *len > 0) {
            id[0] = '\0';
        }
        *len = 0;
        return api_false;
    }
    
    // Generate a dummy ID
    std::string imageId = "Image" + std::to_string(index);
    
    if (id) {
        size_type copyLen = std::min(*len - 1, (size_type)imageId.length());
        if (copyLen > 0) {
            memcpy(id, imageId.c_str(), copyLen);
            id[copyLen] = '\0';
        }
    }
    
    *len = imageId.length();
    return api_true;
}

// Get image description
api_bool GetImageDescription(const_file_format_handle handle, api_image_info* info, 
                             api_image_options* options, uint32 index) {
    if (!handle || !info || !options) {
        return api_false;
    }
    
    std::lock_guard<std::mutex> lock(g_file_instances_mutex);
    auto it = g_file_instances.find((file_format_handle)handle);
    if (it == g_file_instances.end() || index >= it->second->images.size()) {
        return api_false;
    }
    
    MockImage* img = it->second->images[index];
    
    // Fill in image info
    info->width = img->width;
    info->height = img->height;
    info->numberOfChannels = img->channels;
    info->colorSpace = img->colorSpace;
    info->supported = api_true;
    
    // Fill in options
    options->bitsPerSample = img->bitsPerSample;
    options->ieeefpSampleFormat = img->isFloat ? api_true : api_false;
    
    return api_true;
}

// Select an image in the file
api_bool API_FileFormat_SelectImage(file_format_handle handle, uint32 index) {
    if (!handle) {
        LogDbg("SelectImage: Invalid handle");
        return api_false;
    }
    
    std::lock_guard<std::mutex> lock(g_file_instances_mutex);
    auto it = g_file_instances.find(handle);
    if (it == g_file_instances.end()) {
        LogDbg("SelectImage: File instance not found");
        return api_false;
    }
    
    MockFileInstance* instance = it->second;
    
    // Check if the index is valid
    if (index >= instance->images.size()) {
        LogDbg("SelectImage: Invalid image index: " + std::to_string(index) + 
                 " (max: " + std::to_string(instance->images.size() - 1) + ")");
        return api_false;
    }
    
    LogDbg("SelectImage: Selecting image index " + std::to_string(index));
    
    // Update the selected image index
    instance->selectedImage = index;
    
    return api_true;
}
  
// Get selected image index
uint32 GetSelectedImageIndex(const_file_format_handle handle) {
    if (!handle) {
        LogDbg("GetSelectedImageIndex: Invalid handle");
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(g_file_instances_mutex);
    auto it = g_file_instances.find((file_format_handle)handle);
    if (it == g_file_instances.end()) {
        LogDbg("GetSelectedImageIndex: File instance not found");
        return 0;
    }
    
    LogDbg("GetSelectedImageIndex: Current index is " + std::to_string(it->second->selectedImage));
    return it->second->selectedImage;
}
  
// Read image data
api_bool ReadImagePixelData(file_format_handle handle, image_handle image) {
    if (!handle || !image) {
        return api_false;
    }
    
    std::lock_guard<std::mutex> lock(g_file_instances_mutex);
    auto it = g_file_instances.find(handle);
    if (it == g_file_instances.end() || 
        it->second->selectedImage >= it->second->images.size()) {
        return api_false;
    }
    
    // Get the selected image
    MockImage* srcImg = it->second->images[it->second->selectedImage];
    
    // Get the target image
    std::lock_guard<std::mutex> img_lock(g_image_map_mutex);
    auto img_it = g_image_map.find(image);
    if (img_it == g_image_map.end()) {
        return api_false;
    }
    
    MockImage* dstImg = img_it->second;
    
    // Check compatibility
    if (dstImg->width != srcImg->width || 
        dstImg->height != srcImg->height || 
        dstImg->channels != srcImg->channels) {
        return api_false;
    }
    
    // Copy pixel data
    for (uint32_t i = 0; i < dstImg->channels; i++) {
        size_t pixelCount = dstImg->width * dstImg->height;
        size_t bytesPerSample = (dstImg->bitsPerSample <= 8) ? 1 : 
                                ((dstImg->bitsPerSample <= 16) ? 2 : 
                                ((dstImg->bitsPerSample <= 32) ? 4 : 8));
        
        memcpy(dstImg->pixelData[i], srcImg->pixelData[i], pixelCount * bytesPerSample);
    }
    
    // Copy stats
    for (uint32_t i = 0; i < dstImg->channels; i++) {
        dstImg->stats[i*2] = srcImg->stats[i*2];        // min
        dstImg->stats[i*2+1] = srcImg->stats[i*2+1];    // max
    }
    
    return api_true;
}

// Close file
api_bool API_FileFormat_CloseImageFile(file_format_handle handle) {
    if (!handle) {
        return api_false;
    }
    
    std::lock_guard<std::mutex> lock(g_file_instances_mutex);
    auto it = g_file_instances.find(handle);
    if (it == g_file_instances.end()) {
        return api_false;
    }
    
    // Clean up images
    for (MockImage* img : it->second->images) {
        // Free pixel data for each channel
        for (uint32_t i = 0; i < img->channels; i++) {
	  //            free(img->pixelData[i]);
        }
        
        delete[] img->pixelData;
        delete[] img->stats;
        delete img;
    }
    
    delete it->second;
    g_file_instances.erase(it);
    
    return api_true;
}

// Write image data to file
api_bool API_FileFormat_WriteImageFile(file_format_handle handle, const char* hints) {
    if (!handle) {
        return api_false;
    }
    
    std::lock_guard<std::mutex> lock(g_file_instances_mutex);
    auto it = g_file_instances.find(handle);
    if (it == g_file_instances.end()) {
        return api_false;
    }
    
    // In a real implementation, we would write to the file
    // For testing, just log that we're writing
    LogDbg("WriteImageFile called for: " + it->second->path);
    
    return api_true;
}

// Create a new file
api_bool API_FileFormat_CreateImageFile(file_format_handle handle, const char16_type* filePath, 
                        const char* hints) {
    if (!handle || !filePath) {
        return api_false;
    }
    
    std::string path = Utf16ToUtf8(filePath);
    LogDbg("CreateImageFile called for: " + path);
    
    std::lock_guard<std::mutex> lock(g_file_instances_mutex);
    auto it = g_file_instances.find(handle);
    if (it == g_file_instances.end()) {
        return api_false;
    }
    
    // Store file path
    it->second->path = path;
    
    return api_true;
}

// Create a new file with extended options
api_bool API_FileFormat_CreateImageFileEx(file_format_handle handle, const char16_type* filePath, 
                          uint32 count, const char* hints, uint32 flags) {
    if (!handle || !filePath) {
        return api_false;
    }
    
    std::string path = Utf16ToUtf8(filePath);
    LogDbg("CreateImageFileEx called for: " + path + " with count: " + std::to_string(count) +
             " and flags: " + std::to_string(flags));
    
    std::lock_guard<std::mutex> lock(g_file_instances_mutex);
    auto it = g_file_instances.find(handle);
    if (it == g_file_instances.end()) {
        return api_false;
    }
    
    // Store file path
    it->second->path = path;
    
    // Clear any existing images
    for (MockImage* img : it->second->images) {
        // Free pixel data for each channel
        for (uint32_t i = 0; i < img->channels; i++) {
            free(img->pixelData[i]);
        }
        
        delete[] img->pixelData;
        delete[] img->stats;
        delete img;
    }
    
    it->second->images.clear();
    it->second->selectedImage = 0;
    
    // Pre-allocate space for the requested number of images
    // (we won't create them yet - they'll be created when FileCreateImage is called)
    LogDbg("CreateImageFileEx: Prepared for " + std::to_string(count) + " images");
    
    return api_true;
}
  
// Create an image
api_bool API_FileFormat_CreateImage(file_format_handle handle, const api_image_info* info) {
    if (!handle || !info) {
        return api_false;
    }
    
    std::lock_guard<std::mutex> lock(g_file_instances_mutex);
    auto it = g_file_instances.find(handle);
    if (it == g_file_instances.end()) {
        return api_false;
    }
    
    // Create a new image
    MockImage* img = new MockImage();
    img->width = info->width;
    img->height = info->height;
    img->channels = info->numberOfChannels;
    img->bitsPerSample = 32; // Default to 32-bit float
    img->isFloat = true;
    img->colorSpace = info->colorSpace;
    
    // Allocate pixel data
    img->pixelData = new void*[img->channels];
    for (uint32_t i = 0; i < img->channels; i++) {
        img->pixelData[i] = calloc(img->width * img->height, sizeof(float));
    }
    
    // Create stats
    img->stats = new double[img->channels * 2];
    for (uint32_t i = 0; i < img->channels; i++) {
        img->stats[i*2] = 0.0;     // min
        img->stats[i*2+1] = 1.0;   // max
    }
    
    // Add to the file's image list
    it->second->images.push_back(img);
    it->second->selectedImage = it->second->images.size() - 1;
    
    return api_true;
}

// Write image data to the file
api_bool WriteImagePixelData(file_format_handle handle, const_image_handle image) {
    if (!handle || !image) {
        return api_false;
    }
    
    std::lock_guard<std::mutex> lock(g_file_instances_mutex);
    auto it = g_file_instances.find(handle);
    if (it == g_file_instances.end() || 
        it->second->selectedImage >= it->second->images.size()) {
        return api_false;
    }
    
    // Get the target image in the file
    MockImage* dstImg = it->second->images[it->second->selectedImage];
    
    // Get the source image
    std::lock_guard<std::mutex> img_lock(g_image_map_mutex);
    auto img_it = g_image_map.find((image_handle)image);
    if (img_it == g_image_map.end()) {
        return api_false;
    }
    
    MockImage* srcImg = img_it->second;
    
    // Check compatibility
    if (dstImg->width != srcImg->width || 
        dstImg->height != srcImg->height || 
        dstImg->channels != srcImg->channels) {
        return api_false;
    }
    
    // Copy pixel data
    for (uint32_t i = 0; i < srcImg->channels; i++) {
        size_t pixelCount = srcImg->width * srcImg->height;
        size_t bytesPerSample = (srcImg->bitsPerSample <= 8) ? 1 : 
                               ((srcImg->bitsPerSample <= 16) ? 2 : 
                               ((srcImg->bitsPerSample <= 32) ? 4 : 8));
        
        memcpy(dstImg->pixelData[i], srcImg->pixelData[i], pixelCount * bytesPerSample);
    }
    
    // Copy stats
    for (uint32_t i = 0; i < srcImg->channels; i++) {
        dstImg->stats[i*2] = srcImg->stats[i*2];        // min
        dstImg->stats[i*2+1] = srcImg->stats[i*2+1];    // max
    }
    
    return api_true;
}

api_bool API_FileFormat_WriteImage(file_format_handle handle, const_image_handle image) {
    if (!handle || !image) {
        return api_false;
    }
    
    std::lock_guard<std::mutex> file_lock(g_file_instances_mutex);
    auto it = g_file_instances.find(handle);
    if (it == g_file_instances.end()) {
        return api_false;
    }
    
    MockFileInstance* instance = it->second;
    
    std::string extension = GetFileExtension(instance->path);
    bool isXISF = (extension == ".xisf");
    
    if (isXISF) {
	LogDbg("WriteImage: Using XISF writer for: " + instance->path);

	// Get the MockImage
	std::lock_guard<std::mutex> img_lock(g_image_map_mutex);
	auto img_it = g_image_map.find((image_handle)image);
	if (img_it == g_image_map.end()) {
	    LogDbg("WriteImage: Image not found in map");
	    return api_false;
	}

	MockImage* mockImg = img_it->second;

	LogDbg("WriteImage: MockImage is " + 
		 std::to_string(mockImg->width) + "x" + 
		 std::to_string(mockImg->height) + ", " +
		 std::to_string(mockImg->channels) + " channels");

	if (!mockImg->pixelData) {
	    LogDbg("WriteImage: pixelData is NULL!");
	    return api_false;
	}

	try {
	    // Determine color space
	    pcl::ColorSpace::value_type colorSpace;
	    if (mockImg->channels == 1) {
		colorSpace = pcl::ColorSpace::Gray;
	    } else if (mockImg->channels == 3) {
		colorSpace = pcl::ColorSpace::RGB;
	    } else {
		colorSpace = pcl::ColorSpace::RGB; // Default for other cases
	    }

	    // Create a PCL image with correct constructor
	    pcl::FImage outputImage(mockImg->width, mockImg->height, colorSpace);

	    // If we need more channels than the color space provides, allocate them
	    if (mockImg->channels > outputImage.NumberOfChannels()) {
		outputImage.AllocateData(mockImg->width, mockImg->height, mockImg->channels, colorSpace);
	    }

	    for (uint32_t c = 0; c < mockImg->channels; c++) {
		if (!mockImg->pixelData[c]) {
		    LogDbg("WriteImage: Channel " + std::to_string(c) + " is NULL!");
		    return api_false;
		}

		memcpy(outputImage.PixelData(c), 
		       mockImg->pixelData[c], 
		       mockImg->width * mockImg->height * sizeof(float));
	    }

	    pcl::XISFWriter xisfWriter;
	    pcl::XISFOptions xisfOptions;
	    xisfOptions.verbosity = 2;
	    xisfWriter.SetOptions(xisfOptions);

	    xisfWriter.Create(pcl::String(instance->path.c_str()), 1);

	    pcl::ImageOptions imgOptions;
	    imgOptions.bitsPerSample = 32;
	    imgOptions.ieeefpSampleFormat = true;
	    xisfWriter.SetImageOptions(imgOptions);

	    xisfWriter.WriteImage(outputImage);

	    xisfWriter.Close();
	    LogDbg("WriteImage: Successfully wrote XISF file");
	    return api_true;

	} catch (const std::exception& e) {
	    LogDbg("WriteImage: Exception: " + std::string(e.what()));
	    return api_false;
	}
    }    
    return api_false;
}  
  
// Set the RGB working space for an image in a file
api_bool SetImageRGBWS(file_format_handle handle, const api_RGBWS* rgbws) {
    if (!handle || !rgbws) {
        LogDbg("SetImageRGBWS: Invalid handle or RGBWS data");
        return api_false;
    }
    
    LogDbg("SetImageRGBWS: Setting RGB working space");
    
    // In a real implementation, you would store the RGBWS data
    // For the mock, we'll just return success
    return api_true;
}

// Set metadata for an image in a file
api_bool SetImageId(file_format_handle handle, const char* id) {
    if (!handle || !id) {
        LogDbg("SetImageId: Invalid handle or id");
        return api_false;
    }
    
    LogDbg("SetImageId: Setting image id to: " + std::string(id));
    
    // In a real implementation, you would store the id
    // For the mock, we'll just return success
    return api_true;
}

// Set the sample format for an image in a file
api_bool SetImageOptions(file_format_handle handle, const api_image_options* options) {
    if (!handle || !options) {
        LogDbg("SetImageOptions: Invalid handle or options");
        return api_false;
    }
    
    LogDbg("SetImageOptions: Setting image options (bits: " + 
             std::to_string(options->bitsPerSample) + 
             ", float: " + std::to_string(options->ieeefpSampleFormat) + ")");
    
    std::lock_guard<std::mutex> lock(g_file_instances_mutex);
    auto it = g_file_instances.find(handle);
    if (it == g_file_instances.end() || 
        it->second->selectedImage >= it->second->images.size()) {
        return api_false;
    }
    
    MockImage* img = it->second->images[it->second->selectedImage];
    img->bitsPerSample = options->bitsPerSample;
    img->isFloat = (options->ieeefpSampleFormat == api_true);
    
    return api_true;
}

// Set the image description for an image in a file
api_bool SetImageDescription(file_format_handle handle, const api_image_info* info) {
    if (!handle || !info) {
        LogDbg("SetImageDescription: Invalid handle or info");
        return api_false;
    }
    
    LogDbg("SetImageDescription: Setting image description");
    
    std::lock_guard<std::mutex> lock(g_file_instances_mutex);
    auto it = g_file_instances.find(handle);
    if (it == g_file_instances.end() || 
        it->second->selectedImage >= it->second->images.size()) {
        return api_false;
    }
    
    MockImage* img = it->second->images[it->second->selectedImage];
    img->width = info->width;
    img->height = info->height;
    img->channels = info->numberOfChannels;
    img->colorSpace = info->colorSpace;
    
    return api_true;
}

image_handle API_SharedImage_CreateImage(uint32_t w, uint32_t h, uint32_t n, uint32_t nbits, api_bool flt, uint32_t cs, void* ptr) {
    LogDbg("SharedCreateImage called with dimensions: " + 
             std::to_string(w) + "x" + std::to_string(h) + 
             ", channels: " + std::to_string(n));
    
    MockImage* img = new MockImage();
    img->width = w;
    img->height = h;
    img->channels = n;
    img->bitsPerSample = nbits;
    img->isFloat = (flt == api_true);
    img->colorSpace = cs;
    
    // Allocate the ARRAY of pointers (not the actual pixels yet)
    // PCL will allocate actual pixels via API_Global_Allocate and give us pointers via SetImagePixelData
    img->pixelData = new void*[n];
    for (uint32_t i = 0; i < n; i++) {
        img->pixelData[i] = nullptr;  // Initialize to null - PCL will fill these in
    }
    
    img->stats = new double[n * 2];
    for (uint32_t i = 0; i < n; i++) {
        img->stats[i*2] = 0.0;
        img->stats[i*2+1] = (flt == api_true) ? 1.0 : 
                           ((nbits <= 8) ? 255 : 
                           ((nbits <= 16) ? 65535 : 4294967295.0));
    }
    
    image_handle handle = (image_handle)img;
    
    std::lock_guard<std::mutex> lock(g_image_map_mutex);
    g_image_map[handle] = img;
    
    LogDbg("SharedCreateImage: Created image with null pixel pointers (PCL will allocate)");
    
    return handle;
}

api_bool API_SharedImage_GetImagePixelData(image_handle handle, void*** data) {
    if (!handle || !data) {
        return api_false;
    }
    
    std::lock_guard<std::mutex> lock(g_image_map_mutex);
    auto it = g_image_map.find(handle);
    if (it == g_image_map.end()) {
        return api_false;
    }
    
    MockImage* img = it->second;
    LogDbg("GetImagePixelData: Returning pixelData for " + 
             std::to_string(img->channels) + " channels, pixelData=" + 
             (img->pixelData ? "valid" : "NULL"));
    
    *data = img->pixelData;
    return api_true;
}

// Get image dimensions and channels
api_bool API_SharedImage_GetImageGeometry(const_image_handle handle, uint32_t* w, uint32_t* h, uint32_t* n) {
    if (!handle) {
        return api_false;
    }
    
    std::lock_guard<std::mutex> lock(g_image_map_mutex);
    auto it = g_image_map.find((image_handle)handle);
    if (it == g_image_map.end()) {
        // Return some default values for testing
        if (w) *w = 1024;
        if (h) *h = 1024;
        if (n) *n = 3;
        return api_true;
    }
    
    if (w) *w = it->second->width;
    if (h) *h = it->second->height;
    if (n) *n = it->second->channels;
    
    return api_true;
}

// Get image color space
api_bool API_SharedImage_GetImageColorSpace(const_image_handle handle, uint32_t* cs) {
    if (!handle || !cs) {
        return api_false;
    }
    
    std::lock_guard<std::mutex> lock(g_image_map_mutex);
    auto it = g_image_map.find((image_handle)handle);
    if (it == g_image_map.end()) {
        // Default to RGB color space (0)
        *cs = 0;
        return api_true;
    }
    
    *cs = it->second->colorSpace;
    return api_true;
}

// Get image RGB working space
api_bool API_SharedImage_GetImageRGBWS(const_image_handle handle, api_RGBWS* rgbws) {
    if (!handle || !rgbws) {
        return api_false;
    }
    
    // Use sRGB values (these are valid and won't create a singular matrix)
    rgbws->Y[0] = 0.2126;  // Red
    rgbws->Y[1] = 0.7152;  // Green
    rgbws->Y[2] = 0.0722;  // Blue
    
    rgbws->x[0] = 0.6400;  // Red
    rgbws->x[1] = 0.3000;  // Green
    rgbws->x[2] = 0.1500;  // Blue
    
    rgbws->y[0] = 0.3300;  // Red
    rgbws->y[1] = 0.6000;  // Green
    rgbws->y[2] = 0.0600;  // Blue
    
    rgbws->gamma = 2.2;
    
    return api_true;
}

// Set image sample value range
api_bool SetImageSampleRange(image_handle handle, uint32_t channel, double min, double max) {
    if (!handle) {
        return api_false;
    }
    
    std::lock_guard<std::mutex> lock(g_image_map_mutex);
    auto it = g_image_map.find(handle);
    if (it == g_image_map.end() || channel >= it->second->channels) {
        return api_false;
    }
    
    it->second->stats[channel*2] = min;
    it->second->stats[channel*2+1] = max;
    
    return api_true;
}

// Get image sample value range
api_bool GetImageSampleRange(const_image_handle handle, uint32_t channel, double* min, double* max) {
    if (!handle) {
        return api_false;
    }
    
    std::lock_guard<std::mutex> lock(g_image_map_mutex);
    auto it = g_image_map.find((image_handle)handle);
    if (it == g_image_map.end() || channel >= it->second->channels) {
        // Return some default values
        if (min) *min = 0.0;
        if (max) *max = 1.0;
        return api_true;
    }
    
    if (min) *min = it->second->stats[channel*2];
    if (max) *max = it->second->stats[channel*2+1];
    
    return api_true;
}

// Destroy shared image
api_bool DestroyImage(image_handle handle) {
    if (!handle) {
        return api_false;
    }
    
    std::lock_guard<std::mutex> lock(g_image_map_mutex);
    auto it = g_image_map.find(handle);
    if (it == g_image_map.end()) {
        return api_false;
    }
    
    // Free pixel data for each channel
    for (uint32_t i = 0; i < it->second->channels; i++) {
        free(it->second->pixelData[i]);
    }
    
    delete[] it->second->pixelData;
    delete[] it->second->stats;
    delete it->second;
    
    g_image_map.erase(it);
    
    return api_true;
}

// Rescale image (useful for testing)
api_bool RescaleImage(image_handle handle, double min, double max) {
    if (!handle) {
        return api_false;
    }
    
    std::lock_guard<std::mutex> lock(g_image_map_mutex);
    auto it = g_image_map.find(handle);
    if (it == g_image_map.end()) {
        return api_false;
    }
    
    // Update stats for all channels
    for (uint32_t i = 0; i < it->second->channels; i++) {
        it->second->stats[i*2] = min;
        it->second->stats[i*2+1] = max;
    }
    
    return api_true;
}

api_bool AttachToImage(image_handle handle, void* owner) {
    if (!handle) {
        return api_false;
    }
    
    std::lock_guard<std::mutex> lock(g_image_map_mutex);
    auto it = g_image_map.find(handle);
    if (it == g_image_map.end()) {
        return api_false;
    }
    
    // In a real implementation, you would:
    // 1. Store the owner pointer (if you need it for tracking)
    // 2. Increment a reference count
    
    // For this mock implementation, we'll just return success
    LogDbg("AttachToImage called with handle: " + std::to_string((uintptr_t)handle) + 
             ", owner: " + std::to_string((uintptr_t)owner));
    
    return api_true;
}
  
api_bool API_SharedImage_DetachFromImage(image_handle handle, void* owner) {
    if (!handle) {
        return api_false;
    }
    
    std::lock_guard<std::mutex> lock(g_image_map_mutex);
    auto it = g_image_map.find(handle);
    if (it == g_image_map.end()) {
        // Handle is not in our map, just return success
        // This is safer than failing when the handle might have been already deleted
        return api_true;
    }
    
    // Note: In a real implementation, you'd decrement a reference count
    // For this mock implementation, we'll just return success and not actually 
    // delete anything here, to avoid potential double-free issues
    
    return api_true;
}

api_bool API_SharedImage_SetImagePixelData(image_handle handle, void** data) {
    if (!handle) {
        LogDbg("SetImagePixelData: Invalid handle");
        return api_false;
    }
    
    std::lock_guard<std::mutex> lock(g_image_map_mutex);
    auto it = g_image_map.find(handle);
    if (it == g_image_map.end()) {
        LogDbg("SetImagePixelData: Image handle not found in map");
        return api_false;
    }
    
    MockImage* img = it->second;
    
    if (data == nullptr) {
        LogDbg("SetImagePixelData: Clearing pixel data (data is null)");
        img->pixelData = nullptr;
        return api_true;
    }
    
    // Add detailed logging
    LogDbg("SetImagePixelData: Setting pixel data for image " + 
             std::to_string(img->width) + "x" + std::to_string(img->height) + 
             " with " + std::to_string(img->channels) + " channels");
    for (uint32_t c = 0; c < img->channels; c++) {
        LogDbg("  Channel " + std::to_string(c) + ": " + 
                 (data[c] ? "valid pointer" : "NULL"));
    }
    
    img->pixelData = data;
    
    LogDbg("SetImagePixelData: Successfully set pixel data pointer array");
    return api_true;
}  

api_bool API_SharedImage_SetImageGeometry(image_handle handle, uint32_t w, uint32_t h, uint32_t n) {
    if (!handle) {
        return api_false;
    }
    
    LogDbg("SetImageGeometry called with dimensions: " + 
             std::to_string(w) + "x" + std::to_string(h) + 
             ", channels: " + std::to_string(n));
    
    std::lock_guard<std::mutex> lock(g_image_map_mutex);
    auto it = g_image_map.find(handle);
    if (it == g_image_map.end()) {
        return api_false;
    }
    
    MockImage* img = it->second;
    
    // Just update the metadata - DON'T touch pixelData!
    // PCL has already given us the pointers via SetImagePixelData
    img->width = w;
    img->height = h;
    img->channels = n;
    
    // Update stats array (we own this)
    if (img->stats) {
        delete[] img->stats;
    }
    img->stats = new double[n * 2];
    for (uint32_t i = 0; i < n; i++) {
        img->stats[i*2] = 0.0;
        img->stats[i*2+1] = (img->isFloat) ? 1.0 : 
                           ((img->bitsPerSample <= 8) ? 255 : 
                           ((img->bitsPerSample <= 16) ? 65535 : 4294967295.0));
    }
    
    LogDbg("SetImageGeometry: Successfully updated geometry metadata");
    return api_true;
}
  
// Set the color space of an image
api_bool API_SharedImage_SetImageColorSpace(image_handle handle, uint32_t cs) {
    if (!handle) {
        LogDbg("SetImageColorSpace: Invalid handle");
        return api_false;
    }
    
    LogDbg("SetImageColorSpace called with color space: " + std::to_string(cs));
    
    std::lock_guard<std::mutex> lock(g_image_map_mutex);
    auto it = g_image_map.find(handle);
    if (it == g_image_map.end()) {
        LogDbg("SetImageColorSpace: Image handle not found in map");
        return api_false;
    }
    
    // Set the new color space
    it->second->colorSpace = cs;
    
    LogDbg("SetImageColorSpace: Successfully set color space to " + std::to_string(cs));
    return api_true;
}

} // namespace pcl_mock

//     ____   ______ __
//    / __ \ / ____// /
//   / /_/ // /    / /
//  / ____// /___ / /___   PixInsight Class Library
// /_/     \____//_____/   PCL 2.9.4
// ----------------------------------------------------------------------------
// pcl/APIInterface.h - Released 2025-04-07T08:52:44Z
// ----------------------------------------------------------------------------
// This file is part of the PixInsight Class Library (PCL).
// PCL is a multiplatform C++ framework for development of PixInsight modules.
//
// Copyright (c) 2003-2025 Pleiades Astrophoto S.L. All Rights Reserved.
//
// Use of this source code is governed by the PixInsight Class Library License
// version 2.0, which can be found in the LICENSE file as well as at:
// https://pixinsight.com/license/PCL-License-2.0.html
// ----------------------------------------------------------------------------

extern "C"
{

// ----------------------------------------------------------------------------
// GlobalContext API
// ----------------------------------------------------------------------------

   /*
    * Error information
    */
   void        (API_Global_ClearError)()
   {

     abort();
   }
   api_bool    (API_Global_ErrorMessage)( uint32, char16_type*, size_type* )
   {

     abort();
   }

   /*
    * Thread status functions
    */

   api_bool    (API_Global_ResetProcessStatus)()
   {

     abort();
   }
   api_bool    (API_Global_EnableAbort)()
   {
     return EnableAbort();
   }
   api_bool    (API_Global_DisableAbort)()
   {

     abort();
   }
   api_bool    (API_Global_Abort)()
   {

     abort();
   }

   /*
    * Console functions
    */
   api_handle  (API_Global_GetThreadWindowId)()
   {
     // *** ### disabled function
     abort();
   }

   console_handle (API_Global_GetConsole)()
   {

     return GetConsole();
   }
   api_bool    (API_Global_ValidateConsole)( const_console_handle )
   {

     abort();
   }
   api_bool    (API_Global_WriteConsole)( console_handle handle, const char16_type *text, api_bool appendNewline )
   {

     return WriteConsole(handle, text, appendNewline);
   }
   int32       (API_Global_ReadConsoleChar)( console_handle )
   {
     // TODO - still not implemented
     abort();
   }

   // ### The following two functions return strings allocated by the caller module.
   char16_type*(API_Global_ReadConsoleString)( api_handle, console_handle )
   {
     // TODO - still not implemented
     abort();
   }
   char16_type*(API_Global_GetConsoleText)( api_handle, const_console_handle )
   {
     // ### security issues
     abort();
   }

   api_bool    (API_Global_FlushConsole)( console_handle )
   {

     abort();
   }
   api_bool    (API_Global_ShowConsole)( console_handle, api_bool )
   {

     abort();
   }
   api_bool    (API_Global_ExecuteCommand)( api_handle, console_handle, const char16_type* cmd )
   {

     abort();
   }

   /*
    * Global cursor position
    */
   void        (API_Global_GetCursorPosition)( int32* x, int32* y )
   {

     abort();
   }
   void        (API_Global_SetCursorPosition)( int32 x, int32 y )
   {

     abort();
   }

   /*
    * Returns an OR combination of pcl::ModifierKey values corresponding to the
    * current state of supported modifier keys.
    */
   uint32      (API_Global_GetKeyboardModifiers)()
   {

     abort();
   }

   /*
    * Tool tip window
    */
   void        (API_Global_ShowToolTipWindow)( int32 x, int32 y, const char16_type*,
                                              const_control_handle, int32, int32, int32, int32 )
   {

     abort();
   }
   void        (API_Global_HideToolTipWindow)()
   {

     abort();
   }
   api_bool    (API_Global_GetToolTipWindowText)( char16_type*, size_type* )
   {

     abort();
   }

   /*
    * MessageBox functions
    */
   uint32      (API_Global_MessageBox)( const char16_type* text, const char16_type* caption, uint32 button0, uint32 button1, uint32 button2, uint32 defButton, uint32 escButton, uint32 icon )
   {

     abort();
   }

   /*
    * Instance and interface launch functions
    */
   void        (API_Global_LaunchProcessInstance4)( meta_process_handle, const_process_handle, int32 mode, uint32 flags )
   {

     abort();
   }
   void        (API_Global_LaunchProcessInstanceOnView)( meta_process_handle, const_process_handle, view_handle, uint32 flags )
   {

     abort();
   }
   api_bool    (API_Global_LaunchProcessInterface)( meta_interface_handle, uint32 flags )
   {

     abort();
   }

   /*
    * Readout options
    */
   void        (API_Global_GetReadoutOptions)( api_readout_options* options )
   {

     memset(options, 0, sizeof(api_readout_options));
   }
  
   void        (API_Global_SetReadoutOptions)( const api_readout_options* options )
   {

     abort();
   }

   /*
    * Real-time preview
    * ### Obsolete -- preserved for compatibility -- see RealTimePreviewContext
    */
   api_bool    (API_Global_SetRealTimePreviewOwner)( interface_handle, uint32 flags )
   {

     abort();
   }
   api_bool    (API_Global_IsRealTimePreviewUpdating)()
   {

     abort();
   }
   void        (API_Global_UpdateRealTimePreview)()
   {

     abort();
   }

   /*
    * Integrated documentation system
    */
   api_bool    (API_Global_BrowseProcessDocumentation)( meta_process_handle, uint32 flags )
   {

     abort();
   }

   /*
    * Global settings
    */

   api_bool    (API_Global_GetGlobalReal)( const char*, double* )
   {

     abort();
   }
   api_bool    (API_Global_GetGlobalColor)( const char*, uint32* )
   {

     abort();
   }
   api_bool    (API_Global_GetGlobalFont)( const char*, char16_type*, size_type*, int32* sizePt )
   {

     abort();
   }
   api_bool    (API_Global_GetGlobalString)( const char*, char16_type*, size_type* )
   {

     abort();
   }

   api_bool    (API_Global_EnterGlobalSettingsUpdateContext)()
   {

     abort();
   }
   api_bool    (API_Global_IsGlobalSettingsUpdateContextActive)()
   {

     abort();
   }

   api_bool    (API_Global_SetGlobalFlag)( const char*, api_bool )
   {

     abort();
   }
   api_bool    (API_Global_SetGlobalInteger)( const char*, uint32, api_bool isSigned )
   {

     abort();
   }
   api_bool    (API_Global_SetGlobalReal)( const char*, double )
   {

     abort();
   }
   api_bool    (API_Global_SetGlobalColor)( const char*, uint32 )
   {

     abort();
   }
   api_bool    (API_Global_SetGlobalFont)( const char*, const char16_type*, int32 sizePt )
   {

     abort();
   }
   api_bool    (API_Global_SetGlobalString)( const char*, const char16_type* )
   {

     abort();
   }

   api_bool    (API_Global_CancelGlobalSettingsUpdate)( api_handle, uint32 reserved )
   {

     abort();
   }
   api_bool    (API_Global_ExitGlobalSettingsUpdateContext)()
   {

     abort();
   }

   /*
    * Module-defined settings
    */
   // ### The following two functions return data allocated by the caller module.
   api_bool    (API_Global_ReadSettingsBlock)( api_handle, void**, size_type*, const char* key, api_bool global )
   {

     abort();
   }
   api_bool    (API_Global_ReadSettingsString)( api_handle, char16_type**, const char* key, api_bool global )
   {

     abort();
   }

   api_bool    (API_Global_ReadSettingsFlag)( api_handle, api_bool*, const char* key, api_bool global )
   {

     abort();
   }

   api_bool    (API_Global_ReadSettingsUnsignedInteger)( api_handle, uint32*, const char* key, api_bool global )
   {

     abort();
   }
   api_bool    (API_Global_ReadSettingsReal)( api_handle, double*, const char* key, api_bool global )
   {

     abort();
   }

   api_bool    (API_Global_WriteSettingsBlock)( api_handle, const void*, size_type, const char* key, api_bool global )
   {

     abort();
   }
   api_bool    (API_Global_WriteSettingsString)( api_handle, const char16_type*, const char* key, api_bool global )
   {

     abort();
   }
   api_bool    (API_Global_WriteSettingsFlag)( api_handle, api_bool, const char* key, api_bool global )
   {

     abort();
   }
   api_bool    (API_Global_WriteSettingsInteger)( api_handle, int32, const char* key, api_bool global )
   {

     abort();
   }
   api_bool    (API_Global_WriteSettingsUnsignedInteger)( api_handle, uint32, const char* key, api_bool global )
   {

     abort();
   }
   api_bool    (API_Global_WriteSettingsReal)( api_handle, double, const char* key, api_bool global )
   {

     abort();
   }

   api_bool    (API_Global_DeleteSettingsItem)( api_handle, const char* key, api_bool global )
   {

     abort();
   }

   uint32      (API_Global_GetSettingsItemGlobalAccess)( api_handle, const char* key )
   {
     // bit#0=W bit#1=R
     abort();
   }
   api_bool    (API_Global_SetSettingsItemGlobalAccess)( api_handle, const char* key, uint32 flags )
   {
     // bit#0=W bit#1=R
     abort();
   }

   /*
    * Miscellaneous message broadcasting
    */
   void        (API_Global_BroadcastImageUpdated)( const_view_handle, const void* /*reserved*/ )
   {

     abort();
   }
   void        (API_Global_BroadcastGlobalFiltersUpdated)( const void* /*reserved*/ )
   {

     abort();
   }

   /*
    * Miscellaneous color management
    */
   api_bool    (API_Global_GetProfilesDirectory)( int32, char16_type*, size_type* )
   {

     abort();
   }

   /*
    * Access to the global PixelTraits LUT
    */
   const ::api_pixtraits_lut* (API_Global_GetPixelTraitsLUT)( uint32 version )
   {
     // version must be zero
     abort();
   }

   /*
    * Fast module thread control (API_Global_since core version 1.8.8-7)
    */
   int32       (API_Global_MaxProcessorsAllowedForModule)( api_handle, uint32 flags/*unused*/ )
   {

     abort();
   }

   /*
    * Instance slot of the running application in [1,256] (API_Global_since core version 1.8.9-2 build 1581)
    */
   int32       (API_Global_ApplicationInstanceSlot)( api_handle )
   {

     abort();
   }

   /*
    * Application configuration directory (API_Global_since core version 1.8.9-2 build 1581)
    */
   api_bool    (API_Global_GetApplicationConfigurationDirectory)( char16_type*, size_type* )
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
// FileFormatDefinitionContext API
// ----------------------------------------------------------------------------

void API_FileFormatDefinition_EnterFileFormatDefinitionContext()
{

  abort();
}
api_bool API_FileFormatDefinition_IsFileFormatDefinitionContextActive()
{

  abort();
}
void API_FileFormatDefinition_BeginFileFormatDefinition(meta_format_handle, const char* fmtName, const char16_type** fmtExtensions, const char** fmtMimeTypes)
{

  abort();
}
api_bool API_FileFormatDefinition_GetFileFormatBeingDefined(char*, size_type*)
{
  abort();
}
void API_FileFormatDefinition_SetFileFormatVersion(uint32)
{

  abort();
}
void API_FileFormatDefinition_SetFileFormatDescription(const char16_type*)
{

  abort();
}
void API_FileFormatDefinition_SetFileFormatImplementation(const char16_type*)
{

  abort();
}
void API_FileFormatDefinition_SetFileFormatIconSVG(const char*)
{

  abort();
}
void API_FileFormatDefinition_SetFileFormatIconSVGFile(const char16_type*)
{

  abort();
}
void API_FileFormatDefinition_SetFileFormatIconImage(const char**)
{
  // ### deprecated
  abort();
}
   void           (API_FileFormatDefinition_SetFileFormatIconImageFile)( const char16_type* )
   {
     // ### deprecated
     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatIconSmallImage)( const char** )
   {
     // ### deprecated
     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatIconSmallImageFile)( const char16_type* )
   {
     // ### deprecated
     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatCaps)( const api_format_capabilities* )
   {

     abort();
   }

   void           (API_FileFormatDefinition_SetFileFormatCreationRoutine)( pcl::format_creation_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatDestructionRoutine)( pcl::format_destruction_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatValidateFormatSpecificDataRoutine)( pcl::format_validate_format_specific_data_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatDisposeFormatSpecificDataRoutine)( pcl::format_dispose_format_specific_data_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatEditPreferencesRoutine)( pcl::format_edit_preferences_routine )
   {

     abort();
   }

   void           (API_FileFormatDefinition_SetFileFormatOpenRoutine)( pcl::format_open_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatGetImageCountRoutine)( pcl::format_get_image_count_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatGetImageIdRoutine)( pcl::format_get_image_id_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatGetImageDescriptionRoutine)( pcl::format_get_image_description_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatCloseRoutine)( pcl::format_close_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatIsOpenRoutine)( pcl::format_is_open_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatGetFilePathRoutine)( pcl::format_get_file_path_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatSetSelectedImageIndexRoutine)( pcl::format_set_selected_image_index_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatGetSelectedImageIndexRoutine)( pcl::format_get_selected_image_index_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatSetFormatSpecificDataRoutine)( pcl::format_set_format_specific_data_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatGetFormatSpecificDataRoutine)( pcl::format_get_format_specific_data_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatGetImageFormatInfoRoutine)( pcl::format_get_image_format_info_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatBeginKeywordExtractionRoutine)( pcl::format_begin_extraction_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatGetKeywordCountRoutine)( pcl::format_get_keyword_count_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatGetNextKeywordRoutine)( pcl::format_get_next_keyword_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatEndKeywordExtractionRoutine)( pcl::format_end_extraction_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatBeginICCProfileExtractionRoutine)( pcl::format_begin_extraction_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatGetICCProfileRoutine)( pcl::format_get_icc_profile_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatEndICCProfileExtractionRoutine)( pcl::format_end_extraction_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatBeginThumbnailExtractionRoutine)( pcl::format_begin_extraction_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatGetThumbnailRoutine)( pcl::format_get_thumbnail_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatEndThumbnailExtractionRoutine)( pcl::format_end_extraction_routine )
   {

     abort();
   }

   void           (API_FileFormatDefinition_SetFileFormatEnumerateImagePropertiesRoutine)( pcl::format_enumerate_image_properties_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatBeginImagePropertyExtractionRoutine)( pcl::format_begin_extraction_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatGetImagePropertyRoutine)( pcl::format_get_image_property_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatEndImagePropertyExtractionRoutine)( pcl::format_end_extraction_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatBeginImagePropertyEmbeddingRoutine)( pcl::format_begin_embedding_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatSetImagePropertyRoutine)( pcl::format_set_image_property_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatEndImagePropertyEmbeddingRoutine)( pcl::format_end_embedding_routine )
   {

     abort();
   }

   void           (API_FileFormatDefinition_SetFileFormatEnumeratePropertiesRoutine)( pcl::format_enumerate_image_properties_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatBeginPropertyExtractionRoutine)( pcl::format_begin_extraction_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatGetPropertyRoutine)( pcl::format_get_image_property_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatEndPropertyExtractionRoutine)( pcl::format_end_extraction_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatBeginPropertyEmbeddingRoutine)( pcl::format_begin_embedding_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatSetPropertyRoutine)( pcl::format_set_image_property_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatEndPropertyEmbeddingRoutine)( pcl::format_end_embedding_routine )
   {

     abort();
   }

   void           (API_FileFormatDefinition_SetFileFormatBeginRGBWSExtractionRoutine)( pcl::format_begin_extraction_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatGetImageRGBWSRoutine)( pcl::format_get_image_rgbws_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatEndRGBWSExtractionRoutine)( pcl::format_end_extraction_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatBeginRGBWSEmbeddingRoutine)( pcl::format_begin_embedding_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatSetImageRGBWSRoutine)( pcl::format_set_image_rgbws_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatEndRGBWSEmbeddingRoutine)( pcl::format_end_embedding_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatBeginDisplayFunctionExtractionRoutine)( pcl::format_begin_extraction_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatGetImageDisplayFunctionRoutine)( pcl::format_get_image_display_function_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatEndDisplayFunctionExtractionRoutine)( pcl::format_end_extraction_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatBeginDisplayFunctionEmbeddingRoutine)( pcl::format_begin_embedding_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatSetImageDisplayFunctionRoutine)( pcl::format_set_image_display_function_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatEndDisplayFunctionEmbeddingRoutine)( pcl::format_end_embedding_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatBeginColorFilterArrayExtractionRoutine)( pcl::format_begin_extraction_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatGetImageColorFilterArrayRoutine)( pcl::format_get_image_color_filter_array_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatEndColorFilterArrayExtractionRoutine)( pcl::format_end_extraction_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatBeginColorFilterArrayEmbeddingRoutine)( pcl::format_begin_embedding_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatSetImageColorFilterArrayRoutine)( pcl::format_set_image_color_filter_array_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatEndColorFilterArrayEmbeddingRoutine)( pcl::format_end_embedding_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatReadImageRoutine)( pcl::format_read_image_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatAllowIncrementalReadRoutine)( pcl::format_allow_incremental_op_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatReadSamplesRoutine)( pcl::format_read_pixels_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatQueryOptionsRoutine)( pcl::format_query_options_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatCreateRoutine)( pcl::format_create_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatSetImageIdRoutine)( pcl::format_set_image_id_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatSetImageOptionsRoutine)( pcl::format_set_image_options_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatCreateImageRoutine)( pcl::format_create_image_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatCloseImageRoutine)( pcl::format_close_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatBeginKeywordEmbeddingRoutine)( pcl::format_begin_embedding_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatAddKeywordRoutine)( pcl::format_add_keyword_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatEndKeywordEmbeddingRoutine)( pcl::format_end_embedding_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatBeginICCProfileEmbeddingRoutine)( pcl::format_begin_embedding_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatSetICCProfileRoutine)( pcl::format_set_icc_profile_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatEndICCProfileEmbeddingRoutine)( pcl::format_end_embedding_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatBeginThumbnailEmbeddingRoutine)( pcl::format_begin_embedding_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatSetThumbnailRoutine)( pcl::format_set_thumbnail_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatEndThumbnailEmbeddingRoutine)( pcl::format_end_embedding_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatWriteImageRoutine)( pcl::format_write_image_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatAllowIncrementalWriteRoutine)( pcl::format_allow_incremental_op_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatWriteSamplesRoutine)( pcl::format_write_pixels_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatQueryInexactReadRoutine)( pcl::format_query_inexact_read_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatQueryLossyWriteRoutine)( pcl::format_query_lossy_write_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_SetFileFormatQueryFormatStatusRoutine)( pcl::format_query_format_status_routine )
   {

     abort();
   }
   void           (API_FileFormatDefinition_EndFileFormatDefinition)()
   {

     abort();
   }

   void           (API_FileFormatDefinition_ExitFileFormatDefinitionContext)()
   {

     abort();
   }

// ----------------------------------------------------------------------------
// ModuleContext API
// ----------------------------------------------------------------------------

api_bool API_Module_LoadResource(api_handle, const char16_type*, const char16_type*)
{

  abort();
}
api_bool API_Module_UnloadResource(api_handle, const char16_type*, const char16_type*)
{

  abort();
}
api_bool API_Module_EvaluateScript(api_handle, api_property_value* result, const char16_type* sourceCode, const char* language)
{

  abort();
}
api_bool API_Module_HasEntitlement(api_handle, const char*)
{

  abort();
}

// ----------------------------------------------------------------------------
// ProcessContext API
// ----------------------------------------------------------------------------

api_bool API_Process_EnumerateProcessCategories(pcl::category_enumeration_callback, char*, size_type*, void*)
{

  abort();
}
api_bool API_Process_EnumerateProcesses(pcl::process_enumeration_callback, void*)
{

  abort();
}
meta_process_handle API_Process_GetProcessByName(api_handle, const char* id)
{

  abort();
}
api_bool API_Process_GetProcessIdentifier(meta_process_handle, char*, size_type*)
{

  abort();
}
api_bool API_Process_GetProcessCategory(meta_process_handle, char*, size_type*)
{

  abort();
}
uint32 API_Process_GetProcessVersion(meta_process_handle)
{

  abort();
}
api_bool API_Process_GetProcessAliasIdentifiers(meta_process_handle, char*, size_type*)
{

  abort();
}
api_bool API_Process_GetProcessDescription(meta_process_handle, char16_type*, size_type*)
{

  abort();
}
api_bool API_Process_GetProcessScriptComment(meta_process_handle, char16_type*, size_type*)
{

  abort();
}
bitmap_handle API_Process_GetProcessIcon(meta_process_handle)
{

  abort();
}
bitmap_handle API_Process_GetProcessSmallIcon(meta_process_handle)
{

  abort();
}
api_bool API_Process_GetProcessProperties(meta_process_handle, api_process_properties*)
{

  abort();
}
interface_handle API_Process_GetProcessDefaultInterface(meta_process_handle)
{

  abort();
}
api_bool API_Process_EditProcessPreferences(meta_process_handle)
{

  abort();
}
api_bool API_Process_BrowseProcessDocumentation(meta_process_handle, uint32 flags)
{

  abort();
}
int32 API_Process_RunProcessCommandLine(meta_process_handle, const char16_type*)
{

  abort();
}
api_bool API_Process_LaunchProcess(meta_process_handle)
{

  abort();
}
api_bool API_Process_EnumerateProcessParameters(meta_process_handle, pcl::parameter_enumeration_callback, void*)
{

  abort();
}
api_bool API_Process_EnumerateTableColumns(meta_parameter_handle, pcl::parameter_enumeration_callback, void*)
{

  abort();
}
meta_parameter_handle API_Process_GetParameterByName(meta_process_handle, const char* id)
{

  abort();
}
meta_parameter_handle API_Process_GetTableColumnByName(meta_parameter_handle, const char* id)
{

  abort();
}
meta_process_handle API_Process_GetParameterProcess(meta_parameter_handle)
{

  abort();
}
meta_parameter_handle API_Process_GetParameterTable(meta_parameter_handle)
{

  abort();
}
uint32 API_Process_GetParameterType(meta_parameter_handle)
{

  abort();
}
api_bool API_Process_GetParameterIdentifier(meta_parameter_handle, char*, size_type*)
{

  abort();
}
api_bool API_Process_GetParameterAliasIdentifiers(meta_parameter_handle, char*, size_type*)
{

  abort();
}
api_bool API_Process_GetParameterDescription(meta_parameter_handle, char16_type*, size_type*)
{

  abort();
}
api_bool API_Process_GetParameterScriptComment(meta_parameter_handle, char16_type*, size_type*)
{

  abort();
}
api_bool API_Process_GetParameterRequired(meta_parameter_handle)
{

  abort();
}
api_bool API_Process_GetParameterReadOnly(meta_parameter_handle)
{
  // Boolean, numeric and string parameter types
  abort();
}
   api_bool             (API_Process_GetParameterDefaultValue)( meta_parameter_handle, void* value, size_type* length )
   {

     abort();
   }

   // Enumerated parameters
   size_type            (API_Process_GetParameterElementCount)( meta_parameter_handle )
   {

     abort();
   }
   api_bool             (API_Process_GetParameterElementIdentifier)( meta_parameter_handle, size_type index, char*, size_type* )
   {

     abort();
   }
   api_bool             (API_Process_GetParameterElementAliasIdentifiers)( meta_parameter_handle, size_type index, char*, size_type* )
   {

     abort();
   }
   api_enum             (API_Process_GetParameterElementValue)( meta_parameter_handle, size_type index )
   {

     abort();
   }
   int32                (API_Process_GetParameterDefaultElementIndex)( meta_parameter_handle )
   {

     abort();
   }

   // Numeric parameters
   api_bool             (API_Process_GetParameterRange)( meta_parameter_handle, double* minValue, double* maxValue )
   {

     abort();
   }
   int32                (API_Process_GetParameterPrecision)( meta_parameter_handle )
   {

     abort();
   }
   api_bool             (API_Process_GetParameterScientificNotation)( meta_parameter_handle )
   {

     abort();
   }

   // Variable length parameters
   api_bool             (API_Process_GetParameterLengthLimits)( meta_parameter_handle, size_type* minLength, size_type* maxLength )
   {

     abort();
   }

   // String parameters
   api_bool             (API_Process_GetParameterAllowedCharacters)( meta_parameter_handle, char16_type*, size_type* )
   {

     abort();
   }

   process_handle       (API_Process_CreateProcessInstance)( api_handle, meta_process_handle )
   {

     abort();
   }

   meta_process_handle  (API_Process_GetProcessInstanceProcess)( const_process_handle )
   {

     abort();
   }

   uint32               (API_Process_GetProcessInstanceVersion)( const_process_handle )
   {

     abort();
   }

   process_handle       (API_Process_CloneProcessInstance)( api_handle, const_process_handle, uint32 flags )
   {

     abort();
   }
   api_bool             (API_Process_AssignProcessInstance)( process_handle, const_process_handle, uint32 flags )
   {

     abort();
   }
   api_bool             (API_Process_ValidateProcessInstance)( process_handle, char16_type*, size_type )
   {

     abort();
   }

   api_bool             (API_Process_GetUpdatesViewHistory)( const_process_handle, const_view_handle )
   {

     abort();
   }
   api_bool             (API_Process_ValidateViewExecutionMask)( const_process_handle, const_view_handle, const_window_handle )
   {

     abort();
   }
   api_bool             (API_Process_ValidateViewExecution)( const_process_handle, const_view_handle, char16_type*, size_type )
   {

     abort();
   }
   api_bool             (API_Process_ExecuteOnView)( process_handle, view_handle, uint32 flags )
   {

     abort();
   }

   api_bool             (API_Process_ValidateGlobalExecution)( const_process_handle, char16_type*, size_type )
   {

     abort();
   }
   api_bool             (API_Process_ExecuteGlobal)( process_handle, uint32 flags )
   {

     abort();
   }

   api_bool             (API_Process_ValidateImageExecution)( const_process_handle, const_image_handle, char16_type*, size_type )
   {

     abort();
   }
   api_bool             (API_Process_ExecuteOnImage)( process_handle, image_handle, const char*, uint32 flags )
   {

     abort();
   }

   api_bool             (API_Process_LaunchProcessInstance1)( process_handle )
   {

     abort();
   }

   api_bool             (API_Process_ValidateInterfaceLaunch)( const_process_handle )
   {

     abort();
   }
   api_bool             (API_Process_LaunchInterface)( process_handle )
   {

     abort();
   }

   // ### TODO: The following two functions are not yet implemented.
   api_bool             (API_Process_ValidateInterface)( const_process_handle, const_interface_handle, char16_type*, size_type )
   {

     abort();
   }
   interface_handle     (API_Process_GetInterface)( const_process_handle )
   {

     abort();
   }

   api_bool             (API_Process_GetProcessInstanceDescription)( const_process_handle, char16_type*, size_type* )
   {

     abort();
   }
   api_bool             (API_Process_SetProcessInstanceDescription)( process_handle, const char16_type* )
   {

     abort();
   }

   api_bool             (API_Process_GetExecutionTimes)( const_process_handle, double* startJD, double* elapsedSecs )
   {

     abort();
   }

   // ### Returns a string allocated by the caller module.
   char16_type*         (API_Process_GetProcessInstanceSourceCode)( api_handle, const_process_handle, const char* language, const char* varId, uint32 indent )
   {

     abort();
   }
   process_handle       (API_Process_CreateProcessInstanceFromSourceCode)( const char16_type* source, const char* language )
   {

     abort();
   }

   process_handle       (API_Process_CreateProcessInstanceFromIcon)( api_handle, const char* iconId )
   {

     abort();
   }

   api_bool             (API_Process_EnumerateProcessIcons)( pcl::icon_enumeration_callback, char*, size_type*, void* )
   {

     abort();
   }

   api_bool             (API_Process_GetParameterValue)( const_process_handle, meta_parameter_handle, size_type tableRow, uint32* parType, void* value, size_type* length )
   {

     abort();
   }
   api_bool             (API_Process_SetParameterValue)( process_handle, meta_parameter_handle, size_type tableRow, const void* value, size_type length )
   {

     abort();
   }

   size_type            (API_Process_GetTableRowCount)( const_process_handle, meta_parameter_handle )
   {

     abort();
   }

   api_bool             (API_Process_AllocateTableRows)( process_handle, meta_parameter_handle, size_type rowCount )
   {

     abort();
   }

// ----------------------------------------------------------------------------
// InterfaceContext API
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// FileFormatContext API
// ----------------------------------------------------------------------------

api_bool API_FileFormat_EnumerateFileFormats(pcl::format_enumeration_callback, void*)
{

  abort();
}
meta_format_handle API_FileFormat_GetFileFormatByName(api_handle, const char* id)
{

  abort();
}

meta_format_handle API_FileFormat_GetFileFormatByMimeType(api_handle, const char* mimeType, api_bool toRead, api_bool toWrite)
{

  abort();
}

api_bool API_FileFormat_GetFileFormatName(meta_format_handle handle, char* name, size_type* len)
{
    if (handle == nullptr || len == nullptr) {
        return 0;
    }
    
    // FIXED: Cast to MockFormatCapabilities
    const MockFormatCapabilities* format = static_cast<const MockFormatCapabilities*>(handle);
    
    if (name == nullptr) {
        *len = format->name.length();
        return 1;
    }
    
    size_type copyLen = std::min(*len - 1, format->name.length());
    std::strncpy(name, format->name.c_str(), copyLen);
    name[copyLen] = '\0';
    *len = copyLen;
    
    return 1;
}

api_bool API_FileFormat_GetFileFormatFileExtensions(meta_format_handle, char16_type**, size_type* extCount, size_type* maxExtLen)
{

  abort();
}
api_bool API_FileFormat_GetFileFormatMimeTypes(meta_format_handle, char**, size_type* mimeCount, size_type* maxMimeLen)
{

  abort();
}
uint32 API_FileFormat_GetFileFormatVersion(meta_format_handle)
{

  abort();
}
api_bool API_FileFormat_GetFileFormatDescription(meta_format_handle, char16_type*, size_type*)
{

  abort();
}

api_bool API_FileFormat_GetFileFormatImplementation(meta_format_handle, char16_type*, size_type*)
{

  abort();
}

bitmap_handle API_FileFormat_GetFileFormatIcon(meta_format_handle)
{

  abort();
}

bitmap_handle API_FileFormat_GetFileFormatSmallIcon(meta_format_handle)
{

  abort();
}

api_bool API_FileFormat_GetFileFormatStatus(meta_format_handle, char16_type*, size_type*, void*)
{

  abort();
}

api_bool API_FileFormat_EditFileFormatPreferences(meta_format_handle)
{

  abort();
}

file_format_handle API_FileFormat_CreateFileFormatInstance(api_handle handle, meta_format_handle meta_handle)
{

  return CreateFileFormatInstance( handle, meta_handle) ;
}

meta_format_handle API_FileFormat_GetFileFormatInstanceFormat(const_file_format_handle)
{

  abort();
}

api_bool API_FileFormat_IsImageFileOpen(const_file_format_handle)
{

  abort();
}
api_bool API_FileFormat_GetImageFilePath(file_format_handle, char16_type*, size_type*)
{

  abort();
}
api_bool API_FileFormat_OpenImageFile(file_format_handle, const char16_type*)
{

  abort();
}

uint32 API_FileFormat_GetImageCount(const_file_format_handle handle)
{
  return GetImageCount( handle);
}

api_bool API_FileFormat_GetImageId(const_file_format_handle handle, char *id, size_type *len, uint32 index)
{
  return GetImageId( handle, id, len, index);
}

api_bool API_FileFormat_GetImageDescription(const_file_format_handle handle, api_image_info *info, api_image_options *options, uint32 index)
{
  return GetImageDescription( handle, info, options, index);
}

uint32 API_FileFormat_GetSelectedImageIndex(const_file_format_handle)
{

  abort();
}

void *API_FileFormat_GetFormatSpecificData(file_format_handle)
{

  abort();
}

api_bool API_FileFormat_SetFormatSpecificData(file_format_handle, const void*)
{

  abort();
}
api_bool API_FileFormat_ValidateFormatSpecificData(meta_format_handle, const void*)
{

  abort();
}
void API_FileFormat_DisposeFormatSpecificData(meta_format_handle, const void*)
{

  abort();
}
api_bool API_FileFormat_GetImageFormatInfo(const_file_format_handle, char16_type*, size_type*)
{

  abort();
}
api_bool API_FileFormat_BeginKeywordExtraction(file_format_handle)
{

  abort();
}
size_type API_FileFormat_GetKeywordCount(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_GetNextKeyword(file_format_handle, char*, char*, char*, uint32)
{

  abort();
}
void API_FileFormat_EndKeywordExtraction(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_BeginICCProfileExtraction(file_format_handle)
{

  abort();
}
void API_FileFormat_EndICCProfileExtraction(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_BeginThumbnailExtraction(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_GetThumbnail(file_format_handle, image_handle)
{

  abort();
}
void API_FileFormat_EndThumbnailExtraction(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_EnumerateProperties(file_format_handle, pcl::property_enumeration_callback, char*, size_type*, void*)
{

  abort();
}
api_bool API_FileFormat_BeginPropertyExtraction(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_GetProperty(file_format_handle, const char* id, api_property_value*)
{

  abort();
}
void API_FileFormat_EndPropertyExtraction(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_BeginPropertyEmbedding(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_SetProperty(file_format_handle, const char* id, const api_property_value*)
{

  abort();
}
void API_FileFormat_EndPropertyEmbedding(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_EnumerateImageProperties(file_format_handle, pcl::property_enumeration_callback, char*, size_type*, void*)
{

  abort();
}
api_bool API_FileFormat_BeginImagePropertyExtraction(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_GetImageProperty(file_format_handle, const char* id, api_property_value*)
{

  abort();
}
void API_FileFormat_EndImagePropertyExtraction(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_BeginImagePropertyEmbedding(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_SetImageProperty(file_format_handle, const char* id, const api_property_value*)
{

  abort();
}
void API_FileFormat_EndImagePropertyEmbedding(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_BeginRGBWSExtraction(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_GetImageRGBWS(file_format_handle, float*, api_bool*, float*, float*, float*)
{

  abort();
}
void API_FileFormat_EndRGBWSExtraction(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_BeginRGBWSEmbedding(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_SetImageRGBWS(file_format_handle, float, api_bool, const float*, const float*, const float*)
{

  abort();
}
void API_FileFormat_EndRGBWSEmbedding(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_BeginDisplayFunctionExtraction(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_GetImageDisplayFunction(file_format_handle, double*, double*, double*, double*, double*)
{

  abort();
}
void API_FileFormat_EndDisplayFunctionExtraction(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_BeginDisplayFunctionEmbedding(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_SetImageDisplayFunction(file_format_handle, const double*, const double*, const double*, const double*, const double*)
{

  abort();
}
void API_FileFormat_EndDisplayFunctionEmbedding(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_BeginColorFilterArrayExtraction(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_GetImageColorFilterArray(file_format_handle, char*, size_type*, int32*, int32*, char16_type*, size_type*)
{

  abort();
}
void API_FileFormat_EndColorFilterArrayExtraction(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_BeginColorFilterArrayEmbedding(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_SetImageColorFilterArray(file_format_handle, const char*, int32, int32, const char16_type*)
{

  abort();
}
void API_FileFormat_EndColorFilterArrayEmbedding(file_format_handle)
{

  abort();
}

api_bool API_FileFormat_CanReadIncrementally(const_file_format_handle)
{

  abort();
}
api_bool API_FileFormat_ReadSamples(file_format_handle, void*, uint32, uint32, uint32, uint32, api_bool, api_bool)
{

  abort();
}
api_bool API_FileFormat_QueryImageFileOptions(file_format_handle, api_image_options*, const void**, uint32)
{

  abort();
}

api_bool API_FileFormat_SetImageId(file_format_handle, const char*)
{

  abort();
}
api_bool API_FileFormat_SetImageOptions(file_format_handle, const api_image_options*)
{

  abort();
}
api_bool API_FileFormat_BeginKeywordEmbedding(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_AddKeyword(file_format_handle, const char*, const char*, const char*)
{

  abort();
}
void API_FileFormat_EndKeywordEmbedding(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_BeginICCProfileEmbedding(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_SetICCProfile(file_format_handle, const void*)
{

  abort();
}
void *API_FileFormat_GetICCProfile(file_format_handle)
{

  abort();
}
void API_FileFormat_EndICCProfileEmbedding(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_BeginThumbnailEmbedding(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_SetThumbnail(file_format_handle, const_image_handle)
{

  abort();
}
void API_FileFormat_EndThumbnailEmbedding(file_format_handle)
{

  abort();
}

api_bool API_FileFormat_CanWriteIncrementally(const_file_format_handle)
{

  abort();
}

api_bool API_FileFormat_WriteSamples(file_format_handle, const void*, uint32, uint32, uint32, uint32, api_bool, api_bool)
{

  abort();
}
api_bool API_FileFormat_CloseImage(file_format_handle)
{

  abort();
}
api_bool API_FileFormat_WasInexactRead(const_file_format_handle)
{

  abort();
}
api_bool API_FileFormat_WasLossyWrite(const_file_format_handle)
{

  abort();
}

// ----------------------------------------------------------------------------
// UIContext API
// ----------------------------------------------------------------------------

api_bool API_UI_AttachToUIObject(api_handle, api_handle)
{

  abort();
}

api_handle API_UI_GetUIObjectModule(const_api_handle)
{

  abort();
}

api_bool API_UI_SetHandleDestroyedEventRoutine(api_handle, pcl::destroy_event_routine)
{

  abort();
}

// ----------------------------------------------------------------------------
// ActionContext API
// ----------------------------------------------------------------------------

action_handle API_Action_CreateActionSVG(api_handle, api_handle client, const char16_type* menuItem, const char16_type* toolBar, const char* svgIcon, uint32 flags)
{

  abort();
}
action_handle API_Action_CreateActionSVGFile(api_handle, api_handle client, const char16_type* menuItem, const char16_type* toolBar, const char16_type* svgIconPath, uint32 flags)
{
  // ### deprecated
  abort();
}
   action_handle  (API_Action_CreateAction)( api_handle, api_handle client,
                                            const char16_type* menuItem, const char16_type* toolBar,
                                            const_bitmap_handle icon,
                                            uint32 flags )
   {

     abort();
   }

   api_bool       (API_Action_GetActionMenuItem)( const_action_handle, char16_type*, size_type* )
   {

     abort();
   }

   api_bool       (API_Action_GetActionMenuText)( const_action_handle, char16_type*, size_type* )
   {

     abort();
   }
   void           (API_Action_SetActionMenuText)( action_handle, const char16_type* )
   {

     abort();
   }

   api_bool       (API_Action_GetActionToolBar)( const_action_handle, char16_type*, size_type* )
   {

     abort();
   }

   api_bool       (API_Action_GetActionToolTip)( const_action_handle, char16_type*, size_type* )
   {

     abort();
   }
   void           (API_Action_SetActionToolTip)( action_handle, const char16_type* )
   {

     abort();
   }

   bitmap_handle  (API_Action_GetActionIcon)( const_action_handle )
   {

     abort();
   }
   void           (API_Action_SetActionIconSVG)( action_handle, const char* )
   {

     abort();
   }
   void           (API_Action_SetActionIconSVGFile)( action_handle, const char16_type* )
   {

     abort();
   }
   // ### deprecated
   void           (API_Action_SetActionIcon)( action_handle, const_bitmap_handle )
   {

     abort();
   }

   void           (API_Action_GetActionAccelerator)( const_action_handle, int32* keyModifiers, int32* keyCode )
   {

     abort();
   }
   void           (API_Action_SetActionAccelerator)( action_handle, int32 keyModifiers, int32 keyCode )
   {

     abort();
   }

   api_bool       (API_Action_SetActionExecutionRoutine)( action_handle, pcl::action_execution_routine )
   {

     abort();
   }
   api_bool       (API_Action_SetActionStateQueryRoutine)( action_handle, pcl::action_state_query_routine )
   {

     abort();
   }

// ----------------------------------------------------------------------------
// ControlContext API
// ----------------------------------------------------------------------------

void API_Control_GetFrameRect(const_control_handle handle,
                              int32* x, int32* y, int32* w, int32* h)
{
    LogDebug("GetFrameRect called");

    QWidget* widget = reinterpret_cast<QWidget*>(const_cast<control_handle>(handle));
    if (!widget) {
        LogDebug("GetFrameRect: null widget");
        if (x) *x = 0; if (y) *y = 0; if (w) *w = 0; if (h) *h = 0;
        return;
    }

    QRect r = widget->geometry();   // includes frame for top-level windows
    if (x) *x = r.x();
    if (y) *y = r.y();
    if (w) *w = r.width();
    if (h) *h = r.height();
}

api_bool API_Control_GetClientRect(const_control_handle handle,
                                   int32* x, int32* y,
                                   int32* w, int32* hgt)
{
    if (!w || !hgt) return api_false;
    
    MockControl* wdg = GetControlBox(handle);
    if (wdg) {
    QRect r = wdg->widget->contentsRect();

      if (x) *x = r.x();
      if (y) *y = r.y();
      *w   = r.width();
      *hgt = r.height();

      return api_true;

    }

    // Return a harmless safe rect
    if (x) *x = 0;
    if (y) *y = 0;
    *w   = 0;
    *hgt = 0;
    return api_true;

}

void API_Control_SetClientRect(control_handle handle,
                               int32 x, int32 y, int32 w, int32 h)
{
    LogDebug("SetClientRect called: x=" + std::to_string(x) +
             " y=" + std::to_string(y) +
             " w=" + std::to_string(w) +
             " h=" + std::to_string(h));

    QWidget* widget = reinterpret_cast<QWidget*>(handle);
    if (!widget) {
        LogDebug("SetClientRect: null widget");
        return;
    }

    // To set a *client* rect, we must adjust for frame margins
    // but since this is mock API, simplest is: place at (x,y) and size to (w,h)
    // ignoring OS window frame thickness.
    // PCL mock only needs approximate behaviour for layout tests.

    widget->setGeometry(x, y, w, h);
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

   control_handle (API_Control_GetControlWindow)( const_control_handle )
   {
     // returns client handle
     abort();
   }

   api_bool       (API_Control_GetControlMouseTrackingEnabled)( const_control_handle )
   {

     abort();
   }
   void           (API_Control_SetControlMouseTrackingEnabled)( control_handle, api_bool )
   {

     //     abort();
   }

   void           (API_Control_GetControlVisibleRect)( const_control_handle, int32*, int32*, int32*, int32* )
   {

     abort();
   }

   api_bool       (API_Control_GetWindowState)( const_control_handle, api_bool* active, api_bool* modal, api_bool* maximized, api_bool* minimized )
   {
     // returns true if control is a window
     abort();
   }

   void           (API_Control_ActivateWindow)( control_handle )
   {

     abort();
   }

   api_bool       (API_Control_GetControlFocus)( const_control_handle )
   {

     abort();
   }
   void           (API_Control_SetControlFocus)( control_handle, api_bool )
   {

     abort();
   }

   int32          (API_Control_GetControlFocusStyle)( const_control_handle )
   {

     abort();
   }

   control_handle (API_Control_GetFocusChildControl)( const_control_handle )
   {
     // returns client handle
     abort();
   }

   control_handle (API_Control_GetChildControlToFocus)( const_control_handle )
   {
     // returns client handle
     abort();
   }

   api_bool API_Control_SetChildControlToFocus(
	   control_handle parentHandle,
	   control_handle childHandle )
   {
       LogDbg("API_Control_SetChildControlToFocus called");

       MockControl* pwdg = GetControlBox(parentHandle);
       if (!pwdg) return api_false;
       QWidget* parent = pwdg->widget;
       MockControl* cwdg = GetControlBox(childHandle);
       if (!cwdg) return api_false;
       QWidget* child = cwdg->widget;

       if (!parent || !child)
	   return api_false;

       // Must actually be a descendant
       if (!child->isAncestorOf(parent) && !parent->isAncestorOf(child)) {
	   // Not required by PI, but prevents nonsensical calls
	   LogDbg("SetChildControlToFocus: widget is not a child of parent");
       }

       if (!child->isVisible()) {
	   // Same as PixInsight – ensure visible before focusing
	   child->show();
       }

       child->setFocus(Qt::OtherFocusReason);

       return api_true;
   }

   control_handle (API_Control_GetNextSiblingControlToFocus)( const_control_handle )
   {
     // returns client handle
     abort();
   }
   void           (API_Control_SetNextSiblingControlToFocus)( control_handle, control_handle )
   {

     abort();
   }

   api_bool       (API_Control_GetControlUpdatesEnabled)( const_control_handle )
   {

     abort();
   }
   void           (API_Control_SetControlUpdatesEnabled)( control_handle, api_bool )
   {

     //     abort();
   }

   void           (API_Control_UpdateControlRect)( control_handle, int32, int32, int32, int32 )
   {

     abort();
   }

   void           (API_Control_RepaintControlRect)( control_handle, int32, int32, int32, int32 )
   {

     abort();
   }

   void           (API_Control_RestyleControl)( control_handle )
   {

     abort();
   }

   void           (API_Control_EnsureControlLayoutUpdated)( control_handle )
   {

     //     abort();
   }

   void           (API_Control_ScrollControl)( control_handle, int32, int32 )
   {

     abort();
   }
   void           (API_Control_ScrollControlRect)( control_handle, int32, int32, int32, int32, int32, int32 )
   {

     abort();
   }

   void           (API_Control_SetControlCursorToParent)( control_handle )
   {

     abort();
   }

   api_bool       (API_Control_GetControlStyleSheet)( const_control_handle, char16_type*, size_type* )
   {

     abort();
   }
   void           (API_Control_SetControlStyleSheet)( control_handle, const char16_type* )
   {

     abort();
   }

   uint32         (API_Control_GetControlBackgroundColor)( const_control_handle )
   {

     abort();
   }
   api_bool API_Control_SetControlBackgroundColor(
	   control_handle handle,
	   uint32 argb )
   {
       LogDbg("API_Control_SetControlBackgroundColor called");

       MockControl* wdg = GetControlBox(handle);
       if (!wdg) return api_false;
       QWidget* w = wdg->widget;
       if (!w)
	   return api_false;

       // Convert ARGB → QColor
       QColor color(
	   (argb >> 16) & 0xFF,   // R
	   (argb >>  8) & 0xFF,   // G
	   (argb >>  0) & 0xFF,   // B
	   (argb >> 24) & 0xFF    // A
       );

       // Apply using Qt palette so it behaves like PixInsight
       QPalette pal = w->palette();
       pal.setColor(QPalette::Base, color);
       pal.setColor(QPalette::Window, color);
       w->setPalette(pal);
       w->setAutoFillBackground(true);

       return api_true;
   }

   uint32         (API_Control_GetControlForegroundColor)( const_control_handle )
   {

     abort();
   }
   void           (API_Control_SetControlForegroundColor)( control_handle, uint32 )
   {

     abort();
   }

   uint32         (API_Control_GetControlCanvasColor)( const_control_handle )
   {

     abort();
   }
   void           (API_Control_SetControlCanvasColor)( control_handle, uint32 )
   {

     abort();
   }

   uint32         (API_Control_GetControlAlternateCanvasColor)( const_control_handle )
   {

     abort();
   }
   void           (API_Control_SetControlAlternateCanvasColor)( control_handle, uint32 )
   {

     abort();
   }

   uint32         (API_Control_GetControlTextColor)( const_control_handle )
   {

     abort();
   }
   void           (API_Control_SetControlTextColor)( control_handle, uint32 )
   {

     abort();
   }

   uint32         (API_Control_GetControlButtonColor)( const_control_handle )
   {

     abort();
   }
   void           (API_Control_SetControlButtonColor)( control_handle, uint32 )
   {

     abort();
   }

   uint32         (API_Control_GetControlButtonTextColor)( const_control_handle )
   {

     abort();
   }
   void           (API_Control_SetControlButtonTextColor)( control_handle, uint32 )
   {

     abort();
   }

   uint32         (API_Control_GetControlHighlightColor)( const_control_handle )
   {

     abort();
   }
   void           (API_Control_SetControlHighlightColor)( control_handle, uint32 )
   {

     abort();
   }

   uint32         (API_Control_GetControlHighlightedTextColor)( const_control_handle )
   {

     abort();
   }
   void           (API_Control_SetControlHighlightedTextColor)( control_handle, uint32 )
   {

     abort();
   }

   void           (API_Control_SetControlFont)( control_handle, const_font_handle )
   {

     //     abort();
   }

   void           (API_Control_GetWindowOpacity)( const_control_handle, double* )
   {

     abort();
   }
   void           (API_Control_SetWindowOpacity)( control_handle, double )
   {

     abort();
   }

   api_bool       (API_Control_GetWindowTitle)( const_control_handle, char16_type*, size_type* )
   {

     abort();
   }
   void           (API_Control_SetWindowTitle)( control_handle, const char16_type* )
   {

     //     abort();
   }

   api_bool       (API_Control_GetInfoText)( const_control_handle, char16_type*, size_type* )
   {

     abort();
   }
   void           (API_Control_SetInfoText)( control_handle, const char16_type* )
   {

     abort();
   }

   api_bool       (API_Control_GetRealTimePreviewActive)( const_control_handle )
   {

     abort();
   }
   void           (API_Control_SetRealTimePreviewActive)( control_handle, api_bool )
   {

     abort();
   }

   api_bool       (API_Control_GetTrackViewActive)( const_control_handle )
   {

     abort();
   }
   void           (API_Control_SetTrackViewActive)( control_handle, api_bool )
   {

     abort();
   }

   api_bool       (API_Control_GetWindowToolTip)( const_control_handle, char16_type*, size_type* )
   {

     abort();
   }
   void           (API_Control_SetWindowToolTip)( control_handle, const char16_type* )
   {

     //     abort();
   }

   api_bool       (API_Control_GetControlDisplayPixelRatio)( const_control_handle, double* ratio)
   {
     *ratio = 1.0;
     return true;
   }

   api_bool       (API_Control_GetControlDevicePixelRatio)( const_control_handle, double* ratio)
   {
     *ratio = 1.0;
     return true;
   }

// ----------------------------------------------------------------------------
// DialogContext API
// ----------------------------------------------------------------------------

control_handle API_Dialog_CreateDialog(api_handle, api_handle client, control_handle parent, uint32 flags)
{

  abort();
}
int32 API_Dialog_ExecuteDialog(control_handle)
{

  abort();
}
void API_Dialog_OpenDialog(control_handle)
{

  abort();
}
void API_Dialog_ReturnDialog(control_handle, int32)
{

  abort();
}
api_bool API_Dialog_GetDialogResizable(const_control_handle)
{

  abort();
}
void API_Dialog_SetDialogResizable(control_handle, api_bool)
{

  abort();
}
api_bool API_Dialog_SetExecuteDialogEventRoutine(control_handle, api_handle, pcl::event_routine)
{

  abort();
}
api_bool API_Dialog_SetReturnDialogEventRoutine(control_handle, api_handle, pcl::value_event_routine)
{

  abort();
}
api_bool API_Dialog_ExecuteOpenFileDialog(char16_type* fileName, const char16_type* caption, const char16_type* initialPath, const char16_type* filters, const char16_type* selectedExtension)
{

  abort();
}
api_bool API_Dialog_ExecuteOpenMultipleFilesDialog(char16_type* fileName, ::file_enumeration_callback, void*, const char16_type* caption, const char16_type* initialPath, const char16_type* filters, const char16_type* selectedExtension)
{

  abort();
}
api_bool API_Dialog_ExecuteSaveFileDialog(char16_type* filePath, const char16_type* caption, const char16_type* initialPath, const char16_type* filters, const char16_type* selectedExtension, api_bool overwritePrompt)
{

  abort();
}
api_bool API_Dialog_ExecuteGetDirectoryDialog(char16_type* dirPath, const char16_type* caption, const char16_type* initialPath)
{

  abort();
}

// ----------------------------------------------------------------------------
// FrameContext API
// ----------------------------------------------------------------------------

control_handle API_Frame_CreateFrame(api_handle, api_handle client, control_handle parent, uint32 flags)
{

  abort();
}
int32 API_Frame_GetFrameStyle(const_control_handle)
{

  abort();
}
void API_Frame_SetFrameStyle(control_handle, int32)
{

  //  abort();
}
int32 API_Frame_GetFrameLineWidth(const_control_handle)
{

  abort();
}
void API_Frame_SetFrameLineWidth(control_handle, int32)
{

  //  abort();
}
int32 API_Frame_GetFrameBorderWidth(const_control_handle)
{

  abort();
}

// ----------------------------------------------------------------------------
// GroupBoxContext API
// ----------------------------------------------------------------------------

control_handle API_GroupBox_CreateGroupBox(
        api_handle /*handle*/,
        api_handle client,
        const char16_type* title,
        control_handle parent,
        uint32 flags)
{
    LogDbg("API_GroupBox_CreateGroupBox called");

    MockGroupBox* mg = new MockGroupBox();
    mg->clientHandle = client;

    QGroupBox* box = mg->box;

    if (title && *title) {
        QString q = QString::fromUtf16(reinterpret_cast<const ushort*>(title));
        box->setTitle(q);
    }

    // Flags may include: checkable, flat, etc.
    if (flags & 0x01)
        box->setCheckable(true);
    if (flags & 0x02)
        box->setFlat(true);

    if (parent) {
        QWidget* p = reinterpret_cast<QWidget*>(parent);
        box->setParent(p);
    }

    control_handle h = reinterpret_cast<control_handle>(box);

    {
        std::lock_guard<std::mutex> lock(g_groupbox_map_mutex);
        g_groupbox_map[h] = mg;
    }

    return h;
}

api_bool API_GroupBox_GetGroupBoxTitle(
        const_control_handle handle,
        char16_type* text,
        size_type* len)
{
    LogDbg("API_GroupBox_GetGroupBoxTitle called");

    std::lock_guard<std::mutex> lock(g_groupbox_map_mutex);
    auto it = g_groupbox_map.find(const_cast<control_handle>(handle));

    if (it == g_groupbox_map.end()) {
        if (len) *len = 0;
        return api_false;
    }

    QString title = it->second->box->title();
    std::u16string u16 = title.toStdU16String();

    if (!text) {
        if (len) *len = u16.length();
        return api_true;
    }

    if (!len || *len == 0)
        return api_false;

    size_type copyLen = std::min(*len - 1, (size_type)u16.length());
    memcpy(text, u16.c_str(), copyLen * sizeof(char16_type));
    text[copyLen] = 0;
    *len = copyLen;

    return api_true;
}
  
api_bool API_GroupBox_SetGroupBoxTitle(
        control_handle handle,
        const char16_type* title)
{
    LogDbg("API_GroupBox_SetGroupBoxTitle called");

    std::lock_guard<std::mutex> lock(g_groupbox_map_mutex);
    auto it = g_groupbox_map.find(handle);

    if (it == g_groupbox_map.end())
        return api_false;

    QGroupBox* box = it->second->box;

    QString q = QString::fromUtf16(reinterpret_cast<const ushort*>(title));
    box->setTitle(q);

    return api_true;
}
  
api_bool API_GroupBox_GetGroupBoxCheckable(const_control_handle)
{

  abort();
}
void API_GroupBox_SetGroupBoxCheckable(control_handle, api_bool)
{

  abort();
}
api_bool API_GroupBox_GetGroupBoxChecked(const_control_handle handle)
{
    LogDbg("API_GroupBox_GetGroupBoxChecked called");

    std::lock_guard<std::mutex> lock(g_groupbox_map_mutex);
    auto it = g_groupbox_map.find(const_cast<control_handle>(handle));

    if (it == g_groupbox_map.end())
        return api_false;

    return it->second->box->isChecked() ? api_true : api_false;
}

api_bool API_GroupBox_SetGroupBoxChecked(
        control_handle handle,
        api_bool checked)
{
    LogDbg("API_GroupBox_SetGroupBoxChecked called");

    std::lock_guard<std::mutex> lock(g_groupbox_map_mutex);
    auto it = g_groupbox_map.find(handle);
    if (it == g_groupbox_map.end()) return api_false;

    QGroupBox* box = it->second->box;

    if (!box->isCheckable())
        box->setCheckable(true);

    box->setChecked(checked != api_false);

    return api_true;
}  

   api_bool       (API_GroupBox_SetGroupBoxCheckEventRoutine)( control_handle, api_handle, pcl::button_check_event_routine )
   {

     abort();
   }

// ----------------------------------------------------------------------------
// TabBoxContext API
// ----------------------------------------------------------------------------

control_handle API_TabBox_CreateTabBox(api_handle, api_handle client, control_handle parent, uint32 flags)
{

  abort();
}
int32 API_TabBox_GetTabBoxLength(const_control_handle)
{

  abort();
}
int32 API_TabBox_GetTabBoxCurrentPageIndex(const_control_handle)
{

  abort();
}
void API_TabBox_SetTabBoxCurrentPageIndex(control_handle, int32)
{

  abort();
}
control_handle API_TabBox_GetTabBoxPageByIndex(const_control_handle, int32)
{
  // returns client handle
  abort();
}

   void           (API_TabBox_InsertTabBoxPage)( control_handle, int32, control_handle, const char16_type*, const_bitmap_handle )
   {

     abort();
   }

   void           (API_TabBox_RemoveTabBoxPage)( control_handle, int32 )
   {

     abort();
   }

   int32          (API_TabBox_GetTabBoxPosition)( const_control_handle )
   {
     // 0=top, 1=bottom
     abort();
   }
   void           (API_TabBox_SetTabBoxPosition)( control_handle, int32 )
   {
     //   idem.
     abort();
   }

   api_bool       (API_TabBox_GetTabBoxPageEnabled)( const_control_handle, int32 )
   {

     abort();
   }
   void           (API_TabBox_SetTabBoxPageEnabled)( control_handle, int32, api_bool )
   {

     abort();
   }

   api_bool       (API_TabBox_GetTabBoxPageLabel)( const_control_handle, int32, char16_type*, size_type* )
   {

     abort();
   }
   void           (API_TabBox_SetTabBoxPageLabel)( control_handle, int32, const char16_type* )
   {

     abort();
   }

   bitmap_handle  (API_TabBox_GetTabBoxPageIcon)( const_control_handle, int32 )
   {

     abort();
   }
   void           (API_TabBox_SetTabBoxPageIcon)( control_handle, int32, const_bitmap_handle )
   {

     abort();
   }

   api_bool       (API_TabBox_GetTabBoxPageToolTip)( const_control_handle, int32, char16_type*, size_type* )
   {

     abort();
   }
   void           (API_TabBox_SetTabBoxPageToolTip)( control_handle, int32, const char16_type* )
   {

     abort();
   }

   control_handle (API_TabBox_GetTabBoxLeftControl)( const_control_handle )
   {
     // returns client handle
     abort();
   }
   control_handle (API_TabBox_GetTabBoxRightControl)( const_control_handle )
   {
     // returns client handle
     abort();
   }
   void           (API_TabBox_SetTabBoxControls)( control_handle, control_handle, control_handle )
   {

     abort();
   }

   api_bool       (API_TabBox_SetTabBoxPageSelectedEventRoutine)( control_handle, api_handle, pcl::value_event_routine )
   {

     abort();
   }

// ----------------------------------------------------------------------------
// ButtonContext API
// ----------------------------------------------------------------------------

api_bool API_Button_GetButtonPushed(const_control_handle)
{
  // returns true if button pushed
  abort();
}
   void           (API_Button_SetButtonPushed)( control_handle, api_bool )
   {

     abort();
   }

   api_bool       (API_Button_GetButtonDefaultEnabled)( const_control_handle )
   {
     // returns true if button is default
     abort();
   }
   void           (API_Button_SetButtonDefaultEnabled)( control_handle, api_bool )
   {

     abort();
   }

   api_bool       (API_Button_GetButtonTristateEnabled)( const_control_handle )
   {
     // returns true if button is in tristate mode
     abort();
   }
   void           (API_Button_SetButtonTristateEnabled)( control_handle, api_bool )
   {

     abort();
   }

   api_bool       (API_Button_SetButtonPressEventRoutine)( control_handle, api_handle, pcl::event_routine )
   {

     abort();
   }
   api_bool       (API_Button_SetButtonReleaseEventRoutine)( control_handle, api_handle, pcl::event_routine )
   {

     abort();
   }
   api_bool       (API_Button_SetButtonCheckEventRoutine)( control_handle, api_handle, pcl::button_check_event_routine )
   {

     abort();
   }

// ----------------------------------------------------------------------------
// EditContext API
// ----------------------------------------------------------------------------

api_bool API_Edit_GetEditModified(const_control_handle)
{

  abort();
}
void API_Edit_SetEditModified(control_handle, api_bool)
{

  abort();
}
api_bool API_Edit_GetEditPasswordEnabled(const_control_handle)
{

  abort();
}
void API_Edit_SetEditPasswordEnabled(control_handle, api_bool)
{

  abort();
}
int32 API_Edit_GetEditMaxLength(const_control_handle)
{

  abort();
}
void API_Edit_SetEditMaxLength(control_handle, int32)
{

  abort();
}
api_bool API_Edit_GetEditMask(const_control_handle, char16_type*, size_type*)
{

  abort();
}
void API_Edit_SetEditMask(control_handle, const char16_type*)
{

  abort();
}
api_bool API_Edit_GetEditValidatingRegExp(const_control_handle, char16_type*, size_type*, api_bool* caseSensitive)
{

  abort();
}
api_bool API_Edit_SetEditValidatingRegExp(
        control_handle handle,
        const char16_type* regexp,
        api_bool caseSensitive )
{
    LogDbg("API_Edit_SetEditValidatingRegExp called");

    std::lock_guard<std::mutex> lock(g_edit_map_mutex);
    auto it = g_edit_map.find(handle);
    if (it == g_edit_map.end())
        return api_false;

    MockEdit* edit = it->second;
    QLineEdit* w = edit->edit;

    // Convert UTF-16 PixInsight regexp → QString
    QString pattern = QString::fromUtf16(reinterpret_cast<const ushort*>(regexp));

    // Delete old validator if present
    if (edit->validator) {
        delete edit->validator;
        edit->validator = nullptr;
    }

    // Compile regular expression
    QRegularExpression re(pattern);

    // Apply case sensitivity
    re.setPatternOptions(caseSensitive ? QRegularExpression::NoPatternOption
                                       : QRegularExpression::CaseInsensitiveOption);

    // Create new validator
    edit->validator = new QRegularExpressionValidator(re, w);

    // Install validator on the QLineEdit
    w->setValidator(edit->validator);

    return api_true;
}

api_bool API_Edit_GetEditValid(const_control_handle handle)
{
    LogDbg("API_Edit_GetEditValid called");

    std::lock_guard<std::mutex> lock(g_edit_map_mutex);
    auto it = g_edit_map.find(const_cast<control_handle>(handle));
    if (it == g_edit_map.end())
        return api_false;

    MockEdit* edit = it->second;
    QLineEdit* w = edit->edit;

    if (!w->validator())
        return api_true; // No validator → always valid

    return w->hasAcceptableInput() ? api_true : api_false;
}

void API_Edit_SetEditSelected(control_handle handle, api_bool selected)
{
    LogDbg("API_Edit_SetEditSelected called");

    std::lock_guard<std::mutex> lock(g_edit_map_mutex);
    auto it = g_edit_map.find(handle);
    if (it == g_edit_map.end())
        return;

    MockEdit* edit = it->second;
    QLineEdit* w = edit->edit;

    if (selected)
        w->selectAll();
    else
        w->deselect();
}

int32 API_Edit_GetEditAlignment(const_control_handle handle)
{
    LogDbg("API_Edit_GetEditAlignment called");

    std::lock_guard<std::mutex> lock(g_edit_map_mutex);
    auto it = g_edit_map.find(const_cast<control_handle>(handle));
    if (it == g_edit_map.end())
        return 0;

    MockEdit* edit = it->second;
    Qt::Alignment a = edit->edit->alignment();

    if (a & Qt::AlignRight)
        return 1; // right
    else
        return 0; // left (default)
}
  
void API_Edit_SetEditAlignment(control_handle handle, int32 align)
{
    LogDbg("API_Edit_SetEditAlignment called");

    std::lock_guard<std::mutex> lock(g_edit_map_mutex);
    auto it = g_edit_map.find(handle);
    if (it == g_edit_map.end())
        return;

    MockEdit* edit = it->second;
    QLineEdit* w = edit->edit;

    Qt::Alignment a = Qt::AlignVCenter;
    if (align == 1)
        a |= Qt::AlignRight;
    else
        a |= Qt::AlignLeft;

    w->setAlignment(a);
}

int32 API_Edit_GetEditCaretPosition(const_control_handle handle)
{
    LogDbg("API_Edit_GetEditCaretPosition called");

    std::lock_guard<std::mutex> lock(g_edit_map_mutex);
    auto it = g_edit_map.find(const_cast<control_handle>(handle));
    if (it == g_edit_map.end())
        return 0;

    MockEdit* edit = it->second;
    return edit->edit->cursorPosition();
}
  
void API_Edit_SetEditCaretPosition(control_handle handle, int32 pos)
{
    LogDbg("API_Edit_SetEditCaretPosition called");

    std::lock_guard<std::mutex> lock(g_edit_map_mutex);
    auto it = g_edit_map.find(handle);
    if (it == g_edit_map.end())
        return;

    MockEdit* edit = it->second;
    QLineEdit* w = edit->edit;

    if (pos < 0)
        pos = 0;
    if (pos > w->text().length())
        pos = w->text().length();

    w->setCursorPosition(pos);
}
void API_Edit_GetEditSelection(const_control_handle handle, int32* start, int32* end)
{
    LogDbg("API_Edit_GetEditSelection called");

    if (start) *start = 0;
    if (end)   *end   = 0;

    std::lock_guard<std::mutex> lock(g_edit_map_mutex);
    auto it = g_edit_map.find(const_cast<control_handle>(handle));
    if (it == g_edit_map.end())
        return;

    MockEdit* edit = it->second;
    QLineEdit* w = edit->edit;

    int s = w->selectionStart();
    if (s < 0)
        return;

    int len = w->selectedText().length();
    if (start) *start = s;
    if (end)   *end   = s + len;
} 

void API_Edit_SetEditSelection(control_handle handle, int32 start, int32 end)
{
    LogDbg("API_Edit_SetEditSelection called");

    std::lock_guard<std::mutex> lock(g_edit_map_mutex);
    auto it = g_edit_map.find(handle);
    if (it == g_edit_map.end())
        return;

    MockEdit* edit = it->second;
    QLineEdit* w = edit->edit;

    int len = w->text().length();
    if (start < 0) start = 0;
    if (end   < 0) end   = 0;
    if (start > len) start = len;
    if (end   > len) end   = len;

    int selLen = end - start;
    if (selLen <= 0) {
        w->deselect();
        return;
    }

    w->setSelection(start, selLen);
}
  
api_bool API_Edit_GetEditSelectedText(const_control_handle handle,
                                      char16_type* text,
                                      size_type* len)
{
    LogDbg("API_Edit_GetEditSelectedText called");

    std::lock_guard<std::mutex> lock(g_edit_map_mutex);
    auto it = g_edit_map.find(const_cast<control_handle>(handle));
    if (it == g_edit_map.end()) {
        if (len) *len = 0;
        return api_false;
    }

    MockEdit* edit = it->second;
    QString qsel = edit->edit->selectedText();
    std::u16string u16 = qsel.toStdU16String();

    if (!text) {
        if (len) *len = u16.length();
        return api_true;
    }

    if (!len || *len == 0) {
        return api_false;
    }

    size_type copyLen = std::min(*len - 1, (size_type)u16.length());
    std::memcpy(text, u16.c_str(), copyLen * sizeof(char16_type));
    text[copyLen] = 0;
    *len = copyLen;

    return api_true;
}

api_bool API_Edit_SetEditCompletedEventRoutine(
    control_handle handle,
    api_handle receiver,
    pcl::event_routine handler)
{
    LogDebug("API_Edit_SetEditCompletedEventRoutine called");

    std::lock_guard<std::mutex> lock(g_edit_map_mutex);
    auto it = g_edit_map.find(handle);
    if (it == g_edit_map.end())
        return api_false;

    MockEdit* mockEdit = it->second;

    // Store exactly what PCL told us:
    mockEdit->editCompletedReceiver = receiver; // pcl::Control* (NumericControl)
    mockEdit->editCompletedHandler  = handler;  // EditEventDispatcher::EditCompleted

    QObject::disconnect(mockEdit->edit, nullptr, nullptr, nullptr);

    QObject::connect(
        mockEdit->edit,
        &QLineEdit::editingFinished,
        [mockEdit]()
        {
            if (!mockEdit->editCompletedHandler) {
                LogDebug("editingFinished: no handler");
                return;
            }
            if (!mockEdit->pclEdit) {
                LogDebug("editingFinished: no pclEdit");
                return;
            }
            if (!mockEdit->editCompletedReceiver) {
                LogDebug("editingFinished: no receiver");
                return;
            }

            control_handle hSender =
                reinterpret_cast<control_handle>(mockEdit->pclEdit);              // pcl::Edit*
            control_handle hReceiver =
                reinterpret_cast<control_handle>(mockEdit->editCompletedReceiver); // pcl::Control*

            mockEdit->editCompletedHandler(hSender, hReceiver);
        });

    return api_true;
}

api_bool API_Edit_SetReturnPressedEventRoutine(
        control_handle handle,
        api_handle receiver,
        pcl::event_routine routine )
{
    LogDbg("API_Edit_SetReturnPressedEventRoutine called");

    std::lock_guard<std::mutex> lock(g_edit_map_mutex);
    auto it = g_edit_map.find(handle);
    if (it == g_edit_map.end()) return api_false;

    MockEdit* mockEdit = it->second;
    mockEdit->returnPressedHandler  = routine;
    mockEdit->returnPressedReceiver = receiver;

    QLineEdit* w = mockEdit->edit;
    // Do NOT disconnect all events; combine them
    if (routine)
        QObject::connect(w, &QLineEdit::returnPressed,
            [mockEdit]() {
                if (mockEdit->pclEdit && mockEdit->returnPressedHandler && mockEdit->editCompletedReceiver) {
		    control_handle hSender =
			reinterpret_cast<control_handle>(mockEdit->pclEdit);              // pcl::Edit*
		    control_handle hReceiver =
			reinterpret_cast<control_handle>(mockEdit->editCompletedReceiver); // pcl::Control*
                    mockEdit->returnPressedHandler(hSender, hReceiver);
                }
            });

    return api_true;
}

api_bool API_Edit_SetTextUpdatedEventRoutine(
        control_handle handle,
        api_handle receiver,
        pcl::unicode_event_routine routine )
{
    LogDbg("API_Edit_SetTextUpdatedEventRoutine called");

    std::lock_guard<std::mutex> lock(g_edit_map_mutex);
    auto it = g_edit_map.find(handle);
    if (it == g_edit_map.end()) return api_false;

    MockEdit* edit = it->second;
    edit->textUpdatedHandler  = routine;
    edit->textUpdatedReceiver = receiver;

    QLineEdit* w = edit->edit;

    if (routine)
        QObject::connect(w, &QLineEdit::textChanged,
            [edit, w](const QString& qs) {
                if (edit->textUpdatedHandler && edit->textUpdatedReceiver) {
                    control_handle h = reinterpret_cast<control_handle>(w);
                    std::u16string u16 = qs.toStdU16String();
                    edit->textUpdatedHandler(reinterpret_cast<api_handle>(g_activeInterface),
                                             h,
                                             reinterpret_cast<const char16_type*>(u16.c_str()));
                }
            });

    return api_true;
}

api_bool API_Edit_SetCaretPositionUpdatedEventRoutine(
        control_handle handle,
        api_handle receiver,
        pcl::range_event_routine routine )
{
    LogDbg("API_Edit_SetCaretPositionUpdatedEventRoutine called");

    std::lock_guard<std::mutex> lock(g_edit_map_mutex);
    auto it = g_edit_map.find(handle);
    if (it == g_edit_map.end()) return api_false;

    MockEdit* edit = it->second;
    edit->caretPositionUpdatedHandler  = routine;
    edit->caretPositionUpdatedReceiver = receiver;

    QLineEdit* w = edit->edit;

    if (routine)
        QObject::connect(w, &QLineEdit::cursorPositionChanged,
            [edit, w](int oldPos, int newPos) {
                if (edit->caretPositionUpdatedHandler && edit->caretPositionUpdatedReceiver) {
                    control_handle h = reinterpret_cast<control_handle>(w);
                    edit->caretPositionUpdatedHandler(reinterpret_cast<api_handle>(g_activeInterface),
                                                      h,
                                                      oldPos,
                                                      newPos);
                }
            });

    return api_true;
}

api_bool API_Edit_SetSelectionUpdatedEventRoutine(
        control_handle handle,
        api_handle receiver,
        pcl::range_event_routine routine )
{
    LogDbg("API_Edit_SetSelectionUpdatedEventRoutine called");

    std::lock_guard<std::mutex> lock(g_edit_map_mutex);
    auto it = g_edit_map.find(handle);
    if (it == g_edit_map.end()) return api_false;

    MockEdit* edit = it->second;
    edit->selectionUpdatedHandler  = routine;
    edit->selectionUpdatedReceiver = receiver;

    QLineEdit* w = edit->edit;

    if (routine)
        QObject::connect(w, &QLineEdit::selectionChanged,
            [edit, w]() {
                if (edit->selectionUpdatedHandler && edit->selectionUpdatedReceiver) {
                    control_handle h = reinterpret_cast<control_handle>(w);
                    int start = w->selectionStart();
                    int len   = w->selectedText().length();
                    edit->selectionUpdatedHandler(reinterpret_cast<api_handle>(g_activeInterface),
                                                  h,
                                                  start,
                                                  start + len);
                }
            });

    return api_true;
}
  
// ----------------------------------------------------------------------------
// TextBoxContext API
// ----------------------------------------------------------------------------

api_bool API_TextBox_GetTextBoxReadOnly(const_control_handle)
{

  abort();
}
void API_TextBox_SetTextBoxReadOnly(control_handle, api_bool)
{

  abort();
}
void API_TextBox_SetTextBoxSelected(control_handle, api_bool)
{

  abort();
}
int32 API_TextBox_GetTextBoxCaretPosition(const_control_handle)
{

  abort();
}
void API_TextBox_SetTextBoxCaretPosition(control_handle, int32)
{

  abort();
}
void API_TextBox_GetTextBoxSelection(const_control_handle, int32*, int32*)
{

  abort();
}
void API_TextBox_SetTextBoxSelection(control_handle, int32, int32)
{

  abort();
}
api_bool API_TextBox_GetTextBoxSelectedText(const_control_handle, char16_type*, size_type*)
{

  abort();
}
void API_TextBox_InsertTextBoxText(control_handle, const char16_type*)
{

  abort();
}
void API_TextBox_DeleteTextBoxText(control_handle)
{

  abort();
}
api_bool API_TextBox_SetTextBoxUpdatedEventRoutine(control_handle, api_handle, pcl::unicode_event_routine)
{

  abort();
}
api_bool API_TextBox_SetTextBoxCaretPositionUpdatedEventRoutine(control_handle, api_handle, pcl::range_event_routine)
{

  abort();
}
api_bool API_TextBox_SetTextBoxSelectionUpdatedEventRoutine(control_handle, api_handle, pcl::range_event_routine)
{

  abort();
}

// ----------------------------------------------------------------------------
// ComboBoxContext API
// ----------------------------------------------------------------------------

int32 API_ComboBox_FindComboBoxItem(const_control_handle, const char16_type*, int32, api_bool exactMatch, api_bool caseSensitive)
{

  abort();
}

void API_ComboBox_RemoveComboBoxItem(control_handle, int32)
{

  abort();
}

api_bool API_ComboBox_GetComboBoxItemText(const_control_handle, int32, char16_type*, size_type*)
{

  abort();
}
void API_ComboBox_SetComboBoxItemText(control_handle, int32, const char16_type*)
{

  abort();
}
bitmap_handle API_ComboBox_GetComboBoxItemIcon(const_control_handle, int32)
{

  abort();
}
void API_ComboBox_SetComboBoxItemIcon(control_handle, int32, const_bitmap_handle)
{

  abort();
}
api_bool API_ComboBox_GetComboBoxEditEnabled(const_control_handle)
{

  abort();
}
void API_ComboBox_SetComboBoxEditEnabled(control_handle, api_bool)
{

  abort();
}
api_bool API_ComboBox_GetComboBoxEditText(const_control_handle, char16_type*, size_type*)
{

  abort();
}
void API_ComboBox_SetComboBoxEditText(control_handle, const char16_type*)
{

  abort();
}
api_bool API_ComboBox_GetComboBoxAutoCompletionEnabled(const_control_handle)
{

  abort();
}
void API_ComboBox_SetComboBoxAutoCompletionEnabled(control_handle, api_bool)
{

  abort();
}
void API_ComboBox_GetComboBoxIconSize(const_control_handle, int32*, int32*)
{

  abort();
}
void API_ComboBox_SetComboBoxIconSize(control_handle, int32, int32)
{

  abort();
}
int32 API_ComboBox_GetComboBoxMaxVisibleItemCount(const_control_handle)
{

  abort();
}
void API_ComboBox_SetComboBoxMaxVisibleItemCount(control_handle, int32)
{

  abort();
}
int32 API_ComboBox_GetComboBoxMinItemCharWidth(const_control_handle)
{

  abort();
}
void API_ComboBox_SetComboBoxMinItemCharWidth(control_handle, int32)
{

  abort();
}
void API_ComboBox_SetComboBoxListVisible(control_handle, api_bool)
{

  abort();
}
api_bool API_ComboBox_SetComboBoxItemSelectedEventRoutine(control_handle, api_handle, pcl::value_event_routine)
{

  //  abort();
  return api_true;
}
api_bool API_ComboBox_SetComboBoxItemHighlightedEventRoutine(control_handle, api_handle, pcl::value_event_routine)
{

  abort();
}
api_bool API_ComboBox_SetComboBoxEditTextUpdatedEventRoutine(control_handle, api_handle, pcl::event_routine)
{

  abort();
}

// ----------------------------------------------------------------------------
// SliderContext API
// ----------------------------------------------------------------------------

int32 API_Slider_GetSliderStepSize(const_control_handle)
{

  abort();
}
void API_Slider_SetSliderStepSize(control_handle, int32)
{

  //  abort();
}
int32 API_Slider_GetSliderPageSize(const_control_handle)
{

  abort();
}
void API_Slider_SetSliderPageSize(control_handle, int32)
{

  //  abort();
}
int32 API_Slider_GetSliderTickInterval(const_control_handle)
{

  abort();
}
void API_Slider_SetSliderTickInterval(control_handle, int32)
{

  //  abort();
}
int32 API_Slider_GetSliderTickStyle(const_control_handle)
{

  abort();
}
void API_Slider_SetSliderTickStyle(control_handle, int32)
{

  //  abort();
}
api_bool API_Slider_GetSliderTrackingEnabled(const_control_handle)
{

  abort();
}
void API_Slider_SetSliderTrackingEnabled(control_handle, api_bool)
{

  abort();
}

api_bool API_Slider_SetSliderValueUpdatedEventRoutine(
        control_handle handle,
        api_handle receiver,
        pcl::value_event_routine routine )
{
    LogDbg("API_Slider_SetSliderValueUpdatedEventRoutine called");

    if (!handle)
        return api_false;
    /*
    std::lock_guard<std::mutex> lock(g_slider_map_mutex);

    MockSlider* sliderCtrl = nullptr;
    auto it = g_slider_map.find(handle);

    if (it == g_slider_map.end()) {
        // Slider wasn't created with API_Slider_CreateSlider.
        // Try to treat the handle as a QSlider*.
        QWidget* w = reinterpret_cast<QWidget*>(handle);
        QSlider* slider = qobject_cast<QSlider*>(w);
        if (!slider) {
            LogDbg("ValueUpdatedEventRoutine: handle is not a QSlider");
            return api_false;
        }

        sliderCtrl = new MockSlider(false); // orientation doesn't matter
        sliderCtrl->slider = slider;
        sliderCtrl->clientHandle = receiver;

        g_slider_map[handle] = sliderCtrl;
        LogDbg("ValueUpdatedEventRoutine: created new MockSlider entry");
    }
    else {
        sliderCtrl = it->second;
    }

    // Store the handler
    sliderCtrl->clientHandle = receiver;

    // Hook Qt → PCL callback
    QSlider* slider = sliderCtrl->slider;

    QObject::connect(slider, &QSlider::valueChanged,
                     [sliderCtrl, slider, routine]() {
        if (routine && sliderCtrl->clientHandle) {
            control_handle h = reinterpret_cast<control_handle>(slider);
            routine(sliderCtrl->clientHandle, h, slider->value());
        }
    });
    */
    return api_true;
}
  
api_bool API_Slider_SetSliderRangeUpdatedEventRoutine(control_handle, api_handle, pcl::range_event_routine)
{

  abort();
}

// ----------------------------------------------------------------------------
// SpinBoxContext API
// ----------------------------------------------------------------------------


api_bool API_SpinBox_GetSpinBoxWrappingEnabled(const_control_handle)
{

  abort();
}
void API_SpinBox_SetSpinBoxWrappingEnabled(control_handle, api_bool)
{

  abort();
}
api_bool API_SpinBox_GetSpinBoxEditable(const_control_handle)
{

  abort();
}
void API_SpinBox_SetSpinBoxEditable(control_handle, api_bool)
{

  abort();
}

api_bool API_SpinBox_GetSpinBoxMinimumValueText(const_control_handle, char16_type*, size_type*)
{

  abort();
}
void API_SpinBox_SetSpinBoxMinimumValueText(control_handle, const char16_type*)
{

  abort();
}
int32 API_SpinBox_GetSpinBoxAlignment(const_control_handle)
{
  // only left and right alignments
  abort();
}
   void           (API_SpinBox_SetSpinBoxAlignment)( control_handle, int32 )
   {
     //    idem.
     abort();
   }

   api_bool       (API_SpinBox_SetSpinBoxRangeUpdatedEventRoutine)( control_handle, api_handle, pcl::range_event_routine )
   {

     abort();
   }

// ----------------------------------------------------------------------------
// LabelContext API
// ----------------------------------------------------------------------------

int32 API_Label_GetLabelAlignment(const_control_handle)
{

  abort();
}
void API_Label_SetLabelAlignment(control_handle handle, int32 alignment)
{
  API_Label_SetLabelTextAlignment(handle, alignment);
}

api_bool API_Label_GetLabelWordWrappingEnabled(const_control_handle)
{

  abort();
}
void API_Label_SetLabelWordWrappingEnabled(control_handle, api_bool)
{

  abort();
}
api_bool API_Label_GetLabelRichTextEnabled(const_control_handle)
{

  abort();
}
void API_Label_SetLabelRichTextEnabled(control_handle, api_bool)
{

  abort();
}

// ----------------------------------------------------------------------------
// BitmapBoxContext API
// ----------------------------------------------------------------------------

control_handle API_BitmapBox_CreateBitmapBox(api_handle, api_handle client, const_bitmap_handle, control_handle parent, uint32 flags)
{

  abort();
}
bitmap_handle API_BitmapBox_GetBitmapBoxBitmap(const_control_handle)
{

  abort();
}
void API_BitmapBox_SetBitmapBoxBitmap(control_handle, const_bitmap_handle)
{

  abort();
}
int32 API_BitmapBox_GetBitmapBoxMargin(const_control_handle)
{

  abort();
}
void API_BitmapBox_SetBitmapBoxMargin(control_handle, int32)
{

  abort();
}
api_bool API_BitmapBox_GetBitmapBoxAutoFitEnabled(const_control_handle)
{

  abort();
}
void API_BitmapBox_SetBitmapBoxAutoFitEnabled(control_handle, api_bool)
{

  abort();
}

// ----------------------------------------------------------------------------
// ScrollBoxContext API
// ----------------------------------------------------------------------------

control_handle API_ScrollBox_CreateScrollBox(
        api_handle api,
        api_handle client,
        control_handle parent,
        uint32 flags )
{
    LogDebug("CreateScrollBox called");

    QWidget* parentWidget =
        parent ? reinterpret_cast<QWidget*>(parent) : nullptr;

    // QScrollArea is the Qt equivalent of a PixInsight ScrollBox
    QScrollArea* scroll = new QScrollArea(parentWidget);

    // PCL-style behaviour:
    // - widgetResizable = true allows automatic sizing of contents
    scroll->setWidgetResizable(true);

    // Scrollbars appear as needed
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // Register in MockControl system
    control_handle handle = reinterpret_cast<control_handle>(scroll);
    MockControl* mc = GetOrCreateMockControl(handle);

    // Set owner API/client handles (if your framework uses them)
    //    mc->apiHandle    = api;
    mc->clientHandle = client;

    LogDebug("ScrollBox created: handle=" +
             std::to_string(reinterpret_cast<std::uintptr_t>(handle)));

    return handle;
}

control_handle API_ScrollBox_CreateScrollBoxViewport(
        control_handle scrollBoxHandle,
        api_handle client )
{
    LogDebug("CreateScrollBoxViewport called");

    if (!scrollBoxHandle) {
        LogDebug("CreateScrollBoxViewport: null scroll box");
        return nullptr;
    }

    QScrollArea* scroll =
        reinterpret_cast<QScrollArea*>(scrollBoxHandle);

    // Create viewport widget — this is the content widget inside the scrollbox.
    QWidget* viewport = new QWidget(scroll);
    viewport->setObjectName("ScrollBoxViewport");

    // This is important: assign it as the scrollbox’s widget.
    scroll->setWidget(viewport);

    // Register with mock control system
    control_handle viewportHandle =
        reinterpret_cast<control_handle>(viewport);

    MockControl* mc = GetOrCreateMockControl(viewportHandle);
    mc->clientHandle = client;

    LogDebug("ScrollBox viewport created: handle=" +
             std::to_string(reinterpret_cast<std::uintptr_t>(viewportHandle)));

    return viewportHandle;
}

void API_ScrollBox_GetScrollBarsVisible(const_control_handle, api_bool*, api_bool*)
{

  abort();
}
void API_ScrollBox_SetScrollBarsVisible(control_handle, api_bool, api_bool)
{

  abort();
}
void API_ScrollBox_GetScrollBoxAutoScrollEnabled(const_control_handle, api_bool*, api_bool*)
{

  //   abort();
}
void API_ScrollBox_SetScrollBoxAutoScrollEnabled(control_handle, api_bool, api_bool)
{

  //  abort();
}
void API_ScrollBox_GetScrollBoxHorizontalRange(const_control_handle, int32*, int32*)
{

  abort();
}
void API_ScrollBox_SetScrollBoxHorizontalRange(control_handle, int32, int32)
{

  abort();
}
void API_ScrollBox_GetScrollBoxVerticalRange(const_control_handle, int32*, int32*)
{

  abort();
}
void API_ScrollBox_SetScrollBoxVerticalRange(control_handle, int32, int32)
{

  abort();
}
void API_ScrollBox_GetScrollBoxPageSize(const_control_handle, int32*, int32*)
{

  abort();
}
void API_ScrollBox_SetScrollBoxPageSize(control_handle, int32, int32)
{

  abort();
}
void API_ScrollBox_GetScrollBoxLineSize(const_control_handle, int32*, int32*)
{

  abort();
}
void API_ScrollBox_SetScrollBoxLineSize(control_handle, int32, int32)
{

  abort();
}
void API_ScrollBox_GetScrollBoxPosition(const_control_handle, int32*, int32*)
{

  abort();
}
void API_ScrollBox_SetScrollBoxPosition(control_handle, int32, int32)
{

  abort();
}
void API_ScrollBox_GetScrollBoxTrackingEnabled(const_control_handle, api_bool*, api_bool*)
{

  abort();
}
void API_ScrollBox_SetScrollBoxTrackingEnabled(control_handle, api_bool, api_bool)
{

  abort();
}

api_bool API_ScrollBox_SetScrollBoxHorizontalPosUpdatedEventRoutine(control_handle, api_handle, pcl::value_event_routine)
{

  //  abort();
  return api_true;
}

api_bool API_ScrollBox_SetScrollBoxVerticalPosUpdatedEventRoutine(control_handle, api_handle, pcl::value_event_routine)
{

  //  abort();
  return api_true;
}

api_bool API_ScrollBox_SetScrollBoxHorizontalRangeUpdatedEventRoutine(control_handle, api_handle, pcl::range_event_routine)
{

  abort();
}
api_bool API_ScrollBox_SetScrollBoxVerticalRangeUpdatedEventRoutine(control_handle, api_handle, pcl::range_event_routine)
{

  abort();
}

// ----------------------------------------------------------------------------
// TreeBoxContext API
// ----------------------------------------------------------------------------

control_handle (API_TreeBox_CreateTreeBox)( api_handle module,
                                            api_handle client,
                                            control_handle parent,
                                            uint32 /*flags*/ )
{
    Q_UNUSED(module);
    LogDbg("CreateTreeBox called");

    auto* tree = new QTreeWidget;
    if (parent)
        tree->setParent(reinterpret_cast<QWidget*>(parent));

    // Sensible defaults
    tree->setColumnCount(1);
    tree->setHeaderHidden(false);

    auto* mock = new MockTreeBox;
    mock->widget       = tree;
    mock->tree         = tree;
    mock->clientHandle = client;
    mock->layout       = nullptr;

    control_handle handle = reinterpret_cast<control_handle>(tree);

    {
        std::lock_guard<std::mutex> lock(g_treebox_mutex);
        g_treebox_map[handle] = mock;
    }
    {
        std::lock_guard<std::mutex> lock(g_control_map_mutex);
        g_control_map[handle] = mock;
    }

    return handle;
}

control_handle (API_TreeBox_CreateTreeBoxViewport)( control_handle hTree,
                                                    api_handle    /*client*/ )
{
    LogDbg("CreateTreeBoxViewport called");

    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
        return nullptr;

    QWidget* viewport = mock->tree->viewport();
    return reinterpret_cast<control_handle>(viewport);
}

api_handle API_TreeBox_CreateTreeBoxNode(api_handle, api_handle nodeClient)
{
    // In the PCL API, nodeClient is already the pcl::TreeBox::Node*.
    // We just use that as the handle.
    LogDbg("CreateTreeBoxNode called");
    return nodeClient;
}

int32 API_TreeBox_GetTreeBoxChildCount(const_control_handle hTree)
{
    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
        return 0;

    return mock->tree->topLevelItemCount();
}

api_handle API_TreeBox_GetTreeBoxChild(const_control_handle hTree, int32 idx)
{
    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
        return nullptr;

    QTreeWidget* tree = mock->tree;
    if (idx < 0 || idx >= tree->topLevelItemCount())
        return nullptr;

    QTreeWidgetItem* item = tree->topLevelItem(idx);
    if (!item)
        return nullptr;

    pcl::TreeBox::Node* node = NodeFromItem(item);
    return reinterpret_cast<api_handle>(node);
}

int32 (API_TreeBox_GetTreeBoxChildIndex)( const_control_handle hTree,
                                           const_api_handle    hNode )
{
    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
        return -1;

    QTreeWidgetItem* item = ItemFromNodeHandle(hNode);
    if (!item)
        return -1;

    QTreeWidget* tree = mock->tree;
    QTreeWidgetItem* parent = item->parent();

    if (!parent)
    {
        int idx = tree->indexOfTopLevelItem(item);
        return idx;
    }

    return parent->indexOfChild(item);
}

void (API_TreeBox_InsertTreeBoxNode)( control_handle hTree,
                                      int32          index,
                                      api_handle     hNode )
{
    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
        return;

    QTreeWidget* tree = mock->tree;
    auto* node = reinterpret_cast<pcl::TreeBox::Node*>(hNode);

    auto* item = new QTreeWidgetItem;
    if (index < 0 || index > tree->topLevelItemCount())
        tree->addTopLevelItem(item);
    else
        tree->insertTopLevelItem(index, item);

    {
        std::lock_guard<std::mutex> lock(g_treebox_mutex);
        g_node_to_item[node] = item;
        g_item_to_node[item] = node;
    }
}

void (API_TreeBox_RemoveTreeBoxNode)( control_handle hTree, int32 index )
{
    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
        return;

    QTreeWidget* tree = mock->tree;
    if (index < 0 || index >= tree->topLevelItemCount())
        return;

    QTreeWidgetItem* item = tree->takeTopLevelItem(index);
    if (!item)
        return;

    {
        std::lock_guard<std::mutex> lock(g_treebox_mutex);
        RemoveItemSubtreeFromMaps(item);
    }

    delete item;
}

void (API_TreeBox_ClearTreeBox)( control_handle hTree )
{
    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
        return;

    QTreeWidget* tree = mock->tree;

    {
        std::lock_guard<std::mutex> lock(g_treebox_mutex);

        // Remove all items belonging to this tree from the maps.
        const int topCount = tree->topLevelItemCount();
        for (int i = 0; i < topCount; ++i)
            RemoveItemSubtreeFromMaps(tree->topLevelItem(i));
    }

    tree->clear();
}

api_handle API_TreeBox_GetTreeBoxCurrentNode(const_control_handle hTree)
{
    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
        return nullptr;

    QTreeWidgetItem* item = mock->tree->currentItem();
    if (!item)
        return nullptr;

    pcl::TreeBox::Node* node = NodeFromItem(item);
    return reinterpret_cast<api_handle>(node);
}

void (API_TreeBox_SetTreeBoxCurrentNode)( control_handle hTree,
                                          api_handle     hNode )
{
    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
        return;

    QTreeWidgetItem* item = ItemFromNodeHandle(hNode);
    if (!item)
        return;

    mock->tree->setCurrentItem(item);
}

api_bool (API_TreeBox_GetTreeBoxMultipleNodeSelectionEnabled)(
    const_control_handle hTree )
{
    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
        return api_false;

    QAbstractItemView::SelectionMode mode = mock->tree->selectionMode();
    return (mode == QAbstractItemView::ExtendedSelection ||
            mode == QAbstractItemView::MultiSelection)
               ? api_true
               : api_false;
}

void (API_TreeBox_SetTreeBoxMultipleNodeSelectionEnabled)(
    control_handle hTree, api_bool enabled )
{
    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
        return;

    mock->multipleSelection = (enabled != 0);

    mock->tree->setSelectionMode(
        enabled ? QAbstractItemView::ExtendedSelection
                : QAbstractItemView::SingleSelection);
}

api_bool (API_TreeBox_GetTreeBoxSelectedNodes)( const_control_handle hTree,
                                                api_handle*          outNodes,
                                                size_type*           ioLen )
{
    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
    {
        if (ioLen)
            *ioLen = 0;
        return api_false;
    }

    QList<QTreeWidgetItem*> selItems = mock->tree->selectedItems();
    size_type count = selItems.size();

    if (!ioLen)
        return api_false;

    if (!outNodes)
    {
        *ioLen = count;
        return api_true;
    }

    if (*ioLen < count)
    {
        *ioLen = count;
        return api_false;
    }

    size_type i = 0;
    for (QTreeWidgetItem* item : selItems)
    {
        pcl::TreeBox::Node* node = NodeFromItem(item);
        outNodes[i++] = reinterpret_cast<api_handle>(node);
    }

    *ioLen = count;
    return api_true;
}

api_handle (API_TreeBox_GetTreeBoxNodeByPos)( const_control_handle hTree,
                                              int32 x, int32 y )
{
    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
        return nullptr;

    QTreeWidget* tree = mock->tree;
    QTreeWidgetItem* item = tree->itemAt(x, y);
    if (!item)
        return nullptr;

    pcl::TreeBox::Node* node = NodeFromItem(item);
    return reinterpret_cast<api_handle>(node);
}

void (API_TreeBox_SetTreeBoxNodeIntoView)( control_handle hTree,
                                           api_handle     hNode )
{
    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
        return;

    QTreeWidgetItem* item = ItemFromNodeHandle(hNode);
    if (!item)
        return;

    mock->tree->scrollToItem(item);
}

void (API_TreeBox_GetTreeBoxNodeRect)( const_control_handle hTree,
                                       const_api_handle     hNode,
                                       int32* x, int32* y,
                                       int32* w, int32* h )
{
    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
        return;

    QTreeWidgetItem* item = ItemFromNodeHandle(hNode);
    if (!item)
        return;

    QRect r = mock->tree->visualItemRect(item);
    if (x) *x = r.x();
    if (y) *y = r.y();
    if (w) *w = r.width();
    if (h) *h = r.height();
}

int32 (API_TreeBox_GetTreeBoxColumnCount)( const_control_handle hTree )
{
    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
        return 0;
    return mock->tree->columnCount();
}

void (API_TreeBox_SetTreeBoxColumnCount)( control_handle hTree, int32 n )
{
    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
        return;
    if (n < 1)
        n = 1;
    mock->tree->setColumnCount(n);
}

api_bool (API_TreeBox_GetTreeBoxColumnVisible)( const_control_handle hTree,
                                                int32 col )
{
    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
        return api_false;

    return mock->tree->isColumnHidden(col) ? api_false : api_true;
}

void (API_TreeBox_SetTreeBoxColumnVisible)( control_handle hTree,
                                            int32 col, api_bool vis )
{
    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
        return;

    mock->tree->setColumnHidden(col, vis ? false : true);
}

int32 (API_TreeBox_GetTreeBoxColumnWidth)( const_control_handle hTree,
                                           int32 col )
{
    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
        return 0;

    return mock->tree->columnWidth(col);
}

void (API_TreeBox_SetTreeBoxColumnWidth)( control_handle hTree,
                                          int32 col, int32 width )
{
    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
        return;

    mock->tree->setColumnWidth(col, width);
}

void (API_TreeBox_AdjustTreeBoxColumnWidthToContents)( control_handle hTree,
                                                       int32 col )
{
    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
        return;

    mock->tree->resizeColumnToContents(col);
}

// For now: we just store header text in the QTreeWidgetItem header.
// You can add proper UTF-16 conversion if/when needed.
api_bool (API_TreeBox_GetTreeBoxHeaderText)( const_control_handle hTree,
                                             int32 /*col*/,
                                             char16_type* text,
                                             size_type*   len )
{
    if (len)
        *len = 0;
    if (text)
        *text = 0;
    // Not used in your tests yet; returning false is fine.
    return api_false;
}

void (API_TreeBox_SetTreeBoxHeaderText)( control_handle hTree,
                                         int32 col,
                                         const char16_type* text )
{
    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
        return;

    QString qtext = QString::fromUtf16(
        reinterpret_cast<const ushort*>(text));
    QTreeWidgetItem* headerItem = mock->tree->headerItem();
    if (!headerItem)
    {
        headerItem = new QTreeWidgetItem;
        mock->tree->setHeaderItem(headerItem);
    }
    headerItem->setText(col, qtext);
}

bitmap_handle (API_TreeBox_GetTreeBoxHeaderIcon)( const_control_handle,
                                                  int32 )
{
    return nullptr;
}

void (API_TreeBox_SetTreeBoxHeaderIcon)( control_handle,
                                         int32, const_bitmap_handle )
{
    // Not needed for tests; no-op.
}

int32 (API_TreeBox_GetTreeBoxHeaderAlignment)( const_control_handle,
                                               int32 )
{
    return 0;
}

void (API_TreeBox_SetTreeBoxHeaderAlignment)( control_handle,
                                              int32, int32 )
{
    // No-op
}

api_bool (API_TreeBox_GetTreeBoxHeaderVisible)( const_control_handle hTree )
{
    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
        return api_false;

    return mock->tree->header()->isHidden() ? api_false : api_true;
}

void (API_TreeBox_SetTreeBoxHeaderVisible)( control_handle hTree,
                                            api_bool vis )
{
    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
        return;

    mock->tree->header()->setHidden(vis ? false : true);
}

int32 (API_TreeBox_GetTreeBoxIndentSize)( const_control_handle hTree )
{
    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
        return 0;
    return mock->tree->indentation();
}

void (API_TreeBox_SetTreeBoxIndentSize)( control_handle hTree, int32 size )
{
    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
        return;
    mock->tree->setIndentation(size);
}

api_bool (API_TreeBox_GetTreeBoxHeaderSortingEnabled)(
    const_control_handle hTree )
{
    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
        return api_false;

    return mock->tree->isSortingEnabled() ? api_true : api_false;
}

void (API_TreeBox_SetTreeBoxHeaderSortingEnabled)(
    control_handle hTree, api_bool enabled )
{
    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
        return;

    mock->tree->setSortingEnabled(enabled != 0);
}

void (API_TreeBox_SortTreeBox)( control_handle hTree,
                                int32 col, api_bool ascending )
{
    auto* mock = GetMockTreeBox(hTree);
    if (!mock || !mock->tree)
        return;

    mock->tree->sortItems(col,
        ascending ? Qt::AscendingOrder : Qt::DescendingOrder);
}

control_handle (API_TreeBox_GetTreeBoxNodeParentBox)( const_api_handle hNode )
{
    QTreeWidgetItem* item = ItemFromNodeHandle(hNode);
    if (!item)
        return nullptr;

    QTreeWidget* tree = item->treeWidget();
    return reinterpret_cast<control_handle>(tree);
}

api_handle (API_TreeBox_GetTreeBoxNodeParent)( const_api_handle hNode )
{
    QTreeWidgetItem* item = ItemFromNodeHandle(hNode);
    if (!item)
        return nullptr;

    QTreeWidgetItem* parent = item->parent();
    if (!parent)
        return nullptr;

    pcl::TreeBox::Node* node = NodeFromItem(parent);
    return reinterpret_cast<api_handle>(node);
}

int32 (API_TreeBox_GetTreeBoxNodeChildCount)( const_api_handle hNode )
{
    QTreeWidgetItem* item = ItemFromNodeHandle(hNode);
    if (!item)
        return 0;

    return item->childCount();
}

api_handle (API_TreeBox_GetTreeBoxNodeChild)( const_api_handle hNode,
                                              int32            idx )
{
    QTreeWidgetItem* item = ItemFromNodeHandle(hNode);
    if (!item)
        return nullptr;

    if (idx < 0 || idx >= item->childCount())
        return nullptr;

    QTreeWidgetItem* child = item->child(idx);
    if (!child)
        return nullptr;

    pcl::TreeBox::Node* node = NodeFromItem(child);
    return reinterpret_cast<api_handle>(node);
}

void (API_TreeBox_InsertTreeBoxNodeChild)( api_handle hParentNode,
                                           int32      idx,
                                           api_handle hChildNode )
{
    QTreeWidgetItem* parentItem = ItemFromNodeHandle(hParentNode);
    if (!parentItem)
        return;

    auto* childNode = reinterpret_cast<pcl::TreeBox::Node*>(hChildNode);
    auto* childItem = new QTreeWidgetItem;

    if (idx < 0 || idx > parentItem->childCount())
        parentItem->addChild(childItem);
    else
        parentItem->insertChild(idx, childItem);

    std::lock_guard<std::mutex> lock(g_treebox_mutex);
    g_node_to_item[childNode] = childItem;
    g_item_to_node[childItem] = childNode;
}

void (API_TreeBox_RemoveTreeBoxNodeChild)( api_handle hParentNode,
                                           int32      idx )
{
    QTreeWidgetItem* parentItem = ItemFromNodeHandle(hParentNode);
    if (!parentItem)
        return;

    if (idx < 0 || idx >= parentItem->childCount())
        return;

    QTreeWidgetItem* child = parentItem->takeChild(idx);
    if (!child)
        return;

    std::lock_guard<std::mutex> lock(g_treebox_mutex);
    RemoveItemSubtreeFromMaps(child);
    delete child;
}

// Basic enabled / expanded / selected flags via Qt::ItemFlags & QTreeWidget API.

api_bool (API_TreeBox_GetTreeBoxNodeEnabled)( const_api_handle hNode )
{
    QTreeWidgetItem* item = ItemFromNodeHandle(hNode);
    if (!item)
        return api_false;

    return (item->flags() & Qt::ItemIsEnabled) ? api_true : api_false;
}

void (API_TreeBox_SetTreeBoxNodeEnabled)( api_handle hNode, api_bool enabled )
{
    QTreeWidgetItem* item = ItemFromNodeHandle(hNode);
    if (!item)
        return;

    Qt::ItemFlags f = item->flags();
    if (enabled)
        f |= Qt::ItemIsEnabled;
    else
        f &= ~Qt::ItemIsEnabled;
    item->setFlags(f);
}

api_bool (API_TreeBox_GetTreeBoxNodeExpanded)( const_api_handle hNode )
{
    QTreeWidgetItem* item = ItemFromNodeHandle(hNode);
    if (!item)
        return api_false;

    return item->isExpanded() ? api_true : api_false;
}

void (API_TreeBox_SetTreeBoxNodeExpanded)( api_handle hNode, api_bool exp )
{
    QTreeWidgetItem* item = ItemFromNodeHandle(hNode);
    if (!item)
        return;

    item->setExpanded(exp != 0);
}

api_bool (API_TreeBox_GetTreeBoxNodeSelected)( const_api_handle hNode )
{
    QTreeWidgetItem* item = ItemFromNodeHandle(hNode);
    if (!item)
        return api_false;

    return item->isSelected() ? api_true : api_false;
}

void (API_TreeBox_SetTreeBoxNodeSelected)( api_handle hNode, api_bool sel )
{
    QTreeWidgetItem* item = ItemFromNodeHandle(hNode);
    if (!item)
        return;

    item->setSelected(sel != 0);
}

api_bool (API_TreeBox_GetTreeBoxNodeColText)( const_api_handle /*hNode*/,
                                              int32, char16_type* text,
                                              size_type* len )
{
    // For now, we don't need to round-trip text out via the API
    // in your tests. Return empty string.
    if (len)
        *len = 0;
    if (text)
        *text = 0;
    return api_false;
}

void (API_TreeBox_SetTreeBoxNodeColText)( api_handle hNode,
                                          int32 col,
                                          const char16_type* text )
{
    QTreeWidgetItem* item = ItemFromNodeHandle(hNode);
    if (!item)
        return;

    QString qtext = QString::fromUtf16(
        reinterpret_cast<const ushort*>(text));
    item->setText(col, qtext);
}

// stubs

void           (API_TreeBox_SelectAllTreeBoxNodes)( control_handle )
   {

     //     abort();
   }

api_bool       (API_TreeBox_GetTreeBoxAlternateRowColorEnabled)( const_control_handle )
   {

     //     abort();
     return api_true;
   }

void           (API_TreeBox_SetTreeBoxAlternateRowColorEnabled)( control_handle, api_bool )
   {

     //     abort();
   }

api_bool       (API_TreeBox_SetTreeBoxCurrentNodeUpdatedEventRoutine)( control_handle, api_handle, pcl::item_range_event_routine )
   {

     //     abort();
     return api_true;
   }

api_bool       (API_TreeBox_SetTreeBoxNodeActivatedEventRoutine)( control_handle, api_handle, pcl::item_value_event_routine )
   {

     //     abort();
     return api_true;
   }

api_bool       (API_TreeBox_SetTreeBoxNodeUpdatedEventRoutine)( control_handle, api_handle, pcl::item_value_event_routine )
   {

     //     abort();
     return api_true;
   }

api_bool       (API_TreeBox_SetTreeBoxNodeEnteredEventRoutine)( control_handle, api_handle, pcl::item_value_event_routine )
   {

     //     abort();
     return api_true;
   }

api_bool       (API_TreeBox_SetTreeBoxNodeClickedEventRoutine)( control_handle, api_handle, pcl::item_value_event_routine )
   {

     //     abort();
     return api_true;
   }

api_bool       (API_TreeBox_SetTreeBoxNodeDoubleClickedEventRoutine)( control_handle, api_handle, pcl::item_value_event_routine )
   {

     //     abort();
     return api_true;
   }

api_bool       (API_TreeBox_SetTreeBoxNodeExpandedEventRoutine)( control_handle, api_handle, pcl::item_event_routine )
   {

     //     abort();
     return api_true;
   }

api_bool       (API_TreeBox_SetTreeBoxNodeCollapsedEventRoutine)( control_handle, api_handle, pcl::item_event_routine )
   {

     //     abort();
     return api_true;
   }

api_bool       (API_TreeBox_SetTreeBoxNodeSelectionUpdatedEventRoutine)( control_handle, api_handle, pcl::event_routine )
   {

     //     abort();
     return api_true;
   }

 void           (API_TreeBox_SetTreeBoxNodeColAlignment)( api_handle, int32, int32 )
   {

     //    abort();
   }

   void           (API_TreeBox_SetTreeBoxNodeColIcon)( api_handle, int32, const_bitmap_handle )
   {

     //    abort();
   }

   void           (API_TreeBox_SetTreeBoxNodeColToolTip)( api_handle, int32, const char16_type* )
   {

     //     abort();
   }

   void           (API_TreeBox_SetTreeBoxRootDecorationEnabled)( control_handle, api_bool )
   {

     //     abort();
   }

// ----------------------------------------------------------------------------
// TimerContext API
// ----------------------------------------------------------------------------

control_handle API_Timer_CreateTimer(api_handle /*ignore*/, api_handle client)
{
    LogDbg("API_Timer_CreateTimer called");

    MockTimer* mt = new MockTimer();
    mt->clientHandle = client;

    control_handle h = reinterpret_cast<control_handle>(mt->timer);

    {
        std::lock_guard<std::mutex> lock(g_timer_map_mutex);
        g_timer_map[h] = mt;
    }

    return h;
}

void API_Timer_GetTimerInterval(const_timer_handle, uint32* msec)
{

  abort();
}
api_bool API_Timer_SetTimerInterval(control_handle handle, uint32 msec)
{
    LogDbg("API_Timer_SetInterval called");

    std::lock_guard<std::mutex> lock(g_timer_map_mutex);
    auto it = g_timer_map.find(handle);

    if (it == g_timer_map.end())
        return api_false;

    it->second->timer->setInterval(static_cast<int>(msec));
    return api_true;
}
  
api_bool API_Timer_GetTimerSingleShot(const_timer_handle)
{

  //  abort();
  return api_true;
}
void API_Timer_SetTimerSingleShot(timer_handle, api_bool)
{

  //  abort();
}
api_bool API_Timer_IsTimerActive(const_timer_handle)
{

  abort();
}
api_bool API_Timer_StartTimer(control_handle handle)
{
    LogDbg("API_Timer_StartTimer called");

    std::lock_guard<std::mutex> lock(g_timer_map_mutex);
    auto it = g_timer_map.find(handle);

    if (it == g_timer_map.end())
        return api_false;

    it->second->timer->start();
    return api_true;
}

api_bool API_Timer_StopTimer(control_handle handle)
{
    LogDbg("API_Timer_StopTimer called");

    std::lock_guard<std::mutex> lock(g_timer_map_mutex);
    auto it = g_timer_map.find(handle);

    if (it == g_timer_map.end())
        return api_false;

    it->second->timer->stop();
    return api_true;
}  

api_bool API_Timer_SetTimerNotifyEventRoutine(timer_handle, api_handle, pcl::timer_event_routine)
{

  //  abort();
  return api_true;
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
// FontContext API
// ----------------------------------------------------------------------------

font_handle API_Font_CloneFont(api_handle, const_font_handle)
{

  abort();
}
api_bool API_Font_GetFontFace(const_font_handle, char16_type*, size_type*)
{

  abort();
}
void API_Font_SetFontFace(font_handle, const char16_type*)
{

  abort();
}
api_bool API_Font_GetFontExactMatch(const_font_handle)
{

  abort();
}
int32 API_Font_GetFontPixelSize(const_font_handle)
{

  abort();
}
void API_Font_SetFontPixelSize(font_handle, int32)
{

  abort();
}

void API_Font_GetFontPointSize(const_font_handle, double* sz)
{

  //  abort();
  *sz = 12.0;
}

void API_Font_SetFontPointSize(font_handle, double)
{

  //  abort();
}
api_bool API_Font_GetFontFixedPitch(const_font_handle)
{

  abort();
}
void API_Font_SetFontFixedPitch(font_handle, api_bool)
{

  abort();
}
api_bool API_Font_GetFontKerning(const_font_handle)
{

  abort();
}
void API_Font_SetFontKerning(font_handle, api_bool)
{

  abort();
}
int32 API_Font_GetFontStretchFactor(const_font_handle)
{

  abort();
}
void API_Font_SetFontStretchFactor(font_handle, int32)
{

  abort();
}
int32 API_Font_GetFontWeight(const_font_handle)
{

  abort();
}
void API_Font_SetFontWeight(font_handle, int32)
{

  abort();
}
api_bool API_Font_GetFontItalic(const_font_handle)
{

  //  abort();
  return api_false;
}

void API_Font_SetFontItalic(font_handle, api_bool)
{

  //  abort();
  
}

api_bool API_Font_GetFontUnderline(const_font_handle)
{

  //  abort();
  return api_false;
}
void API_Font_SetFontUnderline(font_handle, api_bool)
{

  //  abort();
}
api_bool API_Font_GetFontOverline(const_font_handle)
{

  abort();
}
void API_Font_SetFontOverline(font_handle, api_bool)
{

  abort();
}
api_bool API_Font_GetFontStrikeOut(const_font_handle)
{

  abort();
}
void API_Font_SetFontStrikeOut(font_handle, api_bool)
{

  abort();
}
int32 API_Font_GetFontAscent(const_font_handle)
{

  abort();
}
int32 API_Font_GetFontDescent(const_font_handle)
{

  abort();
}

int32 API_Font_GetFontHeight(const_font_handle)
{

  //  abort();
  return 12;
}

int32 API_Font_GetFontLineSpacing(const_font_handle)
{

  abort();
}
api_bool API_Font_GetFontCharDefined(const_font_handle, int32)
{

  abort();
}
int32 API_Font_GetFontMaxWidth(const_font_handle)
{

  abort();
}
int32 API_Font_GetStringPixelWidth(const_font_handle handle,
                                   const char16_type* str16)
{
    LogDbg("API_Font_GetStringPixelWidth called");

    if (!handle || !str16)
        return 0;

    std::lock_guard<std::mutex> lock(g_font_map_mutex);
    auto it = g_font_map.find(handle);
 
    if (it == g_font_map.end())
        return 0;

    MockFont* mf = it->second;
    const QFont& font = mf->font;

    QString s = QString::fromUtf16(
        reinterpret_cast<const ushort*>(str16));

    QFontMetrics fm(font);

    int px = fm.horizontalAdvance(s);

    return static_cast<int32>(px);
}
  
int32 API_Font_GetCharPixelWidth(const_font_handle, int32)
{

  abort();
}
void API_Font_GetStringPixelRect(const_font_handle, const char16_type*, int32*, int32*, int32*, int32*, uint32 flags)
{

  abort();
}
api_bool API_Font_EnumerateFonts(font_enumeration_callback f, char16_type* fontFace, size_type* len, void* data, const char* writingSystem)
{

  abort();
}
api_bool API_Font_EnumerateWritingSystems(font_enumeration_callback f, char16_type* wrSystem, size_type* len, void* data, const char16_type* font)
{

  abort();
}
api_bool API_Font_EnumerateFontStyles(font_enumeration_callback f, char16_type* style, size_type* len, void* data, const char16_type* font)
{

  abort();
}
api_bool API_Font_EnumerateOptimalFontPointSizes(font_size_enumeration_callback f, double* ptSize, void* data, const char16_type* font, const char16_type* style)
{

  abort();
}
api_bool API_Font_GetFontScalable(const char16_type* font, const char16_type* style)
{

  abort();
}
api_bool API_Font_GetNominalFontFixedPitch(const char16_type* font, const char16_type* style)
{

  abort();
}
api_bool API_Font_GetNominalFontItalic(const char16_type* font, const char16_type* style)
{

  abort();
}
int32 API_Font_GetNominalFontWeight(const char16_type* font, const char16_type* style)
{

  abort();
}

// ----------------------------------------------------------------------------
// CursorContext API
// ----------------------------------------------------------------------------

cursor_handle API_Cursor_CreateBitmapCursor(api_handle, const_bitmap_handle, int32, int32)
{

  abort();
}
cursor_handle API_Cursor_CloneCursor(api_handle, const_cursor_handle)
{

  abort();
}
void API_Cursor_GetCursorHotSpot(const_cursor_handle, int32*, int32*)
{

  abort();
}

// ----------------------------------------------------------------------------
// SizerContext API
// ----------------------------------------------------------------------------

   int32          (API_Sizer_GetSizerIndex)( const_sizer_handle, const_sizer_handle )
   {

     abort();
   }
   int32          (API_Sizer_GetSizerControlIndex)( const_sizer_handle, const_control_handle )
   {

     abort();
   }

   void           (API_Sizer_RemoveSizer)( sizer_handle, sizer_handle )
   {

     abort();
   }

   void           (API_Sizer_SetSizerStretchFactor)( sizer_handle, sizer_handle, int32 )
   {

     abort();
   }
   void           (API_Sizer_SetSizerControlStretchFactor)( sizer_handle, control_handle, int32 )
   {

     abort();
   }

   void           (API_Sizer_SetSizerAlignment)( sizer_handle, sizer_handle, int32 )
   {

     abort();
   }
   void           (API_Sizer_SetSizerControlAlignment)( sizer_handle, control_handle, int32 )
   {

     abort();
   }

   api_bool       (API_Sizer_GetSizerResourcePixelRatio)( const_sizer_handle, double* )
   {

     abort();
   }
   api_bool       (API_Sizer_GetSizerDevicePixelRatio)( const_sizer_handle, double* )
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

   /*
    * Fast Fourier Transforms (one-dimensional)

   size_type      (API_Numerical_FFTComplexOptimizedLengthF)( size_type n )
   {

     abort();
   }
   size_type      (API_Numerical_FFTComplexOptimizedLengthD)( size_type n )
   {

     abort();
   }

   size_type      (API_Numerical_FFTRealOptimizedLengthF)( size_type n )
   {

     abort();
   }
   size_type      (API_Numerical_FFTRealOptimizedLengthD)( size_type n )
   {

     abort();
   }

   fft_handle     (API_Numerical_FFTCreateComplexTransformF)( size_type n )
   {

     abort();
   }
   fft_handle     (API_Numerical_FFTCreateComplexTransformD)( size_type n )
   {

     abort();
   }

   fft_handle     (API_Numerical_FFTCreateComplexInverseTransformF)( size_type n )
   {

     abort();
   }
   fft_handle     (API_Numerical_FFTCreateComplexInverseTransformD)( size_type n )
   {

     abort();
   }

   fft_handle     (API_Numerical_FFTCreateRealTransformF)( size_type n )
   {

     abort();
   }
   fft_handle     (API_Numerical_FFTCreateRealTransformD)( size_type n )
   {

     abort();
   }

   fft_handle     (API_Numerical_FFTCreateRealInverseTransformF)( size_type n )
   {

     abort();
   }
   fft_handle     (API_Numerical_FFTCreateRealInverseTransformD)( size_type n )
   {

     abort();
   }

   api_bool       (API_Numerical_FFTDestroyTransform)( fft_handle hFFT )
   {

     abort();
   }

   api_bool       (API_Numerical_FFTComplexTransformF)( fft_handle hFFT, void* y, const void* x )
   {
     // void* = fcomplex*
     abort();
   }
   api_bool       (API_Numerical_FFTComplexTransformD)( fft_handle hFFT, void* y, const void* x )
   {
     // void* = dcomplex*
     abort();
   }

   api_bool       (API_Numerical_FFTComplexInverseTransformF)( fft_handle hFFT, void* y, const void* x )
   {
     // void* = fcomplex*
     abort();
   }
   api_bool       (API_Numerical_FFTComplexInverseTransformD)( fft_handle hFFT, void* y, const void* x )
   {
     // void* = dcomplex*
     abort();
   }

   api_bool       (API_Numerical_FFTRealTransformF)( fft_handle hFFT, void* y, const float* x )
   {
     // void* = fcomplex*
     abort();
   }
   api_bool       (API_Numerical_FFTRealTransformD)( fft_handle hFFT, void* y, const double* x )
   {
     // void* = dcomplex*
     abort();
   }

   api_bool       (API_Numerical_FFTRealInverseTransformF)( fft_handle hFFT, float* x, const void* y )
   {
     // void* = fcomplex*
     abort();
   }
   api_bool       (API_Numerical_FFTRealInverseTransformD)( fft_handle hFFT, double* x, const void* y )
   {
     // void* = dcomplex*
     abort();
   }
    */
// ----------------------------------------------------------------------------
// GPUContext API
// ----------------------------------------------------------------------------

api_bool API_GPU_InitCUDARuntime(api_handle, uint32 /*flags*/)
{

  abort();
}
api_bool API_GPU_IsCUDADeviceAvailable(api_handle)
{

  abort();
}
api_bool API_GPU_EnumerateCUDADevices(api_handle, pcl::cuda_device_enumeration_callback, void* deviceProps/*cudaDeviceProp*/, size_type structSize, void* data)
{

  abort();
}
cuda_device_handle API_GPU_GetCUDASelectedDevice(api_handle)
{

  abort();
}
api_bool API_GPU_GetCUDADeviceProperties(api_handle, cuda_device_handle, void* deviceProps, size_type structSize)
{

  abort();
}
size_type API_GPU_GetCUDADeviceTotalGlobalMem(api_handle, cuda_device_handle)
{

  abort();
}
int32 API_GPU_GetCUDADeviceMaxThreadsPerBlock(api_handle, cuda_device_handle)
{

  abort();
}
size_type API_GPU_GetCUDADeviceSharedMemoryPerBlock(api_handle, cuda_device_handle)
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

// ----------------------------------------------------------------------------
// ViewContext API
// ----------------------------------------------------------------------------

view_handle API_View_GetViewById(const char* fullId)
{

  abort();
}
void API_View_EnumerateViews(pcl::view_enumeration_callback, void*, api_bool includeMainViews, api_bool includePreviews)
{

  abort();
}
api_bool API_View_IsPreview(const_view_handle)
{

  abort();
}
api_bool API_View_IsVolatilePreview(const_view_handle)
{

  abort();
}
api_bool API_View_IsStoredPreview(const_view_handle)
{

  abort();
}
window_handle API_View_GetViewParentWindow(const_view_handle)
{

  abort();
}
api_bool API_View_GetViewId(const_view_handle, char*, size_type*)
{

  abort();
}
api_bool API_View_GetViewFullId(const_view_handle, char*, size_type*)
{

  abort();
}
api_bool API_View_SetViewId(view_handle, const char*)
{

  abort();
}
void API_View_GetViewLocks(const_view_handle, api_bool*, api_bool*)
{

  abort();
}
void API_View_LockView(view_handle, api_bool, api_bool, api_bool)
{

  abort();
}
void API_View_UnlockView(view_handle, api_bool, api_bool, api_bool)
{

  abort();
}
api_bool API_View_IsViewDynamicTarget(const_view_handle)
{

  abort();
}
void API_View_AddViewToDynamicTargets(view_handle)
{

  abort();
}
void API_View_RemoveViewFromDynamicTargets(view_handle)
{

  abort();
}

image_handle API_View_GetViewImage(const_view_handle vhandle)
{
    LogDbg("API_View_GetViewImage called");

    if (!vhandle)
        return nullptr;

    MockView* mv = nullptr;

    // Look up the view
    {
        std::lock_guard<std::mutex> lock(g_view_map_mutex);

        auto it = g_view_map.find(vhandle);
        if (it == g_view_map.end())
            return nullptr;

        mv = it->second;
    }

    // View without an image?
    if (mv->image == nullptr)
        return nullptr;

    image_handle ih = reinterpret_cast<image_handle>(mv->image);

    // Double-check the image exists in the map (safety only)
    {
        std::lock_guard<std::mutex> lock(g_image_map_mutex);
        if (g_image_map.find(ih) == g_image_map.end())
        {
            // Should not happen — but if it does, insert it
            g_image_map[ih] = mv->image;
        }
    }

    return ih;
}

api_bool API_View_IsViewColorImage(const_view_handle)
{

  abort();
}
api_bool API_View_GetViewDimensions(const_view_handle, int32*, int32*)
{

  abort();
}
api_bool API_View_GetViewScreenTransferFunctions(const_view_handle, double* m, double* c0, double* c1, double* r0, double* r1)
{

  abort();
}
api_bool API_View_SetViewScreenTransferFunctions(view_handle, const double* m, const double* c0, const double* c1, const double* r0, const double* r1, api_bool)
{

  abort();
}
api_bool API_View_DestroyViewScreenTransferFunctions(view_handle, api_bool)
{

  abort();
}
api_bool API_View_GetViewScreenTransferFunctionsEnabled(view_handle)
{

  abort();
}
void API_View_SetViewScreenTransferFunctionsEnabled(view_handle, api_bool, api_bool)
{

  abort();
}
api_bool API_View_IsReservedViewPropertyId(const char* id)
{

  abort();
}
api_bool API_View_EnumerateViewProperties(const_view_handle, pcl::property_enumeration_callback, char*, size_type*, void*)
{

  abort();
}
api_bool API_View_GetViewPropertyValue(api_handle hModule, const_view_handle, const char* id, api_property_value*)
{

  abort();
}
api_bool API_View_GetViewPropertyAttributes(api_handle hModule, const_view_handle, const char* id, uint32* flags, uint64* type)
{

  abort();
}
api_bool API_View_SetViewPropertyValue(api_handle hModule, view_handle, const char* id, const api_property_value*, uint32 flags, api_bool notify)
{

  abort();
}
api_bool API_View_SetViewPropertyAttributes(api_handle hModule, view_handle, const char* id, uint32 flags, api_bool notify)
{

  abort();
}
api_bool API_View_GetViewPropertyExists(api_handle hModule, const_view_handle, const char* id, uint64* type)
{

  abort();
}
api_bool API_View_DeleteViewProperty(api_handle hModule, view_handle, const char* id, api_bool notify)
{

  abort();
}
api_bool API_View_ComputeViewProperty(api_handle hModule, view_handle, const char* id, api_bool notify, api_property_value*)
{

  abort();
}

// ----------------------------------------------------------------------------
// ImageWindowContext API
// ----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// ImageWindow Creation
// -----------------------------------------------------------------------------

window_handle API_ImageWindow_CreateImageWindow(
    int32 width,
    int32 height,
    int32 numberOfChannels,
    int32 bitsPerSample,
    api_bool floatSample,
    api_bool color,
    api_bool initialProcessing,
    const char* id )
{
    // Create underlying Qt window
    QWidget* win = new QWidget();
    win->setWindowTitle( id ? id : "ImageWindow" );
    win->resize( width, height );

    // Allocate a PCL-style view + image to attach to this window
    MockImageWindow* mw = new MockImageWindow();
    mw->window = win;

    // Register window in global map
    {
        std::lock_guard<std::mutex> lock( g_view_map_mutex );
        g_image_window_map[ win ] = mw;
    }

    // ---- Allocate Image Data --------------------------------------------------
    MockImage* img = new MockImage();
    img->width          = width;
    img->height         = height;
    img->channels       = numberOfChannels;
    img->bitsPerSample  = bitsPerSample;
    img->isFloat        = floatSample;
    img->colorSpace     = color ? 1 : 0;

    // Allocate channel data
    img->pixelData = new void*[ numberOfChannels ];
    img->stats = new double[ numberOfChannels * 2 ];

    size_t elementSize = floatSample ? sizeof(float) :
                        (bitsPerSample <= 8 ? sizeof(uint8_t) :
                        (bitsPerSample <= 16 ? sizeof(uint16_t) :
                                               sizeof(uint32_t)));

    size_t pixels = size_t(width) * size_t(height);

    for (int c = 0; c < numberOfChannels; ++c)
    {
        img->pixelData[c] = malloc( pixels * elementSize );
        memset( img->pixelData[c], 0, pixels * elementSize );
        img->stats[2*c + 0] = 0.0;   // min
        img->stats[2*c + 1] = 1.0;   // max
    }

    // Register in image map
    image_handle ih = reinterpret_cast<image_handle>( img );
    {
        std::lock_guard<std::mutex> lock( g_image_map_mutex );
        g_image_map[ih] = img;
    }

    // Attach the image to the MockImageWindow
    mw->currentImage = ih;

    // Show the window
    win->show();

    return reinterpret_cast<window_handle>(win);
}

api_bool API_ImageWindow_LoadImageWindows(const char16_type* url, const char* id, const char* hints, api_bool asACopy, api_bool allowMessages, pcl::window_enumeration_callback, void*)
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

window_handle API_ImageWindow_GetImageWindowByFilePath(const char16_type*)
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
api_bool API_ImageWindow_GetImageWindowFileURL(const_window_handle, char16_type*, size_type*)
{

  abort();
}
api_bool API_ImageWindow_GetImageWindowFilePath(const_window_handle, char16_type*, size_type*)
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
void API_ImageWindow_LoadImageWindowICCProfile(window_handle, const char16_type*)
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
api_bool API_ImageWindow_GetSwapDirectory(int32, char16_type*, size_type*)
{

  abort();
}
api_bool API_ImageWindow_SetSwapDirectories(const char16_type**, int32)
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
void API_ImageView_LoadImageViewICCProfile(control_handle, const char16_type*)
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
api_bool API_CodeEditor_GetEditorFilePath(const_control_handle, char16_type*, size_type*)
{

  abort();
}
void API_CodeEditor_SetEditorFilePath(control_handle, const char16_type*)
{

  abort();
}
api_bool API_CodeEditor_GetEditorText(const_control_handle, char16_type*, size_type*)
{

  abort();
}
void API_CodeEditor_SetEditorText(control_handle, const char16_type*)
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
api_bool API_CodeEditor_SaveEditorText(control_handle, const char16_type* filePath, const char* encoding)
{

  abort();
}
api_bool API_CodeEditor_LoadEditorText(control_handle, const char16_type* filePath, const char* encoding)
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
api_bool API_CodeEditor_GetEditorSelectedText(const_control_handle, char16_type*, size_type*)
{

  abort();
}
void API_CodeEditor_InsertEditorText(control_handle, const char16_type*)
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
int32 API_CodeEditor_EditorHighlightAllMatches(control_handle, const char16_type*, uint32 flags)
{

  abort();
}
void API_CodeEditor_EditorClearMatches(control_handle)
{

  abort();
}
api_bool API_CodeEditor_EditorFind(control_handle, const char16_type*, uint32 flags)
{

  abort();
}
api_bool API_CodeEditor_EditorReplace(control_handle, const char16_type*)
{

  abort();
}
int32 API_CodeEditor_EditorReplaceAll(control_handle, const char16_type*, const char16_type*, uint32 flags)
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
api_bool API_WebView_LoadWebViewContent(control_handle, const char16_type* URI)
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
api_bool API_WebView_SaveWebViewAsPDF(control_handle, const char16_type* filePath, const double* pageWidth, const double* pageHeight, const double* marginLeft, const double* marginTop, const double* marginRight, const double* marginBottom, int32 orientation)
{

  abort();
}
api_bool API_WebView_GetWebViewHasSelection(const_control_handle)
{

  abort();
}
api_bool API_WebView_GetWebViewSelectedText(const_control_handle, char16_type*, size_type*)
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
api_bool API_WebView_EvaluateWebViewScript(control_handle, const char16_type* sourceCode, const char* language)
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

   int32          (API_ExternalProcess_ExecuteProgram)( const char16_type* program, const char16_type** argv, size_type argc )
   {

     abort();
   }

   api_bool       (API_ExternalProcess_StartProgram)( const char16_type* program, const char16_type** argv, size_type argc,
                                            const char16_type* workingDirectory, uint64* pid )
   {

     abort();
   }

   external_process_handle (API_ExternalProcess_CreateExternalProcess)( api_handle hModule, api_handle hClient )
   {

     abort();
   }

   api_bool       (API_ExternalProcess_StartExternalProcess)( external_process_handle,
                                                    const char16_type* program, const char16_type** argv, size_type argc )
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

   api_bool       (API_ExternalProcess_RedirectExternalProcessToFile)( external_process_handle, int32 stream, const char16_type* fileName, api_bool append )
   {

     abort();
   }
   api_bool       (API_ExternalProcess_PipeExternalProcess)( external_process_handle, int32 stream, external_process_handle toProcess )
   {

     abort();
   }

   api_bool       (API_ExternalProcess_GetExternalProcessWorkingDirectory)( const_external_process_handle, char16_type*, size_type* )
   {

     abort();
   }
   api_bool       (API_ExternalProcess_SetExternalProcessWorkingDirectory)( external_process_handle, const char16_type* )
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
   api_bool       (API_ExternalProcess_SetExternalProcessEnvironment)( external_process_handle, const char16_type** vars, size_type count )
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
   void        (API_ExternalProcess_SetProcessDescription)( const char16_type* )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessScriptComment)( const char16_type* )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessIconSVG)( const char* )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessIconSVGFile)( const char16_type* )
   {

abort();
   }
   void        (API_ExternalProcess_SetProcessIconImage)( const char** )
   {
   // ### deprecated
abort();
   }
   void        (API_ExternalProcess_SetProcessIconImageFile)( const char16_type* )
   {
   // ### deprecated
abort();
   }
   void        (API_ExternalProcess_SetProcessIconSmallImage)( const char** )
   {
   // ### deprecated
abort();
   }
   void        (API_ExternalProcess_SetProcessIconSmallImageFile)( const char16_type* )
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
   void        (API_ExternalProcess_SetParameterDescription)( const char16_type* )
{

abort();
}
   void        (API_ExternalProcess_SetParameterScriptComment)( const char16_type* )
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
   void        (API_ExternalProcess_SetDefaultStringValue)( const char16_type* )
{

abort();
}
   void        (API_ExternalProcess_SetStringAllowedCharacters)( const char16_type* )
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
api_bool API_NetworkTransfer_SetNetworkTransferURL(network_transfer_handle, const char16_type* url, const char16_type* userName, const char16_type* userPassword)
{

  abort();
}
api_bool API_NetworkTransfer_SetNetworkTransferProxyURL(network_transfer_handle, const char16_type* proxy, const char16_type* userName, const char16_type* userPassword)
{

  abort();
}
api_bool API_NetworkTransfer_SetNetworkTransferSSL(network_transfer_handle, api_bool useSSL, api_bool forceSSL, api_bool verifyPeer, api_bool verifyHost)
{

  abort();
}
api_bool API_NetworkTransfer_SetNetworkTransferCustomHTTPHeaders(network_transfer_handle, const char16_type* nlsHeaders)
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
api_bool API_NetworkTransfer_PerformNetworkTransferPOST(network_transfer_handle, const char16_type* postFields)
{

  abort();
}
api_bool API_NetworkTransfer_PerformNetworkTransferSMTP(network_transfer_handle, const char16_type* mailFrom, const char16_type* mailRecipients)
{

  abort();
}
void API_NetworkTransfer_CloseNetworkTransferConnection(network_transfer_handle)
{

  abort();
}
api_bool API_NetworkTransfer_GetNetworkTransferURL(const_network_transfer_handle, char16_type*, size_type*)
{

  abort();
}
api_bool API_NetworkTransfer_GetNetworkTransferProxyURL(const_network_transfer_handle, char16_type*, size_type*)
{

  abort();
}
api_bool API_NetworkTransfer_GetNetworkTransferCustomHTTPHeaders(const_network_transfer_handle, char16_type*, size_type*)
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
api_bool API_NetworkTransfer_GetNetworkTransferContentType(const_network_transfer_handle, char16_type*, size_type*)
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
   api_bool       (API_NetworkTransfer_GetNetworkTransferErrorInformation)( const_network_transfer_handle, char16_type*, size_type* )
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

}  // extern "C"

#include "PCLMockAPI.moc"
