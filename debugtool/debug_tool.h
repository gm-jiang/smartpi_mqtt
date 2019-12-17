#ifndef __DEBUG_TOOL_H__
#define __DEBUG_TOOL_H__


#define BUF_MAX_LEN 4096
#define DEBUG_ENTRY_MAX_NUM 50
#define DEBUG_PARAM_MAX_NUM 20
#define DEBUG_CMD_MAX_LEN 10
#define DEBUG_HELP_INFO_MAX_LEN 256

enum
{
    DEBUG_RET_SUCCEED,
    DEBUG_RET_CMD_ERR,
    DEBUG_RET_PARAM_ERR,
};

typedef int(*pDbgProcfunc)(int argc, char *argv[], char *ret_msg);

typedef struct
{
    char cmd[DEBUG_CMD_MAX_LEN];
    char help_info[DEBUG_HELP_INFO_MAX_LEN];
    pDbgProcfunc pfnDbgProc;
}debug_tool_entry;

int debugtool_init();
void debugtool_register(debug_tool_entry entry[], int num);

#endif



