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
  
control_handle API_Control_CreateControl(api_handle module);

label_handle   API_Label_CreateLabel(api_handle module,
                                     api_handle client,
                                     control_handle parent);

edit_handle    API_Edit_CreateEdit(api_handle module,
                                   api_handle client,
                                   control_handle parent);

slider_handle  API_Slider_CreateSlider(api_handle module,
                                       api_handle client,
                                       control_handle parent);

button_handle  API_Button_CreateCheckBox(api_handle module,
                                         api_handle client,
                                         control_handle parent);

combo_handle   API_ComboBox_CreateComboBox(api_handle module,
                                           api_handle client,
                                           control_handle parent);

spin_handle    API_SpinBox_CreateSpinBox(api_handle module,
                                         api_handle client,
                                         control_handle parent);

// ---------------------------------------------------------------
// Sizer APIs
// ---------------------------------------------------------------
sizer_handle API_Sizer_CreateSizer(api_handle module,
                                   api_handle client,
                                   api_bool vertical);

api_bool API_Sizer_InsertSizerControl(sizer_handle sizer,
                                      api_handle client,
                                      control_handle child,
                                      int index,
                                      int stretch,
                                      uint32 flags);

api_bool API_Sizer_InsertSizer(sizer_handle sizer,
                               api_handle client,
                               sizer_handle childSizer,
                               int index,
                               int stretch,
                               uint32 flags);

api_bool API_Control_SetControlSizer(control_handle ctrl,
                                     api_handle client,
                                     sizer_handle sizer);

// ---------------------------------------------------------------
// Visibility / Sizing / Appearance
// ---------------------------------------------------------------
api_bool API_Control_SetControlVisible(control_handle ctrl,
                                       api_handle client,
                                       uint32 flags);

api_bool API_Control_SetControlFixedSize(control_handle ctrl,
                                         api_handle client,
                                         int32 w,
                                         int32 h);

api_bool API_Control_SetControlMinSize(control_handle ctrl,
                                       api_handle client,
                                       int32 w,
                                       int32 h);

api_bool API_Control_SetControlBackgroundColor(control_handle ctrl,
                                               api_handle client,
                                               uint32 rgba);

// ---------------------------------------------------------------
// Type-specific Control APIs
// ---------------------------------------------------------------
api_bool API_ComboBox_SetEditable(control_handle ctrl,
                                  api_handle client,
                                  api_bool editable);

api_bool API_Slider_SetSliderValue(control_handle ctrl,
                                   api_handle client,
                                   int value);

api_bool API_SpinBox_SetSpinBoxRange(control_handle ctrl,
                                     api_handle client,
                                     int minValue,
                                     int maxValue);

api_bool API_SpinBox_SetSpinBoxValue(control_handle ctrl,
                                     api_handle client,
                                     int value);

// ---------------------------------------------------------------
// Event registration (stubs only)
// ---------------------------------------------------------------
api_bool API_Control_SetChildControlToFocus(control_handle ctrl,
                                            api_handle client,
                                            control_handle child);

api_bool API_Control_SetControlFocusStyle(control_handle,
                                          api_handle,
                                          uint32 style);

api_bool API_Edit_SetEditCompletedEventRoutine(edit_handle edit,
                                               api_handle client,
                                               api_handle receiver,
                                               pcl::edit_event_routine);

api_bool API_Edit_SetReturnPressedEventRoutine(edit_handle edit,
                                               api_handle client,
                                               api_handle receiver,
                                               pcl::edit_event_routine);

api_bool API_Slider_SetSliderValueUpdatedEventRoutine(slider_handle,
                                                      api_handle,
                                                      api_handle,
                                                      pcl::api_slider_value_event_routine);

api_bool API_Button_SetButtonClickEventRoutine(button_handle,
                                               api_handle,
                                               api_handle,
                                               pcl::api_button_event_routine);

api_bool API_SpinBox_SetValueUpdatedEventRoutine(spin_handle,
                                                 api_handle,
                                                 api_handle,
                                                 pcl::api_spinbox_value_event_routine);

};
