#include "ExportHelper.h"
#include "QtUiExporter.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QLibraryInfo>

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
    
    // Generate CMakeLists.txt
    QString cmakePath = dir.filePath("CMakeLists.txt");
    QFile cmakeFile(cmakePath);
    if (!cmakeFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "ExportHelper: cannot write" << cmakePath;
        return false;
    }
    QTextStream cmakeOut(&cmakeFile);
    generateCMakeLists(cmakeOut, baseName);
    cmakeFile.close();
    qInfo() << "Exported:" << cmakePath;
    
    // Generate example main.cpp
    QString mainPath = dir.filePath("main.cpp");
    QFile mainFile(mainPath);
    if (!mainFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "ExportHelper: cannot write" << mainPath;
        return false;
    }
    QTextStream mainOut(&mainFile);
    generateMainCpp(mainOut, baseName);
    mainFile.close();
    qInfo() << "Exported:" << mainPath;
    
    // Generate README
    QString readmePath = dir.filePath("README.md");
    QFile readmeFile(readmePath);
    if (!readmeFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "ExportHelper: cannot write" << readmePath;
        return false;
    }
    QTextStream readmeOut(&readmeFile);
    generateReadme(readmeOut, baseName, outputDir);
    readmeFile.close();
    qInfo() << "Exported:" << readmePath;
    
    qInfo() << "===========================================";
    qInfo() << "Export completed successfully!";
    qInfo() << "Generated files:";
    qInfo() << "  " << headerPath;
    qInfo() << "  " << sourcePath;
    qInfo() << "  " << metadataPath;
    qInfo() << "  " << cmakePath;
    qInfo() << "  " << mainPath;
    qInfo() << "  " << readmePath;
    qInfo() << "===========================================";
    
    return true;
}

void ExportHelper::generateCMakeLists(QTextStream& out, const QString& baseName)
{
    QString qtPath = detectQtPath();
    
    out << "cmake_minimum_required(VERSION 3.16)\n";
    out << "project(" << baseName << ")\n\n";
    
    out << "# C++ Standard\n";
    out << "set(CMAKE_CXX_STANDARD 17)\n";
    out << "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n\n";
    
    out << "# Qt Configuration\n";
    out << "set(CMAKE_AUTOMOC ON)\n";
    out << "set(CMAKE_AUTORCC ON)\n";
    out << "set(CMAKE_AUTOUIC ON)\n\n";
    
    out << "# Qt Path Configuration\n";
    if (!qtPath.isEmpty()) {
        out << "# Auto-detected Qt path:\n";
        out << "set(CMAKE_PREFIX_PATH \"" << qtPath << "\" CACHE PATH \"Qt installation path\")\n\n";
    } else {
        out << "# Set your Qt installation path:\n";
        out << "# set(CMAKE_PREFIX_PATH \"/usr/local/Qt-5.15.2\" CACHE PATH \"Qt installation path\")\n";
        out << "# set(CMAKE_PREFIX_PATH \"/opt/homebrew/Cellar/qt@5/5.15.17_1\" CACHE PATH \"Qt installation path (Homebrew)\")\n\n";
    }
    
    out << "# Alternative paths (uncomment if needed):\n";
    out << "# macOS Homebrew: /opt/homebrew/Cellar/qt@5/5.15.17_1\n";
    out << "# macOS Qt.io: /usr/local/Qt-5.15.2/clang_64\n";
    out << "# Linux: /opt/Qt/5.15.2/gcc_64\n";
    out << "# Windows: C:/Qt/5.15.2/msvc2019_64\n\n";
    
    out << "# Find Qt5\n";
    out << "find_package(Qt5 REQUIRED COMPONENTS\n";
    out << "    Core\n";
    out << "    Widgets\n";
    out << ")\n\n";
    
    out << "# Check Qt version\n";
    out << "message(STATUS \"Found Qt version: ${Qt5_VERSION}\")\n";
    out << "message(STATUS \"Qt location: ${Qt5_DIR}\")\n\n";
    
    out << "# Source files\n";
    out << "set(SOURCES\n";
    out << "    main.cpp\n";
    out << "    " << baseName << ".cpp\n";
    out << ")\n\n";
    
    out << "set(HEADERS\n";
    out << "    " << baseName << ".h\n";
    out << ")\n\n";
    
    out << "# Executable\n";
    out << "add_executable(${PROJECT_NAME}\n";
    out << "    ${SOURCES}\n";
    out << "    ${HEADERS}\n";
    out << ")\n\n";
    
    out << "# Link Qt libraries\n";
    out << "target_link_libraries(${PROJECT_NAME}\n";
    out << "    Qt5::Core\n";
    out << "    Qt5::Widgets\n";
    out << ")\n\n";
    
    out << "# Platform-specific settings\n";
    out << "if(APPLE)\n";
    out << "    set_target_properties(${PROJECT_NAME} PROPERTIES\n";
    out << "        MACOSX_BUNDLE FALSE\n";
    out << "    )\n";
    out << "endif()\n\n";
    
    out << "if(WIN32)\n";
    out << "    set_target_properties(${PROJECT_NAME} PROPERTIES\n";
    out << "        WIN32_EXECUTABLE FALSE\n";
    out << "    )\n";
    out << "endif()\n\n";
    
    out << "# Installation\n";
    out << "install(TARGETS ${PROJECT_NAME}\n";
    out << "    RUNTIME DESTINATION bin\n";
    out << ")\n\n";
    
    out << "# Build instructions:\n";
    out << "# mkdir build && cd build\n";
    out << "# cmake ..\n";
    out << "# make (or cmake --build .)\n";
    out << "# ./${PROJECT_NAME}\n";
}

QString ExportHelper::detectQtPath()
{
    // Get Qt library path from the currently running Qt
    QString qtLibPath = QLibraryInfo::location(QLibraryInfo::LibrariesPath);
    
    if (qtLibPath.isEmpty()) {
        return QString();
    }
    
    // Convert library path to CMake prefix path
    // e.g., /opt/homebrew/Cellar/qt@5/5.15.17_1/lib -> /opt/homebrew/Cellar/qt@5/5.15.17_1
    QDir libDir(qtLibPath);
    if (libDir.cdUp()) {
        QString prefixPath = libDir.absolutePath();
        
        // Verify it's a valid Qt installation by checking for lib/cmake
        QDir cmakeDir(prefixPath + "/lib/cmake");
        if (cmakeDir.exists()) {
            return prefixPath;
        }
    }
    
    // Alternative: try to get from Qt core location
    QString qtCorePath = QLibraryInfo::location(QLibraryInfo::PrefixPath);
    if (!qtCorePath.isEmpty()) {
        QDir cmakeDir(qtCorePath + "/lib/cmake");
        if (cmakeDir.exists()) {
            return qtCorePath;
        }
    }
    
    return QString();
}

void ExportHelper::generateMainCpp(QTextStream& out, const QString& baseName)
{
    out << "#include <QApplication>\n";
    out << "#include \"" << baseName << ".h\"\n\n";
    
    out << "int main(int argc, char** argv)\n";
    out << "{\n";
    out << "    QApplication app(argc, argv);\n\n";
    
    out << "    " << baseName << " dialog;\n";
    out << "    dialog.setWindowTitle(\"" << baseName << "\");\n";
    out << "    dialog.show();\n\n";
    
    out << "    return app.exec();\n";
    out << "}\n";
}

void ExportHelper::generateReadme(QTextStream& out, const QString& baseName, 
                                   const QString& outputDir)
{
    out << "# " << baseName << "\n\n";
    out << "Exported Qt interface from PCL PixInsight module.\n\n";
    
    out << "## Files Generated\n\n";
    out << "- `" << baseName << ".h` - Header file with class declaration\n";
    out << "- `" << baseName << ".cpp` - Implementation file\n";
    out << "- `" << baseName << "_metadata.json` - Widget metadata\n";
    out << "- `CMakeLists.txt` - CMake build configuration\n";
    out << "- `main.cpp` - Example application\n";
    out << "- `README.md` - This file\n\n";
    
    out << "## Build Instructions\n\n";
    
    out << "### Prerequisites\n\n";
    out << "- Qt 5.15+ or Qt 6.x\n";
    out << "- CMake 3.16+\n";
    out << "- C++17 compatible compiler\n\n";
    
    out << "### macOS (Homebrew Qt)\n\n";
    out << "```bash\n";
    out << "# Set Qt path\n";
    out << "export CMAKE_PREFIX_PATH=/opt/homebrew/Cellar/qt@5/5.15.17_1\n\n";
    out << "# Or add to CMakeLists.txt:\n";
    out << "# set(CMAKE_PREFIX_PATH \"/opt/homebrew/Cellar/qt@5/5.15.17_1\")\n\n";
    out << "# Build\n";
    out << "mkdir build && cd build\n";
    out << "cmake ..\n";
    out << "make\n\n";
    out << "# Run\n";
    out << "./" << baseName << "\n";
    out << "```\n\n";
    
    out << "### macOS (Qt from qt.io)\n\n";
    out << "```bash\n";
    out << "mkdir build && cd build\n";
    out << "cmake .. -DCMAKE_PREFIX_PATH=/usr/local/Qt-5.15.2\n";
    out << "make\n";
    out << "./" << baseName << "\n";
    out << "```\n\n";
    
    out << "### Linux\n\n";
    out << "```bash\n";
    out << "mkdir build && cd build\n";
    out << "cmake ..\n";
    out << "make\n";
    out << "./" << baseName << "\n";
    out << "```\n\n";
    
    out << "### Windows\n\n";
    out << "```cmd\n";
    out << "mkdir build\n";
    out << "cd build\n";
    out << "cmake .. -DCMAKE_PREFIX_PATH=C:\\Qt\\5.15.2\\msvc2019_64\n";
    out << "cmake --build . --config Release\n";
    out << "Release\\" << baseName << ".exe\n";
    out << "```\n\n";
    
    out << "## Troubleshooting\n\n";
    out << "### Qt not found\n\n";
    out << "Set `CMAKE_PREFIX_PATH` to your Qt installation:\n\n";
    out << "```bash\n";
    out << "cmake .. -DCMAKE_PREFIX_PATH=/path/to/Qt/5.15.2/gcc_64\n";
    out << "```\n\n";
    
    out << "### Find Qt on macOS (Homebrew)\n\n";
    out << "```bash\n";
    out << "brew --prefix qt@5\n";
    out << "# Use the output path in CMAKE_PREFIX_PATH\n";
    out << "```\n\n";
    
    out << "## Widget Structure\n\n";
    out << "See `" << baseName << "_metadata.json` for complete widget inventory.\n\n";
    
    out << "## Integration\n\n";
    out << "To use this dialog in your application:\n\n";
    out << "```cpp\n";
    out << "#include \"" << baseName << ".h\"\n\n";
    out << "" << baseName << " dialog;\n";
    out << "dialog.exec();  // Modal\n";
    out << "// or\n";
    out << "dialog.show();  // Non-modal\n";
    out << "```\n\n";
    
    out << "## License\n\n";
    out << "Generated code follows the same license as your PixInsight module.\n";
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
