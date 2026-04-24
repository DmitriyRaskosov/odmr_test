QT += core gui widgets charts
CONFIG += c++17

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = photonCounter
TEMPLATE = app

SOURCES += src/main.cpp src/mainwindow.cpp src/capturethread.cpp
HEADERS += inc/mainwindow.h inc/capturethread.h

target.path = /usr/local/bin
INSTALLS += target
INCLUDEPATH += inc
# Для raw socket нужны права root (запускать с sudo)