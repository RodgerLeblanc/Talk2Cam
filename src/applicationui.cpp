/*
 * Copyright (c) 2011-2013 BlackBerry Limited.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "applicationui.hpp"
#include "Talk2WatchInterface.h"
#include "UdpModule.h"

#include <bb/cascades/Application>
#include <bb/cascades/QmlDocument>
#include <bb/cascades/AbstractPane>
#include <bb/cascades/LocaleHandler>

#include <bb/ApplicationInfo>
#include <bb/cascades/multimedia/Camera>

using namespace bb::cascades;

ApplicationUI::ApplicationUI(bb::cascades::Application *app) :
        QObject(app)
{
    // prepare the localization
    m_pTranslator = new QTranslator(this);
    m_pLocaleHandler = new LocaleHandler(this);

    bool res = QObject::connect(m_pLocaleHandler, SIGNAL(systemLanguageChanged()), this, SLOT(onSystemLanguageChanged()));
    // This is only available in Debug builds
    Q_ASSERT(res);
    // Since the variable is not used in the app, this is added to avoid a
    // compiler warning
    Q_UNUSED(res);

    // initial load
    onSystemLanguageChanged();

    // Create scene document from main.qml asset, the parent is set
    // to ensure the document gets destroyed properly at shut down.
    QmlDocument *qml = QmlDocument::create("asset:///main.qml").parent(this);

    // Create root object for the UI
    AbstractPane *root = qml->createRootObject<AbstractPane>();

    // Set created root object as the application scene
    app->setScene(root);

    m_label = root->findChild<Label*>("m_label");
    m_camera = root->findChild<bb::cascades::multimedia::Camera*>("m_camera");

    bb::ApplicationInfo appInfo;
    QString version = appInfo.version() ;

	settings.setValue("appName", "Talk2Cam");
	settings.setValue("version", version);
	settings.setValue("appKey", "614bfd49-5f54-4c43-8df0-4ec1ef94cea1"); // generated randomly online

    t2w = new Talk2WatchInterface(this);
    connect(t2w, SIGNAL(transmissionReady()), this, SLOT(onTransmissionReady()));

    udp = new UdpModule;
    udp->listenOnPort(9113);
    connect(udp, SIGNAL(reveivedData(QString)), this, SLOT(onUdpDataReceived(QString)));
}

void ApplicationUI::onSystemLanguageChanged()
{
    QCoreApplication::instance()->removeTranslator(m_pTranslator);
    // Initiate, load and install the application translation files.
    QString locale_string = QLocale().name();
    QString file_name = QString("Talk2Cam_%1").arg(locale_string);
    if (m_pTranslator->load(file_name, "app/native/qm")) {
        QCoreApplication::instance()->installTranslator(m_pTranslator);
    }
}

void ApplicationUI::onTransmissionReady()
{
	authorizeAppWithT2w();
}

void ApplicationUI::authorizeAppWithT2w()
{
	if (t2w->isTalk2WatchProInstalled() || t2w->isTalk2WatchProServiceInstalled()) {
		QString t2wAuthUdpPort = "9113"; // Random port, use the same value as the port in udp->listenOnPort(xxxx);
		QString description = "This app is a helper app for Tony, use all the code you want freely!";
		t2w->setAppValues(settings.value("appName").toString(), settings.value("version").toString(), settings.value("appKey").toString(), "UDP", t2wAuthUdpPort, description);
		t2w->sendAppAuthorizationRequest();
	}
}

void ApplicationUI::onUdpDataReceived(QString _data)
{
	qDebug() << "onUdpDataReceived in..." << _data;
	if(_data=="AUTH_SUCCESS") {
		qDebug() << "Auth_Success!!!";
		// Create action as soon as this app has been aknowledge by T2W. You can call this
		// at every startup (like this app does), but it's not ideal. Change your code accordingly.
		t2wActionTitle << "Smile!";  // << "And so on..."
		t2wActionCommand << "TALK2WATCH_TAKE_PICTURE";
		t2wActionDescription << "Take a picture";
		if (t2wActionTitle.size() > 0)
			t2w->createAction(t2wActionTitle[0], t2wActionCommand[0], t2wActionDescription[0]);
		return;
	}
	if (_data=="CREATE_ACTION_SUCCESS") {
		qDebug() << "Create_Action_success";
		// Only create next action after the first one has been acked
		if (t2wActionTitle.size() < 1)
			return;
		t2wActionTitle.removeFirst();
		t2wActionCommand.removeFirst();
		t2wActionDescription.removeFirst();
		if (t2wActionTitle.size() > 0)
			t2w->createAction(t2wActionTitle[0], t2wActionCommand[0], t2wActionDescription[0]);
	}
	if (_data=="TALK2WATCH_TAKE_PICTURE") {
		if (m_camera->isVisible()) {
			m_camera->capturePhoto();
//			m_label->setText("You look nice!");
			t2w->sendSms("Picture saved", "You look nice!");
		}
		else {
			m_camera->stopViewfinder();
			m_camera->open();
			m_camera->startViewfinder();
			m_camera->setVisible(true);
//			m_label->setText("Starting the camera... Please restart script");
			t2w->sendSms("Starting the camera...", "Please fire the script again to take the picture");
		}
	}
}
