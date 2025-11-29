// gradients_main.cpp
#include <QApplication>

#include <pcl/Console.h>
#include <pcl/api/APIInterface.h>
#include <pcl/MetaModule.h>
#include "GradientsModule.h"
#include "GradientsHdrCompositionInterface.h"
#include "GradientsHdrCompositionProcess.h"
#include "PCLMockAPI.h"

using namespace pcl;

void *(resolver)(const char *arg)
{
  return nullptr;
}

int main( int argc, char** argv )
{
   QApplication app( argc, argv );
   SetDebugLogging(true);

   Module = new GradientsModule;
   TheGradientsHdrCompositionProcess = new GradientsHdrCompositionProcess;
   TheGradientsHdrCompositionInterface = new GradientsHdrCompositionInterface;

   // Simulate the PixInsight host calling Launch().
   bool dynamic = false;
   unsigned flags = 0;
   API = new APIInterface(resolver);
   TheGradientsHdrCompositionInterface->Launch( *TheGradientsHdrCompositionProcess, nullptr, dynamic, flags );

   // In the real host, the interface window would be shown by the core.
   // Here we do it ourselves.
   TheGradientsHdrCompositionInterface->Show();

   // Enter the Qt event loop – all your GUI interactions and mock API calls happen from here.
   return app.exec();
}
