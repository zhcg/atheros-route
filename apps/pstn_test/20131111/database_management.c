#include "database_management.h"

/**
 * 数据插入
 */
static int sqlite3_insert(unsigned char columns_count, char (*columns_name)[30], char (*columns_value)[100], unsigned short *columns_value_len);

/**
 * 数据置空
 */
static int sqlite3_clear(unsigned char columns_count, char (*columns_name)[30]);

/**
 * 数据修改
 */
static int sqlite3_update(unsigned char columns_count, char (*columns_name)[30], char (*columns_value)[100], unsigned short *columns_value_len);

/**
 * 数据查询
 */
static int sqlite3_select(unsigned char columns_count, char (*columns_name)[30], char (*columns_value)[100]);

/**
 * 清空数据表
 */
static int sqlite3_clear_table();

/**
 * 初始化结构体
 */
struct class_database_management database_management = 
{
    sqlite3_clear_table, sqlite3_insert, sqlite3_clear, sqlite3_update, sqlite3_select
};

/**
 * 数据插入
 */ 
int sqlite3_insert(unsigned char columns_count, char (*columns_name)[30], char (*columns_value)[100], unsigned short *columns_value_len)
{
    PRINT_STEP("entry...\n");
    int res = 0;
    res = sqlite3_update(columns_count, columns_name, columns_value, columns_value_len);
    PRINT_STEP("exit...\n");
    return res;
}

/**
 * 数据置空
 */
int sqlite3_clear(unsigned char columns_count, char (*columns_name)[30])
{
    PRINT_STEP("entry...\n");
    int i = 0;
    sqlite3 *db;
    char *err_msg;
    char sql[300] = {0};
    
    if (columns_count == 0)
    {
        return 0;
    }
    
    if (columns_name == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "para is NULL!");
        return NULL_ERR;
    }
    
    if (sqlite3_open(common_tools.config->db, &db) != 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, sqlite3_errmsg(db));
        return SQLITE_OPEN_ERR;
    }
    sprintf(sql, "update %s set ", TB);
    for (i = 0; i < columns_count; i++)
    {
        strncat(sql, columns_name[i], strlen(columns_name[i]));
        if (memcmp(columns_name[i], "register_state", strlen("register_state")) == 0)
        {
            strcat(sql, "=251");
        }
        else
        {
            strcat(sql, "=\"\"");
        }        
        
        
        if (i != (columns_count - 1))
        {
            strcat(sql, ",");
        }
    }
    PRINT("sql = %s\n", sql);
    if (sqlite3_exec(db, sql, NULL, NULL, &err_msg) != 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, err_msg);
        sqlite3_close(db);
        return SQLITE_EXEC_ERR;
    }
    sqlite3_close(db);
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 数据修改
 */
int sqlite3_update(unsigned char columns_count, char (*columns_name)[30], char (*columns_value)[100], unsigned short *columns_value_len)
{
    PRINT_STEP("entry...\n");
    int i = 0;
    sqlite3 *db;
    char *err_msg;
    char sql[1024] = {0};
    unsigned long value = 0;
    if (columns_count == 0)
    {
        return 0;
    }
    
    if ((columns_name == NULL) || (columns_value == NULL))
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "para is NULL!");
        return NULL_ERR;
    }
    
    if (sqlite3_open(common_tools.config->db, &db) != 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, sqlite3_errmsg(db));
        return SQLITE_OPEN_ERR;
    }
    
    sprintf(sql, "update %s set ", TB);
    for (i = 0; i < columns_count; i++)
    {
        strncat(sql, columns_name[i], strlen(columns_name[i]));
        
        if (memcmp(columns_name[i], "register_state", strlen("register_state")) != 0)
        {
            strcat(sql, "=\"");
            strncat(sql, columns_value[i], (size_t)columns_value_len[i]);
            strcat(sql, "\"");
        }
        else
        {
            memcpy(&value, columns_value[i], sizeof(value));
            strcat(sql, "=");
            memset(columns_value[i], 0, sizeof(columns_value[i]));
            common_tools.long_to_str(value, columns_value[i], sizeof(columns_value[i]));
            strncat(sql, columns_value[i], strlen(columns_value[i]));
        }        
        
        if (i != (columns_count - 1))
        {
            strcat(sql, ",");
        }  
    }
    printf("%s\n", sql);
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, sql);
    if (sqlite3_exec(db, sql, NULL, NULL, &err_msg) != 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, err_msg);
        sqlite3_close(db);
        return SQLITE_EXEC_ERR;
    }
    sqlite3_close(db);
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 数据查询
 */
int sqlite3_select(unsigned char columns_count, char (*columns_name)[30], char (*columns_value)[100])
{
    PRINT_STEP("entry...\n");   
    int i = 0;
    int index = 0;
    char **result_buf; //是 char ** 类型，两个*号
    int row_count = 0, column_count = 0;
    sqlite3 *db;
    char *err_msg;
    char sql[128] = {0};
    
    if (columns_count == 0)
    {
        return 0;
    }
    
    if ((columns_name == NULL) || (columns_value == NULL))
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "para is NULL!");
        return NULL_ERR;
    }
    
    if (sqlite3_open(common_tools.config->db, &db) != 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, sqlite3_errmsg(db));
        return SQLITE_OPEN_ERR;
    }
    
    strcpy(sql, "select ");
    for (i = 0; i < columns_count; i++)
    {
        strncat(sql, columns_name[i], strlen(columns_name[i]));
        if (i != (columns_count - 1))
        {
            strncat(sql, ",", 1);
        }  
    }
    strcat(sql, " from ");
    strcat(sql, TB);
    
    if(sqlite3_get_table(db, sql, &result_buf, &row_count, &column_count, &err_msg) != 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, err_msg);
        sqlite3_free_table(result_buf);
        return SQLITE_GET_TABLE_ERR;
    }
    
    index = column_count;
    for (i = 0; i < column_count; i++)
    {
        if (strcmp(result_buf[index], "") == 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "no data");
            return NULL_ERR;    
        }
        memcpy(columns_value[i], result_buf[index], strlen(result_buf[index]));
        ++index;
    }
    
    sqlite3_free_table(result_buf);
    sqlite3_close(db);
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 清空数据表
 */
int sqlite3_clear_table()    
{
    int res = 0;
    unsigned char columns_count = 18;
    char columns_name[18][30] = {"base_sn", "base_mac", "base_ip", "base_user_name", "base_password",
        "pad_sn", "pad_mac", "pad_ip", "pad_user_name", "pad_password", "sip_ip", "sip_port", "heart_beat_cycle", 
        "business_cycle", "ssid_user_name", "ssid_password", "token", "register_state"};
    if ((res = sqlite3_clear(columns_count, columns_name)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_clear failed!");
        return res;
    }
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "table clear success!");
    return 0;
}
