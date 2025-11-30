#pragma once

#include <QWidget>
#include <QLayout>
#include <QTextStream>
#include <QMap>
#include <QSet>

struct QtUiExportOptions
{
    QString className = "GeneratedDialog";
    QString baseClass = "QWidget";      // could be QDialog if you like
    QString rootVariable = "this";      // the root parent for layouts
    bool    useMemberPointers = true;   // generate member variables
};

class QtUiExporter
{
public:
    QtUiExporter(QWidget* rootWidget, const QtUiExportOptions& opt);

    // Writes a full .h/.cpp pair into the given streams
    void writeHeader(QTextStream& out);
    void writeSource(QTextStream& out);

private:
    QWidget* m_root;
    QtUiExportOptions m_opt;

    struct VarInfo {
        QString typeName;   // e.g. "QLabel"
        QString varName;    // e.g. "labelTitle"
        bool isLayout = false;
    };

    QMap<const QObject*, VarInfo> m_vars;
    QMap<QString, int> m_typeCounters;

    QString indent(int level) const;

    QString makeVarName(const QObject* obj, const QString& typeName);
    QString qtTypeName(const QObject* obj) const;

    void assignVariableNames(QObject* obj);
    void assignVariableNamesRec(QObject* obj);

    void collectMembers(QStringList& memberLines);
    void generateSetupBody(QTextStream& out);

    void genWidgetCtor(QWidget* w, QTextStream& out, int level);
    void genLayoutCtor(QLayout* l, QTextStream& out, int level, const QString& parentWidgetVar);
    void handleLayoutChildren(QLayout* layout, const QString& layoutVarName,
                              QTextStream& out, int level);

    void emitWidgetProperties(QWidget* w, const QString& varName, QTextStream& out, int level);
    void emitSpecificWidgetProps(QWidget* w, const QString& varName, QTextStream& out, int level);
};
