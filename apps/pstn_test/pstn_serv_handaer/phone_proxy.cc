/*
 * =====================================================================================
 *
 *       Filename:  phone_proxy.cc
 *
 *    Description:  phone proxy for PAD or Handset
 *
 *        Version:  1.0
 *        Created:  05/03/2013 04:07:40 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (chen.chunsheng badwtg1111@hotmail.com), 
 *   Organization:  handaer
 *
 * =====================================================================================
 */
#include <iostream>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/epoll.h>
#include <assert.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <linux/tcp.h>


#include <sys/stat.h>
#include <sys/time.h>

//////////////////////////

//////
 /////
 
 
#include "phone_thread_wrapper.h"

#include "phone_proxy.h"

#include "phone_log.h"

extern volatile int total_receive ;
extern int phone_ring_volume;

extern pthread_mutex_t phone_ring_mutex;

using namespace webrtc;
using namespace std;
namespace handaer{

CriticalSectionWrapper& _spi_critSect = 
			*CriticalSectionWrapper::CreateCriticalSection();



PhoneProxy::timer_info_t PhoneProxy::timer_info_g[PhoneProxy::CLIENT_NUM*2];
#if 1
//static int audio_argc = 9+6;
static int audio_argc = 9 +9+6;
static const char * audio_argv[] = {	
		"mediastream",	
		"--local",	
		"55556", // LOCAL_AUDIO_PORT	
		"--remote",	
		NULL, //audio_argv[4]	
		"--payload",	
		NULL, // audio_argv[6]
#if 0
		"--outfile",
		"/var/recvfrompad.wav",
#endif
		"--jitter",
		"100",
	//	"200",

#if 1

		"--ng",
		"--ng-threshold",
		//"0.02",
		"0.01",
		"--ng-floorgain",
		"0.0005",
    	"--agc",
#endif
#if 1	
	"--el",
	//"--el-speed",
	//"",
	"--el-thres",
	"0.01",
	"--el-force",
	"100000",
	"--el-sustain",
	"100",
	"--el-transmit-thres",
	"1.7"
#endif

};
#endif
#if 0
static int audio_argc = 13;
static const char *audio_argv[] = {

	"mediastream",
	"--local",
	"55556",
	"--remote",
	NULL, //argv_tele[4]
	"--payload",
	NULL,//"3",//"0",//"8",//argv_tele[6]
	//"audio/pcmu/8000",

#if 0
	"--ec",
	"--ec-tail",
	"120",//"300",
	"--ec-delay",
	"300",//"620",//"0",
	"--ec-framesize",
	"128",//"160",
#endif
#if 1
//	"--agc",

	"--ng",
	"--ng-threshold",
	"0.006",
	"--ng-floorgain",
	"0.0005",
#if 0	
	"--el",
	//"--el-speed",
	//"",
	"--el-thres",
	"0.01",
	"--el-force",
	"100000",
	"--el-sustain",
	"400",
	"--el-transmit-thres",
	"17"
#endif
#endif
	"--jitter",
	"60",
};

#endif

void PhoneProxy::setNoiseGateThreshold(const char *_ng_thres){
#if 0	
		audio_argv[11] = _ng_thres;
#endif
}




bool PhoneProxy::startAudio(cli_info_t *p_ci, int payload){
	char remote_info[64] = {0};
	char payload_info[8] = {0};	
	//cout << __FUNCTION__ << p_ci->client_ip << endl;
	PDEBUG("current client ip: ",p_ci->client_ip);
	sprintf(remote_info, "%s:%d", 
				p_ci->client_ip, 
				//"10.10.10.188",
				REMOTE_AUDIO_PORT //,
				//55558
				);
	//cout << __FUNCTION__ << remote_info << endl;
	PDEBUG("remote_info: ",remote_info);
	audio_argv[4] = remote_info;
	
	sprintf(payload_info, "%d", payload);	
	audio_argv[6] = payload_info;	
					
	if(p_ci->_ms_datas == NULL)
		p_ci->_ms_datas = mediastream_init(audio_argc, (char **)audio_argv);	
	if (p_ci->_ms_datas == NULL) 
		return false;
	audio_argv[4] = audio_argv[6] = NULL;
	mediastream_begin(p_ci->_ms_datas);
	
	return true;
}

bool PhoneProxy::stopAudio(cli_info_t *p_ci, int payload){
	if(p_ci->_ms_datas){
		mediastream_end(p_ci->_ms_datas);
		mediastream_uninit(p_ci->_ms_datas);
		p_ci->_ms_datas = NULL;
	}
	return true;

	
}


void PhoneProxy::phoneRingStart(){
	// CriticalSectionScoped lock(&_spi_critSect);

	if(dnd)
		return;
	if(ringing)
		return;

	ringing = true;
	
	const char *file;
	MSSndCard *sc;
	const char * card_id=NULL;
	
	char *ring_select = NULL;
	int n_ring_select;
	char pcm_file[20] = {0};

	pthread_mutex_lock(&phone_ring_mutex);

	char v_buffer[10] = {0};
	int fd = open("/var/voice.txt", O_RDONLY|O_NONBLOCK);
	read(fd, v_buffer, 2);
	close(fd);

	
	phone_ring_volume = atoi(v_buffer);
	PLOG(LOG_INFO,"phone_ring_volume=%d",phone_ring_volume);
	if(4 == phone_ring_volume){
		pthread_mutex_unlock(&phone_ring_mutex);
		return;
	}
	
	
	ortp_init();
	// ortp_set_log_level_mask(ORTP_MESSAGE|ORTP_WARNING|ORTP_ERROR|ORTP_FATAL);
	ortp_set_log_level_mask(ORTP_ERROR|ORTP_FATAL);

	ms_init();

	char r_buffer[10] = {0};
	// system("nvram_get 2860 Ring > /var/ring.txt");
	int fd_ring = open("/var/ring.txt", O_RDONLY|O_NONBLOCK);
	read(fd, r_buffer, 2);
	close(fd);

	pthread_mutex_unlock(&phone_ring_mutex);
	
		
	n_ring_select = atoi(r_buffer);
	snprintf(pcm_file, 16, 
					"/bin/ring_%d.wav", n_ring_select);
	PLOG(LOG_INFO,"%s : %d",pcm_file ,n_ring_select);

	if(n_ring_select > 0 && n_ring_select < 4){
		file = (const char *)pcm_file;
	}else {	
		file="/bin/oldphone-mono.wav";	
	}
	
	sc=ms_snd_card_manager_get_card(ms_snd_card_manager_get(),card_id);
#ifdef __linux
	if (sc==NULL)
	  sc = ms_alsa_card_new_custom(card_id, card_id);
#endif
	if (!rs)
		rs = ring_start(file,RING_INTERVAL,sc);
	return;
	
}




void PhoneProxy::phoneRingStop(){
	if(dnd)
		return;
	if(!ringing)
		return;
	ringing = false;

	if(4 == phone_ring_volume)
		return;
	
	if(rs){
		ring_stop(rs);
		rs = NULL;
		PLOG(LOG_INFO, "ring_stop");
	}
	ms_exit();
	ortp_exit();
	return;
}

void PhoneProxy::setDND(bool yes_no){
	dnd = yes_no;
}

static CriticalSectionWrapper& _phone_critSect = *CriticalSectionWrapper::CreateCriticalSection();

PhoneProxy::PhoneProxy()
	:	_ptrThreadPhoneProxy(NULL),
		_phoneProxyThreadId(0),	
		_phoneProxyRunning(false),	
		_ptrThreadHeartBeating(NULL),
		_heartBeatingThreadId(0),	
		_heartBeatingisRunning(false),
		pcs(NULL),
		timer(NULL),
#if 0
		_ms_datas(NULL),
#endif
		server_port(-1),
		payload_type(-1),
		serv_listen_sockfd(-1),
		_phone_control_service_event(DEFAULT_INCOMING) ,
		phone_in_use(false) ,
		// _phone_critSect(*CriticalSectionWrapper::CreateCriticalSection()),
		//timer_count(0),
		//ring_count(0)
		incoming_call(false),
		ci_transout(NULL),
		ci_transinto(NULL)
		,ci_talkback_out(NULL)
		,ci_talkback_into(NULL)
		,rs(NULL)
		,dnd(false)
		,ringing(false)
		{

	pthread_mutex_lock(&phone_ring_mutex);

	// CriticalSectionScoped lock(&_spi_critSect);

	memset(incoming_response, 0, sizeof(incoming_response));
	
	phone_proxy_fd[0] = -1;
	phone_proxy_fd[1] = -1;

	memset(ci, 0 ,sizeof(ci));
	for (int i=0; i<CLIENT_NUM ; i++) {
		ci[i].client_fd = -1;
		ci[i].id = -1;
		
	}
	
	memset(&_cli_req, 0, sizeof(cli_request_t));
	_cli_req.cmd = DEFAULT;

	//system("nvram_set freespace phone_name1 PAD");
	//system("nvram_set freespace phone_name2 handset1");
	//system("nvram_set freespace phone_name3 handset2");	
	//system("nvram_set freespace phone_name4 handset3");

	memset(phone_name_id, 0, sizeof(phone_name_id));
	memset( username_in_flash, 0, sizeof(username_in_flash));
	memset( passwd_in_flash, 0, sizeof(passwd_in_flash));

#if 1
	for (int i=0; i<CLIENT_NUM ; i++) {		
		snprintf(phone_name_id[i], 12, 
						"phone_name%d", (i+1));		
		// const char *name = nvram_bufget(1, phone_name_id[i]);

		char name[10] = {0};
		char command[128] = {0};
		char name_file[64] = {0};
		// sprintf(command, "nvram_get freespace %s > /var/%s.txt",
						 // phone_name_id[i], phone_name_id[i]);


		// system(command);

		sprintf(name_file, "/var/%s.txt", phone_name_id[i]);
		int fd = open(name_file, O_RDWR|O_CREAT|O_NONBLOCK);
		read(fd, name, 10);
		// name[strlen(name)-1] = '\0';
		// for(int i = 0; i < 10; i++)
		// 	printf("%x ", name[i]);
		// printf("\n");
		close(fd);









		if(memcmp(name, "", 1) == 0){
			username_is_null[i] = true;
		}
		if(!username_is_null[i])
			memcpy(username_in_flash[i], name, strlen(name)+1);

		cout << phone_name_id[i] << "... "  
			 << username_in_flash[i] << " " << endl;
	}
	// const char *passwd_in = nvram_bufget(1, "password");
	char passwd_in[10] = {0};
	// system("nvram_get freespace password > /var/password.txt");
	
	int fd = open("/var/password.txt", O_RDWR|O_CREAT|O_NONBLOCK);
	read(fd, passwd_in, 10);
	// passwd_in[strlen(passwd_in)-1] = '\0';
	close(fd);





	if(memcmp(passwd_in, "", 1) == 0){
		// system("nvram_set freespace password 000000");
		fd = open("/var/password.txt", O_RDWR|O_CREAT|O_NONBLOCK);
		write(fd, "000000", 7);
		close(fd);
	}
	// passwd_in = nvram_bufget(1, "password");
	// system("nvram_get freespace password > /var/password.txt");
	memset(passwd_in, 0, sizeof(passwd_in));
	fd = open("/var/password.txt", O_RDWR|O_CREAT|O_NONBLOCK);
	read(fd, passwd_in, 10);
	// passwd_in[strlen(passwd_in)-1] = '\0';
	close(fd);	



	memcpy(passwd_in_flash, passwd_in, strlen(passwd_in)+1);
	//cout << "passwd_in_flash: " << passwd_in_flash << endl;
	PLOG(LOG_INFO, "passwd_in_flash: %s", passwd_in_flash);
	pthread_mutex_unlock(&phone_ring_mutex);
#endif
}

PhoneProxy::~PhoneProxy(){
	phoneProxyExit();
	delete &_phone_critSect;
}




int PhoneProxy::listenOnServerSocket(int port){
	int listen_fd, on, ret;	
	struct sockaddr_in srv_addr;
	
	listen_fd = socket(PF_INET, SOCK_STREAM, 0);
	if(listen_fd < 0) {
		perror("cannot create listening socket: ");
		exit (-1);
	}
	//cout << "socket success" << endl;
	
	int rett;	
	rett = fcntl(listen_fd, F_GETFL, 0);  
	fcntl(listen_fd, F_SETFL, rett | O_NONBLOCK);

	/* Enable address reuse */
	on = 1;	
	ret = setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	on=1;	 
	setsockopt(listen_fd, IPPROTO_TCP, TCP_NODELAY,&on,sizeof(on));
	
	struct linger {
		int l_onoff;
		int l_linger;
	};
	struct linger m_sLinger;
	m_sLinger.l_onoff=0;
	m_sLinger.l_linger = 1;   
	setsockopt(listen_fd,SOL_SOCKET,SO_LINGER,&m_sLinger,sizeof(m_sLinger)); 




	//fill server addr info
	memset(&srv_addr, 0, sizeof(srv_addr));
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	srv_addr.sin_port = htons(port);
	
	ret = bind(listen_fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr));
	if(ret == -1){
		perror("cannot bind server socket: ");
		close(listen_fd);
		exit (-1);
	}
	//cout << "bind success" << endl;

	//listen
	ret = listen(listen_fd, BACK_LOG);	
	if(ret == -1) {
		perror("cannot listen the client connect request: ");
		close(listen_fd);
		exit (-1);
	}
	//cout << "listen success" << endl;

	serv_listen_sockfd = listen_fd;
	//cout << __FUNCTION__ << " : " << serv_listen_sockfd << endl;
	PDEBUG("serv_listen_sockfd = %d", serv_listen_sockfd);

	return listen_fd;
}



int PhoneProxy::my_accept(struct sockaddr *addr, socklen_t *addrlen){
	int com_fd;	
	com_fd = accept(serv_listen_sockfd, addr, addrlen);	
	if (com_fd < 0) {
		if (errno == EINTR) {
			cout << __FUNCTION__ << " : " << serv_listen_sockfd << endl;
			perror("accept EINTR Error: ");
			return (-EINTR);
		} else 	if (errno == EAGAIN) {
			// cout << __FUNCTION__ << " : " << serv_listen_sockfd << endl;
			// perror("accept EAGAIN Error: ");
			return (-EAGAIN);
		}
		 else {
			perror("cannot accept client connect request: ");
			close(serv_listen_sockfd);
			return (-1);
		}
	}
	//cout << "com_fd = " << com_fd << endl;
	PDEBUG("com_fd = %d",com_fd);

	return com_fd;
}

int PhoneProxy::accepting_loop(bool yes_no){
	while(yes_no){
		sleep(1);
	}

#if 0	
	struct sockaddr_in clt_addr;
	socklen_t clt_len;
	int clt_sockfd;
	while (yes_no) {

accept_again:

		usleep(30 * 1000);
		clt_len = sizeof(clt_addr);
		clt_sockfd = my_accept((struct sockaddr *)&clt_addr, &clt_len);
		if(clt_sockfd == -EINTR || clt_sockfd == -EAGAIN || clt_sockfd == -1){
			continue;
		}
		char *new_ip = NULL;
		struct in_addr ia = clt_addr.sin_addr; //(int)ip
		new_ip = inet_ntoa(ia);

		for(int i=0; i<CLIENT_NUM ; i++){
			if(strcmp(new_ip, ci[i].client_ip) == 0){
				close(clt_sockfd);
				goto accept_again;
			}
		}


		
		for(int i=0; i<CLIENT_NUM ; i++){
			
			if(ci[i].client_fd == -1){
				ci[i].client_fd = clt_sockfd;
				//cout << "accept" << i << " " << ci[i].client_fd << endl;
				memset(ci[i].client_ip, 0, sizeof(ci[i].client_ip));
				memcpy(ci[i].client_ip, new_ip, strlen(new_ip)+1);
//				cout << " f: " << __FUNCTION__
//					 << " l: " << __LINE__
//					 << " client_ip = " << ci[i].client_ip
//					 << " client_num = " << i << endl;
				PLOG(LOG_INFO,"ip = %s client_num = %d",ci[i].client_ip,i);

				if(ci[i].timer_setting == false){
					ci[i].timer_setting = true;				
					ci[i].old_timer_counter = ci[i].timer_counter = 0;
					ci[i].exit_threshold = 0;
					setClientTimer(&ci[i], OPTION_INTERVAL, recycleClient, &ci[i], this, TIMER_HEARTBEAT);
				}
				
#if 1
				int ret;	
				ret = fcntl(clt_sockfd, F_GETFL, 0);  
				fcntl(clt_sockfd, F_SETFL, ret | O_NONBLOCK);
				
				int on=1;	 
				setsockopt(clt_sockfd, IPPROTO_TCP, TCP_NODELAY,&on,sizeof(on));

				struct linger {
					int l_onoff;
					int l_linger;
				};
				struct linger m_sLinger;
				m_sLinger.l_onoff=0;
				m_sLinger.l_linger = 1;   
				setsockopt(clt_sockfd,SOL_SOCKET,SO_LINGER,&m_sLinger,sizeof(m_sLinger)); 

				
#endif
#if 0
				struct epoll_event ev;
				ev.events = EPOLLIN;
				ev.data.ptr = &ci[i];
				if(epoll_ctl(epollfd, EPOLL_CTL_ADD, clt_sockfd, &ev) == -1){
					perror("epoll_ctl : client_fd ");
					exit (-1);
				}
#endif

				
				break;
				
			}
		}	
		for(int i=0; i<CLIENT_NUM; i++){
			//cout << i << " " << ci[i].client_fd << endl;
			PLOG(LOG_INFO, "%d:cli_fd=%d", i ,ci[i].client_fd);
		}
	}
	
#endif
	return 0;
}


PhoneProxy::PstnCmdEnum PhoneProxy::getCmdtypeFromString(char *cmd_str){
	PstnCmdEnum cmdtype;
	
	if (strncmp(cmd_str, "OPTION_", 7) == 0) {
		cmdtype = HEARTBEAT;	
	} else if (strncmp(cmd_str, "DIALING", 7) == 0) {
		cmdtype = DIALING;
	} else if (strncmp(cmd_str, "OFFHOOK", 7) == 0) {
		cmdtype = OFFHOOK;	
	} else if (strncmp(cmd_str, "ONHOOK_", 7) == 0) {
		cmdtype = ONHOOK;		
	} else if (strncmp(cmd_str, "DTMFSND", 7) == 0) {
		cmdtype = DTMF_SEND;	
	} else if (strncmp(cmd_str, "FLASH__", 7) == 0) {
		cmdtype = FLASH;
	} else if (strncmp(cmd_str, "MUTE___", 7) == 0) {
		cmdtype = MUTE;	
	} else if (strncmp(cmd_str, "QUERY__", 7) == 0) {	
		cmdtype = QUERY;
	} else if (strncmp(cmd_str, "LOGIN__", 7) == 0) {
		cmdtype = LOGIN;	
	} else if (strncmp(cmd_str, "LOGOUT_", 7) == 0) {
		cmdtype = LOG_OUT;	
	} else if (strncmp(cmd_str, "LOGOUTA", 7) == 0) {
		cmdtype = LOGOUTALL;	
	} else if (strncmp(cmd_str, "SWTOSON", 7) == 0) {
		cmdtype = SWITCHTOSONMACHINE;	
	} else if (strncmp(cmd_str, "SWOFFHK", 7) == 0) {
		cmdtype = SWITCHOFFHOOK;	
	} else if (strncmp(cmd_str, "SWONHOK", 7) == 0) {
		cmdtype = SWITCHONHOOK;
	} else if (strncmp(cmd_str, "SWIBACK", 7) == 0) {
		cmdtype = SWITCHBACK;	
	} else if (strncmp(cmd_str, "TB_DIAL", 7) == 0) {
		cmdtype = TALKBACKDIALING;
	} else if (strncmp(cmd_str, "TB_OFHK", 7) == 0) {
		cmdtype = TALKBACKOFFHOOK;
	} else if (strncmp(cmd_str, "TB_ONHK", 7) == 0) {
		cmdtype = TALKBACKONHOOK;	
	} else if (strncmp(cmd_str, "INCOMOK", 7) == 0) {
		cmdtype = INCOMING_OK;	
	} else 	{
		cout << "command undefined" << endl;
		cmdtype = UNDEFINED;
	}

	return cmdtype;	
}



int PhoneProxy::netWrite(int fd,const void *buffer,int length){
	int bytes_left; 
	int written_bytes; 
	const char *ptr;
	ptr = (const char *)buffer; 
	bytes_left = length; 

	struct timeval tv;
	fd_set w_set;
	int result = -1;

	int timeout_write = 0;

	struct timeval orig, cur;
	gettimeofday(&orig, NULL);


	while(bytes_left > 0) {

		gettimeofday(&cur, NULL);

		timeout_write = ((cur.tv_sec - orig.tv_sec)*1000) + ( (cur.tv_usec - orig.tv_usec)/1000 );
		if(timeout_write > 500){
			PLOG(LOG_INFO, "netWrite timeout");
			break;
		}
	// while(bytes_left >= 0) {
		// PLOG(LOG_INFO, "before write"); 
		FD_ZERO(&w_set);
		FD_SET(fd, &w_set);

		tv.tv_sec = 0;
		tv.tv_usec = 30 * 1000;
		result = select(fd+1, NULL, &w_set, NULL, &tv);
		if (result < 0)	{
			if (EINTR == errno)	{
				continue;
			}
			// close(sockfd);
			perror("select in netWrite : ");
			return -1;
		} else if (0 == result)	{
			//this means timeout, it is not an error, so we return 0.
			return 0;
		} else {
			if (FD_ISSET(fd, &w_set)) {


				//begin to write
				written_bytes = write(fd,ptr,bytes_left); 
				if(written_bytes <= 0) { //error      
					perror("netWrite error : "); 
					if(errno == EINTR || EAGAIN == errno ){ //interrupt error, we continue writing
						written_bytes = 0;
					} else {             //other error, exit 
						return (-1);
					} 
				} 

				bytes_left -= written_bytes; 
				ptr += written_bytes;     //from where left,  continue writing
				// PLOG(LOG_INFO, "write ok: bytes_left = %d", bytes_left);
			}

		}
	} 
	return (0); 
}



int PhoneProxy::netRead(int fd,void *buffer,int length){
	int bytes_left; 
	int bytes_read; 
	char *ptr;	
    ptr = (char *)buffer;
	bytes_left = length; 
	while(bytes_left > 0) { 
		bytes_read = read(fd,ptr,bytes_left); 
		if(bytes_read < 0) { 
			if(errno == EINTR){ 
				bytes_read = 0;
				perror("net_read error EINTR: ");
			}else {
				perror("net_read error other: ");
				return (-1);
			}
		} else if (bytes_read == 0) {
			perror("net_read bytes = 0 : ");
			break; 
		}
		bytes_left -= bytes_read; 
		ptr += bytes_read; 
	} 
	
	return (length - bytes_left); 
}




//buffer end with '\n' (in count) and then '\0'(out of count)
ssize_t PhoneProxy::netReadLine(int fd,void *buffer,size_t maxlen, int i){
	ssize_t 	n;
	PLOG(LOG_INFO, "=-------------------------");
	if ( (n = readline(fd, buffer, maxlen, i)) < 0){
		perror("PhoneProxy::netReadLine readline error: ");
		PLOG(LOG_INFO,"readline error ");
	}
	PLOG(LOG_INFO, "======----------------");
	return(n);	
}



// head      id     length       cmd        arg_len      argument (64)       terminator
// HEADS    1	     022    	DIALING	   012       913146672056         \n

int PhoneProxy::parseClient(cli_info_t *ci_ , int i){

//	cout << __FUNCTION__ << " : " << __LINE__ << endl;
//	cout << "client_fd : " << ci_->client_fd << endl;
	PDEBUG( "client_fd : %d ", ci_->client_fd);
	ssize_t nread;
	char buff[MAX_LEN_PCLI] = {0};
	char *pbuff = buff;

	if(ci_->client_fd == -1){
		PLOG(LOG_ERROR,"%s@%s offline because of client_fd = -1", ci_->username, ci_->client_ip);
		destroyClient(ci_);
		return (-1);
	} else {
		CriticalSectionScoped lock(&_phone_critSect);
		nread = netReadLine(ci_->client_fd, buff, MAX_LEN_PCLI, i);
		// nread =  netRead(ci_->client_fd, buff, MAX_LEN_PCLI);
		// nread = read(ci_->client_fd, buff, MAX_LEN_PCLI);

	}
	//nread = read(ci_->client_fd, buff, MAX_LEN_PCLI);
	//cout << __FUNCTION__ << " : " << __LINE__ << endl;
	//cout << "nread : " << nread << endl;
	PDEBUG("nread : %d ",nread);
	if(nread < 0){
		perror("someone offline<0: ");
		PLOG(LOG_INFO, "%s@%s offline<0", 
			ci_->username, ci_->client_ip);
		destroyClient(ci_);
		return (-1);
	}
	if(nread == 0){
		perror("someone offline=0: ");
		PLOG(LOG_INFO, "%s@%s offline=0", 
			ci_->username, ci_->client_ip);
		destroyClient(ci_);
		return (-1);
	}
	//cout << buff << endl;
	// PLOG(LOG_INFO, "%s:%d@%s:%s", ci_->username, 
	// 					ci_->id, ci_->client_ip, buff);
	memset(&_cli_req, 0, sizeof(cli_request_t));
	memcpy(_cli_req.head, pbuff, 5);
	if(strcmp(_cli_req.head, "HEADS") != 0){
		memset(&_cli_req, 0, sizeof(cli_request_t));
		return -1;
	}

	_cli_req.fd = ci_->client_fd;
	//ci_->id = 
		_cli_req.id = pbuff[5];
	
	
	char length[4] = {0};
	char arg_len[4] = {0};
	memcpy(length, pbuff+6, 3);
	memcpy(arg_len, pbuff+16, 3);

	_cli_req.length = atoi(length);
	_cli_req.arglen = atoi(arg_len);
	if(_cli_req.length - _cli_req.arglen != 10){
		memset(&_cli_req, 0, sizeof(cli_request_t));
		return -1;
	}

	char cmd[8] = {0};
	memcpy(cmd, pbuff+9, 7);
	_cli_req.cmd = getCmdtypeFromString(cmd);
	// if( _cli_req.cmd != HEARTBEAT)
	{
		PLOG(LOG_INFO, "%s:%d@%s:%d:%s", ci_->username, 
						ci_->id, ci_->client_ip, ci_->client_fd,buff);
	}

	if(_cli_req.cmd == DIALING){
		if(_cli_req.id != ci_->id + '0'){
			PLOG(LOG_INFO, "%s:%d:%d@%s:%d:%s", ci_->username, 
					ci_->id, _cli_req.id,ci_->client_ip, ci_->client_fd,buff);

			_cli_req.cmd = UNDEFINED;	
		}

	}


	//cout << "_cli_req.cmd :" <<_cli_req.cmd << endl;
	if(_cli_req.cmd == UNDEFINED){
		PLOG(LOG_INFO, "UNDEFINED");
		memset(&_cli_req, 0, sizeof(cli_request_t));
		return -1;
	}
	if(_cli_req.cmd == LOGIN){
		char usename_len_clt[4] = {0};
		memcpy(usename_len_clt, pbuff+19, 3);
		_cli_req.user_name_len = atoi(usename_len_clt);

		if(_cli_req.user_name_len >= 19 || _cli_req.user_name_len < 3){
			memset(&_cli_req, 0, sizeof(cli_request_t));
			_cli_req.cmd = UNDEFINED;
			return -1;
		}
		memcpy(_cli_req.username, pbuff+22, _cli_req.user_name_len);
		memcpy(_cli_req.password, pbuff+22+_cli_req.user_name_len, 6);
		return 0;
	}

	

	memcpy(_cli_req.arg, pbuff+19, _cli_req.arglen);

	return 0;
}




int PhoneProxy::dispatchClientId(cli_info_t *cci){	
#if 0	
	for(int i = 0;i < CLIENT_NUM; i++){
		
		for(int j = 0;j < CLIENT_NUM; j++)
			cout <<j<< " :ci-client_fd " << (int)ci[j].client_fd << endl;

		if(ci[i].client_fd ==  cci->client_fd){			
			cout << ci[i].client_fd <<" " << cci->client_fd <<endl;			
			ci[i].id = i+1;			
			cout <<i<< " :ci->id " << (int)cci->id << endl;			
			return (0); // for client i, id = i+1
		}

		if(strcmp(_cli_req.username, ci[i].username) == 0
			 )
			return -1; // the same username

	}	
#endif		
		

	int client_num = 0;
	char online_bit[CLIENT_NUM+1] = {'0','0','0','0','\0'};
	client_num = queryOnlineInfo(online_bit);
	if(client_num == CLIENT_NUM)
		return -1;
	if((online_bit[0] == '1' )
		&& (strcmp(_cli_req.username, "PAD")==0))
		return -1;
	if((client_num == CLIENT_NUM-1) 
			&& (online_bit[0] == '0' ) 
			&& (strcmp(_cli_req.username, "handset1")==0) )
		return -1;
	
	for (int i=0 ; i<CLIENT_NUM ; i++){
		if(online_bit[i] == '1')
			continue;
		if(strcmp(_cli_req.username, "PAD")==0){
			cci->id = 1;
			break;
		}
		
		if((strcmp(_cli_req.username, "handset1")==0)
			 && (i > 0)){
			cci->id = i+1;
			break;
		}
	}
	//cout << __FUNCTION__ <<" : "<<  (int)cci->id << endl;
	PDEBUG("dispatch Client Id : %d", cci->id);
#if 0	
	else if(strcmp(_cli_req.username, "handset1")==0)
		cci->id = 2;
	else if(strcmp(_cli_req.username, "handset2")==0)
		cci->id = 3;
	else if(strcmp(_cli_req.username, "handset3")==0)
		cci->id = 4;
#endif	
	return 0; //success
}

int PhoneProxy::queryOnlineInfo(char *h1){
	int online_count = 0;
	for(int i = 0;i < CLIENT_NUM; i++){
		if(ci[i].id == (char)-1)
			continue;
		
			online_count ++;
#if 0			
			h1[ci[i].id - 1] = '1';
#endif

#if 1
			if(strcmp(ci[i].username, "PAD")==0)
				h1[0] = '1';
			if(strcmp(ci[i].username, "handset1")==0)
				h1[1] = '1';
			if(strcmp(ci[i].username, "handset2")==0)
				h1[2] = '1';
			if(strcmp(ci[i].username, "handset3")==0)
				h1[3] = '1';
#endif
		
	}
	return online_count;
}

static CriticalSectionWrapper& _trans_critSect = 
			*CriticalSectionWrapper::CreateCriticalSection();

static int timer_count_trans = 0;
static int timeout_trans = 0;

static int timer_count_talkback = 0;
static int timeout_talkback = 0;

static CriticalSectionWrapper& _incom_critSect = 
			*CriticalSectionWrapper::CreateCriticalSection();

static int timer_count = 0;
static int ring_count = 0;
static bool ring_should_finish  = false;

static CriticalSectionWrapper& _going_critSect = 
			*CriticalSectionWrapper::CreateCriticalSection();

static int already_incoming = 0;

int PhoneProxy::handleClient(cli_info_t *ci_){
	if(ci_->client_fd == -1){
		//cout << "the client is exit" << endl;
		perror("the client is exit in handleClient");
		PLOG(LOG_INFO, "the client is exit");
		return (-1);
	}
	char send_buf[MAX_LEN_PCLI] = {0};
	//cout << __FUNCTION__ << " : " << __LINE__ << endl;
	PDEBUG("in handleClient");
	switch(_cli_req.cmd){
		case UNDEFINED:
			break;
		case DEFAULT:
			break;
		case HEARTBEAT:
			{
				//cout << "HEARTBEAT" << endl;
				
				if(ci_->timer_setting == true){
					ci_->timer_counter ++;
				}

				int client_num = 0;
				char online_bit[CLIENT_NUM+1] = {'0','0','0','0','\0'};
				client_num = queryOnlineInfo(online_bit);
				snprintf(send_buf, 29, 
					"HEADR%d010OPTI_OK000004%s\r\n", (ci_->id == (char)-1)?0:ci_->id, 
					online_bit);
				//cout << send_buf << endl;
				// PLOG(LOG_INFO, "%s", send_buf);
				CriticalSectionScoped lock(&_phone_critSect);
				if(ci_->client_fd != -1)
					netWrite(ci_->client_fd, send_buf, strlen(send_buf));
#if 0
				for(int i=0;i<CLIENT_NUM;i++){
					cout << ci[i].username << "..." << ci[i].client_fd;
					printf(" id=%d\n", ci[i].id); 
				}
#endif
			}
			break;
		case DIALING:
			if(ci_->registerd /*&& ci_->using_phone*/){
				CriticalSectionScoped lock(&_phone_critSect);
				if(ci_->client_fd != -1)
					netWrite(ci_->client_fd, "HEADR0010DIAL_OK000\r\n", 21);
				if(ci_->timer_setting == true){
					ci_->timer_counter ++;
				}
				// PLOG(LOG_INFO, "in_user %d ci->inuser %d", phone_in_use, ci_->using_phone );

				if((!phone_in_use) ){
					// CriticalSectionScoped lock(&_trans_critSect);

					ci_->using_phone = true;
					phone_in_use = true;

					memset(client_ip_using, 0, sizeof(client_ip_using));
					memcpy(client_ip_using, ci_->client_ip, sizeof(ci_->client_ip));

					ring_should_finish = true;				
					timer_count = ring_count = 0;
					// phoneRingStop();

					netWrite(phone_proxy_fd[0], "RINGOFF\n", 8);
					incoming_call = false;


					PLOG(LOG_INFO, "before in dialing");

					snprintf(send_buf, 22, 
						"HEADR0010RINGOFF000\r\n");			
					for(int i=0; i<CLIENT_NUM; i++){
						ci[i].has_incom_ok = false;
						if(ci[i].id  == ci_->id)
							continue;
						if(ci[i].client_fd != -1)
							netWrite(ci[i].client_fd, send_buf, strlen(send_buf));
					}
					
					PLOG(LOG_INFO, "after in dialing");


					startAudio(ci_, PAYLOAD_PCMU);
					
					pcs->onHook();
					usleep(300000);
					
					pcs->offHook();
					usleep(1000000);

				
					pcs->setGoingNumber(_cli_req.arg, _cli_req.arglen);
					//netWrite(phone_proxy_fd[0], "DIALING\n", 8);

					pcs->dialUp(pcs->going_number,strlen(pcs->going_number));

					PLOG(LOG_INFO, "after dialUp");
					// PLOG(LOG_INFO, "in_user %d ci->inuser %d", phone_in_use, ci_->using_phone );

				}else if( (!strcmp(ci_->client_ip, client_ip_using) ) ) {
					snprintf(send_buf, 22, 
						"HEADR0010CALLING000\r\n");
					if(ci_->client_fd != -1)
						netWrite(ci_->client_fd, send_buf, strlen(send_buf));
					

				} else {
					snprintf(send_buf, 22, 
						"HEADR0010INUSING000\r\n");
					if(ci_->client_fd != -1)
						netWrite(ci_->client_fd, send_buf, strlen(send_buf));

				}
			}else {
				CriticalSectionScoped lock(&_phone_critSect);
				snprintf(send_buf, 22, "HEADR0010UNREGST000\r\n");
				if(ci_->client_fd != -1)
					netWrite(ci_->client_fd, send_buf, strlen(send_buf));

				
			}
			break;
		case OFFHOOK:
			if(ci_->registerd){
				CriticalSectionScoped lock(&_phone_critSect);

				if(ci_->client_fd != -1)
					netWrite(ci_->client_fd, "HEADR0010OFHK_OK000\r\n", 21);
				if(ci_->timer_setting == true){
					ci_->timer_counter ++;
				}
				if((!phone_in_use) ){
					
#if 0
					if(!incoming_call){
						//netWrite(phone_proxy_fd[0], "ONHOOK\n", 7);
						pcs->onHook();
						usleep(500000);
					}
#endif
					
					if(incoming_call){
						ci_->using_phone = true;
						phone_in_use = true;
						// already_incoming = 0;

						memset(client_ip_using, 0, sizeof(client_ip_using));
						memcpy(client_ip_using, ci_->client_ip, sizeof(ci_->client_ip));

						ring_should_finish = true;
					
						timer_count = ring_count = 0;
						// phoneRingStop();
						
						startAudio(ci_, PAYLOAD_PCMU);
						//netWrite(phone_proxy_fd[0], "OFFHOOK\n", 8);					
						pcs->offHook();
						already_incoming = 0;
						usleep(10000);
						netWrite(phone_proxy_fd[0], "RINGOFF\n", 8);
						incoming_call = false;
						snprintf(send_buf, 22, 
							"HEADR0010RINGOFF000\r\n");			
						for(int i=0; i<CLIENT_NUM; i++){
							ci[i].has_incom_ok = false;
							if(ci[i].id  == ci_->id)
								continue;
							if(ci[i].client_fd != -1){
								PLOG(LOG_INFO, "send ringoff to %s",ci[i].username);
								netWrite(ci[i].client_fd, send_buf, strlen(send_buf));
							}
						}
						usleep(500000);
						for(int i=0; i<CLIENT_NUM; i++){
							ci[i].has_incom_ok = false;
							if(ci[i].id  == ci_->id)
								continue;
							if(ci[i].client_fd != -1){
								PLOG(LOG_INFO, "send ringoff to %s",ci[i].username);
								netWrite(ci[i].client_fd, send_buf, strlen(send_buf));

							}
						}

					}
						
				}else if( (!strcmp(ci_->client_ip, client_ip_using) ) ) {
					snprintf(send_buf, 22, 
						"HEADR0010CALLING000\r\n");
					if(ci_->client_fd != -1)
						netWrite(ci_->client_fd, send_buf, strlen(send_buf));
					

				} else {
					snprintf(send_buf, 22, 
						"HEADR0010INUSING000\r\n");
					if(ci_->client_fd != -1)
						netWrite(ci_->client_fd, send_buf, strlen(send_buf));

				}
			}else {
				CriticalSectionScoped lock(&_phone_critSect);
				snprintf(send_buf, 22, 
					"HEADR0010UNREGST000\r\n");
				if(ci_->client_fd != -1)
					netWrite(ci_->client_fd, send_buf, strlen(send_buf));

				
			}
			break;
		case ONHOOK:
			if(ci_->registerd ){
				CriticalSectionScoped lock(&_phone_critSect);

				if(ci_->client_fd != -1)
					netWrite(ci_->client_fd, "HEADR0010ONHK_OK000\r\n", 21);
				if(ci_->timer_setting == true){
					ci_->timer_counter ++;
				}
				if(incoming_call){
					// phoneRingStop();
					timer_count = ring_count = 0;
					incoming_call = false;
					ci_->using_phone = false;
					phone_in_use = false;
					//netWrite(phone_proxy_fd[0], "OFFHOOK\n", 8);
					pcs->offHook();
					already_incoming = 0;
					ring_should_finish = true;
					usleep(1000000);
					
					//netWrite(phone_proxy_fd[0], "ONHOOK\n", 7);
					pcs->onHook();
					usleep(10000);
					netWrite(phone_proxy_fd[0], "RINGOFF\n", 8);

					snprintf(send_buf, 22, 
						"HEADR0010RINGOFF000\r\n");			
					for(int i=0; i<CLIENT_NUM; i++){
						ci[i].has_incom_ok = false;
						if(ci[i].id  == ci_->id)
							continue;
						if(ci[i].client_fd != -1)
							netWrite(ci[i].client_fd, send_buf, strlen(send_buf));
					}
					
					

					
					usleep(2000000);
					ring_should_finish = false;


					
					break;
				}
				// printf(char *__format, ...)
				 PLOG(LOG_INFO, "in_user %d ci->client_ip:%s client_ip_using : %s", 
				 	    phone_in_use, ci_->client_ip,  client_ip_using);
				 
				if (  phone_in_use && 
					(//ci_->using_phone || 
						(!strcmp(ci_->client_ip, client_ip_using) ) )
					) {

					memset(client_ip_using, 0, sizeof(client_ip_using));
					ring_should_finish = false;
					CriticalSectionScoped lock(&_trans_critSect);
					ci_->using_phone = false;
					phone_in_use = false;
					ci_transout = NULL;
					timer_count_trans = timeout_trans = 0;
					usleep(500000);
					// PLOG(LOG_INFO, "in_user %d ci->inuser %d", phone_in_use, ci_->using_phone );
					//netWrite(phone_proxy_fd[0], "ONHOOK\n", 7);
					pcs->onHook();
					stopAudio(ci_, PAYLOAD_PCMU);
					usleep(10000);

					PLOG(LOG_INFO, "onhook success");
					
				}
				// PLOG(LOG_INFO, "in_user %d ci->inuser %d", phone_in_use, ci_->using_phone );
			}
			break;
		case DTMF_SEND:
			if(ci_->registerd && ci_->using_phone){
				if(ci_->timer_setting == true){
					ci_->timer_counter ++;
				}
				pcs->setDtmfNumber(_cli_req.arg[0]);
				usleep(10000);
				//netWrite(phone_proxy_fd[0], "DTMFSEND\n", 9);
				pcs->sendDtmf(pcs->dtmf);
			}
			break;
		case FLASH:
			if(ci_->registerd && ci_->using_phone){
				if(ci_->timer_setting == true){
					ci_->timer_counter ++;
				}
				usleep(10000);
				//netWrite(phone_proxy_fd[0], "FLASH\n", 6);
				pcs->flash();
			}
			break;
		case MUTE:
			if(ci_->registerd && ci_->using_phone){
				if(ci_->timer_setting == true){
					ci_->timer_counter ++;
				}
				bool mute;
				if(_cli_req.arg[0] == '1')
					mute = true;
				else if(_cli_req.arg[0] == '0')
					mute = false;
				pcs->setMute(mute);
				usleep(10000);
				//netWrite(phone_proxy_fd[0], "MUTE\n", 5);
				pcs->mute(pcs->mute_ok);
			}
			break;
		case QUERY:
			if(ci_->registerd){

			}			
			break;
		case LOGIN:
			{
				if(ci_->timer_setting == true){
					ci_->timer_counter ++;
				}
				// CriticalSectionScoped spi_lock(&_spi_critSect);
				CriticalSectionScoped lock(&_phone_critSect);
				pthread_mutex_lock(&phone_ring_mutex);
				
				//cout << "LOGIN" << endl;
				PLOG(LOG_INFO, "LOGIN , ci_->registerd= %d", ci_->registerd);
				if(ci_->registerd){
					pthread_mutex_unlock(&phone_ring_mutex);
					break;
				} else {
					//cout << "LOGIN not registered" << endl;
					PLOG(LOG_INFO, "LOGIN not registered");
					if(strcmp(_cli_req.password, passwd_in_flash) == 0){
						
						int id_ret = dispatchClientId(ci_);
						//ci_->id = id_ret;
						//cout << "id_ret = " << id_ret << endl;
						PLOG(LOG_INFO,"id_ret = %d",id_ret);
						if(id_ret == -1){						
							snprintf(send_buf, 22, 
								"HEADR0010NMEXIST000\r\n");
							//cout << send_buf << endl;
							PLOG(LOG_INFO,"%s", send_buf);
							if(ci_->client_fd != -1)
								netWrite(ci_->client_fd, send_buf, strlen(send_buf));
							
							destroyClient(ci_);
							

						} else {
							ci_->registerd = true;
							

							
							snprintf(send_buf, 22, 
								"HEADR%d010LOGINRE000\r\n", ci_->id);
							//cout << send_buf << endl;
							PLOG(LOG_INFO,"%s",send_buf);

							// sleep(3);
							if(ci_->client_fd != -1)
								netWrite(ci_->client_fd, send_buf, strlen(send_buf));


	#if 0
							for(int i = 0;i < CLIENT_NUM; i++)
								cout <<i<< " :ci-client_fd L1 " << (int)ci[i].client_fd << endl;
	#endif
							
							memset(ci_->username, 0, USERNAME_LEN);
							memset(ci_->password, 0, PASSWD_LEN);

							if(strcmp(_cli_req.username, "PAD")==0){
								memcpy(ci_->username, _cli_req.username, _cli_req.user_name_len);
							}
							if(strcmp(_cli_req.username, "handset1")==0){
								snprintf(ci_->username, 9, 
														"handset%d", (ci_->id - 1));
							}
							memcpy(ci_->password, _cli_req.password, 6);

							bool flashHasThisName = false;
							for(int i = 0;i < CLIENT_NUM; i++){
								if(strcmp(ci_->username ,username_in_flash[i]) == 0){
									flashHasThisName = true;
									break;
								}
							}

							if(!flashHasThisName){
								if(strcmp(ci_->username, "PAD")==0){
									int fd = open("/var/phone_name1.txt", O_RDWR|O_CREAT|O_NONBLOCK);
									write(fd, "PAD", 4);
									close(fd);
								}
									// system("nvram_set freespace phone_name1 PAD");
								if(strcmp(ci_->username, "handset1")==0){
									int fd = open("/var/phone_name2.txt", O_RDWR|O_CREAT|O_NONBLOCK);
									write(fd, "handset1", 9);
									close(fd);
								}
									// system("nvram_set freespace phone_name2 handset1");
								if(strcmp(ci_->username, "handset2")==0){
									int fd = open("/var/phone_name3.txt", O_RDWR|O_CREAT|O_NONBLOCK);
									write(fd, "handset2", 9);
									close(fd);
								}
									// system("nvram_set freespace phone_name3 handset2");
								if(strcmp(ci_->username, "handset3")==0){
									int fd = open("/var/phone_name4.txt", O_RDWR|O_CREAT|O_NONBLOCK);
									write(fd, "handset3", 9);
									close(fd);
								}
									// system("nvram_set freespace phone_name4 handset3");

								for (int i=0; i<CLIENT_NUM ; i++) {										
														
									// const char *name = nvram_bufget(1, phone_name_id[i]);

									char name[10] = {0};
									char command[128] = {0};
									char name_file[64] = {0};
									// sprintf(command, "nvram_get freespace %s > /var/%s.txt",
													 // phone_name_id[i], phone_name_id[i]);


									// system(command);

									sprintf(name_file, "/var/%s.txt", phone_name_id[i]);
									int fd = open(name_file, O_RDONLY|O_NONBLOCK);
									read(fd, name, 10);
									// name[strlen(name)-1] = '\0';
									close(fd);




									if(memcmp(name, "", 1) == 0){
										username_is_null[i] = true;
									} else {
										username_is_null[i] = false;
									}
									if(!username_is_null[i])
										memcpy(username_in_flash[i], name, strlen(name)+1);

								//	cout << phone_name_id[i] << "... "  
								//		 << username_in_flash[i] << " " << endl;
									PLOG(LOG_INFO,"%s : %s",phone_name_id[i],username_in_flash[i]);
									
								}

								
							}

	#if 0
							for(int i = 0;i < CLIENT_NUM; i++)
								cout <<i<< " :ci-client_fd L2 " << (int)ci[i].client_fd << endl;
	#endif						

							
						}
					}				
				}

				pthread_mutex_unlock(&phone_ring_mutex);
			}
			break;
		case LOG_OUT:
			if(ci_->registerd){
				if(ci_->timer_setting == true){
					ci_->timer_counter ++;
				}

				CriticalSectionScoped lock(&_phone_critSect);
				for(int i=0; i<CLIENT_NUM; i++){
					if((ci[i].id+48) == _cli_req.arg[0]){
						

						snprintf(send_buf, 22, 
							"HEADR%d010LOOUTRE000\r\n", ci_->id);
						//cout << send_buf << endl;
						PLOG(LOG_INFO,"%s",send_buf);
						if(ci_->client_fd != -1)
							netWrite(ci_->client_fd, send_buf, strlen(send_buf));
						destroyClient(&ci[i]);

						break;
					}
				}
			}			
			break;
		case LOGOUTALL:
			if(ci_->registerd){

			}			
			break;
		case SWITCHTOSONMACHINE:
			if(ci_->registerd){
				if(ci_->timer_setting == true){
					ci_->timer_counter ++;
				}
				CriticalSectionScoped lock_swtoson(&_phone_critSect);
				CriticalSectionScoped lock(&_trans_critSect);
				timeout_trans = TIMEOUT_TRANS;
				for(int i=0; i<CLIENT_NUM; i++){
					if(ci_->id == ci[i].id){
						ci_transout = &ci[i];
						break;
					}
				}
				
				snprintf(send_buf, 23, 
					"HEADR%d011SWINCOM001%c\r\n", ci_->id, _cli_req.arg[0]);
				//cout << send_buf << endl;
				PDEBUG("%s",send_buf);
				
				// 48 -- 0x30 -- '0'
				for(int i=0; i<CLIENT_NUM; i++){
					if((ci[i].id + 48) == _cli_req.arg[0]){
						ci_transinto = &ci[i];
						//cout << send_buf << endl;
						PLOG(LOG_INFO,"%s",send_buf);
						if(ci[i].client_fd != -1)
							netWrite(ci[i].client_fd, send_buf, strlen(send_buf));
						stopAudio(ci_, PAYLOAD_PCMU);
						break;
					}
				}
			}
			break;
		case SWITCHOFFHOOK:
			if(ci_->registerd){
				if(ci_->timer_setting == true){
					ci_->timer_counter ++;
				}
				CriticalSectionScoped lock_swoffhook(&_phone_critSect);
				CriticalSectionScoped lock(&_trans_critSect);
				timeout_trans = timer_count_trans = 0;
				ci_transinto = ci_transout = NULL;
				snprintf(send_buf, 23, 
					"HEADR%d011SW_RMFK001%c\r\n", ci_->id, _cli_req.arg[0]);			
				for(int i=0; i<CLIENT_NUM; i++){
					if((ci[i].id + 48) == _cli_req.arg[0]){
						phone_in_use = ci_->using_phone = true;

						memset(client_ip_using, 0, sizeof(client_ip_using));
						memcpy(client_ip_using, ci_->client_ip, sizeof(ci_->client_ip));

						ci[i].using_phone = false;
						startAudio(ci_, PAYLOAD_PCMU);
						if(ci[i].client_fd != -1)
							netWrite(ci[i].client_fd, send_buf, strlen(send_buf));
						
						break;
					}
				}
				if(ci_->id + 48 == _cli_req.id && _cli_req.arg[0] == 0+48){
					phone_in_use = ci_->using_phone = true;
					memset(client_ip_using, 0, sizeof(client_ip_using));
					memcpy(client_ip_using, ci_->client_ip, sizeof(ci_->client_ip));
					startAudio(ci_, PAYLOAD_PCMU);
				}
			}
			break;
		case SWITCHONHOOK:
			if(ci_->registerd){
				if(ci_->timer_setting == true){
					ci_->timer_counter ++;
				}
				CriticalSectionScoped lock_swonhook(&_phone_critSect);
				CriticalSectionScoped lock(&_trans_critSect);
				timeout_trans = TIMEOUT_TRANS/2;
				timer_count_trans = 0;
				ci_transinto = NULL;
				snprintf(send_buf, 23, 
					"HEADR%d011SWIBACK001%c\r\n", ci_->id, _cli_req.arg[0]);			
				for(int i=0; i<CLIENT_NUM; i++){
					if((ci[i].id + 48) == _cli_req.arg[0]){
						phone_in_use = ci[i].using_phone = true;
						ci_->using_phone  = false;
						PLOG(LOG_INFO,"%s",send_buf);
						if(ci[i].client_fd != -1)
							netWrite(ci[i].client_fd, send_buf, strlen(send_buf));
						//startAudio(&ci[i],PAYLOAD_PCMU);
						break;
					}
				}	
			}
			break;
		case SWITCHBACK:
			break;
		case TALKBACKDIALING:
			if(ci_->registerd){
				if(ci_->timer_setting == true){
					ci_->timer_counter ++;
				}
				CriticalSectionScoped lock(&_phone_critSect);
				timeout_talkback = TIMEOUT_TRANS;

				for(int i=0; i<CLIENT_NUM; i++){
					if(ci_->id == ci[i].id){
						ci_talkback_out = &ci[i];
						break;
					}
				}
			
				for(int i=0; i<CLIENT_NUM; i++){
					if((ci[i].id+48) ==  _cli_req.arg[0]){
						ci_talkback_into = &ci[i];
						
						int clt_ip_len = strlen(ci[i].client_ip);
					//	HEADR1024TB_REIP01410.10.10.111\r\n

						snprintf(send_buf, 22+clt_ip_len, "HEADR%d%03dTB_REIP%03d%s\r\n", 
								ci[i].id, 12+clt_ip_len, 
								clt_ip_len, ci[i].client_ip);
						if(ci_->client_fd != -1)
							netWrite(ci_->client_fd, send_buf, strlen(send_buf));
						//cout << send_buf << endl;
						PDEBUG("%s",send_buf);

						clt_ip_len = strlen(ci_->client_ip);

						snprintf(send_buf, 22+clt_ip_len, "HEADR%d%03dTBINCOM%03d%s\r\n", 
								ci_->id, 12+clt_ip_len, 
								clt_ip_len, ci_->client_ip);
						if(ci[i].client_fd != -1)
							netWrite(ci[i].client_fd, send_buf, strlen(send_buf));
						//cout << send_buf << endl;
						PDEBUG("%s",send_buf);

						
						
						break;
					}
				}
			}
			break;
		case TALKBACKOFFHOOK:
			if(ci_->registerd){
				if(ci_->timer_setting == true){
					ci_->timer_counter ++;
				}
				CriticalSectionScoped lock(&_phone_critSect);
				if(ci_->client_fd != -1)
					netWrite(ci_->client_fd, "HEADR0010TBOF_OK000\r\n", 21);
				
				timeout_talkback = timer_count_talkback = 0;
				ci_talkback_out = ci_talkback_into = NULL;
				
				snprintf(send_buf, 23, 
					"HEADR%d011TB_RMFH001%c\r\n", ci_->id, _cli_req.arg[0]);			
				for(int i=0; i<CLIENT_NUM; i++){
					if((ci[i].id+48) ==  _cli_req.arg[0]){
						if(ci[i].client_fd != -1)
							netWrite(ci[i].client_fd, send_buf, strlen(send_buf));
						break;
					}
				}
			}

			break;
		case TALKBACKONHOOK:
			//cout << "TALKBACKONHOOK" << endl;
			if(ci_->registerd){
				if(ci_->timer_setting == true){
					ci_->timer_counter ++;
				}
				CriticalSectionScoped lock(&_phone_critSect);
				if(ci_->client_fd != -1)
					netWrite(ci_->client_fd, "HEADR0010TBON_OK000\r\n", 21);
				
				timeout_talkback = timer_count_talkback = 0;
				ci_talkback_out = ci_talkback_into = NULL;

				
				snprintf(send_buf, 23, 
					"HEADR%d011TB_RMNK001%c\r\n", ci_->id, _cli_req.arg[0]);			
				for(int i=0; i<CLIENT_NUM; i++){
				//	printf("ci.id=%d,cli_req.arg[0]=%c\n",
				//			ci[i].id,_cli_req.arg[0]);
					if((ci[i].id+48) ==  _cli_req.arg[0]){
						//cout << send_buf << endl;
						PDEBUG("%s",send_buf);
						if(ci[i].client_fd != -1)
							netWrite(ci[i].client_fd, send_buf, strlen(send_buf));
						break;
					}
				}
			}

			break;
		case INCOMING_OK:
			if(ci_->registerd){
				if(ci_->timer_setting == true){
					ci_->timer_counter ++;
				}
				CriticalSectionScoped lock(&_incom_critSect);
				ci_->has_incom_ok = true;
			}
			break;
		default:
			break;
	}
	
	return 0;
}

int PhoneProxy::parsePhoneControlServiceEvent(){
	ssize_t nread;
	char buff[MAX_NUM_LEN_] = {0};	
	nread = netReadLine(phone_proxy_fd[0], buff, MAX_NUM_LEN_, CLIENT_NUM );
	if(nread < 0){
		return (-1);
	}
	if(strcmp(buff, "RINGON\n") == 0){
//		cout << __FUNCTION__ << " " << __LINE__
//			 << " " << "RINGON" << endl;
		PLOG(LOG_INFO, "RINGON");

		_phone_control_service_event = RINGON;
	}
	if(strcmp(buff, "RINGOFF\n") == 0){
		_phone_control_service_event = RINGOFF;
	}
	if(strcmp(buff, "INCOMING\n") == 0){
//		cout << __FUNCTION__ << " " << __LINE__
//			 << " " << "incoming" << endl;
		PLOG(LOG_INFO, "INCOMING");
			 
		_phone_control_service_event = INCOMING_NUM;
	}
	if(strcmp(buff, "BUSYLINE\n") == 0){
		_phone_control_service_event = BUSYLINE;
	}
	if(strcmp(buff, "OTHER_OFFHOOK\n") == 0){
		_phone_control_service_event = OTHER_OFFHOOK;
	}


	return 0;
}		




int PhoneProxy::handlePhoneControlServiceEvent(){
	char send_buf[MAX_LEN_PCLI] = {0};
	char tele_no[MAX_NUM_LEN_] = {0};
	int ring_on = 0;
	// int 
	
	if(timer_count - ring_count > 0 )
	{
		timer_count = ring_count = 0;
		if(!phone_in_use)
			_phone_control_service_event = RINGOFF;
	} 
	switch(_phone_control_service_event){
		case DEFAULT_INCOMING:
			return (-1);
#if 0			
		case RINGON:
			incoming_call = true;
			ring_count += 5;
			for(int i=0; i<CLIENT_NUM; i++){
				if(ci[i].client_fd != -1 && ci[i].registerd){
					netWrite(ci[i].client_fd, "HEADR0010RINGON_000\r\n", 22);					
				}
			}
			phoneRingStart();
			break;
#endif			
		case RINGOFF:
		{
			already_incoming = 0;
			
			// phoneRingStop();

			// pcs->onHook();
			CriticalSectionScoped lock(&_phone_critSect);
			for(int i=0; i<CLIENT_NUM; i++){
				ci[i].has_incom_ok = false;
				if(ci[i].client_fd != -1 && ci[i].registerd){
					
					netWrite(ci[i].client_fd, "HEADR0010RINGOFF000\r\n", 21);					
				}
			}
			usleep(50000);
			for(int i=0; i<CLIENT_NUM; i++){
				if(ci[i].client_fd != -1 && ci[i].registerd){
					netWrite(ci[i].client_fd, "HEADR0010RINGOFF000\r\n", 21);					
				}
			}
			PLOG(LOG_INFO, "RINGOFF");	
			// netWrite(phone_proxy_fd[0], "RINGOFF\n", 8);


			
		}
			break;
		case RINGON:
		{
			
			ring_on = 1;
			if(ring_should_finish){
				PLOG(LOG_INFO, "ring_should_finish");
				usleep(3000000);
				ring_should_finish = false;
				break;
			}
			ring_should_finish = false;
			incoming_call = true;
			
			int tele_no_len;
			tele_no_len = 0;
			pcs->getIncomingNumber(tele_no, &tele_no_len);
			
			 
			PLOG(LOG_INFO,"Incoming tel no in ring: %s, len=%d",tele_no,tele_no_len);
			if(tele_no_len > 0)
				snprintf(send_buf, 22+tele_no_len, "HEADR0%03dINCOMIN%03d%s\r\n",
							 10+tele_no_len, tele_no_len, tele_no);
			else
				snprintf(send_buf, 22, "HEADR0010INCOMIN000\r\n");
				
			memset(incoming_response,0,sizeof(incoming_response));
			strcpy(incoming_response, send_buf);

			// if(already_incoming == 0){

				CriticalSectionScoped lock(&_phone_critSect);

				for(int i=0; i<CLIENT_NUM; i++){
					if(ci[i].client_fd != -1 && ci[i].registerd){

						// if(!(ci[i].has_incom_ok)){

							if(!already_incoming)
								netWrite(ci[i].client_fd, "HEADR0010RINGOFF000\r\n", 21);
							// usleep(500000);
							

							
							// if((ci[i].id != 1 ) || (ci[i].id == 1 &&  !already_incoming )){
								netWrite(ci[i].client_fd, send_buf, strlen(send_buf));
								PLOG(LOG_INFO, "Ringing sent to %s@%s", 
										ci[i].username, ci[i].client_ip);
							// }
						// }
						//ci[i].timeout_incoming = TIMEOUT_INCOMING;
						//ci[i].has_incom_ok = false;
						//setClientTimer(&ci[i],TIMEOUT_INCOMING,resendIncoming,&ci[i],this,TIMER_INCOMING);
					}
				}

			// }
			if(already_incoming == 0)
				already_incoming = 1;

			ring_count += 5;
			
			// phoneRingStart();
		}
			break;

			
		case INCOMING_NUM:
		{

			if(ring_should_finish){
				PLOG(LOG_INFO, "ring_should_finish");
				usleep(3000000);
				ring_should_finish = false;
				break;
			}
			ring_should_finish = false;
			incoming_call = true;
			
			int tele_no_len;
			tele_no_len = 0;
			pcs->getIncomingNumber(tele_no, &tele_no_len);
//			cout << __FUNCTION__ << " " << __LINE__
//			 << " " << "incoming no : " << tele_no << endl;
			PLOG(LOG_INFO,"Incoming tel no : %s, len=%d",tele_no,tele_no_len);
			if(tele_no_len > 0)
				snprintf(send_buf, 22+tele_no_len, "HEADR0%03dINCOMIN%03d%s\r\n",
							 10+tele_no_len, tele_no_len, tele_no);
			else
				snprintf(send_buf, 22, "HEADR0010INCOMIN000\r\n");
				
			memset(incoming_response,0,sizeof(incoming_response));
			strcpy(incoming_response, send_buf);

			
			already_incoming = 1;
			
			CriticalSectionScoped lock(&_phone_critSect);
			for(int i=0; i<CLIENT_NUM; i++){
				if(ci[i].client_fd != -1 && ci[i].registerd){
					// if(!(ci[i].has_incom_ok)){

						netWrite(ci[i].client_fd, "HEADR0010RINGOFF000\r\n", 21);
						// usleep(500000);						

						PLOG(LOG_INFO, "Incoming sent to %s@%s", 
							ci[i].username, ci[i].client_ip);
						netWrite(ci[i].client_fd, send_buf, strlen(send_buf));
					// }
					//ci[i].timeout_incoming = TIMEOUT_INCOMING;
					//ci[i].has_incom_ok = false;
					//setClientTimer(&ci[i],TIMEOUT_INCOMING,resendIncoming,&ci[i],this,TIMER_INCOMING);
				}
			}		
				
			ring_count = 5;				
			
			// phoneRingStart();
		}
			break;	
		case BUSYLINE:
			for(int i=0; i<CLIENT_NUM; i++){
				CriticalSectionScoped lock(&_phone_critSect);
				if(ci[i].client_fd != -1 && ci[i].registerd){
					netWrite(ci[i].client_fd, "HEADR0010LINBUSY000\r\n", 21);					
				}
			}			
			break;
		case OTHER_OFFHOOK:
			if(incoming_call){
				incoming_call = false;
				// phoneRingStop();
				netWrite(phone_proxy_fd[0], "RINGOFF\n", 8);

				for(int i=0; i<CLIENT_NUM; i++){
					CriticalSectionScoped lock(&_phone_critSect);
					if(ci[i].client_fd != -1 && ci[i].registerd){
						netWrite(ci[i].client_fd, "HEADR0010RINGOFF000\r\n", 21);					
					}
				}
			}
			break;
		default:
			break;
	}

	_phone_control_service_event = DEFAULT_INCOMING;
	return 0;
}





int32_t PhoneProxy::startPhoneProxy(){
    if (_phoneProxyRunning) {
        return 0;
    }

    _phoneProxyRunning = true;

	// PhoneControlServiceRunning
    const char* threadName = "handaer_phone_proxy_thread";
    _ptrThreadPhoneProxy = ThreadWrapper::CreateThread(phoneProxyThreadFunc,
                                                this,
                                                kRealtimePriority,
                                                threadName);
    if (_ptrThreadPhoneProxy == NULL) {
        cout << "failed to create the phone proxy thread" << endl;
        _phoneProxyRunning = false;
        return -1;
    }

    unsigned int threadID(0);
    if (!_ptrThreadPhoneProxy->Start(threadID)) {
        cout << "failed to start the phone proxy thread" << endl;
        _phoneProxyRunning = false;
        delete _ptrThreadPhoneProxy;
        _ptrThreadPhoneProxy = NULL;
        return -1;
    }
    _phoneProxyThreadId = threadID;


    return 0;	

}
int32_t PhoneProxy::stopPhoneProxy(){
	_phoneProxyRunning = false;

    if ( _ptrThreadPhoneProxy
			&& !_ptrThreadPhoneProxy->Stop() ) {
        cout << "failed to stop the phone proxy thread" << endl;
        return -1;
    } else {
        delete _ptrThreadPhoneProxy;
        _ptrThreadPhoneProxy = NULL;
    }
	
    return 0;

}
bool PhoneProxy::phoneProxyisRunning() const{
	return (_phoneProxyRunning);

}

int32_t PhoneProxy::startHeartBeating(){
    if (_heartBeatingisRunning) {
        return 0;
    }

    _heartBeatingisRunning = true;

	// PhoneControlServiceRunning
    const char* threadName = "handaer_heart_beating_thread";
    _ptrThreadHeartBeating = ThreadWrapper::CreateThread(heartBeatingThreadFunc,
                                                this,
                                                kRealtimePriority,
                                                threadName);
    if (_ptrThreadHeartBeating == NULL) {
        cout << "failed to create the heart beating thread" << endl;
        _heartBeatingisRunning = false;
        return -1;
    }

    unsigned int threadID(0);
    if (!_ptrThreadHeartBeating->Start(threadID)) {
        cout << "failed to start the heart beating thread" << endl;
        _heartBeatingisRunning = false;
        delete _ptrThreadHeartBeating;
        _ptrThreadHeartBeating = NULL;
        return -1;
    }
    _heartBeatingThreadId = threadID;


    return 0;	

}
int32_t PhoneProxy::stopHeartBeating(){
	_heartBeatingisRunning = false;

    if ( _ptrThreadHeartBeating
			&& !_ptrThreadHeartBeating->Stop() ) {
        cout << "failed to stop the heart beating thread" << endl;
        return -1;
    } else {
        delete _ptrThreadHeartBeating;
        _ptrThreadHeartBeating = NULL;
    }
	
    return 0;
}
bool PhoneProxy::heartBeatingisRunning() const{
	return (_heartBeatingisRunning);

}




bool PhoneProxy::phoneProxyThreadFunc(void* pThis){
	return (static_cast<PhoneProxy*>(pThis)->phoneProxyThreadProcess());

}

bool PhoneProxy::resetFdsetAndTimeout(){

	


	return true;
}

int updateMaxfd(fd_set fds, int maxfd) {
    int i;
    int new_maxfd = 0;
    for (i = 0; i <= maxfd; i++) {
        if (FD_ISSET(i, &fds) && i > new_maxfd) {
            new_maxfd = i;
        }
    }
    return new_maxfd;
}



bool PhoneProxy::phoneProxyThreadProcess(){

	
	//cout << __FUNCTION__ << " " << __LINE__ << endl;
	// resetFdsetAndTimeout();
	
	fdR  = fdRall ;
	
	maxfd = updateMaxfd(fdR, maxfd);

			
		// FD_SET(ci[i].client_fd, &fdR);
		
	
	
	tv.tv_sec = 0;
	tv.tv_usec = EPOLL_TIMEOUT_MS * 1000;
	
	switch( select(maxfd+1, &fdR, NULL, NULL, &tv) ) {
	case -1:
		if (errno == EINTR) {
			PLOG(LOG_ERROR,"EINTR");
			perror("select in phoneproxy EINTR Error: ");
			break;
		} else if (errno == EBADF) {
			PLOG(LOG_ERROR,"EBADF");
			perror("select in phoneproxy EBADF Error: ");
			break;
		} else {
			perror("select in phoneproxy error: ");			
			break;
		}		
	case 0:	//timeout			
		break;
	default:
		if ( FD_ISSET(serv_listen_sockfd, &fdR) ) {

			struct sockaddr_in clt_addr;
			socklen_t clt_len;
			int clt_sockfd;	
				
			clt_len = sizeof(clt_addr);
			clt_sockfd = my_accept((struct sockaddr *)&clt_addr, &clt_len);
			if(clt_sockfd == -EINTR || clt_sockfd == -EAGAIN || clt_sockfd == -1){
				break;
			}
			char *new_ip = NULL;
			struct in_addr ia = clt_addr.sin_addr; //(int)ip
			new_ip = inet_ntoa(ia);

			for(int i=0; i<CLIENT_NUM ; i++){
				if(strcmp(new_ip, ci[i].client_ip) == 0){
					// close(ci[i].client_fd);
					// FD_CLR(ci[i].client_fd, &fdRall);
					destroyClient(&ci[i]);
					break;
				}
			}


			
			for(int i=0; i<CLIENT_NUM ; i++){
				
				if(ci[i].client_fd == -1){
					ci[i].client_fd = clt_sockfd;

					FD_SET(ci[i].client_fd, &fdRall);
					maxfd = max(maxfd, ci[i].client_fd);


					//cout << "accept" << i << " " << ci[i].client_fd << endl;
					memset(ci[i].client_ip, 0, sizeof(ci[i].client_ip));
					memcpy(ci[i].client_ip, new_ip, strlen(new_ip)+1);
	//				cout << " f: " << __FUNCTION__
	//					 << " l: " << __LINE__
	//					 << " client_ip = " << ci[i].client_ip
	//					 << " client_num = " << i << endl;
					PLOG(LOG_INFO,"ip = %s client_num = %d",ci[i].client_ip,i);

					if(ci[i].timer_setting == false){
						ci[i].timer_setting = true;				
						ci[i].old_timer_counter = ci[i].timer_counter = 0;
						ci[i].exit_threshold = 0;
						setClientTimer(&ci[i], OPTION_INTERVAL, recycleClient, &ci[i], this, TIMER_HEARTBEAT);
					}
					
	#if 1
					int ret;	
					ret = fcntl(clt_sockfd, F_GETFL, 0);  
					fcntl(clt_sockfd, F_SETFL, ret | O_NONBLOCK);
					
					// int on=1;	 
					// setsockopt(clt_sockfd, IPPROTO_TCP, TCP_NODELAY,&on,sizeof(on));

					// struct linger {
					// 	int l_onoff;
					// 	int l_linger;
					// };
					// struct linger m_sLinger;
					// m_sLinger.l_onoff=0;
					// m_sLinger.l_linger = 1;   
					// setsockopt(clt_sockfd,SOL_SOCKET,SO_LINGER,&m_sLinger,sizeof(m_sLinger)); 

					
	#endif
	#if 0
					struct epoll_event ev;
					ev.events = EPOLLIN;
					ev.data.ptr = &ci[i];
					if(epoll_ctl(epollfd, EPOLL_CTL_ADD, clt_sockfd, &ev) == -1){
						perror("epoll_ctl : client_fd ");
						exit (-1);
					}
	#endif

					
					break;
					
				}
			}	
			for(int i=0; i<CLIENT_NUM; i++){
				//cout << i << " " << ci[i].client_fd << endl;
				PLOG(LOG_INFO, "%d:cli_fd=%d", i ,ci[i].client_fd);
			}
			




		} 

		for(int i=0; i<CLIENT_NUM ; i++){
			if(ci[i].client_fd == -1)
				continue;
			if ( FD_ISSET(ci[i].client_fd, &fdR) ) {
				//cout << "client request in phoneproxy" << endl;
				PDEBUG("client request in phoneproxy");
				// CriticalSectionScoped lock(&_going_critSect);

				PLOG(LOG_INFO, "%s:%d@%s:%d:i=%d", ci[i].username, 
					ci[i].id, ci[i].client_ip, ci[i].client_fd,i);

				parseClient(&ci[i], i); 
				handleClient(&ci[i]);				
			}
		}

		if ( FD_ISSET(phone_proxy_fd[0], &fdR) ) {
			//cout << "serial event in phoneproxy" << endl;
			PDEBUG("serial event in phoneproxy");
			parsePhoneControlServiceEvent();
			handlePhoneControlServiceEvent();
		} 
		
		break;
	}//end switch (select)

same_client:


#if	0
	int nfds;
	struct epoll_event events[MAX_EVENTS];
	nfds = epoll_wait(epollfd, events, MAX_EVENTS, EPOLL_TIMEOUT_MS);
	if(nfds == -1){
		perror("epoll_wait : ");
		exit(-1);
	}
	if(nfds > 0)
		cout << " nfds : " << nfds << endl;
	for(int n=0; n<nfds; ++n){
		if(events[n].data.fd == phone_proxy_fd[0]){
			parsePhoneControlServiceEvent();
			handlePhoneControlServiceEvent();
		} else {
			parseClient((cli_info_t *) events[n].data.ptr); 
			handleClient((cli_info_t *) events[n].data.ptr);
		}
	}
#endif

	if(ring_count > 0)
		handlePhoneControlServiceEvent();


	if( timeout_trans == TIMEOUT_TRANS
			&& timer_count_trans >= TIMEOUT_TRANS/2 ){
		CriticalSectionScoped lock_trans1(&_phone_critSect);
		CriticalSectionScoped lock(&_trans_critSect);
		if(ci_transout)
			ci_transout->using_phone =  true;
		phone_in_use = true;
		memset(client_ip_using, 0, sizeof(client_ip_using));
		memcpy(client_ip_using, ci_transout->client_ip, sizeof(ci_transout->client_ip));
		if(ci_transinto)
			ci_transinto->using_phone = false;
		timeout_trans = TIMEOUT_TRANS/2;
		timer_count_trans = 0;
		char send_buf[32] = {0};
		if(ci_transout && ci_transinto)
			snprintf(send_buf, 23, "HEADR%d011SWIBACK001%d\r\n",
					 (ci_transinto->id == (char)-1)?0:ci_transinto->id, 
					 (ci_transout->id == (char)-1)?0:ci_transout->id);
		PLOG(LOG_INFO,"%s",send_buf);

		
		if(ci_transout && (ci_transout->client_fd != -1))
			netWrite(ci_transout->client_fd, send_buf, strlen(send_buf));
		if(ci_transinto && (ci_transinto->client_fd != -1))
			netWrite(ci_transinto->client_fd, "HEADR0010RINGOFF000\r\n", 22);
		ci_transinto = NULL;
	}
	if(timeout_trans == TIMEOUT_TRANS/2 
		&& timer_count_trans >= TIMEOUT_TRANS/2){
		CriticalSectionScoped lock_trans2(&_phone_critSect);
		CriticalSectionScoped lock(&_trans_critSect);
		timer_count_trans = timeout_trans = 0;
		if(ci_transout)
			 ci_transout->using_phone = false;
		phone_in_use = false;
		memset(client_ip_using, 0, sizeof(client_ip_using));

		//netWrite(phone_proxy_fd[0], "ONHOOK\n", 7);
		pcs->onHook();
		
		if(ci_transout && (ci_transout->client_fd != -1))
			netWrite(ci_transout->client_fd, "HEADR0010RINGOFF000\r\n", 22);
		ci_transout = NULL;
	}

	if(timeout_talkback > 0 
		&& timer_count_talkback >= timeout_talkback){
		timeout_talkback = timer_count_talkback = 0;
		char send_buf[32] = {0};
		if(ci_talkback_into)
			snprintf(send_buf, 22, 
				"HEADR%d010TB_UNOF000\r\n", (ci_talkback_into->id == (char)-1)?0:ci_talkback_into->id);

		CriticalSectionScoped lock(&_phone_critSect);
		if(ci_talkback_out && (ci_talkback_out->client_fd != -1))
			netWrite(ci_talkback_out->client_fd, send_buf, strlen(send_buf));
		memset(send_buf, 0, sizeof(send_buf));
		if(ci_talkback_out)
			snprintf(send_buf, 22, 
				"HEADR%d010TB_ROFF000\r\n", (ci_talkback_out->id == (char)-1)?0:ci_talkback_out->id);
		if(ci_talkback_into && (ci_talkback_into->client_fd != -1))
			netWrite(ci_talkback_into->client_fd, send_buf, strlen(send_buf));

		ci_talkback_out = ci_talkback_into = NULL;
		

	}

	
	return true;
}

bool PhoneProxy::heartBeatingThreadFunc(void* pThis){
	return (static_cast<PhoneProxy*>(pThis)->heartBeatingThreadProcess());

}

static void xxx(int x){
	if(ring_count > 0)
		timer_count ++;	
	if(timeout_trans > 0){
		CriticalSectionScoped lock(&_trans_critSect);
		timer_count_trans ++;
	}
	if(timeout_talkback > 0){
		timer_count_talkback ++;
	}

	
	//cout << "total_receive: " << total_receive << endl;
	PDEBUG("audio data recv: %d", total_receive);
	total_receive = 0;
}




void PhoneProxy::notify( int signum ){
	xxx(0);
	assert( signum == SIGALRM );
	int i;
	for(i = 0; i < CLIENT_NUM*2; i++) {
		if(timer_info_g[i].in_use == 0) {
			continue;
		}
        timer_info_g[i].elapse ++;
		if(timer_info_g[i].elapse == 
		   			timer_info_g[i].interval) {
            timer_info_g[i].elapse = 0;
            timer_info_g[i].timer_func_cb(timer_info_g[i].timer_func_cb_data,
				        timer_info_g[i].this_obj);
		}
	}
	
	return;
}



bool PhoneProxy::heartBeatingThreadProcess(){
	//cout << __FUNCTION__ << " " << __LINE__ << endl;

#if 1	
	timer->setTimer(TIMER_SEC, 0);
	timer->setHandler(notify);    
//	timer->setHandler(xxx); 
	timer->reset();
#endif	
	return false;
}

void PhoneProxy::get_current_format_time(char * tstr) {
	time_t t;
	t = time(NULL);
	strcpy(tstr, ctime(&t));
	tstr[strlen(tstr)-1] = '\0'; // replace '\n' with '\0'
	return;
}



int PhoneProxy::destroyClient(cli_info_t *pci){

	CriticalSectionScoped lock(&_phone_critSect);
	
//	cout << __FUNCTION__ << " " << __LINE__ << endl;
	if(pci->timer_setting){
		deleteClientTimer(pci, TIMER_HEARTBEAT);
		pci->timer_setting = false;
	}
#if 0
	//int epollfd_tmp = getEpollFd();
	if(epoll_ctl(epollfd, EPOLL_CTL_DEL, pci->client_fd, NULL)) {
		perror("epoll_ctl_del : ");
		return (-1);
	}
#endif
	if(pci->using_phone){
		pci->using_phone = phone_in_use = false;
		memset(client_ip_using, 0, sizeof(client_ip_using));
		//netWrite(phone_proxy_fd[0], "ONHOOK\n", 7);
		pcs->onHook();
		stopAudio(pci, 0);
	}
	PLOG(LOG_INFO,"before close(pci->client_fd)");
	//netWrite(pci->client_fd, "You will be not online\n", 23);
	close(pci->client_fd);
	FD_CLR(pci->client_fd, &fdRall);

	PLOG(LOG_INFO,"after close(pci->client_fd)");

	char send_buf[32] = {0};
	snprintf(send_buf, 22, 
		"HEADR%d010OFFLINE000\r\n", ((pci->id==-1)?0:pci->id));			
	for(int i=0; i<CLIENT_NUM; i++){
		if(ci[i].id  == pci->id){
			PLOG(LOG_INFO,"in continue: %s",ci[i].username);
			continue;
		}
		if(ci[i].client_fd != -1){
			PLOG(LOG_INFO,"before offline: %s",ci[i].username);
			
			netWrite(ci[i].client_fd, send_buf, strlen(send_buf));
			PLOG(LOG_INFO,"after offline: %s",ci[i].username);
		}
	}
	
	memset(pci, 0, sizeof(cli_info_t));
	pci->client_fd = -1;
	pci->id = -1;
	pci->registerd = false;

#if 0
	for(int i=0; i<CLIENT_NUM; i++){
		cout << i << " " << ci[i].client_fd << endl;
	}
#endif
	pthread_mutex_unlock(&phone_ring_mutex);
	return 0;
}


// arg --> cli_info_t *
int PhoneProxy::recycleClient(void *arg, void *pThis){
	if(arg == NULL){
		return -1;
	}
	PhoneProxy *ppx = static_cast<PhoneProxy*>(pThis);
	cli_info_t * cli_info_ = (cli_info_t *)arg;
//	cout << "client_fd " << cli_info_->client_fd << endl;

	char tstr[200] = {0};
	get_current_format_time(tstr);
//	cout << __FUNCTION__ << " " << tstr << endl;
//	cout << "cli_info_->old_timer_counter " << cli_info_->old_timer_counter
//	 << endl << "cli_info_->timer_counter " << cli_info_->timer_counter
//	 << endl << "cli_info_->exit_threshold " << cli_info_->exit_threshold 
//	 << endl;
	if(cli_info_->old_timer_counter == cli_info_->timer_counter){
		cli_info_->exit_threshold ++;
	}else{
		cli_info_->exit_threshold  = 0;
		cli_info_->old_timer_counter = cli_info_->timer_counter;
	}
	if(cli_info_->exit_threshold > 3){
		PLOG(LOG_INFO,"%s",tstr);
		ppx->destroyClient(cli_info_);		
	}
	return 0;
}

int PhoneProxy::resendIncoming(void *arg, void *pThis){
	if(arg == NULL){
		return -1;
	}
	PhoneProxy *ppx = static_cast<PhoneProxy*>(pThis);
	cli_info_t * cli_info_ = (cli_info_t *)arg;
	CriticalSectionScoped lock_resend(&_phone_critSect);
	CriticalSectionScoped lock(&_incom_critSect);
	if(!cli_info_->has_incom_ok  && (cli_info_->client_fd != -1))
		ppx->netWrite(cli_info_->client_fd,ppx->incoming_response,strlen(ppx->incoming_response));
	ppx->deleteClientTimer(cli_info_, TIMER_INCOMING);
	cli_info_->has_incom_ok = false;
	return 0;
}

int PhoneProxy::setClientTimer(cli_info_t* cli_info, int interval, 
			timer_process_cbfn_t timer_proc, void *arg, void* pThis,
			TimerTypeEnum _timer_type) {
	if (timer_proc == NULL || interval <= 0) {
		return(-1);
	}
	
	int i;
	for(i=0 ; i<CLIENT_NUM*2; i++){
		if(timer_info_g[i].in_use == 1) {
			continue;
		}

		memset(&timer_info_g[i], 0, sizeof(timer_info_g[i]));

		timer_info_g[i].timer_func_cb = timer_proc;
		if(arg != NULL) {			
	        timer_info_g[i].timer_func_cb_data = arg;
		}
		timer_info_g[i].this_obj = pThis;
	    timer_info_g[i].interval = interval;
	    timer_info_g[i].elapse = 0;
	    timer_info_g[i].in_use = 1;
	
		cli_info->ti[_timer_type] = &timer_info_g[i];
		break;
	}
	
	if(i >= CLIENT_NUM*2) {
		return(-1);
	}
	return 0;
}



int PhoneProxy::deleteClientTimer(cli_info_t* cli_info, TimerTypeEnum _timer_type) {
	//cout << __FUNCTION__ << " " << __LINE__ << endl;
	PDEBUG("deleteClientTimer");

	if(cli_info->ti[_timer_type] == NULL){
		return -1;
	}
	
	memset(cli_info->ti[_timer_type], 0, sizeof(timer_info_t));
	cli_info->ti[_timer_type] = NULL;
	
	return(0);
}

bool PhoneProxy::resetTimerInfo(){
	memset(timer_info_g, 0, sizeof(timer_info_g));
	return true;
}


bool PhoneProxy::phoneProxyInit(int port){
#if 0
	epollfd = epoll_create(MAX_EVENTS);
	if(epollfd == -1){
		perror("epoll create: ");
		exit (-1);
	}
#endif
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, phone_proxy_fd) < 0) {   
	    perror("socketpair: ");   
	    exit (-1);   
    }

    int ret;	
	ret = fcntl(phone_proxy_fd[0], F_GETFL, 0);  
	fcntl(phone_proxy_fd[0], F_SETFL, ret | O_NONBLOCK);


	
	PhoneControlService::setPhoneProxyFd(phone_proxy_fd[1]);
	pcs = new PhoneControlService();

#if 0
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = phone_proxy_fd[0];
	if(epoll_ctl(epollfd, EPOLL_CTL_ADD, phone_proxy_fd[0], &ev) == -1){
		// perror("epoll_ctl : phone_proxy_fd[0] ");
		exit (-1);
	}
#endif
	listenOnServerSocket(port);

	FD_ZERO(&fdR);
	FD_ZERO(&fdRall);

	FD_SET(phone_proxy_fd[0], &fdRall);
	FD_SET(serv_listen_sockfd, &fdRall);
	
	maxfd = max(phone_proxy_fd[0], serv_listen_sockfd);







	resetTimerInfo();
	timer = PhoneTimerWrapper::CreatePhoneTimerWrapper();
	startHeartBeating();
	startPhoneProxy();

#if 0
	sigset_t signal_set;
	sigemptyset(&signal_set);
	sigaddset(&signal_set, SIGALRM);
	int status = pthread_sigmask(SIG_BLOCK, &signal_set, NULL);
	if(status != 0){
		perror("set signal mask : ");
	}
#endif
	//netWrite(phone_proxy_fd[0], "ONHOOK\n", 7);
	pcs->onHook();

	cli_info_t cii;
	memset(&cii, 0, sizeof(cli_info_t));
	memcpy(cii.client_ip, "0.0.0.0", 8);

	// startAudio(&cii, 0);
	// sleep(2);
	// stopAudio(&cii, 0);



	struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigaction( SIGPIPE, &sa, 0 );

	pthread_mutex_init(&phone_ring_mutex, NULL);

	return true;
}


bool PhoneProxy::phoneProxyExit(){
	stopPhoneProxy();
	stopHeartBeating();

	timer->cancel();
	if(timer != NULL){
		delete timer;
		timer = NULL;
	}
	resetTimerInfo();

	close(serv_listen_sockfd);
#if 0
	close(epollfd);
#endif
	if(pcs != NULL){
		delete pcs;
		pcs = NULL;
	}

	return true;
}





}

