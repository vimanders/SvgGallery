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

    # Paths for rebuild target
    SCINTILLA_SRC_WIN   = $$shell_path($$PWD/../Scintilla)
    PREPACKED_DIR_WIN   = $$shell_path($$PWD/../ScintillaPrepacked)
    WIN_QMAKE           = $$shell_path(C:/Qt/6.10.2/llvm-mingw_64/bin/qmake.bat)
    WIN_MAKE            = $$shell_path(C:/Qt/Tools/llvm-mingw1706_64/bin/mingw32-make.exe)
    EDITBASE_BUILD_WIN  = $$SCINTILLA_SRC_WIN\\qt\\ScintillaEditBase\\build_win
    EDIT_BUILD_WIN      = $$SCINTILLA_SRC_WIN\\qt\\ScintillaEdit\\build_win

    # Step 1: DLL削除
    # Step 2: ScintillaEditBase 再ビルド
    # Step 3: ScintillaEdit 再ビルド
    # Step 4: DLL を ScintillaPrepacked へコピー
    rebuild_scintilla.target   = rebuild_scintilla
    rebuild_scintilla.commands = \
        cmd /c "del /q $$PREPACKED_DIR_WIN\\*.dll 2>nul" $$escape_expand(\n\t) \
        cmd /c "if exist $$EDITBASE_BUILD_WIN rmdir /s /q $$EDITBASE_BUILD_WIN" $$escape_expand(\n\t) \
        cmd /c "mkdir $$EDITBASE_BUILD_WIN" $$escape_expand(\n\t) \
        cmd /c "cd /d $$EDITBASE_BUILD_WIN && call $$WIN_QMAKE ..\\ScintillaEditBase.pro CONFIG+=release && $$WIN_MAKE -j4 release" $$escape_expand(\n\t) \
        cmd /c "if exist $$EDIT_BUILD_WIN rmdir /s /q $$EDIT_BUILD_WIN" $$escape_expand(\n\t) \
        cmd /c "mkdir $$EDIT_BUILD_WIN" $$escape_expand(\n\t) \
        cmd /c "cd /d $$EDIT_BUILD_WIN && call $$WIN_QMAKE ..\\ScintillaEdit.pro CONFIG+=release && $$WIN_MAKE -j4 release" $$escape_expand(\n\t) \
        cmd /c "copy /y $$EDITBASE_BUILD_WIN\\release\\ScintillaEditBase5.dll $$PREPACKED_DIR_WIN\\" $$escape_expand(\n\t) \
        cmd /c "copy /y $$EDIT_BUILD_WIN\\release\\ScintillaEdit5.dll $$PREPACKED_DIR_WIN\\"

    QMAKE_EXTRA_TARGETS += rebuild_scintilla
}

android {
    ANDROID_EXTRA_LIBS += \
        $$PWD/../ScintillaPrepacked/libScintillaEdit_$${ANDROID_TARGET_ARCH}.so \
        $$PWD/../ScintillaPrepacked/libScintillaEditBase_$${ANDROID_TARGET_ARCH}.so \
        $$PWD/../ScintillaPrepacked/libLexilla_$${ANDROID_TARGET_ARCH}.so

    # Paths for rebuild target
    SCINTILLA_SRC       = $$shell_path($$PWD/../Scintilla)
    PREPACKED_DIR       = $$shell_path($$PWD/../ScintillaPrepacked)
    ANDROID_QMAKE       = $$shell_path(C:/Qt/6.10.2/android_arm64_v8a/bin/qmake.bat)
    NINJA_EXE           = $$shell_path(C:/Qt/Tools/Ninja/ninja.exe)
    EDITBASE_SRC        = $$SCINTILLA_SRC\\qt\\ScintillaEditBase
    EDITBASE_BUILD      = $$EDITBASE_SRC\\build_android
    EDIT_SRC            = $$SCINTILLA_SRC\\qt\\ScintillaEdit
    EDIT_BUILD          = $$EDIT_SRC\\build_android

    # Step 1: .so削除
    # Step 2: ScintillaEditBase 再ビルド
    # Step 3: ScintillaEdit 再ビルド
    # Step 4: .so を ScintillaPrepacked へコピー
    rebuild_scintilla.target   = rebuild_scintilla
    rebuild_scintilla.commands = \
        cmd /c "del /q $$PREPACKED_DIR\\*.so 2>nul" $$escape_expand(\n\t) \
        cmd /c "if exist $$EDITBASE_BUILD rmdir /s /q $$EDITBASE_BUILD" $$escape_expand(\n\t) \
        cmd /c "mkdir $$EDITBASE_BUILD" $$escape_expand(\n\t) \
        cmd /c "cd /d $$EDITBASE_BUILD && call $$ANDROID_QMAKE ..\\ScintillaEditBase.pro CONFIG+=release && $$NINJA_EXE" $$escape_expand(\n\t) \
        cmd /c "if exist $$EDIT_BUILD rmdir /s /q $$EDIT_BUILD" $$escape_expand(\n\t) \
        cmd /c "mkdir $$EDIT_BUILD" $$escape_expand(\n\t) \
        cmd /c "cd /d $$EDIT_BUILD && call $$ANDROID_QMAKE ..\\ScintillaEdit.pro CONFIG+=release && $$NINJA_EXE" $$escape_expand(\n\t) \
        cmd /c "copy /y $$EDITBASE_BUILD\\libScintillaEditBase_arm64-v8a.so $$PREPACKED_DIR\\" $$escape_expand(\n\t) \
        cmd /c "copy /y $$EDIT_BUILD\\libScintillaEdit_arm64-v8a.so $$PREPACKED_DIR\\"

    QMAKE_EXTRA_TARGETS += rebuild_scintilla
}
