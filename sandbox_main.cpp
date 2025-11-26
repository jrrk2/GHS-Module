// gradients_main.cpp
#include <QApplication>

#include <pcl/Console.h>
#include <pcl/MetaModule.h>
#include "SandboxModule.h"
#include "SandboxInterface.h"
#include "SandboxProcess.h"
#include "PCLMockAPI.h"

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
   if ( !iface.Launch( proc, nullptr, dynamic, flags ) )
   {
      fputs( "Launch() returned false – interface did not accept the process.\n", stderr );
      return 1;
   }

   // In the real host, the interface window would be shown by the core.
   // Here we do it ourselves.
   iface.Show();

   // Enter the Qt event loop – all your GUI interactions and mock API calls happen from here.
   return app.exec();
}
