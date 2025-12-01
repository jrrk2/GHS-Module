// RootWidgetSelector.h
// Smart heuristic-based selection of the best root widget from candidates

#pragma once

#include <QWidget>
#include <QLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QList>
#include <QSet>
#include <QString>
#include <QDebug>

struct RootWidgetScore
{
    QWidget* widget;
    int score;
    QString analysis;
    
    RootWidgetScore() : widget(nullptr), score(0) {}
    RootWidgetScore(QWidget* w, int s, const QString& a) 
        : widget(w), score(s), analysis(a) {}
};

class RootWidgetSelector
{
public:
    /**
     * Find the best root widget from a list of candidates using heuristics
     * 
     * @param candidates List of top-level widgets to evaluate
     * @param verbose Print detailed scoring analysis
     * @return The widget most likely to be the ProcessInterface root
     */
    static QWidget* selectBestRoot(const QList<QWidget*>& candidates, 
                                   bool verbose = true);
    
    /**
     * Score a single widget based on how likely it is to be a ProcessInterface root
     * 
     * @param widget Widget to evaluate
     * @return Score and analysis
     */
    static RootWidgetScore scoreWidget(QWidget* widget);
    
private:
    static QString indent(int level) { return QString(level * 2, ' '); }
};

// ============================================================================
// Implementation
// ============================================================================

QWidget* RootWidgetSelector::selectBestRoot(const QList<QWidget*>& candidates, 
                                            bool verbose)
{
    if (candidates.isEmpty()) {
        if (verbose) {
            qWarning() << "RootWidgetSelector: No candidates provided!";
        }
        return nullptr;
    }
    
    if (candidates.size() == 1) {
        if (verbose) {
            qDebug() << "RootWidgetSelector: Only one candidate, using it.";
        }
        return candidates[0];
    }
    
    if (verbose) {
        qDebug() << "\n" << QString(70, '=');
        qDebug() << "ROOT WIDGET SELECTION - Analyzing" << candidates.size() << "candidates";
        qDebug() << QString(70, '=') << "\n";
    }
    
    QList<RootWidgetScore> scores;
    
    for (QWidget* widget : candidates) {
        if (!widget) continue;
        
        RootWidgetScore result = scoreWidget(widget);
        scores.append(result);
        
        if (verbose) {
            qDebug() << "Candidate:" << widget;
            qDebug() << result.analysis;
            qDebug() << "TOTAL SCORE:" << result.score;
            qDebug() << "";
        }
    }
    
    // Find best score
    QWidget* bestWidget = nullptr;
    int bestScore = -1;
    
    for (const RootWidgetScore& result : scores) {
        if (result.score > bestScore) {
            bestScore = result.score;
            bestWidget = result.widget;
        }
    }
    
    if (verbose) {
        qDebug() << QString(70, '-');
        qDebug() << "SELECTED:" << bestWidget;
        qDebug() << "SCORE:" << bestScore;
        qDebug() << QString(70, '=') << "\n";
    }
    
    // Warn if confidence is low
    if (bestScore < 100 && verbose) {
        qWarning() << "Low confidence in selection (score < 100)!";
        qWarning() << "Consider setting explicit object names in PCL code.";
    }
    
    return bestWidget;
}

RootWidgetScore RootWidgetSelector::scoreWidget(QWidget* widget)
{
    RootWidgetScore result;
    result.widget = widget;
    result.score = 0;
    
    QStringList analysis;
    
    if (!widget) {
        return result;
    }
    
    analysis << QString("  Type: %1").arg(widget->metaObject()->className());
    analysis << QString("  Size: %1x%2").arg(widget->width()).arg(widget->height());
    analysis << QString("  ObjectName: \"%1\"").arg(widget->objectName());
    
    // ========================================================================
    // Heuristic 1: Layout Type and Content
    // ========================================================================
    
    if (auto* vbox = qobject_cast<QVBoxLayout*>(widget->layout())) {
        result.score += 100;
        analysis << "  [+100] Has VBoxLayout (typical for ProcessInterface)";
        
        int itemCount = vbox->count();
        int itemScore = itemCount * 20;
        result.score += itemScore;
        analysis << QString("  [+%1] VBoxLayout has %2 items").arg(itemScore).arg(itemCount);
        
        // Bonus for typical ProcessInterface parameter count (3-10)
        if (itemCount >= 3 && itemCount <= 10) {
            result.score += 50;
            analysis << "  [+50] Item count in typical ProcessInterface range (3-10)";
        }
    }
    else if (auto* hbox = qobject_cast<QHBoxLayout*>(widget->layout())) {
        result.score += 10;
        analysis << "  [+10] Has HBoxLayout (less likely to be root)";
        
        int itemCount = hbox->count();
        int itemScore = itemCount * 5;
        result.score += itemScore;
        analysis << QString("  [+%1] HBoxLayout has %2 items").arg(itemScore).arg(itemCount);
    }
    else if (auto* grid = qobject_cast<QGridLayout*>(widget->layout())) {
        result.score += 80;
        analysis << "  [+80] Has QGridLayout (possible for ProcessInterface)";
        
        int itemCount = grid->count();
        int itemScore = itemCount * 15;
        result.score += itemScore;
        analysis << QString("  [+%1] GridLayout has %2 items").arg(itemScore).arg(itemCount);
    }
    else if (!widget->layout()) {
        analysis << "  [+0] No layout (unlikely to be root)";
    }
    
    // ========================================================================
    // Heuristic 2: Widget Size
    // ========================================================================
    
    int area = widget->width() * widget->height();
    if (area > 100000) {
        result.score += 40;
        analysis << QString("  [+40] Very large size: %1 pixels²").arg(area);
    }
    else if (area > 50000) {
        result.score += 30;
        analysis << QString("  [+30] Large size: %1 pixels²").arg(area);
    }
    else if (area > 10000) {
        result.score += 10;
        analysis << QString("  [+10] Medium size: %1 pixels²").arg(area);
    }
    else {
        analysis << QString("  [+0] Small size: %1 pixels²").arg(area);
    }
    
    // ========================================================================
    // Heuristic 3: Child Widget Count and Diversity
    // ========================================================================
    
    QList<QWidget*> allChildren = widget->findChildren<QWidget*>();
    int childCount = allChildren.size();
    int childScore = childCount * 2;
    result.score += childScore;
    analysis << QString("  [+%1] Has %2 child widgets").arg(childScore).arg(childCount);
    
    // Count unique widget types
    QSet<QString> widgetTypes;
    for (QWidget* child : allChildren) {
        widgetTypes.insert(child->metaObject()->className());
    }
    
    if (widgetTypes.size() >= 5) {
        result.score += 30;
        analysis << QString("  [+30] Has %1 different widget types (diverse interface)").arg(widgetTypes.size());
    }
    else if (widgetTypes.size() >= 3) {
        result.score += 15;
        analysis << QString("  [+15] Has %1 different widget types").arg(widgetTypes.size());
    }
    
    // ========================================================================
    // Heuristic 4: Specific Widget Patterns
    // ========================================================================
    
    QList<QLabel*> labels = widget->findChildren<QLabel*>();
    QList<QLineEdit*> edits = widget->findChildren<QLineEdit*>();
    QList<QSpinBox*> spinboxes = widget->findChildren<QSpinBox*>();
    QList<QCheckBox*> checkboxes = widget->findChildren<QCheckBox*>();
    QList<QComboBox*> comboboxes = widget->findChildren<QComboBox*>();
    
    // ProcessInterface typically has multiple labels (for parameter names)
    if (labels.size() >= 3) {
        result.score += 20;
        analysis << QString("  [+20] Has %1 labels (typical for multi-parameter interface)").arg(labels.size());
    }
    
    // And multiple input widgets
    int inputCount = edits.size() + spinboxes.size() + checkboxes.size() + comboboxes.size();
    if (inputCount >= 3) {
        result.score += 25;
        analysis << QString("  [+25] Has %1 input widgets").arg(inputCount);
    }
    else if (inputCount >= 1) {
        result.score += 10;
        analysis << QString("  [+10] Has %1 input widgets").arg(inputCount);
    }
    
    // ========================================================================
    // Heuristic 5: Object Name Hints
    // ========================================================================
    
    QString objName = widget->objectName().toLower();
    if (objName.contains("interface") || objName.contains("dialog")) {
        result.score += 50;
        analysis << "  [+50] Object name suggests interface/dialog";
    }
    else if (objName.contains("process") || objName.contains("module")) {
        result.score += 30;
        analysis << "  [+30] Object name suggests process/module";
    }
    
    // ========================================================================
    // Heuristic 6: Layout Properties
    // ========================================================================
    
    if (widget->layout()) {
        QMargins margins = widget->layout()->contentsMargins();
        // ProcessInterface typically has consistent margins (often 6-8px)
        if (margins.left() >= 6 && margins.left() <= 10 &&
            margins.left() == margins.right()) {
            result.score += 10;
            analysis << QString("  [+10] Typical interface margins: %1px").arg(margins.left());
        }
        
        int spacing = widget->layout()->spacing();
        if (spacing >= 4 && spacing <= 8) {
            result.score += 5;
            analysis << QString("  [+5] Typical interface spacing: %1px").arg(spacing);
        }
    }
    
    result.analysis = analysis.join("\n");
    return result;
}
