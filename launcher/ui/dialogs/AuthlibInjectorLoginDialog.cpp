// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2023 Evan Goode <mail@evangoo.de>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "AuthlibInjectorLoginDialog.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui_AuthlibInjectorLoginDialog.h"

#include "Application.h"
#include "GetAuthlibInjectorApiLocation.h"

#include <QtWidgets/QPushButton>

AuthlibInjectorLoginDialog::AuthlibInjectorLoginDialog(QWidget* parent) : QDialog(parent), ui(new Ui::AuthlibInjectorLoginDialog)
{
    ui->setupUi(this);
    ui->userTextBox->setFocus();
    ui->loadingLabel->setVisible(false);
    ui->errorMessage->setVisible(false);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    setAcceptDrops(true);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

AuthlibInjectorLoginDialog::~AuthlibInjectorLoginDialog()
{
    delete ui;
}

QString AuthlibInjectorLoginDialog::fixUrl(QString url)
{
    QString fixed{ url };
    fixed = fixed.trimmed();
    if (!fixed.contains("://")) {
        fixed.prepend("https://");
    }
    if (fixed.endsWith("/")) {
        fixed = fixed.left(fixed.size() - 1);
    }
    return fixed;
}

QUrl AuthlibInjectorLoginDialog::extractAuthlibInjectorUrl(const QMimeData* mimeData)
{
    if (mimeData->hasText()) {
        const auto& text = mimeData->text().toStdString();
        const std::string authlib_injector_uri_prefix{ "authlib-injector:yggdrasil-server:" };
        if (text.rfind(authlib_injector_uri_prefix, 0) == 0) {
            const auto& url_str = text.substr(authlib_injector_uri_prefix.length());
            const auto& url = QUrl(QUrl::fromPercentEncoding(url_str.c_str()));
            if (url.isValid()) {
                return url;
            }
        }
    }
    return {};
}

void AuthlibInjectorLoginDialog::dragEnterEvent(QDragEnterEvent* event)
{
    if (extractAuthlibInjectorUrl(event->mimeData()).isValid()) {
        event->acceptProposedAction();
    }
}

void AuthlibInjectorLoginDialog::dropEvent(QDropEvent* event)
{
    const auto& url = extractAuthlibInjectorUrl(event->mimeData());
    if (url.isValid()) {
        //ui->authlibInjectorTextBox->setText(url.toString());
    }
    event->acceptProposedAction();
}

// Stage 1: User interaction
void AuthlibInjectorLoginDialog::accept()
{
    ui->errorMessage->setVisible(false);
    auto fixedAuthlibInjectorUrl = AuthlibInjectorLoginDialog::fixUrl("https://elysium.lunarie.party/authlib-injector");

/*
    auto response = CustomMessageBox::selectable(this, QObject::tr("Confirm account creation"),
                                                 QObject::tr("Warning: you are about to send the username and password you entered to an "
                                                             "unofficial, third-party authentication server:\n"
                                                             "%1\n\n"
                                                             "Never use your Mojang or Microsoft password for a third-party account!\n\n"
                                                             "Are you sure you want to proceed?")
                                                     .arg(fixedAuthlibInjectorUrl),
                                                 QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                        ->exec();
    if (response != QMessageBox::Yes)
        return;
*/

    setUserInputsEnabled(false);
    ui->loadingLabel->setVisible(true);

    // Get the authlib-injector API root
    auto netJob = NetJob::Ptr(new NetJob("Get authlib-injector API root", APPLICATION->network()));
    netJob->setAskRetry(false);
    netJob->setAutoRetryLimit(0);

    auto username = ui->userTextBox->text();
    m_apiLocationRequest = GetAuthlibInjectorApiLocation::make(fixedAuthlibInjectorUrl, m_account, username);
    netJob->addNetAction(m_apiLocationRequest);

    m_apiLocationTask.reset(netJob);
    connect(netJob.get(), &NetJob::succeeded, this, &AuthlibInjectorLoginDialog::onApiLocationTaskSucceeded);
    connect(netJob.get(), &NetJob::failed, this, &AuthlibInjectorLoginDialog::onApiLocationTaskFailed);
    m_apiLocationTask->start();
}

void AuthlibInjectorLoginDialog::setUserInputsEnabled(bool enable)
{
    ui->userTextBox->setEnabled(enable);
    ui->passTextBox->setEnabled(enable);
    //ui->authlibInjectorTextBox->setEnabled(enable);
    ui->buttonBox->setEnabled(enable);
}

// Enable the OK button only when all textboxes contain something.
void AuthlibInjectorLoginDialog::on_userTextBox_textEdited(const QString& newText)
{
    ui->buttonBox->button(QDialogButtonBox::Ok)
        ->setEnabled(!newText.isEmpty() && !ui->passTextBox->text().isEmpty());
}
void AuthlibInjectorLoginDialog::on_passTextBox_textEdited(const QString& newText)
{
    ui->buttonBox->button(QDialogButtonBox::Ok)
        ->setEnabled(!newText.isEmpty() && !ui->passTextBox->text().isEmpty());
}
void AuthlibInjectorLoginDialog::on_authlibInjectorTextBox_textEdited(const QString& newText)
{
    ui->buttonBox->button(QDialogButtonBox::Ok)
        ->setEnabled(!newText.isEmpty() && !ui->passTextBox->text().isEmpty());
}

void AuthlibInjectorLoginDialog::onApiLocationTaskFailed(const QString& reason)
{
    onTaskFailed(m_apiLocationRequest->errorString());
}

void AuthlibInjectorLoginDialog::onTaskFailed(const QString& reason)
{
    // Set message
    auto lines = reason.split('\n');
    QString processed;
    for (auto line : lines) {
        if (line.size()) {
            processed += "<font color='red'>" + line + "</font><br />";
        } else {
            processed += "<br />";
        }
    }
    ui->errorMessage->setText(processed);
    ui->errorMessage->setVisible(true);

    // Re-enable user-interaction
    setUserInputsEnabled(true);
    ui->loadingLabel->setVisible(false);
}

void AuthlibInjectorLoginDialog::onApiLocationTaskSucceeded()
{
    m_account = m_apiLocationRequest->getAccount();
    m_loginTask = m_account->login(false, ui->passTextBox->text());
    connect(m_loginTask.get(), &Task::failed, this, &AuthlibInjectorLoginDialog::onTaskFailed);
    connect(m_loginTask.get(), &Task::succeeded, this, &AuthlibInjectorLoginDialog::onTaskSucceeded);
    connect(m_loginTask.get(), &Task::status, this, &AuthlibInjectorLoginDialog::onTaskStatus);
    m_loginTask->start();
}

void AuthlibInjectorLoginDialog::onTaskSucceeded()
{
    QDialog::accept();
}

void AuthlibInjectorLoginDialog::onTaskStatus(const QString& status)
{
    ui->errorMessage->setText(status);
}

// Public interface
MinecraftAccountPtr AuthlibInjectorLoginDialog::newAccount(QWidget* parent)
{
    AuthlibInjectorLoginDialog dlg(parent);
    dlg.ui->label->setText(tr("Please enter the username and password you've used to register on the Elysium website."));
    if (dlg.exec() == QDialog::Accepted) {
        return dlg.m_account;
    }
    return nullptr;
}
