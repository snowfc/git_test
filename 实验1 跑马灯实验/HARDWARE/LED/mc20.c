/*********************************SNOWFC********************************
*@文件:mc20.c
*@作者:FangChen
*@日期:2017年4月6日10:20:40
*@描述:MC20驱动
************************************************************************/
#include "mc20.h"
#include "usart.h"
#include "delay.h"
#include "fc_at_drv.h"
#include <string.h>
#include <stdio.h>

#if MC20_USE_OS
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#endif

fc_at_t mc20_at;

static uint8_t  mc20_cmd_buf[MC20_CMD_SIZE];
static uint8_t  mc20_ram_buf[MC20_RAM_SIZE];
static uint8_t  mc20_tcp_buf[MC20_TCP_SIZE];
static uint8_t  mc20_rx_buf[MC20_RX_SIZE];
static uint16_t mc20_rx_len[MC20_LEN_SIZE];

static uint8_t mc20_csq = 0;

static bool isInit = false;

#if MC20_USE_OS
static xSemaphoreHandle wait_send_mutex;
#endif

#if MC20_USE_DMA
static DMA_InitTypeDef DMA_InitStructure;
static uint8_t  dma_send_buf[MC20_TX_SIZE] = {0};
#endif


/**********************************************************************
 * @描述： 寻找字符串中第cx个endascii的位置，如+FTP: 0,5 找到','之后5的偏移位置
 * @参数： *dest：字符串
          endascii：遇到该字符结束
          cx:遇到几次endascii字符
 * @返回： NULL
**********************************************************************/
static unsigned short my_str_pos(unsigned char *dest,unsigned char endascii,unsigned char cx)
{
    unsigned char *p = dest;

    while(cx)
    {
        if(*dest < ' ' || *dest > '~') return 0XFF; //遇到非法字符,则不存在第cx个符号

        if(*dest == endascii) cx--;

        dest++;
    }

    return dest - p;
}

/**********************************************************************
  * @描述:   字符串拆解函数
  * @参数:   *dest:缓存
             destsize:缓存大小
			 *src：要拆解的函数
			 endascii：遇到该字符标记一次
  * @返回值:
**********************************************************************/
static unsigned short my_str_split(char *dest, unsigned short destsize, const char *src, char endascii)
{
    unsigned short nums = 0;

//  assert(dest != NULL && src != NULL);

    if(dest == NULL || src == NULL)
    {
        return 0;
    }

    while(*src != '\0' && nums < destsize)
    {
        if(*src == endascii) break;

        *dest++ = *src++;
        nums++;
    }

    return nums;
}

/**********************************************************************
  * @名称:   my_atoi
  * @描述:   字符串转整型函数
  * @参数:   *str:字符串
  * @返回值: result:整型数据
**********************************************************************/
static int my_atoi(char *str)
{
    char *p = str;
    int result = 0;

//  assert(str != NULL);

    if(str == NULL)
    {
        return 0;
    }

    if(*str == '+' || *str == '-')
    {
        str++;
    }

    if(!(*str >= '0' && *str <= '9'))
    {
        return 0;
    }

    while(*str != '\0')
    {
        if((*str < '0') || (*str > '9'))
        {
            break;
        }

        result = result * 10 + (*str - '0');

        str++;
    }

    if(*p == '-')
    {
        result = -result;
    }

    return result;
}

#if MC20_USE_DMA
/**********************************************************************
 * @描述： 串口发送数据，使用DMA
 * @参数： *buf：数据
          len：数据长度
 * @返回： NULL
**********************************************************************/
static void mc20_usart_send(uint8_t *buf, uint16_t len)
{
    if(!isInit) return;
	
//	while(DMA_GetFlagStatus(MC20_TX_DMA_FLAG_TC) != RESET);	/*等待DMA空闲*/
	
	memcpy(dma_send_buf, buf, len);		                    /*复制数据到DMA缓冲区*/
	
	DMA_InitStructure.DMA_BufferSize = len;
	DMA_Init(MC20_TX_DMA_CHANNEL, &DMA_InitStructure);	    /*重新初始化DMA数据流*/
	DMA_ITConfig(MC20_TX_DMA_CHANNEL, DMA_IT_TC, ENABLE);   /*开启DMA传输完成中断*/		
	USART_DMACmd(MC20_USART, USART_DMAReq_Tx, ENABLE);      /* 使能USART DMA TX请求 */
	USART_ClearFlag(MC20_USART, USART_FLAG_TC);		        /* 清除传输完成中断标志位 */
	DMA_Cmd(MC20_TX_DMA_CHANNEL, ENABLE);	                /* 使能DMA USART TX数据流 */
	
	xSemaphoreTake(wait_send_mutex, portMAX_DELAY);
}

#else 
/**********************************************************************
 * @描述： 串口发送数据
 * @参数： *buf：数据
          len：数据长度
 * @返回： NULL
**********************************************************************/
static void mc20_usart_send(uint8_t *buf, uint16_t len)
{
    if(!isInit) return;

    while(len--)
    {
		while ((MC20_USART->SR & USART_FLAG_TXE) == 0);
		
		MC20_USART->DR = (uint8_t)*buf++;	
    }
}

#endif

/**********************************************************************
 * @描述： mc20 at指令操作资源创建
 * @参数： NULL
 * @返回： NULL
**********************************************************************/
void mc20_at_create(void)
{
	fc_at_delay_create(&mc20_at,delay_ms);
	fc_at_cmd_create(&mc20_at,mc20_cmd_buf,MC20_CMD_SIZE,mc20_usart_send);
	fc_at_ram_create(&mc20_at,mc20_ram_buf,MC20_RAM_SIZE);
	fc_at_comrx_create(&mc20_at,mc20_rx_buf,MC20_RX_SIZE);
	fc_at_data_create(&mc20_at,mc20_tcp_buf,MC20_TCP_SIZE);
	fc_at_len_create(&mc20_at,mc20_rx_len,MC20_LEN_SIZE);
}

/**********************************************************************
 * @描述： mc20电源控制初始化
 * @参数： NULL
 * @返回： NULL
**********************************************************************/
static void mc20_power_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);	
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;				 
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD; 		 
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;		
	GPIO_Init(GPIOA, &GPIO_InitStructure);					
	GPIO_SetBits(GPIOA,GPIO_Pin_8);						

    MC20_POWER_OFF();
}

#if MC20_USE_DMA
/**********************************************************************
 * @描述： 串口DMA初始化
 * @参数： NULL
 * @返回： NULL
**********************************************************************/
static void mc20_usart_dma_init(void)
{
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
	
	//串口接收相应的DMA配置
	DMA_DeInit(DMA1_Channel5);
	while (DMA_GetCmdStatus(DMA1_Channel5) != DISABLE){}                    
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&MC20_USART->DR;
	DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)mc20_rx_buf; 
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
	DMA_InitStructure.DMA_BufferSize = MC20_RX_SIZE; 
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
	DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;
	DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
	DMA_Init(DMA1_Channel5, &DMA_InitStructure);
	DMA_Cmd (DMA1_Channel5,ENABLE);
		
	//串口发送相应的DMA配置
	DMA_DeInit(DMA1_Channel4);
	while (DMA_GetCmdStatus(DMA1_Channel4) != DISABLE){} 
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&MC20_USART->DR;
	DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)dma_send_buf; 
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
	DMA_InitStructure.DMA_BufferSize = MC20_TX_SIZE;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
	DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;
	DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
	DMA_Init(DMA1_Channel4, &DMA_InitStructure);
	DMA_Cmd (DMA1_Channel4,DISABLE);
}
#endif

/**********************************************************************
 * @描述： mc20串口初始化
 * @参数： NULL
 * @返回： NULL
**********************************************************************/
static void mc20_usart_init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1|RCC_APB2Periph_GPIOA, ENABLE);
  
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9; 
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;	
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	NVIC_InitStructure.NVIC_IRQChannel = MC20_USART_IRQ;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=3 ;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;		
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			
	NVIC_Init(&NVIC_InitStructure);
	
	USART_InitStructure.USART_BaudRate = MC20_BAUDRATE;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;	
	USART_Init(MC20_USART, &USART_InitStructure); 
	
	USART_ITConfig(MC20_USART, USART_IT_RXNE, ENABLE);//开启接收中断
	USART_ITConfig(MC20_USART, USART_IT_IDLE, ENABLE);//开启空闲中断
	
#if USART_USE_DMA
	mc20_usart_dma_init();
#endif
	
	USART_Cmd(MC20_USART, ENABLE);   
	
	USART_ClearFlag(MC20_USART, USART_FLAG_TC);
}

/**********************************************************************
 * @描述： mc20初始化
 * @参数： NULL
 * @返回： NULL
**********************************************************************/
void mc20_init(void)
{
	mc20_at_create();
	mc20_power_init();
	mc20_usart_init();
	
#if MC20_USE_OS
    wait_send_mutex = xSemaphoreCreateMutex(); 	   
#endif	
	
    isInit = true;
}


/*****************************************************************************************************************/


/**********************************************************************
 * @描述： mc20信号值（0-99）
 * @参数： NULL
 * @返回： 信号值
**********************************************************************/
uint8_t mc20_csq_val(void)
{
	return mc20_csq;
}

/**********************************************************************
 * @描述： mc20清标志量
 * @参数： NULL
 * @返回： NULL
**********************************************************************/
void mc20_flag_clear(void)
{
	mc20_at.flag = 0;
}

/**********************************************************************
 * @描述： mc20是否正常
 * @参数： NULL
 * @返回： NULL
**********************************************************************/
bool is_mc20_ready(void)
{
    return isInit;
}


/**********************************************************************
  * @名称:   mc20_send_atdata
  * @描述:   通过AT指令发送数据,指令，SIM800:AT+CIPSEND, MC20:AT+QISEND
  * @参数:   *data:发送的数据字符串
             len:数据长度，（最大字节不超过1024byte）
  * @返回值:  ACKNULL/ACKSUCCESS/ACKERROR/ACKTIMEOUT
**********************************************************************/
static at_status_t mc20_send_atdata(uint8_t *data, uint16_t len)
{
	at_status_t status = AT_STA_NULL;
	
	fc_at_input_cmd(&mc20_at,"AT+QISEND=%d\r\n",len);
	status = fc_at_send_cmd(&mc20_at,mc20_at.cmd,">","ERROR",AT_CMD_DELAY_2S);
	
	if(status == AT_STA_ERROR) return status;

    mc20_usart_send(data, len);
	
	status = fc_at_cmd_wait(&mc20_at,"SEND OK","ERROR",AT_CMD_DELAY_5S);
	
    return status;
}


/**********************************************************************
  * @名称:   gprs_send_data
  * @描述:   模块发送透传数据
  * @参数:   NULL
  * @返回值: NULL
**********************************************************************/
void gprs_send_data(uint8_t *buf, uint16_t len)
{
    if(mc20_at.mode == AT_MODE)
    {
		mc20_send_atdata(buf, len);
    }
    else
    {
        mc20_usart_send(buf, len);
    }
}


/**********************************************************************
  * @名称:   mc20_power_ready
  * @描述:   上电等待模块可用
  * @参数:   *succAck:返回正确字符串
             *errorAck:返回错误字符串
             waittime：等待超时时间
  * @返回值:  AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_power_ready(void)
{
    MC20_POWER_OFF();
    delay_ms(1000);
    MC20_POWER_ON();

    return fc_at_cmd_wait(&mc20_at,"SMS Ready","+CPIN: NOT INSERTED",AT_CMD_DELAY_30S);
}

/**********************************************************************
  * @名称:   mc20_at_test
  * @描述:   AT测试命令,最多5次测试
  * @参数:   NULL
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_at_test(void)
{
    uint8_t cnt = 3;
    at_status_t AckSta;

    while(cnt--)
    {
        AckSta = fc_at_send_cmd(&mc20_at,"AT\r\n","OK","ERROR",AT_CMD_DELAY_2S);

        if(AckSta == AT_STA_OK)
        {
            break;
        }
        else if(AckSta == AT_STA_ERROR)
        {
            delay_ms(1000);
        }
    }

    return AckSta;
}


/**********************************************************************
  * @名称:   mc20_at_ipr
  * @描述:   配置固定波特率
  * @参数:   NULL
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_at_ipr(uint32_t baudrate)
{
	fc_at_input_cmd(&mc20_at,"AT+IPR=%d&w\r\n",baudrate);

    return fc_at_send_cmd(&mc20_at,mc20_at.cmd,"OK","ERROR",AT_CMD_DELAY_2S);
}


/**********************************************************************
  * @名称:   mc20_set_echo
  * @描述:   关闭或开启命令回显
  * @参数:   mode:0关闭，1开启
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_set_echo(uint8_t mode)
{
    if(mode)
        return fc_at_send_cmd(&mc20_at,"ATE1\r\n","OK","ERROR",AT_CMD_DELAY_2S);
    else
        return fc_at_send_cmd(&mc20_at,"ATE0\r\n","OK","ERROR",AT_CMD_DELAY_2S);
}


/**********************************************************************
  * @名称:   mc20_at_csq
  * @描述:   查询信号强度，+CSQ: 30,0
  * @参数:   NULL
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_at_csq(void)
{
	char *p = NULL;
	uint32_t rssi,ber;
    at_status_t AckSta = AT_STA_NULL;

    AckSta = fc_at_send_cmd(&mc20_at,"AT+CSQ\r\n","+CSQ:","ERROR",AT_CMD_DELAY_2S);

    if(AckSta == AT_STA_OK)
    {
		p = strstr((const char*)mc20_at.ram,"+CSQ:");
		
		sscanf(p, "+CSQ:%d,%d", &rssi, &ber);
		
		if(rssi > 0 && rssi <32) mc20_csq = 113 - 2 * rssi;
		else mc20_csq = 113;
    }

    return AckSta;
}


/**********************************************************************
  * @名称:   mc20_at_ccid
  * @描述:   获取SIM卡CCID号
  * @参数:   CCID:SIM卡CCID号
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_at_ccid(uint8_t *CCID)
{
	uint8_t *poffset;
    at_status_t rev = AT_STA_NULL;

    rev = fc_at_send_cmd(&mc20_at,"AT+CCID\r\n","+CCID:","ERROR",AT_CMD_DELAY_2S);

    if(rev == AT_STA_OK)
    {
        poffset = (uint8_t*)strstr((const char*)mc20_at.ram, "+CCID:");

        memcpy(CCID, poffset + 8, 20);
    }

    return rev;
}


/**********************************************************************
  * @名称:   mc20_at_qimode
  * @描述:   设置AT模式还是透传模式
  * @参数:   mode:0.AT模式,1.透传模式
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_at_qimode(uint8_t mode)
{
    if(mode)
        return fc_at_send_cmd(&mc20_at,"AT+QIMODE=1\r\n","OK","ERROR",AT_CMD_DELAY_2S);
    else
        return fc_at_send_cmd(&mc20_at,"AT+QIMODE=0\r\n","OK","ERROR",AT_CMD_DELAY_2S);
}


/**********************************************************************
  * @名称:   mc20_at_qimux
  * @描述:   设置单路连接还是多路连接
  * @参数:   mode:1 多路连接，0 单路连接
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_at_qimux(uint8_t mode)
{
    if(mode)
        return fc_at_send_cmd(&mc20_at,"AT+QIMUX=1\r\n","OK","ERROR",AT_CMD_DELAY_2S);
    else
        return fc_at_send_cmd(&mc20_at,"AT+QIMUX=0\r\n","OK","ERROR",AT_CMD_DELAY_2S);
}


/**********************************************************************
  * @名称:   sim800_at_cpin
  * @描述:   查询SIM是否ready
  * @参数:   NULL
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_at_cpin(void)
{
    uint8_t cnt = 15;
    at_status_t AckSta = AT_STA_NULL;

    while(cnt--)
    {
        AckSta = fc_at_send_cmd(&mc20_at,"AT+CPIN?\r\n","+CPIN: READY","+CPIN: NOT INSERTED",AT_CMD_DELAY_2S);

        if(AckSta == AT_STA_OK)
        {
            break;
        }
        else if(AckSta == AT_STA_ERROR)
        {
            delay_ms(1000);
        }
    }

    return AckSta;
}

/**********************************************************************
  * @名称:   mc20_get_csq
  * @描述:   获取信号
  * @参数:   NULL
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_get_csq(void)
{
    uint8_t cnt = 15;
    at_status_t AckSta = AT_STA_NULL;

    while(cnt--)
    {
        AckSta = mc20_at_csq();

        if(AckSta == AT_STA_OK)
        {
			if(mc20_csq >= 51 && mc20_csq < 90)
			{
				break;
			}
			else
			{
				delay_ms(1000);
				AckSta = AT_STA_ERROR;
			}
        }
        else if(AckSta == AT_STA_ERROR)
        {
            delay_ms(1000);
        }
    }

    return AckSta;
}


/**********************************************************************
  * @名称:   mc20_at_creg
  * @描述:   查询是否注册到GSM
  * @参数:   NULL
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_at_creg(void)
{
    uint8_t cnt = 30;
    at_status_t AckSta = AT_STA_NULL;

    while(cnt--)
    {
        AckSta = fc_at_send_cmd(&mc20_at,"AT+CREG?\r\n","+CREG: 0,1","+CREG: 0,0",AT_CMD_DELAY_2S);

        if(AckSta == AT_STA_OK)
        {
            break;
        }
        else if(AckSta == AT_STA_ERROR)
        {
            delay_ms(1000);
        }
		else 
		{
			if(strstr((const char*)mc20_at.ram,"+CREG: 0,5") != NULL)
			{
				AckSta = AT_STA_OK;
				break;
			}
		}
    }

    return AckSta;
}

/**********************************************************************
  * @名称:   mc20_at_cgreg
  * @描述:   查询是否注册到GPRS
  * @参数:   NULL
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_at_cgreg(void)
{
    uint8_t cnt = 30;
    at_status_t AckSta = AT_STA_NULL;

    while(cnt--)
    {
        AckSta = fc_at_send_cmd(&mc20_at,"AT+CGREG?\r\n","+CGREG: 0,1","+CGREG: 0,0",AT_CMD_DELAY_2S);

        if(AckSta == AT_STA_OK)
        {
            break;
        }
        else if(AckSta == AT_STA_ERROR)
        {
            delay_ms(1000);
        }
		else 
		{
			if(strstr((const char*)mc20_at.ram,"+CGREG: 0,5") != NULL)
			{
				AckSta = AT_STA_OK;
				break;
			}
		}
    }

    return AckSta;
}


/**********************************************************************
  * @名称:   mc20_at_cgatt
  * @描述:   激活/关闭GPRS服务
  * @参数:   mode:1激活，0关闭
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_at_cgatt(uint8_t mode)
{
    if(mode)
        return fc_at_send_cmd(&mc20_at,"AT+CGATT=1\r\n","OK","ERROR",AT_CMD_DELAY_60S);
    else
        return fc_at_send_cmd(&mc20_at,"AT+CGATT=0\r\n","OK","ERROR",AT_CMD_DELAY_60S);
}


/**********************************************************************
  * @名称:   mc20_at_cgdcont
  * @描述:   定义PDP上下文
  * @参数:   *APN：中国移动"CMNET",中国联通"UNINET"
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_at_cgdcont(char *APN)
{
	fc_at_input_cmd(&mc20_at,"AT+CGDCONT=1,\"IP\",\"%s\"\r\n",APN);

    return fc_at_send_cmd(&mc20_at,mc20_at.cmd,"OK","ERROR",AT_CMD_DELAY_2S);
}


/**********************************************************************
  * @名称:   mc20_at_qicsgp
  * @描述:   Select CSD or GPRS as the Bearer
  * @参数:   *APN：中国移动"CMNET",中国联通"UNINET"
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_at_qicsgp(uint8_t mode, char *APN)
{
	fc_at_input_cmd(&mc20_at,"AT+QICSGP=%d,\"%s\"\r\n",mode, APN);

    return fc_at_send_cmd(&mc20_at,mc20_at.cmd,"OK","ERROR",AT_CMD_DELAY_2S);
}


/**********************************************************************
  * @名称:   mc20_at_qifgcnt
  * @描述:   设置前台GPRS/CSD场景
  * @参数:   mode:1激活，0关闭
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_at_qifgcnt(uint8_t context)
{
    fc_at_input_cmd(&mc20_at,"AT+QIFGCNT=%d\r\n",context);

    return fc_at_send_cmd(&mc20_at,mc20_at.cmd,"OK","ERROR",AT_CMD_DELAY_2S);
}


/**********************************************************************
  * @名称:   mc20_at_qifgcnt
  * @描述:   激活/关闭PDP上下文
  * @参数:   mode:1激活，0关闭
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_at_cgact(uint8_t mode)
{
    if(mode)
        return fc_at_send_cmd(&mc20_at,"AT+CGACT=1\r\n","OK","ERROR",AT_CMD_DELAY_60S);
    else
        return fc_at_send_cmd(&mc20_at,"AT+CGACT=0\r\n","OK","ERROR",AT_CMD_DELAY_60S);
}


/**********************************************************************
  * @名称:   mc20_at_qihead
  * @描述:   设置TCP回应消息头
  * @参数:   NULL
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_at_qihead(void)
{
    return fc_at_send_cmd(&mc20_at,"AT+QIHEAD=1\r\n","OK","ERROR",AT_CMD_DELAY_2S);
}


/**********************************************************************
  * @名称:   mc20_at_qiact
  * @描述:   激活GPRS/CSD上下文
  * @参数:   NULL
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_at_qiact(void)
{
    return fc_at_send_cmd(&mc20_at,"AT+QIACT\r\n","OK","ERROR",AT_CMD_DELAY_60S);
}


/**********************************************************************
  * @名称:   mc20_at_qiregapp
  * @描述:   激活GPRS/CSD上下文
  * @参数:   NULL
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_at_qiregapp(void)
{
    return fc_at_send_cmd(&mc20_at,"AT+QIREGAPP\r\n","OK","ERROR",AT_CMD_DELAY_2S);
}


/**********************************************************************
  * @名称:   mc20_at_qideact
  * @描述:   激活GPRS/CSD上下文
  * @参数:   NULL
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_at_qideact(void)
{
    return fc_at_send_cmd(&mc20_at,"AT+QIDEACT\r\n","DEACT OK","ERROR",AT_CMD_DELAY_60S);
}


/**********************************************************************
  * @名称:   mc20_at_qidsnip
  * @描述:   修改服务器地址格式
  * @参数:   mode：0 IP格式，1 域名格式
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_at_qidsnip(uint8_t mode)
{
    if(mode)
        return fc_at_send_cmd(&mc20_at,"AT+QIDNSIP=1\r\n","OK","ERROR",AT_CMD_DELAY_2S);
    else
        return fc_at_send_cmd(&mc20_at,"AT+QIDNSIP=0\r\n","OK","ERROR",AT_CMD_DELAY_2S);
}


/**********************************************************************
  * @名称:   mc20_at_qiopen
  * @描述:   修改服务器地址格式
  * @参数:   service_type: "TCP"、"UDP"、"TCP LISTENER"、"UDP SERVICE"
  *         ip_addr:服务器IP地址
  *         ip_port:服务器端口号，range is 0-65535
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_at_qiopen(char *service_type, char *ip_addr, uint16_t ip_port)
{
    fc_at_input_cmd(&mc20_at,"AT+QIOPEN=\"%s\",\"%s\",%d\r\n", service_type, ip_addr, ip_port);

    return fc_at_send_cmd(&mc20_at,mc20_at.cmd,"CONNECT OK","CONNECT FAIL",AT_CMD_DELAY_60S);
}


/**********************************************************************
  * @名称:   mc20_at_qiclose
  * @描述:   关闭TCP连接
  * @参数:   NULL
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_at_qiclose(void)
{
    return fc_at_send_cmd(&mc20_at,"AT+QICLOSE\r\n","CLOSE OK","ERROR",AT_CMD_DELAY_30S);
}

/*****************************************************************************GNSS定位***********************************************************************************************/

/**********************************************************************
  * @名称:   mc20_at_qgnssepo
  * @描述:   使能 EPO功能
  * @参数:   mode:0关闭，1开启
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_at_qgnssepo(uint8_t mode)
{
    if(mode)
        return fc_at_send_cmd(&mc20_at,"AT+QGNSSEPO=1\r\n","OK","ERROR",AT_CMD_DELAY_2S);
    else
        return fc_at_send_cmd(&mc20_at,"AT+QGNSSEPO=0\r\n","OK","ERROR",AT_CMD_DELAY_2S);
}

/**********************************************************************
  * @名称:   mc20_at_qcellloc
  * @描述:   获取基站定位信息
            +QCELLLOC: 120.014236,30.281816
  * @参数:   *gpsinfo：定位信息
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_at_qcellloc(lbs_info_t *lbs)
{
	uint8_t *p = NULL;
	uint8_t posx = 0;
	at_status_t rev = AT_STA_NULL;
//	uint8_t dx = 0;

    rev = fc_at_send_cmd(&mc20_at,"AT+QCELLLOC=1\r\n","+QCELLLOC:","ERROR",AT_CMD_DELAY_15S);

    if(rev == AT_STA_OK)
    {
		p = (uint8_t*)strstr((const char*)mc20_at.ram,"+QCELLLOC:");
		posx = my_str_pos(p,':',1);
		if(posx != 0XFF) 
		{
			my_str_split((char*)lbs->log,sizeof(lbs->log),(const char*)(p+posx+1),',');
//			lbs->log = nmea_str2num(p + posx + 1, &dx);
		}
		posx = my_str_pos(p,',',1);
		if(posx != 0XFF) 
		{
			lbs->sta = 1;//定位成功
			my_str_split((char*)lbs->lat,sizeof(lbs->lat),(const char*)(p+posx),0x0D);
//			lbs->lat = nmea_str2num(p + posx, &dx);	
		}
    }
	else lbs->sta = 0;

    return rev;
}


/**********************************************************************
  * @名称:   mc20_at_qgrefloc
  * @描述:   设置经纬度
  * @参数:   log:经度
             lat：纬度
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_at_qgrefloc(uint32_t log, uint32_t lat)
{
    fc_at_input_cmd(&mc20_at,"AT+QGREFLOC=%.6f,%.6f\r\n", (float)lat/1000000, (float)log/1000000);

    return fc_at_send_cmd(&mc20_at,mc20_at.cmd,"OK","ERROR",AT_CMD_DELAY_5S);
}


/**********************************************************************
  * @名称:   mc20_at_qgepoadid
  * @描述:   使能 EPO功能后出发秒定功能
  * @参数:   NULL
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_at_qgepoadid(void)
{
    return fc_at_send_cmd(&mc20_at,"AT+QGEPOAID\r\n","OK","ERROR",AT_CMD_DELAY_2S);
}


/**********************************************************************
  * @名称:   mc20_at_qgnssc
  * @描述:   开启/关闭GNSS功能
  * @参数:   mode:0关闭，1开启
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_at_qgnssc(u8 mode)
{
    if(mode)
        return fc_at_send_cmd(&mc20_at,"AT+QGNSSC=1\r\n","OK","ERROR",AT_CMD_DELAY_2S);
    else
        return fc_at_send_cmd(&mc20_at,"AT+QGNSSC=0\r\n","OK","ERROR",AT_CMD_DELAY_2S);
}


/**********************************************************************
  * @名称:   mc20_at_qgnssts
  * @描述:   查询时间同步状态，+QGNSSTS: 1同步，+QGNSSTS: 0未同步
  * @参数:   NULL
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_at_qgnssts(void)
{
    uint8_t cnt = 10;
    at_status_t AckSta = AT_STA_NULL;

    while(cnt--)
    {
        AckSta = fc_at_send_cmd(&mc20_at,"AT+QGNSSTS?\r\n","+QGNSSTS: 1","+QGNSSTS: 0",AT_CMD_DELAY_2S);

        if(AckSta == AT_STA_OK)
        {
            break;
        }
        else if(AckSta == AT_STA_ERROR)
        {
            delay_ms(1000);
        }
    }

    return AckSta;
}


/**********************************************************************
  * @名称:   mc20_at_qgnssrd
  * @描述:   获取定位信息
            +QGNSSRD: $GNGGA,022022.000,3016.9943,N,12000.7162,E,1,8,1.79,27.6,M,7.0,M,,*43
            +QGNSSRD: $GNRMC,021923.000,A,3016.9825,N,12000.7147,E,0.00,185.72,150217,,,A*76
  * @参数:   *querymode:以什么模式查询，(“GGA”,“RMC”,“GSV”,“GSA”,“VTG”,“GNS”)
            *buff：定位信息
  * @返回值: AT_STA_NULL/AT_STA_OK/AT_STA_ERROR/AT_STA_TIMEOUT
**********************************************************************/
at_status_t mc20_at_qgnssrd(char *querymode, uint8_t *buf)
{
    at_status_t AckSta = AT_STA_NULL;
    uint8_t *poffset;

    if(strncmp(querymode, "ALL", 3) == 0)
    {
        AckSta = fc_at_send_cmd(&mc20_at,"AT+QGNSSRD?\r\n","+QGNSSRD:","ERROR:",AT_CMD_DELAY_2S);
    }
    else
    {
        fc_at_input_cmd(&mc20_at,"AT+QGNSSRD=\"%s\"\r\n", querymode);

        AckSta = fc_at_send_cmd(&mc20_at,mc20_at.cmd,"+QGNSSRD:","ERROR:",AT_CMD_DELAY_2S);
    }

    if(AckSta == AT_STA_OK)
    {
        poffset = (uint8_t*)strstr((const char*)mc20_at.ram, "+QGNSSRD:");

        strcpy((char*)buf, (const char*)(poffset + 10));
    }

    return AckSta;
}

/**********************************************************************
  * @描述:   判断是否为闰年
  * @参数:   year:要判断的年份
  * @返回值: 1：闰年，0：平年
**********************************************************************/
static uint8_t is_leap_year(uint16_t year)
{
    if(year % 400 == 0 || (year % 4 == 0 && year % 100 != 0))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/**********************************************************************
  * @名称:   lsb_time_adjust
  * @描述:   格林尼治时间转换成北京时间，hour+8
  * @参数:   Time：待转换的时间
  * @返回值:  NULL
**********************************************************************/
void lsb_time_adjust(lbs_time_t *lbstime)
{
	uint8_t nLeapMonTable[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; //平年月份表
	uint8_t  LeapMonTable[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; //闰年月份表

    lbstime->hour += 8;

    if(lbstime->hour >= 24)
    {
        lbstime->hour -= 24;
        lbstime->date++;

        if(lbstime->date > nLeapMonTable[lbstime->month - 1] 
			&& is_leap_year(2000 + lbstime->year) == 0)
        {
            lbstime->date = 1;
            lbstime->month++;
        }
        else if(lbstime->date > LeapMonTable[lbstime->month - 1] 
			&& is_leap_year(2000 + lbstime->year) == 1)
        {
            lbstime->date = 1;
            lbstime->month++;
        }

        if(lbstime->month > 12)
        {
            lbstime->year++;
        }
    }
}

/**********************************************************************
  * @名称:   mc20_cmd_mode_analysis
  * @描述:   AT模式接收解析
  * @参数:   NULL
  * @返回值: NULL
**********************************************************************/
void mc20_rx_ipd_analysis(void)
{
	uint8_t *p = NULL;
	uint8_t  posx = 0;
	uint16_t rxlen = 0;
	
	rxlen = fc_at_recv_cb(&mc20_at,mc20_at.ram);
	
	if(rxlen != 0)
	{
		p = (uint8_t*)strstr((const char*)mc20_at.ram,"IPD");//IPD<data length>:data
		if(p != NULL)
		{
			posx = my_str_pos(p,':',1);
			if(posx != 0xFF) 
			{
				rxlen = my_atoi((char*)(p+3));
				fc_at_data_write(&mc20_at,p+posx,rxlen);
			}
		}
//		else if(strstr((const char*)mc20_at.ram, "CLOSED") != NULL))
//		{
//			fc_at_flag_set(&mc20_at,MC20_TCP_FLAG,FLAG_RESET);
//			fc_at_flag_set(&mc20_at,MC20_RELINK_FLAG,FLAG_SET);
//		}
//		else if(strstr((const char*)mc20_at.ram, "SMS Ready") != NULL)//模块可能重启了
//		{
//			fc_at_flag_clear();
//			fc_at_flag_set(&mc20_at,MC20_RESET_FLAG,FLAG_SET);
//		}
	}
}

#if MC20_USE_DMA

/**********************************************************************
  * @名称:   MC20_USART_IRQHandler
  * @描述:   串口接收中断
  * @参数:   NULL
  * @返回值: NULL
**********************************************************************/
void MC20_USART_IRQHandler(void)
{
    uint8_t rx_data_isr = 0;

    if(USART_GetITStatus(MC20_USART, USART_IT_IDLE) != RESET)
    {
        rx_data_isr = MC20_USART->SR;
		rx_data_isr = MC20_USART->DR;
		
		fc_at_usart_rx_len_cb(&mc20_at);
    }
}


/**********************************************************************
  * @名称:   MC20_RX_DMA_IRQHandler
  * @描述:   串口DMA接收中断
  * @参数:   NULL
  * @返回值: NULL
**********************************************************************/
void MC20_RX_DMA_IRQHandler(void)
{
    if(DMA_GetITStatus(MC20_RX_DMA_FLAG_TC) != RESET)
    {
        DMA_ClearITPendingBit(MC20_RX_DMA_FLAG_TC);
    }
}

/**********************************************************************
  * @名称:   MC20_TX_DMA_IRQHandler
  * @描述:   串口DMA发送中断
  * @参数:   NULL
  * @返回值: NULL
**********************************************************************/
void MC20_TX_DMA_IRQHandler(void)
{
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
	
    if(DMA_GetITStatus(MC20_TX_DMA_FLAG_TC) != RESET)
    {
		DMA_ITConfig(MC20_TX_DMA_CHANNEL, DMA_IT_TC, DISABLE);
        DMA_ClearITPendingBit(MC20_TX_DMA_FLAG_TC);
        DMA_Cmd(MC20_TX_DMA_CHANNEL, DISABLE);	

		xSemaphoreGiveFromISR(wait_until_send_done, &xHigherPriorityTaskWoken);
    }
}

#else

/**********************************************************************
  * @名称:   MC20_USART_IRQHandler
  * @描述:   串口接收中断
  * @参数:   NULL
  * @返回值: NULL
**********************************************************************/
void MC20_USART_IRQHandler(void)
{
    uint8_t rx_data_isr = 0;

    if(USART_GetITStatus(MC20_USART, USART_IT_RXNE) != RESET)
    {
        rx_data_isr = USART_ReceiveData(MC20_USART) & 0x00FF;
		
		fc_at_usart_rx_cb(&mc20_at,&rx_data_isr,1);

		USART_ClearITPendingBit(MC20_USART, USART_IT_RXNE);
    }
	
	else if(USART_GetITStatus(MC20_USART, USART_IT_IDLE) != RESET)
	{
		rx_data_isr = MC20_USART->SR;
		rx_data_isr = MC20_USART->DR;
		
		fc_at_usart_rx_len_cb(&mc20_at);
	}
}

#endif




