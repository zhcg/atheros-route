#ifndef _SW_SHELL_LIB_H
#define _SW_SHELL_LIB_H

#ifdef __cplusplus
extern "C" {
#endif    

int next_cmd(char *out_cmd);
ssize_t getline(char **lineptr, size_t *n, FILE *stream);


#ifdef __cplusplus
}
#endif                          /* __cplusplus */

#endif                          /* _SW_SHELL_LIB_H */

