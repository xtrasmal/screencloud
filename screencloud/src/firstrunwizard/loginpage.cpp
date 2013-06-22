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

#include "loginpage.h"
#include <QDebug>

LoginPage::LoginPage(QWidget *parent) :
    QWizardPage(parent)
{
    serverQueryFinished = false;
    serverQueryError = false;
    manager = new QNetworkAccessManager(this);
    connect(manager, SIGNAL(finished(QNetworkReply*)),
             this, SLOT(replyFinished(QNetworkReply*)));
    //Setup GUI
    setTitle(tr("Login"));
    input_email = new QLineEdit(this);
    input_email->setValidator(new QRegExpValidator(QRegExp(".*@.*"), this));
    input_password = new QLineEdit(this);
    input_password->setEchoMode(QLineEdit::Password);
    label_forgotPassword = new QLabel("<a href=\"http://screencloud.net/users/forgot_password\">Forgot password?</a>", this);
    label_forgotPassword->setOpenExternalLinks(true);
    label_message = new QLabel(this);
    label_message->setWordWrap(true);

    registerField("login.email*", input_email);
    registerField("login.password*", input_password);


    QFormLayout* loginForm = new QFormLayout();
    loginForm->addRow("Email:", input_email);
    loginForm->addRow("Password:", input_password);
    loginForm->addWidget(label_forgotPassword);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addLayout(loginForm);
    layout->addStretch();
    layout->addWidget(label_message);

    setLayout(layout);
}

int LoginPage::nextId() const
{
    return FirstRunWizard::Page_Tutorial1;
}

bool LoginPage::validatePage()
{
    serverQueryFinished = false;
    serverQueryError = false;
    label_message->setText("Connecting to server...");
    QByteArray token, tokenSecret;
    QOAuth::Interface *qoauth = new QOAuth::Interface;
    qoauth->setConsumerKey(CONSUMER_KEY_SCREENCLOUD);
    qoauth->setConsumerSecret(CONSUMER_SECRET_SCREENCLOUD);
    QByteArray url( "https://screencloud.net/1.0/oauth/access_token_xauth" );
    QString urlString = QString(url);
    // create a request parameters map
    QOAuth::ParamMap map;
    map.insert( "data[User][email]", QUrl::toPercentEncoding(input_email->text()));
    map.insert( "data[User][password]", QUrl::toPercentEncoding(input_password->text()) );

    QOAuth::ParamMap reply =
    qoauth->accessToken( urlString , QOAuth::POST, token,
                         tokenSecret, QOAuth::HMAC_SHA1, map );

    QNetworkRequest request;
    request.setUrl(QUrl(url));
    //manager->get(request);
    if ( qoauth->error() == QOAuth::NoError ) {
        //Save to qsettings
        QSettings settings("screencloud", "ScreenCloud");
        settings.beginGroup("account");
        settings.setValue("token", reply.value( QOAuth::tokenParameterName() ));
        settings.setValue("token_secret", reply.value( QOAuth::tokenSecretParameterName() ));
        settings.setValue("email", input_email->text());
        settings.setValue("logged_in", true);
        settings.endGroup();
        return true;
    }else
    {
        label_message->setText("<font color='red'>Login failed. Please check your email, password and internet connection.</font>");
        return false;
    }

}

void LoginPage::replyFinished(QNetworkReply *reply)
{
    label_message->setText("Reading response from server...");
    qDebug() << reply->readAll();
    if(reply->error() != QNetworkReply::NoError)
    {
        serverQueryError = true;
        if(reply->error() == QNetworkReply::ContentAccessDenied || reply->error() == QNetworkReply::AuthenticationRequiredError)
        {
            label_message->setText("<font color='red'>Your email/password combination was incorrect or account is not activated</font>");
        }else
        {
            label_message->setText("<font color='red'>Server returned an unknown error. Please try again later. (Error code: " + QString::number(reply->error()) + ")</font>");
        }

    }else
    {
        //No error in request
        label_message->setText("Logged in...");
    }
    serverQueryFinished = true;


}
