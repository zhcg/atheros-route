#include "register.h"
#include "sqlite3.h"

int register_fd = -1;
client_t client_list[4] = {0};
const char hex_to_asc_table[16] = {0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
	0x38,0x39,0x41,0x42,0x43,0x44,0x45,0x46};

int ComFunChangeHexBufferToAsc(unsigned char *hexbuf,int hexlen,char *ascstr)
{
	unsigned char i;
	unsigned char ch,h,l;
	char *pdes = ascstr;
	for(i = 0;i < hexlen;i++)
	{
		ch = hexbuf[i];
		h = (ch >> 4) & 0x0F;
		l = ch & 0x0F;
		*(pdes++) = hex_to_asc_table[h];
		*(pdes++) = hex_to_asc_table[l];
	}
	*pdes = '\0';
	return hexlen * 2;
}
//输出一个数组的内容，以十六进制模式
void ComFunPrintfBuffer(unsigned char *pbuffer,unsigned char len)
{
	char *pstr;
	pstr = (char *)malloc(512);
	if(pstr == NULL)
		return;
	ComFunChangeHexBufferToAsc(pbuffer,len,pstr);

	printf("%s\r\n",pstr);

	free((void *)pstr);
}

int check_dir()
{
	struct stat st;
	int ret = 0;
	ret = stat("/configure_backup/terminal_dev_register/db/",&st);
	if(ret < 0)
	{
		PRINT("no db dir\n");
		system("mkdir -p /configure_backup/terminal_dev_register/db/");
	}
	else
		PRINT("found db dir\n");
	return 0;
}

int reset_db(sqlite3 *db)
{
    char reset_sql_cmd[1024] = {0};
    char reset_sql_cmd2[1024] = {0};
    char reset_sql_cmd3[1024] = {0};
    char table_data[1024] =
       "base_sn varchar2(34) default \"\",\
base_mac varchar2(12) default \"\",\
base_ip varchar2(15) default \"\",\
base_user_name varchar2(50) default \"\",\
base_password varchar2(30) default \"\",\
pad_sn varchar2(34) default \"\",\
pad_mac varchar2(12) default \"\",\
pad_ip varchar2(15) default \"\",\
pad_user_name varchar2(50) default \"\",\
pad_password varchar2(30) default \"\",\
sip_ip varchar2(15) default \"\",\
sip_port varchar2(10) default \"\",\
heart_beat_cycle varchar2(10) default \"\",\
business_cycle varchar2(10) default \"\",\
ssid_user_name varchar2(50) default \"\",\
ssid_password varchar2(30) default \"\",\
device_token varchar2(100) default \"\",\
position_token varchar2(100) default \"\",\
default_ssid varchar2(50) default \"\",\
default_ssid_password varchar2(30) default \"\",\
register_state varchar2(5) default \"251\",\
authentication_state varchar2(5) default \"240\"";
    char table_data2[1024] =
"device_name varchar2(30) default \"\",\
device_id varchar2(10) default \"\",\
device_mac varchar2(15) default \"\",\
device_code varchar2(5) default \"\"";
    char table_data3[1024] =
"_id integer default \"\",\
password varchar2(30) default \"\"";

	char insert_default_cmd[512] = {0};
	sprintf(insert_default_cmd,"insert into %s (register_state,authentication_state) values (\"251\",\"240\");",TB);
    sprintf(reset_sql_cmd,"create table %s(%s)",TB,table_data);
    PRINT("reset_sql_cmd = %s\n",reset_sql_cmd);
    sprintf(reset_sql_cmd2,"create table %s(%s)",REGISTERTB,table_data2);
    PRINT("reset_sql_cmd2 = %s\n",reset_sql_cmd2);
    sprintf(reset_sql_cmd3,"create table %s(%s)",PASSWORD,table_data3);
    PRINT("reset_sql_cmd3 = %s\n",reset_sql_cmd2);
    PRINT("insert_default_cmd = %s\n",insert_default_cmd);
	if(sqlite3_exec(db,reset_sql_cmd,NULL,NULL,NULL) == SQLITE_OK)
		sqlite3_exec(db,insert_default_cmd,NULL,NULL,NULL);
	sqlite3_exec(db,reset_sql_cmd2,NULL,NULL,NULL);
	sqlite3_exec(db,reset_sql_cmd3,NULL,NULL,NULL);
	return 0;
}

int sqlite3_select(char *tb_name,char *data_name, char *data_value,char *where_name,char *out_value)
{
    int i = 0;
    int index = 0;
    char **result_buf; //是 char ** 类型，两个*号
    int row_count = 0, column_count = 0;
    sqlite3 *db;
    char *err_msg;
    char sql[128] = {0};
        
    if ((data_name == NULL) || (out_value == NULL))
    {
        PRINT("para is NULL!\n");
        return -1;
    }
    check_dir();
    if (sqlite3_open("/configure_backup/terminal_dev_register/db/terminal_base_db", &db) != 0)
    {
        PRINT("%s\n",sqlite3_errmsg(db));
        return -2;
    }
    //select age from data where age="15"
    strcpy(sql, "select ");
    strncat(sql, data_name, strlen(data_name));
    strcat(sql, " from ");
    strcat(sql, tb_name);
    if(where_name != NULL && data_value != NULL)
    {
		strcat(sql, " where ");
		strncat(sql, where_name, strlen(where_name));
		strcat(sql, "=\"");
		strncat(sql, data_value, strlen(data_value));
		strcat(sql, "\" ");
		strcat(sql," collate NOCASE;");
	}
    PRINT("sql:%s\n",sql);
    if(sqlite3_get_table(db, sql, &result_buf, &row_count, &column_count, &err_msg) != 0)
    {
		reset_db(db);
		if(sqlite3_get_table(db, sql, &result_buf, &row_count, &column_count, &err_msg) != 0)
		{
			PRINT("%s\n", err_msg);
			sqlite3_free_table(result_buf);
			return -3;
		}
    }
    
    index = column_count;
    for (i = 0; i < column_count; i++)
    {
        if (strcmp(result_buf[index], "") == 0)
        {
            PRINT("no data\n");
            return -4;    
        }
        memcpy(out_value, result_buf[index], strlen(result_buf[index]));
        ++index;
    }
    
    sqlite3_free_table(result_buf);
    sqlite3_close(db);
    return 0;
}

int sqlite3_insert(int columns_count,char *tb_name, char (*columns_name)[30], char (*columns_value)[100])
{
    int i = 0;
    sqlite3 *db;
    char *err_msg;
    char sql[1024] = {0};
    unsigned long value = 0;
    if (columns_count == 0)
    {
        return -1;
    }
    
    if ((columns_name == NULL) || (columns_value == NULL))
    {
        PRINT("para is NULL!\n");
        return -2;
    }
    
    if (sqlite3_open("/configure_backup/terminal_dev_register/db/terminal_base_db", &db) != 0)
    {
        PRINT("open err %s\n", sqlite3_errmsg(db));
        return -3;
    }
    
    sprintf(sql, "insert into %s (", tb_name);
    for (i = 0; i < columns_count; i++)
    {
		PRINT("columns_name = %s\n",columns_name[i]);
        strncat(sql, columns_name[i], strlen(columns_name[i]));
        if ((columns_count > 1) && (i != (columns_count - 1)))
        {
            strcat(sql, ",");
        } 
    }
    strcat(sql, ") values (");
    for (i = 0; i < columns_count; i++)
    {
        strcat(sql, "\"");
        strncat(sql, columns_value[i], strlen(columns_value[i]));
        strcat(sql, "\"");   
        
        if ((columns_count > 1) && (i != (columns_count - 1)))
        {
            strcat(sql, ",");
        }  
    }
    strcat(sql, ")");
    PRINT("%s\n", sql);
    if (sqlite3_exec(db, sql, NULL, NULL, &err_msg) != 0)
    {
        PRINT("exec error\n");
        sqlite3_close(db);
        return -4;
    }
    sqlite3_close(db);
    return 0;
}

int sqlite3_delete_row(unsigned char columns_count,char *tb_name, char (*columns_name)[30], char (*columns_value)[100])
{
    int i = 0;
    sqlite3 *db;
    char *err_msg;
    char sql[1024] = {0};
    unsigned long value = 0;
    if (columns_count == 0)
    {
        return -1;
    }
    
    if ((columns_name == NULL) || (columns_value == NULL))
    {
        PRINT("para is NULL!\n");
        return -2;
    }
    
    if (sqlite3_open("/configure_backup/terminal_dev_register/db/terminal_base_db", &db) != 0)
    {
        PRINT("open err%s\n", sqlite3_errmsg(db));
        return -3;
    }
    
    sprintf(sql, "delete from %s where ", tb_name);
    for (i = 0; i < columns_count; i++)
    {
        strncat(sql, columns_name[i], strlen(columns_name[i]));
        
        strcat(sql, "=\"");
        strncat(sql, columns_value[i], strlen(columns_value[i]));
        strcat(sql, "\"");    
        
        if ((columns_count > 1) && (i != (columns_count - 1)))
        {
            strcat(sql, " and ");
        }  
    }
    PRINT("%s\n", sql);
    if (sqlite3_exec(db, sql, NULL, NULL, &err_msg) != 0)
    {
        PRINT("exec err%s\n", err_msg);
        sqlite3_close(db);
        return -4;
    }
    sqlite3_close(db);
    return 0;
}

int sqlite3_update(char *tb_name,unsigned char columns_count, char (*columns_name)[30], char (*columns_value)[100],char *where_name,char *where_value)
{
    int i = 0;
    sqlite3 *db;
    char *err_msg;
    char sql[1024] = {0};
    unsigned long value = 0;
    if (columns_count == 0)
    {
        return -1;
    }
    
    if ((columns_name == NULL) || (columns_value == NULL))
    {
        PRINT("para is NULL!\n");
        return -2;
    }
    
    if (sqlite3_open("/configure_backup/terminal_dev_register/db/terminal_base_db", &db) != 0)
    {
        PRINT("open err%s\n", sqlite3_errmsg(db));
        return -3;
    }
    
    sprintf(sql, "update %s set ", tb_name);
    for (i = 0; i < columns_count; i++)
    {
        strncat(sql, columns_name[i], strlen(columns_name[i]));
        
        strcat(sql, "=\"");
        strcat(sql, columns_value[i]);
        strcat(sql, "\"");    
        
        if (i != (columns_count - 1))
        {
            strcat(sql, ",");
        }  
    }
    strcat(sql," where ");
    strcat(sql,where_name);
	strcat(sql, "=\"");
	strcat(sql,where_value);
	strcat(sql, "\"");    
   
    PRINT("%s\n", sql);
    if (sqlite3_exec(db, sql, NULL, NULL, &err_msg) != 0)
    {
        PRINT("exec err%d\n", err_msg);
        sqlite3_close(db);
        return -4;
    }
    sqlite3_close(db);
    return 0;
}


void init()
{
	int sockfd,on,i;
	unsigned char pwd[30]={0};
	struct sockaddr_in servaddr;

	sockfd = socket(AF_INET,SOCK_STREAM,0);
	if(sockfd<=0)
	{
		perror("create control socket fail\n");
		exit(-1);
	}
	PRINT("control socket success\n");

	on = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(REGISTER_PORT);

	if(bind(sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr)) == -1)
	{
		perror("control bind error");
		exit(-1);
	}
	PRINT("control bind success\n");

	if(listen(sockfd,10)<0)
	{
		perror("control listen error\n");
		exit(-1);
	}
	register_fd = sockfd;
	PRINT("control listen success\n");
	
	for(i=0;i<4;i++)
	{
		client_list[i].client_fd = -1;
	}
	
	sqlite3_select("terminal_pwd_tb","password",NULL,NULL,pwd);
	PRINT("%s\n",pwd);
	if(strlen(pwd) == 0)
	{
		PRINT("init password\n");
		sqlite3_insert(1,"terminal_pwd_tb",&"password",&DEFAULT_PASSWORD);
	}
		
}

int netWrite(int fd,const void *buffer,int length)
{
	int bytes_left;
	int written_bytes;
	int total=0;
	const char *ptr;
	ptr = (const char *)buffer;
	bytes_left = length;
	while(bytes_left > 0) {
		//begin to write
		written_bytes = write(fd,ptr,bytes_left);
		if(written_bytes <= 0) { //error
			if(errno == EINTR) //interrupt error, we continue writing
				written_bytes = 0;
			else             //other error, exit
				return (-1);
		}
		total += written_bytes;
		bytes_left -= written_bytes;
		ptr += written_bytes;     //from where left,  continue writing
	}
	PRINT("total = %d\n",total);
	return (0);
}

void sock_loop_accept(void *argv)
{
	PRINT("%s running.......\n",__FUNCTION__);
	int clientfd;
	int i;
	fd_set fdset;
	struct timeval tv;
	struct timeval timeout;
	memset(&tv, 0, sizeof(struct timeval));
	memset(&timeout, 0, sizeof(struct timeval));
	struct sockaddr_in client;
	socklen_t len = sizeof(client);

	while (1)
	{
		FD_ZERO(&fdset);
		FD_SET(register_fd, &fdset);

		tv.tv_sec =   1;
		tv.tv_usec = 0;
		switch(select(register_fd + 1, &fdset, NULL, NULL,&tv))
		{
			case -1:
			case 0:
				break;
			default:
				if (FD_ISSET(register_fd, &fdset) > 0)
				{
					clientfd = accept(register_fd, (struct sockaddr*)&client, &len);
					if (clientfd < 0)
					{
						PRINT("accept failed!\n");
						break;
					}
					PRINT("accept success\n");
					PRINT("clientfd=%d\n",clientfd);
					char *new_ip = NULL;
					struct in_addr ia = client.sin_addr;
					new_ip = inet_ntoa(ia);
					PRINT("client_ip=%s\n",new_ip);
					for(i=0;i<4;i++)
					{
						if(client_list[i].client_fd == -1){
							client_list[i].client_fd = clientfd;
							memset(client_list[i].client_ip, 0, sizeof(client_list[i].client_ip));
							memcpy(client_list[i].client_ip, new_ip, strlen(new_ip)+1);
							break;
						}
					}
				}
				for(i=0; i<4; i++)
					printf("%d:cli_fd=%d  , cli_ip=%s\n", i ,client_list[i].client_fd,client_list[i].client_ip);
		}
	}
}

int destroy_client(client_t* client)
{
	PRINT("%s\n",__FUNCTION__);
	if(client->client_fd != -1)
	{
		close(client->client_fd);
		memset(client,0,sizeof(client_t));
		client->client_fd = -1;
	}
	PRINT("wp = %d\n",client->wp);
	PRINT("rp = %d\n",client->rp);
	return 0;
}

void tcp_loop_recv(void *argv)
{
	PRINT("tcp_loop_recv is  running.......\n");
	int ret=0;
	int i;

	fd_set socket_fdset;
	struct timeval tv;
	memset(&tv, 0, sizeof(struct timeval));

	while(1)
	{
		int maxfd = -1;
		int tmp_ret = 0;
		FD_ZERO(&socket_fdset);
		for(i=0; i<4; i++)
		{
			if(client_list[i].client_fd == -1)
				continue;
			FD_SET(client_list[i].client_fd, &socket_fdset);
			maxfd = MAX(maxfd, client_list[i].client_fd);
		}

		tv.tv_sec =  1;
		tv.tv_usec = 0;

		switch(select(maxfd+ 1, &socket_fdset, NULL, NULL, &tv))
		{
			case -1:
			case 0:
				break;
			default:
				for(i=0; i<4; i++)
				{
					if(client_list[i].client_fd == -1)
						continue;
					if ( FD_ISSET(client_list[i].client_fd, &socket_fdset) )
					{
						ret=recv(client_list[i].client_fd,client_list[i].msg+client_list[i].wp,BUF_LEN_256-client_list[i].wp,0);
						if(ret<=0)
						{
							PRINT("socket recv error\n");
							destroy_client(&client_list[i]);
							break;
						}
						PRINT("ret = %d\n",ret);
						ComFunPrintfBuffer(client_list[i].msg+client_list[i].wp,ret);
						client_list[i].wp += ret;
					}
				}
				break;
		}
	}
}

unsigned char sumxor(const  unsigned char  *arr, int len)
{
	int i=0;
	unsigned char sum = 0;
	for(i=0; i<len; i++)
	{
		sum ^= arr[i];
	}

	return sum;
}

int print_str(unsigned char *data,int len)
{
	int i;
	for(i=0;i<len;i++)
	{
		printf("%c",data[i]);
	}
	printf("\n");
}

int return_msg(int *fd,unsigned char cmd,unsigned char *data,int data_len)
{
	unsigned char sendbuf[BUF_LEN_128]={0};
	unsigned char ecbbuf[BUF_LEN_128]={0};
	unsigned char key[8] = {0};
	int i,index = 0;
	int data_len_bak = data_len;
	sendbuf[index++] = PACKET_HEAD_BYTE1;
	sendbuf[index++] = PACKET_HEAD_BYTE2;
	if((data_len+1)%8 != 0)
	{
		data_len = (((data_len+1)/8)+1) * 8 - 1;
	}
	PRINT("data_len = %d\n",data_len);
	sendbuf[index++] = (data_len+1) & 0xff;
	sendbuf[index++] = (data_len+1) >> 8;
	key[0] = (unsigned char)PACKET_HEAD_BYTE1;
	key[1] = (unsigned char)PACKET_HEAD_BYTE2;
	key[2] = (data_len+1) & 0xff;
	key[3] = (data_len+1) >> 8;
	key[4] = (unsigned char)~PACKET_HEAD_BYTE1;
	key[5] = (unsigned char)~PACKET_HEAD_BYTE2;
	key[6] = ~((data_len+1) & 0xff);
	key[7] = ~((data_len+1) >> 8);
	sendbuf[index++] = cmd;
	memcpy(&sendbuf[index],data,data_len_bak);
	index += data_len_bak;
	for(i=0;i<data_len-data_len_bak;i++)
	{
		sendbuf[index+i] = 0x20;
	}
	index += i;
	DesEcb(sendbuf+4,data_len+1,ecbbuf,key,0);
	PRINT("ecbbuf:");
	ComFunPrintfBuffer(ecbbuf,data_len+1);
	memcpy(sendbuf+4,ecbbuf,data_len+1);
	sendbuf[index++]= sumxor(sendbuf+4,data_len+1);
	if(*fd > 0)
	{
		netWrite(*fd,sendbuf,index);
		ComFunPrintfBuffer(sendbuf,index);
	}
	return 0;
}

int registe_req(client_t *cli,unsigned char *packet)
{
	// 5a a5 00 xx xx 00 03 1 2 3 00 03 a b c 00 03 m a c
	unsigned char *data = packet+5;
	int pwd_len = data[1]*8+data[0];
	int name_len = data[pwd_len+3]*8+data[pwd_len+2];
	int mac_len = data[pwd_len+name_len+5]*8 +data[pwd_len+name_len+4]; 
	unsigned char *pwdp = &data[2];
	unsigned char *namep = &data[pwd_len+4];
	unsigned char *macp = &data[pwd_len+name_len+6];
	unsigned char pwd[20]={0};
	unsigned char comming_mac[20]={0};
	unsigned char db_dev_mac[20]={0};
	unsigned char coming_name[17]={0};
	int leng = 0;
	unsigned char result[2] = {0};
	char columns_name[2][30] = {"device_mac","device_name"};
	char columns_val[2][100] ={0};
	leng = (packet[3]<<8)+packet[2];
	PRINT("leng = %d\n",leng);
	PRINT("pwd_len = %d\n",pwd_len);
	PRINT("name_len = %d\n",name_len);
	PRINT("mac_len = %d\n",mac_len);
	print_str(pwdp,pwd_len);
	print_str(namep,name_len);
	print_str(macp,mac_len);
	memcpy(comming_mac,macp,mac_len);
	sqlite3_select("terminal_pwd_tb","password",NULL,NULL,pwd);
	sqlite3_select("terminal_register_tb","device_mac",comming_mac,"device_mac",db_dev_mac);
	if(strlen(pwd) != pwd_len || strncmp(pwdp,pwd,pwd_len)!=0)
	{
		result[0] = ERR;
		result[1] = 0x02;		
		PRINT("password err\n");
		return_msg(&(cli->client_fd),REGISTE_REQ,result,2);
	} 
	else if(strlen(db_dev_mac) != 0)
	{
		result[0] = ERR;
		result[1] = 0x03;		
		PRINT("already register\n");
		return_msg(&(cli->client_fd),REGISTE_REQ,result,2);
	}
	else
	{
		memcpy(columns_val[0],macp,mac_len);
		memcpy(columns_val[1],namep,name_len);
		if(sqlite3_insert(2,"terminal_register_tb",columns_name,columns_val) == 0)
		{
			PRINT("OK\n");
			result[0] = OK;
			return_msg(&(cli->client_fd),REGISTE_REQ,result,1);
		}
	}
	if(cli->client_fd != -1)
	{
		cli->rp += leng;
		cli->rp += 5;
	}
	return 0;
}

int unregiste_req(client_t *cli,unsigned char *packet)
{
	unsigned char *data = packet+5;
	int mac_len = data[1]*8+data[0];
	int leng = 0;
	unsigned char result[2] = {0};
	unsigned char coming_mac[16]={0};
	leng = (packet[3]<<8)+packet[2];
	PRINT("leng = %d\n",leng);
	PRINT("mac_len = %d\n",mac_len);
	print_str(&data[2],mac_len);
	memcpy(coming_mac,&data[2],mac_len);
	if(sqlite3_delete_row(1,"terminal_register_tb",&"device_mac",&coming_mac) == 0)
	{
		PRINT("OK\n");
		result[0] = OK;
		return_msg(&(cli->client_fd),UNREGISTE_REQ,result,1);
	}
	else
	{
		result[0] = ERR;
		result[1] = 0x01;		
		PRINT("ERR\n");
		return_msg(&(cli->client_fd),UNREGISTE_REQ,result,2);
	}
	if(cli->client_fd != -1)
	{
		cli->rp += leng;
		cli->rp += 5;
	}
	
	return 0;
}

int rename_req(client_t *cli,unsigned char *packet)
{
	unsigned char *data = packet+5;
	int name_len = data[1]*8+data[0];
	int mac_len = data[name_len+3]*8+data[name_len+2];
	unsigned char *namep = &data[2];
	unsigned char *macp = &data[name_len+4];
	unsigned char result[2] = {0};
	int leng = 0;
	unsigned char incoming_name[30]={0};
	unsigned char incoming_mac[100]={0};
	leng = (packet[3]<<8)+packet[2];
	PRINT("leng = %d\n",leng);
	PRINT("name_len = %d\n",name_len);
	PRINT("mac_len = %d\n",mac_len);
	print_str(namep,name_len);
	print_str(macp,mac_len);
	memcpy(incoming_name,namep,name_len);
	memcpy(incoming_mac,macp,mac_len);
	
	if(sqlite3_update("terminal_register_tb",1,&"device_name",&incoming_name,"device_mac",incoming_mac) == 0)
	{
		PRINT("OK\n");
		result[0] = OK;
		return_msg(&(cli->client_fd),RENAME_REQ,result,1);
	}
	else
	{
		result[0] = ERR;
		result[1] = 0x01;		
		PRINT("ERR\n");
		return_msg(&(cli->client_fd),RENAME_REQ,result,2);
	}
	if(cli->client_fd != -1)
	{
		cli->rp += leng;
		cli->rp += 5;
	}
	
	return 0;
}

int password_req(client_t *cli,unsigned char *packet)
{
	unsigned char *data = packet+5;
	int old_pwd_len = data[1]*8+data[0];
	int new_pwd_len = data[old_pwd_len+3]*8+data[old_pwd_len+2];
	unsigned char *old_pwdp = &data[2];
	unsigned char *new_pwdp = &data[old_pwd_len+4];
	unsigned char result[2] = {0};
	int leng = 0;
	unsigned char old_pwd[17]={0};
	unsigned char new_pwd[17]={0};
	unsigned char pwd[30]={0};
	leng = (packet[3]<<8)+packet[2];
	PRINT("leng = %d\n",leng);
	PRINT("old_pwd_len = %d\n",old_pwd_len);
	PRINT("new_pwd_len = %d\n",new_pwd_len);
	print_str(old_pwdp,old_pwd_len);
	print_str(new_pwdp,new_pwd_len);
	memcpy(old_pwd,old_pwdp,old_pwd_len);
	memcpy(new_pwd,new_pwdp,new_pwd_len);

	sqlite3_select("terminal_pwd_tb","password",NULL,NULL,pwd);
	if(strlen(pwd) != old_pwd_len || strncmp(pwd,old_pwd,old_pwd_len))
	{
		PRINT("password error\n");
		result[0] = ERR;
		result[1] = 0x02;		
		return_msg(&(cli->client_fd),PASSWORD_REQ,result,2);
		goto OVER;
	}
	
	if(sqlite3_update("terminal_pwd_tb",1,&"password",&new_pwd,"password",old_pwd) == 0)
	{
		PRINT("OK\n");
		result[0] = OK;
		return_msg(&(cli->client_fd),PASSWORD_REQ,result,1);
	}
	else
	{
		result[0] = ERR;
		result[1] = 0x01;		
		return_msg(&(cli->client_fd),PASSWORD_REQ,result,2);
	}
OVER:
	if(cli->client_fd != -1)
	{
		cli->rp += leng;
		cli->rp += 5;
	}
	
	return 0;
}

int check_command(client_t *cli)
{
	char length[2]={0};
	int leng = 0;
	memcpy(length,cli->msg+cli->rp+2,2);
	leng = (length[1]<<8)+length[0];
	unsigned char databuf[BUF_LEN_128]={0};
	unsigned char key[8] = {0};
	key[0] = (unsigned char)PACKET_HEAD_BYTE1;
	key[1] = (unsigned char)PACKET_HEAD_BYTE2;
	key[2] = length[0];
	key[3] = length[1];
	key[4] = (unsigned char)~PACKET_HEAD_BYTE1;
	key[5] = (unsigned char)~PACKET_HEAD_BYTE2;
	key[6] = ~length[0];
	key[7] = ~length[1];
	PRINT("key: ");
	ComFunPrintfBuffer(key,8);
	PRINT("leng = %d\n",leng);
	// 5a a5 00 02 xx yy zz 
	if((cli->msg+cli->rp)[0]!=PACKET_HEAD_BYTE1 || (cli->msg+cli->rp)[1]!=PACKET_HEAD_BYTE2)
	{
		PRINT("head err\n");
		return -1;
	}
	if((leng+5) > (cli->wp-cli->rp))
	{
		PRINT("length err\n");
		return -2;
	}
	if((cli->msg+cli->rp)[leng+4]!=sumxor(cli->msg+cli->rp+4,leng))
	{
		PRINT("xor err\n");
		return -3;
	}
	memcpy(databuf,cli->msg+cli->rp+4,leng);
	PRINT("databuf : ");
	ComFunPrintfBuffer(databuf,leng);
	
	DesEcb(databuf,leng,cli->msg+cli->rp+4,key,1);
	PRINT("after ecb : ");
	ComFunPrintfBuffer(cli->msg+cli->rp+4,leng);
	return 0;
}

int system_call(client_t* cli)
{
	int i;
	switch(*(cli->msg+cli->rp+4))
	{
		case REGISTE_REQ:
			PRINT("REGISTE_REQ\n");
			registe_req(cli,cli->msg+cli->rp);
			break;
		case UNREGISTE_REQ:
			PRINT("UNREGISTE_REQ\n");
			unregiste_req(cli,cli->msg+cli->rp);
			break;
		case RENAME_REQ:
			PRINT("RENAME_REQ\n");
			rename_req(cli,cli->msg+cli->rp);
			break;
		case PASSWORD_REQ:
			PRINT("PASSWORD_REQ\n");
			password_req(cli,cli->msg+cli->rp);
			break;
		default:
			break;
	}
}

void loop_handle(void *argv)
{
	PRINT("%s running.......\n",__FUNCTION__);
	int i;
	int ret = 0;
	while(1)
	{
		for(i=0;i<4;i++)
		{
			if(client_list[i].client_fd == -1)
				continue;
			if(client_list[i].wp > client_list[i].rp)
			{
				PRINT("wp = %d\n",client_list[i].wp);
				PRINT("rp = %d\n",client_list[i].rp);
				ret =check_command(&client_list[i]);
				if(ret == 0)
				{
					PRINT("check success\n");
				}
				else
				{
					PRINT("check error\n");
					destroy_client(&client_list[i]);
					continue;
				}
				system_call(&client_list[i]);
				destroy_client(&client_list[i]);
			}
		}
		usleep(100*1000);
	}
}




