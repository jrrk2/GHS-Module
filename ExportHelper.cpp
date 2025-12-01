#include "ExportHelper.h"
#include "QtUiExporter.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>

bool ExportHelper::exportInterface(QWidget* widget,
                                   const QString& baseName,
                                   const QString& outputDir)
{
    return exportInterfaceCustom(widget, baseName, outputDir, true, true, "QWidget");
}

bool ExportHelper::exportInterfaceCustom(QWidget* widget,
                                        const QString& baseName,
                                        const QString& outputDir,
                                        bool generateSignals,
                                        bool addComments,
                                        const QString& baseClass)
{
    if (!widget) {
        qWarning() << "ExportHelper: null widget";
        return false;
    }
    
    // Create output directory
    QDir dir(outputDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning() << "ExportHelper: cannot create directory" << outputDir;
            return false;
        }
    }
    
    // Configure export options
    QtUiExportOptions opt;
    opt.className = baseName;
    opt.baseClass = baseClass;
    opt.rootVariable = "this";
    opt.useMemberPointers = true;
    opt.generateSignalsSlots = generateSignals;
    opt.addComments = addComments;
    opt.exportMetadata = true;
    
    // Create exporter
    QtUiExporterEnhanced exporter(widget, opt);
    
    // Export header file
    QString headerPath = dir.filePath(baseName + ".h");
    QFile hFile(headerPath);
    if (!hFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "ExportHelper: cannot write" << headerPath;
        return false;
    }
    QTextStream hOut(&hFile);
    exporter.writeHeader(hOut);
    hFile.close();
    qInfo() << "Exported:" << headerPath;
    
    // Export source file
    QString sourcePath = dir.filePath(baseName + ".cpp");
    QFile cppFile(sourcePath);
    if (!cppFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "ExportHelper: cannot write" << sourcePath;
        return false;
    }
    QTextStream cppOut(&cppFile);
    exporter.writeSource(cppOut);
    cppFile.close();
    qInfo() << "Exported:" << sourcePath;
    
    // Export metadata JSON
    QString metadataPath = dir.filePath(baseName + "_metadata.json");
    QFile jsonFile(metadataPath);
    if (!jsonFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "ExportHelper: cannot write" << metadataPath;
        return false;
    }
    QTextStream jsonOut(&jsonFile);
    exporter.writeMetadata(jsonOut);
    jsonFile.close();
    qInfo() << "Exported:" << metadataPath;
    
    qInfo() << "===========================================";
    qInfo() << "Export completed successfully!";
    qInfo() << "Generated files:";
    qInfo() << "  " << headerPath;
    qInfo() << "  " << sourcePath;
    qInfo() << "  " << metadataPath;
    qInfo() << "===========================================";
    
    return true;
}

bool ExportHelper::quickExport(QWidget* widget)
{
    if (!widget) {
        qWarning() << "ExportHelper::quickExport: null widget";
        return false;
    }
    
    QString baseName = widget->objectName();
    if (baseName.isEmpty()) {
        baseName = "ExportedInterface";
    }
    
    return exportInterface(widget, baseName, "./exported");
}
