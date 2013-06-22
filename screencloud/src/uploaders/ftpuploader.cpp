//
// ScreenCloud - An easy to use screenshot sharing application
// Copyright (C) 2013 Olav Sortland Thoresen <olav.s.th@gmail.com>
//
// This program is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation; either version 2 of the License, or (at your option) any later
// version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
// PARTICULAR PURPOSE. See the GNU General Public License for more details.
//

#include "ftpuploader.h"
#include <QDebug>


FTPUploader::FTPUploader(QObject *parent) :
    Uploader(parent)
{
    name = "FTP";
    shortname = "ftp";
    icon = QIcon(":/uploaders/ftp.png");
    ftp = new QFtp(this);
    connect(ftp, SIGNAL(commandFinished(int, bool)),this, SLOT(ftpCommandFinished(int, bool)) );
    connect(ftp, SIGNAL(listInfo(const QUrlInfo &)),this, SLOT(ftpListInfo(const QUrlInfo &)));
    connect(ftp, SIGNAL(dataTransferProgress(qint64, qint64) ),this, SLOT(updateDataTransferProgress(qint64, qint64)));
    loadSettings();
}
FTPUploader::~FTPUploader()
{
    delete ftp;
}

void FTPUploader::loadSettings()
{
    QSettings settings("screencloud", "ScreenCloud");
    settings.beginGroup("uploaders");
    settings.beginGroup(shortname);
    host = settings.value("host", QString("")).toString();
    port = settings.value("port", 21).toUInt();
    username = settings.value("username", QString("")).toString();
    password = Security::decrypt(settings.value("password", QString("")).toString());
    if(!path.isEmpty() && (path.at(path.size() -1) != QChar('/') || path.at(path.size() -1) != QDir::separator()) ) //Make sure that the path has a separator at the end
    {
        path = path + "/";
    }
    path = settings.value("path", QString("/")).toString();
    prefix = settings.value("prefix",  "").toString();
    suffix = settings.value("suffix", QDate::currentDate().toString("yyyy-MM-dd") + " " + QTime::currentTime().toString("hh:mm:ss")).toString();
    urlToPath = settings.value("url", "").toString();
    if(!urlToPath.isEmpty() && urlToPath.at(urlToPath.size() -1) != QChar('/')) //Make sure that the url has a separator at the end
    {
        urlToPath = urlToPath + "/";
    }
    screenshotName = settings.value("name", "Screenshot at ").toString();
    filenameSetExternally = false;
    //Date time settings
    dateTimeAsPrefix = settings.value("dateTimeAsPrefix", false).toBool();
    dateTimeAsSuffix = settings.value("dateTimeAsSuffix", true).toBool();
    settings.beginGroup("prefix-datetime-settings");
    dateTimeFormatPrefix = settings.value("date-time-format", QString("")).toString();
    settings.endGroup();
    settings.beginGroup("suffix-datetime-settings");
    dateTimeFormatSuffix = settings.value("date-time-format", QString("yyyy-MM-dd hh:mm:ss")).toString();
    settings.endGroup();
    //Load the date/time before showing the filename
    if(dateTimeAsPrefix)
    {
        prefix = QDateTime::currentDateTime().toString(dateTimeFormatPrefix);
    }
    if(dateTimeAsSuffix)
    {
        suffix = QDateTime::currentDateTime().toString(dateTimeFormatSuffix);
    }
    //Finally, set the filename
    filename = prefix + screenshotName + suffix + "." + format;

    settings.endGroup();
    settings.endGroup(); //uploaders

    settings.beginGroup("general");
    format = settings.value("format", "png").toString();
    jpegQuality = settings.value("jpeg-quality", 90).toInt();
    settings.endGroup();
}
void FTPUploader::saveSettings()
{
    QSettings settings("screencloud", "ScreenCloud");
    settings.beginGroup("uploaders");
    settings.beginGroup(shortname);
    settings.setValue("host", uiInputHost->text());
    settings.setValue("port", uiInputPort->text());
    settings.setValue("username", uiInputUsername->text());
    settings.setValue("password", Security::encrypt(uiInputPassword->text()));
    settings.setValue("path", uiInputDir->text());
    settings.setValue("url", uiInputURL->text());
    if(!dateTimeAsPrefix)
    {
        settings.setValue("prefix", uiInputPrefix->text());
    }else
    {
        settings.remove("prefix");
    }
    settings.setValue("name", uiInputName->text());
    if(!dateTimeAsSuffix)
    {
        settings.setValue("suffix", uiInputSuffix->text());
    }else
    {
        settings.remove("suffix");
    }
    settings.setValue("dateTimeAsPrefix", uiCheckBoxDateTimeAsPrefix->isChecked());
    settings.setValue("dateTimeAsSuffix", uiCheckBoxDateTimeAsSuffix->isChecked());
    settings.setValue("configured", true);
    settings.endGroup();
    settings.endGroup();
}


void FTPUploader::upload(QImage *screenshot)
{
    if(dateTimeAsPrefix)
    {
        prefix = QDateTime::currentDateTime().toString(dateTimeFormatPrefix);
    }
    if(dateTimeAsSuffix)
    {
        suffix = QDateTime::currentDateTime().toString(dateTimeFormatSuffix);
    }
    if(!filenameSetExternally)
    {
        filename = validateFilename(prefix + screenshotName + suffix + QString(".") + format);
    }
    //Save to a buffer
    buffer = new QBuffer(&bufferArray, this);
    buffer->open(QIODevice::WriteOnly);
    QString remoteFileName = path + filename;
    qDebug() << "Remote filename: " << remoteFileName.toLatin1();
    if(format == "jpg")
    {
        if(!screenshot->save(buffer, format.toAscii(), jpegQuality))
        {
            emit uploadingError("Failed to save screenshot to buffer");
        }
    }else
    {
        if(!screenshot->save(buffer, format.toAscii()))
        {
                emit uploadingError("Failed to save screenshot to buffer");
        }
    }
    //Upload to ftp
    id_connect = ftp->connectToHost(host, port);
    id_login = ftp->login(username, password);
    id_cd = ftp->cd(path); //id = 3
    id_put = ftp->put(bufferArray, remoteFileName, QFtp::Binary); //id == 4

}

void FTPUploader::ftpCommandFinished(int id, bool error)
{
    if(id == id_connect) //CONNECT
    {
        if(error)
        {
            id_close = ftp->close();
            emit uploadingError("Could not connect to " + host + " on port " + QString::number(port));
            return;
        }

    }else if(id == id_login) //LOGIN
    {
        if(error)
        {
            id_close = ftp->close();
            emit uploadingError("Failed to login");
            return;
        }

    }else if(id == id_cd) //CD
    {
        if(error)
        {
            id_close = ftp->close();
            emit uploadingError("Failed to change directory on FTP server. Are you sure that the directory " + path + " really exists?");
        }
    }else if(id == id_put) //PUT
    {
        if(!error)
        {
            buffer->reset();
            buffer->close();
            id_close = ftp->close();
            qDebug() << "Successfully uploaded";
            if(!urlToPath.isEmpty())
            {
                emit uploadingFinished(urlToPath + filename);
            }else
            {
                emit uploadingFinished("");
            }
        }else
        {
            //ERROR
            id_close = ftp->close();
            emit uploadingError("FTP upload failed");

        }

    }else if(id == id_close) //CLOSE
    {
        delete buffer;
        emit uploadingError("Failed to close FTP connection");
    }else
    {
        qDebug() << "Error on command " << id;
        emit uploadingError("Error on ftp command " + QString::number(id));
    }
}

void FTPUploader::ftpListInfo(const QUrlInfo &i)
{
}

void FTPUploader::updateDataTransferProgress(qint64 done, qint64 total)
{
}

void FTPUploader::setupSettingsUi()
{
    loadSettings();
    uiInputHost = settingsWidget->findChild<QLineEdit*>("input_host");
    uiInputPort = settingsWidget->findChild<QLineEdit*>("input_port");
    uiInputUsername = settingsWidget->findChild<QLineEdit*>("input_username");
    uiInputPassword = settingsWidget->findChild<QLineEdit*>("input_password");
    uiInputDir = settingsWidget->findChild<QLineEdit*>("input_dir");
    uiInputDir->setValidator(new QRegExpValidator(QRegExp("^[\x20-\x7F]*$"), uiInputDir)); //Only match ascii
    uiInputURL = settingsWidget->findChild<QLineEdit*>("input_url");
    uiInputPrefix = settingsWidget->findChild<QLineEdit*>("input_prefix");
    uiInputPrefix->setValidator(new QRegExpValidator(QRegExp("^[\x20-\x7F]*$"), uiInputDir)); //Only match ascii
    uiInputName = settingsWidget->findChild<QLineEdit*>("input_name");
    uiInputName->setValidator(new QRegExpValidator(QRegExp("^[\x20-\x7F]*$"), uiInputDir)); //Only match ascii
    uiInputSuffix = settingsWidget->findChild<QLineEdit*>("input_suffix");
    uiInputSuffix->setValidator(new QRegExpValidator(QRegExp("^[\x20-\x7F]*$"), uiInputDir)); //Only match ascii
    uiCheckBoxDateTimeAsPrefix =  settingsWidget->findChild<QCheckBox*>("checkBox_dateTimePrefix");
    uiCheckBoxDateTimeAsSuffix = settingsWidget->findChild<QCheckBox*>("checkBox_dateTimeSuffix");
    uiButtonSettingsDateTimePrefix = settingsWidget->findChild<QPushButton*>("button_settingsDateTimePrefix");
    uiButtonSettingsDateTimeSuffix = settingsWidget->findChild<QPushButton*>("button_settingsDateTimeSuffix");
    connect(uiButtonSettingsDateTimePrefix, SIGNAL(clicked()), this, SLOT(openDateTimeSettingsPrefix()));
    connect(uiButtonSettingsDateTimeSuffix, SIGNAL(clicked()), this, SLOT(openDateTimeSettingsSuffix()));

    uiInputHost->setText(host);
    uiInputPort->setText(QString::number(port));
    uiInputUsername->setText(username);
    uiInputPassword->setText(password);
    uiInputDir->setText(path);
    uiInputURL->setText(urlToPath);
    if(!dateTimeAsSuffix)
    {
        uiInputPrefix->setText(prefix);
    }
    uiInputName->setText(screenshotName);
    if(!dateTimeAsSuffix)
    {
        uiInputSuffix->setText(suffix);
    }
    uiCheckBoxDateTimeAsPrefix->setChecked(dateTimeAsPrefix);
    uiCheckBoxDateTimeAsSuffix->setChecked(dateTimeAsSuffix);
}

void FTPUploader::openDateTimeSettingsPrefix()
{
    DateTimeDialog prefixDateTimeSettings(settingsDialog, this->shortname, true);
    prefixDateTimeSettings.exec();
}

void FTPUploader::openDateTimeSettingsSuffix()
{
    DateTimeDialog prefixDateTimeSettings(settingsDialog, this->shortname, false);
    prefixDateTimeSettings.exec();
}
