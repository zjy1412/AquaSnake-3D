#include <QApplication>
#include "ui.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    UIManager mainWindow;
    mainWindow.setMinimumSize(1024, 768);
    mainWindow.show();
    
    return app.exec();
}