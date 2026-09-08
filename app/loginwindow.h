#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include <QDialog>
#include "usermanager.h"

class QLineEdit;
class QPushButton;
class QComboBox;
class QLabel;
class MainWindow;

class LoginWindow : public QDialog
{
    Q_OBJECT

public:
    explicit LoginWindow(QWidget *parent = nullptr);
    ~LoginWindow() override;

private slots:
    void onLoginClicked();
    void onRegisterClicked();
    void onCancelClicked();
    void onMainLogout();

private:
    void openMainWindow();
    void showStatus(const QString &s, bool ok);

    QLineEdit *m_userEdit;
    QLineEdit *m_passEdit;
    QLineEdit *m_pass2Edit; // 注册确认密码
    QPushButton *m_btnLogin;
    QPushButton *m_btnRegister;
    QPushButton *m_btnCancel;
    QComboBox *m_modeCombo; // 运行模式
    QLabel *m_hint;

    UserManager m_users;
    bool m_isAdmin;
    MainWindow *m_mainWindow;
};

#endif // LOGINWINDOW_H
