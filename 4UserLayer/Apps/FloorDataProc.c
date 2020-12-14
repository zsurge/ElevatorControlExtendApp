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
ELEVATOR_TRANBUFF_STRU gElevtorData,gRecvElevtorData;


/*----------------------------------------------*
 * �ڲ�����ԭ��˵��                             *
 *----------------------------------------------*/



void sendQueueToDev(ELEVATOR_TRANBUFF_STRU *devSendData)
{
    /* ʹ����Ϣ����ʵ��ָ������Ĵ��� */
    if(xQueueSend(xTransDataQueue,              /* ��Ϣ���о�� */
               (void *)devSendData,   /* ����ָ�����recv_buf�ĵ�ַ */
               (TickType_t)10) != pdPASS )
    {
        log_d("the queue is full!\r\n");                
        xQueueReset(xTransDataQueue);
    } 
    else
    {
        log_d("send queue value = %x,devsn=%d\r\n",devSendData->value,devSendData->devSn);
    }

}




SYSERRORCODE_E packetToElevatorExtend(USERDATA_STRU *localUserData,ELEVATOR_BUFF_STRU *devSendData)
{     
    SYSERRORCODE_E result = NO_ERR;
    char authLayer[64] = {0}; //Ȩ��¥�㣬���64��
    int num = 0;       
    
    memcpy(authLayer,localUserData->accessFloor,FLOOR_ARRAY_LEN);
    
    num = strlen((const char*)authLayer);

    log_d("localUserData->accessFloor num = %d\r\n",num);

    
    if(num > 1)//���Ȩ�ޣ��ֶ�
    {
        result = calcMultilFloor(authLayer,num,devSendData);
    }
    else    //����Ȩ�ޣ�ֱ�Ӻ�Ĭ��Ȩ��¥�㣬�Զ�
    {
        if(localUserData->defaultFloor != authLayer[0])
        {
        
            log_d("defaultFloor != authLayer,%d,%d\r\n",localUserData->defaultFloor,authLayer[0]);
            localUserData->defaultFloor = authLayer[0];
        }   
        
	    if(localUserData->defaultFloor == 0)
	    {
	        return INVALID_FLOOR;//��Ч��¥��
	    }
	    
        result = calcSingleFloor(localUserData->defaultFloor,devSendData); 
        
    } 

    log_d("1 value = %x,devsn=%d\r\n",devSendData->data[0].value,devSendData->data[0].devSn);
    log_d("2 value = %x,devsn=%d\r\n",devSendData->data[1].value,devSendData->data[1].devSn);
    log_d("3 value = %x,devsn=%d\r\n",devSendData->data[2].value,devSendData->data[2].devSn);
    log_d("4 value = %x,devsn=%d\r\n",devSendData->data[3].value,devSendData->data[3].devSn);
   
    
    return result;
}





//����Ȩ��
SYSERRORCODE_E calcSingleFloor(uint8_t floor,ELEVATOR_BUFF_STRU *eBuf)
{    
   
    if(floor == 0)
    {
         return INVALID_FLOOR;//��Ч��¥��
    }

    if(floor > 0 && floor<=16)
    {
        eBuf->data[0].devSn = 1;
        eBuf->data[0].value = setbit(0,floor-1);
    }
    else if(floor >=17 && floor<=32)
    {
        eBuf->data[0].devSn = 2;
        eBuf->data[0].value = setbit(0,floor-17);    
    }
    else if(floor >=33 && floor<=48)
    {
        eBuf->data[0].devSn = 3;
        eBuf->data[0].value = setbit(0,floor-33);    
    }
    else if(floor >=49 && floor<=64)
    {
        eBuf->data[0].devSn = 4;
        eBuf->data[0].value = setbit(0,floor-49);    
    }        

    return NO_ERR;
}

//���Ȩ��
SYSERRORCODE_E calcMultilFloor(uint8_t *floorBuf,uint8_t num,ELEVATOR_BUFF_STRU *eBuf)
{
    uint8_t i = 0;    

    //��Ŀǰ���󣬳���1��Ȩ�ޣ��Ͱ�����¥�㴦��
    #if 1
    for(i=0;i<num;i++)
    {
        if(floorBuf[i] > 0 && floorBuf[i]<=16)
        {
            eBuf->data[0].devSn  = 1;
            eBuf->data[0].value = setbit(eBuf->data[0].value,floorBuf[i]-1);
        }
        else if(floorBuf[i] >=17 && floorBuf[i]<=32)
        {
            eBuf->data[1].devSn  = 1;
            eBuf->data[1].value = setbit(eBuf->data[1].value,floorBuf[i]-17);    
        }
        else if(floorBuf[i] >=33 && floorBuf[i]<=48)
        {
            eBuf->data[2].devSn  = 1;
            eBuf->data[2].value = setbit(eBuf->data[2].value,floorBuf[i]-33);    
        }
        else if(floorBuf[i] >=49 && floorBuf[i]<=64)
        {
            eBuf->data[3].devSn  = 1;
            eBuf->data[3].value = setbit(eBuf->data[3].value,floorBuf[i]-49);    
        } 
    }
    #endif

//    if(num >1)
//    {
//        eBuf->data[0].devSn = 1;
//        eBuf->data[0].value = 0x00ff;
//        eBuf->data[1].devSn = 1;
//        eBuf->data[1].value = 0xff00;
//        eBuf->data[2].devSn = 1;
//        eBuf->data[2].value = 0x00ff;
//        eBuf->data[3].devSn = 1;
//        eBuf->data[3].value = 0xff00;
//    }

    


    log_d("1 value = %x,devsn=%d\r\n",eBuf->data[0].value,eBuf->data[0].devSn);
    log_d("2 value = %x,devsn=%d\r\n",eBuf->data[1].value,eBuf->data[1].devSn);
    log_d("3 value = %x,devsn=%d\r\n",eBuf->data[2].value,eBuf->data[2].devSn);
    log_d("4 value = %x,devsn=%d\r\n",eBuf->data[3].value,eBuf->data[3].devSn);


    return NO_ERR;
}



