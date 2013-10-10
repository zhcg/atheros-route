/*
---------------------------------------------------------------------------
 tsp_auth.c,v 1.12 2004/03/09 21:55:47 dgregoire Exp
---------------------------------------------------------------------------
* This source code copyright (c) Hexago Inc. 2002-2004.
* 
* This program is free software; you can redistribute it and/or modify it 
* under the terms of the GNU General Public License (GPL) Version 2, 
* June 1991 as published by the Free  Software Foundation.
* 
* This program is distributed in the hope that it will be useful, 
* but WITHOUT ANY WARRANTY;  without even the implied warranty of 
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
* See the GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License 
* along with this program; see the file GPL_LICENSE.txt. If not, write 
* to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
* MA 02111-1307 USA
---------------------------------------------------------------------------
*/


#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#define _USES_SYS_SOCKET_H_

#include "platform.h"

#include "tsp_auth.h"
#include "tsp_cap.h"
#include "log.h"
#include "base64.h"
#include "md5.h"


/* structures */

typedef struct  {
	char *realm,
	    *nonce,
	    *qop,
	    *algorithm,
	    *charset,
	    *rspauth;
} tChallenge;


static
int
AuthANONYMOUS(SOCKET socket, net_tools_t *nt)
{
	char Buffer[1024];
	char string[] = "AUTHENTICATE ANONYMOUS\r\n";
	
	if (nt->netsendrecv(socket, string, sizeof(string), Buffer, sizeof(Buffer)) == -1) {
		Display(2, ELError, "AuthANONYMOUS", "Error in netsendrecv()");
		return -1;
	}
	
	if (memcmp(Buffer, "200", 3)) { /* Not successful */
		Display(4, ELError, "AuthANONYMOUS", "No success: %s", Buffer);
		return -1;
	}
	
	return 0;
}

/* */

static
int
AuthPLAIN(SOCKET socket, char *user, char *passwd, net_tools_t *nt) {

	char BufferIn[1024];
	char BufferOut[1024];
	char string[] = "AUTHENTICATE PLAIN\r\n";
	int Length;

	if ( nt->netsend(socket, string, sizeof(string)) == -1 ) {
		Display(2, ELError, "AuthPLAIN", "Not able to write to server socket");
		return -1;
	}

	memset(BufferIn, 0, sizeof(BufferIn));
	Length = snprintf(BufferIn, sizeof(BufferIn), "%c%s%c%s\r\n",'\0', user, '\0', passwd);

	if ( nt->netsendrecv(socket, BufferIn, Length, BufferOut, sizeof(BufferOut)) == -1 ) {
		Display(2, ELError, "AuthPLAIN", "Not able to sendrecv() to server socket");
		return -1;
	}

	if (memcmp(BufferOut, "200", 3)) { /* Not successful */
		Display(4, ELError, "AuthPLAIN", "No success: %s", BufferOut);
		return -1;
	}

	return 0;
}

/* */

static
int
InsertInChallegeStruct(tChallenge *c, char *Token, char *Value) {

	if(strcmp(Token, "realm")==0) {
		c->realm = strdup(Value);
		return 0;
	}

	if(strcmp(Token, "nonce")==0) {
		c->nonce = strdup(Value);
		return 0;
	}

	if(strcmp(Token, "qop")==0) {
		c->qop = strdup(Value);
		return 0;
	}

	if(strcmp(Token, "algorithm")==0) {
		c->algorithm = strdup(Value);
		return 0;
	}

	if(strcmp(Token, "charset")==0) {
		c->charset = strdup(Value);
		return 0;
	}

	if(strcmp(Token, "rspauth")==0) {
		c->rspauth = strdup(Value);
		return 0;
	}
	
	return -1;
}

/* */

static
void
ExtractChallenge(tChallenge *c, char *String) {
	char *s, *e, Token[strlen(String)+1], Value[strlen(String)+1];
	int len;

	memset(c, 0, sizeof(tChallenge));

	*Token=*Value=0;
	
	for(s=e=String; ; e++) {
		if(*e== ',' || *e == '\r' || *e == '\n' || *e==0) {
			if(s!=e) {
				if(*Token && (*Value==0)) {
					len = (int)((char *)e-(char *)s);
/* Chop the quotes */
					if((*s == '"') && len) { s++; len--; }
					if((s[len-1] == '"') && len) len--;
					if(len) memcpy(Value, s, len);
					Value[len] = 0;
				}
				if(*Token && *Value) {
					InsertInChallegeStruct(c, Token,Value);
					*Value = *Token = 0;
				}
			}

			if(*e == 0) break;
			s = ++e;
		}

		if((*e=='=' || *e== ',' || *e == '\r' || *e == '\n' || *e==0) && (*Token == 0) && (e != s)) {
			len = (int)((char *)e-(char *)s);
			memcpy(Token, s, len);
			Token[len] = 0;
			if(*e == 0) break;
			s = ++e;
		}
	}
}

/* */

static
int
AuthDIGEST_MD5(SOCKET socket, char *user, char *passwd, char *host, net_tools_t *nt) {

	char Buffer[4096], Response[33], cResponse[33], *ChallengeString;
	char string[] = "AUTHENTICATE DIGEST-MD5\r\n";
	char BufferIn[4096];
	time_t cnonce = time(NULL);
	tChallenge c;

	memset(BufferIn, 0, sizeof(BufferIn));
	
	if (nt->netsendrecv(socket, string, sizeof(string), BufferIn, sizeof(BufferIn)) == -1) {
		Display(2, ELError, "AuthDIGEST_MD5", "Not able to read/write to server socket");
		return 1;
	}

	if (memcmp(BufferIn, "300", 3) == 0) { /* Not successful */
		Display(4, ELError, "AuthDIGEST_MD5", "No success: %s", Buffer+3);
		return -1;
	}

	if((ChallengeString = malloc(strlen(BufferIn) + 1)) == NULL) {
		Display(2, ELError, "AuthDIGEST_MD5", "Not enough memory for ChallengeString!");
		return -1;
	}

	base64decode(ChallengeString, BufferIn);
	ExtractChallenge(&c, ChallengeString);
	free(ChallengeString);

   {
   /*-----------------------------------------------------------*/
   /*
      Extract from : RFC 2831 Digest SASL Mechanism

      Let H(s) be the 16 octet MD5 hash [RFC 1321] of the octet string s.

      Let KD(k, s) be H({k, ":", s}), i.e., the 16 octet hash of the string
      k, a colon and the string s.

      Let HEX(n) be the representation of the 16 octet MD5 hash n as a
      string of 32 hex digits (with alphabetic characters always in lower
      case, since MD5 is case sensitive).

      response-value  =
         HEX( KD ( HEX(H(A1)),
                 { nonce-value, ":" nc-value, ":",
                   cnonce-value, ":", qop-value, ":", HEX(H(A2)) }))

      If authzid is not specified, then A1 is

         A1 = { H( { username-value, ":", realm-value, ":", passwd } ),
           ":", nonce-value, ":", cnonce-value }

      If the "qop" directive's value is "auth", then A2 is:

         A2       = { "AUTHENTICATE:", digest-uri-value }

   */
	   char *A1_1Fmt      = "%s:%s:%s",
	       *A1Fmt        = ":%s:%lu",
	       *A2Fmt        = "%s:tsp/%s",
	       *ChallRespFmt = "%s:%s:00000001:%lu:%s:%s",
	       *ResponseFmt   = "charset=%s,username=\"%s\",realm=\"%s\",nonce=\"%s\",nc=00000001,cnonce=\"%lu\",digest-uri=\"tsp/%s\",response=%s,qop=auth",
	       A1[33], A1_1[33], A2[33], cA2[33], *String;
	   int len;

      /*-----------------------------------------------------------*/
      /* Build HEX(H(A2)) & HEX(H(cA2))                            */

	   len = strlen(A2Fmt) + 12 /* AUTHENTICATE */ + strlen(host) + 1;
	   if((String = malloc(len)) == NULL) {
		   Display(2, ELError, "AuthDIGEST_MD5", "Not enough memory for String!");
		   return -1;
	   }

	   snprintf(String, len, A2Fmt, "AUTHENTICATE", host);
#if DEBUG
	   printf("A2 = %s\n", String);
#endif
	   strncpy(A2, md5(String), 33);
	   snprintf(String, len, A2Fmt, "", host);
#if DEBUG
	   printf("cA2 = %s\n", String);
#endif
	   strncpy(cA2, md5(String), 33);
	   free(String);

      /*-----------------------------------------------------------*/
      /* Build HEX(H(A1))                                          */
      /* A1_1 = { username-value, ":", realm-value, ":", passwd }  */
      /* A1 = { H( A1_1 ), ":", nonce-value, ":", cnonce-value }   */

	   len = strlen(A1_1Fmt) + strlen(user) + strlen(c.realm) +
	       strlen(passwd) +  1;
	   if((String = malloc(len)) == NULL) {
		   Display(2, ELError, "AuthDIGEST_MD5", "Not enough memory for String!");
		   return -1;
	   }

	   snprintf(String, len, A1_1Fmt, user, c.realm, passwd);
#if DEBUG
	   printf("A1_1 = %s\n", String);
#endif
	   md5digest(String, A1_1);
	   free(String);

	   len = 16 /* A1_1 */ + 1 +
	       strlen(c.nonce) + 16 /* cnonce */ +  1;
	   if((String = malloc(len)) == NULL) {
		   Display(2, ELError, "AuthDIGEST_MD5", "Not enough memory for String!");
		   return -1;
	   }

	   memcpy(String, A1_1, 16);
	   snprintf(String + 16, len - 16, A1Fmt, c.nonce, cnonce);
	   strncpy(A1, md5(String), 33);
	   free(String);
#if DEBUG
	   printf("A1 = [%s]\n", A1);
#endif      

      /*-----------------------------------------------------------*/
      /* Build server's and client's challenge responses           */
	   len = strlen(ChallRespFmt) + 32 /* md5(A1) */ + strlen(c.nonce) +16 /* cnonce */ + strlen(c.qop) + 32 /* md5(A2) */ +  1;
	   if((String = malloc(len)) == NULL) {
		   Display(2, ELError, "AuthDIGEST_MD5", "Not enough memory for String!");
		   return -1;
	   }

	   snprintf(String, len, ChallRespFmt, A1, c.nonce, cnonce, c.qop, A2);
#if DEBUG
	   printf("Response = [%s]\n", String);
#endif
	   strncpy(Response, md5(String), 33);
#if DEBUG
	   printf("MD5 Response = %s\n", Response);
#endif
	   snprintf(String, len, ChallRespFmt, A1, c.nonce, cnonce, c.qop, cA2);
#if DEBUG
	   printf("cResponse = [%s]\n", String);
#endif
	   strncpy(cResponse, md5(String), 33);
#if DEBUG
	   printf("MD5 cResponse = %s\n", cResponse);
#endif
	   free(String);

      /*-----------------------------------------------------------*/
      /* Build Response                                            */

	   len = strlen(ResponseFmt) + strlen(c.charset) + strlen(user) +
	       strlen(c.realm) + strlen(c.nonce) + 16 /*cnonce*/ +
	       strlen(host)    + 32 /* md5 response */;
	   if((String = malloc(len)) == NULL) {
		   Display(2, ELError, "AuthDIGEST_MD5", "Not enough memory for String!");
		   return -1;
	   }

	   snprintf(String, len, ResponseFmt, c.charset, user, c.realm, c.nonce, cnonce, host, Response);
	   memset(Buffer, 0, sizeof(Buffer));
	   base64encode(Buffer, String, strlen(String));
	   free(String);
   }


   memset(BufferIn, 0, sizeof(BufferIn));
   
   if ( nt->netprintf(socket, BufferIn, sizeof(BufferIn), "%s\r\n", Buffer) == -1) {
	   Display(2, ELError, "AuthDIGEST_MD5", "Not able to write to server socket");
	   return -1;
   }

   /*-----------------------------------------------------------*/
   /* Verify server response                                    */

   if (memcmp(BufferIn, "300", 3) == 0) { /* Not successful */
      Display(4 ,ELError, "AuthDIGEST_MD5", "No success: %s", Buffer+4);
      return -1;
   }

   if((ChallengeString = malloc(strlen(BufferIn) + 1)) == NULL) {
      Display(2, ELError, "AuthDIGEST_MD5", "Not enough memory for ChallengeString!");
      return -1;
   }
   
   base64decode(ChallengeString, BufferIn);
   ExtractChallenge(&c, ChallengeString);
   free(ChallengeString);

   if(memcmp(c.rspauth, cResponse, 32)) {
      Display(2, ELError, "AuthDIGEST_MD5", "Invalid response from server");
      return -1;
   }

   if (nt->netrecv(socket, Buffer, sizeof (Buffer) ) == -1) {
      Display(2, ELError, "AuthDIGEST_MD5", "Not able to read from server socket");
      return -1;
   }
   
   if (memcmp(Buffer, "200", 3)) { /* Not successful */
      Display(4, ELError, "AuthDIGEST_MD5", "No success: %s", Buffer+4);
      return -1;
   }

   return 0;
}

/* */

/* exports */


int
tspAuthenticate(SOCKET socket, char *method, char *user, char *passwd, char *host, tCapability cap, net_tools_t *nt) 
{
	int rc = -100;
	tCapability Mechanism;
	
	
	if(!(cap & (TUNNEL_V6V4|TUNNEL_V6UDPV4)))
		return -1; /* Server does not support our types of tunnels */

	if(memcmp(method, "ANY", 4) && memcmp(method, "any", 4))
		Mechanism = tspSetCapability("AUTH", method);
	else
		Mechanism = AUTH_ANY;

	if(strcmp(user, "anonymous")) { /* If not anonymous user */
		if(Mechanism & cap & AUTH_DIGEST_MD5) {
			Display(2, ELNotice, "tspAuthenticate", "Using authentification mecanism DIGEST-MD5");
			rc = AuthDIGEST_MD5(socket, user, passwd, host, nt);
			goto EndAuthenticate;
		}
		if(Mechanism & cap & AUTH_PLAIN) {
			Display(2, ELNotice, "tspAuthenticate", "Using authentification mecanism AUTH-PLAIN");
			rc = AuthPLAIN(socket, user, passwd, nt);
			goto EndAuthenticate;
		}
	} else { /* ANONYMOUS user */
		if(cap & AUTH_ANONYMOUS) {
			Display(2, ELNotice, "tspAuthenticate", "Using authentification mecanism AUTH-ANONYMOUS");
			rc = AuthANONYMOUS(socket, nt);
			goto EndAuthenticate;
		}
	}

 EndAuthenticate:
	if(rc == -100) {
		Display(0, ELError, "tspAuthenticate", "No common authentication mechanism found");
	    Display(0, ELNotice, "tspAuthenticate", "Did you forget to specify a username?");
	}
	return rc;
}






