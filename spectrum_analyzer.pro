QT += widgets printsupport

TARGET = spectrum_analyzer
TEMPLATE = app

CONFIG += c++11

INCLUDEPATH += /home/stopkins/lab5/fftw-arm/include

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

LIBS += -Wl,--whole-archive /home/stopkins/lab5/fftw-arm/lib/libfftw3.a -Wl,--no-whole-archive -lpthread -lm

target.path = /root
INSTALLS += target
