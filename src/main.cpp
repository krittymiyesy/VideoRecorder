
#include "mainwindow.h"
#include <QApplication>

#include <QTextCodec>

#include "AppConfig.h"

#undef main

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QTextCodec *codec = QTextCodec::codecForName("GBK"); //��ȡϵͳ����
    QTextCodec::setCodecForLocale(codec);

    AppConfig::InitAllDataPath();

    MainWindow w;

    return a.exec();
}
