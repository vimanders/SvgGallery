QT += core gui svg svgwidgets widgets core5compat

android {
    QT += core-private
}

QTPLUGIN += qsvg

CONFIG += c++17

include(Project.pri)
include(Scintilla.pri)

win32 | android {
    # Step 1: buildフォルダ削除 → Step 2以降はrebuild_scintillaへ → Step 6: 本体ビルド
    # Qt Creatorのカスタムビルドステップで "rebuild" ターゲットを指定して実行
    BUILD_DIR = $$shell_path($$OUT_PWD)

    rebuild.target   = rebuild
    rebuild.depends  = rebuild_scintilla
    rebuild.commands = \
        cmd /c "if exist $$BUILD_DIR rmdir /s /q $$BUILD_DIR" $$escape_expand(\n\t) \
        cmd /c "echo Scintillaの再ビルド完了。Qt CreatorでRebuildを実行してください。"

    QMAKE_EXTRA_TARGETS += rebuild
}

