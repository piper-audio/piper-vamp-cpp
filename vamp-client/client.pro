
TEMPLATE = app

CONFIG += qt stl c++11 exceptions console warn_on
QT -= xml network gui widgets

!win32 {
    QMAKE_CXXFLAGS += -Werror
}

OBJECTS_DIR = ../o
MOC_DIR = ../o

VAMPSDK_DIR = ../../vamp-plugin-sdk
PIPER_DIR = ../../piper

QMAKE_CXXFLAGS = -I$$VAMPSDK_DIR -I..

# Using the "console" CONFIG flag above should ensure this happens for
# normal Windows builds, but this may be necessary when cross-compiling
win32-x-g++:QMAKE_LFLAGS += -Wl,-subsystem,console
    
TARGET = client

SOURCES += \
	client.cpp

