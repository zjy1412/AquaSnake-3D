#include <QApplication>
#include "gamewidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    GameWidget widget;
    widget.resize(800, 600);
    widget.show();
    
    return app.exec();
}