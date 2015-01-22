#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>

char cmd_buf[1024];
/**
 * 获得命令输出
 */
int get_cmd_out(char *cmd, char *buf, unsigned short buf_len)
{
    int pipe_fd[2] = {0};
	pid_t child_pid = 0;
	int res = 0;
	if ((cmd == NULL) || (buf == NULL))
	{
	    printf("para is NULL!\n");
	    return -1;
	}
	
	if (pipe(pipe_fd) < 0)
	{
	    printf("pipe failed!\n");
	    return -1;
	}
	
	if ((child_pid = fork()) < 0)
	{
	    printf("fork failed!\n");
		return -1;
	}
	
	memset(cmd_buf, 0, 1024);
	
	if (child_pid == 0) // subprocess write
	{
		close(pipe_fd[0]);
		
		//printf("pipe_fd[1] = %d\n", pipe_fd[1]);		
		sprintf(cmd_buf, "%s 1>&%d", cmd, pipe_fd[1]);
		//printf("cmd_buf = %s\n", cmd_buf);
		
		if (system(cmd_buf) < 0)
		{
		    res = -1;
		}
		close(pipe_fd[1]);
		//printf("_____________________________get_cmd_out\n");
	    exit(res); //此处用exit，结束当前进程
	    //return res;
	}
	else // parent process read
	{
		close(pipe_fd[1]);
		memset(buf, 0, buf_len);
		//printf("pipe_fd[0] = %d\n", pipe_fd[0]);
		if (read(pipe_fd[0], buf, buf_len - 1) < 0)
		{
		    printf("read failed!\n");
		    close(pipe_fd[0]);
		    wait(&res);
		    return -1;
		}
		close(pipe_fd[0]);
		wait(&res);
		if (res != 0)
		{
		    printf("system failed!\n");
		    res = -1;
		}
		return res;
	}
}

// 检测应用程序的运行状态

int main(int argc, char ** argv)
{
    //char *modules_cmd = "ps | grep modules_server | sed '/grep/'d | sed '/\\[modules_server\\]/'d | sed '/sed/'d";
	char cmd[1024] = {0};
	char buf[1024] = {0};
	int i;
	if(argc <= 1)
	{
		printf("cmd err\n");
		exit(-1);
	}
    while (1)
    {
		for(i=0;i<argc-1;i++)
		{
			memset(cmd,0,1024);
			sprintf(cmd,"ps | grep %s | sed '/%s/'d  | sed '/grep/'d | sed '/\\[%s\\]/'d | sed '/sed/'d",argv[i+1],argv[0],argv[i+1]);
		//	printf("cmd = %s\n",cmd);
			usleep(200*1000);
			memset(buf,0,1024);
			if (get_cmd_out(cmd, buf, sizeof(buf)) < 0)
			{
				printf("get_cmd_out failed!\n");
				continue;
			}
			else
			{
				if (strlen(buf) == 0)
				{
					printf("%s stop!\n",argv[i+1]);
					memset(cmd,0,1024);
					sprintf(cmd,"%s &",argv[i+1]);
					system(cmd);
					printf("%s restart!\n",argv[i+1]);
				}
			}
		}
        sleep(5);
    }
}
