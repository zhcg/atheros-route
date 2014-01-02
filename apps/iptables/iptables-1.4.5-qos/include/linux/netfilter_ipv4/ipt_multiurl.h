#ifndef	_IPT_MULTI_URL_H_
#define	_IPT_MULTI_URL_H_


#define MULTIURL_VERSION			"0.0.1"

#define IPT_MULTIURL_MAX_URL		10
#define	IPT_MULTIURL_STRLEN		31

#define IPT_MULTIURL		1 << 1

struct xt_multiurl_info
{
	int		url_count;
	char	urls[IPT_MULTIURL_MAX_URL][IPT_MULTIURL_STRLEN];
};


#endif
