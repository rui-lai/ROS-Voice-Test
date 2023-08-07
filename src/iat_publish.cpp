/**
 * @file iat_publish.cpp
 * @author lairui (1319621467@qq.com)
 * @brief 科大讯飞语音听写(iFly Auto Transform)sdk测试
 * @version 0.1
 * @date 2023-08-06
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termio.h>
#include "qisr.h"
#include "msp_cmn.h"
#include "msp_errors.h"
#include "speech_recognizer.h"
#include <iconv.h>

#include "ros/ros.h"
#include "std_msgs/String.h"

#define FRAME_LEN   640
#define BUFFER_SIZE 4096

int wakeupFlag   = 0 ;
int resultFlag   = 0 ;

static void show_result(char *string, char is_over)
{
    resultFlag=1;
    printf("\rResult: [ %s ]", string);
    if(is_over)
        putchar('\n');
}

static char *g_result = NULL;
static unsigned int g_buffersize = BUFFER_SIZE;

// 识别结果回调函数，将语音识别的结果保存在全局变量g_result中
void on_result(const char *result, char is_last)
{
    if (result) {
        // 计算还能保存多少字符到全局变量g_result中
        size_t left = g_buffersize - 1 - strlen(g_result);
        size_t size = strlen(result);
        // 如果剩余的空间不足以保存新的识别结果，则重新分配内存
        if (left < size) {
            g_result = (char*)realloc(g_result, g_buffersize + BUFFER_SIZE);
            if (g_result)
                g_buffersize += BUFFER_SIZE;
            else {
                printf("mem alloc failed\n");
                return;
            }
        }
        // 将新的识别结果拼接到全局变量g_result末尾
        strncat(g_result, result, size);
        // 调用show_result函数显示识别结果
        show_result(g_result, is_last);
    }
}

void on_speech_begin()
{
    // 初始化g_result，释放其内存，并重新分配
    if (g_result)
    {
        free(g_result);
    }
    g_result = (char*)malloc(BUFFER_SIZE);
    g_buffersize = BUFFER_SIZE;
    memset(g_result, 0, g_buffersize);

    printf("Start Listening...\n");
    printf("Press \"Space\" key Stop\n");
}

void on_speech_end(int reason)
{
    if (reason == END_REASON_VAD_DETECT)
        printf("\nSpeaking done \n");
    else
        printf("\nRecognizer error %d\n", reason);
}

/* demo recognize the audio from microphone */
static void demo_mic(const char* session_begin_params)
{
    int errcode;
    int i = 0;

    // 创建语音识别器结构体，用于保存语音识别器的状态信息
    struct speech_rec iat;

    // 创建语音识别器的回调函数结构体
    struct speech_rec_notifier recnotifier = {
        on_result,          // 识别结果回调函数
        on_speech_begin,    // 开始识别回调函数
        on_speech_end       // 结束识别回调函数
    };

    // 初始化语音识别器，设置回调函数
    errcode = sr_init(&iat, session_begin_params, SR_MIC, &recnotifier);
    if (errcode) {
        printf("speech recognizer init failed\n");
        return;
    }

    // 开始语音识别
    errcode = sr_start_listening(&iat);
    if (errcode) {
        printf("start listen failed %d\n", errcode);
    }

    // 监听用户输入，按下Space则结束语音识别
    int ch;
    while(1){
        ch = getchar();
        if(ch == 32){
            printf("\nSpeaking done \n");
            break;
        }

    }

    // 停止语音识别
    errcode = sr_stop_listening(&iat);
    if (errcode) {
        printf("stop listening failed %d\n", errcode);
    }

    // 释放语音识别器的资源
    sr_uninit(&iat);
}



int main(int argc, char* argv[])
{
    // 初始化ROS节点
    ros::init(argc, argv, "iFlyAutoTransform");
    ros::NodeHandle n;
    ros::Rate loop_rate(10);

    // 创建话题发布者
    ros::Publisher iat_text_pub = n.advertise<std_msgs::String>("iat_text", 1000);

    // 保存终端输入设置，并禁用终端缓冲和回显
    termios tms_old, tms_new;
    tcgetattr(0, &tms_old);
    tms_new = tms_old;
    tms_new.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &tms_new);

    ROS_INFO("Press \"Space\" key to Start,Press \"Enter\" key to Exit.");
    int count=0;
    int ch;

    // 等待用户输入
    while(ros::ok())
    {
        ch = getchar();
        printf("Pressed Key Value %d\n",ch);
        if(ch == 32){  //输入Space
            wakeupFlag = 1;
        }
        if(ch == 10){  //输入Enter
            ROS_INFO("Node Exit.");
            break;
        }
        if (wakeupFlag){
            int ret = MSP_SUCCESS;
            // 访问语音识别服务的参数
            const char* login_params = "appid = 86af5674, work_dir = .";

            const char* session_begin_params =
                "sub = iat, domain = iat, language = zh_cn, "
                "accent = mandarin, sample_rate = 16000, "
                "result_type = plain, result_encoding = utf8";

            ret = MSPLogin(NULL, NULL, login_params);
            if(MSP_SUCCESS != ret){
                MSPLogout();
                printf("MSPLogin failed , Error code %d.\n",ret);
            }
            printf("Demo recognizing the speech from microphone\n");
            // printf("Speak in 10 seconds\n");
            // 执行语音识别
            demo_mic(session_begin_params);
            // printf("10 sec passed\n");
            wakeupFlag=0;
            // 注销语音识别服务
            MSPLogout();
        }
        // 语音识别完成，将结果发布到ROS话题上
        if(resultFlag){
            resultFlag=0;
            std_msgs::String msg;
            msg.data = g_result;
            iat_text_pub.publish(msg);
        }
        ROS_INFO("Press \"Space\" key to Start,Press \"Enter\" key to Exit.");
        ros::spinOnce();
        loop_rate.sleep();
        count++;
    }

exit:
    // 恢复终端输入设置
    tcsetattr(0, TCSANOW, &tms_old);
    MSPLogout();
    return 0;
}
