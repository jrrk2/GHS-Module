// ===============================================================
//  PCLMockAPI.h  —  Unified Mock PixInsight Core API (Header)
// ===============================================================
//  This header declares the subset of the PCL API required by
//  your module diagnostic environment. All handle types are
//  opaque void pointers. The implementation is in PCLMockAPI.cpp.
//
//  NOTES:
//   • All controls & sizers use handle = void*
//   • All widgets are QWidget* internally
//   • Only APIs actually used by your module are declared here
//   • Event routines are stubbed as function pointers
//   • api_bool is int (0=false, 1=true)
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
// Mock Object: The only structure we need
// =============================================================
struct MockBase
{
    QWidget*     widget  = nullptr;   // QWidget* if control
    QBoxLayout*  layout  = nullptr;   // QBoxLayout* if sizer
    bool vertical = false;          // <-- REQUIRED
    bool         isSizer = false;

    api_handle   moduleHandle = nullptr;
    control_handle pcl_handle = nullptr;
    control_handle eventReceiver = nullptr;
  
    // Control event callbacks
    pcl::control_event_routine        onShow     = nullptr;
    pcl::mouse_event_routine          onMouseMove = nullptr;
    pcl::mouse_button_event_routine   onMousePress = nullptr;
    pcl::mouse_button_event_routine   onMouseRelease = nullptr;
    pcl::keyboard_event_routine       onKeyPress = nullptr;

    // Button
    pcl::button_click_event_routine   onButtonClick = nullptr;
    pcl::button_check_event_routine   onButtonCheck = nullptr;

    // TreeBox
    pcl::item_value_event_routine     onTreeNodeActivated = nullptr;
    pcl::item_range_event_routine     onTreeNodeUpdated = nullptr;
    pcl::event_routine                onTreeSelectionUpdated = nullptr;
};

extern QList<MockBase*> g_topLevelWidgets;  // All candidates!

// ---------------------------------------------------------------
// Core Creation APIs
// ---------------------------------------------------------------

extern "C" {
void SetDebugLogging(bool);
};
