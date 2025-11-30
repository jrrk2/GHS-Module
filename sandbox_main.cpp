// gradients_main.cpp
#include <QApplication>

#include <pcl/Console.h>
#include <pcl/api/APIInterface.h>
#include <pcl/MetaModule.h>
#include "SandboxModule.h"
#include "SandboxInterface.h"
#include "SandboxProcess.h"
#include "PCLMockAPI.h"
#include "QtUiExporter.h"
#include <QFile>

// Suppose this is your root container widget for the interface:
QWidget* g_rootWidget; // set by your mock PCL

void dumpUiAsQt(const QString& baseName = "GHSDialog")
{
    QtUiExportOptions opt;
    opt.className   = baseName;
    opt.baseClass   = "QWidget";
    opt.rootVariable = "this";

    QtUiExporter exporter(g_rootWidget, opt);

    QFile hFile(baseName + ".h");
    QFile cppFile(baseName + ".cpp");
    hFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
    cppFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);

    QTextStream hout(&hFile);
    QTextStream cout(&cppFile);

    exporter.writeHeader(hout);
    exporter.writeSource(cout);
}

using namespace pcl;

int main( int argc, char** argv )
{
   QApplication app( argc, argv );
   SetDebugLogging(true);

   Module = new SandboxModule;
   SandboxProcess proc;
   // Create the interface.
   SandboxInterface iface;

   // Simulate the PixInsight host calling Launch().
   bool dynamic = false;
   unsigned flags = 0;
   API = new APIInterface(nullptr);
   
   if ( !iface.Launch( proc, nullptr, dynamic, flags ) )
   {
      fputs( "Launch() returned false – interface did not accept the process.\n", stderr );
      return 1;
   }

   // In the real host, the interface window would be shown by the core.
   // Here we do it ourselves.
   iface.Show();

   g_rootWidget = g_lastTopLevel->widget;
   
   dumpUiAsQt();
   
   // Enter the Qt event loop – all your GUI interactions and mock API calls happen from here.
   return app.exec();
}
