#include "exevents.h"


/**
 * 注册失败分析
 */
static int cacm_register_failed(struct s_cacm *cacm, eXosip_event_t *ev)
{
	int status_code = 0;
	char *reason = NULL;
	char *ru = NULL;
	
	osip_uri_t *requri = osip_message_get_uri(ev->request);
	osip_uri_to_str(requri, &ru);

	if (ev->response)
	{
		status_code = osip_message_get_status_code(ev->response);
		reason = osip_message_get_reason_phrase(ev->response);
	}
	
	PRINT("registration_faillure! status_code = %d\n", status_code);
	switch(status_code)
	{
		case 401: // 注册鉴权
		case 407: // 呼叫请求鉴权
		{
		    PRINT("401 Unauthorized!\n");
		    if (eXosip_add_authentication_info(cacm->user_name, cacm->user_name, cacm->password, NULL, NULL) < 0)
        	{
        	    PERROR("eXosip_add_authentication_info failed!\n");
        	    return ADD_AUTHENTICATION_INFO_ERR;
        	}
        	
        	eXosip_lock();
    		if (eXosip_default_action(ev) < 0)
    		{
    		    PERROR("eXosip_default_action failed!\n");
    		}
    		eXosip_unlock();
    		PRINT("after eXosip_default_action!\n");
			break;
		}
		case 403:
		{
		    break;
		}
		case 400:
		{
		    break;
		}
		case 431: // 无法识别
		{
		    PRINT("431 DevID Not Match Error!\n");
		    break;
		}
		default:
		{
		    break;
		}
	}
	osip_free(ru);
	
	return 0;
}

/**
 * 注册失败成功
 */
static int cacm_register_success(struct s_cacm *cacm, eXosip_event_t *ev)
{
    char *ru = NULL;
    osip_header_t *h = NULL;
	osip_uri_t *requri = osip_message_get_uri(ev->request);
	
	int status_code = osip_message_get_status_code(ev->response);
	
	osip_uri_to_str(requri,&ru);
	if (cacm->expires == 0)
	{
	    PRINT("Success from the server (%s) logout.(status_code = %d)\n", ru, status_code);
	}
	else
    {
	    PRINT("Registration on %s successful.(status_code = %d)\n", ru, status_code);
	}
	osip_free(ru);
	cacm->register_flag = 1;
	return 0;
}

/**
 * 其他请求
 */
static int cacm_other_request(struct s_cacm *cacm, eXosip_event_t *ev)
{
    int res = 0;
    unsigned char state_type = 0;
    char *tmp = NULL; 
	char buf[2048] = {0};
	char *recv_buf = NULL;
    char send_buf[4096] = {0};
    size_t recv_buf_len = 0;
    
	PRINT("cacm_other_request entry!\n");
	if (ev->request == NULL) 
	{
	    return NULL_ERR;
	}
	
	if (strcmp(ev->request->sip_method, "MESSAGE") == 0)
	{
	    PRINT("Receiving MESSAGE request!\n");
		//linphone_core_text_received(lc, ev);
		//eXosip_message_send_answer(ev->tid, 200, NULL);
	}
	else if (strcmp(ev->request->sip_method, "OPTIONS") == 0)
	{   
	    PRINT("Receiving OPTIONS request!\n");
		osip_message_t *options = NULL;
		osip_message_to_str(ev->request, &recv_buf, &recv_buf_len);
		
		if (recv_buf == NULL)
		{
		    eXosip_options_send_answer(ev->tid, 730, NULL);
		    return NULL_ERR;
		}
		
		PRINT("recv_buf = %s\n", recv_buf);
		if ((tmp = strstr(recv_buf, "i=")) == NULL)
		{
		    eXosip_options_send_answer(ev->tid, 730, NULL);
		    return NULL_ERR;
		}
		
		state_type = (unsigned char)(*(tmp + 2) - '0');
		PRINT("state_type = %d\n", state_type);
		
		
		switch (state_type)
        {
            case 1:
            {
                eXosip_options_send_answer(ev->tid, 200, NULL);
                return NULL_ERR;
            }
            case 2:
            {
                eXosip_options_send_answer(ev->tid, 200, NULL);
                return NULL_ERR;
            }
            case 3:
            {
                if ((res = cacm_tools.get_base_state(cacm, buf, sizeof(buf))) < 0)
        		{
        		    PERROR("get_base_state failed!\n");
        		    eXosip_options_send_answer(ev->tid, 500, NULL);
        		    return NULL_ERR;
        		}
                break;
            }
            #if CACM_STOP // 当值为1时编译
            case 4:
            {
                if (strstr(recv_buf, "c=closecacm") == NULL)
        		{
        		    strcpy(buf, "fail");
        		}
        		else
        		{
        	        strcpy(buf, "success");
        	    }
                break;
            }
            #endif
            default:
            {
                eXosip_options_send_answer(ev->tid, 200, NULL);
                return NULL_ERR;   
            }
        }
        snprintf(send_buf, sizeof(send_buf),
	        "v=0\r\n"
	        "o=%s\r\n"
	        "s=conversation\r\n"
	        "i=%d\r\n"
	        "c=%s\r\n", cacm->base_sn, state_type, buf);
	    
	    PRINT("send_buf = %s\n", send_buf);
	    eXosip_options_build_answer(ev->tid, 200, &options);
	    
		osip_message_set_body(options, send_buf, strlen(send_buf));
		osip_message_set_allow(options,"INVITE, ACK, BYE, CANCEL, OPTIONS, MESSAGE, SUBSCRIBE, NOTIFY, INFO");
		osip_message_set_content_type (options, "application/sdp");
		eXosip_options_send_answer(ev->tid, 200, options);
		
		osip_free(recv_buf);
		
		#if CACM_STOP
		if (memcmp(buf, "success", strlen("success")) == 0)
		{
		    cacm->exit_flag = 1;
		    if (cacm->pthread_heartbeat_id != 0)
		    {
		        pthread_cancel(cacm->pthread_heartbeat_id);
		    }
		    if (cacm->pthread_heartbeat_id != 0)
		    {
		        pthread_cancel(cacm->pthread_base_state_id);
		    }
		    if (cacm->pthread_heartbeat_id != 0)
		    {
		        pthread_cancel(cacm->pthread_real_time_event_id);
		    }
		}
		#endif
	}
	else if (strcmp(ev->request->sip_method, "WAKEUP") == 0) 
	{
	    PRINT("Receiving WAKEUP request!\n");
		eXosip_message_send_answer(ev->tid, 200, NULL);
	}
	else 
	{
		osip_message_to_str(ev->request, &recv_buf, &recv_buf_len);
		if (recv_buf != NULL)
		{
			PRINT("Unsupported request received:%s\n", recv_buf);
			osip_free(recv_buf);
		}
		/*answer with a 501 Not implemented*/
		eXosip_message_send_answer(ev->tid, 501, NULL);
	}
	return 0;
}

/**
 * 分析事件
 */
static int cacm_parse_status_code(eXosip_event_t *ev)
{
    if (ev == NULL)
    {
        PERROR("ev is NULL\n");
	    return NULL_ERR;
    }
	if (ev->response == NULL)
	{
	    PERROR("ev->response is NULL\n");
	    return NULL_ERR;
		
	}
	
	int status_code = osip_message_get_status_code(ev->response);
	PRINT("status_code = %d\n", status_code);
	
	switch(status_code)
	{
	    case 211:
	    {
	        break;
	    }
		case 401:
		case 407:
		{
		    break;
		}
		case 400:
		{
		    break;
		}
		case 404:
		{
		    break;
		}
		case 415:
		{
		    break;
		}
		case 431: // 无法识别
		{
		    PRINT("431 DevID Not Match Error!\n");
		    break;
		}
		case 480:
		{
		    break;
		}   	     	      	   	     	      	   	     	      	   	     	 
		case 486:
		{
		    break;
		}
		case 487:
		{
		    break;
		}
		case 600:
		{
		    break;
		}
		case 603:
		{
		    break;
		}
		default:
		{
		    break;
		}
	}
	return 0;
}

/**
 * 分析事件
 */
static int cacm_parse_event(struct s_cacm *cacm, eXosip_event_t *ev)
{
    int res = 0;
    if (ev == NULL)
    {
        PERROR("ev is NULL!\n");
        return NULL_ERR;
    }
    PRINT("ev->type = %d\n", ev->type);
    #if 0
    if ((res = cacm_parse_status_code(ev)) < 0)
    {
        PERROR("cacm_parse_status_code failed!\n");
        return res;
    }
    #else
    cacm_parse_status_code(ev);
    #endif
    
	switch(ev->type)
	{
	    case EXOSIP_REGISTRATION_NEW: // 一个新的注册（可能是作为服务器时使用）
		{
		    PRINT("announce new registration.\n");
			break;
		}
		case EXOSIP_REGISTRATION_SUCCESS:  // 用户已经成功注册
		{
		    PRINT("eXosip registeration success!\n");
			cacm_register_success(cacm, ev);	      	   	     	      	   	     	      	   	     	 
			break;
		}
	    case EXOSIP_REGISTRATION_FAILURE: // 用户没有注册
		{
		    PRINT("eXosip registeration failed!\n");
			cacm_register_failed(cacm, ev);	         	     	      	   	     	      	   	     	      	   	     	 
			break;
		}
		case EXOSIP_REGISTRATION_REFRESHED: // 注册已被更新
		{
		    PRINT("registration has been refreshed!\n");
			break;
		}
		case EXOSIP_REGISTRATION_TERMINATED: // UA终止注册（server）
		{
		    PRINT("UA is not registred any more!\n");
			break;
		}
		/******************************************************/
		case EXOSIP_CALL_INVITE: 
		{
		    PRINT("announce a new call.\n");
			break;
		}
		case EXOSIP_CALL_REINVITE: 
		{
		    PRINT("announce a new INVITE within call\n");
			break;
		}
		case EXOSIP_CALL_TIMEOUT:
		case EXOSIP_CALL_NOANSWER:
		{	
		    PRINT("announce no answer within the timeout!\n");
			break;
		}
		case EXOSIP_CALL_PROCEEDING: // 通过远程应用程序发布处理
		{
			PRINT("announce processing by a remote app!\n");  	     	      	   	     	      	   	     	      	   	     	 
			break;
		}
		case EXOSIP_CALL_RINGING: // 回铃
		{
			PRINT("announce ringback\n");  	   	     	      	   	     	      	   	     	 
			break;
		}
		case EXOSIP_CALL_ANSWERED: // 表明开始呼叫
		{
			PRINT("announce start of call.\n");    	      	   	     	      	   	     	      	   	     	 
			break;
		}
		case EXOSIP_CALL_REDIRECTED: // 表明是一个重定向
		{
		    PRINT("announce a redirection.\n");
			break;
		}
		case EXOSIP_CALL_REQUESTFAILURE: // 请求失败
		case EXOSIP_CALL_SERVERFAILURE:  // 服务器故障
		case EXOSIP_CALL_GLOBALFAILURE:  // 表明全程失败
		{	
		    PRINT("announce a request failure or announce a server failure or announce a global failure\n");
			break;
		}
		case EXOSIP_CALL_ACK:
		{	
		    PRINT("ACK received for 200ok to INVITE.\n");
			break;
		}
		case EXOSIP_CALL_CANCELLED: // 取消呼叫
		{
			PRINT("announce that call has been cancelled.\n");   	      	   	     	      	   	     	      	   	     	 
			break;
		}
		case EXOSIP_CALL_MESSAGE_NEW: // 表明一个新的呼入请求
		{
			PRINT("announce new incoming request.\n");	     	      	   	     	      	   	     	      	   	     	 
			break;
		}
		case EXOSIP_CALL_MESSAGE_PROCEEDING: 
		{
			PRINT("announce a 1xx for request.\n");	     	      	   	     	      	   	     	      	   	     	 
			break;
		}
		case EXOSIP_CALL_MESSAGE_ANSWERED: 
		{
			PRINT("announce a 200ok.\n");	     	      	   	     	      	   	     	      	   	     	 
			break;
		}
		case EXOSIP_CALL_MESSAGE_REDIRECTED: 
		case EXOSIP_CALL_MESSAGE_SERVERFAILURE:
		case EXOSIP_CALL_MESSAGE_GLOBALFAILURE:
		{
			PRINT("announce a failure.\n");	     	      	   	     	      	   	     	      	   	     	 
			break;
		}
		case EXOSIP_CALL_MESSAGE_REQUESTFAILURE:
		{
		    PRINT("EXOSIP_CALL_MESSAGE_REQUESTFAILURE\n");
			if (ev->did < 0 && ev->response && (ev->response->status_code == 407 || ev->response->status_code == 401))
			{
				 eXosip_default_action(ev);
			}
			break;
		}
		case EXOSIP_CALL_CLOSED: // 在此呼叫中收到挂机
		{
		    PRINT("a BYE was received for this call.\n");
		    break;
		}
		case EXOSIP_CALL_RELEASED: // 释放
		{
			PRINT("call context is cleared.\n");     	      	   	     	      	   	     	      	   	     	 
			break;
		}
		/******************************************************/
		case EXOSIP_MESSAGE_NEW: // 新的请求
		{
		    PRINT("announce new incoming request.\n");
		    cacm_other_request(cacm, ev);
			break;
		}
		case EXOSIP_MESSAGE_PROCEEDING:
		{
		    PRINT("announce a 1xx for request.\n");	     	      	   	     	      	   	     	      	   	     	 
			break;
		}
		case EXOSIP_MESSAGE_ANSWERED: 
		{
			PRINT("announce a 200ok.\n");  	      	   	     	      	   	     	      	   	     	 
			break;
		}
		case EXOSIP_MESSAGE_REQUESTFAILURE:
		{
		    PRINT("EXOSIP_MESSAGE_REQUESTFAILURE!\n");
		    if (ev->response && (ev->response->status_code == 407 || ev->response->status_code == 401)){
				/*the user is expected to have registered to the proxy, thus password is known*/
				eXosip_default_action(ev);
			}
			break;
		}
		case EXOSIP_MESSAGE_REDIRECTED: 
		case EXOSIP_MESSAGE_SERVERFAILURE:
		case EXOSIP_MESSAGE_GLOBALFAILURE:
		{
			PRINT("announce a failure.\n");	     	      	   	     	      	   	     	      	   	     	 
			break;
		}
		/******************************************************/
		case EXOSIP_SUBSCRIPTION_UPDATE:
		{
		    PRINT("announce incoming SUBSCRIBE.\n");
			break;
		}
		case EXOSIP_SUBSCRIPTION_CLOSED:
		{
		   	PRINT("announce end of subscription.\n");
			break;
		}
		case EXOSIP_SUBSCRIPTION_NOANSWER:
		{
		   	PRINT("announce no answer.\n");
			break;
		}
		case EXOSIP_SUBSCRIPTION_PROCEEDING:
		{
		    PRINT("announce a 1xx.\n");
		    break;
	    }
        case EXOSIP_SUBSCRIPTION_ANSWERED:
        {
		    PRINT("announce a 200ok.\n");
		    break;
	    }
        case EXOSIP_SUBSCRIPTION_REDIRECTED:
        {
		    PRINT("announce a redirection.\n");
		    break;
	    }
        case EXOSIP_SUBSCRIPTION_REQUESTFAILURE: 
        case EXOSIP_SUBSCRIPTION_SERVERFAILURE: 
        case EXOSIP_SUBSCRIPTION_GLOBALFAILURE: 
        {
		    PRINT("announce a request failure or announce a server failure or announce a global failure.\n");
		    break;
	    }
		case EXOSIP_SUBSCRIPTION_NOTIFY:
		{
		   	PRINT("announce new NOTIFY request.\n");
			break;
		}
		case EXOSIP_SUBSCRIPTION_RELEASED:
		{
		    PRINT("call context is cleared.\n");
			break;
	    }     
        case EXOSIP_IN_SUBSCRIPTION_NEW:
        {
		    PRINT("announce new incoming SUBSCRIBE.\n");
			break;
		}
        case EXOSIP_IN_SUBSCRIPTION_RELEASED:
        {
            PRINT("announce end of subscription.\n");
			break;
        }
        case EXOSIP_NOTIFICATION_NOANSWER:
        {
            PRINT("announce no answer.\n");
			break;
        }
        case EXOSIP_NOTIFICATION_PROCEEDING:
        {
            PRINT("announce a 1xx.\n");
			break;
        }
        case EXOSIP_NOTIFICATION_ANSWERED:
        {
            PRINT("announce a 200ok.\n");
			break;
        }
        case EXOSIP_NOTIFICATION_REDIRECTED:
        {
            PRINT("announce a redirection.\n");
			break;
        }
        case EXOSIP_NOTIFICATION_REQUESTFAILURE:
        case EXOSIP_NOTIFICATION_SERVERFAILURE:
        case EXOSIP_NOTIFICATION_GLOBALFAILURE:
        {
            PRINT("announce a request failure or announce a server failure or announce a global failure.\n");
			break;
        }
        case EXOSIP_EVENT_COUNT:
        {
            PRINT("MAX number of events.\n");
			break;
        }
		/******************************************************/
		default:
		{
			PRINT("Unhandled exosip event!\n");
			break;
		}
	}
	eXosip_event_free(ev);
	return res;
}

struct class_exevents exevents = 
{
    cacm_register_failed,
    cacm_register_success,
    cacm_other_request,
    cacm_parse_event,
    cacm_parse_status_code,
};
