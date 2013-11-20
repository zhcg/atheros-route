#include "nvram_interface.h"

/**
 * 数据插入
 */
static int nvram_insert(unsigned char flash_block_index, unsigned char columns_count, char (*columns_name)[30], char (*columns_value)[100], unsigned short *columns_value_len);

/**
 * 数据置空
 */
static int nvram_delete(unsigned char flash_block_index, unsigned char columns_count, char (*columns_name)[30]);

/**
 * 数据修改
 */
static int nvram_update(unsigned char flash_block_index, unsigned char columns_count, char (*columns_name)[30], char (*columns_value)[100], unsigned short *columns_value_len);

/**
 * 数据查询
 */
static int nvram_select(unsigned char flash_block_index, unsigned char columns_count, char (*columns_name)[30], char (*columns_value)[100]);

/**
 * 清空数据表
 */
static int nvram_clear_all_data();

struct class_nvram_interface nvram_interface = 
{
    .insert = nvram_insert, 
    .del = nvram_delete, 
    .update = nvram_update, 
    .select = nvram_select, 
    .clear = nvram_clear_all_data,
    .sem_key = 1021,
};
/**
 * 数据插入
 */ 
int nvram_insert(unsigned char flash_block_index, unsigned char columns_count, char (*columns_name)[30], char (*columns_value)[100], unsigned short *columns_value_len)
{
    PRINT_STEP("entry...\n");
    int res = 0;
    res = nvram_update(flash_block_index, columns_count, columns_name, columns_value, columns_value_len);
    PRINT("_________________________________\n");
    PRINT_STEP("exit...\n");
    return res;
}

/**
 * 数据置空
 */
int nvram_delete(unsigned char flash_block_index, unsigned char columns_count, char (*columns_name)[30])
{
    PRINT_STEP("entry...\n");
    int i = 0;
    if (columns_count == 0)
    {
        return 0;
    }
    
    if (columns_name == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "para is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    
    nvram_init(flash_block_index);
    for (i = 0; i < columns_count; i++)
    {
        if (memcmp(columns_name[i], "register_state", strlen("register_state")) == 0)
        {
            unsigned char *value = "251";
            if (nvram_bufset(flash_block_index, columns_name[i], value) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_bufset falied!", NVRAM_SET_ERR);
                return NVRAM_SET_ERR;
            } 
        }
        else
        {
            if (nvram_bufset(flash_block_index, columns_name[i], "") < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_bufset falied!", NVRAM_SET_ERR);
                return NVRAM_SET_ERR;
            }
        }
    }
    /*
    if (nvram_commit(flash_block_index) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_commit falied!", NVRAM_COMMIT_ERR);
        return NVRAM_COMMIT_ERR;
    }
    */
    nvram_close(flash_block_index);
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 数据修改
 */
int nvram_update(unsigned char flash_block_index, unsigned char columns_count, char (*columns_name)[30], char (*columns_value)[100], unsigned short *columns_value_len)
{
    PRINT_STEP("entry...\n");
    PRINT("%s %p %p %p columns_count = %d\n", columns_name, columns_name, columns_value, columns_value_len, columns_count);
    int i = 0;
    if (columns_count == 0)
    {
        return 0;
    }
    
    if ((columns_name == NULL) || (columns_value == NULL))
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "para is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    int sem_id = 0;
    int res = 0;
    struct sembuf semopbuf;
    if ((sem_id = semget(nvram_interface.sem_key, 1, IPC_CREAT | 0666)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "semget failed!", SEMGET_ERR);
        return SEMGET_ERR;
    }
    
    if (semctl(sem_id, 0, SETVAL, 1) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "semctl failed!", SEMCTL_ERR);
        return SEMCTL_ERR;
    }
    semopbuf.sem_flg = SEM_UNDO;
    semopbuf.sem_num = 0;
    semopbuf.sem_op = -1;
    
    if (semop(sem_id, &semopbuf, 1) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "semop failed!", SEMOP_ERR);
        return SEMOP_ERR;
    }
    if (pthread_mutex_lock(&nvram_interface.mutex) != 0)
    {
        PERROR("pthread_mutex_lock failed!\n");
        return PTHREAD_LOCK_ERR;
    }
    PRINT("_______________\n");
	#if SMART_RECOVERY == 1
    /* 
     * 读取第三分区数据backup标志：0：不需要恢复，此数据插入第二分区的同时也插入第三分区
     * 1：此数据为恢复数据不需要插入第三分区，然后更改此标志
     */
    if (flash_block_index == RT5350_FREE_SPACE)
    {
        // 比较recovery_freespace标志是否为1
	    nvram_init(RT5350_BACKUP_SPACE);
        if (memcmp((void *)nvram_bufget(RT5350_BACKUP_SPACE, "recovery_freespace"), "1", 1) == 0)
        {
            PRINT("This data does not need backup!");
        }
        else
        {
            for (i = 0; i < columns_count; i++)
            {
                if (nvram_bufset(RT5350_BACKUP_SPACE, columns_name[i], columns_value[i]) < 0)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_bufset falied!", NVRAM_SET_ERR);
                    res = NVRAM_SET_ERR;
                    goto EXIT;
                } 
            }
            /*
            if (nvram_commit(RT5350_BACKUP_SPACE) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_commit falied!", NVRAM_COMMIT_ERR);
                res = NVRAM_COMMIT_ERR;
                goto EXIT;
            }
            */
        }
        nvram_close(RT5350_BACKUP_SPACE);
    }
	#endif

    nvram_init(flash_block_index);
    for (i = 0; i < columns_count; i++)
    {
        PRINT("columns_name[%d] = %s, columns_value[i] = %s, flash_block_index = %d\n", i, columns_name[i], columns_value[i], flash_block_index);
        if (nvram_bufset(flash_block_index, columns_name[i], columns_value[i]) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_bufset falied!", NVRAM_SET_ERR);
            res = NVRAM_SET_ERR;
            goto EXIT;
        } 
    }
    
    /*
    if (nvram_commit(flash_block_index) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_commit falied!", NVRAM_COMMIT_ERR);
        res = NVRAM_COMMIT_ERR;
        goto EXIT;
    }
    */
    nvram_close(flash_block_index);
    res = 0;

EXIT:
    PRINT("_________________________________\n");
    if (pthread_mutex_unlock(&nvram_interface.mutex) != 0)
    {
        PERROR("pthread_mutex_unlock failed!\n");
        return PTHREAD_UNLOCK_ERR;
    }
    PRINT("_________________________________\n");
    semopbuf.sem_op = 1;
    
    if (semop(sem_id, &semopbuf, 1) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "semop failed!", SEMOP_ERR);
        return SEMOP_ERR;
    }
    PRINT_STEP("exit...\n");
    return res;
}

/**
 * 数据查询
 */
int nvram_select(unsigned char flash_block_index, unsigned char columns_count, char (*columns_name)[30], char (*columns_value)[100])
{
    PRINT_STEP("entry...\n");   
    int i = 0;
    if (columns_count == 0)
    {
        return 0;
    }
    
    if ((columns_name == NULL) || (columns_value == NULL))
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "para is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    
    int sem_id = 0;
    struct sembuf semopbuf;
    if ((sem_id = semget(nvram_interface.sem_key, 1, IPC_CREAT | 0666)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "semget failed!", SEMGET_ERR);
        return SEMGET_ERR;
    }
    
    if (semctl(sem_id, 0, SETVAL, 1) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "semctl failed!", SEMCTL_ERR);
        return SEMCTL_ERR;
    }
    semopbuf.sem_flg = SEM_UNDO;
    semopbuf.sem_num = 0;
    semopbuf.sem_op = -1;
    
    if (semop(sem_id, &semopbuf, 1) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "semop failed!", SEMOP_ERR);
        return SEMOP_ERR;
    }
    
    pthread_mutex_lock(&nvram_interface.mutex);    
    // nvram_close主要完成把数据从cache写入flash，并释放资源
    //nvram_close(flash_block_index);
    
    // nvram_init主要完成从flash中读取数据放入cache
    nvram_init(flash_block_index);
    // 当没有此项记录是nvram_bufget返回""
    for (i = 0; i < columns_count; i++)
    {
        strcpy(columns_value[i], (char *)nvram_bufget(flash_block_index, columns_name[i])) ;
        /*
        if (strcmp(columns_value[i], "") == 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "no data", NULL_ERR);
            return NULL_ERR;
        }
        */
    }
    
    nvram_close(flash_block_index);
    
    pthread_mutex_unlock(&nvram_interface.mutex);
    
    semopbuf.sem_op = 1;
    
    if (semop(sem_id, &semopbuf, 1) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "semop failed!", SEMOP_ERR);
        return SEMOP_ERR;
    }
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 清空数据表
 */
int nvram_clear_all_data()    
{
    int res = 0;
    unsigned char columns_count = 18;
    char columns_name[18][30] = {"base_sn", "base_mac", "base_ip", "base_user_name", "base_password",
        "pad_sn", "pad_mac", "pad_ip", "pad_user_name", "pad_password", "sip_ip", "sip_port", "heart_beat_cycle", 
        "business_cycle", "ssid_user_name", "ssid_password", "token", "register_state"};
    if ((res = nvram_delete(RT5350_FREE_SPACE, columns_count, columns_name)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_delete failed!", res);
        return res;
    }
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "table clear success!", 0);
    return 0;
}
