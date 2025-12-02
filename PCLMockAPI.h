// ===============================================================
//  PCLMockAPI.h  —  Unified Mock PixInsight Core API (Header)
// ===============================================================
#pragma once
#include <cstdint>
#include <QtWidgets>
#include <pcl/api/APIDefs.h>
#include <pcl/ProcessInterface.h>

// ---------------------------------------------------------------
// Basic Types
// ---------------------------------------------------------------
typedef void* api_handle;
typedef control_handle label_handle;
typedef control_handle edit_handle;
typedef control_handle slider_handle;
typedef control_handle button_handle;
typedef control_handle combo_handle;
typedef control_handle spin_handle;
typedef void* sizer_handle;
typedef void* thread_handle;

// ---------------------------------------------------------------
// Event Routine Typedefs (Stubs)
// ---------------------------------------------------------------
namespace pcl
{
    typedef void (*edit_event_routine)(api_handle receiver,
                                       edit_handle edit,
                                       const char16_t* text);
    typedef void (*api_button_event_routine)(api_handle receiver,
                                             button_handle btn);
    typedef void (*api_slider_value_event_routine)(api_handle receiver,
                                                   slider_handle slider,
                                                   int value);
    typedef void (*api_spinbox_value_event_routine)(api_handle receiver,
                                                    spin_handle spin,
                                                    int value);
}

// =============================================================
// Mock Object: Unified structure for all object types
// =============================================================
struct MockBase
{
    // Type flags
    bool isSizer = false;
    bool isTreeNode = false;
    
    // Module ownership
    api_handle moduleHandle = nullptr;
    
    // PCL-side handle (client pointer - this is the KEY in g_objects)
    control_handle pcl_handle = nullptr;
    
    // Qt objects (only ONE should be set based on type)
    QWidget* widget = nullptr;              // For controls
    QBoxLayout* layout = nullptr;           // For sizers
    QTreeWidgetItem* treeItem = nullptr;    // For tree nodes
    
    // Layout properties
    bool vertical = false;
    
    // Event receiver for callbacks
    control_handle eventReceiver = nullptr;
    
    // Control event callbacks
    pcl::control_event_routine onShow = nullptr;
    pcl::mouse_event_routine onMouseMove = nullptr;
    pcl::mouse_button_event_routine onMousePress = nullptr;
    pcl::mouse_button_event_routine onMouseRelease = nullptr;
    pcl::keyboard_event_routine onKeyPress = nullptr;
    
    // Button event callbacks
    pcl::button_click_event_routine onButtonClick = nullptr;
    pcl::button_check_event_routine onButtonCheck = nullptr;
    
    // TreeBox event callbacks
    pcl::item_value_event_routine onTreeNodeActivated = nullptr;
    pcl::item_range_event_routine onTreeNodeUpdated = nullptr;
    pcl::event_routine onTreeSelectionUpdated = nullptr;
    
    // Destructor - Qt manages memory via parent-child relationships
    ~MockBase()
    {
        // Only delete if no Qt parent manages it
        if (widget && !widget->parent()) {
	  //            delete widget;
            widget = nullptr;
        }
        
        // Tree items are owned by their tree or parent item
        if (treeItem && !treeItem->treeWidget() && !treeItem->parent()) {
            delete treeItem;
            treeItem = nullptr;
        }
        
        // Layouts are owned by their parent widget
        if (layout && !layout->parent()) {
            delete layout;
            layout = nullptr;
        }
    }
};

extern QList<MockBase*> g_topLevelWidgets;

// ---------------------------------------------------------------
// Core APIs
// ---------------------------------------------------------------
extern "C" {
void SetDebugLogging(bool);
};
