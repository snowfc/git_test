#ifndef __FC_AT_DRV_H_
#define __FC_AT_DRV_H_

#define NONE_OS_DELAY          0   //是否使能无阻塞延时，1使能，0失能


#define AT_CMD_DELAY_1S        1000
#define AT_CMD_DELAY_2S        2000
#define AT_CMD_DELAY_5S        5000
#define AT_CMD_DELAY_10S       10000
#define AT_CMD_DELAY_15S       15000
#define AT_CMD_DELAY_30S       30000
#define AT_CMD_DELAY_45S       45000
#define AT_CMD_DELAY_60S       60000
#define AT_CMD_DELAY_90S       90000

typedef enum
{
    AT_STA_NULL,
    AT_STA_OK,
    AT_STA_ERROR,
    AT_STA_TIMEOUT
}at_status_t;

typedef enum
{
	AT_MODE,                       //AT模式
	PASS_MODE                      //透传模式
}at_mode_t;

typedef enum
{
	FLAG_REST,                     //清位
	FLAG_SET                       //置位
}at_flag_t;


typedef struct
{
	unsigned char *buff;         //FIFO数据
	unsigned short size;         //FIFO大小
	volatile unsigned short in;  //入口下标(写地址下标)
	volatile unsigned short out; //出口下标(读地址下标)
}at_fifo_t;

typedef struct
{
	unsigned short *buff;        //FIFO数据
	unsigned short size;         //FIFO大小
	volatile unsigned short in;  //入口下标(写地址下标)
	volatile unsigned short out; //出口下标(读地址下标)
}at_len_fifo_t;



typedef void (*send_cb_t)(unsigned char*,unsigned short);

#if NONE_OS_DELAY
typedef unsigned long (*ticks_cb_t)(void);
#else
typedef void (*delay_cb_t)(unsigned short);
#endif

typedef struct 
{
	unsigned char *cmd;             //发送指令
	unsigned char *ram;             //接收缓存
	volatile at_status_t  status;   //指令返回状态
	volatile at_mode_t  mode;       //模式
	unsigned short cmd_max_len;     //最大指令长度
	unsigned short ram_max_len;     //最大指令长度
	unsigned short revcnt;          //接收长度
	unsigned short revlen;          //接收长度
    volatile unsigned long flag;    //标志量
	at_fifo_t comrx;                //串口接收队列 
	at_fifo_t data;                 //模块接收到帧数据
	at_len_fifo_t comlen;           //串口接收帧长度队列 
	send_cb_t send;                 //发送函数
	#if NONE_OS_DELAY
	ticks_cb_t tick;                //系统心跳获取函数，用于非阻塞延时
	#else 
	delay_cb_t delay;               //延时函数，单位：ms
	#endif
}fc_at_t;

/**********************************************************************
  * @名称:   fc_at_mode_set
  * @描述:   模块传输模式设置
  * @参数:   *echo:结构体指针
             mode：模式，AT_MODE/PASS_MODE
  * @返回值: NULL
**********************************************************************/
void fc_at_mode_set(fc_at_t *echo,at_mode_t mode);

/**********************************************************************
  * @名称:   fc_at_mode_set
  * @描述:   获取模块传输模式
  * @参数:   *echo:结构体指针
  * @返回值: mode：模式，AT_MODE/PASS_MODE
**********************************************************************/
at_mode_t fc_at_mode_get(fc_at_t *echo);

/**********************************************************************
  * @名称:   fc_at_flag_clear
  * @描述:   清全部标志量
  * @参数:   *echo:结构体指针
  * @返回值: NULL
**********************************************************************/
void fc_at_flag_clear(fc_at_t *echo);

/**********************************************************************
  * @名称:   fc_at_flag_set
  * @描述:   设置标志量，最多32种
  * @参数:   *echo:结构体指针
  * @参数:   flag:第几位标志量
  * @参数:   sta:FLAG_SET/FLAG_REST
  * @返回值: NULL
**********************************************************************/
void fc_at_flag_set(fc_at_t *echo,unsigned char flag,at_flag_t sta);

/**********************************************************************
  * @名称:   fc_at_flag_get
  * @描述:   获取标志量，最多32种
  * @参数:   *echo:结构体指针
  * @参数:   flag:第几位标志量
  * @返回值: FLAG_SET/FLAG_REST
**********************************************************************/
at_flag_t fc_at_flag_get(fc_at_t *echo,unsigned char flag);

/**********************************************************************
  * @名称:   fc_at_cmd_wait
  * @描述:   指令返回结果
  * @参数:   *echo:结构体指针
  * @参数:   delayms：延时毫秒,10ms为一个单位
  * @返回值: NULL
**********************************************************************/
at_status_t fc_at_cmd_wait(fc_at_t *echo,const char *ackok,const char *ackerr,unsigned long delayms);

/**********************************************************************
  * @名称:   fc_at_input_cmd
  * @描述:   将带参数的指令写入cmd缓存中
  * @参数:   *echo:结构体指针
             *format：信息
  * @返回值: NULL
**********************************************************************/
void fc_at_input_cmd(fc_at_t *echo,char *format,...);

/**********************************************************************
  * @名称:   fc_at_send_cmd
  * @描述:   通用发送指令函数
  * @参数:   *echo:结构体指针
  * @参数:   *cmd:指令
  * @参数:   *ackok:正确响应
  * @参数:   *ackerr:错误响应
  * @参数:   *delayms:延时毫秒数
  * @返回值: NULL
**********************************************************************/
at_status_t fc_at_send_cmd(fc_at_t *echo,unsigned char *cmd,const char *ackok,const char *ackerr,unsigned int delayms);

#if NONE_OS_DELAY
/**********************************************************************
  * @名称:   fc_at_ticks_create
  * @描述:   系统心跳获取函数创建
  * @参数:   *echo:结构体指针
             tick：系统心跳获取函数
  * @返回值: NULL
**********************************************************************/
void fc_at_ticks_create(fc_at_t *echo,ticks_cb_t tick);
#else
/**********************************************************************
  * @名称:   fc_at_delay_create
  * @描述:   delay延时函数创建
  * @参数:   *echo:结构体指针
             delay：延时函数
  * @返回值: NULL
**********************************************************************/
void fc_at_delay_create(fc_at_t *echo,delay_cb_t delay);
#endif

/**********************************************************************
  * @名称:   fc_at_cmd_create
  * @描述:   at指令模块创建
  * @参数:   *echo:结构体指针
             *cmdbuf：发送指令缓存
             cmdmax：指令最大空间
             send：发送函数
  * @返回值: NULL
**********************************************************************/
void fc_at_cmd_create(fc_at_t *echo,unsigned char *cmdbuf,unsigned short cmdmax,send_cb_t send);

/**********************************************************************
  * @名称:   fc_at_ram_create
  * @描述:   at缓存模块创建
  * @参数:   *echo:结构体指针
			 *rambuf：接收缓存，用于指令返回比较
             rammax：接收缓存最大空间
  * @返回值: NULL
**********************************************************************/
void fc_at_ram_create(fc_at_t *echo,unsigned char *rambuf,unsigned short rammax);

/**********************************************************************
  * @名称:   fc_at_comrx_create
  * @描述:   at串口接收队列模块创建
  * @参数:   *echo:结构体指针
             *combuf：串口接收缓存
             commax：最大空间
  * @返回值: NULL
**********************************************************************/
void fc_at_comrx_create(fc_at_t *echo,unsigned char *combuf,unsigned short commax);

/**********************************************************************
  * @名称:   fc_at_data_create
  * @描述:   at 协议接收队列模块创建
  * @参数:   *echo:结构体指针
			 *databuf：TCP数据缓存
             datamax：接收缓存最大空间
  * @返回值: NULL
**********************************************************************/
void fc_at_data_create(fc_at_t *echo,unsigned char *databuf,unsigned short datamax);

/**********************************************************************
  * @名称:   fc_at_len_create
  * @描述:   at串口接收帧长度队列模块创建
  * @参数:   *echo:结构体指针
             *lenbuf：串口接收缓存
             lenmax：最大空间
  * @返回值: NULL
**********************************************************************/
void fc_at_len_create(fc_at_t *echo,unsigned short *lenbuf,unsigned short lenmax);

/**********************************************************************
  * @名称:   fc_at_recv_cb
  * @描述:   模块数据接收回调函数
  * @参数:   *echo:结构体指针
  * @参数:   *pbuf：数据指针
  * @返回值: true/false
**********************************************************************/
unsigned short fc_at_recv_cb(fc_at_t *echo,unsigned char *pbuf);

/**********************************************************************
  * @名称:   fc_at_usart_rx_cb
  * @描述:   串口数据接收存入队列,放在中断中
  * @参数:   *echo:结构体指针
  * @参数:   *buff：数据
  * @参数:   len：长度
  * @返回值: NULL
**********************************************************************/
void fc_at_usart_rx_cb(fc_at_t *echo,unsigned char *buff,unsigned short len);

/**********************************************************************
  * @名称:   fc_at_usart_rx_len_cb
  * @描述:   串口接收一帧数据长度存入队列
  * @参数:   *echo:结构体指针
  * @返回值: NULL
**********************************************************************/
void fc_at_usart_rx_len_cb(fc_at_t *echo);

/**********************************************************************
  * @名称:   fc_at_data_get
  * @描述:   获取队列中的数据，一般指从模块中接收到的帧协议
  * @参数:   *echo:结构体指针
  * @参数:   *buff:数据缓存指针
  * @参数:   len：长度
  * @返回值: 实际读取的长度
**********************************************************************/
unsigned short fc_at_data_get(fc_at_t *echo,unsigned char *buff,unsigned short len);

/**********************************************************************
  * @名称:   fc_at_data_write
  * @描述:   将数据写入队列中，一般指从模块中接收到的帧协议
  * @参数:   *echo:结构体指针
  * @参数:   *buff:数据缓存指针
  * @参数:   len：长度
  * @返回值: NULL
**********************************************************************/
void fc_at_data_write(fc_at_t *echo,unsigned char *buff,unsigned short len);

#endif
