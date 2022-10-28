unix {
    LIBS += -L/opt/anaconda3/lib -lcrypto -lssl
    INCLUDEPATH += /opt/anaconda3/include
    INCLUDEPATH += $$PWD

    HEADERS += \
        $$PWD/Aes256CBC.h

    SOURCES += \
        $$PWD/Aes256CBC.cpp
}
