#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>

#include "mqtt_app.h"
#include "dbgPrint.h"
#include "debug_tool.h"

static void test_init(void);
static int test_start(int argc, char *argv[], char *ret_msg);

int main(int argc, char *argv[]) 
{
    debugtool_init();
    test_init();
    mqtt_demo_entry(argc, argv);

    while(1)
    {
        sleep(3);
    }
    return 0;
}



static int test_start(int argc, char *argv[], char *ret_msg)
{
    int ret = 0;
    int devID = 0xABCD;

    if (argc < 2)
    {
        ret_msg += sprintf(ret_msg, "Error: cmd format is mtdbg reset <devID>\n");
        return DEBUG_RET_PARAM_ERR;
    }

    /*get the parameter*/
    devID = atoi(argv[1]);

    if (argc >= 3)
    {
        ret_msg += sprintf(ret_msg, "Error: too many param.\n");
        return DEBUG_RET_PARAM_ERR;
    }

    if (devID != 0)
    {
        dbg_print(PRINT_LEVEL_DEBUG, "reset system, devid = %d\n", devID);
        ret_msg += sprintf(ret_msg, "reset system devid = %d\n",devID);
    }
    return ret;
}

void test_init(void)
{
    debug_tool_entry entry[] = {{"test", "auto-test the basic functions, param is lock device ID", test_start}};
    debugtool_register(entry, sizeof(entry)/sizeof(debug_tool_entry));
    return ;
}
