/*
 * Copyright (c) 2016 Beijing Sankuai Inc.
 *
 * The right to copy, distribute, modify, or otherwise make use
 * of this software may be licensed only pursuant to the terms
 * of an applicable Beijing Sankuai license agreement.
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <pthread.h>
#include "debug_tool.h"
#include "dbgPrint.h"

#define MTDBG_SOCKET "/tmp/mtdbg.socket"

debug_tool_entry debug_entry[DEBUG_ENTRY_MAX_NUM];
int debug_entry_num = 0;

void debugtool_register(debug_tool_entry entry[], int num)
{
    int i;
    debug_tool_entry *cur_entry;

    dbg_print(PRINT_LEVEL_WARNING, "debugtool_register num=%d, current_num=%d\n",num,debug_entry_num);
    for (i = 0; i < num; i++)
    {
        if (debug_entry_num == DEBUG_ENTRY_MAX_NUM)
        {
            dbg_print(PRINT_LEVEL_ERROR, "Error: debug entry is full.\r\n");
            return;
        }
        cur_entry = &debug_entry[debug_entry_num++];
        strcpy(cur_entry->cmd, entry[i].cmd);
        strcpy(cur_entry->help_info, entry[i].help_info);
        cur_entry->pfnDbgProc = entry[i].pfnDbgProc;
    }

    return ;
}

debug_tool_entry *debugtool_find_entry(char *cmd)
{
    int i;
    for (i = 0; i < debug_entry_num; i++)
        if (strcmp(debug_entry[i].cmd, cmd) == 0)
            return &debug_entry[i];

    return NULL;
}

void debugtool_get_help_info(char *ret_msg)
{
    int i;
    dbg_print(PRINT_LEVEL_INFO_LOWLEVEL, "debug_entry_num = %d\n", debug_entry_num);
    for (i = 0; i < debug_entry_num; i++)
    {
        ret_msg += sprintf(ret_msg, "%-10s - ", debug_entry[i].cmd);
        ret_msg += sprintf(ret_msg, "%s\n", debug_entry[i].help_info);
    }
    return ;
}

/*mtdbg {cmd | help} [para1 para2]*/
int debugtool_exec(int argc,  char *argv[], char *ret_msg)
{
    char *cmd_str;
    char **sub_argv = &argv[1];
    int sub_argc = argc - 1;
    debug_tool_entry *pentry;

    if (argc < 2)
        return DEBUG_RET_CMD_ERR;

    cmd_str = argv[1];
    if (strcmp(cmd_str, "help") == 0)
    {
        debugtool_get_help_info(ret_msg);
        return DEBUG_RET_SUCCEED;
    }

    pentry = debugtool_find_entry(cmd_str);
    if (pentry == NULL)
        return DEBUG_RET_CMD_ERR;

    if (pentry->pfnDbgProc == NULL)
        return DEBUG_RET_CMD_ERR;

    return pentry->pfnDbgProc(sub_argc, sub_argv, ret_msg);
}

void *debugtool_task(void *argument)
{
    int lsn_fd, apt_fd;
    struct sockaddr_un srv_addr;
    struct sockaddr_un clt_addr;
    socklen_t clt_len;
    int ret;
    int i,k;
    char recv_buf[BUF_MAX_LEN];
    char send_buf[BUF_MAX_LEN];
    char *ret_msg = send_buf+sizeof(int);
    int ret_code;
    char *pbuf;
    int argc;
    char *argv[DEBUG_PARAM_MAX_NUM];
    int rcv_num;
    int send_num;

    //create socket
RESTART_DEBUG_TOOLS:
    lsn_fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if(lsn_fd < 0)
    {
        dbg_print(PRINT_LEVEL_ERROR, "can't create communication socket!\n\
                  socket failed reason:%s!\n",strerror(errno));
        return NULL;
    }

    //create local IP and PORT
    srv_addr.sun_family = AF_UNIX;
    strncpy(srv_addr.sun_path, MTDBG_SOCKET, sizeof(srv_addr.sun_path) - 1);
    unlink(MTDBG_SOCKET);

    //bind sockfd and sockaddr
    ret = bind(lsn_fd, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
    if(ret == -1)
    {
        dbg_print(PRINT_LEVEL_ERROR, "can't bind local sockaddr!\n\
                  bind failed reason:%s!\n",strerror(errno));
        close(lsn_fd);
        unlink(MTDBG_SOCKET);
        return NULL;
    }

    //listen lsn_fd, try listen 1
    ret = listen(lsn_fd, 1);
    if(ret == -1)
    {
        dbg_print(PRINT_LEVEL_ERROR, "can't listen client connect request.\n\
                  listen failed reason:%s!\n",strerror(errno));
        close(lsn_fd);
        return NULL;
    }

    dbg_print(PRINT_LEVEL_WARNING, "start debug tool.\n");

    clt_len = sizeof(clt_addr);
    while(1)
    {
        apt_fd = accept(lsn_fd, (struct sockaddr*)&clt_addr, &clt_len);
        if(apt_fd < 0)
        {
            //EINTR means returned when interrupted by signal accidentally,we can go on to accept other socket
            if(EINTR == errno)
            {
                continue;
            }
            else
            {
                dbg_print(PRINT_LEVEL_ERROR, "can't listen client connect request.\n\
                          accept failed reason:%s", strerror(errno));
                close(lsn_fd);
                goto RESTART_DEBUG_TOOLS;
            }
        }
        dbg_print(PRINT_LEVEL_INFO_LOWLEVEL, "debug tool: received a connection\n");

        memset(recv_buf, 0, BUF_MAX_LEN);
        rcv_num = read(apt_fd, recv_buf, BUF_MAX_LEN);
        dbg_print(PRINT_LEVEL_INFO_LOWLEVEL, "debug tool: received data %d bytes.\n", rcv_num);

        memcpy(&argc, recv_buf, sizeof(int));
        if(argc > DEBUG_PARAM_MAX_NUM)
        {
            dbg_print(PRINT_LEVEL_ERROR, "too many param.\n");
            close(apt_fd);
            continue;
        }
        pbuf = recv_buf + sizeof(int);
        memset(argv,0,sizeof(argv));
        for (i = 0; i < argc; i++)
        {
            argv[i] = malloc(strlen(pbuf) + 1);
            if (argv[i] == NULL)
            {
                dbg_print(PRINT_LEVEL_ERROR, "debug tool memery alloc failed\n");
                for (k = 0; k < i; k++)
                {
                    free(argv[k]);
                }
                close(apt_fd);
                break;
            }
            strcpy(argv[i], pbuf);
            pbuf += strlen(pbuf) + 1;
        }
        if(NULL == argv[argc-1])
        {
            close(apt_fd);
            continue;
        }

        dbg_print(PRINT_LEVEL_INFO_LOWLEVEL, "argc =  %d\n", argc);
        for (i = 0; i < argc; i++)
        {
            dbg_print(PRINT_LEVEL_INFO_LOWLEVEL, "%s ", argv[i]);
        }
        dbg_print(PRINT_LEVEL_INFO_LOWLEVEL, "\n");

        memset(send_buf, 0, BUF_MAX_LEN);
        ret_code = debugtool_exec(argc, argv, ret_msg);

        dbg_print(PRINT_LEVEL_INFO_LOWLEVEL, "debug tool ret_code = %d\n", ret_code);
        dbg_print(PRINT_LEVEL_INFO_LOWLEVEL, "debug tool ret_msg = %s\n", ret_msg);

        for (i = 0; i < argc; i++)
        {
            free(argv[i]);
        }

        memcpy(send_buf, &ret_code, sizeof(int));
        send_num = sizeof(int) + strlen(ret_msg);

        int temp_num = write(apt_fd, send_buf, send_num);
        if(temp_num != send_num)
        {
            dbg_print(PRINT_LEVEL_ERROR, "send messge to client failed\n");
        }
        close(apt_fd);
    }

    close(lsn_fd);
    unlink(MTDBG_SOCKET);
    return NULL;
}


int debugtool_init()
{
    pthread_t apDebugToolThread;
    pthread_create(&apDebugToolThread, NULL, debugtool_task, NULL);
    return 0;
}


