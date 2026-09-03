QT += widgets network

CONFIG += c++17

# 添加包含路径
INCLUDEPATH += $$PWD/Robot
INCLUDEPATH += $$PWD/Tcp
INCLUDEPATH += $$PWD/Dialog

SOURCES += \
    Dialog/robotdialog.cpp \
    Robot/robot.cpp \
    Robot/robotManager.cpp \
    Robot/robotcontroller.cpp \
    Tcp/robotTcpManager.cpp \
    loginwindow.cpp \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    Dialog/robotdialog.h \
    Robot/robot.h \
    Robot/robotManager.h \
    Robot/robotcontroller.h \
    Tcp/robotTcpManager.h \
    loginwindow.h \
    mainwindow.h

FORMS += \
    mainwindow.ui \
    Dialog/robotdialog.ui

# 默认部署规则
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target