#include <QApplication>
#include <QPushButton>
#include "src/FishyRenderer.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    auto *fish = new Fishy::FishyRenderer();
    qDebug() << fish->render();


    return QApplication::exec();
}
