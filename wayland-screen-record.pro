#-------------------------------------------------
#
# Project created by QtCreator 2021-03-15T13:08:43
#
#-------------------------------------------------

QT       += core gui core widgets dbus concurrent KWindowSystem KWaylandClient KI18n KConfigCore network x11extras multimedia multimediawidgets

CONFIG += link_pkgconfig c++11
PKGCONFIG += xcb xcb-util dframeworkdbus gbm epoxy

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = wayland-screen-record
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += c++11

SOURCES += \
        main.cpp \
        mainwindow.cpp \
    waylandadmin.cpp \
    dateadmin.cpp \
    recordadmin.cpp \
    avinputstream.cpp \
    avoutputstream.cpp \
    writeframethread.cpp

HEADERS += \
        mainwindow.h \
    waylandadmin.h \
    dateadmin.h \
    recordadmin.h \
    avinputstream.h \
    avoutputstream.h \
    writeframethread.h

FORMS += \
        mainwindow.ui

LIBS += -lX11 -lXext -lXtst -lXfixes -lXcursor -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lswscale -lswresample -lKF5WaylandClient -lKF5ConfigCore
LIBS += -L"libprocps" -lprocps

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
