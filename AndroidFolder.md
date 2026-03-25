# AndroidFolder

AndroidのStorage Access Framework (SAF) を通じて、ユーザが選択したフォルダへのアクセスを提供するクラス。

---

## 特徴

- **Javaファイル不要** — `QJniObject` のみでSAF APIを呼ぶ
- **シンプルなインタフェース** — ファイル名だけで読み書きできる
- **自動パーミッション管理** — 許可の取得・永続化をクラス内で完結
- **URI永続化** — `QSettings` に保存し、次回起動時に自動復元
- **非Androidビルド対応** — `#ifdef Q_OS_ANDROID` でガード、他OSではスタブ動作
- **フラットフォルダ専用** — サブフォルダは対象外、ファイルのみを扱う

---

## セットアップ

### .pro

```qmake
android {
    QT += core core-private
}
```

### ファイル構成

```
AndroidFolder.h
AndroidFolder.cpp
```

---

## クラスインタフェース

```cpp
class AndroidFolder : public QObject
{
    Q_OBJECT

public:
    explicit AndroidFolder(QObject *parent = nullptr);

    // 状態
    bool    isReady() const;   // URIが設定済みでアクセス可能か
    QString treeUri() const;   // 現在のtreeURI（未設定なら空文字）

    // フォルダ選択
    void openDialog();                      // SAFダイアログを起動（非同期）
    bool setUri(const QString &treeUri);    // URIを直接セット（テスト用）
    void reset();                           // 保存済みURIを破棄

    // ファイルアクセス（すべてファイル名で指定）
    QStringList fileNames() const;
    QByteArray  read(const QString &fileName) const;
    bool        write(const QString &fileName, const QByteArray &data);
    bool        remove(const QString &fileName);
    bool        exists(const QString &fileName) const;

signals:
    void ready(bool ok);   // openDialog() 完了時に発火
};
```

---

## 状態遷移

```
【初回起動】
  コンストラクタ
    → QSettings にURIなし → isReady() == false

  openDialog() 呼び出し
    → SAFダイアログ表示
    → キャンセル          → ready(false)
    → フォルダ選択
        → パーミッション永続化
        → QSettingsに保存
        → isReady() == true
        → ready(true)

【2回目以降の起動】
  コンストラクタ
    → QSettings からURI復元 → isReady() == true（openDialog不要）
```

---

## 内部のしくみ

### documentIdキャッシュ

SAFの内部ではファイルを `documentId`（不透明な文字列）で管理する。
`fileNames()` を呼んだ時点で `fileName → documentId` のマッピングをキャッシュに保存し、
`read()` / `write()` / `remove()` / `exists()` はすべてキャッシュを参照するため高速に動作する。

```
fileNames() 呼び出し
  → SAFにクエリ
  → { "foo.svg": "primary:Download/foo.svg", ... } をキャッシュに保存

write("foo.svg", data)
  → キャッシュから docId を取得
  → docId あり → buildDocumentUriUsingTree → openOutputStream("wt") → 上書き
  → docId なし → createDocument → 新規作成（キャッシュにも追加）
```

`write()` はキャッシュが空の場合、自動的に `fileNames()` を呼んでキャッシュを埋めてから書き込む。
フォルダが変更された場合（`openDialog()` で再選択）はキャッシュを自動クリアする。

---

## 使い方

### 初期化

```cpp
// SvgGallery.h
#include "AndroidFolder.h"

class SvgGallery : public QWidget {
    ...
#ifdef Q_OS_ANDROID
    AndroidFolder *m_androidFolder = nullptr;
#endif
};
```

### フォルダ選択ダイアログ

```cpp
void SvgGallery::browseDirectory()
{
#ifdef Q_OS_ANDROID
    if (!m_androidFolder) {
        m_androidFolder = new AndroidFolder(this);
        connect(m_androidFolder, &AndroidFolder::ready, this, [this](bool ok) {
            if (ok) {
                m_pathInput->setText(m_androidFolder->treeUri());
                loadSvgs();
            }
        });
    }
    m_androidFolder->openDialog();
#else
    QString directory = QFileDialog::getExistingDirectory(...);
    ...
#endif
}
```

### ファイル一覧の取得と読み込み

SAFはファイルパスを持たないため、キャッシュディレクトリに一時書き出して既存のコンポーネント（`SvgPair` 等）に渡す。

```cpp
void SvgGallery::loadSvgs()
{
#ifdef Q_OS_ANDROID
    if (!m_androidFolder || !m_androidFolder->isReady()) {
        showError(tr("Please select a directory first."));
        return;
    }

    // fileNames() がdocumentIdキャッシュも同時に構築する
    QStringList allFiles = m_androidFolder->fileNames();
    QStringList svgFiles, pngFiles;
    for (const QString &f : allFiles) {
        if (f.endsWith(".svg", Qt::CaseInsensitive)) svgFiles << f;
        if (f.endsWith(".png", Qt::CaseInsensitive)) pngFiles << f;
    }

    QString cacheDir = QStandardPaths::writableLocation(
                           QStandardPaths::CacheLocation) + "/svggallery/";
    QDir().mkpath(cacheDir);

    for (const QString &svgFile : svgFiles) {
        QString cachePath = cacheDir + svgFile;
        QByteArray data = m_androidFolder->read(svgFile);
        QFile f(cachePath);
        if (f.open(QIODevice::WriteOnly)) { f.write(data); f.close(); }

        // cachePath を SvgPair に渡す（SvgPair は無改造で動く）
        SvgPair *widget = new SvgPair(cachePath, ...);
        ...
    }
#else
    // デスクトップ版（QDir を使った既存コード）
    ...
#endif
}
```

### ファイルの保存（元フォルダへの書き戻し）

**重要：** キャッシュへの書き込みだけでは元フォルダには反映されない。
必ず `m_androidFolder->write()` で元フォルダにも書き戻すこと。

```cpp
void SvgGallery::saveSvgContent()
{
    QByteArray content = m_editor->text();

    // 1. キャッシュに書き込む（reloadCurrentSvg() のため）
    QFile file(m_currentSvgPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(content);
        file.close();
    }

#ifdef Q_OS_ANDROID
    // 2. SAFの元フォルダに書き戻す
    if (m_androidFolder && m_androidFolder->isReady()) {
        const QString fileName = QFileInfo(m_currentSvgPath).fileName();
        if (!m_androidFolder->write(fileName, content)) {
            showError(tr("Failed to save to original folder: %1").arg(fileName));
            return;
        }
    }
#endif

    reloadCurrentSvg();
}
```

---

## 注意事項

| 項目 | 内容 |
|---|---|
| キャッシュと元フォルダ | `loadSvgs()` はキャッシュに書き出して表示コンポーネントに渡す。保存時は必ず `m_androidFolder->write()` で元フォルダにも書き戻すこと |
| `fileNames()` のタイミング | `write()` 前に `fileNames()` が呼ばれていればキャッシュが有効。未呼び出しの場合は `write()` が自動的に呼ぶ |
| パーミッション | SAFはAndroidManifestへの追加不要。`openDialog()` でユーザが選択した時点で読み書き両方の権限が永続化される |
| フォルダ変更 | `openDialog()` を再度呼べばフォルダを変更できる。キャッシュは自動クリアされる |
| `"wt"` モード | 上書きのtruncateモード。Android 5.0以降で動作 |
