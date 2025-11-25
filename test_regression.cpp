#include <pcl/Control.h>
#include <pcl/PushButton.h>
#include <pcl/Label.h>
#include <pcl/Sizer.h>
#include <pcl/Edit.h>
#include <pcl/Console.h>
#include <pcl/ImageWindow.h>
#include <pcl/Font.h>
#include <pcl/Bitmap.h>

#include <QApplication>
#include <QDialog>
#include <QPushButton>
#include <QVBoxLayout>

using namespace pcl;

// ----------------------------------------------------------------------------
// Regression test functions (each opens its own PCL window)
// ----------------------------------------------------------------------------

static void Test_Controls()
{
   Console().WriteLn( "<end><cbr><br>-- Control Creation Test --" );

   Control w( Control::Null(), 0 );
   w.SetWindowTitle( "Control Test" );
   w.Resize( 300, 150 );
   w.Show();
}

static void Test_Sizers()
{
   Console().WriteLn( "<end><cbr><br>-- Sizer / Layout Test --" );

   Control w( Control::Null(), 0 );
   w.SetWindowTitle( "Sizer Test" );

   Sizer s( true ); // vertical
   w.SetSizer( s );

   Label a( "One", w );   a.AdjustToContents();
   Label b( "Two", w );   b.AdjustToContents();
   Label c( "Three", w ); c.AdjustToContents();

   s.Add( a );
   s.Add( b );
   s.Add( c );

   w.AdjustToContents();
   w.Show();
}

static void Test_Edit()
{
   Console().WriteLn( "<end><cbr><br>-- Edit Control Test --" );

   Control w( Control::Null(), 0 );
   w.SetWindowTitle( "Edit Test" );

   Sizer s( true ); // vertical
   w.SetSizer( s );

   Edit e( "Type here", w );
   e.AdjustToContents();

   s.Add( e );

   w.AdjustToContents();
   w.Show();
}

static void Test_Button()
{
   Console().WriteLn( "<end><cbr><br>-- Button Test (no click handler) --" );

   Control w( Control::Null(), 0 );
   w.SetWindowTitle( "Button Test" );

   Sizer s( true ); // vertical
   w.SetSizer( s );

   // Signature: PushButton( const String& text = String(),
   //                        const Bitmap& icon = Bitmap::Null(),
   //                        Control& parent = Control::Null() );
   PushButton b( "Click Me", Bitmap::Null(), w );
   b.AdjustToContents();

   s.Add( b );

   w.AdjustToContents();
   w.Show();
}

static void Test_ImageWindow()
{
   Console().WriteLn( "<end><cbr><br>-- ImageWindow Test --" );

   ImageWindow w( 256, 256, 1,
                  32,   // bits per sample
                  true, // float
                  true, // color
                  "Mock Test Image" );

   w.Show();
}

static void Test_FontCursor()
{
   Console().WriteLn( "<end><cbr><br>-- Font / Text Test --" );

   Control w( Control::Null(), 0 );
   w.SetWindowTitle( "Font / Text Test" );

   Sizer s( true ); // vertical
   w.SetSizer( s );

   Label l( "Sample Text with Custom Font", w );
   Font f = l.Font();
   f.SetPointSize( 14 );
   f.SetItalic( true );
   l.SetFont( f );
   l.AdjustToContents();

   s.Add( l );

   w.AdjustToContents();
   w.Show();
}

// ----------------------------------------------------------------------------
// MAIN GUI WINDOW: Qt panel of buttons to launch the tests
// ----------------------------------------------------------------------------

class RegressionPanel : public QDialog
{
public:
   explicit RegressionPanel( QWidget* parent = nullptr )
      : QDialog( parent )
   {
      setWindowTitle( "Mock API Regression Tests" );

      QVBoxLayout* layout = new QVBoxLayout( this );

      auto addButton = [layout]( const char* text, void (*fn)() )
      {
         QPushButton* btn = new QPushButton( text );
         layout->addWidget( btn );

         QObject::connect( btn, &QPushButton::clicked,
                           [fn]()
                           {
                              if ( fn != nullptr )
                                 fn();
                           } );
      };

      addButton( "Controls",        &Test_Controls );
      addButton( "Sizers",          &Test_Sizers );
      addButton( "Edit",            &Test_Edit );
      addButton( "Buttons",         &Test_Button );
      addButton( "ImageWindow",     &Test_ImageWindow );
      addButton( "Fonts / Text",    &Test_FontCursor );

      layout->addStretch();
      resize( 320, 300 );
   }
};

// ----------------------------------------------------------------------------
// main()
// ----------------------------------------------------------------------------

int main( int argc, char** argv )
{
   QApplication app( argc, argv );

   RegressionPanel panel;
   panel.show();

   return app.exec();
}
