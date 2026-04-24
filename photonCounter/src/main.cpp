#include <QApplication>
#include "mainwindow.h"

#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <interface>\n", argv[0]);
        return 1;
    }
    MainWindow w(argv[1]);
    w.show();
    return a.exec();
}
