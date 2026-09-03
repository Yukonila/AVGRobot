#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include <QDialog>

class QLineEdit;
class QPushButton;
class MainWindow;

class LoginWindow : public QDialog
{
    Q_OBJECT

public:
    explicit LoginWindow(QWidget *parent = nullptr);
    ~LoginWindow() override;

private slots:
    void onLoginClicked();
    void onCancelClicked();
    void onMainLogout();

private:
    void openMainWindow();
    bool checkAccount(const QString &user, const QString &pass) const;

    QLineEdit *m_userEdit;
    QLineEdit *m_passEdit;
    QPushButton *m_btnLogin;
    QPushButton *m_btnCancel;

    MainWindow *m_mainWindow;
};

#endif // LOGINWINDOW_H
