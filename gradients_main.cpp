// gradients_main.cpp
#include <QApplication>

#include <pcl/Console.h>
#include <pcl/MetaModule.h>
#include "GradientsModule.h"
#include "GradientsHdrCompositionInterface.h"
#include "GradientsHdrCompositionProcess.h"

using namespace pcl;

int main( int argc, char** argv )
{
   QApplication app( argc, argv );

   Module = new GradientsModule;
   GradientsHdrCompositionProcess proc;
   // Make sure the process singleton exists and has been constructed.
   if ( TheGradientsHdrCompositionProcess == nullptr )
   {
      // In a pure PixInsight module this is guaranteed; in our mock harness we sanity-check.
      fputs( "TheGradientsHdrCompositionProcess is null – did you link GradientsHdrCompositionProcess.o?\n", stderr );
      return 1;
   }

   // Create the interface.
   GradientsHdrCompositionInterface iface;

   // Simulate the PixInsight host calling Launch().
   bool dynamic = false;
   unsigned flags = 0;
   if ( !iface.Launch( *TheGradientsHdrCompositionProcess, nullptr, dynamic, flags ) )
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
