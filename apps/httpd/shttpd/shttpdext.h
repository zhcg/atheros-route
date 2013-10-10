/* shttpdext.h -- header of shttpdext.c
 * by bowen guo, 2008-09-18 */
#ifndef SHTTPDEXT_H
#define SHTTPDEXT_H

#define ADMIN_TYPE 1
#define USER_TYPE 2
#define LOGINPAGE "/login.html"
#define ACCOUNTERR "/login.html?ErrNo=1"
#define ALREADYERR "/login.html?ErrNo=2"
#define TRYMAXERR "/login.html?ErrNo=3"
#define LOGINREQ "/login"
#define LOGOUTREQ "/logout"
#define LOGINPIC1 "/athlogo.jpg"
#define LOGINPIC2 "/e8logo.gif"
#define CAPTIVEREQ "/athCgiMain.cgi?action=captiveportal"
int check_authorize(struct conn *c);
int CurretUserType();
void shttpdext_init(struct shttpd_ctx *ctx);

#endif

