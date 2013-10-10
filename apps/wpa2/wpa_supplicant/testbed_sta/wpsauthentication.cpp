/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: wpsauthentication.cpp
//  Description: WPS authenticaiton source
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions
//   are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in
//       the documentation and/or other materials provided with the
//       distribution.
//     * Neither the name of Sony Corporation nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
//   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**************************************************************************/

#include "pagetemplate.h"
#include "wpsauthentication.h"
#include "testbedsta.h"
#include "mainprocess.h"

#include <QMessageBox>
#include <QTimer>


WpsAuthentication::WpsAuthentication(QWidget *wizard, QWidget *parent /* = 0 */, Qt::WindowFlags f /* = 0 */)
: PageTemplate(parent, f)
{
	wiz = reinterpret_cast<TestbedSta *>(wizard);
	setupUi(this);

	timer = new QTimer;
	connect(timer, SIGNAL(timeout()), this, SLOT(increment()));
}

WpsAuthentication::~WpsAuthentication()
{
	delete timer;
}

bool WpsAuthentication::pre_next()
{
	bool ret = false;

	wiz->pbBack->setEnabled(false);
	wiz->pbNext->setEnabled(false);
	wiz->pbCancel->setEnabled(true);

	do {
		if (MainProcess::MODE_MANUAL == MainProcess::getMode()) {
			char ssid[33] = {0};

			label->setText("Complete");
			lblComment->setText("Complete set configuration manually.\n\nSSID : ");
			if (!MainProcess::getNetworkParam(MainProcess::getNetworkIndex(), "ssid", ssid, true))
				QMessageBox::warning(this, label->text(), "Could not get parameter : ssid\n");
			else
				lblComment->setText(lblComment->text() + ssid);

			wiz->pbCancel->setText("E&xit");
			label_2->setVisible(false);
			lblPIN->setVisible(false);
			progressBar->setVisible(false);
			ret = true;
			break;
		} else
			label->setText("WPS Authenticating ...");

		if (MainProcess::isEnabledUpnp())
			pin[0] = 0;
		else if (!MainProcess::getNetworkParam(MainProcess::getNetworkIndex(), "password", pin, true))
			pin[0] = 0;

		MainProcess::connectMonitor(this, SLOT(receiveMsgs()));
		if (!MainProcess::isEnabledUpnp() && !MainProcess::connectNetwork(MainProcess::getNetworkIndex())) {
			MainProcess::disconnectMonitor(this, SLOT(receiveMsgs()));
			QMessageBox::warning(this, label->text(),
								 "Could not start WPS authentication\n");
			break;
		}
		progressBar->setValue(0);
		progressBar->setVisible(true);
		timer->start(100);

		ret = true;

		lblPIN->setText(pin);
		if (lblPIN->text().length())
			label_2->setVisible(true);
		else
			label_2->setVisible(false);

		lblComment->setText("Try to authenticate in 2 minutes\n");
	} while (0);

	return ret;
}

void WpsAuthentication::cancel()
{
	if (!wiz->pbCancel->text().compare("&Cancel") &&
		timer->isActive()) {
		timer->stop();

		if (MainProcess::MODE_REG_REGSTA != MainProcess::getMode()) {
			MainProcess::removeNetwork(MainProcess::getNetworkIndex());
		} else {
			MainProcess::sendSelectedRegistrar(MainProcess::getControlUrl(), false);
			wiz->pbNext->setEnabled(true);
		}

		label->setText("Canceled by user indication");
		label_2->setVisible(false);
		lblPIN->setVisible(false);
		progressBar->setVisible(false);
		lblComment->setText("Cancel WPS authentication.\n");
	} else {
		if (MainProcess::MODE_MANUAL != MainProcess::getMode())
			MainProcess::disconnectMonitor(this, SLOT(receiveMsgs()));
		wiz->close();
	}
}

bool WpsAuthentication::post_next()
{
	MainProcess::disconnectMonitor(this, SLOT(receiveMsgs()));
	return true;
}

void WpsAuthentication::processCtrlRequest(char *buf, size_t len)
{
	int priority;
	char req[BUFSIZ];
	char msg[BUFSIZ];

	if (MainProcess::getCtrlRequelst(buf, len, &priority, req, msg)) {
		if (!strcmp(req, CTRL_REQ_EAP_WPS_FAIL)) {
			timer->stop();

			MainProcess::removeNetwork(MainProcess::getNetworkIndex());

			label->setText("Fail");
			lblComment->setText("Fail WPS authentication");
			label_2->setVisible(false);
			lblPIN->setVisible(false);
			progressBar->setVisible(false);
		} else if (!strcmp(req, CTRL_REQ_EAP_WPS_COMP)) {
			int index = atoi(msg + 1);
			char ssid[33] = {0};

			timer->stop();
			UtilSleep::mSleep(500);

			MainProcess::removeNetwork(MainProcess::getNetworkIndex());

			if (!MainProcess::getNetworkParam(index, "ssid", ssid, true)) {
				QMessageBox::warning(this, label->text(),
									 "Could not get parameter : ssid\n");
			}
			MainProcess::setNetworkIndex(index);
			MainProcess::connectNetwork(MainProcess::getNetworkIndex());

			label->setText("Complete");
			lblComment->setText("Complete WPS Authentication.\n\nSSID : ");
			lblComment->setText(lblComment->text() + ssid);
			label_2->setVisible(false);
			lblPIN->setVisible(false);
			progressBar->setVisible(false);

			wiz->pbNext->setEnabled(true);
			wiz->pbNext->setFocus();
		} else if (!strcmp(req, CTRL_REQ_UPNP_FAIL)) {
			timer->stop();
			MainProcess::sendSelectedRegistrar(MainProcess::getControlUrl(), false);

			label->setText("Fail");
			lblComment->setText("Fail WPS authentication thru UPnP");
			if (MainProcess::MODE_REG_REGSTA == MainProcess::getMode()) {
				wiz->pbNext->setEnabled(true);
				wiz->pbNext->setFocus();
			}
			label_2->setVisible(false);
			lblPIN->setVisible(false);
			progressBar->setVisible(false);
		} else if (!strcmp(req, CTRL_REQ_UPNP_COMP)) {
			int index = atoi(msg + 1);
			char ssid[33] = {0};

			timer->stop();
			MainProcess::sendSelectedRegistrar(MainProcess::getControlUrl(), false);
			MainProcess::setNetworkIndex(index);
			if (!MainProcess::getNetworkParam(MainProcess::getNetworkIndex(), "ssid", ssid, true)) {
				QMessageBox::warning(this, label->text(),
									 "Could not get parameter : ssid\n");
			}

			if (MainProcess::MODE_REG_REGSTA != MainProcess::getMode()) {
				MainProcess::connectNetwork(MainProcess::getNetworkIndex());
			}

			label->setText("Complete");
			lblComment->setText("Complete WPS Authentication thru UPnP.\n\nSSID : ");
			lblComment->setText(lblComment->text() + ssid);
			label_2->setVisible(false);
			lblPIN->setVisible(false);
			progressBar->setVisible(false);

			wiz->pbNext->setEnabled(true);
			wiz->pbNext->setFocus();
		} 
	}
}

void WpsAuthentication::receiveMsgs()
{
	char msg[BUFSIZ];
	size_t len;

	while (MainProcess::ctrlPending()) {
		len = sizeof(msg)  - 1;
		if (MainProcess::receive(msg, &len))
			processCtrlRequest(msg, len);
	}
	MainProcess::reconnectMonitor();
}

void WpsAuthentication::increment()
{
	if (progressBar->value() < progressBar->maximum())
		progressBar->setValue(progressBar->value() + 1);
	else {
		timer->stop();

		if (MainProcess::MODE_REG_REGSTA != MainProcess::getMode()) {
			MainProcess::removeNetwork(MainProcess::getNetworkIndex());
		}

		label_2->setVisible(false);
		lblPIN->setVisible(false);
		label->setText("Timeout");
		progressBar->setFormat("Timeout");
		progressBar->update();
		lblComment->setText("Timeout WPS Authentication.\n"
							"Could not authenticate in 2 minutes\n");
		if (MainProcess::isEnabledUpnp()) {
			MainProcess::sendSelectedRegistrar(MainProcess::getControlUrl(), false);
			wiz->pbNext->setEnabled(true);
		}
	}
}

