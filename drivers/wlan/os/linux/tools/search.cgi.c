#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cgiMain.h"

typedef struct wireless_signal{
	char ssid[128];
	char Encryption[128];
	char freq[128];
	char macaddr[128];
	int signal;
}wireless_signal_t;

#define MAX_NUMBER 512
wireless_signal_t signal_list_2g[MAX_NUMBER];
wireless_signal_t signal_list_5g[MAX_NUMBER];
wireless_signal_t param_bak;
static int ii = 0;

char* getcgidata(FILE* fp, char* requestmethod);
static FILE *errOut;
static int atoii (char *zzzz)
{
  int i = 0;
  int num=0;
for(i=0;i<20;i++)
{
  if(zzzz[i] >= '0' && zzzz[i] <= '9')
  	{
	  num = num * 10 + (zzzz[i] - '0');
 	}else
 	{
 			break;
 	}
}
  return num;
}

char *Execute_cmd(char *cmd,char *buffer)
{
    FILE            *f;
    char            *retBuff = buffer;
    char            cmdLine[1024];

    /*
    ** Provide he command string to popen
    */

    f = popen(cmd, "r");

    if(f)
    {
        /*
        ** Read each line.
        */

        while(1)
        {
            *buffer = 0;
            fgets(buffer,120,f);
            if(strlen(buffer) == 0)
            {
                break;
            }
            strcat(buffer,"<br>");
            buffer += strlen(buffer);
        }

        pclose(f);
    }

    return(retBuff);
}

int main()
   {
    char *input;
    char *req_method;
    char name[64];
    char pass[64];
    unsigned long  i = 0;
    unsigned long  j = 0;
    char rspBuff1[65536];
    char rspBuff2[65536];
    char rspBuff3[65536];
    char rspBuff4[65536];
    char rspBuff5[65536];
    char  *val1[MAX_NUMBER];
    char  *val2[MAX_NUMBER];
    char  *val3[MAX_NUMBER];
    char  *val4[MAX_NUMBER];
    char  *val5[MAX_NUMBER];    
    unsigned long num=0;
    char  buf[100];//wangyu add  for ath scan
    unsigned long lists=0;
	int count_write;

	write_systemLog("search wireless signal list  begin"); 

    req_method = getenv("REQUEST_METHOD");
    input = getcgidata(stdin, req_method);
    errOut = fopen("/dev/ttyS0","w");

    // 我们获取的input字符串可能像如下的形式
    // Username="admin"&Password="aaaaa"
    // 其中"Username="和"&Password="都是固定的
    // 而"admin"和"aaaaa"都是变化的，也是我们要获取的
    // 前面9个字符是UserName=
    // 在"UserName="和"&"之间的是我们要取出来的用户名
    for ( i = 2; i < (int)strlen(input); i++ )
    {
        if ( input[i] == '&' )
        {
            name[j] = '\0';
            break;
        } 
        name[j++] = input[i];
    }

    // 前面9个字符 + "&Password="10个字符 + Username的字符数
    // 是我们不要的，故省略掉，不拷贝
    for ( i = 7 + strlen(name), j = 0; i < (int)strlen(input); i++ )
    {
        pass[j++] = input[i];
    }
    pass[j] = '\0';

    fprintf(errOut,"1------Your Username is %s<br>Your Password is %s<br> \n", name, pass);
    //printf("Your Username is %s<br>Your Password is %s<br> \n", name, pass);
    if(strcmp(name,"2G")==0)
    {
        num=0;
		ii = 0;
        //do check ath0
        //Execute_cmd("iwlist ath0 scanning > /tmp/scanlist", rspBuff);
//		fprintf(errOut,"2-----Your Username is %s<br>Your Password is %s<br> \n", name, pass);

        system("iwlist ath0 scanning > /tmp/scanlist 2>&1");

        Execute_cmd("cat /tmp/scanlist | grep Cell | awk '{print $5}'", rspBuff1);
       // fprintf(errOut,"\n-[luodp] %s\n",rspBuff1);
        val1[num]=strtok(rspBuff1,"\n");
        while(val1[num]) {
        num++;
        val1[num]=strtok(NULL,"\n"); 
        }
       //fprintf(errOut,"[luodp] new20\n");
        int i=0;
        //for(i=1;i<num-1;i++)
       // {
        //fprintf(errOut,"[luodp] %s\n",val1[i]+4);
        //}
        
//		fprintf(errOut,"Your Username is %s<br>Your Password is %s<br> \n", name, pass);

        num=0;
        //Signal level
        Execute_cmd("cat /tmp/scanlist | grep Signal | awk '{print $3}' | cut -d \"-\" -f2", rspBuff5);
        //fprintf(errOut,"\n[luodp] %s\n",rspBuff5);
        val5[num]=strtok(rspBuff5,"\n");
        while(val5[num]) {
        num++;
        val5[num]=strtok(NULL,"\n"); 
        }
        num=0;
        //ssid
        Execute_cmd("cat /tmp/scanlist | grep ESSID | awk '{print $1}' | cut -d \":\" -f2", rspBuff2);
        //fprintf(errOut,"\n[luodp] %s\n",rspBuff2);
        val2[num]=strtok(rspBuff2,"\n");
        while(val2[num]) {
        num++;
        val2[num]=strtok(NULL,"\n"); 
        }
       // for(i=1;i<num-1;i++)
       // {
        //fprintf(errOut,"[luodp] %s\n",val2[i]+4);
       // }
        num=0;
        //security
        Execute_cmd("cat /tmp/scanlist | grep Encryption | awk '{print $2}' | cut -d \":\" -f2", rspBuff3);
      //  fprintf(errOut,"\n[luodp] %s\n",rspBuff3);
        val3[num]=strtok(rspBuff3,"\n");
        while(val3[num]) {
        num++;
        val3[num]=strtok(NULL,"\n"); 
        }
       // for(i=1;i<num-1;i++)
        //{
       // fprintf(errOut,"[luodp] %s\n",val3[i]+4);
       // }
        num=0;
        //channel
        Execute_cmd("cat /tmp/scanlist | grep Frequency | awk '{print $4}' | cut -d \")\" -f1", rspBuff4);
        //fprintf(errOut,"\n[luodp] %s\n",rspBuff4);
        val4[num]=strtok(rspBuff4,"\n");
        while(val4[num]) 
        {
            num++;
            val4[num]=strtok(NULL,"\n"); 
        }
        /*for(i=1;i<num-1;i++)
        {
        fprintf(errOut,"[luodp] %s\n",val4[i]+4);
        }*/
        lists=num-1;
       /* for(i=1;i<lists;i++)
        {
        fprintf(errOut,"[luodp] %s(%s)(%s)(%s)\n",val2[i]+4,val1[i]+4,val3[i]+4,val4[i]+4);
        }*/
        
//	   fprintf(errOut,"3----Your Username is %s<br>Your Password is %s<br> lists = %d ---\n", name, pass,lists);
        char cmdd[512]={0};
        char pic[16]={0};
        FILE *fp;
        if((fp=fopen("/usr/www/DWS_2G.xml","w+"))==NULL)
        {
            fprintf(errOut,"fopen DWS_2G fail\n");
            return 0;
        }
        if(val3[0]!=NULL)
        {
        //val1-mac,val2-ssid,val3-security,val4-channel
            if(strcmp(val2[0],"\"\""))
            {
            if(strcmp(val3[0],"on")==0)
                sprintf(val3[0],"WPA");
            else  
                sprintf(val3[0],"None");
            memset(cmdd,0x00,512);	
            memset(buf,0x00,100);	
            memcpy(buf,val2[0]+1,(strlen(val2[0])-2));
			
			#if 0
            sprintf(cmdd,"<option id=\"%s(%s)(%s)(%s)\">%s(-%sdbm)</option>",buf,val1[0],val4[0],val3[0],buf,val5[0]);
            fwrite(cmdd,strlen(cmdd),1,fp);
			#endif	

	//		fprintf(errOut,"6----Your Username is %s<br>Your Password is %s<br> \n", name, pass);
			
			memcpy(signal_list_2g[ii].ssid,buf,(strlen(buf)));
			memcpy(signal_list_2g[ii].Encryption,val3[0],(strlen(val3[0])));			
			memcpy(signal_list_2g[ii].freq,val4[0],(strlen(val4[0])));
			memcpy(signal_list_2g[ii].macaddr,val1[0],(strlen(val1[0])));
			signal_list_2g[ii].signal= 0 - atoii(val5[0]);
			ii ++;

            }
            for(i=1;i<lists;i++)
            {
                memset(cmdd,0x00,512);
                if(strcmp(val2[i]+4,"\"\""))
                {
                if(strcmp(val3[i]+4,"on")==0)
                sprintf(val3[i]+4,"WPA");
                else
                sprintf(val3[i]+4,"None");
                //fprintf(errOut,"[luodp] %s(%s)(%s)(%s) i=== %d \n",val2[i]+4,val1[i]+4,val3[i]+4,val4[i]+4,i);
                memset(buf,0x00,100);	
                memcpy(buf,val2[i]+5,(strlen(val2[i]+4)-2));
				#if 0
                sprintf(cmdd,"<option id=\"%s(%s)(%s)(%s)\">%s(-%sdbm)</option>",buf,val1[i]+4,val4[i]+4,val3[i]+4,buf,val5[i]+4);
                fwrite(cmdd,strlen(cmdd),1,fp);
				#endif
				
				memcpy(signal_list_2g[ii].ssid,buf,(strlen(buf)));
				memcpy(signal_list_2g[ii].Encryption,val3[i]+4,(strlen(val3[i]+4)));
				memcpy(signal_list_2g[ii].freq,val4[i]+4,(strlen(val4[i]+4)));
				memcpy(signal_list_2g[ii].macaddr,val1[i]+4,(strlen(val1[i]+4)));
				signal_list_2g[ii].signal= 0 - atoii(val5[i]+4);
				ii ++;
				
                }
				
            }
			
//			fprintf(errOut,"44----Your Username is %s<br>Your Password is %s<br>  ii ==== %d \n", name, pass,ii);

			for(i=0;i<ii;i++){
				for(j=0; j<ii-1-i;j++){
	//				fprintf(errOut,"2-----------signal_list_2g \n");
					memset(&param_bak,0,sizeof(struct wireless_signal));
					if ((signal_list_2g[j].signal) < (signal_list_2g[j + 1].signal)){
	//					fprintf(errOut,"33-----------signal_list_2g \n");
						
						memcpy(&param_bak,&signal_list_2g[j],sizeof(struct wireless_signal));
						memcpy(&signal_list_2g[j],&signal_list_2g[j + 1],sizeof(struct wireless_signal));
						memcpy(&signal_list_2g[j + 1],&param_bak,sizeof(struct wireless_signal));
					}
				}							
			}
			
//			fprintf(errOut,"45----Your Username is %s<br>Your Password is %s<br>  ii ==== %d \n", name, pass,ii);
		
			for ( j = 0; j < ii ; j++ ){
                memset(cmdd,0x00,512);


                
              //  sprintf(cmdd,"<option id=\"%s(%s)(%s)(%s)\">%s(%ddbm)</option>",
		//			signal_list_2g[j].ssid,signal_list_2g[j].macaddr,signal_list_2g[j].freq,signal_list_2g[j].Encryption,signal_list_2g[j].ssid,signal_list_2g[j].signal);

                if(strcmp(signal_list_2g[j].Encryption,"WPA")==0)
                {
                    int power=signal_list_2g[j].signal;
                    if(power<-90)
                    {
                        sprintf(pic,"WPAxiao.gif");

                    }
                    else if((power>=-90)&&(power<-60))
                    {
                        sprintf(pic,"WPAzhong.gif");
                    }
                    else
                    {
                        sprintf(pic,"WPAda.gif");                   
                    }

                }
                else
                {
                    int power=signal_list_2g[j].signal;
                    if(power<-90)
                    {
                        sprintf(pic,"NONExiao.gif");
                        
                    }
                    else if((power>=-90)&&(power<-60))
                    {
                        sprintf(pic,"NONEzhong.gif");
                    }
                    else
                    {
                        sprintf(pic,"NONEda.gif");
                    }

                }
                sprintf(cmdd,"<li rel=\"%s(%s)(%s)(%s)\"> <span><img src=\"/images/%s\"/></span>%s </li>",signal_list_2g[j].ssid,signal_list_2g[j].macaddr,signal_list_2g[j].freq,signal_list_2g[j].Encryption,pic,signal_list_2g[j].ssid);
                count_write = fwrite(cmdd,strlen(cmdd),1,fp);

	//			fprintf(errOut,"22-----------signal_list_2g[%d]:ssid %s Encryption %s signal %d dbm \n",
	//				j,signal_list_2g[j].ssid,signal_list_2g[j].Encryption,signal_list_2g[j].signal);
	
			}
			
        }
        fclose(fp);
		
		
    }
    else if(strcmp(name,"5G")==0)
    {
        num=0;
		ii = 0;
        //do check ath2
        //Execute_cmd("iwlist ath2 scanning > /tmp/scanlist", rspBuff);

        system("iwlist ath2 scanning > /tmp/scanlist_2 2>&1");
        //mac
        Execute_cmd("cat /tmp/scanlist_2 | grep Cell | awk '{print $5}'", rspBuff1);
        //fprintf(errOut,"\n-[luodp] %s\n",rspBuff1);
        val1[num]=strtok(rspBuff1,"\n");
        while(val1[num]) {
        num++;
        val1[num]=strtok(NULL,"\n"); 
        }
        //fprintf(errOut,"[luodp] new20\n");
        //int i=0;
        /*for(i=1;i<num-1;i++)
        {
        fprintf(errOut,"[luodp] %s\n",val1[i]+4);
        }*/
        num=0;
        //Signal level
        Execute_cmd("cat /tmp/scanlist_2 | grep Signal | awk '{print $3}' | cut -d \"-\" -f2", rspBuff5);
        //fprintf(errOut,"\n[luodp] %s\n",rspBuff5);
        val5[num]=strtok(rspBuff5,"\n");
        while(val5[num]) {
        num++;
        val5[num]=strtok(NULL,"\n"); 
        }
        num=0;
        //ssid
        Execute_cmd("cat /tmp/scanlist_2 | grep ESSID | awk '{print $1}' | cut -d \":\" -f2", rspBuff2);
        //fprintf(errOut,"\n[luodp] %s\n",rspBuff2);
        val2[num]=strtok(rspBuff2,"\n");
        while(val2[num]) {
        num++;
        val2[num]=strtok(NULL,"\n"); 
        }
        /*for(i=1;i<num-1;i++)
        {
        fprintf(errOut,"[luodp] %s\n",val2[i]+4);
        }*/
        num=0;
        //security
        Execute_cmd("cat /tmp/scanlist_2 | grep Encryption | awk '{print $2}' | cut -d \":\" -f2", rspBuff3);
        //fprintf(errOut,"\n[luodp] %s\n",rspBuff);
        val3[num]=strtok(rspBuff3,"\n");
        while(val3[num]) {
        num++;
        val3[num]=strtok(NULL,"\n"); 
        }
        /*for(i=1;i<num-1;i++)
        {
        fprintf(errOut,"[luodp] %s\n",val3[i]+4);
        }*/
        num=0;
        //channel
        Execute_cmd("cat /tmp/scanlist_2 | grep Frequency | awk '{print $4}' | cut -d \")\" -f1", rspBuff4);
        //fprintf(errOut,"\n[luodp] %s\n",rspBuff);
        val4[num]=strtok(rspBuff4,"\n");
        while(val4[num]) {
        num++;
        val4[num]=strtok(NULL,"\n"); 
        }
        /*for(i=1;i<num-1;i++)
        {
        fprintf(errOut,"[luodp] %s\n",val4[i]+4);
        }*/
        lists=num-1;
        /*for(i=1;i<lists;i++)
        {
        fprintf(errOut,"[luodp] %s(%s)(%s)(%s)\n",val2[i]+4,val1[i]+4,val3[i]+4,val4[i]+4);
        }*/
        char cmdd[512]={0};
        char pic[16]={0};
        FILE *fp;
        if((fp=fopen("/usr/www/DWS_5G.xml","w+"))==NULL)
        {
            fprintf(errOut,"fopen DWS_5G fail\n");
            return 0;
        }
        if(val3[0]!=NULL)
        {
        //val1-mac,val2-ssid,val3-security,val4-channel
        if(strcmp(val2[0],"\"\""))
        {
        if(strcmp(val3[0],"on")==0)
        sprintf(val3[0],"WPA");
        else  
        sprintf(val3[0],"None");
        memset(cmdd,0x00,512);
        memset(buf,0x00,100);	
        memcpy(buf,val2[0]+1,(strlen(val2[0])-2));
        //					fprintf(errOut,"[luodp] buff----------- %s\n",buf);
		#if 0
        sprintf(cmdd,"<option id=\"%s(%s)(%s)(%s)\">%s(-%sdbm)</option>",buf,val1[0],val4[0],val3[0],buf,val5[0]);
        fwrite(cmdd,strlen(cmdd),1,fp);
		#endif

		memcpy(signal_list_5g[ii].ssid,buf,(strlen(buf)));
		memcpy(signal_list_5g[ii].Encryption,val3[0],(strlen(val3[0])));
		memcpy(signal_list_5g[ii].freq,val4[0],(strlen(val4[0])));
		memcpy(signal_list_5g[ii].macaddr,val1[0],(strlen(val1[0])));
		signal_list_5g[ii].signal= 0 - atoii(val5[0]);
		ii ++;
        }
        for(i=1;i<lists;i++)
        {
        if(strcmp(val2[i]+4,"\"\""))
        {
        if(strcmp(val3[i]+4,"on")==0)
        sprintf(val3[i]+4,"WPA");
        else
        sprintf(val3[i]+4,"None");
        //fprintf(errOut,"[luodp] %s(%s)(%s)(%s) \n",val2[i]+4,val1[i]+4,val3[i]+4,val4[i]+4);
        memset(cmdd,0x00,512);
        memset(buf,0x00,100);	
        memcpy(buf,val2[i]+5,(strlen(val2[i]+4)-2));
        //							fprintf(errOut,"[luodp] buff----------- %s\n",buf);
		#if 0
        sprintf(cmdd,"<option id=\"%s(%s)(%s)(%s)\">%s(-%sdbm)</option>",buf,val1[i]+4,val4[i]+4,val3[i]+4,buf,val5[i]+4);
        fwrite(cmdd,strlen(cmdd),1,fp);
	#endif
		memcpy(signal_list_5g[ii].ssid,buf,(strlen(buf)));
		memcpy(signal_list_5g[ii].Encryption,val3[i]+4,(strlen(val3[i]+4)));
		memcpy(signal_list_5g[ii].freq,val4[i]+4,(strlen(val4[i]+4)));
		memcpy(signal_list_5g[ii].macaddr,val1[i]+4,(strlen(val1[i]+4)));
		signal_list_5g[ii].signal= 0 - atoii(val5[i]+4);
		ii ++;
        }
        }
		
		for(i=0;i<ii;i++){
			for(j=0; j<ii-1-i;j++){
//				fprintf(errOut,"2-----------signal_list_5g \n");
				memset(&param_bak,0,sizeof(struct wireless_signal));
				if ((signal_list_5g[j].signal) < (signal_list_5g[j + 1].signal)){
//					fprintf(errOut,"33-----------signal_list_5g \n");
					
					memcpy(&param_bak,&signal_list_5g[j],sizeof(struct wireless_signal));
					memcpy(&signal_list_5g[j],&signal_list_5g[j + 1],sizeof(struct wireless_signal));
					memcpy(&signal_list_5g[j + 1],&param_bak,sizeof(struct wireless_signal));
				}
			}							
		}
	
		for ( j = 0; j < ii ; j++ ){
			
			memset(cmdd,0x00,512);	
                if(strcmp(signal_list_5g[j].Encryption,"WPA")==0)
                {
                    int power=signal_list_5g[j].signal;
                    if(power<-90)
                    {
                        sprintf(pic,"WPAxiao.gif");

                    }
                    else if((power>=-90)&&(power<-60))
                    {
                        sprintf(pic,"WPAzhong.gif");
                    }
                    else
                    {
                        sprintf(pic,"WPAda.gif");                   
                    }

                }
                else
                {
                    int power=signal_list_5g[j].signal;
                    if(power<-90)
                    {
                        sprintf(pic,"NONExiao.gif");
                        
                    }
                    else if((power>=-90)&&(power<-60))
                    {
                        sprintf(pic,"NONEzhong.gif");
                    }
                    else
                    {
                        sprintf(pic,"NONEda.gif");
                    }

                }
                sprintf(cmdd,"<li rel=\"%s(%s)(%s)(%s)\"> <span><img src=\"/images/%s\"/></span>%s </li>",signal_list_5g[j].ssid,signal_list_5g[j].macaddr,signal_list_5g[j].freq,signal_list_5g[j].Encryption,pic,signal_list_5g[j].ssid);
                
			//sprintf(cmdd,"<option id=\"%s(%s)(%s)(%s)\">%s(%ddbm)</option>",
			//	signal_list_5g[j].ssid,signal_list_5g[j].macaddr,signal_list_5g[j].freq,signal_list_5g[j].Encryption,signal_list_5g[j].ssid,signal_list_5g[j].signal);
			fwrite(cmdd,strlen(cmdd),1,fp);

//				fprintf(errOut,"22-----------signal_list_5g[%d]:ssid %s Encryption %s signal %d dbm \n",
//					j,signal_list_5g[j].ssid,signal_list_5g[j].Encryption,signal_list_5g[j].signal);
			}
        }
        fclose(fp);
        }
        else if(strcmp(name,"2GCHECK")==0)
        {
              char tmp[128];
		Execute_cmd("cfg -e | grep \"WWAN_2G_FLAG=\" | awk -F \"=\" '{print $2}'",tmp);
              printf("%s",tmp);
              fprintf(errOut,"2GCHECK-----------%s\n",tmp);
        }
         else if(strcmp(name,"5GCHECK")==0)
        {
              char tmp[128];
		Execute_cmd("cfg -e | grep \"WWAN_5G_FLAG=\" | awk -F \"=\" '{print $2}'",tmp);
              printf("%s",tmp);
              fprintf(errOut,"5GCHECK-----------%s\n",tmp);
         }
	write_systemLog("search wireless signal list  end"); 
    return 1;
}

char* getcgidata(FILE* fp, char* requestmethod)
{
    char* input;
    int len;
    int size = 1024;
    int i = 0;
    if (!strcmp(requestmethod, "GET"))
    {
        input = getenv("QUERY_STRING");
        return input;
    }
    else if (!strcmp(requestmethod, "POST"))
    {
        len = atoi(getenv("CONTENT_LENGTH"));
         input = (char*)malloc(sizeof(char)*(size + 1));
        if (len == 0)
        {
            input[0] = '\0';
            return input;
        }
        while(1)
        {
            input[i] = (char)fgetc(fp);
            if (i == size)
            {
                input[i+1] = '\0';
                return input;
            }
            --len;
            if (feof(fp) || (!(len)))
            {
                i++;
                input[i] = '\0';
                return input;
            }
            i++;
         }
    }
    return NULL;
}

