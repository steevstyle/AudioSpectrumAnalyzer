QT += widgets printsupport

TARGET = spectrum_analyzer
TEMPLATE = app

CONFIG += c++11

HEADERS = \
    mainwindow.h \
    dspthread.h \
    spectrumdata.h \
    qcustomplot.h

SOURCES = \
    main.cpp \
    mainwindow.cpp \
    dspthread.cpp \
    qcustomplot.cpp

LIBS += -lfftw3 -lpthread -lm

target.path = /root
INSTALLS += target