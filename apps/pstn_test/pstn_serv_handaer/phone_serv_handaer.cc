/*
 * =====================================================================================
 *
 *       Filename:  phone_serv_handaer.cc
 *
 *    Description:  phone service for PAD or Handset
 *
 *        Version:  1.0
 *        Created:  05/03/2013 04:07:36 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (chen.chunsheng badwtg1111@hotmail.com), 
 *   Organization:  handaer
 *
 * =====================================================================================
 */
#include <iostream>
#include <stdlib.h>
#include "phone_proxy.h"

#include "phone_log.h"

using namespace handaer;
using namespace std;
int main(int argc, char *argv[]){
	if(argc < 1){
		//cout << "Using : " << argv[0] << " serv_port " << endl;
		PLOG(LOG_ERROR,"Using : %s serv_port ng_threshold",argv[0]);
		return -1;
	}
	// int port = atoi(argv[1]);
	int port = 63290;
	
	PhoneProxy *ppx = new PhoneProxy();

	// ppx->setNoiseGateThreshold((const char *)argv[2]);
	ppx->setNoiseGateThreshold("0.05");
	
	ppx->phoneProxyInit(port);
	ppx->accepting_loop(true);

	return 0;
}
