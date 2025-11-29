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
typedef void* control_handle;
typedef void* label_handle;
typedef void* edit_handle;
typedef void* slider_handle;
typedef void* button_handle;
typedef void* combo_handle;
typedef void* spin_handle;
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

// ---------------------------------------------------------------
// Core Creation APIs
// ---------------------------------------------------------------

extern "C" {
void SetDebugLogging(bool);
};
