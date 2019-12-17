/*******************************************************************************
 * Copyright (c) 2012, 2016 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *   http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial contribution
 *    Ian Craggs - change delimiter option from char to string
 *    Al Stockdill-Mander - Version using the embedded C client
 *    Ian Craggs - update MQTTClient function names
 *******************************************************************************/

/*

 stdout subscriber

 compulsory parameters:

  topic to subscribe to

 defaulted parameters:

    --host localhost
    --port 1883
    --qos 2
    --delimiter \n
    --clientid stdout_subscriber

    --userid none
    --password none

 for example:

    stdoutsub topic/of/interest --host iot.eclipse.org

*/
#include <stdio.h>
#include <memory.h>
#include "MQTTClient.h"

#include <stdio.h>
#include <signal.h>

#include <sys/time.h>
#include "dbgPrint.h"
#include "debug_tool.h"

volatile int toStop = 0;
sem_t publish_sem;

void usage()
{
    printf("MQTT stdout subscriber\n");
    printf("Usage: stdoutsub topicname <options>, where options are:\n");
    printf("  --host <hostname> (default is localhost)\n");
    printf("  --port <port> (default is 1883)\n");
    printf("  --qos <qos> (default is 2)\n");
    printf("  --delimiter <delim> (default is \\n)\n");
    printf("  --clientid <clientid> (default is hostname+timestamp)\n");
    printf("  --username none\n");
    printf("  --password none\n");
    printf("  --showtopics <on or off> (default is on if the topic has a wildcard, else off)\n");
    exit(-1);
}


void cfinish(int sig)
{
    signal(SIGINT, NULL);
    dbg_print(PRINT_LEVEL_ERROR, "capture the signal\n");
}


struct opts_struct
{
    char* clientid;
    int nodelimiter;
    char* delimiter;
    enum QoS qos;
    char* username;
    char* password;
    char* host;
    int port;
    int showtopics;
} opts =
{
    (char*)"rasppi", 0, (char*)"\n", QOS1, "testuser_pi", "testpassword", (char*)"localhost", 1883, 0
};


void getopts(int argc, char** argv)
{
    int count = 2;

    while (count < argc)
    {
        if (strcmp(argv[count], "--qos") == 0)
        {
            if (++count < argc)
            {
                if (strcmp(argv[count], "0") == 0)
                    opts.qos = QOS0;
                else if (strcmp(argv[count], "1") == 0)
                    opts.qos = QOS1;
                else if (strcmp(argv[count], "2") == 0)
                    opts.qos = QOS2;
                else
                    usage();
            }
            else
                usage();
        }
        else if (strcmp(argv[count], "--host") == 0)
        {
            if (++count < argc)
                opts.host = argv[count];
            else
                usage();
        }
        else if (strcmp(argv[count], "--port") == 0)
        {
            if (++count < argc)
                opts.port = atoi(argv[count]);
            else
                usage();
        }
        else if (strcmp(argv[count], "--clientid") == 0)
        {
            if (++count < argc)
                opts.clientid = argv[count];
            else
                usage();
        }
        else if (strcmp(argv[count], "--username") == 0)
        {
            if (++count < argc)
                opts.username = argv[count];
            else
                usage();
        }
        else if (strcmp(argv[count], "--password") == 0)
        {
            if (++count < argc)
                opts.password = argv[count];
            else
                usage();
        }
        else if (strcmp(argv[count], "--delimiter") == 0)
        {
            if (++count < argc)
                opts.delimiter = argv[count];
            else
                opts.nodelimiter = 1;
        }
        else if (strcmp(argv[count], "--showtopics") == 0)
        {
            if (++count < argc)
            {
                if (strcmp(argv[count], "on") == 0)
                    opts.showtopics = 1;
                else if (strcmp(argv[count], "off") == 0)
                    opts.showtopics = 0;
                else
                    usage();
            }
            else
                usage();
        }
        count++;
    }

}


static void messageArrived(MessageData* md)
{
    MQTTMessage* message = md->message;

    if (opts.showtopics)
        printf("%.*s\t", md->topicName->lenstring.len, md->topicName->lenstring.data);
    if (opts.nodelimiter)
        printf("%.*s", (int)message->payloadlen, (char*)message->payload);
    else
        printf("%.*s%s", (int)message->payloadlen, (char*)message->payload, opts.delimiter);
    //fflush(stdout);
}

static int MQTTClientConnetServer(MQTTClient *c)
{
    int ret = 0;
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.willFlag = 0;
    data.MQTTVersion = 3;
    data.clientID.cstring = opts.clientid;
    data.username.cstring = opts.username;
    data.password.cstring = opts.password;

    data.keepAliveInterval = 10;
    data.cleansession = 0;
    dbg_print(PRINT_LEVEL_DEBUG, "Connecting to %s %d\n", opts.host, opts.port);

    ret = MQTTConnect(c, &data);
    dbg_print(PRINT_LEVEL_DEBUG, "Connected %d\n", ret);
    return ret;
}

static int MQTTClientDestory(MQTTClient *c)
{
    int ret = 0;

    pthread_mutex_destroy(&(c->mutex));
    pthread_cancel(c->ThreadPing);
    pthread_cancel(c->ThreadParser);
    pthread_cancel(c->TreadEventUpload);

    pthread_join(c->ThreadPing, NULL);
    pthread_join(c->ThreadParser, NULL);
    pthread_join(c->TreadEventUpload, NULL);

    dbg_print(PRINT_LEVEL_WARNING, "MQTTClient_destroy\n");
    return ret;
}


static int publish_message(int argc, char *argv[], char *ret_msg)
{
    int ret = 0;
    char *msg;

    if (argc < 2)
    {
        ret_msg += sprintf(ret_msg, "Error: cmd format is mtdbg publish msg\n");
        return DEBUG_RET_PARAM_ERR;
    }

    /*get the parameter*/
    msg = argv[1];

    if (argc >= 3)
    {
        ret_msg += sprintf(ret_msg, "Error: too many param.\n");
        return DEBUG_RET_PARAM_ERR;
    }

    if (msg != NULL)
    {
        dbg_print(PRINT_LEVEL_DEBUG, "publish message, msg = %s\n", msg);
        sem_post(&publish_sem);
        ret_msg += sprintf(ret_msg, "publish message, msg = %s\n", msg);
    }
    return ret;
}


static void MQTTClientDebugtoolInit(void)
{
    debug_tool_entry entry[] = {{"publish", "publish the message to the sever, param is message", publish_message}};
    debugtool_register(entry, sizeof(entry)/sizeof(debug_tool_entry));
    sem_init(&publish_sem, 0, 0);
    return ;
}

int mqtt_demo_entry(int argc, char** argv)
{
    int rc = 0;
    unsigned char buf[100];
    unsigned char readbuf[100];

    if (argc < 2)
        usage();

    char* topic = argv[1];

    if (strchr(topic, '#') || strchr(topic, '+'))
        opts.showtopics = 1;
    if (opts.showtopics)
        dbg_print(PRINT_LEVEL_DEBUG, "topic is %s\n", topic);

    getopts(argc, argv);

    Network n;
    MQTTClient c;

    signal(SIGINT, cfinish);
    signal(SIGTERM, cfinish);
    signal(SIGPIPE,SIG_IGN);

    NetworkInit(&n);
    MQTTClientDebugtoolInit();

    while (1)
    {
        if (NetworkConnect(&n, opts.host, opts.port) != 0)
            dbg_print(PRINT_LEVEL_DEBUG, "ERROR: tcp conneted failed!\n");
        else
            dbg_print(PRINT_LEVEL_DEBUG, "tcp conneted success!\n");

        MQTTClientInit(&c, &n, 3000, buf, 100, readbuf, 100);
        MQTTClientConnetServer(&c);

        rc = MQTTSubscribe(&c, topic, opts.qos, messageArrived);
        dbg_print(PRINT_LEVEL_DEBUG, "Subscribing to %s result: %d\n", topic, rc);

        while(1)
        {
            sleep(1);
            if (!c.isconnected)
            {
                MQTTDisconnect(&c);
                NetworkDisconnect(&n);
                MQTTClientDestory(&c);
                break;
            }
        }
        sleep(1);
    }

    dbg_print(PRINT_LEVEL_DEBUG, "Stopping\n");

    MQTTDisconnect(&c);
    NetworkDisconnect(&n);

    return 0;
}

