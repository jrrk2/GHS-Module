// sandbox_main_find_all_widgets.cpp
// Alternative approach: Find all widgets regardless of layout hierarchy

#include <QApplication>
#include <QThread>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QSlider>
#include <QDebug>

#include <pcl/Console.h>
#include <pcl/api/APIInterface.h>
#include <pcl/MetaModule.h>

#include "SandboxModule.h"
#include "SandboxInterface.h"
#include "SandboxProcess.h"
#include "PCLMockAPI.h"
#include "ExportHelper.h"
#include "RootWidgetSelector.h"

using namespace pcl;

// Find all widgets of a type, regardless of layout
QList<QWidget*> findAllWidgetsOfType(QWidget* root, const QString& typeName)
{
    QList<QWidget*> found;
    
    std::function<void(QWidget*)> search = [&](QWidget* w) {
        if (!w) return;
        
        if (w->metaObject()->className() == typeName) {
            found.append(w);
        }
        
        // Search all children
        for (QObject* child : w->children()) {
            if (QWidget* childWidget = qobject_cast<QWidget*>(child)) {
                search(childWidget);
            }
        }
    };
    
    search(root);
    return found;
}

// Create a proper widget hierarchy from loose widgets
QWidget* reconstructInterface(QWidget* root)
{
    Console().WriteLn("<end><cbr>Reconstructing interface from widgets...");
    
    // Find all widgets
    QList<QLabel*> labels;
    QList<QLineEdit*> edits;
    QList<QSpinBox*> spinboxes;
    QList<QCheckBox*> checkboxes;
    QList<QComboBox*> comboboxes;
    QList<QSlider*> sliders;
    
    for (QWidget* w : findAllWidgetsOfType(root, "QLabel")) {
        labels.append(qobject_cast<QLabel*>(w));
    }
    for (QWidget* w : findAllWidgetsOfType(root, "QLineEdit")) {
        edits.append(qobject_cast<QLineEdit*>(w));
    }
    for (QWidget* w : findAllWidgetsOfType(root, "QSpinBox")) {
        spinboxes.append(qobject_cast<QSpinBox*>(w));
    }
    for (QWidget* w : findAllWidgetsOfType(root, "QCheckBox")) {
        checkboxes.append(qobject_cast<QCheckBox*>(w));
    }
    for (QWidget* w : findAllWidgetsOfType(root, "QComboBox")) {
        comboboxes.append(qobject_cast<QComboBox*>(w));
    }
    for (QWidget* w : findAllWidgetsOfType(root, "QSlider")) {
        sliders.append(qobject_cast<QSlider*>(w));
    }
    
    Console().WriteLn("<end><cbr>Found widgets:");
    Console().WriteLn("<end><cbr>  Labels: " + String(labels.size()));
    Console().WriteLn("<end><cbr>  Edits: " + String(edits.size()));
    Console().WriteLn("<end><cbr>  SpinBoxes: " + String(spinboxes.size()));
    Console().WriteLn("<end><cbr>  CheckBoxes: " + String(checkboxes.size()));
    Console().WriteLn("<end><cbr>  ComboBoxes: " + String(comboboxes.size()));
    Console().WriteLn("<end><cbr>  Sliders: " + String(sliders.size()));
    
    // Create a new container with proper layout
    QWidget* container = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(container);
    mainLayout->setMargin(8);
    mainLayout->setSpacing(6);
    
    // Rebuild the interface based on the known structure from SandboxInterface
    
    // Parameter One: NumericControl (Label + Edit + Slider)
    if (labels.size() > 0 && edits.size() > 0 && sliders.size() > 0) {
        QHBoxLayout* param1Layout = new QHBoxLayout();
        
        // Find the widgets for ParameterOne
        QLabel* param1Label = nullptr;
        QLineEdit* param1Edit = nullptr;
        QSlider* param1Slider = sliders[0];
        
        // Find label with "One:"
        for (QLabel* lbl : labels) {
            if (lbl->text().contains("One")) {
                param1Label = lbl;
                break;
            }
        }
        
        // Find the first edit (for NumericControl)
        if (edits.size() > 0) {
            param1Edit = edits[0];
        }
        
        if (param1Label) {
            param1Label->setParent(container);
            param1Layout->addWidget(param1Label);
        }
        if (param1Edit) {
            param1Edit->setParent(container);
            param1Layout->addWidget(param1Edit);
        }
        param1Slider->setParent(container);
        param1Layout->addWidget(param1Slider);
        
        mainLayout->addLayout(param1Layout);
    }
    
    // Parameter Two: Label + SpinBox
    if (labels.size() > 1 && spinboxes.size() > 0) {
        QHBoxLayout* param2Layout = new QHBoxLayout();
        
        QLabel* param2Label = nullptr;
        for (QLabel* lbl : labels) {
            if (lbl->text().contains("Two")) {
                param2Label = lbl;
                break;
            }
        }
        
        if (param2Label) {
            param2Label->setParent(container);
            param2Layout->addWidget(param2Label);
        }
        
        QSpinBox* spinbox = spinboxes[0];
        spinbox->setParent(container);
        param2Layout->addWidget(spinbox);
        param2Layout->addStretch();
        
        mainLayout->addLayout(param2Layout);
    }
    
    // Parameter Three: CheckBox
    if (checkboxes.size() > 0) {
        QHBoxLayout* param3Layout = new QHBoxLayout();
        param3Layout->addSpacing(40);  // Indent to align with labels
        
        QCheckBox* checkbox = checkboxes[0];
        checkbox->setParent(container);
        param3Layout->addWidget(checkbox);
        param3Layout->addStretch();
        
        mainLayout->addLayout(param3Layout);
    }
    
    // Parameter Four: Label + ComboBox
    if (labels.size() > 2 && comboboxes.size() > 0) {
        QHBoxLayout* param4Layout = new QHBoxLayout();
        
        QLabel* param4Label = nullptr;
        for (QLabel* lbl : labels) {
            if (lbl->text().contains("Four")) {
                param4Label = lbl;
                break;
            }
        }
        
        if (param4Label) {
            param4Label->setParent(container);
            param4Layout->addWidget(param4Label);
        }
        
        QComboBox* combobox = comboboxes[0];
        combobox->setParent(container);
        param4Layout->addWidget(combobox);
        param4Layout->addStretch();
        
        mainLayout->addLayout(param4Layout);
    }
    
    // Parameter Five: Label + Edit
    if (labels.size() > 3 && edits.size() > 1) {
        QHBoxLayout* param5Layout = new QHBoxLayout();
        
        QLabel* param5Label = nullptr;
        for (QLabel* lbl : labels) {
            if (lbl->text().contains("Five")) {
                param5Label = lbl;
                break;
            }
        }
        
        if (param5Label) {
            param5Label->setParent(container);
            param5Layout->addWidget(param5Label);
        }
        
        // Find the second edit (for Parameter Five)
        if (edits.size() > 1) {
            QLineEdit* param5Edit = edits[edits.size() - 1];  // Last edit
            param5Edit->setParent(container);
            param5Layout->addWidget(param5Edit, 100);
        }
        
        mainLayout->addLayout(param5Layout);
    }
    
    Console().WriteLn("<end><cbr>Interface reconstructed with " + 
                      String(mainLayout->count()) + " parameter groups");
    
    return container;
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    SetDebugLogging(true);

    // Initialize API before Console
    API = new APIInterface(nullptr);
    
    Module = new SandboxModule;
    SandboxProcess proc;
    SandboxInterface iface;
    MockBase* interfaceRoot = new MockBase();
    interfaceRoot->isSizer = false;
    interfaceRoot->widget = new QWidget(nullptr);  // True top-level
    interfaceRoot->widget->setWindowTitle("SandboxInterface Mock");
    
    // Add to top-level list
    g_topLevelWidgets.append(interfaceRoot);
    
    // Set the interface's handle (simulate what PixInsight core does)
    // This is what InterfaceDispatcher::Initialize() does:
    iface.handle = (control_handle)interfaceRoot;

    bool dynamic = false;
    unsigned flags = 0;
   
    Console().WriteLn("<end><cbr>========================================");
    Console().WriteLn("<end><cbr>Widget Reconstruction Export");
    Console().WriteLn("<end><cbr>========================================\n");
    Console().WriteLn("<end><cbr>Launching interface...");
    
    if (!iface.Launch(proc, nullptr, dynamic, flags)) {
        fputs("Launch() returned false\n", stderr);
        return 1;
    }

    iface.Show();

    QList<QWidget*> candidates;
    for (MockBase* base : g_topLevelWidgets) {
        if (base && base->widget) {
            candidates.append(base->widget);
        }
    }
    
    // Smart selection!
    QWidget* bestRoot = RootWidgetSelector::selectBestRoot(candidates, true);
    
    if (!bestRoot) {
        fputs("Error: No root widget!\n", stderr);
        return 1;
    }
    
    // Wait for everything to be created
    QApplication::processEvents();
    QThread::msleep(200);
    QApplication::processEvents();
    ExportHelper::exportInterface(bestRoot, "SandboxDialog", "./exported");
    /*
    // Reconstruct the interface properly
    Console().WriteLn("<end><cbr><br>Reconstructing proper interface...");
    QWidget* reconstructed = reconstructInterface(rootWidget);
    
    // Export the reconstructed interface
    Console().WriteLn("<end><cbr><br>Exporting reconstructed interface...");
    ExportHelper::exportInterface(reconstructed, "SandboxDialog_Fixed", "./exported");
    
    Console().WriteLn("<end><cbr><br>========================================");
    Console().WriteLn("<end><cbr>Export complete!");
    Console().WriteLn("<end><cbr>========================================\n");
    Console().WriteLn("<end><cbr>Generated files:");
    Console().WriteLn("<end><cbr>  ./exported/SandboxDialog_Broken.*   (3 widgets)");
    Console().WriteLn("<end><cbr>  ./exported/SandboxDialog_Fixed.*    (all 5 parameters) ✓\n");
    
    // Show reconstructed interface
    reconstructed->show();
    
    */
    return app.exec();
}
