/*************************************************************************
    > File Name: safe_strategy.h
    > Author: yangjilong
    > mail1: yangjilong65@163.com
    > mail2: yang.jilong@handaer.com
    > Created Time: 2013年09月23日 星期一 11时01分02秒
**************************************************************************/
#ifndef _SAFE_STRATEGY_H_ 
#define _SAFE_STRATEGY_H_ 
#include "common_tools.h"

struct class_safe_strategy
{
    int (* rsa_encrypt)(char *str, unsigned short str_len, char *key, char *dest);
    int (* rsa_decrypt)(char *str, unsigned short str_len, char *key, char *dest);
    int (* des3_encrypt)(char *str, unsigned short str_len, char *key, char *dest);
    int (* des3_decrypt)(char *str, unsigned short str_len, char *key, char *dest);
    
    int (* make_3des_key)(unsigned char key_len, char *key);
};

extern struct class_safe_strategy safe_strategy;
#endif
