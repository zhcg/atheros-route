/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: mainprocess.cpp
//  Description: main process control source
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

#include "mainprocess.h"
#include "wpa_ctrl.h"

#include <time.h>

#define WAIT_FOR_PROCESS		3000 // [msec]

#ifndef CONFIG_NATIVE_WINDOWS
#define WPA_SUPPLICANT			"./wpa_supplicant"

#define WPA_SUPPLICANT_CTRL_DIR	"/var/run/wpa_supplicant"
#define WPA_SUPPLICANT_CONF		"./wpa_supplicant.conf"
#else // CONFIG_NATIVE_WINDOWS
#define WPA_SUPPLICANT			".\\wpa_supplicant"

#define WPA_SUPPLICANT_CTRL_DIR	"\\var\\run\\wpa_supplicant"
#define WPA_SUPPLICANT_CONF		".\\wpa_supplicant.conf"
#endif // CONFIG_NATIVE_WINDOWS


MainProcess::MainProcess()
{
}

MainProcess::~MainProcess()
{
	terminate();

	if (mtx) {
		mtx->unlock();
		delete mtx;
		mtx = 0;
	}

	if (mainProcess) {
		delete mainProcess;
		mainProcess = 0;
	}

	if (iface) {
		free(iface);
	}

	if (wirelessInterface) {
		free(wirelessInterface);
		wirelessInterface = 0;
	}

	if (wiredInterface) {
		free(wiredInterface);
		wiredInterface = 0;
	}
}

bool MainProcess::setDebugOut(QObject *receiver, const char *method)
{
	bool ret = false;

	do {
		if (!mainProcess)
			break;

		mainProcess->setReadChannel(QProcess::StandardOutput);
		mainProcess->setProcessChannelMode(QProcess::MergedChannels);
		QObject::connect(mainProcess, SIGNAL(readyReadStandardOutput()),
						 receiver, method);

		ret = true;
	} while (0);

	return ret;
}

QString MainProcess::readDebugMsg()
{
	QString out = "";
	do {
		if (!mainProcess)
			break;
		out = mainProcess->readLine();
	} while (0);

	return out;
}

bool MainProcess::start(const char *wi, const char *driver, const char *nfc)
{
	bool ret = false;
	char cmd[BUFSIZ];

	do {
		terminate();
		if (!mainProcess)
			break;

		os_snprintf(cmd, sizeof(cmd), "%s -C%s -c%s -i%s -D%s %s%s -ddd",
					WPA_SUPPLICANT, WPA_SUPPLICANT_CTRL_DIR, WPA_SUPPLICANT_CONF,
					wi, driver, nfc?"-n":"", nfc?nfc:"");
		mainProcess->start(cmd);
		if (!mainProcess->waitForStarted(WAIT_FOR_PROCESS))
			break;

		if (iface)
			free(iface);
		iface = strdup(wi);

		srand(time(0));

		ret = true;
	} while (0);

	return ret;
}

void MainProcess::terminate(bool enforced /* = false */)
{
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;
#ifndef CONFIG_NATIVE_WINDOWS
	QProcess *rem = new QProcess();
	char remCmd[BUFSIZ];
#endif // CONFIG_NATIVE_WINDOWS

	if (mainProcess &&
		(QProcess::NotRunning != mainProcess->state())) {

		(void)ctrlRequest("TERMINATE", res, &len);
		if (!mainProcess->waitForFinished(WAIT_FOR_PROCESS)) {
			mainProcess->kill();
		}

		disconnectMonitor(0, 0);

		if (monitor) {
			wpa_ctrl_detach(monitor);
			wpa_ctrl_close(monitor);
			monitor = 0;
		}

		if (ctrl) {
			wpa_ctrl_close(ctrl);
			ctrl = 0;
		}

#ifndef CONFIG_NATIVE_WINDOWS
		os_snprintf(remCmd, sizeof(remCmd), "rem -rf %s",
					WPA_SUPPLICANT_CTRL_DIR);
		rem->start(remCmd);
		rem->waitForFinished(-1);
		delete rem;
		rem = 0;
#endif // CONFIG_NATIVE_WINDOWS
	} else if (enforced) {
		(void)ctrlRequest("TERMINATE", res, &len);

		disconnectMonitor(0, 0);

		if (monitor) {
			wpa_ctrl_detach(monitor);
			wpa_ctrl_close(monitor);
			monitor = 0;
		}

		if (ctrl) {
			wpa_ctrl_close(ctrl);
			ctrl = 0;
		}
	}

	if (iface) {
		free(iface);
		iface = 0;
	}
}

bool MainProcess::connectMonitor(QObject *receiver, const char *method)
{ bool ret = false;
	char monitor_iface[BUFSIZ];

	do {
		if (!receiver || !method)
			break;

		if (msgNotifier) {
			delete msgNotifier;
			msgNotifier = 0;
		}

		if (!monitor) {
#if defined(CONFIG_CTRL_IFACE_UNIX) || defined(CONFIG_CTRL_IFACE_UDP)
			snprintf(monitor_iface, sizeof(monitor_iface), "%s/%s",
					 WPA_SUPPLICANT_CTRL_DIR, iface);
#elif defined(CONFIG_CTRL_IFACE_NAMED_PIPE)
			int res;
			size_t monitor_iface_len = sizeof(monitor_iface);
			char *pos;

			monitor = wpa_ctrl_open(0);
			if (!monitor)
				break;

			res = wpa_ctrl_request(monitor, "INTERFACES", 10, monitor_iface, &monitor_iface_len, 0);
			wpa_ctrl_close(monitor);
			if (0 > res) {
				monitor = 0;
				break;
			}

			monitor_iface[monitor_iface_len] = 0;
			if (0 != (pos = strchr(monitor_iface, '\n')))
				*pos = 0;
			if (0 != (pos = strchr(monitor_iface, '\r')))
				*pos = 0;
#endif // defined(CONFIG_CTRL_IFACE_UNIX) || defined(CONFIG_CTRL_IFACE_UDP)

			monitor = wpa_ctrl_open(monitor_iface);
			if (!monitor)
				break;
			if (wpa_ctrl_attach(monitor))
				break;
		}

#if defined(CONFIG_CTRL_IFACE_UNIX) || defined(CONFIG_CTRL_IFACE_UDP)
		msgNotifier = new QSocketNotifier(wpa_ctrl_get_fd(monitor),
										  QSocketNotifier::Read, 0);
		if (!msgNotifier)
			break;

		ret = QObject::connect(msgNotifier, SIGNAL(activated(int)), receiver, method);
#elif defined(CONFIG_CTRL_IFACE_NAMED_PIPE)
		msgNotifier = new QTimer();
		if (!msgNotifier)
			break;

		ret = QObject::connect(msgNotifier, SIGNAL(timeout()), receiver, method);		
		if (ret) {
			msgNotifier->setSingleShot(true);
			msgNotifier->start(500);
		}
#endif // defined(CONFIG_CTRL_IFACE_UNIX) || defined(CONFIG_CTRL_IFACE_UDP)
	} while (0);

	if (!ret) {
		printf("Could not connect with monitor.\n");
		disconnectMonitor(receiver, method);
	}

	return ret;
}

bool MainProcess::reconnectMonitor()
{
#ifdef CONFIG_NATIVE_WINDOWS
	bool ret = false;

	do {
		if (!msgNotifier)
			break;

		msgNotifier->start(100);
		ret = true;
	} while (0);

	return ret;
#else
	return true;
#endif // CONFIG_NATIVE_WINDOWS
}

bool MainProcess::disconnectMonitor(QObject *receiver, const char *method)
{
	if (msgNotifier && receiver && method) {
#if defined(CONFIG_CTRL_IFACE_UNIX) || defined(CONFIG_CTRL_IFACE_UDP)
		QObject::disconnect(msgNotifier, SIGNAL(activated(int)), receiver, method);
#elif defined(CONFIG_CTRL_IFACE_NAMED_PIPE)
		msgNotifier->stop();
		QObject::disconnect(msgNotifier, SIGNAL(timeout()), receiver, method);
#endif // defined(CONFIG_CTRL_IFACE_UNIX) || defined(CONFIG_CTRL_IFACE_UDP)
	}

	if (msgNotifier) {
		delete msgNotifier;
		msgNotifier = 0;
	}

#if defined(CONFIG_CTRL_IFACE_UNIX) || defined(CONFIG_CTRL_IFACE_UDP)
	if (monitor) {
		wpa_ctrl_detach(monitor);
		wpa_ctrl_close(monitor);
		monitor = 0;
	}
#endif // defined(CONFIG_CTRL_IFACE_UNIX) || defined(CONFIG_CTRL_IFACE_UDP)

	return true;
}

bool MainProcess::ctrlPending()
{
	return (monitor && (0 < wpa_ctrl_pending(monitor)));
}

bool MainProcess::receive(char *msg, size_t *len)
{
	int ret = false;
	do {
		if (!monitor || !msg || !len)
			break;

		ret = (0 == wpa_ctrl_recv(monitor, msg, len));
		if (ret)
			msg[*len] = 0;
	} while (0);

	return ret;
}

bool MainProcess::getCtrlRequelst(char *buf, size_t len, int *priority, char *req, char *msg)
{
	bool ret = false;
	char *pos = buf, *pos2;
	size_t req_len;

	do {
		if (!buf || !len)
			break;

		if (req) *req = 0;
		if (msg) *msg = 0;

		if (*pos == '<') {
			pos++;
			if (priority)
				*priority = atoi(pos);
			pos = strchr(pos, '>');
			if (pos)
				pos++;
			else
				pos = buf;
		}

		if (strncmp(pos, "CTRL-", 5) == 0) {
			pos2 = strchr(pos, !strncmp(pos, WPA_CTRL_REQ, strlen(WPA_CTRL_REQ))?':':' ');
			if (pos2) {
				pos2++;
				if (req) {
					req_len = pos2 - (pos + strlen(WPA_CTRL_REQ) + 1);
					strncpy(req, pos + strlen(WPA_CTRL_REQ), req_len);
					req[req_len] = 0;
				}
				if (msg)
					strcpy(msg, pos2);
			} else {
				pos2 = pos;
				if (req)
					*req = 0;
				if (msg)
					strcpy(msg, pos2);
			}
		} else
			strcpy(msg, pos);

		ret = true;
	} while (0);

	return ret;
}

int MainProcess::ctrlRequest(char *cmd, char *res, size_t *len)
{
	int ret = 0;
	char ctrl_iface[BUFSIZ];

	do {
		if (!ctrl) {
#if defined(CONFIG_CTRL_IFACE_UNIX) || defined(CONFIG_CTRL_IFACE_UDP)
			os_snprintf(ctrl_iface, sizeof(ctrl_iface), "%s/%s",
						WPA_SUPPLICANT_CTRL_DIR, iface);

#elif defined(CONFIG_CTRL_IFACE_NAMED_PIPE)
			size_t ctrl_iface_len = sizeof(ctrl_iface);
			char *pos;

			ctrl = wpa_ctrl_open(0);
			if (!ctrl) {
				ret = -3;
				break;
			}

			ret = wpa_ctrl_request(ctrl, "INTERFACES", 10, ctrl_iface, &ctrl_iface_len, 0);
			wpa_ctrl_close(ctrl);
			if (0 > ret) {
				ctrl = 0;
				break;
			}

			ctrl_iface[ctrl_iface_len] = 0;
			if (0 != (pos = strchr(ctrl_iface, '\n')))
				*pos = 0;
			if (0 != (pos = strchr(ctrl_iface, '\r')))
				*pos = 0;
#endif // defined(CONFIG_CTRL_IFACE_UNIX) || defined(CONFIG_CTRL_IFACE_UDP)
			ctrl = wpa_ctrl_open(ctrl_iface);
			if (!ctrl) {
				ret = -3;
				break;
			}
		}

		if (mtx)
			mtx->lock();

		#if 1	/* added by Atheros */
		fprintf(stderr, "SENDING COMMAND: %s\n", cmd);
		#endif
		ret = wpa_ctrl_request(ctrl, cmd, strlen(cmd), res, len, 0);
		if (0 > ret) {
			wpa_ctrl_close(ctrl);
			ctrl = 0;
		}

		if (mtx)
			mtx->unlock();
	} while (0);

	if (0 > ret)
		printf("Fail control-request : %d.\n", ret);

	return ret;
}

bool MainProcess::scanRequest()
{
	bool ret = false;
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		if (0 > ctrlRequest("SCAN", res, &len))
			break;

		if (strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail scan-request.\n");

	return ret;
}

bool MainProcess::getScanResults(char *result, size_t *len)
{
	bool ret = false;

	do {
		if (!result && !len)
			break;

		if (0 > ctrlRequest("SCAN_RESULTS", result, len))
			break;
		result[*len] = 0;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to get scan-results.\n");

	return ret;
}

int MainProcess::addNetwork()
{
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;
	int index = -1;

	do {
		if (0 > ctrlRequest("ADD_NETWORK", res, &len))
			break;

		if (!strncmp(res, "FAIL", 4))
			break;

		index = atoi(res);
	} while (0);

	if (0 > index)
		printf("Fail to add network.\n");

	return index;
}

bool MainProcess::removeNetwork(int index)
{
	bool ret = false;
	char cmd[BUFSIZ];
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		if (0 > index)
			break;

		os_snprintf(cmd, sizeof(cmd), "REMOVE_NETWORK %d", index);
		if (0 > ctrlRequest(cmd, res, &len))
			break;

		if (strncmp(res, "OK", 2))
			break;

		if (index == networkIndex)
			networkIndex = -1;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to remove network.\n");

	return ret;
}

bool MainProcess::setNetworkParam(int index, char *field, char *value, bool quote)
{
	bool ret = false;
	char cmd[BUFSIZ];
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		if ((0 > index) || !field || !value)
			break;

		os_snprintf(cmd, sizeof(cmd), "SET_NETWORK %d %s %s%s%s",
					index, field, quote ? "\"" : "", value, quote ? "\"" : "");
		if (0 > ctrlRequest(cmd, res, &len))
			break;

		if (strncmp(res, "OK", 2))
			break;

		ret = true;
	} while(0);

	if (!ret)
		printf("Fail to set network parameter : %s.\n", field);

	return ret;
}

bool MainProcess::getNetworkParam(int index, char *field, char *value, bool quote)
{
	bool ret = false;
	char cmd[BUFSIZ];
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		if ((0 > index) || !field || !value)
			break;

		os_snprintf(cmd, sizeof(cmd), "GET_NETWORK %d %s", index, field);
		if (0 > ctrlRequest(cmd, res, &len))
			break;
		res[len] = 0;

		if (!strncmp(res, "FAIL", 4))
			break;

		if (quote) {
			if (('"' != res[0]) || !strchr(&res[1], '"'))
				break;
		}

		len = strchr(&res[1], '"') - &res[1];
		strncpy(value, &res[1], len);
		value[len] = 0;

		ret = true;
	} while(0);

	if (!ret)
		printf("Fail to get network parameter : %s.\n", field);

	return ret;
}

bool MainProcess::connectNetwork(int index)
{
	bool ret = false;
	char cmd[BUFSIZ];
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		os_snprintf(cmd, sizeof(cmd), "SELECT_NETWORK %d", index);
		if (0 > ctrlRequest(cmd, res, &len))
			break;

		if (strncmp(res, "OK", 2))
			break;

		len = sizeof(res) - 1;
		os_snprintf(cmd, sizeof(cmd), "ENABLE_NETWORK %d", index);
		if (0 > ctrlRequest(cmd, res, &len))
			break;

		if (strncmp(res, "OK", 2))
			break;

		len = sizeof(res) - 1;
		if (0 > ctrlRequest("REASSOCIATE", res, &len))
			break;

		if (strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to connect network.\n");

	return ret;
}

bool MainProcess::disableNetwork(int index)
{
	bool ret = false;
	char cmd[BUFSIZ];
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		os_snprintf(cmd, sizeof(cmd), "DISABLE_NETWORK %d", index);
		if (0 > ctrlRequest(cmd, res, &len))
			break;

		if (strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to disable network.\n");

	return ret;
}

bool MainProcess::setRegMode(int regmode)
{
	bool ret = false;
	char cmd[BUFSIZ];
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		os_snprintf(cmd, sizeof(cmd), "WPS_SET_REGMODE %d", regmode);
		if (0 > ctrlRequest(cmd, res, &len))
			break;

		if (strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to scan NFC token request.\n");

	return ret;
}

bool MainProcess::setWpsPassword(const char *pwd)
{
	bool ret = false;
	char cmd[BUFSIZ];
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		if (!pwd)
			break;

		os_snprintf(cmd, sizeof(cmd), "WPS_SET_PASSWORD %s", pwd);
		if (0 > ctrlRequest(cmd, res, &len))
			break;

		if (strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to set WPS password.\n");

	return ret;
}

bool MainProcess::setWpsConfiguration(int index)
{
	bool ret = false;
	char cmd[BUFSIZ];
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		if (0 > index)
			break;

		os_snprintf(cmd, sizeof(cmd), "WPS_SET_CONFIGURATION %d", index);
		if (0 > ctrlRequest(cmd, res, &len))
			break;

		if (strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to scan NFC token request.\n");

	return ret;
}

bool MainProcess::clearWpsPassword()
{
	bool ret = false;
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		if (0 > ctrlRequest("WPS_CLEAR_PASSWORD", res, &len))
			break;

		if (strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to clear WPS password.\n");

	return ret;
}

bool MainProcess::setUpnpInterface(const char *ifname)
{
	bool ret = false;
	char cmd[BUFSIZ];
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		if (!ifname)
			break;

		os_snprintf(cmd, sizeof(cmd), "UPNP_SET_IF %s", ifname);
		if (0 > ctrlRequest(cmd, res, &len))
			break;

		if (strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to set UPnP interface.\n");

	return ret;
}

bool MainProcess::isEnabledUpnp()
{
	return enabledUpnp;
}

bool MainProcess::setEnabledUpnp(bool enabled)
{
	bool ret = false;
	char cmd[BUFSIZ];
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		os_snprintf(cmd, sizeof(cmd), "UPNP_ENABLED %d", enabled?1:0);
		if (0 > ctrlRequest(cmd, res, &len))
			break;

		if (strncmp(res, "OK", 2))
			break;

		enabledUpnp = enabled;
		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to be %s UPnP interface.\n", enabled?"enabled":"disabled");

	return ret;
}

bool MainProcess::upnpScanRequest()
{
	bool ret = false;
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		if (0 > ctrlRequest("UPNP_REFRESH 10", res, &len))
			break;

		if (strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail UPnP scan-request.\n");

	return ret;
}

bool MainProcess::getUpnpScanResults(char *result, size_t *len)
{
	bool ret = false;

	do {
		if (!result && !len)
			break;

		if (0 > ctrlRequest("GET_UPNP_SCAN_RESULTS", result, len))
			break;
		result[*len] = 0;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to get UPnP scan-results.\n");

	return ret;
}

bool MainProcess::sendUpnpGetDevInfo(const char *control_url)
{
	bool ret = false;
	char cmd[BUFSIZ];
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		if (!control_url)
			break;

		os_snprintf(cmd, sizeof(cmd), "SEND_UPNP_GETDEVINFO %s", control_url);
		if (0 > ctrlRequest(cmd, res, &len))
			break;

		if (strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to send GetDeviceInfo thru UPnP.\n");

	return ret;
}

bool MainProcess::sendSelectedRegistrar(const char *control_url, bool enabled)
{
	bool ret = false;
	char cmd[BUFSIZ];
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		if (!control_url)
			break;

		os_snprintf(cmd, sizeof(cmd), "SET_UPNP_SEL_REG %s %d", control_url, enabled?1:0);
		if (0 > ctrlRequest(cmd, res, &len))
			break;

		if (strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to send Selected-Registrar thru UPnP.\n");

	return ret;
}

bool MainProcess::writeNfcConfig(int index)
{
	bool ret = false;
	char cmd[BUFSIZ];
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		os_snprintf(cmd, sizeof(cmd), "WRITE_CONFIG_TOKEN %d", index);
		if (0 > ctrlRequest(cmd, res, &len))
			break;

		if (strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to scan NFC token request.\n");

	return ret;
}

bool MainProcess::readNfcConfig()
{
	bool ret = false;
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		if (0 > ctrlRequest("READ_CONFIG_TOKEN", res, &len))
			break;

		if (strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to scan NFC token request.\n");

	return ret;
}

bool MainProcess::writeNfcPassword(int index)
{
	bool ret = false;
	char cmd[BUFSIZ];
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		os_snprintf(cmd, sizeof(cmd), "WRITE_PASSWORD_TOKEN %d", index);
		if (0 > ctrlRequest(cmd, res, &len))
			break;

		if (strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to scan NFC token request.\n");

	return ret;
}

bool MainProcess::readNfcPassword(int index)
{
	bool ret = false;
	char cmd[BUFSIZ];
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		os_snprintf(cmd, sizeof(cmd), "READ_PASSWORD_TOKEN %d", index);
		if (0 > ctrlRequest(cmd, res, &len))
			break;

		if (strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to scan NFC token request.\n");

	return ret;
}

bool MainProcess::cancelScanNfcToken()
{
	bool ret = false;
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		if (0 > ctrlRequest("CANCEL_NFC_COMMAND", res, &len))
			break;

		if (strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to cancel NFC command.\n");

	return ret;
}

bool MainProcess::startPbc()
{
	bool ret = false;
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		len = sizeof(res) - 1;
		if (0 > ctrlRequest("WPS_PBC_ENABLED 1", res, &len))
			break;

		if (strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to start PBC method.\n");

	return ret;
}

bool MainProcess::stopPbc()
{
	bool ret = false;
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		if (0 > ctrlRequest("WPS_PBC_ENABLED 0", res, &len))
			break;

		if (strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to stop PBC method.\n");

	return ret;
}

void MainProcess::generatePIN(char pwd[9])
{
	unsigned long pin;
	unsigned char checksum;
	unsigned long acc = 0;
	unsigned long tmp;

	if (!pwd) {
		printf("Could not generate PIN with NULL-pointer.\n");
		return;
	}

	pin = rand() % 10000000;
	tmp = pin * 10;

	acc += 3 * ((tmp / 10000000) % 10);
	acc += 1 * ((tmp / 1000000) % 10);
	acc += 3 * ((tmp / 100000) % 10);
	acc += 1 * ((tmp / 10000) % 10);
	acc += 3 * ((tmp / 1000) % 10);
	acc += 1 * ((tmp / 100) % 10);
	acc += 3 * ((tmp / 10) % 10);

	checksum = (unsigned char)(10 - (acc % 10)) % 10;
	os_snprintf(pwd, 9, "%08lu", pin * 10 + checksum);
}

bool MainProcess::validatePIN(const char pwd[9])
{
	bool ret = true;
	unsigned long pin, check;
	unsigned char checksum;
	unsigned long acc = 0;
	char *tmp = 0;

	do {
		pin = strtol(pwd, &tmp, 10);
		if (!tmp || *tmp)
			break;
		check = (pin / 10) * 10;
		acc += 3 * ((check / 10000000) % 10);
		acc += 1 * ((check / 1000000) % 10);
		acc += 3 * ((check / 100000) % 10);
		acc += 1 * ((check / 10000) % 10);
		acc += 3 * ((check / 1000) % 10);
		acc += 1 * ((check / 100) % 10);
		acc += 3 * ((check / 10) % 10);
		checksum = (unsigned char)(10 - (acc % 10)) % 10;

		if (checksum != (unsigned char)atoi(&pwd[7]))
			ret = false;
	} while (0);

	return ret;
}

