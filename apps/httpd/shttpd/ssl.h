/*
 * Copyright (c) 2004-2005 Sergey Lyubka <valenok@gmail.com>
 * All rights reserved
 *
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Sergey Lyubka wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.
 */

/*
 * Snatched from OpenSSL includes. I put the prototypes here to be independent
 * from the OpenSSL source installation. Having this, shttpd + SSL can be
 * built on any system with binary SSL libraries installed.
 */

typedef struct ssl_st SSL;
typedef struct ssl_method_st SSL_METHOD;
typedef struct ssl_ctx_st SSL_CTX;

#define	SSL_ERROR_WANT_READ	2
#define	SSL_ERROR_WANT_WRITE	3
#define SSL_FILETYPE_PEM	1

/*
 * Dynamically loaded SSL functionality
 */
struct ssl_func {
	const char	*name;		/* SSL function name	*/
	union variant	ptr;		/* Function pointer	*/
};

extern void SSL_free(SSL *s);
extern int SSL_accept(SSL *s);
extern int SSL_connect(SSL *s);
extern int SSL_read(SSL *s,void *buf,int num);
extern int SSL_write(SSL *s,const void *buf,int num);
extern int SSL_get_error(const SSL *s,int i);
extern int SSL_set_fd(SSL *s,int fd);
extern void *SSLv23_server_method(void);
extern SSL *SSL_new(SSL_CTX *ctx);
extern SSL_CTX *SSL_CTX_new(void *meth);
extern int SSL_library_init(void);
extern int SSL_CTX_use_PrivateKey_file(SSL_CTX *ctx, const char *file, int type);
extern int SSL_CTX_use_certificate_file(SSL_CTX *ctx, const char *file, int type);

