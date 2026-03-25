#pragma once

// AndroidFolder.h
// SAF (Storage Access Framework) を通じてユーザ選択フォルダにアクセスするクラス。
// フラットなフォルダ（ファイルのみ、サブフォルダ対象外）を対象とする。
// Javaファイル不要。QJniObject のみで実装。Qt 6.2+ 対象。
//
// .pro への追加:
//   android: QT += core
//   ANDROID_ABIS = arm64-v8a   # 必要に応じて

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QStringList>
#include <QHash>

#ifdef Q_OS_ANDROID
#  include <QJniObject>
// Qt 6.2～6.10+: .pro に QT += core-private が必要
#  include <QtCore/private/qandroidextras_p.h>
#endif

class AndroidFolder : public QObject
{
    Q_OBJECT

public:
    explicit AndroidFolder(QObject *parent = nullptr);
    ~AndroidFolder() override;

    // ── 状態 ────────────────────────────────────────────────
    /// URIが設定済みでアクセス可能か
    bool    isReady() const;
    /// 現在保持している treeURI（未設定なら空文字）
    QString treeUri() const;

    // ── フォルダ選択 ─────────────────────────────────────────
    /// SAF フォルダ選択ダイアログを起動する（非同期）。
    /// 完了時に ready(true/false) シグナルが発火する。
    /// isReady()==true でも呼べる（フォルダの変更が可能）。
    void openDialog();

    /// URI を直接セットする（テスト用・外部から渡す場合）。
    /// パーミッションの取得と QSettings 保存も行う。
    bool setUri(const QString &treeUri);

    /// 保存済み URI を破棄してリセットする。
    void reset();

    // ── ファイルアクセス ─────────────────────────────────────
    /// フォルダ内のファイル名一覧を返す。
    /// isReady()==false なら空リストを返す。
    QStringList fileNames() const;

    /// ファイルを読み込む（ファイル名で指定）。
    /// 失敗時は空の QByteArray を返す。
    QByteArray read(const QString &fileName) const;

    /// ファイルを書き込む（ファイル名で指定）。
    /// 同名ファイルが存在する場合は上書き、なければ新規作成。
    /// 成功で true、失敗で false を返す。
    bool write(const QString &fileName, const QByteArray &data);

    /// ファイルを削除する（ファイル名で指定）。
    bool remove(const QString &fileName);

    /// ファイルが存在するか確認する。
    bool exists(const QString &fileName) const;

signals:
    /// openDialog() の完了時に発火。
    /// ok==true  : URI が取得されアクセス可能になった。
    /// ok==false : ユーザがキャンセル、または許可が得られなかった。
    void ready(bool ok);

private:
    static constexpr const char *kSettingsKey = "AndroidFolder/treeUri";

    QString m_treeUri;

    /// fileNames() 時に fileName→documentId をキャッシュする。
    /// write()/remove()/exists() で再クエリ不要になる。
    mutable QHash<QString, QString> m_docIdCache;

    /// キャッシュを無効化する（フォルダ変更時）。
    void invalidateCache();

    /// 起動時に QSettings から URI を復元する（コンストラクタから呼ぶ）。
    void restoreUri();

    /// パーミッションの永続化。
    void takePersistPermission(const QString &uriStr);

    /// ファイル名から documentId を検索する（内部用）。
    QString findDocumentId(const QString &fileName) const;

#ifdef Q_OS_ANDROID
    /// ACTION_OPEN_DOCUMENT_TREE の結果を受け取る内部クラス。
    class ResultReceiver;
    friend class ResultReceiver;

    void onActivityResult(int requestCode, int resultCode,
                          const QJniObject &intentData);

    static constexpr int kRequestCode = 0xAF01;

    ResultReceiver *m_receiver = nullptr;
#endif
};
