/******************************************************************************

                  版权所有 (C), 2013-2023, 深圳博思高科技有限公司

 ******************************************************************************
  文 件 名   : FloorDataProc.c
  版 本 号   : 初稿
  作    者   : 张舵
  生成日期   : 2019年12月23日
  最近修改   :
  功能描述   : 电梯控制器的指令处理文件
  函数列表   :
  修改历史   :
  1.日    期   : 2019年12月23日
    作    者   : 张舵
    修改内容   : 创建文件

******************************************************************************/

/*----------------------------------------------*
 * 包含头文件                                   *
 *----------------------------------------------*/
#define LOG_TAG    "FloorData"
#include "elog.h"
#include "FloorDataProc.h"




/*----------------------------------------------*
 * 宏定义                                       *
 *----------------------------------------------*/
#define AUTO_REG            1
#define MANUAL_REG          2

/*----------------------------------------------*
 * 常量定义                                     *
 *----------------------------------------------*/

/*----------------------------------------------*
 * 模块级变量                                   *
 *----------------------------------------------*/


/*----------------------------------------------*
 * 内部函数原型说明                             *
 *----------------------------------------------*/
static SYSERRORCODE_E packetToElevator(USERDATA_STRU *localUserData);
//static void calcFloor(uint8_t layer);
void sendQueueToDev(ELEVATOR_BUFF_STRU *devSendData);
static void calcFloor(uint8_t layer,ELEVATOR_BUFF_STRU *devSendData);

static SYSERRORCODE_E authReader(READER_BUFF_STRU *pQueue,USERDATA_STRU *localUserData);
static SYSERRORCODE_E packetRemoteRequestToElevator(uint8_t *tagFloor,uint8_t len);

static SYSERRORCODE_E packetToElevatorExtend(USERDATA_STRU *localUserData);//add 1204


void packetSendBuf(READER_BUFF_STRU *pQueue)
{
    uint8_t jsonBuf[512] = {0};
    uint8_t sendBuf[64] = {0};
    uint16_t len = 0;
    uint16_t ret = 0;
    int tagFloor = 0;
    USERDATA_STRU *localUserData = &gUserDataStru;
    memset(localUserData,0x00,sizeof(USERDATA_STRU));
    
    sendBuf[0] = CMD_STX;
    sendBuf[1] = 0x01;//bsp_dipswitch_read();
    sendBuf[MAX_SEND_LEN-1] = xorCRC(sendBuf,MAX_SEND_LEN-2);
    log_d("card or QR data = %s\r\n",pQueue->data);

    switch(pQueue->authMode)
    {
        case AUTH_MODE_CARD:
        case AUTH_MODE_QR:
            log_d("card or QR auth,pQueue->authMode = %d\r\n",pQueue->authMode);
            ret = authReader(pQueue,localUserData);  
            
            if(ret != NO_ERR)
            {
                log_d("reject access\r\n");
                return ;  //无权限
            }

            //1.发给电梯的数据
//            ret = packetToElevator(localUserData);
            ret = packetToElevatorExtend(localUserData);
            if(ret != NO_ERR)
            {
                log_d("invalid floor\r\n");
                return ;  //无权限   
            }
            
            //2.发给服务器
            packetPayload(localUserData,jsonBuf); 

            len = strlen((const char*)jsonBuf);

            len = mqttSendData(jsonBuf,len);
            log_d("send = %d\r\n",len);            
            break;
        case AUTH_MODE_REMOTE:
            //直接发送目标楼层
            log_d("send desc floor = %d,%d\r\n",pQueue->data[0],pQueue->dataLen);  

            ret = packetRemoteRequestToElevator(pQueue->data,pQueue->dataLen);
            if(ret != NO_ERR)
            {
                log_d("invalid floor\r\n");
                return ;  //无权限   
            }
                    
            break;
        case AUTH_MODE_UNBIND:
            //直接发送停用设备指令
            xQueueReset(xDataProcessQueue); 
            log_d("send AUTH_MODE_UNBIND floor\r\n");
            break;
        case AUTH_MODE_BIND:
            //直接发送启动设置指令
            xQueueReset(xDataProcessQueue); 
            log_d("send AUTH_MODE_BIND floor\r\n");
            break;
        default:
            log_d("invalid authMode\r\n");
            break;    
   }

}

SYSERRORCODE_E authReader(READER_BUFF_STRU *pQueue,USERDATA_STRU *localUserData)
{
    SYSERRORCODE_E result = NO_ERR;
    uint8_t key[CARD_NO_LEN+1] = {0};  
    uint8_t isFind = 0;  
    
    memset(key,0x00,sizeof(key)); 
    log_d("card or QR data = %s,mode = %d\r\n",pQueue->data,pQueue->authMode);
    
    if(pQueue->authMode == AUTH_MODE_QR) 
    {
        //二维码
        log_d("pQueue->data = %s\r\n",pQueue->data);

        isFind = parseQrCode(pQueue->data,localUserData);
        localUserData->authMode = pQueue->authMode; 

        log_d("qrCodeInfo->startTime= %s\r\n",localUserData->startTime); 
        log_d("qrCodeInfo->endTime= %s\r\n",localUserData->endTime);  
        log_d("isfind = %d\r\n",isFind);      

        if(isFind != NO_ERR)
        {
            //未找到记录，无权限
            log_d("not find record\r\n");
            return NO_AUTHARITY_ERR;
        }         
       
    }
    else
    {
        //读卡 CARD 230000000089E1E35D,23         
        memcpy(key,pQueue->data,CARD_NO_LEN);
        log_d("key = %s\r\n",key);     
        
        isFind = readUserData(key,CARD_MODE,localUserData);   

        log_d("isFind = %d,rUserData.cardState = %d\r\n",isFind,localUserData->cardState);

        if(localUserData->cardState != CARD_VALID || isFind != 0)
        {
            //未找到记录，无权限
            log_e("not find record\r\n");
            return NO_AUTHARITY_ERR;
        } 
        
        localUserData->platformType = 4;
        localUserData->authMode = pQueue->authMode; 
        memcpy(localUserData->timeStamp,time_to_timestamp(),TIMESTAMP_LEN);
        log_d("localUserData->timeStamp = %s\r\n",localUserData->timeStamp);         
    }

    log_d("localUserData->cardNo = %s\r\n",localUserData->cardNo);
    log_d("localUserData->userId = %s\r\n",localUserData->userId);
    dbh("localUserData->accessLayer",localUserData->accessFloor,sizeof(localUserData->accessFloor));
    log_d("localUserData->defaultLayer = %d\r\n",localUserData->defaultFloor);    
    log_d("localUserData->startTime = %s\r\n",localUserData->startTime);        
    log_d("localUserData->endTime = %s\r\n",localUserData->endTime);        
    log_d("localUserData->authMode = %d\r\n",localUserData->authMode);
    log_d("localUserData->timeStamp = %s\r\n",localUserData->timeStamp);
    log_d("localUserData->platformType = %s\r\n",localUserData->platformType);

    return result;
}




SYSERRORCODE_E authRemote(READER_BUFF_STRU *pQueue,USERDATA_STRU *localUserData)
{
    SYSERRORCODE_E result = NO_ERR;
    char value[128] = {0};
    int val_len = 0;
    char *buf[6] = {0}; //存放分割后的子字符串 
    int num = 0;
    uint8_t key[8+1] = {0};    

    memset(key,0x00,sizeof(key));   
    
    memset(value,0x00,sizeof(value));

    val_len = ef_get_env_blob((const char*)key, value, sizeof(value) , NULL);
   

    log_d("get env = %s,val_len = %d\r\n",value,val_len);

    if(val_len <= 0)
    {
        //未找到记录，无权限
        log_e("not find record\r\n");
        return NO_AUTHARITY_ERR;
    }

    split(value,";",buf,&num); //调用函数进行分割 
    log_d("num = %d\r\n",num);

    if(num != 5)
    {
        log_e("read record error\r\n");
        return READ_RECORD_ERR;       
    }

    localUserData->authMode = pQueue->authMode;    
    
    if(AUTH_MODE_QR == pQueue->authMode)
    {
        strcpy((char*)localUserData->userId,(const char*)key);
        
        strcpy((char*)localUserData->cardNo,buf[0]);        
    }
    else
    {
        memcpy(localUserData->cardNo,key,CARD_NO_LEN);

        log_d("buf[0] = %s\r\n",buf[0]);
        strcpy((char*)localUserData->userId,buf[0]);        
    }   

    //3867;0;0;2019-12-29;2029-12-31
    
    
    strcpy((char*)localUserData->accessFloor,buf[1]);
    localUserData->defaultFloor = atoi(buf[2]);
    strcpy((char*)localUserData->startTime,buf[3]);
    strcpy((char*)localUserData->endTime,buf[4]);    



    log_d("localUserData->cardNo = %s\r\n",localUserData->cardNo);
    log_d("localUserData->userId = %s\r\n",localUserData->userId);
//    dbh("localUserData->accessLayer",localUserData->accessFloor,sizeof(localUserData->accessFloor));
    log_d("localUserData->defaultLayer = %d\r\n",localUserData->defaultFloor);    
    log_d("localUserData->startTime = %s\r\n",localUserData->startTime);        
    log_d("localUserData->endTime = %s\r\n",localUserData->endTime);        
    log_d("localUserData->authMode = %d\r\n",localUserData->authMode);

    return result;

}


void sendQueueToDev(ELEVATOR_BUFF_STRU *devSendData)
{
    /* 使用消息队列实现指针变量的传递 */
    if(xQueueSend(xTransDataQueue,              /* 消息队列句柄 */
               (void *) &devSendData,   /* 发送指针变量recv_buf的地址 */
               (TickType_t)1000) != pdPASS )
    {
        log_d("the queue is full!\r\n");                
        xQueueReset(xTransDataQueue);
    } 
    else
    {
        log_d("devSendData->value = %d,devSendData->devSn = %d\r\n",devSendData->value,devSendData->devSn);        
    }

}

static SYSERRORCODE_E packetRemoteRequestToElevator(uint8_t *tagFloor,uint8_t len)
{
    SYSERRORCODE_E result = NO_ERR;

    uint8_t floor = 0;
    uint8_t i = 0; 
    
    ELEVATOR_BUFF_STRU *devSendData = &gElevtorData;
    devSendData->devSn = 0;
    devSendData->value = 0;
    
    
    if(len > 1)//多层权限，手动
    {
        for(i=0;i<len;i++)
        {
            log_d("current floor = %d\r\n",tagFloor[i]);
            calcFloor(tagFloor[i],devSendData);  
//            sendQueueToDev(devSendData);
        }  
    }
    else    //单层权限，直接呼默认权限楼层，自动
    {        
        floor = tagFloor[0]; 
        
        if(floor == 0)
        {
            return INVALID_FLOOR;//无效的楼层
        }
        
        calcFloor(floor,devSendData);   
        
//        sendQueueToDev(devSendData);
    }  

    return result;

}




static void calcFloor(uint8_t layer,ELEVATOR_BUFF_STRU *devSendData)
{

    uint16_t floor = 0; //这里是因为有地下三层
    uint16_t tmpFloor = 0;
    uint16_t index = 0;

    uint8_t buf[32] = {0};
    uint16_t bufLen = 0;
    
//    ELEVATOR_BUFF_STRU *devSendData = &gElevtorData;

//    devSendData->devSn = 0;
//    devSendData->value = 0;
    
//    floor = layer + (bsp_dipswitch_read()>>2) & 0x03;
    floor = layer ;

    
    #if 0
    if(layer > MAX_FLOOR)
    {
        floor = layer-MAX_FLOOR;
    }
    else
    {
        //根据拨码开关，来判定补多少动
        //如果是-3层，即补3，是-5层，即补5
//        floor = layer + ((bsp_dipswitch_read()>>1) & 0x07);    
        floor = layer + (bsp_dipswitch_read()>>2) & 0x03;
    }
    #endif

//    log_d("before calculation floor = %d,after calculation floor =%d\r\n",layer,floor);

//((bsp_dipswitch_read() & 0x01) +1); //第1位用来表示机器ID                
//(((bsp_dipswitch_read()>>1) & 0x07));//第2，3，4用来补偿负楼层

    if(floor > 0 && floor<=16)
    {
        devSendData->devSn = 1;
        devSendData->value = setbit(0,floor-1);
    }
    else if(floor >=17 && floor<=32)
    {
        devSendData->devSn = 2;
        devSendData->value = setbit(0,floor-17);    
    }
    else if(floor >=33 && floor<=48)
    {
        devSendData->devSn = 3;
        devSendData->value = setbit(0,floor-33);    
    }
    else if(floor >=49 && floor<=64)
    {
        devSendData->devSn = 4;
        devSendData->value = setbit(0,floor-49);    
    }

    bufLen = packetBuf(devSendData,buf);
    
    RS485_SendBuf(COM6,buf,bufLen); 

     vTaskDelay(20); 
//    /* 使用消息队列实现指针变量的传递 */
//    if(xQueueSend(xTransDataQueue,              /* 消息队列句柄 */
//    			 (void *) &devSendData,   /* 发送指针变量recv_buf的地址 */
//    			 (TickType_t)1000) != pdPASS )
//    {
//        log_d("the queue is full!\r\n");                
//        xQueueReset(xTransDataQueue);
//    } 
//    else
//    {
//        log_d("devSendData->value = %d,devSendData->devSn = %d\r\n",devSendData->value,devSendData->devSn);        
//    }
}


static SYSERRORCODE_E packetToElevatorExtend(USERDATA_STRU *localUserData)
{
    SYSERRORCODE_E result = NO_ERR;
    char authLayer[64] = {0}; //权限楼层，最多64层
    int num = 0;    

    uint8_t floor = 0;

    uint8_t i = 0;  

    ELEVATOR_BUFF_STRU *devSendData = &gElevtorData;
    devSendData->devSn = 0;
    devSendData->value = 0;
    

    memcpy(authLayer,localUserData->accessFloor,FLOOR_ARRAY_LEN);
    
    num = strlen((const char*)authLayer);

    log_d("localUserData->accessFloor num = %d\r\n",num);

    
    if(num > 1)//多层权限，手动
    {
        for(i=0;i<num;i++)
        {
            calcFloor(authLayer[i],devSendData);  
//            sendQueueToDev(devSendData);
        }    
    }
    else    //单层权限，直接呼默认权限楼层，自动
    {
        if(localUserData->defaultFloor != authLayer[0])
        {
        
            log_d("defaultFloor != authLayer,%d,%d\r\n",localUserData->defaultFloor,authLayer[0]);
            localUserData->defaultFloor = authLayer[0];
        }
        
        floor = localUserData->defaultFloor;//authLayer[0];   
        
	    if(floor == 0)
	    {
	        return INVALID_FLOOR;//无效的楼层
	    }
		
        calcFloor(floor,devSendData);   
        
//        sendQueueToDev(devSendData);
    }   



    log_d("@@@@@@@@@@@@@@@@@@@@@@@@@\r\n");
    return result;
}




