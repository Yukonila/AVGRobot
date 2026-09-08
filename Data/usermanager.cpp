#include "usermanager.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>

UserManager::UserManager(QObject *parent)
    : QObject(parent)
{
}

QString UserManager::defaultPath()
{
    QDir dir = QDir::current();
    if (!dir.exists("Data"))
        dir.mkdir("Data");
    return dir.absolutePath() + "/Data/users.json";
}

void UserManager::ensureDefaultAdmin()
{
    // 确保唯一管理员 koni/123 存在
    auto it = m_users.find("koni");
    if (it == m_users.end())
    {
        UserAccount a;
        a.username = "koni";
        a.password = "123";
        a.isAdmin = true;
        m_users.insert("koni", a);
        save();
        emit logMessage("[账号] 已创建唯一管理员 koni/123", 0);
    }
    else if (!it->isAdmin)
    {
        it->isAdmin = true;
        save();
    }
}

bool UserManager::load(const QString &path)
{
    QString filePath = path.isEmpty() ? defaultPath() : path;
    m_users.clear();
    if (QFile::exists(filePath))
    {
        QFile f(filePath);
        if (f.open(QIODevice::ReadOnly))
        {
            QByteArray data = f.readAll();
            f.close();
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(data, &err);
            if (err.error == QJsonParseError::NoError && doc.isObject())
            {
                QJsonObject root = doc.object();
                if (root["users"].isArray())
                {
                    for (const QJsonValue &v : root["users"].toArray())
                    {
                        QJsonObject u = v.toObject();
                        UserAccount acc;
                        acc.username = u["username"].toString();
                        acc.password = u["password"].toString();
                        acc.isAdmin = (u["role"].toString() == "admin") || acc.username == "koni";
                        if (!acc.username.isEmpty())
                            m_users.insert(acc.username, acc);
                    }
                }
            }
        }
    }
    ensureDefaultAdmin();
    return true;
}

bool UserManager::save(const QString &path) const
{
    QString filePath = path.isEmpty() ? defaultPath() : path;
    QJsonArray arr;
    for (const UserAccount &acc : m_users)
    {
        QJsonObject o;
        o["username"] = acc.username;
        o["password"] = acc.password;
        o["role"] = acc.isAdmin ? "admin" : "user";
        arr.append(o);
    }
    QJsonObject root;
    root["users"] = arr;
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

bool UserManager::addUser(const QString &user, const QString &pass, bool forceAdmin)
{
    QString u = user.trimmed();
    if (u.isEmpty() || pass.isEmpty() || m_users.contains(u))
        return false;
    UserAccount acc;
    acc.username = u;
    acc.password = pass;
    // 注册的账号默认为普通用户；管理员只有内置的 koni(或强制指定)
    acc.isAdmin = forceAdmin;
    m_users.insert(u, acc);
    save();
    return true;
}

bool UserManager::isValid(const QString &user, const QString &pass, bool &isAdmin) const
{
    auto it = m_users.constFind(user.trimmed());
    if (it == m_users.constEnd())
        return false;
    if (it->password != pass)
        return false;
    isAdmin = it->isAdmin;
    return true;
}

bool UserManager::removeUser(const QString &user)
{
    if (user.trimmed().isEmpty())
        return false;
    if (user.trimmed() == "koni")
        return false; // 唯一管理员 koni 不可删除
    auto it = m_users.find(user);
    if (it == m_users.end())
        return false;
    if (it->isAdmin && count() == 1)
        return false; // 不能删除最后一个管理员
    m_users.erase(it);
    return save();
}

bool UserManager::setRole(const QString &user, bool admin)
{
    auto it = m_users.find(user);
    if (it == m_users.end())
        return false;
    it->isAdmin = admin;
    return save();
}

bool UserManager::changePassword(const QString &user, const QString &newPass)
{
    if (newPass.isEmpty())
        return false;
    auto it = m_users.find(user);
    if (it == m_users.end())
        return false;
    it->password = newPass;
    return save();
}
