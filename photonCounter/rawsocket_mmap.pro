QT += core gui widgets charts
CONFIG += c++17

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = rawsocket_mmap
TEMPLATE = app

SOURCES += main.cpp mainwindow.cpp capturethread.cpp
HEADERS += mainwindow.h capturethread.h

target.path = /usr/local/bin
INSTALLS += target

# Для raw socket нужны права root (запускать с sudo)