/* sta_access_control.c
 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>

#define STA_MAC "/configure_backup/.staMac"
#define STA_ACL "/configure_backup/.staAcl"

int main(int argc, char *argv[])
{

	struct staList
	{
		int id;
		char macAddr[20];
		char staDesc[80];
		char status[10];  /*on-enable-1; off-disable-0 */
		struct staList *next;
	};
	struct staList stalist;
	FILE *fp, *fp1;
	char con_buf[10];
	char buf[50];

	//system("iptables -N control_sta");
	//system("iptables -A INPUT -j control_sta");
    if ((fp = fopen(STA_ACL, "r")) != NULL)
	{
		if(fread(con_buf, 7, 1, fp) == 0)
		{
			fclose(fp);
			return;
		}
		//LOG(LOG_ERR, "------- OK the con_buf is %s------", con_buf);
		if(strstr(con_buf, "disable"))
		{
			fclose(fp);
			return;
		}
		else if(strstr(con_buf, "enable"))
		{
			if ((fp1 = fopen("STA_MAC", "r")) != NULL) 
			{
				while(fread(&stalist, sizeof stalist, 1, fp1) == 1)
				{
					if(!strcmp(stalist.status, "1"))
					{
						sprintf(buf, "iwpriv ath0 addmac %s", stalist.macAddr);
						system(buf);
						//LOG(LOG_ERR, "------- OK ------ the buf is %s", buf);
						sprintf(buf, "iwpriv ath2 addmac %s", stalist.macAddr);
						system(buf);
						//memset(buf, 0, sizeof buf);
						//sprintf(buf, "iptables -A control_sta -m mac --mac-source %s -j DROP", stalist.macAddr);
						//system(buf);
					}
				}
				system("iwpriv ath0 maccmd 2");
				system("iwpriv ath2 maccmd 2");
				system("ifconfig ath0 down;ifconfig ath0 up > /dev/null 2>&1");
				system("ifconfig ath2 down;ifconfig ath2 up > /dev/null 2>&1");
				fclose(fp1);
			}
		}
	}
	fclose(fp);

}
