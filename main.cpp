#include <QApplication>
#include <QPushButton>
#include "src/FishyRenderer.h"


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    auto *fish = new Fishy::FishyRenderer();
    fish->show();

    return QApplication::exec();
    return 0;
}
