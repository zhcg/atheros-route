#include "database_management.h"

/**
 * 数据插入
 */
static int sqlite3_insert(unsigned char columns_count, char (*columns_name)[30], char (*columns_value)[100], unsigned short *columns_value_len);

/**
 * 删除数据行
 */
static int sqlite3_delete_row(unsigned char columns_count, char (*columns_name)[30], char (*columns_value)[100]);

/**
 * 执行sql命令
 */
static int sqlite3_exec_cmd(char *sql, char *db_name);

/**
 * 执行sql select命令
 */
static int sqlite3_exec_select_cmd(char *sql, char *db_name, char *buf, unsigned short buf_len);

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
 * 数据查询
 */
static int sqlite3_select_table(unsigned char columns_count, char (*columns_name)[30], char *buf, unsigned short buf_len);

/**
 * 清空数据表
 */
static int sqlite3_clear_table();

/**
 * 初始化结构体
 */
struct class_database_management database_management = 
{   
    .exec_cmd = sqlite3_exec_cmd,
    .exec_select_cmd = sqlite3_exec_select_cmd,
    .clear = sqlite3_clear_table, 
    .insert = sqlite3_insert, 
    .delete_row = sqlite3_delete_row, 
    .del = sqlite3_clear, 
    .update = sqlite3_update, 
    .select = sqlite3_select,
    .select_table = sqlite3_select_table
};

/**
 * 数据插入
 */ 
int sqlite3_insert(unsigned char columns_count, char (*columns_name)[30], char (*columns_value)[100], unsigned short *columns_value_len)
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
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "para is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    
    if (sqlite3_open(common_tools.config->db, &db) != 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, sqlite3_errmsg(db), SQLITE_OPEN_ERR);
        return SQLITE_OPEN_ERR;
    }
    
    sprintf(sql, "insert into %s (", REGISTERTB);
    for (i = 0; i < columns_count; i++)
    {
        strncat(sql, columns_name[i], strlen(columns_name[i]));
        if ((columns_count > 1) && (i != (columns_count - 1)))
        {
            strcat(sql, ",");
        } 
    }
    strcat(sql, ") values (");
    for (i = 0; i < columns_count; i++)
    {
        strcat(sql, "\"");
        strncat(sql, columns_value[i], strlen(columns_value[i]));
        strcat(sql, "\"");   
        
        if ((columns_count > 1) && (i != (columns_count - 1)))
        {
            strcat(sql, ",");
        }  
    }
    strcat(sql, ")");
    printf("%s\n", sql);
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, sql, 0);
    if (sqlite3_exec(db, sql, NULL, NULL, &err_msg) != 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, err_msg, SQLITE_EXEC_ERR);
        sqlite3_close(db);
        return SQLITE_EXEC_ERR;
    }
    sqlite3_close(db);
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 删除数据行
 */
int sqlite3_delete_row(unsigned char columns_count, char (*columns_name)[30], char (*columns_value)[100])
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
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "para is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    
    if (sqlite3_open(common_tools.config->db, &db) != 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, sqlite3_errmsg(db), SQLITE_OPEN_ERR);
        return SQLITE_OPEN_ERR;
    }
    
    sprintf(sql, "delete from %s where ", REGISTERTB);
    for (i = 0; i < columns_count; i++)
    {
        strncat(sql, columns_name[i], strlen(columns_name[i]));
        
        strcat(sql, "=\"");
        strncat(sql, columns_value[i], strlen(columns_value[i]));
        strcat(sql, "\"");    
        
        if ((columns_count > 1) && (i != (columns_count - 1)))
        {
            strcat(sql, " and ");
        }  
    }
    printf("%s\n", sql);
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, sql, 0);
    if (sqlite3_exec(db, sql, NULL, NULL, &err_msg) != 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, err_msg, SQLITE_EXEC_ERR);
        sqlite3_close(db);
        return SQLITE_EXEC_ERR;
    }
    sqlite3_close(db);
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 执行sql命令
 */
int sqlite3_exec_cmd(char *sql, char *db_name)
{
    PRINT_STEP("entry...\n");
    int i = 0;
    sqlite3 *db;
    char *err_msg;
    
    if ((sql == NULL) || (db_name == NULL))
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "para is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    
    if (sqlite3_open(db_name, &db) != 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, sqlite3_errmsg(db), SQLITE_OPEN_ERR);
        return SQLITE_OPEN_ERR;
    }
    
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, sql, 0);
    if (sqlite3_exec(db, sql, NULL, NULL, &err_msg) != 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, err_msg, SQLITE_EXEC_ERR);
        sqlite3_close(db);
        return SQLITE_EXEC_ERR;
    }
    sqlite3_close(db);
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 执行sql select命令
 */
int sqlite3_exec_select_cmd(char *sql, char *db_name, char *buf, unsigned short buf_len)
{
    PRINT_STEP("entry...\n");
    int i = 0, j = 0;
    int index = 0;
    char **result_buf; //是 char ** 类型，两个*号
    int row_count = 0, column_count = 0;
    sqlite3 *db;
    char *err_msg;
    
    if ((sql == NULL) || (db_name == NULL) || (buf == NULL))
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "para is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    
    if (buf_len == 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", DATA_ERR);
        return DATA_ERR;
    }
    
    if (sqlite3_open(db_name, &db) != 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, sqlite3_errmsg(db), SQLITE_OPEN_ERR);
        return SQLITE_OPEN_ERR;
    }
    
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, sql, 0);
    if(sqlite3_get_table(db, sql, &result_buf, &row_count, &column_count, &err_msg) != 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, err_msg, SQLITE_GET_TABLE_ERR);
        sqlite3_free_table(result_buf);
        return SQLITE_GET_TABLE_ERR;
    }
    
    PRINT("row_count = %d, column_count = %d\n", row_count, column_count);
    index = column_count;
    for (i = 0; i < row_count; i++)
    {
        for (j = 0; j < column_count; j++)
        {
            strcat(buf, result_buf[index]);
            if (j != (column_count - 1))
            {
                strcat(buf, ",");
            }
            ++index;
        }
        strcat(buf, ";");
    }
    PRINT("buf = %s\n", buf);
    
    sqlite3_free_table(result_buf);
    sqlite3_close(db);
    PRINT_STEP("exit...\n");
    return 0;
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
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "para is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    
    if (sqlite3_open(common_tools.config->db, &db) != 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, sqlite3_errmsg(db), SQLITE_OPEN_ERR);
        return SQLITE_OPEN_ERR;
    }
    sprintf(sql, "update %s set ", TB);
    for (i = 0; i < columns_count; i++)
    {
        strncat(sql, columns_name[i], strlen(columns_name[i]));
        if (memcmp(columns_name[i], "register_state", strlen("register_state")) == 0)
        {
            strcat(sql, "=\"251\"");
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
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, err_msg, SQLITE_EXEC_ERR);
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
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "para is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    
    if (sqlite3_open(common_tools.config->db, &db) != 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, sqlite3_errmsg(db), SQLITE_OPEN_ERR);
        return SQLITE_OPEN_ERR;
    }
    
    sprintf(sql, "update %s set ", TB);
    for (i = 0; i < columns_count; i++)
    {
        strncat(sql, columns_name[i], strlen(columns_name[i]));
        
        strcat(sql, "=\"");
        strcat(sql, columns_value[i]);
        strcat(sql, "\"");    
        
        if (i != (columns_count - 1))
        {
            strcat(sql, ",");
        }  
    }
    printf("%s\n", sql);
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, sql, 0);
    if (sqlite3_exec(db, sql, NULL, NULL, &err_msg) != 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, err_msg, SQLITE_EXEC_ERR);
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
    char sql[1024] = {0};
    
    if (columns_count == 0)
    {
        return 0;
    }
    
    if ((columns_name == NULL) || (columns_value == NULL))
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "para is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    PRINT("common_tools.config->db = %s\n", common_tools.config->db);
    if (sqlite3_open(common_tools.config->db, &db) != 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, sqlite3_errmsg(db), SQLITE_OPEN_ERR);
        return SQLITE_OPEN_ERR;
    }
    PRINT("after sqlite3_open\n");
    strcpy(sql, "select ");
    for (i = 0; i < columns_count; i++)
    {
        PRINT("sql = %s, columns_name[i] = %s\n", sql, columns_name[i]);
        strncat(sql, columns_name[i], strlen(columns_name[i]));
        if (i != (columns_count - 1))
        {
            strncat(sql, ",", 1);
        }  
    }
    PRINT("sql = %s\n", sql);
    strcat(sql, " from ");
    strcat(sql, TB);
    PRINT("sql = %s\n", sql);
    if(sqlite3_get_table(db, sql, &result_buf, &row_count, &column_count, &err_msg) != 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, err_msg, SQLITE_GET_TABLE_ERR);
        sqlite3_free_table(result_buf);
        return SQLITE_GET_TABLE_ERR;
    }
    
    index = column_count;
    for (i = 0; i < column_count; i++)
    {
        if (strcmp(result_buf[index], "") == 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "no data", NULL_ERR);
            return NO_RECORD_ERR;    
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
 * 数据查询
 */
int sqlite3_select_table(unsigned char columns_count, char (*columns_name)[30], char *buf, unsigned short buf_len)
{
    PRINT_STEP("entry...\n");   
    int i = 0, j = 0;
    int index = 0;
    char **result_buf; //是 char ** 类型，两个*号
    int row_count = 0, column_count = 0;
    sqlite3 *db;
    char *err_msg;
    char sql[128] = {0};
    
    if ((columns_count > 0) && (columns_name == NULL))
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "para is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    
    if (buf == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "para is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    
    if (sqlite3_open(common_tools.config->db, &db) != 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, sqlite3_errmsg(db), SQLITE_OPEN_ERR);
        return SQLITE_OPEN_ERR;
    }
    
    strcpy(sql, "select ");
    if (columns_count == 0)
    {
        strcat(sql, "*");
    }
    else 
    {
        for (i = 0; i < columns_count; i++)
        {
            strncat(sql, columns_name[i], strlen(columns_name[i]));
            if (i != (columns_count - 1))
            {
                strncat(sql, ",", 1);
            }  
        }    
    }
    
    strcat(sql, " from ");
    strcat(sql, REGISTERTB);
    
    if(sqlite3_get_table(db, sql, &result_buf, &row_count, &column_count, &err_msg) != 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, err_msg, SQLITE_GET_TABLE_ERR);
        sqlite3_free_table(result_buf);
        return SQLITE_GET_TABLE_ERR;
    }
    
    PRINT("row_count = %d, column_count = %d\n", row_count, column_count);
    index = column_count;
    for (i = 0; i < row_count; i++)
    {
        for (j = 0; j < column_count; j++)
        {
            strcat(buf, result_buf[index]);
            if (j != (column_count - 1))
            {
                strcat(buf, ",");
            }
            ++index;
        }
        strcat(buf, ";");
    }
    PRINT("buf = %s\n", buf);
    
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
    unsigned char columns_count = 19;
    char columns_name[19][30] = {"base_sn", "base_mac", "base_ip", "base_user_name", "base_password",
        "pad_sn", "pad_mac", "pad_ip", "pad_user_name", "pad_password", "sip_ip", "sip_port", "heart_beat_cycle", 
        "business_cycle", "ssid_user_name", "ssid_password", "device_token", "position_token", "register_state"};
    if ((res = sqlite3_clear(columns_count, columns_name)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_clear failed!", res);
        return res;
    }
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "table clear success!", 0);
    return 0;
}
