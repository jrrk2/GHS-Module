// sandbox_main_simple_diagnostic.cpp
// Simplified diagnostic version that compiles cleanly

#include <QApplication>
#include <QTimer>
#include <QThread>
#include <QDebug>
#include <QLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <pcl/Console.h>
#include <pcl/api/APIInterface.h>
#include <pcl/MetaModule.h>

#include "SandboxModule.h"
#include "SandboxInterface.h"
#include "SandboxProcess.h"
#include "PCLMockAPI.h"
#include "ExportHelper.h"

using namespace pcl;

// ============================================================================
// Simple Widget Tree Analysis
// ============================================================================

void analyzeWidget(QWidget* widget, int depth = 0)
{
    if (!widget) return;
    
    QString indent(depth * 2, ' ');
    
    qDebug().noquote() << indent << widget->metaObject()->className()
                       << (widget->objectName().isEmpty() ? "" : QString("(%1)").arg(widget->objectName()))
                       << "size:" << widget->size();
    
    if (widget->layout()) {
        QLayout* layout = widget->layout();
        qDebug().noquote() << indent << "  └─ Layout:" 
                           << layout->metaObject()->className()
                           << "items:" << layout->count();
        
        // Show layout contents
        for (int i = 0; i < layout->count(); ++i) {
            QLayoutItem* item = layout->itemAt(i);
            if (item->widget()) {
                analyzeWidget(item->widget(), depth + 2);
            } else if (item->layout()) {
                qDebug().noquote() << indent << "    [nested layout]";
            } else if (item->spacerItem()) {
                qDebug().noquote() << indent << "    [spacer]";
            }
        }
    }
}

void countWidgets(QWidget* root)
{
    if (!root) return;
    
    int labelCount = 0;
    int editCount = 0;
    int spinboxCount = 0;
    int checkboxCount = 0;
    int comboboxCount = 0;
    int sliderCount = 0;
    
    std::function<void(QWidget*)> count = [&](QWidget* w) {
        if (!w) return;
        
        QString type = w->metaObject()->className();
        if (type == "QLabel") labelCount++;
        else if (type == "QLineEdit") editCount++;
        else if (type == "QSpinBox") spinboxCount++;
        else if (type == "QCheckBox") checkboxCount++;
        else if (type == "QComboBox") comboboxCount++;
        else if (type == "QSlider") sliderCount++;
        
        // Recurse through children
        for (QObject* child : w->children()) {
            if (QWidget* childWidget = qobject_cast<QWidget*>(child)) {
                count(childWidget);
            }
        }
    };
    
    count(root);
    
    qDebug() << "\n=== Widget Count ===";
    qDebug() << "Labels:" << labelCount << "(expected: 3)";
    qDebug() << "LineEdits:" << editCount << "(expected: 2)";
    qDebug() << "SpinBoxes:" << spinboxCount << "(expected: 1)";
    qDebug() << "CheckBoxes:" << checkboxCount << "(expected: 1)";
    qDebug() << "ComboBoxes:" << comboboxCount << "(expected: 1)";
    qDebug() << "Sliders:" << sliderCount << "(expected: 1)";
    qDebug() << "===================\n";
}

QWidget* findBetterRoot(QWidget* start)
{
    // Look for a widget with QVBoxLayout containing 5 items
    QWidget* current = start;
    QWidget* bestCandidate = start;
    int maxItems = 0;
    
    while (current) {
        if (auto* vbox = qobject_cast<QVBoxLayout*>(current->layout())) {
            int count = vbox->count();
            if (count > maxItems) {
                maxItems = count;
                bestCandidate = current;
                qDebug() << "Found candidate with" << count << "items:"
                         << current->metaObject()->className();
            }
        }
        current = current->parentWidget();
    }
    
    return bestCandidate;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    SetDebugLogging(true);
    API = new APIInterface(nullptr);

    Console().WriteLn("<end><cbr>========================================");
    Console().WriteLn("<end><cbr>Sandbox Interface Diagnostic Tool");
    Console().WriteLn("<end><cbr>========================================\n");

    // Initialize module
    Module = new SandboxModule;
    SandboxProcess proc;
    SandboxInterface iface;

    bool dynamic = false;
    unsigned flags = 0;
   
    Console().WriteLn("<end><cbr>Launching interface...");
    
    if (!iface.Launch(proc, nullptr, dynamic, flags)) {
        fputs("Launch() returned false\n", stderr);
        return 1;
    }

    Console().WriteLn("<end><cbr>Showing interface...");
    iface.Show();

    // Get root widget
    QWidget* rootWidget = g_lastTopLevel->widget;
    
    if (!rootWidget) {
        fputs("Error: No root widget!\n", stderr);
        return 1;
    }

    Console().WriteLn("<end><cbr>Root widget obtained: " + 
                      String(rootWidget->metaObject()->className()));
    
    // Process events to ensure layout is complete
    Console().WriteLn("<end><cbr>Processing events...");
    QApplication::processEvents();
    QThread::msleep(200);
    QApplication::processEvents();
    
    // Analyze what we have
    qDebug() << "\n========================================";
    qDebug() << "INITIAL ROOT WIDGET ANALYSIS";
    qDebug() << "========================================";
    qDebug() << "Type:" << rootWidget->metaObject()->className();
    qDebug() << "Name:" << rootWidget->objectName();
    qDebug() << "Size:" << rootWidget->size();
    qDebug() << "Has Layout:" << (rootWidget->layout() ? "YES" : "NO");
    
    if (rootWidget->layout()) {
        qDebug() << "Layout Type:" << rootWidget->layout()->metaObject()->className();
        qDebug() << "Layout Items:" << rootWidget->layout()->count();
    }
    
    qDebug() << "\nWidget Tree:";
    analyzeWidget(rootWidget, 0);
    
    // Count widgets
    countWidgets(rootWidget);
    
    // Try to find better root
    qDebug() << "\nSearching for better root widget...";
    QWidget* betterRoot = findBetterRoot(rootWidget);
    
    if (betterRoot != rootWidget) {
        Console().WriteLn("<end><cbr><br>Found better root!");
        Console().WriteLn("<end><cbr>  Type: " + 
                          String(betterRoot->metaObject()->className()));
        
        if (betterRoot->layout()) {
            Console().WriteLn("<end><cbr>  Layout: " + 
                              String(betterRoot->layout()->metaObject()->className()) +
                              " with " + String(betterRoot->layout()->count()) + " items");
        }
        
        qDebug() << "\n========================================";
        qDebug() << "BETTER ROOT WIDGET ANALYSIS";
        qDebug() << "========================================";
        analyzeWidget(betterRoot, 0);
        countWidgets(betterRoot);
        
        // Export from better root
        Console().WriteLn("<end><cbr><br>Exporting from better root...");
        ExportHelper::exportInterface(betterRoot, "SandboxDialog_Full", "./exported");
    }
    
    // Also export from original root for comparison
    Console().WriteLn("<end><cbr><br>Exporting from original root...");
    ExportHelper::exportInterface(rootWidget, "SandboxDialog_Original", "./exported");
    
    Console().WriteLn("<end><cbr><br>========================================");
    Console().WriteLn("<end><cbr>Diagnostic complete!");
    Console().WriteLn("<end><cbr>========================================\n");
    
    Console().WriteLn("<end><cbr>Check ./exported/ for generated files.");
    Console().WriteLn("<end><cbr>Compare SandboxDialog_Full vs SandboxDialog_Original\n");
    
    // Run event loop
    return app.exec();
}
