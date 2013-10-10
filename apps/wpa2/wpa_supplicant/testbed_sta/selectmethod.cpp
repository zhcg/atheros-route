/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: selectmethod.cpp
//  Description: Select method source
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
#include "selectmethod.h"
#include "mainprocess.h"
#include "testbedsta.h"

#include <QMessageBox>


SelectMethod::SelectMethod(QWidget *wizard, QWidget *parent /* = 0 */, Qt::WindowFlags f /* = 0 */)
: PageTemplate(parent, f)
{
	wiz = reinterpret_cast<TestbedSta *>(wizard);
	setupUi(this);

	connect(pbNfc, SIGNAL(clicked()), SLOT(selNfc())); 
	connect(pbPin, SIGNAL(clicked()), SLOT(selPin())); 
	connect(pbPbc, SIGNAL(clicked()), SLOT(selPbc())); 
}

SelectMethod::~SelectMethod()
{
}

bool SelectMethod::pre_back()
{
	MainProcess::setMethod(MainProcess::METHOD_NONE);

	switch (MainProcess::getMode()) {
	case MainProcess::MODE_ENR:
		lblNfc->setText("Get configure with NFC Config Token");
		lblPin->setText("Get configure with PIN method");
		lblPbc->setText("Get configure with Push-Button method");
		pbPbc->setVisible(true);
		lblPbc->setVisible(true);
		break;
	case MainProcess::MODE_REG_CONFAP:
		lblNfc->setText("Configure Access Point with NFC Config Token");
		lblPin->setText("Configure Access Point with PIN method");
		lblPbc->setText("Configure Access Point with Push-Button method");
		pbPbc->setVisible(true);
		lblPbc->setVisible(true);
		break;
	case MainProcess::MODE_REG_REGAP:
		lblNfc->setText("Register Access Point with NFC Config Token");
		lblPin->setText("Register Access Point with PIN method");
		lblPbc->setText("Register Access Point with Push-Button method");
		pbPbc->setVisible(true);
		lblPbc->setVisible(true);
		break;
	case MainProcess::MODE_REG_REGSTA:
		lblNfc->setText("Register Another Station with NFC Config Token");
		lblPin->setText("Register Another Station Point with PIN method");
		pbPbc->setVisible(false);
		lblPbc->setVisible(false);
		break;
	default:
		break;
	}

	pbNfc->setFocus();

	wiz->pbBack->setEnabled(true);
	wiz->pbNext->setEnabled(false);
	wiz->pbCancel->setEnabled(true);
	return true;
}

bool SelectMethod::pre_next()
{
	MainProcess::setMethod(MainProcess::METHOD_NONE);

	switch (MainProcess::getMode()) {
	case MainProcess::MODE_REG_CONFAP:
		lblNfc->setText("Configure Access Point with NFC Config Token");
		lblPin->setText("Configure Access Point with PIN method");
		lblPbc->setText("Configure Access Point with Push-Button method");
		pbPbc->setVisible(true);
		lblPbc->setVisible(true);
		break;
	case MainProcess::MODE_REG_REGAP:
		lblNfc->setText("Register Access Point with NFC Config Token");
		lblPin->setText("Register Access Point with PIN method");
		lblPbc->setText("Register Access Point with Push-Button method");
		pbPbc->setVisible(true);
		lblPbc->setVisible(true);
		break;
	case MainProcess::MODE_REG_REGSTA:
		lblNfc->setText("Register Another Station with NFC Config Token");
		lblPin->setText("Register Another Station Point with PIN method");
		pbPbc->setVisible(false);
		lblPbc->setVisible(false);
		break;
	default:
		break;
	}

	pbNfc->setFocus();

	wiz->pbBack->setEnabled(true);
	wiz->pbNext->setEnabled(false);
	wiz->pbCancel->setEnabled(true);
	return true;
}

void SelectMethod::cancel()
{
	wiz->close();
}

void SelectMethod::selNfc()
{
	MainProcess::setMethod(MainProcess::METHOD_NFC);
	wiz->next();
}

void SelectMethod::selPin()
{
	MainProcess::setMethod(MainProcess::METHOD_PIN);
	wiz->next();
}

void SelectMethod::selPbc()
{
	MainProcess::setMethod(MainProcess::METHOD_PBC);
	wiz->next();
}

