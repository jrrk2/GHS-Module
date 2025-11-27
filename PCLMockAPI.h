/**
 * PCLMockAPI.h - Main header for the PCL mock API
 * 
 * This file provides the main interface for the PCL mock API, which
 * can be used to simulate the PCL API in test environments without
 * requiring the full PixInsight application.
 */

#ifndef PCL_MOCK_API_H
#define PCL_MOCK_API_H

#include <functional>
#include <string>
#include <pcl/api/APIDefs.h>
#include <pcl/ProcessInterface.h>
#include <QWidget>

// Common PCL API types
//typedef int api_bool;
//const api_bool api_true = 1;
//const api_bool api_false = 0;
typedef unsigned int uint32;
typedef void* api_handle;
typedef void* thread_handle;


// Mock file format structure
struct MockFileFormat {
    std::string name;
    std::vector<std::string> extensions;
    std::vector<std::string> mimeTypes;
    uint32 version;
    std::string description;
    std::string implementation;
    bool canRead;
    bool canWrite;
    bool canReadIncrementally;
    bool canWriteIncrementally;
    bool canStore8Bit;
    bool canStore16Bit;
    bool canStore32Bit;
    bool canStore64Bit;
    bool canStoreFloat;
    bool canStoreDouble;
    bool canStoreComplex;
    bool canStoreDComplex;
    bool canStoreGrayscale;
    bool canStoreRGBColor;
    bool canStoreAlphaChannels;
    bool supportsCompression;
    bool supportsMultipleImages;
};

namespace pcl_mock {

// Function resolver type (matches PCL's definition)
typedef void* (*function_resolver)(const char* name);

/**
 * Initialize the mock API system
 * This must be called before using any mock API functions
 */
void InitializeMockAPI();

/**
 * Get the function resolver that can be used with pcl::APIInterface
 * @return A function resolver that provides mock implementations
 */
function_resolver GetMockFunctionResolver();

/**
 * Set the module handle to be used by the mock API
 * @param handle The module handle to use
 */
void SetModuleHandle(void* handle);

/**
 * Get the current module handle
 * @return The current module handle
 */
void* GetModuleHandle();

/**
 * Register a function with the mock API
 * @param name The name of the function (e.g. "Thread/CreateThread")
 * @param func A pointer to the function implementation
 */
void RegisterFunction(const char* name, void* func);
 /**
 * Register GLobal API functions with the mock API
 */
void RegisterGlobalFunctions();
 /**
 * Register UI API functions with the mock API
 */
void RegisterUIFunctions();
 
 /**
 * Register FileFormat API functions with the mock API
 */
void RegisterFileFormatFunctions();
 
 /**
 * Register SharedImage API functions with the mock API
 */
void RegisterSharedImageFunctions();

/**
 * Set the log file for debug output
 * @param filename The path to the log file
 */
void SetLogFile(const std::string& filename);

} // namespace pcl_mock

extern "C" {
  void SetDebugLogging(bool);
  void SetModuleHandle(void *);
  QWidget* FindInterfaceGuiRoot();
  
};

// SpinBox event handler typedef
namespace pcl {
    typedef void (*spinbox_value_event_routine)(void* receiver, control_handle sender, int32 value);
}

#endif // PCL_MOCK_API_H
