#include "loginwindow.h"
#include "mainwindow.h"

#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>

static const char *kDefaultUser = "koni";
static const char *kDefaultPass = "123";

LoginWindow::LoginWindow(QWidget *parent)
    : QDialog(parent), m_userEdit(nullptr), m_passEdit(nullptr), m_btnLogin(nullptr), m_btnCancel(nullptr), m_mainWindow(nullptr)
{
    setWindowTitle("登录 - AVG 机器人调度系统");
    setMinimumWidth(340);

    auto *userLabel = new QLabel("用户名:", this);
    m_userEdit = new QLineEdit(this);

    auto *passLabel = new QLabel("密码:", this);
    m_passEdit = new QLineEdit(this);
    m_passEdit->setEchoMode(QLineEdit::Password);
    m_passEdit->setPlaceholderText("请输入密码");

    auto *hintLabel = new QLabel(QString("测试账号: %1 / %2").arg(kDefaultUser).arg(kDefaultPass), this);
    hintLabel->setStyleSheet("color: gray;");

    m_btnLogin = new QPushButton("登 录", this);
    m_btnCancel = new QPushButton("取 消", this);

    auto *form = new QFormLayout;
    form->addRow(userLabel, m_userEdit);
    form->addRow(passLabel, m_passEdit);
    form->addRow(hintLabel);

    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(m_btnLogin);
    btnRow->addWidget(m_btnCancel);

    auto *main = new QVBoxLayout(this);
    main->addLayout(form);
    main->addSpacing(12);
    main->addLayout(btnRow);

    connect(m_btnLogin, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);
    connect(m_btnCancel, &QPushButton::clicked, this, &LoginWindow::onCancelClicked);
}

LoginWindow::~LoginWindow()
{
    if (m_mainWindow)
        m_mainWindow->deleteLater();
}

bool LoginWindow::checkAccount(const QString &user, const QString &pass) const
{
    return user == QString::fromLatin1(kDefaultUser) && pass == QString::fromLatin1(kDefaultPass);
}

void LoginWindow::onLoginClicked()
{
    QString user = m_userEdit->text().trimmed();
    QString pass = m_passEdit->text();

    if (user.isEmpty() || pass.isEmpty())
    {
        QMessageBox::warning(this, "提示", "请输入用户名和密码");
        return;
    }

    if (!checkAccount(user, pass))
    {
        QMessageBox::warning(this, "登录失败", "用户名或密码错误");
        m_passEdit->clear();
        m_passEdit->setFocus();
        return;
    }

    openMainWindow();
}

void LoginWindow::onCancelClicked()
{
    reject();
}

void LoginWindow::openMainWindow()
{
    if (m_mainWindow)
    {
        m_mainWindow->deleteLater();
        m_mainWindow = nullptr;
    }

    m_mainWindow = new MainWindow();
    connect(m_mainWindow, &MainWindow::logoutRequested,
            this, &LoginWindow::onMainLogout);

    m_mainWindow->show();
    this->hide();
}

void LoginWindow::onMainLogout()
{
    if (m_mainWindow)
    {
        m_mainWindow->close();
        m_mainWindow->deleteLater();
        m_mainWindow = nullptr;
    }

    m_passEdit->clear();
    m_userEdit->setFocus();
    this->show();
}
