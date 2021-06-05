#include "delay.h"
#include "sys.h"
#include "usart.h"
#include "usart3.h"
#include "esp8266.h"
#include "string.h"
#include "timer.h"
#include "led.h"
#include "exti.h"
#include "oled.h"

/*
项目的主要内容：STM32配合ESP8266模块与服务器数据交互
当开发板上电后，可在串口助手上查看现象。
按下按键（PA0）后，可以再次获取当前天气并通过串口打印。

MCU     ESP8266
3.3V	   VCC
GND	     GND
PB10	   RXD
PB11	   TXD
3.3V	   IO
3.3V	   RST
--------------------------------
MCU     USB转TTL
5V			 VCC
GND			 GND
PA9			 RXD
PA10		 TXD
--------------------------------
MCU     OLED
5V			 VCC
GND			 GND
PB15		 SDA
PB13		 SCL
*/

u8 flag = 0;
char t;
extern Results results[];
extern nt_calendar_obj nwt;								//定义结构体变量
int hour,min,sec;										//时间变量

int main(void)
{
	
	delay_init();	    	 							//延时函数初始化	  
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);		// 设置中断优先级分组2
	EXTIX_Init();										//外部中断初始化
	uart_init(115200);	 								//串口初始化为115200
	usart3_init(115200);	 							//串口初始化为115200
	LED_Init();											//LED初始化
	OLED_Init();										//OLED初始化
	TIM3_Int_Init(9999,7199);							//10Khz的计数频率，计数到10000为1s  
	OLED_Clear();
	OLED_ShowCHinese(10, 0, 0);							//网
	OLED_ShowCHinese(28, 0, 1); 						//络
	OLED_ShowCHinese(46, 0, 2); 						//天
	OLED_ShowCHinese(64, 0, 3); 						//气
	OLED_ShowCHinese(82, 0, 4); 						//时
	OLED_ShowCHinese(100, 0, 5); 						//钟
 
	OLED_ShowString(10, 4, "wifi", 16);
	OLED_ShowCHinese(46, 4, 12);						//初
	OLED_ShowCHinese(64, 4, 13); 						//始
	OLED_ShowCHinese(82, 4, 14); 						//化
	OLED_ShowCHinese(100, 4, 15); 						//中	

	esp8266_start_trans(); 								//esp8266进行初始化，连接wifi 

	OLED_Clear(); 
	OLED_ShowCHinese(10, 0, 0);							//网
	OLED_ShowCHinese(28, 0, 1); 						//络
	OLED_ShowCHinese(46, 0, 2); 						//天
	OLED_ShowCHinese(64, 0, 3); 						//气
	OLED_ShowCHinese(82, 0, 4); 						//时
	OLED_ShowCHinese(100, 0, 5); 						//钟

	OLED_ShowCHinese(10, 4, 12);						//初
	OLED_ShowCHinese(28, 4, 13); 						//始
	OLED_ShowCHinese(46, 4, 14); 						//化
	OLED_ShowCHinese(64, 4, 16); 						//成
	OLED_ShowCHinese(82, 4, 17); 						//功	
	
	delay_ms(3000);

	OLED_Clear(); 
	OLED_ShowCHinese(10, 0, 0);							//网
	OLED_ShowCHinese(28, 0, 1); 						//络
	OLED_ShowCHinese(46, 0, 2); 						//天
	OLED_ShowCHinese(64, 0, 3); 						//气
	OLED_ShowCHinese(82, 0, 4); 						//时
	OLED_ShowCHinese(100, 0, 5); 						//钟

	OLED_ShowCHinese(10, 4, 18);						//获
	OLED_ShowCHinese(28, 4, 19); 						//取
	OLED_ShowCHinese(46, 4, 20); 						//数
	OLED_ShowCHinese(64, 4, 21); 						//据
	OLED_ShowCHinese(82, 4, 15); 						//中

	get_current_weather(); 								//获取天气
	delay_ms(200);
	get_beijing_time();									//获取时间	

	OLED_Clear(); 
	OLED_ShowCHinese(10, 0, 0);							//网
	OLED_ShowCHinese(28, 0, 1); 						//络
	OLED_ShowCHinese(46, 0, 2); 						//天
	OLED_ShowCHinese(64, 0, 3); 						//气
	OLED_ShowCHinese(82, 0, 4); 						//时
	OLED_ShowCHinese(100, 0, 5); 						//钟

	OLED_ShowCHinese(10, 4, 18);						//获
	OLED_ShowCHinese(28, 4, 19); 						//取
	OLED_ShowCHinese(46, 4, 16); 						//成
	OLED_ShowCHinese(64, 4, 17); 						//功	

	delay_ms(3000);

	OLED_Clear(); 		
	hour = nwt.hour;
	min = nwt.min;
	sec = nwt.sec;

	TIM_Cmd(TIM3, ENABLE);  //使能TIMx外设		

	while (1)
	{

	/*******************************************
	按键中断触发，改变LED状态以及flag的值，
	当按键按下时，flag = 1,开始更新数据。	
	******************************************/
		if (flag == 1)
		{
			get_current_weather(); 						//获取天气
			delay_ms(200);
			get_beijing_time();							//获取时间				
			flag = 0;
			hour = nwt.hour;
			min = nwt.min;
			sec = nwt.sec;
		}
		
	/*******************************************
	时间计数判断，其中秒的自加在中断3实现。	
	******************************************/
		if(sec >= 60)
		{
			sec = 0;
			min ++;
		}

		if(min >= 60)
		{
			min = 0;
			hour ++;
		}

		if(hour >= 24)
		{
			hour = 0;
		}

		OLED_ShowString(0, 0, "I_Color", 16);
		OLED_ShowNum(63,0,hour,2,16);							//显示小时
		OLED_ShowString(79, 0, ":", 16);
		OLED_ShowNum(87,0,min,2,16);							//显示分钟 
		OLED_ShowString(103, 0, ":", 16);
		OLED_ShowNum(111,0,sec,2,16);							//显示秒 		
		
		
		OLED_ShowCHinese(0, 3, 2);								//天
		OLED_ShowCHinese(18, 3, 3); 							//气
		OLED_ShowString(40, 3, ":", 16);
		OLED_ShowString(60, 3, (u8*)results[0].now.text, 16);		//采集到的天气

		OLED_ShowCHinese(0, 6, 9);	 							//温
		OLED_ShowCHinese(18, 6, 10); 							//度
		OLED_ShowString(40, 6, ":", 16);
		OLED_ShowString(60, 6, (u8*)results[0].now.temperature, 16);	//采集到的温度
		OLED_ShowCHinese(80, 6, 11); 							//摄氏度
	}
}
