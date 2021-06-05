#include "esp8266.h"
#include "string.h"
#include "usart.h"
#include "usart3.h"
#include "stm32f10x.h"
#include "sys.h" 
#include "delay.h"
#include "led.h"
#include "stdlib.h"
#include "malloc.h"
#include "cJSON.h"

  

 nt_calendar_obj nwt;  //定义结构体变量


const u8* wifista_ssid="1612";							//路由器SSID号
const u8* wifista_password="123456789"; 				//连接密码

#define WEATHER_PORTNUM 	"80"						//天气连接端口号:80	
#define WEATHER_SERVERIP 	"api.seniverse.com"			//天气服务器IP


#define TIME_PORTNUM			"80"					//时间端口号
#define TIME_SERVERIP			"www.beijing-time.org"	//时间服务器IP


//月份数据表											 
u8 const table_week[12]={0,3,3,6,1,4,6,2,5,0,3,5}; 		//月修正数据表	  
//平年的月份日期表
const u8 mon_table[12]={31,28,31,30,31,30,31,31,30,31,30,31};


u8 *p;
Results results[] = {{0}};

/*********************************************************************************
* Function Name    ： esp8266_start_trans,ESP8266初始化设置
* Parameter		   ： 
* Return Value     ： 
* Function Explain ： 连接wifi
* Create Date      ： 2021/6/5 by zzh
**********************************************************************************/
void esp8266_start_trans(void)
{
	u8 *p;
	p=mymalloc(SRAMIN,50);							//申请32字节内存，用于存wifista_ssid，wifista_password
	printf("send:AT\r\n");	
	while(esp8266_send_cmd("AT","OK",20))			//检查WIFI模块是否在线
	{
	} 
	//设置工作模式 1：station模式   2：AP模式  3：兼容 AP+station模式
	printf("send:AT+CWMODE=1\r\n");	
	esp8266_send_cmd("AT+CWMODE=1","OK",50);
	//Wifi模块重启
	printf("send:AT+RST\r\n");	
	esp8266_send_cmd("AT+RST","OK",20);
	delay_ms(1000);         						//延时3S等待重启成功
	delay_ms(1000);
	delay_ms(1000);	
	
	//设置连接到的WIFI网络名称/加密方式/密码,这几个参数需要根据您自己的路由器设置进行修改!! 
	printf("send:AT+CIPMUX=0\r\n");	
	esp8266_send_cmd("AT+CIPMUX=0","OK",20);   		//0：单连接，1：多连接		

	sprintf((char*)p,"AT+CWJAP=\"%s\",\"%s\"",wifista_ssid,wifista_password);		//设置无线参数:ssid,密码
	printf("send:AT+CWJAP=\"%s\",\"%s\"\r\n",wifista_ssid,wifista_password);
	
	while(esp8266_send_cmd(p,"WIFI GOT IP",300));									//连接目标路由器,并且获得IP
	myfree(SRAMIN,p);
}


/*********************************************************************************
* Function Name    ： esp8266_send_cmd,向ESP8266发送命令
* Parameter		   ： cmd:发送的命令字符串;ack:期待的应答结果,如果为空,则表示不需要等待应答;waittime:等待时间(单位:10ms)
* Return Value     ： 返回值:0,发送成功(得到了期待的应答结果);1,发送失败
* Function Explain ： 
* Create Date      ： 2021/6/5 by zzh
**********************************************************************************/
u8 esp8266_send_cmd(u8 *cmd,u8 *ack,u16 waittime)
{
	u8 res=0; 
	USART3_RX_STA=0;
	u3_printf("%s\r\n",cmd);	//发送命令
	if(ack&&waittime)		//需要等待应答
	{
		while(--waittime)	//等待倒计时
		{
			delay_ms(10);
			if(USART3_RX_STA&0X8000)//接收到期待的应答结果
			{
				if(esp8266_check_cmd(ack))
				{
					printf("receive:%s\r\n",(u8*)ack);
					break;//得到有效数据 
				}
					USART3_RX_STA=0;
			} 
		}
		if(waittime==0)res=1; 
	}
	return res;
} 


/*********************************************************************************
* Function Name    ： esp8266_check_cmd,ESP8266发送命令后,检测接收到的应答
* Parameter		   ： str:期待的应答结果
* Return Value     ： 返回值:0,没有得到期待的应答结果;其他,期待应答结果的位置(str的位置)
* Function Explain ： 
* Create Date      ： 2021/6/5 by zzh
**********************************************************************************/
u8* esp8266_check_cmd(u8 *str)
{
	char *strx=0;
	if(USART3_RX_STA&0X8000)		//接收到一次数据了
	{ 
		USART3_RX_BUF[USART3_RX_STA&0X7FFF]=0;//添加结束符
		strx=strstr((const char*)USART3_RX_BUF,(const char*)str);
	} 
	return (u8*)strx;
}


/*********************************************************************************
* Function Name    ： esp8266_quit_trans,ATK-ESP8266退出透传模式
* Parameter		   ： 
* Return Value     ： 返回值:0,退出成功;1,退出失败
* Function Explain ： 
* Create Date      ： 2021/6/5 by zzh
**********************************************************************************/
u8 esp8266_quit_trans(void)
{
	while((USART3->SR&0X40)==0);	//等待发送空
	USART3->DR='+';      
	delay_ms(15);					//大于串口组帧时间(10ms)
	while((USART3->SR&0X40)==0);	//等待发送空
	USART3->DR='+';      
	delay_ms(15);					//大于串口组帧时间(10ms)
	while((USART3->SR&0X40)==0);	//等待发送空
	USART3->DR='+';      
	delay_ms(500);					//等待500ms
	return esp8266_send_cmd("AT","OK",20);//退出透传判断.
}


/*********************************************************************************
* Function Name    ： get_current_weather,获取一次实时天气
* Parameter		   ： 
* Return Value     ： 返回：0---获取成功，1---获取失败
* Function Explain ： 
* Create Date      ： 2021/6/5 by zzh
**********************************************************************************/
u8 get_current_weather(void)
{
	u8 res;

	p=mymalloc(SRAMIN,40);							//申请40字节内存
	
	
	sprintf((char*)p,"AT+CIPSTART=\"TCP\",\"%s\",%s",WEATHER_SERVERIP,WEATHER_PORTNUM);    //配置目标TCP服务器
	printf("send:AT+CIPSTART=\"TCP\",\"%s\",%s\r\n",WEATHER_SERVERIP,WEATHER_PORTNUM);
	res = esp8266_send_cmd(p,"OK",200);//连接到目标TCP服务器
	if(res==1)
	{
		myfree(SRAMIN,p);
		
		return 1;
	}
	delay_ms(300);
	
	printf("send:AT+CIPMODE=1\r\n");	
	esp8266_send_cmd("AT+CIPMODE=1","OK",100);      //传输模式为：透传	

	USART3_RX_STA=0;
	printf("send:AT+CIPSEND\r\n");	
	esp8266_send_cmd("AT+CIPSEND","OK",100);         //开始透传
	printf("start trans...\r\n");
	
	u3_printf("GET https://api.seniverse.com/v3/weather/now.json?key=St4bOhFvShcUJhU4_&location=shenzhen&language=en&unit=c\n\n");	
	delay_ms(20);//延时20ms返回的是指令发送成功的状态
	USART3_RX_STA=0;	//清零串口3数据
	delay_ms(1000);
	if(USART3_RX_STA&0X8000)		//此时再次接到一次数据，为天气的数据
	{ 
		USART3_RX_BUF[USART3_RX_STA&0X7FFF]=0;//添加结束符
	} 
	printf("USART3_RX_BUF=%s\r\n",USART3_RX_BUF);
	
	cJSON_WeatherParse(USART3_RX_BUF, results);		//解析天气数据
	//打印结构体内内容
	printf("打印结构体内内容如下： \r\n");
	printf("%s \r\n",results[0].now.text);
	printf("%s \r\n",results[0].now.temperature);
	
	esp8266_quit_trans();//退出透传
	
	printf("send:AT+CIPCLOSE\r\n");	
	esp8266_send_cmd("AT+CIPCLOSE","OK",50);         //关闭连接
	myfree(SRAMIN,p);
	return 0;
}



/*********************************************************************************
* Function Name    ： cJSON_WeatherParse,解析天气数据
* Parameter		   ： JSON：天气数据包  results：保存解析后得到的有用的数据
* Return Value     ： 0：成功 其他:错误
* Function Explain ： 
* Create Date      ： 2021/6/5 by zzh
**********************************************************************************/
int cJSON_WeatherParse(char *JSON, Results *results)
{
	cJSON *json,*arrayItem,*object,*subobject,*item;
	
	json = cJSON_Parse(JSON); //解析JSON数据包
	if(json == NULL)		  //检测JSON数据包是否存在语法上的错误，返回NULL表示数据包无效
	{
		printf("Error before: [%s] \r\n",cJSON_GetErrorPtr()); //打印数据包语法错误的位置
		return 1;
	}
	else
	{
		if((arrayItem = cJSON_GetObjectItem(json,"results")) != NULL); //匹配字符串"results",获取数组内容
		{
			int size = cJSON_GetArraySize(arrayItem);     //获取数组中对象个数
			printf("cJSON_GetArraySize: size=%d \r\n",size); 
			
			if((object = cJSON_GetArrayItem(arrayItem,0)) != NULL)//获取父对象内容
			{
				/* 匹配子对象1 */
				if((subobject = cJSON_GetObjectItem(object,"location")) != NULL)
				{
					printf("---------------------------------subobject1-------------------------------\r\n");
					if((item = cJSON_GetObjectItem(subobject,"id")) != NULL)   //匹配子对象1成员"id"
					{
						printf("cJSON_GetObjectItem: type=%d, string is %s,valuestring=%s \r\n",item->type,item->string,item->valuestring);
						memcpy(results[0].location.id,item->valuestring,strlen(item->valuestring));
					}
					if((item = cJSON_GetObjectItem(subobject,"name")) != NULL) //匹配子对象1成员"name"
					{
						printf("cJSON_GetObjectItem: type=%d, string is %s,valuestring=%s \r\n",item->type,item->string,item->valuestring);
						memcpy(results[0].location.name,item->valuestring,strlen(item->valuestring));
					}
					if((item = cJSON_GetObjectItem(subobject,"country")) != NULL)//匹配子对象1成员"country"
					{
						printf("cJSON_GetObjectItem: type=%d, string is %s,valuestring=%s \r\n",item->type,item->string,item->valuestring);
						memcpy(results[0].location.country,item->valuestring,strlen(item->valuestring));
					}
					if((item = cJSON_GetObjectItem(subobject,"path")) != NULL)  //匹配子对象1成员"path"
					{
						printf("cJSON_GetObjectItem: type=%d, string is %s,valuestring=%s \r\n",item->type,item->string,item->valuestring);
						memcpy(results[0].location.path,item->valuestring,strlen(item->valuestring));
					}
					if((item = cJSON_GetObjectItem(subobject,"timezone")) != NULL)//匹配子对象1成员"timezone"
					{
						printf("cJSON_GetObjectItem: type=%d, string is %s,valuestring=%s \r\n",item->type,item->string,item->valuestring);
						memcpy(results[0].location.timezone,item->valuestring,strlen(item->valuestring));
					}
					if((item = cJSON_GetObjectItem(subobject,"timezone_offset")) != NULL)//匹配子对象1成员"timezone_offset"
					{
						printf("cJSON_GetObjectItem: type=%d, string is %s,valuestring=%s \r\n",item->type,item->string,item->valuestring);
						memcpy(results[0].location.timezone_offset,item->valuestring,strlen(item->valuestring));
					}
				}
				/* 匹配子对象2 */
				if((subobject = cJSON_GetObjectItem(object,"now")) != NULL)
				{
					printf("---------------------------------subobject2-------------------------------\r\n");
					if((item = cJSON_GetObjectItem(subobject,"text")) != NULL)//匹配子对象2成员"text"
					{
						printf("cJSON_GetObjectItem: type=%d, string is %s,valuestring=%s \r\n",item->type,item->string,item->valuestring);
						memcpy(results[0].now.text,item->valuestring,strlen(item->valuestring));
					}
					if((item = cJSON_GetObjectItem(subobject,"code")) != NULL)//匹配子对象2成员"code"
					{
						printf("cJSON_GetObjectItem: type=%d, string is %s,valuestring=%s \r\n",item->type,item->string,item->valuestring);
						memcpy(results[0].now.code,item->valuestring,strlen(item->valuestring));
					}
					if((item = cJSON_GetObjectItem(subobject,"temperature")) != NULL) //匹配子对象2成员"temperature"
					{
						printf("cJSON_GetObjectItem: type=%d, string is %s,valuestring=%s \r\n",item->type,item->string,item->valuestring);
						memcpy(results[0].now.temperature,item->valuestring,strlen(item->valuestring));
					}
				}
				/* 匹配子对象3 */
				if((subobject = cJSON_GetObjectItem(object,"last_update")) != NULL)
				{
					printf("---------------------------------subobject3-------------------------------\r\n");
					printf("cJSON_GetObjectItem: type=%d, string is %s,valuestring=%s \r\n",subobject->type,subobject->string,subobject->valuestring);
					memcpy(results[0].last_update,item->valuestring,strlen(subobject->valuestring));
				}
			} 
		}
	}
	
	cJSON_Delete(json); //释放cJSON_Parse()分配出来的内存空间
	
	return 0;
}


/*********************************************************************************
* Function Name    ： get_beijing_time,获取时间
* Parameter		   ： 
* Return Value     ： 返回：0---获取成功，1---获取失败
* Function Explain ： 
* Create Date      ： 2021/6/5 by zzh
**********************************************************************************/
u8 get_beijing_time(void)
{
	u8 *p;
	u8 res;
	
	u8 *resp;
	u8 *p_end;
	u8 ipbuf[16]; 	//IP缓存
	p=mymalloc(SRAMIN,40);							//申请40字节内存
	resp=mymalloc(SRAMIN,10);
	p_end=mymalloc(SRAMIN,40);
	
	sprintf((char*)p,"AT+CIPSTART=\"TCP\",\"%s\",%s",TIME_SERVERIP,TIME_PORTNUM);    //配置目标TCP服务器
	printf("%s",p);
	res = esp8266_send_cmd(p,"OK",200);//连接到目标TCP服务器
	if(res==1)
	{
		myfree(SRAMIN,p);
		return 1;
	}
	delay_ms(300);
	esp8266_send_cmd("AT+CIPMODE=1","OK",100);      //传输模式为：透传	
//	atk_8266_get_wanip(ipbuf);//获取WAN IP
////	
//	sprintf((char*)p,"IP地址:%s 端口:%s",ipbuf,(u8*)TIME_PORTNUM);
//	
//	//Show_Str(30,65,200,12,p,12,0);				//显示IP地址和端口	
//	 printf("设备 %s\r\n",p);
//	
//	USART3_RX_STA=0;
	esp8266_send_cmd("AT+CIPSEND","OK",100);         //开始透传
	printf("start trans...\r\n");

	u3_printf("GET /time15.asp HTTP/1.1Host:www.beijing-time.org\n\n");

	delay_ms(20);//延时20ms返回的是指令发送成功的状态
	USART3_RX_STA=0;	//清零串口3数据
	delay_ms(1000);
	if(USART3_RX_STA&0X8000)		//此时再次接到一次数据，为天气的数据
	{ 
		USART3_RX_BUF[USART3_RX_STA&0X7FFF]=0;//添加结束符
	} 
	
	printf("USART3_RX_BUF=%s\r\n",USART3_RX_BUF);	
	//USART3_RX_BUF 为时间信息
	if(USART3_RX_STA & 0x8000)
	{
			resp="Date";
			USART3_RX_BUF[USART3_RX_STA & 0x7ff] = 0;
			printf("get_tim_srt：%s\r\n",USART3_RX_BUF);
			if(strstr((char*)USART3_RX_BUF,(char*)resp)) 
			{       
				resp="GMT";
				p_end = (u8*)strstr((char*)USART3_RX_BUF,(char*)resp);
				p = p_end - 9; 
				printf("get_net_str %s\r\n",p);
				nwt.hour = ((*p - 0x30)*10 + (*(p+1) - 0x30) + 8) % 24;  //GMT0-->GMT8

				nwt.min = ((*(p+3) - 0x30)*10 + (*(p+4) - 0x30)) % 60;

				nwt.sec = ((*(p+6) - 0x30)*10 + (*(p+7) - 0x30)) % 60;

				nwt.year = ((*(p-5) - 0x30)*1000 + (*(p-4) - 0x30)*100+ (*(p-3) - 0x30)*10+ (*(p-2) - 0x30)); 

				nwt.date = ((*(p-12) - 0x30)*10 + (*(p-11) - 0x30)); 

				if        ((u8*)strstr((char*)USART3_RX_BUF,(char*) "Jan")) nwt.month=1; 
				else if   ((u8*)strstr((char*)USART3_RX_BUF,(char*) "Feb")) nwt.month=2; 
				else if   ((u8*)strstr((char*)USART3_RX_BUF,(char*) "Mar")) nwt.month=3; 
				else if   ((u8*)strstr((char*)USART3_RX_BUF,(char*) "Apr")) nwt.month=4; 
				else if   ((u8*)strstr((char*)USART3_RX_BUF,(char*) "May")) nwt.month=5; 
				else if   ((u8*)strstr((char*)USART3_RX_BUF,(char*) "Jun")) nwt.month=6; 
				else if   ((u8*)strstr((char*)USART3_RX_BUF,(char*) "Jul")) nwt.month=7; 
				else if   ((u8*)strstr((char*)USART3_RX_BUF,(char*) "Aug")) nwt.month=8; 
				else if   ((u8*)strstr((char*)USART3_RX_BUF,(char*) "Sep")) nwt.month=9; 
				else if   ((u8*)strstr((char*)USART3_RX_BUF,(char*) "Oct")) nwt.month=10; 
				else if   ((u8*)strstr((char*)USART3_RX_BUF,(char*) "Nov")) nwt.month=11; 
				else if   ((u8*)strstr((char*)USART3_RX_BUF,(char*) "Dec")) nwt.month=12;


				printf("nwt.year = %d\r\n",nwt.year);
				printf("nwt.month = %d\r\n",nwt.month);
				printf("nwt.date = %d\r\n",nwt.date);  //获取data 28日

				printf("nwt.hour = %d\r\n",nwt.hour);
				printf("nwt.min = %d\r\n",nwt.min);
				printf("nwt.sec = %d\r\n",nwt.sec);



				USART3_RX_STA = 0;


				printf("uddate:nettime!!!");
//				RTC_Set(nwt.year,nwt.month ,nwt.date ,nwt.hour ,nwt.min,nwt.sec);
		}
		USART3_RX_STA = 0;																
		myfree(SRAMIN,resp);
		myfree(SRAMIN,p_end);
																
						
    }               
	printf("\r\n\r\n");
	esp8266_quit_trans();//退出透传
	esp8266_send_cmd("AT+CIPCLOSE","OK",50);         //关闭连接
	myfree(SRAMIN,p);
	return 0;
}


/*********************************************************************************
* Function Name    ： atk_8266_get_wanip,获取Client ip地址，本例程未使用
* Parameter		   ： ipbuf:ip地址输出缓存区
* Return Value     ：
* Function Explain ： 
* Create Date      ： 2021/6/5 by zzh
**********************************************************************************/
void atk_8266_get_wanip(u8* ipbuf)
{
	u8 *p,*p1;
		if(esp8266_send_cmd("AT+CIFSR","OK",50))//获取WAN IP地址失败
		{
			ipbuf[0]=0;
			return;
		}		
		p=atk_8266_check_cmd("\"");
		p1=(u8*)strstr((const char*)(p+1),"\"");
		*p1=0;
		sprintf((char*)ipbuf,"%s",p+1);	
}


/*********************************************************************************
* Function Name    ： RTC_Set,RTC设置，本例程未使用
* Parameter		   ： syear,smon,sday,hour,min,sec
* Return Value     ：
* Function Explain ： 
* Create Date      ： 2021/6/5 by zzh
**********************************************************************************/
u8 RTC_Set(u16 syear,u8 smon,u8 sday,u8 hour,u8 min,u8 sec)
{
	u16 t;
	u32 seccount=0;
	if(syear<1970||syear>2099)return 1;	   
	for(t=1970;t<syear;t++)	//把所有年份的秒钟相加
	{
		if(Is_Leap_Year(t))seccount+=31622400;//闰年的秒钟数
		else seccount+=31536000;			  //平年的秒钟数
	}
	smon-=1;
	for(t=0;t<smon;t++)	   //把前面月份的秒钟数相加
	{
		seccount+=(u32)mon_table[t]*86400;//月份秒钟数相加
		if(Is_Leap_Year(syear)&&t==1)seccount+=86400;//闰年2月份增加一天的秒钟数	   
	}
	seccount+=(u32)(sday-1)*86400;//把前面日期的秒钟数相加 
	seccount+=(u32)hour*3600;//小时秒钟数
    seccount+=(u32)min*60;	 //分钟秒钟数
	seccount+=sec;//最后的秒钟加上去

//	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);	//使能PWR和BKP外设时钟  
//	PWR_BackupAccessCmd(ENABLE);	//使能RTC和后备寄存器访问 
//	RTC_SetCounter(seccount);	//设置RTC计数器的值

//	RTC_WaitForLastTask();	//等待最近一次对RTC寄存器的写操作完成  	
	return 0;	    
}


/*********************************************************************************
* Function Name    ： atk_8266_check_cmd,ATK-ESP8266发送命令后,检测接收到的应答
* Parameter		   ： str:期待的应答结果
* Return Value     ： 返回值:0,没有得到期待的应答结果，其他,期待应答结果的位置(str的位置)
* Function Explain ： 
* Create Date      ： 2021/6/5 by zzh
**********************************************************************************/
u8* atk_8266_check_cmd(u8 *str)
{
	
	char *strx=0;
	if(USART3_RX_STA&0X8000)		//接收到一次数据了
	{ 
		USART3_RX_BUF[USART3_RX_STA&0X7FFF]=0;//添加结束符
		strx=strstr((const char*)USART3_RX_BUF,(const char*)str);
	} 
	return (u8*)strx;
}


/*********************************************************************************
* Function Name    ： atk_8266_send_cmd,向ATK-ESP8266发送命令
* Parameter		   ： cmd:发送的命令字符串，ack:期待的应答结果,如果为空,则表示不需要等待应答，waittime:等待时间(单位:10ms)
* Return Value     ： 返回值:0,发送成功(得到了期待的应答结果)，1,发送失败
* Function Explain ： 
* Create Date      ： 2021/6/5 by zzh
**********************************************************************************/
u8 atk_8266_send_cmd(u8 *cmd,u8 *ack,u16 waittime)
{
	u8 res=0; 
	USART3_RX_STA=0;
	u3_printf("%s\r\n",cmd);	//发送命令
	if(ack&&waittime)		//需要等待应答
	{
		while(--waittime)	//等待倒计时
		{
			delay_ms(10);
			if(USART3_RX_STA&0X8000)//接收到期待的应答结果
			{
				if(atk_8266_check_cmd(ack))
				{
					printf("%s ack:%s\r\n",cmd,(u8*)ack);
					break;//得到有效数据 
				}
					USART3_RX_STA=0;
			} 
		}
		if(waittime==0)res=1; 
	}
	return res;
} 


/*********************************************************************************
* Function Name    ： Is_Leap_Year,判断是否闰年
* Parameter		   ： year年份
* Return Value     ： 1，闰年，0，不是闰年
* Function Explain ： 
* Create Date      ： 2021/6/5 by zzh
**********************************************************************************/
u8 Is_Leap_Year(u16 year)
{			  
	if(year%4==0) //必须能被4整除
	{ 
		if(year%100==0) 
		{ 
			if(year%400==0)return 1;//如果以00结尾,还要能被400整除 	   
			else return 0;   
		}else return 1;   
	}else return 0;	
}	
