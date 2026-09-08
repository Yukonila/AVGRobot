QT += widgets network

CONFIG += c++17

# 模块化目录
INCLUDEPATH += $$PWD
INCLUDEPATH += $$PWD/Robot
INCLUDEPATH += $$PWD/Task
INCLUDEPATH += $$PWD/Tcp
INCLUDEPATH += $$PWD/Data
INCLUDEPATH += $$PWD/Dialog
INCLUDEPATH += $$PWD/app
INCLUDEPATH += $$PWD/view

SOURCES += \
    Data/datamanager.cpp \
    Data/usermanager.cpp \
    Dialog/robotdialog.cpp \
    Robot/robot.cpp \
    Robot/robotmanager.cpp \
    Task/robotcontroller.cpp \
    Task/task.cpp \
    Task/taskmanager.cpp \
    Task/taskscheduler.cpp \
    Tcp/tcpRobotServer.cpp \
    app/loginwindow.cpp \
    app/main.cpp \
    app/mainwindow.cpp \
    view/mapeditor.cpp \
    view/mapwidget.cpp

HEADERS += \
    Data/datamanager.h \
    Data/usermanager.h \
    Dialog/robotdialog.h \
    Robot/robot.h \
    Robot/robotmanager.h \
    Task/robotcontroller.h \
    Task/task.h \
    Task/taskmanager.h \
    Task/taskscheduler.h \
    Tcp/tcpRobotServer.h \
    app/loginwindow.h \
    app/mainwindow.h \
    view/mapeditor.h \
    view/mapwidget.h

FORMS += \
    app/mainwindow.ui \
    Dialog/robotdialog.ui

# 默认部署规则
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
