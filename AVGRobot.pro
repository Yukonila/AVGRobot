QT += widgets network  # 添加 network 模块支持 TCP

CONFIG += c++17

# 添加包含路径
INCLUDEPATH += $$PWD/Robot
INCLUDEPATH += $$PWD/Tcp

SOURCES += \
    Robot/robot.cpp \
    Tcp/robotTcpManager.cpp \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    Robot/robot.h \
    Tcp/robotTcpManager.h \
    mainwindow.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target