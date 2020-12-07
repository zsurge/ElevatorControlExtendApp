/******************************************************************************

                  ��Ȩ���� (C), 2013-2023, ���ڲ�˼�߿Ƽ����޹�˾

 ******************************************************************************
  �� �� ��   : FloorDataProc.c
  �� �� ��   : ����
  ��    ��   : �Ŷ�
  ��������   : 2019��12��23��
  ����޸�   :
  ��������   : ���ݿ�������ָ����ļ�
  �����б�   :
  �޸���ʷ   :
  1.��    ��   : 2019��12��23��
    ��    ��   : �Ŷ�
    �޸�����   : �����ļ�

******************************************************************************/

/*----------------------------------------------*
 * ����ͷ�ļ�                                   *
 *----------------------------------------------*/
#define LOG_TAG    "FloorData"
#include "elog.h"
#include "FloorDataProc.h"




/*----------------------------------------------*
 * �궨��                                       *
 *----------------------------------------------*/
#define AUTO_REG            1
#define MANUAL_REG          2

/*----------------------------------------------*
 * ��������                                     *
 *----------------------------------------------*/

/*----------------------------------------------*
 * ģ�鼶����                                   *
 *----------------------------------------------*/


/*----------------------------------------------*
 * �ڲ�����ԭ��˵��                             *
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
                return ;  //��Ȩ��
            }

            //1.�������ݵ�����
//            ret = packetToElevator(localUserData);
            ret = packetToElevatorExtend(localUserData);
            if(ret != NO_ERR)
            {
                log_d("invalid floor\r\n");
                return ;  //��Ȩ��   
            }
            
            //2.����������
            packetPayload(localUserData,jsonBuf); 

            len = strlen((const char*)jsonBuf);

            len = mqttSendData(jsonBuf,len);
            log_d("send = %d\r\n",len);            
            break;
        case AUTH_MODE_REMOTE:
            //ֱ�ӷ���Ŀ��¥��
            log_d("send desc floor = %d,%d\r\n",pQueue->data[0],pQueue->dataLen);  

            ret = packetRemoteRequestToElevator(pQueue->data,pQueue->dataLen);
            if(ret != NO_ERR)
            {
                log_d("invalid floor\r\n");
                return ;  //��Ȩ��   
            }
                    
            break;
        case AUTH_MODE_UNBIND:
            //ֱ�ӷ���ͣ���豸ָ��
            xQueueReset(xDataProcessQueue); 
            log_d("send AUTH_MODE_UNBIND floor\r\n");
            break;
        case AUTH_MODE_BIND:
            //ֱ�ӷ�����������ָ��
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
        //��ά��
        log_d("pQueue->data = %s\r\n",pQueue->data);

        isFind = parseQrCode(pQueue->data,localUserData);
        localUserData->authMode = pQueue->authMode; 

        log_d("qrCodeInfo->startTime= %s\r\n",localUserData->startTime); 
        log_d("qrCodeInfo->endTime= %s\r\n",localUserData->endTime);  
        log_d("isfind = %d\r\n",isFind);      

        if(isFind != NO_ERR)
        {
            //δ�ҵ���¼����Ȩ��
            log_d("not find record\r\n");
            return NO_AUTHARITY_ERR;
        }         
       
    }
    else
    {
        //���� CARD 230000000089E1E35D,23         
        memcpy(key,pQueue->data,CARD_NO_LEN);
        log_d("key = %s\r\n",key);     
        
        isFind = readUserData(key,CARD_MODE,localUserData);   

        log_d("isFind = %d,rUserData.cardState = %d\r\n",isFind,localUserData->cardState);

        if(localUserData->cardState != CARD_VALID || isFind != 0)
        {
            //δ�ҵ���¼����Ȩ��
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
    char *buf[6] = {0}; //��ŷָ������ַ��� 
    int num = 0;
    uint8_t key[8+1] = {0};    

    memset(key,0x00,sizeof(key));   
    
    memset(value,0x00,sizeof(value));

    val_len = ef_get_env_blob((const char*)key, value, sizeof(value) , NULL);
   

    log_d("get env = %s,val_len = %d\r\n",value,val_len);

    if(val_len <= 0)
    {
        //δ�ҵ���¼����Ȩ��
        log_e("not find record\r\n");
        return NO_AUTHARITY_ERR;
    }

    split(value,";",buf,&num); //���ú������зָ� 
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
    /* ʹ����Ϣ����ʵ��ָ������Ĵ��� */
    if(xQueueSend(xTransDataQueue,              /* ��Ϣ���о�� */
               (void *) &devSendData,   /* ����ָ�����recv_buf�ĵ�ַ */
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
    
    
    if(len > 1)//���Ȩ�ޣ��ֶ�
    {
        for(i=0;i<len;i++)
        {
            log_d("current floor = %d\r\n",tagFloor[i]);
            calcFloor(tagFloor[i],devSendData);  
//            sendQueueToDev(devSendData);
        }  
    }
    else    //����Ȩ�ޣ�ֱ�Ӻ�Ĭ��Ȩ��¥�㣬�Զ�
    {        
        floor = tagFloor[0]; 
        
        if(floor == 0)
        {
            return INVALID_FLOOR;//��Ч��¥��
        }
        
        calcFloor(floor,devSendData);   
        
//        sendQueueToDev(devSendData);
    }  

    return result;

}




static void calcFloor(uint8_t layer,ELEVATOR_BUFF_STRU *devSendData)
{

    uint16_t floor = 0; //��������Ϊ�е�������
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
        //���ݲ��뿪�أ����ж������ٶ�
        //�����-3�㣬����3����-5�㣬����5
//        floor = layer + ((bsp_dipswitch_read()>>1) & 0x07);    
        floor = layer + (bsp_dipswitch_read()>>2) & 0x03;
    }
    #endif

//    log_d("before calculation floor = %d,after calculation floor =%d\r\n",layer,floor);

//((bsp_dipswitch_read() & 0x01) +1); //��1λ������ʾ����ID                
//(((bsp_dipswitch_read()>>1) & 0x07));//��2��3��4����������¥��

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
//    /* ʹ����Ϣ����ʵ��ָ������Ĵ��� */
//    if(xQueueSend(xTransDataQueue,              /* ��Ϣ���о�� */
//    			 (void *) &devSendData,   /* ����ָ�����recv_buf�ĵ�ַ */
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
    char authLayer[64] = {0}; //Ȩ��¥�㣬���64��
    int num = 0;    

    uint8_t floor = 0;

    uint8_t i = 0;  

    ELEVATOR_BUFF_STRU *devSendData = &gElevtorData;
    devSendData->devSn = 0;
    devSendData->value = 0;
    

    memcpy(authLayer,localUserData->accessFloor,FLOOR_ARRAY_LEN);
    
    num = strlen((const char*)authLayer);

    log_d("localUserData->accessFloor num = %d\r\n",num);

    
    if(num > 1)//���Ȩ�ޣ��ֶ�
    {
        for(i=0;i<num;i++)
        {
            calcFloor(authLayer[i],devSendData);  
//            sendQueueToDev(devSendData);
        }    
    }
    else    //����Ȩ�ޣ�ֱ�Ӻ�Ĭ��Ȩ��¥�㣬�Զ�
    {
        if(localUserData->defaultFloor != authLayer[0])
        {
        
            log_d("defaultFloor != authLayer,%d,%d\r\n",localUserData->defaultFloor,authLayer[0]);
            localUserData->defaultFloor = authLayer[0];
        }
        
        floor = localUserData->defaultFloor;//authLayer[0];   
        
	    if(floor == 0)
	    {
	        return INVALID_FLOOR;//��Ч��¥��
	    }
		
        calcFloor(floor,devSendData);   
        
//        sendQueueToDev(devSendData);
    }   



    log_d("@@@@@@@@@@@@@@@@@@@@@@@@@\r\n");
    return result;
}




