#include <pcl/Control.h>
#include <pcl/Label.h>
#include <pcl/PushButton.h>
#include <pcl/Sizer.h>
#include <pcl/api/APIInterface.h>

#include <QApplication>

using namespace pcl;

int main( int argc, char** argv )
{
    QApplication app( argc, argv );

    // Create a top-level window
    Control top( Control::Null(), 0 );
    top.SetWindowTitle( "Mock API Test Window" );

    // Vertical sizer
    Sizer sizer( true );
    top.SetSizer( sizer );

    // Label
    Label label( "Hello from PCL using Mock API!", top );
    label.AdjustToContents();
    sizer.Add( label );

    // Correct PushButton construction
    PushButton btn( "Click Me", Bitmap::Null(), top );
    btn.AdjustToContents();
    sizer.Add( btn );

    // Show window
    top.AdjustToContents();
    top.Show();

    return app.exec();
}
