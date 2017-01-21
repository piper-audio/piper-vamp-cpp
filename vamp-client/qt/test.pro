
TEMPLATE = app

CONFIG += qt stl c++11 exceptions console warn_on
QT -= xml network gui widgets

!win32 {
    QMAKE_CXXFLAGS += -Werror
}

OBJECTS_DIR = ../o
MOC_DIR = ../o

VAMPSDK_DIR = ../../../vamp-plugin-sdk
PIPER_DIR = ../../../piper

QMAKE_CXXFLAGS = -I$$VAMPSDK_DIR -I.. -I../..

LIBS += -L/usr/local/lib -lcapnp -lkj -L$$VAMPSDK_DIR -lvamp-hostsdk

# Using the "console" CONFIG flag above should ensure this happens for
# normal Windows builds, but this may be necessary when cross-compiling
win32-x-g++:QMAKE_LFLAGS += -Wl,-subsystem,console

macx*: CONFIG -= app_bundle

TARGET = test

SOURCES += \
        test.cpp \
        ../../vamp-capnp/piper-capnp.cpp
        
HEADERS += \
        ProcessQtTransport.h \
        AutoPlugin.h \
        ../CapnpRRClient.h \
        ../Loader.h \
        ../PluginClient.h \
        ../PluginStub.h \
        ../SynchronousTransport.h
        

