/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: wpspbc.cpp
//  Description: WPS PBC source
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
#include "wpspbc.h"
#include "testbedsta.h"
#include "mainprocess.h"

#include <QMessageBox>
#include <QTimer>


WpsPbc::WpsPbc(QWidget *wizard, QWidget *parent /* = 0 */, Qt::WindowFlags f /* = 0 */)
: PageTemplate(parent, f)
{
	wiz = reinterpret_cast<TestbedSta *>(wizard);
	setupUi(this);

	timer = new QTimer;
	connect(timer, SIGNAL(timeout()), this, SLOT(increment()));
}

WpsPbc::~WpsPbc()
{
	delete timer;
}

bool WpsPbc::pre_next()
{
	bool ret = false;

	label->setText("PBC method");

	wiz->pbNext->setText("&Start");
	wiz->pbBack->setEnabled(false);
	wiz->pbNext->setEnabled(true);
	wiz->pbCancel->setEnabled(true);
	wiz->pbNext->setFocus();

	do {
		progressBar->setValue(0);
		progressBar->setVisible(false);

		MainProcess::connectMonitor(this, SLOT(receiveMsgs()));

		lblComment->setText("Push-button on registrar, the push [Start] button.\n");

		ret = true;
	} while (0);

	return ret;
}

void WpsPbc::cancel()
{
	if (timer->isActive()) {
		timer->stop();
		MainProcess::stopPbc();
		wiz->pbCancel->setText("&Finish");
		wiz->pbNext->setText("&Next");
		wiz->pbNext->setEnabled(false);
		MainProcess::disconnectMonitor(this, SLOT(receiveMsgs()));

		label->setText("Canceled by user indication");
		progressBar->setVisible(false);
		lblComment->setText("Cancel WPS authentication.\n");
	} else {
		wiz->close();
	}
}

bool WpsPbc::post_next()
{
	bool ret = false;
	if (!wiz->pbNext->text().compare("&Next")) {
		MainProcess::disconnectMonitor(this, SLOT(receiveMsgs()));
		ret = true;
	} else {
		do {
			if (!MainProcess::startPbc()) {
				MainProcess::disconnectMonitor(this, SLOT(receiveMsgs()));
				QMessageBox::warning(this, label->text(),
									 "Could not start WPS authentication with PBC method\n");
				wiz->pbNext->setText("&Start");
				break;
			} else {
				progressBar->setValue(0);
				progressBar->setVisible(true);
				timer->start(100);
				wiz->pbNext->setText("&Restart");
			}
			lblComment->setText("Try to authenticate with PBC method in 2 minutes\n");
		} while (0);
	}

	return ret;
}

void WpsPbc::processCtrlRequest(char *buf, size_t len)
{
	int priority;
	char req[BUFSIZ];
	char msg[BUFSIZ];

	if (MainProcess::getCtrlRequelst(buf, len, &priority, req, msg)) {
		if (!strcmp(req, CTRL_REQ_EAP_WPS_FAIL)) {
			timer->stop();

			MainProcess::stopPbc();

			label->setText("Fail");
			lblComment->setText("Fail WPS authentication with PBC method");
			wiz->pbBack->setEnabled(false);
			wiz->pbNext->setText("&Next");
			wiz->pbNext->setEnabled(false);
			progressBar->setVisible(false);
		} else if (!strcmp(req, CTRL_REQ_EAP_WPS_COMP)) {
			int index = atoi(msg + 1);
			char ssid[33];

			timer->stop();
			UtilSleep::mSleep(500);

			MainProcess::setNetworkIndex(index);
			if (!MainProcess::getNetworkParam(MainProcess::getNetworkIndex(), "ssid", ssid, true)) {
				QMessageBox::warning(this, label->text(),
									 "Could not get parameter : ssid\n");
			}
			MainProcess::connectNetwork(MainProcess::getNetworkIndex());

			label->setText("Complete");
			lblComment->setText("Complete WPS Authentication with PBC method.\n\nSSID : ");
			lblComment->setText(lblComment->text() + ssid);
			wiz->pbBack->setEnabled(false);
			wiz->pbNext->setText("&Next");
			wiz->pbNext->setEnabled(true);
			wiz->pbNext->setFocus();
			progressBar->setVisible(false);
		} else if (!strlen(req) && strstr(msg, "WPS-PBC") &&
				   timer->isActive()) {
			lblComment->setText("Try to authenticate with PBC method in 2 minutes\n\n");
			lblComment->setText(lblComment->text() + msg);
		}
	}
}

void WpsPbc::receiveMsgs()
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

void WpsPbc::increment()
{
	if (progressBar->value() < progressBar->maximum())
		progressBar->setValue(progressBar->value() + 1);
	else {
		timer->stop();

		MainProcess::stopPbc();

		label->setText("Timeout");
		progressBar->setFormat("Timeout");
		progressBar->update();
		lblComment->setText("Timeout WPS Authentication with PBC method.\n"
							"Could not authenticate in 2 minutes\n");
		wiz->pbCancel->setText("&Cancel");
		wiz->pbNext->setText("&Next");
		wiz->pbNext->setEnabled(false);
	}
}

