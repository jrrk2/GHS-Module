// sandbox_main.cpp - Updated to use QtUiExporterEnhanced

#include <QApplication>
#include <pcl/Console.h>
#include <pcl/api/APIInterface.h>
#include <pcl/MetaModule.h>

#include "SandboxModule.h"
#include "SandboxInterface.h"
#include "SandboxProcess.h"
#include "PCLMockAPI.h"
#include "ExportHelper.h"  // Use the new enhanced exporter

using namespace pcl;

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    SetDebugLogging(true);

    // Initialize PCL module
    Module = new SandboxModule;
    SandboxProcess proc;
    SandboxInterface iface;

    // Initialize mock API
    bool dynamic = false;
    unsigned flags = 0;
    API = new APIInterface(nullptr);
   
    // Launch the interface
    if (!iface.Launch(proc, nullptr, dynamic, flags)) {
        fputs("Launch() returned false – interface did not accept the process.\n", stderr);
        return 1;
    }

    // Show the interface
    iface.Show();

    // Get the root widget from the mock API
    QWidget* rootWidget = g_lastTopLevel->widget;
    
    if (!rootWidget) {
        fputs("Error: No root widget found!\n", stderr);
        return 1;
    }

    Console().WriteLn("<end><cbr>Interface created successfully.");
    Console().WriteLn("<end><cbr>Root widget: " + String(rootWidget->metaObject()->className()));
    
    // ========================================================================
    // EXPORT THE INTERFACE TO QT C++ CODE
    // ========================================================================
    
    Console().WriteLn("<end><cbr><br>Exporting interface to Qt C++ code...");
    
    // Export with enhanced exporter
    bool exportSuccess = ExportHelper::exportInterface(
        rootWidget,           // The widget to export
        "SandboxDialog",      // Base name for generated files
        "./exported"          // Output directory
    );
    
    if (exportSuccess) {
        Console().WriteLn("<end><cbr>✓ Export successful!");
        Console().WriteLn("<end><cbr>  Generated files:");
        Console().WriteLn("<end><cbr>    - ./exported/SandboxDialog.h");
        Console().WriteLn("<end><cbr>    - ./exported/SandboxDialog.cpp");
        Console().WriteLn("<end><cbr>    - ./exported/SandboxDialog_metadata.json");
    } else {
        Console().WriteLn("<end><cbr>✗ Export failed!");
    }
    
    // You can also export with custom options:
    /*
    ExportHelper::exportInterfaceCustom(
        rootWidget,
        "SandboxDialogCustom",
        "./exported",
        true,           // generateSignalsSlots
        true,           // addComments
        "QDialog"       // baseClass (instead of QWidget)
    );
    */
    
    // ========================================================================
    // RUN THE APPLICATION
    // ========================================================================
    
    Console().WriteLn("<end><cbr><br>Starting Qt event loop...");
    Console().WriteLn("<end><cbr>The interface is now interactive.");
    Console().WriteLn("<end><cbr>You can test all controls and then close the window.");
    
    // Enter the Qt event loop
    int result = app.exec();
    
    Console().WriteLn("<end><cbr><br>Application exited with code: " + String(result));
    
    return result;
}
