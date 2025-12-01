#include "QtUiExporter.h"

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

// ============================================================================
// QtUiExporterEnhanced Implementation
// ============================================================================

QtUiExporterEnhanced::QtUiExporterEnhanced(QWidget* rootWidget, 
                                           const QtUiExportOptions& opt)
    : m_root(rootWidget)
    , m_opt(opt)
{
    if (!m_root) {
        qWarning("QtUiExporterEnhanced: null root widget");
        return;
    }
    
    // Detect composite widgets first
    detectCompositeWidgets(m_root);
    
    // Assign variable names to all widgets and layouts
    assignVariableNames(m_root);
    
    // Build metadata if requested
    if (m_opt.exportMetadata) {
        buildMetadata();
    }
}

QString QtUiExporterEnhanced::indent(int level) const
{
    return QString(level * 4, ' ');
}

QString QtUiExporterEnhanced::qtTypeName(const QObject* obj) const
{
    return QString::fromLatin1(obj->metaObject()->className());
}

QString QtUiExporterEnhanced::toCamelCase(const QString& input) const
{
    if (input.isEmpty()) return input;
    
    QString result = input;
    // Handle underscores: parameter_one -> parameterOne
    if (result.contains('_')) {
        QStringList parts = result.split('_');
        result = parts[0].toLower();
        for (int i = 1; i < parts.size(); ++i) {
            if (!parts[i].isEmpty()) {
                result += parts[i][0].toUpper() + parts[i].mid(1).toLower();
            }
        }
    } else {
        result[0] = result[0].toLower();
    }
    
    return result;
}

QString QtUiExporterEnhanced::makeVarName(const QObject* obj, 
                                          const QString& typeName)
{
    QString base;

    // Prefer objectName if set
    if (!obj->objectName().isEmpty()) {
        base = toCamelCase(obj->objectName());
    } else {
        base = typeName;
        // Strip Q prefix for standard Qt types
        if (base.startsWith('Q') && base.length() > 1) {
            base = base.mid(1);
        }
        base[0] = base[0].toLower();
    }

    // Ensure uniqueness
    int& counter = m_typeCounters[base];
    ++counter;
    if (counter > 1) {
        base += QString::number(counter);
    }

    return base;
}

void QtUiExporterEnhanced::detectPCLWidgetType(QWidget* w, VarInfo& info)
{
    // Try to infer PCL widget type from Qt widget and objectName
    QString objName = w->objectName();
    
    if (qobject_cast<QLabel*>(w)) {
        info.pclTypeName = "Label";
        if (objName.contains("Label", Qt::CaseInsensitive)) {
            info.pclTypeName = "Label";
        }
    }
    else if (qobject_cast<QLineEdit*>(w)) {
        info.pclTypeName = "Edit";
        if (objName.contains("Edit", Qt::CaseInsensitive)) {
            info.pclTypeName = "Edit";
        }
    }
    else if (qobject_cast<QTextEdit*>(w) || qobject_cast<QPlainTextEdit*>(w)) {
        info.pclTypeName = "TextBox";
    }
    else if (qobject_cast<QSpinBox*>(w)) {
        info.pclTypeName = "SpinBox";
    }
    else if (qobject_cast<QDoubleSpinBox*>(w)) {
        info.pclTypeName = "NumericEdit";
    }
    else if (qobject_cast<QCheckBox*>(w)) {
        info.pclTypeName = "CheckBox";
    }
    else if (qobject_cast<QRadioButton*>(w)) {
        info.pclTypeName = "RadioButton";
    }
    else if (qobject_cast<QComboBox*>(w)) {
        info.pclTypeName = "ComboBox";
    }
    else if (qobject_cast<QPushButton*>(w)) {
        info.pclTypeName = "PushButton";
    }
    else if (qobject_cast<QToolButton*>(w)) {
        info.pclTypeName = "ToolButton";
    }
    else if (qobject_cast<QSlider*>(w)) {
        info.pclTypeName = "Slider";
    }
    else if (qobject_cast<QGroupBox*>(w)) {
        info.pclTypeName = "GroupBox";
    }
    else if (qobject_cast<QTabWidget*>(w)) {
        info.pclTypeName = "TabBox";
    }
    else if (qobject_cast<QTreeWidget*>(w)) {
        info.pclTypeName = "TreeBox";
    }
    else if (qobject_cast<QScrollBar*>(w)) {
        info.pclTypeName = "ScrollBox";
    }
    
    // Detect NumericControl pattern
    if (objName.contains("NumericControl", Qt::CaseInsensitive) ||
        objName.contains("Numeric_Control", Qt::CaseInsensitive)) {
        info.pclTypeName = "NumericControl";
        info.isComposite = true;
    }
}

void QtUiExporterEnhanced::detectCompositeWidgets(QWidget* w)
{
    // Detect NumericControl composite pattern
    auto ncInfo = NumericControlDetector::detectNumericControl(w);
    if (ncInfo.isValid()) {
        // Mark this as a composite widget
        w->setProperty("_pclComposite", "NumericControl");
        w->setProperty("_pclBaseName", ncInfo.baseName);
    }
    
    // Recursively check children
    for (QObject* child : w->children()) {
        if (auto* childWidget = qobject_cast<QWidget*>(child)) {
            detectCompositeWidgets(childWidget);
        }
    }
}

void QtUiExporterEnhanced::assignVariableNames(QObject* obj)
{
    assignVariableNamesRec(obj);
}

void QtUiExporterEnhanced::assignVariableNamesRec(QObject* obj)
{
    if (!obj) return;

    if (auto* w = qobject_cast<QWidget*>(obj)) {
        QString type = qtTypeName(w);
        
        if (w != m_root) {
            VarInfo info;
            info.typeName = type;
            info.objectName = w->objectName();
            info.isLayout = false;
            
            // Detect PCL type
            detectPCLWidgetType(w, info);
            
            info.varName = makeVarName(w, type);
            m_vars.insert(w, info);
            
            // Track required includes
            addRequiredInclude(type);
        }

        // Process layout
        if (auto* layout = w->layout()) {
            QString lType = qtTypeName(layout);
            VarInfo linfo;
            linfo.typeName = lType;
            linfo.varName = makeVarName(layout, lType);
            linfo.isLayout = true;
            m_vars.insert(layout, linfo);
            
            addRequiredInclude(lType);
            assignVariableNamesRec(layout);
        }
    }
    else if (auto* layout = qobject_cast<QLayout*>(obj)) {
        // Process layout children
        for (int i = 0; i < layout->count(); ++i) {
            QLayoutItem* item = layout->itemAt(i);
            if (QWidget* w = item->widget())
                assignVariableNamesRec(w);
            else if (QLayout* l = item->layout())
                assignVariableNamesRec(l);
        }
    }
}

void QtUiExporterEnhanced::addRequiredInclude(const QString& className)
{
    // Map Qt class names to headers
    static const QMap<QString, QString> includeMap = {
        {"QLabel", "#include <QLabel>"},
        {"QLineEdit", "#include <QLineEdit>"},
        {"QTextEdit", "#include <QTextEdit>"},
        {"QPlainTextEdit", "#include <QPlainTextEdit>"},
        {"QSpinBox", "#include <QSpinBox>"},
        {"QDoubleSpinBox", "#include <QDoubleSpinBox>"},
        {"QCheckBox", "#include <QCheckBox>"},
        {"QRadioButton", "#include <QRadioButton>"},
        {"QComboBox", "#include <QComboBox>"},
        {"QPushButton", "#include <QPushButton>"},
        {"QToolButton", "#include <QToolButton>"},
        {"QSlider", "#include <QSlider>"},
        {"QGroupBox", "#include <QGroupBox>"},
        {"QTabWidget", "#include <QTabWidget>"},
        {"QTreeWidget", "#include <QTreeWidget>"},
        {"QListWidget", "#include <QListWidget>"},
        {"QTableWidget", "#include <QTableWidget>"},
        {"QScrollBar", "#include <QScrollBar>"},
        {"QProgressBar", "#include <QProgressBar>"},
        {"QVBoxLayout", "#include <QVBoxLayout>"},
        {"QHBoxLayout", "#include <QHBoxLayout>"},
        {"QGridLayout", "#include <QGridLayout>"},
        {"QFormLayout", "#include <QFormLayout>"},
        {"QStackedLayout", "#include <QStackedLayout>"}
    };
    
    if (includeMap.contains(className)) {
        m_requiredIncludes.insert(includeMap[className]);
    }
}

void QtUiExporterEnhanced::writeIncludes(QTextStream& out)
{
    QStringList sorted = m_requiredIncludes.values();
    sorted.sort();
    for (const QString& inc : sorted) {
        out << inc << "\n";
    }
}

void QtUiExporterEnhanced::collectMembers(QStringList& memberLines)
{
    if (m_opt.addComments) {
        memberLines << "";
        memberLines << "    // === Layouts ===";
    }
    
    // Collect layouts
    for (auto it = m_vars.cbegin(); it != m_vars.cend(); ++it) {
        const VarInfo& v = it.value();
        if (!v.isLayout) continue;
        
        if (m_opt.useMemberPointers) {
            QString line = QString("    %1* %2;").arg(v.typeName, v.varName);
            if (m_opt.addComments && !v.objectName.isEmpty()) {
                line += QString(" // objectName: %1").arg(v.objectName);
            }
            memberLines << line;
        }
    }
    
    if (m_opt.addComments) {
        memberLines << "";
        memberLines << "    // === Widgets ===";
    }
    
    // Collect widgets
    for (auto it = m_vars.cbegin(); it != m_vars.cend(); ++it) {
        const VarInfo& v = it.value();
        if (v.isLayout) continue;
        
        if (m_opt.useMemberPointers) {
            QString line = QString("    %1* %2;").arg(v.typeName, v.varName);
            if (m_opt.addComments) {
                if (!v.pclTypeName.isEmpty()) {
                    line += QString(" // PCL: %1").arg(v.pclTypeName);
                }
                if (!v.objectName.isEmpty() && v.objectName != v.varName) {
                    line += QString(" [%1]").arg(v.objectName);
                }
            }
            memberLines << line;
        }
    }
}

void QtUiExporterEnhanced::collectSignalsSlots(QStringList& slotDecls)
{
    if (!m_opt.generateSignalsSlots) return;
    
    QSet<QString> addedSlots;
    
    for (auto it = m_vars.cbegin(); it != m_vars.cend(); ++it) {
        const VarInfo& v = it.value();
        if (v.isLayout) continue;
        
        QString slotName = mapWidgetToSlot(v.typeName);
        if (!slotName.isEmpty() && !addedSlots.contains(slotName)) {
            // Determine slot signature
            if (v.typeName.contains("SpinBox") || v.typeName.contains("Slider")) {
                slotDecls << QString("    void %1(int value);").arg(slotName);
            } else if (v.typeName == "QDoubleSpinBox") {
                slotDecls << QString("    void %1(double value);").arg(slotName);
            } else if (v.typeName.contains("CheckBox") || 
                      v.typeName.contains("RadioButton")) {
                slotDecls << QString("    void %1(bool checked);").arg(slotName);
            } else if (v.typeName == "QComboBox") {
                slotDecls << QString("    void %1(int index);").arg(slotName);
            } else if (v.typeName.contains("LineEdit") || 
                      v.typeName.contains("TextEdit")) {
                slotDecls << QString("    void %1();").arg(slotName);
            } else if (v.typeName.contains("Button")) {
                slotDecls << QString("    void %1();").arg(slotName);
            }
            
            addedSlots.insert(slotName);
        }
    }
}

void QtUiExporterEnhanced::writeHeader(QTextStream& out)
{
    QString guard = m_opt.className.toUpper() + "_H";
    
    out << "#ifndef " << guard << "\n";
    out << "#define " << guard << "\n\n";
    
    if (m_opt.addComments) {
        out << "// ============================================================================\n";
        out << "// " << m_opt.className << ".h\n";
        out << "// Generated from PCL Mock API Qt widget tree\n";
        out << "// ============================================================================\n\n";
    }
    
    out << "#include <" << m_opt.baseClass << ">\n";
    out << "#include <QPointer>\n\n";
    
    // Forward declarations
    out << "// Forward declarations\n";
    out << "class QVBoxLayout;\n";
    out << "class QHBoxLayout;\n";
    out << "class QGridLayout;\n";
    out << "class QLabel;\n";
    out << "class QLineEdit;\n";
    out << "class QSpinBox;\n";
    out << "class QCheckBox;\n";
    out << "class QComboBox;\n";
    out << "class QPushButton;\n\n";
    
    out << "class " << m_opt.className << " : public " << m_opt.baseClass << "\n";
    out << "{\n";
    out << "    Q_OBJECT\n\n";
    out << "public:\n";
    out << "    explicit " << m_opt.className << "(QWidget* parent = nullptr);\n";
    out << "    virtual ~" << m_opt.className << "();\n\n";
    
    if (m_opt.addComments) {
        out << "    // Public interface\n";
    }
    out << "    void updateControls();\n\n";
    
    out << "private:\n";
    out << "    void setupUi();\n";
    
    if (m_opt.generateSignalsSlots) {
        out << "    void connectSignals();\n";
    }
    
    QStringList members;
    collectMembers(members);
    if (!members.isEmpty()) {
        for (const QString& line : members) {
            out << line << "\n";
        }
    }
    
    if (m_opt.generateSignalsSlots) {
        out << "\nprivate slots:\n";
        if (m_opt.addComments) {
            out << "    // Event handlers\n";
        }
        QStringList _slots;
        collectSignalsSlots(_slots);
        for (const QString& slot : _slots) {
            out << slot << "\n";
        }
    }
    
    out << "};\n\n";
    out << "#endif // " << guard << "\n";
}

void QtUiExporterEnhanced::writeSource(QTextStream& out)
{
    if (m_opt.addComments) {
        out << "// ============================================================================\n";
        out << "// " << m_opt.className << ".cpp\n";
        out << "// Generated from PCL Mock API Qt widget tree\n";
        out << "// ============================================================================\n\n";
    }
    
    out << "#include \"" << m_opt.className << ".h\"\n\n";
    
    // Write includes
    writeIncludes(out);
    out << "\n";
    
    // Constructor
    out << m_opt.className << "::" << m_opt.className << "(QWidget* parent)\n";
    out << "    : " << m_opt.baseClass << "(parent)\n";
    out << "{\n";
    out << indent(1) << "setupUi();\n";
    if (m_opt.generateSignalsSlots) {
        out << indent(1) << "connectSignals();\n";
    }
    out << "}\n\n";
    
    // Destructor
    out << m_opt.className << "::~" << m_opt.className << "()\n";
    out << "{\n";
    out << indent(1) << "// Cleanup handled by Qt parent-child relationship\n";
    out << "}\n\n";
    
    // setupUi method
    out << "void " << m_opt.className << "::setupUi()\n";
    out << "{\n";
    
    if (m_root->layout()) {
        genLayoutCtor(m_root->layout(), out, 1, m_opt.rootVariable);
    } else {
        if (m_opt.addComments) {
            out << indent(1) << "// No root layout found\n";
        }
    }
    
    out << "}\n\n";
    
    // connectSignals method
    if (m_opt.generateSignalsSlots) {
        out << "void " << m_opt.className << "::connectSignals()\n";
        out << "{\n";
        generateSignalConnections(out, 1);
        out << "}\n\n";
    }
    
    // updateControls method
    out << "void " << m_opt.className << "::updateControls()\n";
    out << "{\n";
    out << indent(1) << "// TODO: Update widget values from model\n";
    out << "}\n\n";
    
    // Slot implementations
    if (m_opt.generateSignalsSlots) {
        QSet<QString> implementedSlots;
        
        for (auto it = m_vars.cbegin(); it != m_vars.cend(); ++it) {
            const VarInfo& v = it.value();
            if (v.isLayout) continue;
            
            QString slotName = mapWidgetToSlot(v.typeName);
            if (!slotName.isEmpty() && !implementedSlots.contains(slotName)) {
                if (v.typeName.contains("SpinBox") || v.typeName.contains("Slider")) {
                    out << "void " << m_opt.className << "::" << slotName << "(int value)\n";
                } else if (v.typeName == "QDoubleSpinBox") {
                    out << "void " << m_opt.className << "::" << slotName << "(double value)\n";
                } else if (v.typeName.contains("CheckBox") || 
                          v.typeName.contains("RadioButton")) {
                    out << "void " << m_opt.className << "::" << slotName << "(bool checked)\n";
                } else if (v.typeName == "QComboBox") {
                    out << "void " << m_opt.className << "::" << slotName << "(int index)\n";
                } else {
                    out << "void " << m_opt.className << "::" << slotName << "()\n";
                }
                
                out << "{\n";
                out << indent(1) << "// TODO: Implement handler\n";
                out << "}\n\n";
                
                implementedSlots.insert(slotName);
            }
        }
    }
}

QString QtUiExporterEnhanced::mapWidgetToSignal(const QString& widgetType) const
{
    static const QMap<QString, QString> signalMap = {
        {"QSpinBox", "valueChanged"},
        {"QDoubleSpinBox", "valueChanged"},
        {"QSlider", "valueChanged"},
        {"QCheckBox", "toggled"},
        {"QRadioButton", "toggled"},
        {"QComboBox", "currentIndexChanged"},
        {"QLineEdit", "textChanged"},
        {"QTextEdit", "textChanged"},
        {"QPushButton", "clicked"},
        {"QToolButton", "clicked"}
    };
    
    return signalMap.value(widgetType, "");
}

QString QtUiExporterEnhanced::mapWidgetToSlot(const QString& widgetType) const
{
    static const QMap<QString, QString> slotMap = {
        {"QSpinBox", "onValueChanged"},
        {"QDoubleSpinBox", "onValueChanged"},
        {"QSlider", "onValueChanged"},
        {"QCheckBox", "onCheckboxToggled"},
        {"QRadioButton", "onRadioButtonToggled"},
        {"QComboBox", "onItemSelected"},
        {"QLineEdit", "onTextEdited"},
        {"QTextEdit", "onTextEdited"},
        {"QPushButton", "onButtonClicked"},
        {"QToolButton", "onButtonClicked"}
    };
    
    return slotMap.value(widgetType, "");
}

void QtUiExporterEnhanced::generateSignalConnections(QTextStream& out, int level)
{
    for (auto it = m_vars.cbegin(); it != m_vars.cend(); ++it) {
        const QObject* obj = it.key();
        const VarInfo& v = it.value();
        
        if (v.isLayout) continue;
        
        QString signal = mapWidgetToSignal(v.typeName);
        QString slot = mapWidgetToSlot(v.typeName);
        
        if (!signal.isEmpty() && !slot.isEmpty()) {
            out << indent(level)
                << "connect(" << v.varName << ", &" << v.typeName 
                << "::" << signal << ", this, &" << m_opt.className 
                << "::" << slot << ");\n";
        }
    }
}

void QtUiExporterEnhanced::genWidgetCtor(QWidget* w, QTextStream& out, int level)
{
    if (w == m_root) return;

    auto it = m_vars.constFind(w);
    if (it == m_vars.cend()) return;

    const VarInfo& v = it.value();
    QString parentName = m_opt.rootVariable;

    // Determine parent
    if (w->parentWidget() && w->parentWidget() != m_root) {
        auto pit = m_vars.constFind(w->parentWidget());
        if (pit != m_vars.cend()) {
            parentName = pit.value().varName;
        }
    }

    if (m_opt.addComments && !v.pclTypeName.isEmpty()) {
        out << indent(level) << "// PCL: " << v.pclTypeName << "\n";
    }

    out << indent(level) << v.varName << " = new " << v.typeName << "(";

    // Some widgets have text constructors
    if (auto* lbl = qobject_cast<QLabel*>(w)) {
        if (!lbl->text().isEmpty()) {
            out << "QStringLiteral(\"" << lbl->text().toHtmlEscaped() 
                << "\"), " << parentName;
        } else {
            out << parentName;
        }
    }
    else if (auto* btn = qobject_cast<QPushButton*>(w)) {
        if (!btn->text().isEmpty()) {
            out << "QStringLiteral(\"" << btn->text().toHtmlEscaped() 
                << "\"), " << parentName;
        } else {
            out << parentName;
        }
    }
    else {
        out << parentName;
    }

    out << ");\n";

    emitWidgetProperties(w, v.varName, out, level);
}

void QtUiExporterEnhanced::emitWidgetProperties(QWidget* w, const QString& varName,
                                               QTextStream& out, int level)
{
    // Object name
    if (!w->objectName().isEmpty()) {
        out << indent(level) << varName << "->setObjectName(QStringLiteral(\""
            << w->objectName() << "\"));\n";
    }
    
    // Geometry
    if (w->minimumWidth() > 0) {
        out << indent(level) << varName << "->setMinimumWidth(" 
            << w->minimumWidth() << ");\n";
    }
    if (w->minimumHeight() > 0) {
        out << indent(level) << varName << "->setMinimumHeight(" 
            << w->minimumHeight() << ");\n";
    }
    if (w->maximumWidth() < QWIDGETSIZE_MAX) {
        out << indent(level) << varName << "->setMaximumWidth(" 
            << w->maximumWidth() << ");\n";
    }
    if (w->maximumHeight() < QWIDGETSIZE_MAX) {
        out << indent(level) << varName << "->setMaximumHeight(" 
            << w->maximumHeight() << ");\n";
    }
    
    // Tooltip
    if (!w->toolTip().isEmpty()) {
        out << indent(level) << varName << "->setToolTip(QStringLiteral(\""
            << w->toolTip().toHtmlEscaped() << "\"));\n";
    }
    
    // Enabled/Visible
    if (!w->isEnabled()) {
        out << indent(level) << varName << "->setEnabled(false);\n";
    }
    
    // Widget-specific properties
    emitSpecificWidgetProps(w, varName, out, level);
}

void QtUiExporterEnhanced::emitSpecificWidgetProps(QWidget* w, const QString& varName,
                                                   QTextStream& out, int level)
{
    if (auto* edit = qobject_cast<QLineEdit*>(w)) {
        if (!edit->text().isEmpty()) {
            out << indent(level) << varName << "->setText(QStringLiteral(\""
                << edit->text().toHtmlEscaped() << "\"));\n";
        }
        if (!edit->placeholderText().isEmpty()) {
            out << indent(level) << varName << "->setPlaceholderText(QStringLiteral(\""
                << edit->placeholderText().toHtmlEscaped() << "\"));\n";
        }
    }
    else if (auto* slider = qobject_cast<QSlider*>(w)) {
        out << indent(level) << varName << "->setOrientation("
            << (slider->orientation() == Qt::Horizontal ? "Qt::Horizontal" : "Qt::Vertical")
            << ");\n";
        out << indent(level) << varName << "->setRange("
            << slider->minimum() << ", " << slider->maximum() << ");\n";
        if (slider->value() != 0) {
            out << indent(level) << varName << "->setValue(" << slider->value() << ");\n";
        }
    }
    else if (auto* cb = qobject_cast<QCheckBox*>(w)) {
        if (!cb->text().isEmpty()) {
            out << indent(level) << varName << "->setText(QStringLiteral(\""
                << cb->text().toHtmlEscaped() << "\"));\n";
        }
        if (cb->isChecked()) {
            out << indent(level) << varName << "->setChecked(true);\n";
        }
    }
    else if (auto* radio = qobject_cast<QRadioButton*>(w)) {
        if (!radio->text().isEmpty()) {
            out << indent(level) << varName << "->setText(QStringLiteral(\""
                << radio->text().toHtmlEscaped() << "\"));\n";
        }
        if (radio->isChecked()) {
            out << indent(level) << varName << "->setChecked(true);\n";
        }
    }
    else if (auto* combo = qobject_cast<QComboBox*>(w)) {
        for (int i = 0; i < combo->count(); ++i) {
            out << indent(level) << varName << "->addItem(QStringLiteral(\""
                << combo->itemText(i).toHtmlEscaped() << "\"));\n";
        }
        if (combo->currentIndex() > 0) {
            out << indent(level) << varName << "->setCurrentIndex("
                << combo->currentIndex() << ");\n";
        }
    }
    else if (auto* spin = qobject_cast<QSpinBox*>(w)) {
        out << indent(level) << varName << "->setRange("
            << spin->minimum() << ", " << spin->maximum() << ");\n";
        if (spin->value() != 0) {
            out << indent(level) << varName << "->setValue(" << spin->value() << ");\n";
        }
    }
    else if (auto* dspin = qobject_cast<QDoubleSpinBox*>(w)) {
        out << indent(level) << varName << "->setRange("
            << dspin->minimum() << ", " << dspin->maximum() << ");\n";
        out << indent(level) << varName << "->setDecimals(" << dspin->decimals() << ");\n";
        if (dspin->value() != 0.0) {
            out << indent(level) << varName << "->setValue(" << dspin->value() << ");\n";
        }
    }
    else if (auto* group = qobject_cast<QGroupBox*>(w)) {
        if (!group->title().isEmpty()) {
            out << indent(level) << varName << "->setTitle(QStringLiteral(\""
                << group->title().toHtmlEscaped() << "\"));\n";
        }
    }
}

void QtUiExporterEnhanced::genLayoutCtor(QLayout* layout, QTextStream& out,
                                        int level, const QString& parentWidgetVar)
{
    if (!layout) return;

    auto it = m_vars.constFind(layout);
    if (it == m_vars.cend()) return;

    const VarInfo& v = it.value();

    if (m_opt.addComments) {
        out << indent(level) << "// Create layout: " << v.varName << "\n";
    }

    out << indent(level) << v.varName << " = new " << v.typeName << "();\n";

    emitLayoutProperties(layout, v.varName, out, level);

    // Set as root layout if appropriate
    if (parentWidgetVar == m_opt.rootVariable) {
        out << indent(level) << parentWidgetVar << "->setLayout(" << v.varName << ");\n";
    }

    handleLayoutChildren(layout, v.varName, out, level);
}

void QtUiExporterEnhanced::emitLayoutProperties(QLayout* layout, 
                                                const QString& varName,
                                                QTextStream& out, int level)
{
    // Margins
    int left, top, right, bottom;
    layout->getContentsMargins(&left, &top, &right, &bottom);
    if (left != 0 || top != 0 || right != 0 || bottom != 0) {
        if (left == top && top == right && right == bottom) {
            out << indent(level) << varName << "->setContentsMargins("
                << left << ", " << top << ", " << right << ", " << bottom << ");\n";
        } else {
            out << indent(level) << varName << "->setContentsMargins("
                << left << ", " << top << ", " << right << ", " << bottom << ");\n";
        }
    }
    
    // Spacing
    if (layout->spacing() > 0) {
        out << indent(level) << varName << "->setSpacing(" 
            << layout->spacing() << ");\n";
    }
}

void QtUiExporterEnhanced::handleLayoutChildren(QLayout* layout,
                                               const QString& layoutVarName,
                                               QTextStream& out, int level)
{
    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem* item = layout->itemAt(i);
        
        if (QWidget* w = item->widget()) {
            genWidgetCtor(w, out, level);
            auto vit = m_vars.constFind(w);
            if (vit != m_vars.cend()) {
                out << indent(level) << layoutVarName << "->addWidget("
                    << vit.value().varName << ");\n";
            }
        }
        else if (QLayout* l = item->layout()) {
            genLayoutCtor(l, out, level, m_opt.rootVariable);
            auto vit = m_vars.constFind(l);
            if (vit != m_vars.cend()) {
                out << indent(level) << layoutVarName << "->addLayout("
                    << vit.value().varName << ");\n";
            }
        }
        else if (item->spacerItem()) {
            out << indent(level) << layoutVarName << "->addStretch();\n";
        }
    }
}

void QtUiExporterEnhanced::buildMetadata()
{
    QJsonArray widgets;
    QJsonArray layouts;
    
    for (auto it = m_vars.cbegin(); it != m_vars.cend(); ++it) {
        const VarInfo& v = it.value();
        
        if (v.isLayout) {
            QJsonObject layout;
            layout["varName"] = v.varName;
            layout["qtType"] = v.typeName;
            layout["objectName"] = v.objectName;
            layouts.append(layout);
        } else {
            QJsonObject widget;
            widget["varName"] = v.varName;
            widget["qtType"] = v.typeName;
            widget["pclType"] = v.pclTypeName;
            widget["objectName"] = v.objectName;
            widget["isComposite"] = v.isComposite;
            widgets.append(widget);
        }
    }
    
    m_metadata["widgets"] = widgets;
    m_metadata["layouts"] = layouts;
    m_metadata["className"] = m_opt.className;
}

void QtUiExporterEnhanced::writeMetadata(QTextStream& out)
{
    QJsonDocument doc(m_metadata);
    out << doc.toJson(QJsonDocument::Indented);
}

QWidget* QtUiExporterEnhanced::findInterfaceRoot(QWidget* anyWidget)
{
    if (!anyWidget) return nullptr;
    
    QWidget* current = anyWidget;
    while (current->parentWidget()) {
        current = current->parentWidget();
    }
    
    return current;
}

// ============================================================================
// NumericControlDetector Implementation
// ============================================================================

NumericControlDetector::NumericControlInfo 
NumericControlDetector::detectNumericControl(QWidget* widget)
{
    NumericControlInfo info;
    
    if (!widget) return info;
    
    // Check if this is a container with Label + Slider + LineEdit children
    QLabel* label = nullptr;
    QSlider* slider = nullptr;
    QLineEdit* edit = nullptr;
    
    for (QObject* child : widget->children()) {
        if (auto* w = qobject_cast<QWidget*>(child)) {
            if (!label && qobject_cast<QLabel*>(w)) {
                label = qobject_cast<QLabel*>(w);
            }
            else if (!slider && qobject_cast<QSlider*>(w)) {
                slider = qobject_cast<QSlider*>(w);
            }
            else if (!edit && qobject_cast<QLineEdit*>(w)) {
                edit = qobject_cast<QLineEdit*>(w);
            }
        }
    }
    
    // Valid if we found all three components
    if (label && slider && edit) {
        info.label = label;
        info.slider = slider;
        info.edit = edit;
        info.container = widget;
        
        // Extract base name from object names
        QString objName = widget->objectName();
        if (objName.contains("NumericControl", Qt::CaseInsensitive)) {
            info.baseName = objName;
        } else if (!label->objectName().isEmpty()) {
            info.baseName = label->objectName();
        }
    }
    
    return info;
}

bool NumericControlDetector::isNumericControlContainer(QWidget* widget)
{
    return detectNumericControl(widget).isValid();
}

// ============================================================================
// InterfaceExporter Implementation
// ============================================================================

void InterfaceExporter::exportAllInterfaces(const QString& outputDir)
{
    QList<QWidget*> interfaces = findAllInterfaces();
    
    QDir dir(outputDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    for (QWidget* iface : interfaces) {
        QString baseName = iface->objectName();
        if (baseName.isEmpty()) {
            baseName = "Interface" + QString::number((quintptr)iface, 16);
        }
        
        exportInterface(iface, outputDir, baseName);
    }
}

bool InterfaceExporter::exportInterface(QWidget* interface, 
                                        const QString& outputDir,
                                        const QString& baseName)
{
    QtUiExportOptions opt;
    opt.className = baseName;
    opt.baseClass = "QWidget";
    opt.rootVariable = "this";
    opt.useMemberPointers = true;
    opt.generateSignalsSlots = true;
    opt.addComments = true;
    opt.exportMetadata = true;
    
    QtUiExporterEnhanced exporter(interface, opt);
    
    QDir dir(outputDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // Write header
    QFile hFile(dir.filePath(baseName + ".h"));
    if (!hFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Cannot write" << hFile.fileName();
        return false;
    }
    QTextStream hOut(&hFile);
    exporter.writeHeader(hOut);
    hFile.close();
    
    // Write source
    QFile cppFile(dir.filePath(baseName + ".cpp"));
    if (!cppFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Cannot write" << cppFile.fileName();
        return false;
    }
    QTextStream cppOut(&cppFile);
    exporter.writeSource(cppOut);
    cppFile.close();
    
    // Write metadata
    QFile jsonFile(dir.filePath(baseName + "_metadata.json"));
    if (!jsonFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Cannot write" << jsonFile.fileName();
        return false;
    }
    QTextStream jsonOut(&jsonFile);
    exporter.writeMetadata(jsonOut);
    jsonFile.close();
    
    qInfo() << "Exported interface to:" << dir.filePath(baseName + ".{h,cpp}");
    return true;
}

QList<QWidget*> InterfaceExporter::findAllInterfaces()
{
    QList<QWidget*> result;
    
    for (QWidget* widget : QApplication::allWidgets()) {
        // Look for top-level widgets that might be ProcessInterfaces
        if (!widget->parentWidget() && widget->isVisible()) {
            result.append(widget);
        }
    }
    
    return result;
}
