// gradients_diagnostic_main.cpp
#include <QApplication>
#include <QDebug>
#include <QTimer>

#include <pcl/Console.h>
#include <pcl/MetaModule.h>
#include "GradientsModule.h"
#include "GradientsHdrCompositionInterface.h"
#include "GradientsHdrCompositionProcess.h"
#include "PCLMockAPI.h"

using namespace pcl;

// Diagnostic levels - uncomment progressively to test each stage
#define TEST_LEVEL_1_MODULE_CREATION
#define TEST_LEVEL_2_PROCESS_CREATION
#define TEST_LEVEL_3_INTERFACE_CREATION
#define TEST_LEVEL_4_LAUNCH
#define TEST_LEVEL_5_SHOW

void dumpWidgetTree(QWidget* widget, int level = 0)
{
    if (!widget) return;
    
    QString indent = QString(level * 2, ' ');
    qDebug() << indent.toStdString().c_str() 
             << widget->metaObject()->className()
             << "visible:" << widget->isVisible()
             << "size:" << widget->size()
             << "pos:" << widget->pos()
             << "parent:" << (widget->parentWidget() ? widget->parentWidget()->metaObject()->className() : "NULL");
    
    for (QObject* child : widget->children()) {
        QWidget* childWidget = qobject_cast<QWidget*>(child);
        if (childWidget) {
            dumpWidgetTree(childWidget, level + 1);
        }
    }
}

int main( int argc, char** argv )
{
   qDebug() << "========================================";
   qDebug() << "GRADIENTS DIAGNOSTIC MAIN";
   qDebug() << "========================================\n";

   QApplication app( argc, argv );
   SetDebugLogging(true);

   GradientsModule* module = nullptr;
   GradientsHdrCompositionProcess* process = nullptr;
   GradientsHdrCompositionInterface* interface = nullptr;

   // ============================================================
   // TEST LEVEL 1: Module Creation
   // ============================================================
#ifdef TEST_LEVEL_1_MODULE_CREATION
   qDebug() << "\n[LEVEL 1] Creating Module...";
   Module = new GradientsModule;
   module = static_cast<GradientsModule*>(Module);
   qDebug() << "[LEVEL 1] Module created successfully";
   qDebug() << "[LEVEL 1] Module name:" << module->Name().c_str();
   qDebug() << "[LEVEL 1] Module version:" << module->Version();
#endif

   // ============================================================
   // TEST LEVEL 2: Process Creation
   // ============================================================
#ifdef TEST_LEVEL_2_PROCESS_CREATION
   qDebug() << "\n[LEVEL 2] Creating Process...";
   TheGradientsHdrCompositionProcess = new GradientsHdrCompositionProcess;
   process = TheGradientsHdrCompositionProcess;
   qDebug() << "[LEVEL 2] Process created successfully";
   qDebug() << "[LEVEL 2] Process ID:" << process->Id().c_str();
#endif

   // ============================================================
   // TEST LEVEL 3: Interface Creation
   // ============================================================
#ifdef TEST_LEVEL_3_INTERFACE_CREATION
   qDebug() << "\n[LEVEL 3] Creating Interface...";
   TheGradientsHdrCompositionInterface = new GradientsHdrCompositionInterface;
   interface = TheGradientsHdrCompositionInterface;
   qDebug() << "[LEVEL 3] Interface created successfully";
   qDebug() << "[LEVEL 3] Interface ID:" << interface->Id().c_str();
   
   // Check if interface has a window
   if (interface->Window().IsNull()) {
       qDebug() << "[LEVEL 3] WARNING: Interface Window is NULL";
   } else {
       qDebug() << "[LEVEL 3] Interface Window exists";
       
       // Cast the Control reference to QWidget
       QWidget* w = reinterpret_cast<QWidget*>(&interface->Window());
       qDebug() << "[LEVEL 3] Window QWidget:" << w;
       qDebug() << "[LEVEL 3] Window class:" << w->metaObject()->className();
       qDebug() << "[LEVEL 3] Window visible:" << w->isVisible();
       qDebug() << "[LEVEL 3] Window size:" << w->size();
   }
#endif

   // ============================================================
   // TEST LEVEL 4: Launch
   // ============================================================
#ifdef TEST_LEVEL_4_LAUNCH
   qDebug() << "\n[LEVEL 4] Launching Interface...";
   bool dynamic = false;
   unsigned flags = 0;
   
   qDebug() << "[LEVEL 4] Calling Launch()...";
   bool launchResult = interface->Launch(*process, nullptr, dynamic, flags);
   
   qDebug() << "[LEVEL 4] Launch result:" << launchResult;
   
   if (!launchResult) {
       qDebug() << "[LEVEL 4] WARNING: Launch returned false";
   }
   
   // Check window state after launch
   if (!interface->Window().IsNull()) {
       QWidget* w = reinterpret_cast<QWidget*>(&interface->Window());
       qDebug() << "[LEVEL 4] After Launch - Window visible:" << w->isVisible();
       qDebug() << "[LEVEL 4] After Launch - Window size:" << w->size();
       qDebug() << "[LEVEL 4] After Launch - Window pos:" << w->pos();
       
       // Dump widget tree
       qDebug() << "[LEVEL 4] Widget Tree:";
       dumpWidgetTree(w);
   }
#endif

   // ============================================================
   // TEST LEVEL 5: Show
   // ============================================================
#ifdef TEST_LEVEL_5_SHOW
   qDebug() << "\n[LEVEL 5] Showing Interface...";
   qDebug() << "[LEVEL 5] Calling Show()...";
   interface->Show();
   
   // Check window state after show
   if (!interface->Window().IsNull()) {
       QWidget* w = reinterpret_cast<QWidget*>(&interface->Window());
       qDebug() << "[LEVEL 5] After Show - Window visible:" << w->isVisible();
       qDebug() << "[LEVEL 5] After Show - Window size:" << w->size();
       qDebug() << "[LEVEL 5] After Show - Window pos:" << w->pos();
       qDebug() << "[LEVEL 5] After Show - Window flags:" << Qt::hex << w->windowFlags();
       
       // Try to force show
       qDebug() << "[LEVEL 5] Forcing show/raise/activate...";
       w->show();
       w->raise();
       w->activateWindow();
       
       // List all top-level widgets
       qDebug() << "[LEVEL 5] All top-level widgets:";
       for (QWidget* tlw : QApplication::topLevelWidgets()) {
           qDebug() << "  -" << tlw->metaObject()->className()
                    << "visible:" << tlw->isVisible()
                    << "ptr:" << tlw;
       }
   }
#endif

   // ============================================================
   // Final Status
   // ============================================================
   qDebug() << "\n========================================";
   qDebug() << "FINAL STATUS";
   qDebug() << "========================================";
   qDebug() << "Top-level widgets count:" << QApplication::topLevelWidgets().count();
   
   for (QWidget* tlw : QApplication::topLevelWidgets()) {
       qDebug() << "\nTop-level:" << tlw->metaObject()->className();
       qDebug() << "  Visible:" << tlw->isVisible();
       qDebug() << "  Size:" << tlw->size();
       qDebug() << "  Pos:" << tlw->pos();
       qDebug() << "  Window Title:" << tlw->windowTitle();
       
       if (tlw->isVisible()) {
           qDebug() << "  >>> THIS WIDGET IS VISIBLE <<<";
       }
   }

   // Schedule a delayed check
   QTimer::singleShot(1000, [interface]() {
       qDebug() << "\n[DELAYED CHECK] After 1 second:";
       if (!interface->Window().IsNull()) {
           QWidget* w = reinterpret_cast<QWidget*>(&interface->Window());
           qDebug() << "[DELAYED CHECK] Window visible:" << w->isVisible();
           qDebug() << "[DELAYED CHECK] Window size:" << w->size();
       }
       
       qDebug() << "[DELAYED CHECK] Top-level widgets:";
       for (QWidget* tlw : QApplication::topLevelWidgets()) {
           qDebug() << "  -" << tlw->metaObject()->className()
                    << "visible:" << tlw->isVisible();
       }
   });

   qDebug() << "\n========================================";
   qDebug() << "ENTERING EVENT LOOP";
   qDebug() << "========================================\n";

   // Enter the Qt event loop
   return app.exec();
}
