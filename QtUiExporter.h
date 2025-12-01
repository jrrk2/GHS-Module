#pragma once

#include <QWidget>
#include <QLayout>
#include <QTextStream>
#include <QMap>
#include <QSet>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QSlider>
#include <QCheckBox>
#include <QRadioButton>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QToolButton>
#include <QGroupBox>
#include <QTabWidget>
#include <QTreeWidget>
#include <QListWidget>
#include <QTableWidget>
#include <QProgressBar>
#include <QScrollBar>
#include <QDial>

#include <QBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QStackedLayout>
#include <QJsonDocument>
#include <QApplication>
#include <QDir>
#include <QFile>

struct QtUiExportOptions
{
    QString className = "GeneratedDialog";
    QString baseClass = "QWidget";
    QString rootVariable = "this";
    bool useMemberPointers = true;
    bool generateSignalsSlots = true;
    bool addComments = true;
    bool exportMetadata = true;  // Export JSON metadata about PCL->Qt mapping
};

/**
 * Enhanced Qt UI Exporter with comprehensive PCL widget type support
 * 
 * Generates Qt C++ code from existing Qt widget trees created by PCL Mock API.
 * Supports all common PCL widget types with proper property preservation.
 */
class QtUiExporterEnhanced
{
public:
    explicit QtUiExporterEnhanced(QWidget* rootWidget, const QtUiExportOptions& opt);

    // Main export methods
    void writeHeader(QTextStream& out);
    void writeSource(QTextStream& out);
    void writeMetadata(QTextStream& out);  // Export JSON metadata

    // Helper: Find the top-level interface window from any child
    static QWidget* findInterfaceRoot(QWidget* anyWidget);

private:
    QWidget* m_root;
    QtUiExportOptions m_opt;

    struct VarInfo {
        QString typeName;       // Qt type: "QLabel", "QSpinBox", etc.
        QString pclTypeName;    // PCL type hint if available
        QString varName;        // C++ variable name
        QString objectName;     // Qt objectName
        bool isLayout = false;
        bool isComposite = false;  // True for NumericControl-like composites
        QStringList properties; // List of property setter calls
    };

    QMap<const QObject*, VarInfo> m_vars;
    QMap<QString, int> m_typeCounters;
    QSet<QString> m_requiredIncludes;
    QJsonObject m_metadata;

    // Analysis and variable assignment
    void assignVariableNames(QObject* obj);
    void assignVariableNamesRec(QObject* obj);
    void detectPCLWidgetType(QWidget* w, VarInfo& info);
    void detectCompositeWidgets(QWidget* w);
    
    // Code generation helpers
    QString indent(int level) const;
    QString makeVarName(const QObject* obj, const QString& typeName);
    QString qtTypeName(const QObject* obj) const;
    QString toCamelCase(const QString& input) const;
    
    // Member collection
    void collectMembers(QStringList& memberLines);
    void collectSignalsSlots(QStringList& slotDecls);
    
    // Widget generation
    void genWidgetCtor(QWidget* w, QTextStream& out, int level);
    void genLayoutCtor(QLayout* l, QTextStream& out, int level, 
                      const QString& parentWidgetVar);
    void handleLayoutChildren(QLayout* layout, const QString& layoutVarName,
                            QTextStream& out, int level);
    
    // Property emission
    void emitWidgetProperties(QWidget* w, const QString& varName, 
                            QTextStream& out, int level);
    void emitSpecificWidgetProps(QWidget* w, const QString& varName,
                               QTextStream& out, int level);
    void emitLayoutProperties(QLayout* layout, const QString& varName,
                            QTextStream& out, int level);
    
    // Signal/Slot generation
    void generateSignalConnections(QTextStream& out, int level);
    QString mapWidgetToSignal(const QString& widgetType) const;
    QString mapWidgetToSlot(const QString& widgetType) const;
    
    // Metadata generation
    void buildMetadata();
    QJsonObject widgetToJson(QWidget* w) const;
    QJsonObject layoutToJson(QLayout* l) const;
    
    // Include management
    void addRequiredInclude(const QString& className);
    void writeIncludes(QTextStream& out);
};

/**
 * Helper class to detect NumericControl composite patterns
 * PCL's NumericControl consists of: Label + Slider + LineEdit
 */
class NumericControlDetector
{
public:
    struct NumericControlInfo {
        QLabel* label = nullptr;
        QSlider* slider = nullptr;
        QLineEdit* edit = nullptr;
        QWidget* container = nullptr;
        QString baseName;
        bool isValid() const { return label && slider && edit; }
    };
    
    static NumericControlInfo detectNumericControl(QWidget* widget);
    static bool isNumericControlContainer(QWidget* widget);
};

/**
 * Utility to find and export all ProcessInterface widgets in an application
 */
class InterfaceExporter
{
public:
    // Export all top-level ProcessInterface windows
    static void exportAllInterfaces(const QString& outputDir);
    
    // Export a specific interface by name
    static bool exportInterface(QWidget* interface, const QString& outputDir,
                              const QString& baseName);
    
    // Find all ProcessInterface instances in the application
    static QList<QWidget*> findAllInterfaces();
};
