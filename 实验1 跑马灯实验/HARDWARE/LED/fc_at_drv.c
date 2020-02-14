#include "fc_at_drv.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>


/**********************************************************************
  * @名称:   fc_at_fifo_get
  * @描述:   fifo队列中获取数据
  * @参数:   *fifo：队列指针
             *buffer：缓存地址
             len：获取长度
  * @返回值: NULL
**********************************************************************/
static unsigned short fc_at_fifo_get(at_fifo_t *fifo, unsigned char *buffer, unsigned short len)
{
	unsigned short i;
	unsigned short lenght;
	
	lenght = (fifo->in + fifo->size - fifo->out) % fifo->size;
	
	if(lenght == 0) return 0;
	
	if(lenght > len) lenght = len;
	
	for(i = 0; i < lenght; i++)
	{
		buffer[i] = fifo->buff[(fifo->out + i) % fifo->size];
	}
	
	fifo->out = (fifo->out + lenght) % fifo->size;
	
	return lenght;
}

/**********************************************************************
  * @名称:   fc_at_fifo_write
  * @描述:   将数据写入fifo队列中
  * @参数:   *fifo：队列指针
             *buffer：缓存地址
             len：写入长度
  * @返回值: NULL
**********************************************************************/
static unsigned short fc_at_fifo_write(at_fifo_t *fifo, unsigned char *buffer, unsigned short len)
{
	unsigned short i;
	unsigned short lenght;
	
	lenght = (fifo->out + fifo->size - fifo->in - 1) % fifo->size;
	
	if(lenght < len) return 0;
	
	for(i = 0; i < len; i++)
	{
		fifo->buff[(fifo->in + i) % fifo->size] = buffer[i];
	}
	
	fifo->in = (fifo->in + len) % fifo->size;
	
	return len;
}


/**********************************************************************
  * @名称:   fc_at_fifo_get
  * @描述:   fifo队列中获取数据
  * @参数:   *fifo：队列指针
             *buffer：缓存地址
             len：获取长度
  * @返回值: NULL
**********************************************************************/
static unsigned char fc_at_len_fifo_get(at_len_fifo_t *fifo, unsigned short *buffer)
{
	unsigned short lenght;
	
	lenght = (fifo->in + fifo->size - fifo->out) % fifo->size;
	
	if(lenght == 0) return 0;
	
	*buffer = fifo->buff[fifo->out % fifo->size];
	
	fifo->out = (fifo->out + 1) % fifo->size;
	
	return 1;
}

/**********************************************************************
  * @名称:   fc_at_fifo_write
  * @描述:   将数据写入fifo队列中
  * @参数:   *fifo：队列指针
             *buffer：缓存地址
             len：写入长度
  * @返回值: NULL
**********************************************************************/
static unsigned char fc_at_len_fifo_write(at_len_fifo_t *fifo, unsigned short buffer)
{
	unsigned short lenght;
	
	lenght = (fifo->out + fifo->size - fifo->in - 1) % fifo->size;
	
	if(lenght < 1) return 0;
	
	fifo->buff[fifo->in % fifo->size] = buffer;
	
	fifo->in = (fifo->in + 1) % fifo->size;
	
	return 1;
}

/**********************************************************************
  * @名称:   fc_at_mode_set
  * @描述:   模块传输模式设置
  * @参数:   *echo:结构体指针
             mode：模式，AT_MODE/PASS_MODE
  * @返回值: NULL
**********************************************************************/
void fc_at_mode_set(fc_at_t *echo,at_mode_t mode)
{
	echo->mode = mode;
	echo->revcnt = 0;
}

/**********************************************************************
  * @名称:   fc_at_mode_set
  * @描述:   获取模块传输模式
  * @参数:   *echo:结构体指针
  * @返回值: mode：模式，AT_MODE/PASS_MODE
**********************************************************************/
at_mode_t fc_at_mode_get(fc_at_t *echo)
{
	return echo->mode;
}

/**********************************************************************
  * @名称:   fc_at_flag_clear
  * @描述:   清全部标志量
  * @参数:   *echo:结构体指针
  * @返回值: NULL
**********************************************************************/
void fc_at_flag_clear(fc_at_t *echo)
{
	echo->flag  = 0;
}

/**********************************************************************
  * @名称:   fc_at_flag_set
  * @描述:   设置标志量，最多32种
  * @参数:   *echo:结构体指针
  * @参数:   flag:第几位标志量
  * @参数:   sta:FLAG_SET/FLAG_REST
  * @返回值: NULL
**********************************************************************/
void fc_at_flag_set(fc_at_t *echo,unsigned char flag,at_flag_t sta)
{
	if(sta == FLAG_SET)
	{
		echo->flag |= 1<<flag;
	}
	else
	{
		echo->flag &= 0<<flag;
	}
}

/**********************************************************************
  * @名称:   fc_at_flag_get
  * @描述:   获取标志量，最多32种
  * @参数:   *echo:结构体指针
  * @参数:   flag:第几位标志量
  * @返回值: FLAG_SET/FLAG_REST
**********************************************************************/
at_flag_t fc_at_flag_get(fc_at_t *echo,unsigned char flag)
{
	if((echo->flag >>flag) & 0x00000001)
	{
		return FLAG_SET;
	}
	else
	{
		return FLAG_REST;
	}
}

#if NONE_OS_DELAY
/**********************************************************************
  * @名称:   fc_at_ticks_create
  * @描述:   系统心跳获取函数创建
  * @参数:   *echo:结构体指针
             tick：系统心跳获取函数
  * @返回值: NULL
**********************************************************************/
void fc_at_ticks_create(fc_at_t *echo,ticks_cb_t tick)
{
	echo->tick = tick;
}

/**********************************************************************
  * @名称:   fc_at_delay
  * @描述:   系统非阻塞延时
  * @参数:   *echo:结构体指针
			 *tickstart：开始标记时间
             delayms：延时多少ms
  * @返回值: 0：延时未到，1：延时到达
**********************************************************************/
static unsigned char fc_at_delay(fc_at_t *echo,unsigned long *tickstart,unsigned long delayms)
{
	if(*tickstart)
	{
		if((echo->tick() - *tickstart) < delayms) 
			return 0;
		else 
		{
			*tickstart = 0;
			return 1;
		}	
	}
	else
	{
		*tickstart = echo->tick();
		return 0;
	}
}
#else
/**********************************************************************
  * @名称:   fc_at_delay_create
  * @描述:   delay延时函数创建
  * @参数:   *echo:结构体指针
             delay：延时函数
  * @返回值: NULL
**********************************************************************/
void fc_at_delay_create(fc_at_t *echo,delay_cb_t delay)
{
	echo->delay = delay;
}
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
void fc_at_cmd_create(fc_at_t *echo,unsigned char *cmdbuf,unsigned short cmdmax,send_cb_t send)
{
	echo->status = AT_STA_NULL;
	echo->mode = AT_MODE;
	echo->cmd = cmdbuf;
	echo->cmd_max_len = cmdmax;
	echo->flag = 0;
	echo->send = send;
}

/**********************************************************************
  * @名称:   fc_at_ram_create
  * @描述:   at缓存模块创建
  * @参数:   *echo:结构体指针
			 *rambuf：接收缓存，用于指令返回比较
             rammax：接收缓存最大空间
  * @返回值: NULL
**********************************************************************/
void fc_at_ram_create(fc_at_t *echo,unsigned char *rambuf,unsigned short rammax)
{
	echo->ram = rambuf;
	echo->ram_max_len = rammax;
}


/**********************************************************************
  * @名称:   fc_at_comrx_create
  * @描述:   at串口接收队列模块创建
  * @参数:   *echo:结构体指针
             *combuf：串口接收缓存
             commax：最大空间
  * @返回值: NULL
**********************************************************************/
void fc_at_comrx_create(fc_at_t *echo,unsigned char *combuf,unsigned short commax)
{
	echo->comrx.buff = combuf;
	echo->comrx.size = commax;
	echo->comrx.in = 0;
	echo->comrx.out = 0;
	
	echo->revlen = 0;
	echo->revcnt = 0;
}

/**********************************************************************
  * @名称:   fc_at_data_create
  * @描述:   at 协议接收队列模块创建
  * @参数:   *echo:结构体指针
			 *databuf：有效数据缓存
             datamax：接收缓存最大空间
  * @返回值: NULL
**********************************************************************/
void fc_at_data_create(fc_at_t *echo,unsigned char *databuf,unsigned short datamax)
{
	echo->data.buff = databuf;
	echo->data.size = datamax;
	echo->data.in = 0;
	echo->data.out = 0;
}

/**********************************************************************
  * @名称:   fc_at_len_create
  * @描述:   at串口接收帧长度队列模块创建
  * @参数:   *echo:结构体指针
             *lenbuf：串口接收缓存
             lenmax：最大空间
  * @返回值: NULL
**********************************************************************/
void fc_at_len_create(fc_at_t *echo,unsigned short *lenbuf,unsigned short lenmax)
{
	echo->comlen.buff = lenbuf;
	echo->comlen.size = lenmax;
	echo->comlen.in = 0;
	echo->comlen.out = 0;
}


/**********************************************************************
  * @名称:   fc_at_cmd_wait
  * @描述:   指令返回结果
  * @参数:   *echo:结构体指针
  * @参数:   delayms：延时毫秒,10ms为一个单位
  * @返回值: NULL
**********************************************************************/
at_status_t fc_at_cmd_wait(fc_at_t *echo,const char *ackok,const char *ackerr,unsigned long delayms)
{
#if NONE_OS_DELAY
	static unsigned long start_ticks = 0;
	
	if(!fc_at_delay(echo,&start_ticks,delayms))
	{
		if(fc_at_len_fifo_get(&echo->comlen,&echo->revlen))
		{
			fc_at_fifo_get(&echo->comrx,echo->ram,echo->revlen);
			
			echo->ram[echo->revlen] = 0;
			
			if(strstr((char*)echo->ram,ackok) != NULL)
			{
				start_ticks = 0;
				return AT_STA_OK;
			}
			else if(strstr((char*)echo->ram,ackerr) != NULL)
			{
				start_ticks = 0;
				return AT_STA_ERROR;
			}
		}
		return AT_STA_NULL;
	}
	else return AT_STA_TIMEOUT;
#else
	while(delayms--)
	{
		if(fc_at_len_fifo_get(&echo->comlen,&echo->revlen))
		{
			fc_at_fifo_get(&echo->comrx,echo->ram,echo->revlen);
			
			echo->ram[echo->revlen] = 0;
			
			if(strstr((char*)echo->ram,ackok) != NULL)
			{
				return AT_STA_OK;
			}
			else if(strstr((char*)echo->ram,ackerr) != NULL)
			{
				return AT_STA_ERROR;
			}
		}
		echo->delay(1);
	}
	return AT_STA_TIMEOUT;	
#endif	
}

/**********************************************************************
  * @名称:   fc_at_input_cmd
  * @描述:   将带参数的指令写入cmd缓存中
  * @参数:   *echo:结构体指针
             *format：信息
  * @返回值: NULL
**********************************************************************/
void fc_at_input_cmd(fc_at_t *echo,char *format,...)
{
	unsigned short cmd_len = 0;
	va_list args;
	
	va_start(args, format);
	
    cmd_len = vsnprintf((char*)echo->cmd, echo->cmd_max_len, format, args);
	
	va_end(args);
	
	echo->cmd[cmd_len] = 0;
	
//	echo->send(echo->cmd, cmd_len);
}

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
at_status_t fc_at_send_cmd(fc_at_t *echo,unsigned char *cmd,const char *ackok,const char *ackerr,unsigned int delayms)
{
	echo->send(echo->cmd, strlen((const char*)cmd));
	
	return fc_at_cmd_wait(echo,ackok,ackerr,delayms);
}

/**********************************************************************
  * @名称:   fc_at_recv_cb
  * @描述:   模块数据接收回调函数
  * @参数:   *echo:结构体指针
  * @参数:   *pbuf：数据指针
  * @返回值: true/false
**********************************************************************/
unsigned short fc_at_recv_cb(fc_at_t *echo,unsigned char *pbuf)
{
	unsigned short revlen = 0;
	
	if(fc_at_len_fifo_get(&echo->comlen,&echo->revlen))
	{
		revlen = fc_at_fifo_get(&echo->comrx,pbuf,echo->revlen);
		
		return revlen;
	}
	return 0;
}

/**********************************************************************
  * @名称:   fc_at_usart_rx_cb
  * @描述:   串口数据接收存入队列,放在中断中
  * @参数:   *echo:结构体指针
  * @参数:   *buff：数据
  * @参数:   len：长度
  * @返回值: NULL
**********************************************************************/
void fc_at_usart_rx_cb(fc_at_t *echo,unsigned char *buff,unsigned short len)
{
	if(echo->mode == AT_MODE)
	{
		if(fc_at_fifo_write(&echo->comrx,buff,len))
		{
			echo->revcnt += len;
		}
	}
	else
	{
		fc_at_fifo_write(&echo->data,buff,len);
	}
}

/**********************************************************************
  * @名称:   fc_at_usart_rx_len_cb
  * @描述:   串口接收一帧数据长度存入队列
  * @参数:   *echo:结构体指针
  * @返回值: NULL
**********************************************************************/
void fc_at_usart_rx_len_cb(fc_at_t *echo)
{
	if(echo->mode == AT_MODE)
	{
		fc_at_len_fifo_write(&echo->comlen,echo->revcnt);
		echo->revcnt = 0;
	}
}

/**********************************************************************
  * @名称:   fc_at_data_get
  * @描述:   获取队列中的数据，一般指从模块中接收到的帧协议
  * @参数:   *echo:结构体指针
  * @参数:   *buff:数据缓存指针
  * @参数:   len：长度
  * @返回值: 实际读取的长度
**********************************************************************/
unsigned short fc_at_data_get(fc_at_t *echo,unsigned char *buff,unsigned short len)
{
	return fc_at_fifo_get(&echo->data,buff,len);
}

/**********************************************************************
  * @名称:   fc_at_data_write
  * @描述:   将数据写入队列中，一般指从模块中接收到的帧协议
  * @参数:   *echo:结构体指针
  * @参数:   *buff:数据缓存指针
  * @参数:   len：长度
  * @返回值: NULL
**********************************************************************/
void fc_at_data_write(fc_at_t *echo,unsigned char *buff,unsigned short len)
{
	fc_at_fifo_write(&echo->data,buff,len);
}



