#include "QtUiExporter.h"

#include <QLabel>
#include <QLineEdit>
#include <QSlider>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QGroupBox>

#include <QBoxLayout>
#include <QGridLayout>
#include <QFormLayout>

QtUiExporter::QtUiExporter(QWidget* rootWidget, const QtUiExportOptions& opt)
    : m_root(rootWidget)
    , m_opt(opt)
{
    assignVariableNames(rootWidget);
}

QString QtUiExporter::indent(int level) const
{
    return QString(level * 3, QLatin1Char(' '));
}

QString QtUiExporter::qtTypeName(const QObject* obj) const
{
    const char* cls = obj->metaObject()->className();
    return QString::fromLatin1(cls);
}

QString QtUiExporter::makeVarName(const QObject* obj, const QString& typeName)
{
    QString base;

    if (!obj->objectName().isEmpty())
    {
        base = obj->objectName();
    }
    else
    {
        base = typeName;
    }

    // strip any Q prefix only for fallback base names
    if (base == typeName && base.startsWith('Q') && base.length() > 1)
        base = base.mid(1);

    // make it camelCase-ish
    base[0] = base[0].toLower();

    int& counter = m_typeCounters[base];
    ++counter;
    if (counter > 1)
        base += QString::number(counter);

    return base;
}

void QtUiExporter::assignVariableNames(QObject* obj)
{
    assignVariableNamesRec(obj);
}

void QtUiExporter::assignVariableNamesRec(QObject* obj)
{
    if (!obj)
        return;

    if (auto* w = qobject_cast<QWidget*>(obj))
    {
        QString type = qtTypeName(w);
        if (w != m_root) // root is "this"
        {
            VarInfo info;
            info.typeName = type;
            info.varName  = makeVarName(w, type);
            info.isLayout = false;
            m_vars.insert(w, info);
        }

        // Layouts attached to this widget
        if (auto* layout = w->layout())
        {
            QString lType = qtTypeName(layout);
            VarInfo linfo;
            linfo.typeName = lType;
            linfo.varName  = makeVarName(layout, lType);
            linfo.isLayout = true;
            m_vars.insert(layout, linfo);

            assignVariableNamesRec(layout);
        }
    }
    else if (auto* layout = qobject_cast<QLayout*>(obj))
    {
        // Layouts children: items may be widgets or layouts
        for (int i = 0; i < layout->count(); ++i)
        {
            QLayoutItem* item = layout->itemAt(i);
            if (QWidget* w = item->widget())
                assignVariableNamesRec(w);
            else if (QLayout* l = item->layout())
                assignVariableNamesRec(l);
        }
    }
}

void QtUiExporter::collectMembers(QStringList& memberLines)
{
    for (auto it = m_vars.cbegin(); it != m_vars.cend(); ++it)
    {
        const VarInfo& v = it.value();
        if (!m_opt.useMemberPointers)
            continue;

        if (v.isLayout)
        {
            memberLines << QString("    %1* %2;").arg(v.typeName, v.varName);
        }
        else
        {
            memberLines << QString("    %1* %2;").arg(v.typeName, v.varName);
        }
    }
}

void QtUiExporter::writeHeader(QTextStream& out)
{
    out << "#pragma once\n\n";
    out << "#include <" << m_opt.baseClass << ">\n";
    out << "#include <QPointer>\n\n";
    out << "class " << m_opt.className << " : public " << m_opt.baseClass << "\n";
    out << "{\n";
    out << "    Q_OBJECT\n";
    out << "public:\n";
    out << "    explicit " << m_opt.className << "(QWidget* parent = nullptr);\n\n";
    out << "private:\n";
    out << "    void setupUi();\n\n";

    QStringList members;
    collectMembers(members);
    if (!members.isEmpty())
    {
        out << "    // Generated UI members\n";
        for (const QString& line : members)
            out << line << "\n";
    }

    out << "};\n";
}

void QtUiExporter::writeSource(QTextStream& out)
{
    out << "#include \"" << m_opt.className << ".h\"\n\n";
    out << "#include <QLabel>\n";
    out << "#include <QLineEdit>\n";
    out << "#include <QSlider>\n";
    out << "#include <QCheckBox>\n";
    out << "#include <QComboBox>\n";
    out << "#include <QSpinBox>\n";
    out << "#include <QPushButton>\n";
    out << "#include <QGroupBox>\n";
    out << "#include <QBoxLayout>\n";
    out << "#include <QGridLayout>\n";
    out << "#include <QFormLayout>\n\n";

    out << m_opt.className << "::" << m_opt.className
        << "(QWidget* parent)\n"
        << "    : " << m_opt.baseClass << "(parent)\n"
        << "{\n"
        << "    setupUi();\n"
        << "}\n\n";

    out << "void " << m_opt.className << "::setupUi()\n";
    out << "{\n";

    // root layout
    if (m_root->layout())
    {
        genLayoutCtor(m_root->layout(), out, 1, m_opt.rootVariable);
    }

    out << "}\n";
}

void QtUiExporter::genWidgetCtor(QWidget* w, QTextStream& out, int level)
{
    if (w == m_root)
        return; // root is already "this"

    auto it = m_vars.constFind(w);
    if (it == m_vars.cend())
        return;

    const VarInfo& v = it.value();
    QString parentName = m_opt.rootVariable;

    if (w->parentWidget() && w->parentWidget() != m_root)
    {
        auto pit = m_vars.constFind(w->parentWidget());
        if (pit != m_vars.cend())
            parentName = pit.value().varName;
    }

    out << indent(level)
        << v.varName << " = new " << v.typeName << "(";

    // For some widgets we may want a text ctor:
    if (auto* lbl = qobject_cast<QLabel*>(w))
    {
        out << "QStringLiteral(\"" << lbl->text().toHtmlEscaped() << "\"), " << parentName;
    }
    else if (auto* btn = qobject_cast<QPushButton*>(w))
    {
        out << "QStringLiteral(\"" << btn->text().toHtmlEscaped() << "\"), " << parentName;
    }
    else
    {
        out << parentName;
    }

    out << ");\n";

    emitWidgetProperties(w, v.varName, out, level);
}

void QtUiExporter::emitWidgetProperties(QWidget* w, const QString& varName,
                                        QTextStream& out, int level)
{
    // basic geometry / size policies
    if (w->minimumSize().isValid())
    {
        if (!w->minimumSize().isNull())
        {
            out << indent(level)
                << varName << "->setMinimumSize("
                << w->minimumSize().width() << ", "
                << w->minimumSize().height() << ");\n";
        }
    }

    if (!w->maximumSize().isNull() &&
        w->maximumSize() != QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX))
    {
        out << indent(level)
            << varName << "->setMaximumSize("
            << w->maximumSize().width() << ", "
            << w->maximumSize().height() << ");\n";
    }

    if (!w->font().family().isEmpty())
    {
        out << indent(level)
            << "{ QFont f = " << varName << "->font();\n";
        out << indent(level)
            << "  f.setFamily(QStringLiteral(\""
            << w->font().family().toHtmlEscaped() << "\"));\n";
        out << indent(level)
            << "  " << varName << "->setFont(f);\n";
        out << indent(level) << "}\n";
    }

    emitSpecificWidgetProps(w, varName, out, level);
}

void QtUiExporter::emitSpecificWidgetProps(QWidget* w, const QString& varName,
                                           QTextStream& out, int level)
{
    if (auto* edit = qobject_cast<QLineEdit*>(w))
    {
        if (!edit->text().isEmpty())
        {
            out << indent(level)
                << varName << "->setText(QStringLiteral(\""
                << edit->text().toHtmlEscaped() << "\"));\n";
        }
    }
    else if (auto* slider = qobject_cast<QSlider*>(w))
    {
        out << indent(level)
            << varName << "->setOrientation("
            << (slider->orientation() == Qt::Horizontal
                    ? "Qt::Horizontal" : "Qt::Vertical")
            << ");\n";
        out << indent(level)
            << varName << "->setRange("
            << slider->minimum() << ", "
            << slider->maximum() << ");\n";
        out << indent(level)
            << varName << "->setValue(" << slider->value() << ");\n";
    }
    else if (auto* cb = qobject_cast<QCheckBox*>(w))
    {
        if (!cb->text().isEmpty())
        {
            out << indent(level)
                << varName << "->setText(QStringLiteral(\""
                << cb->text().toHtmlEscaped() << "\"));\n";
        }
        if (cb->isChecked())
        {
            out << indent(level)
                << varName << "->setChecked(true);\n";
        }
    }
    else if (auto* combo = qobject_cast<QComboBox*>(w))
    {
        for (int i = 0; i < combo->count(); ++i)
        {
            out << indent(level)
                << varName << "->addItem(QStringLiteral(\""
                << combo->itemText(i).toHtmlEscaped() << "\"));\n";
        }
        if (combo->currentIndex() >= 0)
        {
            out << indent(level)
                << varName << "->setCurrentIndex("
                << combo->currentIndex() << ");\n";
        }
    }
    else if (auto* spin = qobject_cast<QSpinBox*>(w))
    {
        out << indent(level)
            << varName << "->setRange("
            << spin->minimum() << ", " << spin->maximum() << ");\n";
        out << indent(level)
            << varName << "->setValue(" << spin->value() << ");\n";
    }
}

void QtUiExporter::genLayoutCtor(QLayout* layout, QTextStream& out,
                                 int level, const QString& parentWidgetVar)
{
    if (!layout)
        return;

    auto it = m_vars.constFind(layout);
    if (it == m_vars.cend())
        return;

    const VarInfo& v = it.value();
    QString layoutClass = v.typeName;

    out << indent(level)
        << v.varName << " = new " << layoutClass << "(";

    if (auto* box = qobject_cast<QBoxLayout*>(layout))
    {
        // direction ctor
        QString dir = (box->direction() == QBoxLayout::LeftToRight ||
                       box->direction() == QBoxLayout::RightToLeft)
                      ? "QBoxLayout::LeftToRight"
                      : "QBoxLayout::TopToBottom";
        out << dir;
    }

    out << ");\n";

    if (parentWidgetVar == m_opt.rootVariable)
    {
        out << indent(level)
            << parentWidgetVar << "->setLayout(" << v.varName << ");\n";
    }
    else
    {
        out << indent(level)
            << parentWidgetVar << "->setLayout(" << v.varName << ");\n";
    }

    handleLayoutChildren(layout, v.varName, out, level);
}

void QtUiExporter::handleLayoutChildren(QLayout* layout,
                                        const QString& layoutVarName,
                                        QTextStream& out, int level)
{
    for (int i = 0; i < layout->count(); ++i)
    {
        QLayoutItem* item = layout->itemAt(i);
        if (QWidget* w = item->widget())
        {
            genWidgetCtor(w, out, level);
            auto vit = m_vars.constFind(w);
            if (vit != m_vars.cend())
            {
                out << indent(level)
                    << layoutVarName << "->addWidget("
                    << vit.value().varName << ");\n";
            }
        }
        else if (QLayout* l = item->layout())
        {
            genLayoutCtor(l, out, level, m_opt.rootVariable);
            auto vit = m_vars.constFind(l);
            if (vit != m_vars.cend())
            {
                out << indent(level)
                    << layoutVarName << "->addLayout("
                    << vit.value().varName << ");\n";
            }
        }
        // spacers etc. could be handled here later
    }
}
