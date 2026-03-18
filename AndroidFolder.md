# AndroidFolder 設計書

## 概要

AndroidのStorage Access Framework (SAF) を通じて、ユーザが選択したフォルダへのアクセスを提供するクラス。  
ファイルのみを含むフラットなフォルダを対象とし、Qt/C++だけで実装する（Javaファイル不要）。

---

## 設計方針

- **シンプルさ最優先**：サブフォルダ・再帰処理は対象外
- **Javaファイルなし**：`QJniObject` のみでAndroid APIを呼ぶ
- **自動パーミッション管理**：許可の取得・永続化をクラス内で完結
- **URI永続化**：`QSettings` に保存し、次回起動時に自動復元
- **非Androidビルドへの配慮**：`#ifdef Q_OS_ANDROID` でガード、他OSではスタブ動作

---

## クラスインタフェース

```cpp
class AndroidFolder : public QObject
{
    Q_OBJECT

public:
    explicit AndroidFolder(QObject *parent = nullptr);

    // --- 状態 ---

    // URIが設定済みでアクセス可能か
    bool isReady() const;

    // 現在保持しているtreeURI（未設定なら空文字）
    QString treeUri() const;

    // --- フォルダ選択 ---

    // SAFのフォルダ選択ダイアログを起動する（非同期）
    // 完了時に ready(true/false) シグナルが発火する
    // すでにisReady()==trueでも呼べる（フォルダを変更できる）
    void openDialog();

    // URIを直接セットする（テスト用・外部から渡す場合）
    // パーミッションの取得と永続化も行う
    bool setUri(const QString &treeUri);

    // 保存済みURIを破棄してリセットする
    void reset();

    // --- ファイルアクセス ---

    // フォルダ内のファイル名一覧を返す
    // isReady()==false なら空リストを返す
    QStringList fileNames() const;

    // ファイルを読み込む（ファイル名で指定）
    // 失敗時は空のQByteArrayを返す
    QByteArray read(const QString &fileName) const;

    // ファイルを書き込む（ファイル名で指定、存在する場合は上書き）
    // 成功でtrue、失敗でfalseを返す
    bool write(const QString &fileName, const QByteArray &data);

    // ファイルを削除する（ファイル名で指定）
    bool remove(const QString &fileName);

    // ファイルが存在するか確認する
    bool exists(const QString &fileName) const;

signals:
    // openDialog() の完了時に発火
    // ok==true：URIが取得されアクセス可能になった
    // ok==false：ユーザがキャンセル、または許可が得られなかった
    void ready(bool ok);

private:
    // QSettings のキー名
    static constexpr const char *kSettingsKey = "AndroidFolder/treeUri";

    QString m_treeUri;

    // 起動時にQSettingsからURIを復元する（コンストラクタから呼ぶ）
    void restoreUri();

    // パーミッションの永続化
    void takePersistPermission(const QString &treeUri);

    // ファイル名からdocumentIdを検索する（内部用）
    QString findDocumentId(const QString &fileName) const;
};
```

---

## 状態遷移

```
[初回起動]
  コンストラクタ
    → QSettings にURIなし
    → isReady() == false

  openDialog() 呼び出し
    → SAFダイアログ表示（ACTION_OPEN_DOCUMENT_TREE）
    → ユーザがキャンセル → ready(false)
    → ユーザが選択
      → takePersistableUriPermission
      → QSettingsに保存
      → isReady() == true
      → ready(true)

[2回目以降の起動]
  コンストラクタ
    → QSettings からURI復元
    → isReady() == true（openDialog不要）
```

---

## ファイル名とdocumentIdの関係

SAFの内部ではファイルを `documentId`（不透明な文字列）で管理する。  
`AndroidFolder` はこの詳細を隠蔽し、呼び出し側は常にファイル名（`QString`）だけを使う。

- `fileNames()` / `read()` / `write()` / `remove()` / `exists()` はすべてファイル名で指定
- `documentId` の検索はクラス内部の `findDocumentId()` が行う
- `write()` で同名ファイルが存在する場合、既存のdocumentIdに上書きする

---

## 使用例

```cpp
// アプリ起動時
auto *folder = new AndroidFolder(this);

connect(folder, &AndroidFolder::ready, this, [folder](bool ok) {
    if (ok) {
        qDebug() << "Files:" << folder->fileNames();
        QByteArray data = folder->read("config.json");
    }
});

if (folder->isReady()) {
    // 前回選択済み → そのまま使える
    qDebug() << folder->fileNames();
} else {
    // 初回 → ダイアログを出す
    folder->openDialog();
}

// ファイル書き込み
folder->write("result.txt", "Hello, SAF!");

// フォルダを変更したいとき
folder->openDialog(); // isReady()==trueでも呼べる
```

---

## 実装メモ（JNI呼び出し先）

| 機能 | 使用するAndroid API |
|---|---|
| ダイアログ起動 | `Intent.ACTION_OPEN_DOCUMENT_TREE` + `QtAndroidPrivate::startActivity` |
| 結果受け取り | `QAndroidActivityResultReceiver` |
| 許可の永続化 | `ContentResolver.takePersistableUriPermission` |
| ファイル一覧 | `ContentResolver.query` + `DocumentsContract.buildChildDocumentsUriUsingTree` |
| 読み込み | `ContentResolver.openInputStream` |
| 書き込み（新規） | `DocumentsContract.createDocument` + `ContentResolver.openOutputStream` |
| 書き込み（上書き） | `ContentResolver.openOutputStream(uri, "wt")` |
| 削除 | `DocumentsContract.deleteDocument` |

---

## 非Androidビルドの扱い

```cpp
#ifdef Q_OS_ANDROID
    // 実際のSAF処理
#else
    // スタブ：isReady()==false固定、openDialog()は即ready(false)を発火
#endif
```

デスクトップでのビルドエラーを防ぐため、`#ifdef` で分岐する。

---

## ファイル構成

```
AndroidFolder.h    ← クラス宣言
AndroidFolder.cpp  ← 実装（#ifdef Q_OS_ANDROID で分岐）
```

外部ライブラリへの依存なし。Qt Coreのみ。
