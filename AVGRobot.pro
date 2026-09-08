QT += widgets network

CONFIG += c++17

# 添加包含路径
INCLUDEPATH += $$PWD/Robot
INCLUDEPATH += $$PWD/Tcp
INCLUDEPATH += $$PWD/Dialog
INCLUDEPATH += $$PWD/Task
INCLUDEPATH += $$PWD/Data

SOURCES += \
    Data/datamanager.cpp \
    Dialog/robotdialog.cpp \
    Robot/robot.cpp \
    Robot/robotmanager.cpp \
    Task/robotcontroller.cpp \
    Task/task.cpp \
    Task/taskmanager.cpp \
    Task/taskscheduler.cpp \
    loginwindow.cpp \
    main.cpp \
    mainwindow.cpp \
    mapeditor.cpp \
    mapwidget.cpp

HEADERS += \
    Data/datamanager.h \
    Dialog/robotdialog.h \
    Robot/robot.h \
    Robot/robotmanager.h \
    Task/robotcontroller.h \
    Task/task.h \
    Task/taskmanager.h \
    Task/taskscheduler.h \
    loginwindow.h \
    mainwindow.h \
    mapeditor.h \
    mapwidget.h

FORMS += \
    mainwindow.ui \
    Dialog/robotdialog.ui

# 默认部署规则
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target