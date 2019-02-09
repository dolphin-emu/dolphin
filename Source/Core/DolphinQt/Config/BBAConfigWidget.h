// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

class QLineEdit;
class QSpinBox;

class BBAConfigWidget : public QDialog {
    Q_OBJECT

public:
    explicit BBAConfigWidget(bool show_server, QWidget* parent = nullptr);

    QString MacAddr() const;
    void SetMacAddr(const QString& mac_addr);

    QString Server() const;
    void SetServer(const QString& server);

    quint16 Port() const;
    void SetPort(quint16 port);

private slots:
    void Submit();
    void GenerateMac();
    void TextChanged(const QString& text);

private:
    QLineEdit *m_mac_addr;
    QLineEdit *m_server { nullptr };
    QSpinBox *m_port { nullptr };
};
