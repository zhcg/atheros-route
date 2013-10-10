/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: testbedsta.h
//  Description: WiFi - Protected Setup Station graphical user interface header
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

#ifndef TESTBEDSTA_H
#define TESTBEDSTA_H

#include "ui_testbedsta.h"

#include <QLinkedList>

class PageTemplate;
class SetupInterface;
class SelectMode;
class NetConfig;
class SelectMethod;
class WriteNfcConfig;
class ReadNfcConfig;
class SelectAp;
class InputPin;
class DisplayPin;
class WpsAuthentication;
class WpsPbc;
class DebugWindow;


class TestbedSta: public QMainWindow, public Ui::TestbedSta
{
Q_OBJECT
public:
	TestbedSta(QWidget *parent = 0, Qt::WindowFlags f = 0);
	~TestbedSta();

public slots:
	virtual void back();
	virtual void next();
	virtual void cancel();

	virtual void close();
	virtual void about();

private slots:
	void debugging();

protected:
	void closeEvent(QCloseEvent *);

private:
	QLinkedList<PageTemplate *> listPage;
	SetupInterface *setupInterface;
	SelectMode *selectMode;
	NetConfig *netConfig;
	SelectMethod *selectMethod;
	WriteNfcConfig *writeNfcConfig;
	ReadNfcConfig *readNfcConfig;
	SelectAp* selectAp;
	InputPin *inputPin;
	DisplayPin *displayPin;
	WpsAuthentication *wpsAuthentication;
	WpsPbc *wpsPbc;
	DebugWindow *debugWindow;
};

#endif // EXTREGISTRAR_H
