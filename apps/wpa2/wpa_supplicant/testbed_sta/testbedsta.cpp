/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: testbedsta.cpp
//  Description: WiFi - Protected Setup Station user interface source
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

#include "testbedsta.h"
#include "mainprocess.h"
#include "pagetemplate.h"
#include "setupinterface.h"
#include "selectmode.h"
#include "netconfig.h"
#include "selectmethod.h"
#include "writenfcconfig.h"
#include "readnfcconfig.h"
#include "selectap.h"
#include "inputpin.h"
#include "displaypin.h"
#include "wpsauthentication.h"
#include "wpspbc.h"
#include "debugwindow.h"
#include "about.h"

#include <QMessageBox>


QProcess *MainProcess::mainProcess = new QProcess();
struct wpa_ctrl *MainProcess::monitor = 0;
struct wpa_ctrl *MainProcess::ctrl = 0;
char *MainProcess::iface = 0;
QMutex *MainProcess::mtx = new QMutex();
#if defined(CONFIG_CTRL_IFACE_UNIX) || defined(CONFIG_CTRL_IFACE_UDP)
QSocketNotifier *MainProcess::msgNotifier = 0;
#elif defined(CONFIG_CTRL_IFACE_NAMED_PIPE)
QTimer *MainProcess::msgNotifier = 0;
#endif // defined(CONFIG_CTRL_IFACE_UNIX) || defined(CONFIG_CTRL_IFACE_UDP)
MainProcess::MODE MainProcess::mode = MODE_ENR;
MainProcess::METHOD MainProcess::method = METHOD_NONE;
MainProcess::AREA MainProcess::area = AREA_BOTH_INBAND_UPNP;
int MainProcess::networkIndex = -1;
char *MainProcess::wirelessInterface = 0;
char *MainProcess::wiredInterface = 0;
char *MainProcess::controlUrl = 0;
bool MainProcess::enabledUpnp = false;


TestbedSta::TestbedSta(QWidget *parent /* = 0 */, Qt::WindowFlags f /* = 0*/)
: QMainWindow(parent, f)
{
	setupUi(this);

	connect(this, SIGNAL(destroyed(QObject *)), SLOT(close()));

	connect(actQuit, SIGNAL(activated()), SLOT(close()));
	connect(actAbout, SIGNAL(activated()), SLOT(about()));
	connect(pbBack, SIGNAL(clicked()), SLOT(back()));
	connect(pbNext, SIGNAL(clicked()), SLOT(next()));
	connect(pbCancel, SIGNAL(clicked()), SLOT(cancel()));

	setupInterface = new SetupInterface(this, frame);
	setupInterface->close();
	selectMode = new SelectMode(this, frame);
	selectMode->close();
	netConfig = new NetConfig(this, frame);
	netConfig->close();
	selectMethod = new SelectMethod(this, frame);
	selectMethod->close();
	writeNfcConfig = new WriteNfcConfig(this, frame);
	writeNfcConfig->close();
	readNfcConfig = new ReadNfcConfig(this, frame);
	readNfcConfig->close();
	selectAp = new SelectAp(this, frame);
	selectAp->close();
	inputPin = new InputPin(this, frame);
	inputPin->close();
	displayPin = new DisplayPin(this, frame);
	displayPin->close();
	wpsAuthentication = new WpsAuthentication(this, frame);
	wpsAuthentication->close();
	wpsPbc = new WpsPbc(this, frame);
	wpsPbc->close();

	listPage.push_front(reinterpret_cast<PageTemplate *>(setupInterface));
	listPage.front()->pre_next();
	listPage.front()->show();

	debugWindow = 0;
	if (MainProcess::setDebugOut(this, SLOT(debugging()))) {
		debugWindow = new DebugWindow();
		QHBoxLayout *hl = new QHBoxLayout(debugWindow);
		debugWindow->textEdit = new QTextEdit(debugWindow);
		debugWindow->textEdit->setReadOnly(true);
		hl->addWidget(debugWindow->textEdit);
		debugWindow->setLayout(hl);
		debugWindow->setGeometry(x(), y() + height() + 80,
								 debugWindow->width(),
								 debugWindow->height());
		debugWindow->show();
	}
}

TestbedSta::~TestbedSta()
{
	disconnect(actQuit);
	disconnect(pbBack);
	disconnect(pbNext);
	disconnect(pbCancel);

	delete setupInterface;
	delete selectMode;
	delete netConfig;
	delete selectMethod;
	delete writeNfcConfig;
	delete readNfcConfig;
	delete selectAp;
	delete inputPin;
	delete displayPin;
	delete wpsAuthentication;
	delete wpsPbc;
}

void TestbedSta::back()
{
	PageTemplate *p;

	do {
		if(!listPage.front()->post_back())
			break;
		p = listPage.front();
		p->close();
		listPage.pop_front();

		if (!listPage.front()->pre_back()) {
			if (!p->pre_next()) {
				QMessageBox::critical(this, windowTitle(), "Critical Error");
				break;
			}
			listPage.push_front(p);
			break;
		}
		listPage.front()->show();
	} while (0);
}

void TestbedSta::next()
{
	PageTemplate *n = 0;

	do {
		if(!listPage.front()->post_next())
			break;
		listPage.front()->close();

		if ((setupInterface == listPage.front()) ||
			(writeNfcConfig == listPage.front()) ||
			(readNfcConfig == listPage.front()) ||
			(wpsPbc == listPage.front()) ||
			(wpsAuthentication == listPage.front())) {
			n = reinterpret_cast<PageTemplate *>(selectMode);
		} if (selectMode == listPage.front()) {
			switch (MainProcess::getMode()) {
			case MainProcess::MODE_REG_CONFAP:
			case MainProcess::MODE_MANUAL:
				n = reinterpret_cast<PageTemplate *>(netConfig);
				break;
			case MainProcess::MODE_ENR:
			case MainProcess::MODE_REG_REGAP:
			case MainProcess::MODE_REG_REGSTA:
				n = reinterpret_cast<PageTemplate *>(selectMethod);
				break;
			default:
				QMessageBox::critical(this, windowTitle(), "Critical Error");
				break;
			}
		} else if (netConfig == listPage.front()) {
			switch (MainProcess::getMode()) {
			case MainProcess::MODE_REG_CONFAP:
				n = reinterpret_cast<PageTemplate *>(selectMethod);
				break;
			case MainProcess::MODE_MANUAL:
				n = reinterpret_cast<PageTemplate *>(wpsAuthentication);
				break;
			default:
				QMessageBox::critical(this, windowTitle(), "Critical Error");
				break;
			}
		} else if (selectMethod == listPage.front()) {
			switch (MainProcess::getMethod()) {
			case MainProcess::METHOD_NFC:
				switch (MainProcess::getMode()) {
				case MainProcess::MODE_REG_CONFAP:
				case MainProcess::MODE_REG_REGSTA:
					n = reinterpret_cast<PageTemplate *>(writeNfcConfig);
					break;
				case MainProcess::MODE_ENR:
				case MainProcess::MODE_REG_REGAP:
					n = reinterpret_cast<PageTemplate *>(readNfcConfig);
					break;
				default:
					QMessageBox::critical(this, windowTitle(), "Critical Error");
					break;
				}
				break;
			case MainProcess::METHOD_PIN:
				n = reinterpret_cast<PageTemplate *>(selectAp);
				break;
			case MainProcess::METHOD_PBC:
				n = reinterpret_cast<PageTemplate *>(wpsPbc);
				break;
			default:
				QMessageBox::critical(this, windowTitle(), "Critical Error");
				break;
			}
		} else if (selectAp == listPage.front()) {
			switch (MainProcess::getMode()) {
			case MainProcess::MODE_ENR:
				n = reinterpret_cast<PageTemplate *>(displayPin);
				break;
			case MainProcess::MODE_REG_CONFAP:
			case MainProcess::MODE_REG_REGAP:
			case MainProcess::MODE_REG_REGSTA:
				n = reinterpret_cast<PageTemplate *>(inputPin);
				break;
			default:
				QMessageBox::critical(this, windowTitle(), "Critical Error");
				break;
			}
		} else if (inputPin == listPage.front()) {
			if ((MainProcess::MODE_REG_REGSTA == MainProcess::getMode()) &&
				!pbBack->isEnabled())
				n = reinterpret_cast<PageTemplate *>(selectMode);
			else
				n = reinterpret_cast<PageTemplate *>(wpsAuthentication);
		} else if (displayPin == listPage.front()) {
			n = reinterpret_cast<PageTemplate *>(wpsAuthentication);
		}

		if (n) {
			if (!n->pre_next())
				break;
			listPage.push_front(n);
		} else if (!listPage.front()->pre_next()) {
			QMessageBox::critical(this, windowTitle(), "Critical Error");
			break;
		}
		listPage.front()->show();
	} while (0);
}

void TestbedSta::cancel()
{
	listPage.front()->cancel();
}

void TestbedSta::close()
{
	MainProcess::terminate();

	if (debugWindow) {
		debugWindow->close();
		delete debugWindow;
		debugWindow = 0;
	}

	QMainWindow::close();
}

void TestbedSta::closeEvent(QCloseEvent *)
{
	close();
}

void TestbedSta::about()
{
	About license(this);
	license.exec();
}

void TestbedSta::debugging()
{
	QString out;
	do {
		while (1) {
			out = MainProcess::readDebugMsg();
			if (!out.length())
				break;
			if (debugWindow) {
				out.remove(QChar('\n'));
				out.remove(QChar('\r'));
				debugWindow->textEdit->append(out);
			}
		}
	} while (0);
}

