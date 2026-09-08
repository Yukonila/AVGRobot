#ifndef USERMANAGER_H
#define USERMANAGER_H

#include <QObject>
#include <QString>
#include <QHash>
#include <QJsonObject>

// 账号角色：第一个注册账号自动成为管理员，之后注册为普通用户
struct UserAccount
{
    QString username;
    QString password;
    bool isAdmin = false;
};

// 用户账号的 JSON 持久化与校验
class UserManager : public QObject
{
    Q_OBJECT
public:
    explicit UserManager(QObject *parent = nullptr);

    static QString defaultPath(); // 默认 <运行目录>/Data/users.json

    bool load(const QString &path = QString()); // 载入；文件不存在则创建默认管理员
    bool save(const QString &path = QString()) const;

    bool addUser(const QString &user, const QString &pass, bool forceAdmin = false);
    bool isValid(const QString &user, const QString &pass, bool &isAdmin) const;
    int count() const { return m_users.size(); }
    QStringList usernames() const { return m_users.keys(); }
    QList<UserAccount> accounts() const { return m_users.values(); }

    // 管理操作(管理员)
    bool removeUser(const QString &user);
    bool setRole(const QString &user, bool admin);
    bool changePassword(const QString &user, const QString &newPass);

signals:
    void logMessage(const QString &msg, int level);

private:
    void ensureDefaultAdmin();
    QHash<QString, UserAccount> m_users; // 用户名 -> 账号
};

#endif // USERMANAGER_H
