INCLUDEPATH += \
    ../ScintillaPrepacked \

SOURCES += \
    ../ScintillaPrepacked/ScintillaRelay.cpp \

HEADERS += \
    ../ScintillaPrepacked/ScintillaRelay.h \

win32 {
    # Copy DLLs to output directory after build
    QMAKE_POST_LINK += \
        $$QMAKE_COPY $$shell_path(../../../ScintillaPrepacked/ScintillaEdit5.dll) $$shell_path($$OUT_PWD/release/) $$escape_expand(\n\t) \
        $$QMAKE_COPY $$shell_path(../../../ScintillaPrepacked/ScintillaEditBase5.dll) $$shell_path($$OUT_PWD/release/) $$escape_expand(\n\t) \
        $$QMAKE_COPY $$shell_path(../../../ScintillaPrepacked/Lexilla5.dll) $$shell_path($$OUT_PWD/release/)
}

android {
    ANDROID_EXTRA_LIBS += \
        $$PWD/../ScintillaPrepacked/ibScintillaEdit_$${ANDROID_TARGET_ARCH}.so \
        $$PWD/../ScintillaPrepacked/libScintillaEditBase_$${ANDROID_TARGET_ARCH}.so \
        $$PWD/../ScintillaPrepacked/libLexilla_$${ANDROID_TARGET_ARCH}.so
}
