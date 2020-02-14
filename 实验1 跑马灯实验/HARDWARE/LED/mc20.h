#ifndef __MC20_H_
#define __MC20_H_
#include "sys.h"
#include <stdbool.h>

#define MC20_USE_OS            0   //是否使用RTOS操作系统
#define MC20_USE_DMA           0   //是否使用DMA收发

/******************************MC20配置*******************************/
#define MC20_BAUDRATE          115200
#define MC20_USART             USART1
#define MC20_USART_IRQ         USART1_IRQn
#define MC20_USART_IRQHandler  USART1_IRQHandler

#define MC20_TX_DMA_CHANNEL    DMA1_Channel4
#define MC20_RX_DMA_CHANNEL    DMA1_Channel5

#define MC20_POWER_ON()        GPIO_SetBits(GPIOA,GPIO_Pin_8)    //开启模块
#define MC20_POWER_OFF()       GPIO_ResetBits(GPIOA,GPIO_Pin_8)  //关闭模块

#define MC20_TX_SIZE           512  //发送缓存大小
#define MC20_RX_SIZE           1024 //接收缓存大小

#define MC20_CMD_SIZE          64   //指令缓存大小
#define MC20_RAM_SIZE          512  //暂存缓存大小

#define MC20_TCP_SIZE          512  //TCP缓存大小

#define MC20_LEN_SIZE          10   //长度队列大小


/***********************************定义*******************************/
#define MC20_INIT_FLAG           0
#define MC20_BUSY_FLAG           1
#define MC20_CSQ_FLAG            2
#define MC20_TCP_FLAG            3
#define MC20_GPS_FLAG            4
#define MC20_RELINK_FLAG         5
#define MC20_RESET_FLAG          6

/***********************************GPS定义*******************************/
typedef enum
{
	LOC_FAIL,
	LOC_GPS,
	LOC_LBS
}gps_mode_t;//定位模式

typedef struct
{
	volatile uint8_t hour;
	volatile uint8_t min;
	volatile uint8_t sec;			
	volatile uint8_t year;
	volatile uint8_t month;
	volatile uint8_t date; 
}lbs_time_t;//基站/GPS时间

//typedef struct
//{
//    uint8_t sta;   //定位状态
//	uint32_t log;    //经度
//	uint32_t lat;    //纬度
//	lbs_time_t time; //时间
//}lbs_info_t;

//typedef struct 
//{
//	uint8_t  mode;  //定位模式
//	uint8_t  sta;   //定位状态
//	uint8_t  ew;    //东西经
//	uint8_t  ns;    //南北纬
//	uint16_t speed; //速度
//	uint16_t angle; //角度
//	uint32_t log;   //经度
//	uint32_t lat;   //纬度
//	LbsTimeType time;//时间
//}gps_info_t;

typedef struct
{
    uint8_t sta;     //定位状态
	uint8_t log[12]; //经度
	uint8_t lat[12]; //纬度
	lbs_time_t time; //时间
}lbs_info_t;

typedef struct 
{
	uint8_t  mode;    //定位模式
	uint8_t  ram;     //是否缓存
	uint8_t  sta;     //定位状态
	uint8_t  ew;      //东西经
	uint8_t  ns;      //南北纬
	uint8_t  speed[8];//速度
	uint8_t  angle[8];//角度
	uint8_t  log[12]; //经度
	uint8_t  lat[12]; //纬度
	lbs_time_t time;  //时间
}gps_info_t;


/**************************函数声明************************************/


#endif
