#include <QApplication>
#include <QPushButton>
#include "src/Fishy.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    auto *fish = new fishy::Fishy();
    qDebug() << fish->render();


    return QApplication::exec();
}
