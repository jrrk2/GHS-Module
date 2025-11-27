// sandbox_diagnostics_main.cpp
#include <QApplication>
#include <QDebug>
#include <QTimer>

#include <pcl/Console.h>
#include <pcl/MetaModule.h>
#include "SandboxModule.h"
#include "SandboxInterface.h"
#include "SandboxProcess.h"
#include "PCLMockAPI.h"

using namespace pcl;

// Same diagnostic levels
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
   qDebug() << "SANDBOX DIAGNOSTIC MAIN (WORKING VERSION)";
   qDebug() << "========================================\n";

   QApplication app( argc, argv );
   SetDebugLogging(true);

   SandboxModule* module = nullptr;
   SandboxProcess* process = nullptr;
   SandboxInterface* interface = nullptr;

#ifdef TEST_LEVEL_1_MODULE_CREATION
   qDebug() << "\n[LEVEL 1] Creating Module...";
   Module = new SandboxModule;
   module = static_cast<SandboxModule*>(Module);
   qDebug() << "[LEVEL 1] Module created successfully";
#endif

#ifdef TEST_LEVEL_2_PROCESS_CREATION
   qDebug() << "\n[LEVEL 2] Creating Process...";
   process = new SandboxProcess;
   qDebug() << "[LEVEL 2] Process created successfully";
#endif

#ifdef TEST_LEVEL_3_INTERFACE_CREATION
   qDebug() << "\n[LEVEL 3] Creating Interface...";
   interface = new SandboxInterface;
   qDebug() << "[LEVEL 3] Interface created successfully";
   
   if (interface->Window().IsNull()) {
       qDebug() << "[LEVEL 3] WARNING: Interface Window is NULL";
   } else {
       qDebug() << "[LEVEL 3] Interface Window exists";
       // Cast the Control reference to QWidget
       QWidget* w = reinterpret_cast<QWidget*>(&interface->Window());
       qDebug() << "[LEVEL 3] Window class:" << w->metaObject()->className();
       qDebug() << "[LEVEL 3] Window visible:" << w->isVisible();
   }
#endif

#ifdef TEST_LEVEL_4_LAUNCH
   qDebug() << "\n[LEVEL 4] Launching Interface...";
   bool dynamic = false;
   unsigned flags = 0;
   bool launchResult = interface->Launch(*process, nullptr, dynamic, flags);
   qDebug() << "[LEVEL 4] Launch result:" << launchResult;
   
   if (!interface->Window().IsNull()) {
       QWidget* w = reinterpret_cast<QWidget*>(&interface->Window());
       qDebug() << "[LEVEL 4] After Launch - visible:" << w->isVisible();
       qDebug() << "[LEVEL 4] Widget Tree:";
       dumpWidgetTree(w);
   }
#endif

#ifdef TEST_LEVEL_5_SHOW
   qDebug() << "\n[LEVEL 5] Showing Interface...";
   interface->Show();
   
   if (!interface->Window().IsNull()) {
       QWidget* w = reinterpret_cast<QWidget*>(&interface->Window());
       qDebug() << "[LEVEL 5] After Show - visible:" << w->isVisible();
       qDebug() << "[LEVEL 5] After Show - size:" << w->size();
   }
#endif

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

   return app.exec();
}
