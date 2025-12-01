// ExportHelper.h - Convenience functions for exporting PCL interfaces to Qt

#pragma once

#include <QWidget>
#include <QString>
#include <QTextStream>

/**
 * Convenience helper to export a PCL interface to Qt C++ code
 * 
 * Usage in your main():
 *    ExportHelper::exportInterface(g_lastTopLevel->widget, "MyDialog");
 */
class ExportHelper
{
public:
    /**
     * Export a single interface widget to Qt code
     * 
     * @param widget The root widget (typically from g_lastTopLevel->widget)
     * @param baseName Base name for generated files (e.g., "SandboxDialog")
     * @param outputDir Directory to write files (default: "./exported")
     * @return true if export succeeded
     */
    static bool exportInterface(QWidget* widget, 
                               const QString& baseName,
                               const QString& outputDir = "./exported");
    
    /**
     * Export with custom options
     */
    static bool exportInterfaceCustom(QWidget* widget,
                                     const QString& baseName,
                                     const QString& outputDir,
                                     bool generateSignals = true,
                                     bool addComments = true,
                                     const QString& baseClass = "QWidget");
    
    /**
     * Quick export for testing - uses default names
     */
    static bool quickExport(QWidget* widget);

private:
    static void generateCMakeLists(QTextStream& out, const QString& baseName);
    static void generateMainCpp(QTextStream& out, const QString& baseName);
    static void generateReadme(QTextStream& out, const QString& baseName, const QString& outputDir);
    static QString detectQtPath();
};
