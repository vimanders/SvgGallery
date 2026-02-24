#include "SvgGallery.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setStyle("Fusion");
    SvgGallery gallery;
    gallery.show();
    
    return a.exec();
}
