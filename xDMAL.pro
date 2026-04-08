QT       += core gui printsupport opengl network sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11
# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QCUSTOMPLOT_USE_OPENGL

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += $$PWD/Module/Header
INCLUDEPATH += $$PWD/Module/refactor
INCLUDEPATH += $$PWD/3rdparty/QsLog
include($$PWD/3rdparty/QsLog/QsLog.pri)

INCLUDEPATH += $$PWD/fftw/x64/include
LIBS += -L$$PWD/fftw/x64/lib -llibfftw3-3


SOURCES += \
    Module/Src/filecopyworker.cpp \
    Module/Src/mainprocess.cpp \
    Module/Src/pciefunction.cpp \
    Module/Src/planargraph.cpp \
    Module/Src/qcustomplot.cpp \
    Module/Src/sliderruler.cpp \
    Module/Src/rfm2g_card.cpp \
    Module/refactor/dataloader.cpp \
    Module/refactor/pciecommunication.cpp \
    Module/refactor/pcieprotocol.cpp \
    Module/refactor/realdataloader.cpp \
    Module/refactor/signalprocessing.cpp \
    Module/refactor/signalvaluewindow.cpp \
    Module/refactor/signalwindow.cpp \
    main.cpp

HEADERS += \
    Module/Header/filecopyworker.h \
    Module/Header/header.h \
    Module/Header/mainprocess.h \
    Module/Header/pciefunction.h \
    Module/Header/planargraph.h \
    Module/Header/qcustomplot.h \
    Module/Header/sliderruler.h \
    Module/Header/rfm2g_card.h \
    Module/refactor/dataloader.h \
    Module/refactor/pciecommunication.h \
    Module/refactor/pcieprotocol.h \
    Module/refactor/realdataloader.h \
    Module/refactor/signalprocessing.h \
    Module/refactor/signalvaluewindow.h \
    RFM2G/rfm2g_api.h \
    Module/refactor/signalwindow.h

FORMS += \
    mainprocess.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

win32: LIBS += -lSetupAPI

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/Module/ -lfreeglut -lOpengl32
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/Module/ -lfreeglut -lOpengl32

INCLUDEPATH += $$PWD/Module
DEPENDPATH += $$PWD/Module

RESOURCES += \
    resource.qrc

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/RFM2G/ -lrfm2gdll_stdc_64
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/RFM2G/ -lrfm2gdll_stdc_64
INCLUDEPATH += $$PWD/RFM2G
DEPENDPATH += $$PWD/RFM2G
RC_FILE += xDMAL.rc
