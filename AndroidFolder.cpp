// AndroidFolder.cpp

#include "AndroidFolder.h"
#include <QSettings>

// ============================================================
//  Android 専用ブロック
// ============================================================
#ifdef Q_OS_ANDROID

#include <QJniEnvironment>
#include <QCoreApplication>

// ── 内部ヘルパー ───────────────────────────────────────────────

/// アプリの Context を QJniObject で返す。
static QJniObject appContext()
{
    return QJniObject(QNativeInterface::QAndroidApplication::context());
}

/// 文字列から android.net.Uri を生成する。
static QJniObject parseUri(const QString &uriStr)
{
    return QJniObject::callStaticObjectMethod(
        "android/net/Uri", "parse",
        "(Ljava/lang/String;)Landroid/net/Uri;",
        QJniObject::fromString(uriStr).object());
}

/// ContentResolver を取得する。
static QJniObject contentResolver()
{
    return appContext().callObjectMethod(
        "getContentResolver",
        "()Landroid/content/ContentResolver;");
}

/// ファイル拡張子から MIME タイプを返す（簡易版）。
static QString mimeTypeFor(const QString &fileName)
{
    const QString ext = fileName.section(QLatin1Char('.'), -1).toLower();
    // よく使う拡張子のみ。該当なしは octet-stream。
    static const QHash<QString, QString> table = {
        {QStringLiteral("txt"),  QStringLiteral("text/plain")},
        {QStringLiteral("html"), QStringLiteral("text/html")},
        {QStringLiteral("htm"),  QStringLiteral("text/html")},
        {QStringLiteral("csv"),  QStringLiteral("text/csv")},
        {QStringLiteral("json"), QStringLiteral("application/json")},
        {QStringLiteral("xml"),  QStringLiteral("application/xml")},
        {QStringLiteral("pdf"),  QStringLiteral("application/pdf")},
        {QStringLiteral("zip"),  QStringLiteral("application/zip")},
        {QStringLiteral("png"),  QStringLiteral("image/png")},
        {QStringLiteral("jpg"),  QStringLiteral("image/jpeg")},
        {QStringLiteral("jpeg"), QStringLiteral("image/jpeg")},
        {QStringLiteral("gif"),  QStringLiteral("image/gif")},
        {QStringLiteral("svg"),  QStringLiteral("image/svg+xml")},
        {QStringLiteral("mp3"),  QStringLiteral("audio/mpeg")},
        {QStringLiteral("mp4"),  QStringLiteral("video/mp4")},
    };
    return table.value(ext, QStringLiteral("application/octet-stream"));
}

// DocumentsContract 定数（JNI呼び出しで使うカラム名）
namespace DocCol {
static constexpr const char *DocumentId   = "document_id";
static constexpr const char *DisplayName  = "_display_name";
static constexpr const char *MimeType     = "mime_type";
static constexpr const char *DirMimeType  = "vnd.android.document/directory";
}

// ── ResultReceiver ─────────────────────────────────────────────
// QAndroidActivityResultReceiver を継承して onActivityResult を橋渡しする。

class AndroidFolder::ResultReceiver : public QAndroidActivityResultReceiver
{
public:
    explicit ResultReceiver(AndroidFolder *owner) : m_owner(owner) {}

    void handleActivityResult(int receiverRequestCode,
                               int resultCode,
                               const QJniObject &data) override
    {
        m_owner->onActivityResult(receiverRequestCode, resultCode, data);
    }

private:
    AndroidFolder *m_owner;
};

#endif // Q_OS_ANDROID

// ============================================================
//  コンストラクタ / デストラクタ
// ============================================================

AndroidFolder::AndroidFolder(QObject *parent)
    : QObject(parent)
{
#ifdef Q_OS_ANDROID
    m_receiver = new ResultReceiver(this);
#endif
    restoreUri();
}

AndroidFolder::~AndroidFolder()
{
#ifdef Q_OS_ANDROID
    delete m_receiver;
#endif
}

// ============================================================
//  Public API
// ============================================================

bool AndroidFolder::isReady() const
{
    return !m_treeUri.isEmpty();
}

QString AndroidFolder::treeUri() const
{
    return m_treeUri;
}

// ── openDialog ─────────────────────────────────────────────────

void AndroidFolder::openDialog()
{
#ifdef Q_OS_ANDROID
    // ACTION_OPEN_DOCUMENT_TREE インテントを作成
    QJniObject intent("android/content/Intent");
    intent.callObjectMethod(
        "setAction",
        "(Ljava/lang/String;)Landroid/content/Intent;",
        QJniObject::fromString(
            QStringLiteral("android.intent.action.OPEN_DOCUMENT_TREE")).object());

    // 結果を m_receiver（ResultReceiver）で受け取る
    QtAndroidPrivate::startActivity(intent, kRequestCode, m_receiver);
#else
    // 非 Android では即座に失敗を通知
    emit ready(false);
#endif
}

// ── setUri ─────────────────────────────────────────────────────

bool AndroidFolder::setUri(const QString &treeUri)
{
    if (treeUri.isEmpty())
        return false;

#ifdef Q_OS_ANDROID
    takePersistPermission(treeUri);
#endif

    m_treeUri = treeUri;
    QSettings().setValue(QLatin1String(kSettingsKey), treeUri);
    return true;
}

// ── reset ──────────────────────────────────────────────────────

void AndroidFolder::reset()
{
    m_treeUri.clear();
    m_docIdCache.clear();
    QSettings().remove(QLatin1String(kSettingsKey));
}

void AndroidFolder::invalidateCache()
{
    m_docIdCache.clear();
}

// ── fileNames ──────────────────────────────────────────────────

QStringList AndroidFolder::fileNames() const
{
#ifdef Q_OS_ANDROID
    if (!isReady())
        return {};

    QJniEnvironment env;
    QJniObject treeUriObj = parseUri(m_treeUri);

    // フォルダのルート documentId を取得
    QJniObject rootDocId = QJniObject::callStaticObjectMethod(
        "android/provider/DocumentsContract",
        "getTreeDocumentId",
        "(Landroid/net/Uri;)Ljava/lang/String;",
        treeUriObj.object());

    // 子アイテムを列挙するための URI を生成
    QJniObject childrenUri = QJniObject::callStaticObjectMethod(
        "android/provider/DocumentsContract",
        "buildChildDocumentsUriUsingTree",
        "(Landroid/net/Uri;Ljava/lang/String;)Landroid/net/Uri;",
        treeUriObj.object(), rootDocId.object());

    // Projection: [_display_name, mime_type, document_id]
    jclass strCls = env->FindClass("java/lang/String");
    jobjectArray proj = env->NewObjectArray(3, strCls, nullptr);
    env->SetObjectArrayElement(
        proj, 0, QJniObject::fromString(QLatin1String(DocCol::DisplayName)).object());
    env->SetObjectArrayElement(
        proj, 1, QJniObject::fromString(QLatin1String(DocCol::MimeType)).object());
    env->SetObjectArrayElement(
        proj, 2, QJniObject::fromString(QLatin1String(DocCol::DocumentId)).object());

    QJniObject cursor = contentResolver().callObjectMethod(
        "query",
        "(Landroid/net/Uri;[Ljava/lang/String;Ljava/lang/String;"
        "[Ljava/lang/String;Ljava/lang/String;)Landroid/database/Cursor;",
        childrenUri.object(), proj,
        /*selection*/    nullptr,
        /*selectionArgs*/nullptr,
        /*sortOrder*/    nullptr);

    env->DeleteLocalRef(proj);
    env->DeleteLocalRef(strCls);

    QStringList result;
    if (cursor.isValid()) {
        m_docIdCache.clear();   // ← クエリのたびにキャッシュ更新
        while (cursor.callMethod<jboolean>("moveToNext")) {
            // col 1 = mime_type
            const QString mime =
                cursor.callObjectMethod("getString", "(I)Ljava/lang/String;",
                                        static_cast<jint>(1)).toString();
            // ディレクトリは除外（フラットフォルダの仕様）
            if (mime != QLatin1String(DocCol::DirMimeType)) {
                // col 0 = _display_name, col 2 = document_id
                const QString name = cursor.callObjectMethod(
                    "getString", "(I)Ljava/lang/String;",
                    static_cast<jint>(0)).toString();
                const QString docId = cursor.callObjectMethod(
                    "getString", "(I)Ljava/lang/String;",
                    static_cast<jint>(2)).toString();
                result << name;
                m_docIdCache.insert(name, docId);   // ← キャッシュに追加
                qDebug() << "[AndroidFolder::fileNames]" << name << "->" << docId;
            }
        }
        cursor.callMethod<void>("close");
    }
    qDebug() << "[AndroidFolder::fileNames] treeUri:" << m_treeUri;
    qDebug() << "[AndroidFolder::fileNames] total files:" << result.size();
    return result;

#else
    return {};
#endif
}

// ── read ───────────────────────────────────────────────────────

QByteArray AndroidFolder::read(const QString &fileName) const
{
#ifdef Q_OS_ANDROID
    if (!isReady())
        return {};

    const QString docId = findDocumentId(fileName);
    if (docId.isEmpty()) {
        qDebug() << "[AndroidFolder::read] docId NOT FOUND for:" << fileName;
        return {};
    }

    // documentId から完全な URI を構築
    QJniObject docUri = QJniObject::callStaticObjectMethod(
        "android/provider/DocumentsContract",
        "buildDocumentUriUsingTree",
        "(Landroid/net/Uri;Ljava/lang/String;)Landroid/net/Uri;",
        parseUri(m_treeUri).object(),
        QJniObject::fromString(docId).object());

    qDebug() << "[AndroidFolder::read] fileName:" << fileName;
    qDebug() << "[AndroidFolder::read] docId:" << docId;
    qDebug() << "[AndroidFolder::read] docUri:" << docUri.toString();

    // InputStream を開く
    QJniObject inputStream = contentResolver().callObjectMethod(
        "openInputStream",
        "(Landroid/net/Uri;)Ljava/io/InputStream;",
        docUri.object());

    if (!inputStream.isValid())
        return {};

    // ByteArrayOutputStream にストリームを流し込む
    QJniObject baos("java/io/ByteArrayOutputStream");
    QJniEnvironment env;
    jbyteArray buf = env->NewByteArray(8192);

    jint n;
    while ((n = inputStream.callMethod<jint>("read", "([B)I", buf)) > 0) {
        baos.callMethod<void>("write", "([BII)V",
                              buf, static_cast<jint>(0), n);
    }
    inputStream.callMethod<void>("close");
    env->DeleteLocalRef(buf);

    // jbyteArray → QByteArray
    QJniObject jBytes = baos.callObjectMethod("toByteArray", "()[B");
    jbyteArray jba    = jBytes.object<jbyteArray>();
    const jsize len   = env->GetArrayLength(jba);
    QByteArray result(static_cast<int>(len), Qt::Uninitialized);
    env->GetByteArrayRegion(jba, 0, len,
                            reinterpret_cast<jbyte *>(result.data()));
    return result;

#else
    Q_UNUSED(fileName)
    return {};
#endif
}

// ── write ──────────────────────────────────────────────────────

bool AndroidFolder::write(const QString &fileName, const QByteArray &data)
{
#ifdef Q_OS_ANDROID
    if (!isReady())
        return false;

    // キャッシュが空なら fileNames() で先にキャッシュを埋める
    if (m_docIdCache.isEmpty())
        fileNames();

    qDebug() << "[AndroidFolder::write] fileName:" << fileName;
    qDebug() << "[AndroidFolder::write] data size:" << data.size();
    qDebug() << "[AndroidFolder::write] cache size:" << m_docIdCache.size();
    qDebug() << "[AndroidFolder::write] cache keys:" << m_docIdCache.keys();

    QJniObject cr          = contentResolver();
    QJniObject treeUriObj  = parseUri(m_treeUri);
    const QString mime     = mimeTypeFor(fileName);
    const QString docId    = findDocumentId(fileName);

    qDebug() << "[AndroidFolder::write] found docId:" << docId;
    qDebug() << "[AndroidFolder::write] path:" << (docId.isEmpty() ? "NEW FILE" : "OVERWRITE");

    QJniObject docUri;

    if (docId.isEmpty()) {
        // ── 新規作成 ──────────────────────────────────────────
        // フォルダのルート documentUri を取得
        QJniObject rootDocId = QJniObject::callStaticObjectMethod(
            "android/provider/DocumentsContract",
            "getTreeDocumentId",
            "(Landroid/net/Uri;)Ljava/lang/String;",
            treeUriObj.object());

        QJniObject parentDocUri = QJniObject::callStaticObjectMethod(
            "android/provider/DocumentsContract",
            "buildDocumentUriUsingTree",
            "(Landroid/net/Uri;Ljava/lang/String;)Landroid/net/Uri;",
            treeUriObj.object(), rootDocId.object());

        docUri = QJniObject::callStaticObjectMethod(
            "android/provider/DocumentsContract",
            "createDocument",
            "(Landroid/content/ContentResolver;Landroid/net/Uri;"
            "Ljava/lang/String;Ljava/lang/String;)Landroid/net/Uri;",
            cr.object(),
            parentDocUri.object(),
            QJniObject::fromString(mime).object(),
            QJniObject::fromString(fileName).object());

        // 新規作成した documentId をキャッシュに追加
        if (docUri.isValid()) {
            QJniObject newDocId = QJniObject::callStaticObjectMethod(
                "android/provider/DocumentsContract",
                "getDocumentId",
                "(Landroid/net/Uri;)Ljava/lang/String;",
                docUri.object());
            if (newDocId.isValid())
                m_docIdCache.insert(fileName, newDocId.toString());
        }
    } else {
        // ── 上書き ────────────────────────────────────────────
        docUri = QJniObject::callStaticObjectMethod(
            "android/provider/DocumentsContract",
            "buildDocumentUriUsingTree",
            "(Landroid/net/Uri;Ljava/lang/String;)Landroid/net/Uri;",
            treeUriObj.object(),
            QJniObject::fromString(docId).object());
    }

    if (!docUri.isValid())
        return false;

    // "wt" = write + truncate（既存ファイルの内容をクリアして書き込む）
    // 新規作成の場合はファイルが空なので "w" でも同じだが統一して "wt" を使用。
    QJniObject outputStream = cr.callObjectMethod(
        "openOutputStream",
        "(Landroid/net/Uri;Ljava/lang/String;)Ljava/io/OutputStream;",
        docUri.object(),
        QJniObject::fromString(QStringLiteral("wt")).object());

    if (!outputStream.isValid()) {
        qDebug() << "[AndroidFolder::write] openOutputStream FAILED";
        return false;
    }

    // QByteArray → jbyteArray → OutputStream.write()
    QJniEnvironment env;
    jbyteArray jba = env->NewByteArray(data.size());
    env->SetByteArrayRegion(jba, 0, data.size(),
                            reinterpret_cast<const jbyte *>(data.constData()));
    outputStream.callMethod<void>("write", "([B)V", jba);
    outputStream.callMethod<void>("close");
    env->DeleteLocalRef(jba);
    qDebug() << "[AndroidFolder::write] SUCCESS";
    return true;

#else
    Q_UNUSED(fileName) Q_UNUSED(data)
    return false;
#endif
}

// ── remove ─────────────────────────────────────────────────────

bool AndroidFolder::remove(const QString &fileName)
{
#ifdef Q_OS_ANDROID
    if (!isReady())
        return false;

    const QString docId = findDocumentId(fileName);
    if (docId.isEmpty())
        return false;

    QJniObject docUri = QJniObject::callStaticObjectMethod(
        "android/provider/DocumentsContract",
        "buildDocumentUriUsingTree",
        "(Landroid/net/Uri;Ljava/lang/String;)Landroid/net/Uri;",
        parseUri(m_treeUri).object(),
        QJniObject::fromString(docId).object());

    return static_cast<bool>(
        QJniObject::callStaticMethod<jboolean>(
            "android/provider/DocumentsContract",
            "deleteDocument",
            "(Landroid/content/ContentResolver;Landroid/net/Uri;)Z",
            contentResolver().object(),
            docUri.object()));

#else
    Q_UNUSED(fileName)
    return false;
#endif
}

// ── exists ─────────────────────────────────────────────────────

bool AndroidFolder::exists(const QString &fileName) const
{
    return !findDocumentId(fileName).isEmpty();
}

// ============================================================
//  Private 実装
// ============================================================

void AndroidFolder::restoreUri()
{
    // QSettings から保存済み URI を復元する。
    // パーミッションは以前の起動で永続化されているので再取得不要。
    const QString saved =
        QSettings().value(QLatin1String(kSettingsKey)).toString();
    if (!saved.isEmpty())
        m_treeUri = saved;
}

// ── Android 専用 private ───────────────────────────────────────

#ifdef Q_OS_ANDROID

void AndroidFolder::takePersistPermission(const QString &uriStr)
{
    // Intent.FLAG_GRANT_READ_URI_PERMISSION  = 0x00000001
    // Intent.FLAG_GRANT_WRITE_URI_PERMISSION = 0x00000002
    constexpr jint flags = 0x00000001 | 0x00000002;

    contentResolver().callMethod<void>(
        "takePersistableUriPermission",
        "(Landroid/net/Uri;I)V",
        parseUri(uriStr).object(),
        flags);
}

void AndroidFolder::onActivityResult(int requestCode,
                                     int resultCode,
                                     const QJniObject &intentData)
{
    if (requestCode != kRequestCode)
        return;

    // Activity.RESULT_OK == -1
    if (resultCode != -1 || !intentData.isValid()) {
        emit ready(false);
        return;
    }

    QJniObject uri =
        intentData.callObjectMethod("getData", "()Landroid/net/Uri;");

    if (!uri.isValid()) {
        emit ready(false);
        return;
    }

    const QString uriStr =
        uri.callObjectMethod("toString", "()Ljava/lang/String;").toString();

    // パーミッション永続化・QSettings 保存
    setUri(uriStr);
    invalidateCache();
    emit ready(true);
}

QString AndroidFolder::findDocumentId(const QString &fileName) const
{
    // まずキャッシュを確認（fileNames()呼び出し済みなら即返る）
    if (m_docIdCache.contains(fileName))
        return m_docIdCache.value(fileName);

    // キャッシュにない場合はSAFに直接クエリ（フォールバック）
    if (!isReady())
        return {};

    QJniEnvironment env;
    QJniObject treeUriObj = parseUri(m_treeUri);

    QJniObject rootDocId = QJniObject::callStaticObjectMethod(
        "android/provider/DocumentsContract",
        "getTreeDocumentId",
        "(Landroid/net/Uri;)Ljava/lang/String;",
        treeUriObj.object());

    QJniObject childrenUri = QJniObject::callStaticObjectMethod(
        "android/provider/DocumentsContract",
        "buildChildDocumentsUriUsingTree",
        "(Landroid/net/Uri;Ljava/lang/String;)Landroid/net/Uri;",
        treeUriObj.object(), rootDocId.object());

    // Projection: [document_id, _display_name]
    jclass strCls = env->FindClass("java/lang/String");
    jobjectArray proj = env->NewObjectArray(2, strCls, nullptr);
    env->SetObjectArrayElement(
        proj, 0, QJniObject::fromString(QLatin1String(DocCol::DocumentId)).object());
    env->SetObjectArrayElement(
        proj, 1, QJniObject::fromString(QLatin1String(DocCol::DisplayName)).object());

    QJniObject cursor = contentResolver().callObjectMethod(
        "query",
        "(Landroid/net/Uri;[Ljava/lang/String;Ljava/lang/String;"
        "[Ljava/lang/String;Ljava/lang/String;)Landroid/database/Cursor;",
        childrenUri.object(), proj,
        nullptr, nullptr, nullptr);

    env->DeleteLocalRef(proj);
    env->DeleteLocalRef(strCls);

    QString result;
    if (cursor.isValid()) {
        while (cursor.callMethod<jboolean>("moveToNext")) {
            const QString name =
                cursor.callObjectMethod("getString", "(I)Ljava/lang/String;",
                                        static_cast<jint>(1)).toString();
            if (name == fileName) {
                result = cursor.callObjectMethod(
                    "getString", "(I)Ljava/lang/String;",
                    static_cast<jint>(0)).toString();
                m_docIdCache.insert(fileName, result);  // ← フォールバック結果もキャッシュ
                break;
            }
        }
        cursor.callMethod<void>("close");
    }
    return result;
}

#else // !Q_OS_ANDROID  ─────────────────── スタブ ───────────────

void AndroidFolder::takePersistPermission(const QString &) {}

QString AndroidFolder::findDocumentId(const QString &) const { return {}; }

#endif // Q_OS_ANDROID
