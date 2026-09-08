#include "loginwindow.h"
#include "mainwindow.h"

#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>

LoginWindow::LoginWindow(QWidget *parent)
    : QDialog(parent)
    , m_userEdit(nullptr)
    , m_passEdit(nullptr)
    , m_pass2Edit(nullptr)
    , m_btnLogin(nullptr)
    , m_btnRegister(nullptr)
    , m_btnCancel(nullptr)
    , m_modeCombo(nullptr)
    , m_hint(nullptr)
    , m_isAdmin(false)
    , m_mainWindow(nullptr)
{
    setWindowTitle("登录 / 注册 - AVG 物流机器人任务调度系统");
    setMinimumWidth(360);

    auto *userLabel = new QLabel("用户名:", this);
    m_userEdit = new QLineEdit(this);

    auto *passLabel = new QLabel("密码:", this);
    m_passEdit = new QLineEdit(this);
    m_passEdit->setEchoMode(QLineEdit::Password);

    auto *pass2Label = new QLabel("确认密码:", this);
    m_pass2Edit = new QLineEdit(this);
    m_pass2Edit->setEchoMode(QLineEdit::Password);

    // 账号库：载入；无账号时会自动建默认管理员 admin/admin123
    m_users.load();

    m_hint = new QLabel(this);
    m_hint->setStyleSheet("color: #666;");
    if (m_users.count() == 1)
        m_hint->setText("默认管理员: admin / admin123\n(第一个注册的账号会成为管理员)");
    else
        m_hint->setText("第一个注册的账号会成为管理员");

    m_btnLogin = new QPushButton("登 录", this);
    m_btnRegister = new QPushButton("注 册", this);
    m_btnCancel = new QPushButton("取 消", this);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem("模拟机器人(自动移动演示)", 0);
    m_modeCombo->addItem("TCP 接入真实机器人", 1);

    auto *form = new QFormLayout;
    form->addRow(userLabel, m_userEdit);
    form->addRow(passLabel, m_passEdit);
    form->addRow(pass2Label, m_pass2Edit);
    form->addRow("运行模式:", m_modeCombo);
    form->addRow(m_hint);

    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(m_btnLogin);
    btnRow->addWidget(m_btnRegister);
    btnRow->addWidget(m_btnCancel);

    auto *main = new QVBoxLayout(this);
    main->addLayout(form);
    main->addSpacing(12);
    main->addLayout(btnRow);

    connect(m_btnLogin, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);
    connect(m_btnRegister, &QPushButton::clicked, this, &LoginWindow::onRegisterClicked);
    connect(m_btnCancel, &QPushButton::clicked, this, &LoginWindow::onCancelClicked);
}

LoginWindow::~LoginWindow()
{
    if (m_mainWindow)
        m_mainWindow->deleteLater();
}

void LoginWindow::showStatus(const QString &s, bool ok)
{
    m_hint->setStyleSheet(ok ? "color:#1a7f37;" : "color:#d92332;");
    m_hint->setText(s);
}

void LoginWindow::onLoginClicked()
{
    QString user = m_userEdit->text().trimmed();
    QString pass = m_passEdit->text();
    if (user.isEmpty() || pass.isEmpty())
    {
        showStatus("请输入用户名和密码", false);
        return;
    }
    bool isAdmin = false;
    if (!m_users.isValid(user, pass, isAdmin))
    {
        showStatus("用户名或密码错误", false);
        m_passEdit->clear();
        return;
    }
    m_isAdmin = isAdmin;
    openMainWindow();
}

void LoginWindow::onRegisterClicked()
{
    QString user = m_userEdit->text().trimmed();
    QString pass = m_passEdit->text();
    QString pass2 = m_pass2Edit->text();
    if (user.isEmpty() || pass.isEmpty())
    {
        showStatus("请输入用户名和密码", false);
        return;
    }
    if (pass != pass2)
    {
        showStatus("两次输入的密码不一致", false);
        return;
    }
    if (m_users.addUser(user, pass))
    {
        bool isFirst = (m_users.count() == 1);
        showStatus(isFirst ? QString("注册成功，%1 为管理员，请登录").arg(user)
                           : QString("注册成功，%1 为普通用户，请登录").arg(user),
                   true);
        m_passEdit->clear();
        m_pass2Edit->clear();
    }
    else
    {
        showStatus("注册失败：用户名已存在或无效", false);
    }
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

    bool simulate = (m_modeCombo->currentData().toInt() == 0);
    m_mainWindow = new MainWindow(nullptr, simulate, m_isAdmin);
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
    m_pass2Edit->clear();
    m_userEdit->setFocus();
    this->show();
}
