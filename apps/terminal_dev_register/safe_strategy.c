/*************************************************************************
    > File Name: safe_strategy.c
    > Author: yangjilong
    > mail1: yangjilong65@163.com
    > mail2: yang.jilong@handaer.com
    > Created Time: 2013年09月23日 星期一 11时00分48秒
**************************************************************************/
#include "safe_strategy.h"

/**
 * 非对称加密(RSA)
 */
static int rsa_encrypt(char *str, unsigned short str_len, char *key, char *dest);

/**
 * 非对称解密(RSA)
 */
static int rsa_decrypt(char *str, unsigned short str_len, char *key, char *dest);

/**
 * 对称加密(3des)
 */
static int des3_encrypt(char *str, unsigned short str_len, char *key, char *dest);

/**
 * 对称加密(3des)
 */
static int des3_decrypt(char *str, unsigned short str_len, char *key, char *dest);

/**
 * 生成临时密钥(3des)
 */
int make_3des_key(unsigned char key_len, char *key);

struct class_safe_strategy safe_strategy = 
{
    rsa_encrypt, rsa_decrypt, des3_encrypt, des3_decrypt
};


/**
 * 非对称加密(RSA)
 */
int rsa_encrypt(char *str, unsigned short str_len, char *key, char *dest)
{
    if (str == NULL)
    {
        PERROR("str is NULL!\n");
        return NULL_ERR;
    }
    
    if (dest == NULL)
    {
        PERROR("dest is NULL!\n");
        return NULL_ERR;
    }
    
    memcpy(dest, str, str_len);
    return 0;
}

/**
 * 非对称解密(RSA)
 */
int rsa_decrypt(char *str, unsigned short str_len, char *key, char *dest)
{
    if (str == NULL)
    {
        PERROR("str is NULL!\n");
        return NULL_ERR;
    }
    
    if (dest == NULL)
    {
        PERROR("dest is NULL!\n");
        return NULL_ERR;
    }
    
    memcpy(dest, str, str_len);
    return 0;
}

/**
 * 对称加密(3des)
 */
int des3_encrypt(char *str, unsigned short str_len, char *key, char *dest)
{
    if (str == NULL)
    {
        PERROR("str is NULL!\n");
        return NULL_ERR;
    }
    
    if (dest == NULL)
    {
        PERROR("dest is NULL!\n");
        return NULL_ERR;
    }
    
    memcpy(dest, str, str_len);
    return 0;
}

/**
 * 对称加密(3des)
 */
int des3_decrypt(char *str, unsigned short str_len, char *key, char *dest)
{
    if (str == NULL)
    {
        PERROR("str is NULL!\n");
        return NULL_ERR;
    }
    
    if (dest == NULL)
    {
        PERROR("dest is NULL!\n");
        return NULL_ERR;
    }
    
    memcpy(dest, str, str_len);
    return 0;
}

/**
 * 生成临时密钥(3des)
 */
int make_3des_key(unsigned char key_len, char *key)
{
    if (key_len == 0)
    {
        PERROR("key_len is zero!\n");
        return LENGTH_ERR;
    }
    
    if (key == NULL)
    {
        PERROR("dest is NULL!\n");
        return NULL_ERR;
    }
    
    strcpy(key, "00000000");
    return 0;
}
