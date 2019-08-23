#include "lora_ui.h"
#include "usart.h"		
#include "delay.h"	
#include "led.h"   	 
#include "key.h"	 	 	 	 	 
#include "lcd.h" 	  
#include "flash.h" 	 
#include "touch.h" 	 
#include "malloc.h"
#include "string.h"    
#include "text.h"		
#include "usart2.h" 
#include "ff.h"
#include "lora_cfg.h"
#include "lora_app.h"

//********************************************************************************	 
//ATK-LORA-01模块UI和底层处理驱动
//********************************************************************************


//usmart支持部分 
//将收到的AT指令应答数据返回给电脑串口
//mode:0,不清零USART2_RX_STA;
//     1,清零USART2_RX_STA;
void lora_at_response(u8 mode)
{
	if(USART2_RX_STA&0X8000)		//接收到一次数据了
	{ 
		USART2_RX_BUF[USART2_RX_STA&0X7FFF]=0;//添加结束符
		printf("%s",USART2_RX_BUF);	//发送到串口
		if(mode)USART2_RX_STA=0;
	} 
}


//lora发送命令后,检测接收到的应答
//str:期待的应答结果
//返回值:0,没有得到期待的应答结果
//其他,期待应答结果的位置(str的位置)
u8* lora_check_cmd(u8 *str)
{
	char *strx=0;
	if(USART2_RX_STA&0X8000)		//接收到一次数据了
	{ 
		USART2_RX_BUF[USART2_RX_STA&0X7FFF]=0;//添加结束符
		strx=strstr((const char*)USART2_RX_BUF,(const char*)str);
	} 
	return (u8*)strx;
}

//lora发送命令
//cmd:发送的命令字符串(不需要添加回车了),当cmd<0XFF的时候,发送数字(比如发送0X1A),大于的时候发送字符串.
//ack:期待的应答结果,如果为空,则表示不需要等待应答
//waittime:等待时间(单位:10ms)
//返回值:0,发送成功(得到了期待的应答结果)
//       1,发送失败
u8 lora_send_cmd(u8 *cmd,u8 *ack,u16 waittime)
{
	u8 res=0; 
	USART2_RX_STA=0;
	if((u32)cmd<=0XFF)
	{
		while(DMA1_Channel7->CNDTR!=0);	//等待通道7传输完成   
		USART2->DR=(u32)cmd;
	}else u2_printf("%s\r\n",cmd);//发送命令

	if(ack&&waittime)		//需要等待应答
	{
	   while(--waittime)	//等待倒计时
	   { 
		  delay_ms(10);
		  if(USART2_RX_STA&0X8000)//接收到期待的应答结果
		  {
			  if(lora_check_cmd(ack))
			  {
				 USART2_RX_STA=0;
				 break;//得到有效数据 
			  }
		  } 
	   }
	   if(waittime==0)res=1; 
	}
	return res;
} 


//将1个字符转换为16进制数字
//chr:字符,0~9/A~F/a~F
//返回值:chr对应的16进制数值
u8 chr2hex(u8 chr)
{
	if(chr>='0'&&chr<='9')return chr-'0';
	if(chr>='A'&&chr<='F')return (chr-'A'+10);
	if(chr>='a'&&chr<='f')return (chr-'a'+10); 
	return 0;
}


//字符转成16进制数据
//test[] 存储位置
//buffer[] 需转换的数组
//len： 长度
void char2hex_buff(u8 test[],u8 buff[],u8 len)
{
	u8 i=0;
	for(i=0;i<len;i++)
	{
 		test[i] = chr2hex(buff[i]);
	}
}


//将16进制数据转换成数字
u32 hexbuff_num(u8 test[],u8 buff[],u8 len)
{
	u8 i=0;
	u32 num=0; 
	
	for(i=0;i<len;i++) test[i] = chr2hex(buff[i]);
    if(len==5)  num = test[0]*10000+test[1]*1000+test[2]*100+test[3]*10+test[4];
	else if(len==4) num = test[0]*1000+test[1]*100+test[2]*10+test[3];		
	else if(len==3) num = test[0]*100+test[1]*10+test[2];		
	else if(len==2) num = test[0]*10+test[1];
	else if(len==1) num = test[0];
	
	return num;
	
}


//计算数字位数
//如:100,3位数、10,1位数
u8 num_count(u32 num)
{
	u32 date=0;
	u8 count=0;
	
	date = num;
	do
	{
		count++;
		date/=10;
	}while(date>0);
	
	return count;
}


//键盘码表
const u8* kbd_tbl1[11]={"1","2","3","4","5","6","7","8","9","0","DEL"};
u8** kbd_tbl;
u8* kbd_fn_tbl[2]={"OK","Esc"};

//加载键盘界面（尺寸为240*140）
//x,y:界面起始坐标（320*240分辨率的时候，x必须为0）
void lora_load_keyboard(u16 x,u16 y,u8 **kbtbl)
{
	u16 i;
	POINT_COLOR=RED;
	kbd_tbl=kbtbl;
	LCD_Fill(x,y,x+240,y+140,WHITE);
	LCD_DrawRectangle(x,y,x+240,y+140);						   	 
	LCD_DrawRectangle(x+80,y,x+160,y+112);	
	LCD_DrawRectangle(x,y+28,x+240,y+56);
	LCD_DrawRectangle(x,y+84,x+240,y+112);
	POINT_COLOR=BLUE;
	for(i=0;i<13;i++)
	{
		if(i<11)Show_Str_Mid(x+(i%3)*80,y+6+28*(i/3),(u8*)kbd_tbl[i],16,80);
		else if(i==11)
			Show_Str_Mid(x+(i%3)*80,y+6+28*(i/3),kbd_fn_tbl[i-11],16,80); 
		else
			Show_Str_Mid(x+(i%3)*80+80,y+6+28*(i/3),kbd_fn_tbl[i-11],16,80); 
	}  		 					   
}


//按键状态设置
//x,y:键盘坐标
//key:键值（0~15）
//sta:状态，0，松开；1，按下；
void lora_key_staset(u16 x,u16 y,u8 keyx,u8 sta)
{		  
	u16 i=keyx/3,j=keyx%3;
	if(keyx>15)return;
	if(sta)
	{
		if(keyx>11) 
		   LCD_Fill(2,y+i*28+1,238,318,GREEN);
		else LCD_Fill(x+j*80+1,y+i*28+1,x+j*80+78,y+i*28+26,GREEN);
	}
	else
	{
		if(keyx>11)
			 LCD_Fill(2,y+i*28+1,238,318,WHITE);
		else LCD_Fill(x+j*80+1,y+i*28+1,x+j*80+78,y+i*28+26,WHITE);
	}	 
    if((i==3)&&(j==2))
		Show_Str_Mid(x+j*80,y+6+28*i,(u8*)kbd_fn_tbl[0],16,80);
	else if((i==4)&&(j<3)) 
	    Show_Str_Mid(80,180+6+28*i,kbd_fn_tbl[1],16,80); 
    else 
        Show_Str_Mid(x+j*80,y+6+28*i,(u8*)kbd_tbl[keyx],16,80);			
	
}


//得到触摸屏的输入
//x,y:键盘坐标
//返回值：按键键值（1~15有效；0,无效）
static u8 key_x=0;//0,没有任何按键按下；1~15，1~15号按键按下
u8 lora_get_keynum(u16 x,u16 y)
{
	u16 i,j;
	u8 key=0;
	tp_dev.scan(0); 		 
	if(tp_dev.sta&TP_PRES_DOWN)	//触摸屏被按下
	{	
		for(i=0;i<5;i++)
		{
			for(j=0;j<3;j++)
			{
			 	if(tp_dev.x[0]<(x+j*80+80)&&tp_dev.x[0]>(x+j*80)&&tp_dev.y[0]<(y+i*28+28)&&tp_dev.y[0]>(y+i*28))
				{	
					key=i*3+j+1;	 
					break;	 		   
				}
			}
			if(key)
			{	   
				if(key_x==key)key=0;
				else 
				{
					lora_key_staset(x,y,key_x-1,0);
					key_x=key;
					lora_key_staset(x,y,key_x-1,1);
				}
				break;
			}
		}  
	}else if(key_x) 
	{
		lora_key_staset(x,y,key_x-1,0);
		key_x=0;
	} 
	return key; 
}

u8* cnttbl_mode[3]={"general mode","wakeup mode","power mode"};
u8* cnttbl_sta[2]={"Tran","Dire"};
u8* cnttbl_power[4]={"11dBm","14dBm","17dBm","20dBm"};
u8* cnttbl_rate[6]={"0.3kbps","1.2kbps","2.4kbps","4.8kbps","9.6kbps","19.2kbps"};

//主菜单UI
void Menu_ui(void)
{
	u8 num=0;
	
	LCD_Clear(WHITE);
	POINT_COLOR=RED;
	Show_Str_Mid(0,30,"ATK-LORA-01 _^0^_",24,240); 
	
	POINT_COLOR=BLUE;
//	Show_Str(40+30,50+20,200,16,"KEY0:next",16,0); 				    	 
//	Show_Str(40+30,70+20,200,16,"KEY1:last",16,0);				    	 
//	Show_Str(40+30,90+20,200,16,"WK_UP:Y/N",16,0); 
  Show_Str(40+30,60+20,200,16,"Communications",16,0);				    	 
  Show_Str(40+30,80+20,200,16," begin in  3s",16,0);
	
	POINT_COLOR=RED;
//	Show_Str(40+30,45+95,200,16,"Communicate",16,0);
	Show_Str(50+30,40+90,200,24,"LORA_",24,0);
	POINT_COLOR=BLUE;
	Show_Str(50+30+12*5,40+90,200,24,"0",24,0);
	POINT_COLOR=RED;
	Show_Str(20+30,70+95,200,16,"Address：",16,0);
	Show_Str(20+30,95+95,200,16,"Channel：",16,0);
	Show_Str(20+30,120+95,200,16,"Rate：",16,0);
	Show_Str(20+30,145+95,200,16,"Power：",16,0);
	Show_Str(20+30,170+95,200,16,"Mode：",16,0);
	Show_Str(20+30,195+95,200,16,"State：",16,0);
	
	POINT_COLOR=BLUE;
//	Show_Str(30+10,45+95,200,16,"->",16,0); 
	num = num_count(LoRa_CFG.addr);
	LCD_ShowNum(20+110+10,70+95,LoRa_CFG.addr,num,16);
	num = num_count(LoRa_CFG.chn);
	LCD_ShowxNum(20+110+10,95+95,LoRa_CFG.chn,num,16,0);
	
	Show_Str(20+110+10,120+95,200,16,(u8*)cnttbl_rate[LoRa_CFG.wlrate],16,0);
	Show_Str(20+110+10,145+95,200,16,(u8*)cnttbl_power[LoRa_CFG.power],16,0);
	Show_Str(20+110+10,170+95,200,16,(u8*)cnttbl_mode[LoRa_CFG.mode],16,0);
	Show_Str(20+110+10,195+95,200,16,(u8*)cnttbl_sta[LoRa_CFG.mode_sta],16,0);
	
	delay_ms(1000);
	Show_Str(40+30,80+20,200,16," begin in  2s",16,0);
	delay_ms(1000);
	Show_Str(40+30,80+20,200,16," begin in  1s",16,0);
	delay_ms(1000);
}


//主界面参数配置
void Menu_cfg(u8 num)
{
    u8 key=0;
	u8 t=0;
	u8 count=0;
	
	POINT_COLOR=BLUE;
    while(1)
    {
	  key = KEY_Scan(0);
	  
	  if(key==KEY1_PRES)//按键+ 
	  {
		  switch(num)
		  {
			  case 1: LoRa_CFG.addr++; break;//地址
	          case 2: LoRa_CFG.chn++;//信道
			          if(LoRa_CFG.chn>31) LoRa_CFG.chn=0;
          			  break;
			  case 3: LoRa_CFG.wlrate++;//无线速率
			          if(LoRa_CFG.wlrate>5) LoRa_CFG.wlrate=0;
			          break;
			  case 4: LoRa_CFG.power++;//发射功率
			          if(LoRa_CFG.power>3)  LoRa_CFG.power=0;
			          break;	   
			  case 5: LoRa_CFG.mode++;//工作模式
			          if(LoRa_CFG.mode>2) LoRa_CFG.mode=0;
         			  break;
              case 6: LoRa_CFG.mode_sta++;//工作状态
			          if(LoRa_CFG.mode_sta>1) LoRa_CFG.mode_sta=0;
			          break;		   
		  }
	  }else if(key==KEY0_PRES)//按键-
	  {
		  switch(num)
		  {
			  case 1: LoRa_CFG.addr--; break;//地址
	          case 2: LoRa_CFG.chn--;//信道
			          if(LoRa_CFG.chn==255) LoRa_CFG.chn=31;
			          break;
			  case 3: LoRa_CFG.wlrate--;//无线速率 
			         if(LoRa_CFG.wlrate==255) LoRa_CFG.wlrate=5;
			          break;
			  case 4: LoRa_CFG.power--;//发射功率
			          if(LoRa_CFG.power==255)  LoRa_CFG.power=3;
        			  break;	   
			  case 5: LoRa_CFG.mode--;//工作模式
                      if(LoRa_CFG.mode==255) LoRa_CFG.mode=2; 			  
			          break;
              case 6: LoRa_CFG.mode_sta--;//工作状态
			          if(LoRa_CFG.mode_sta>1) LoRa_CFG.mode_sta=1;
        			  break;			   
		  }
	  }else if(key==WKUP_PRES)  break;//退出

	  
	  //更新显示
	  if(key==KEY1_PRES||key==KEY0_PRES)
	  {
		  switch(num)
		  {
			  case 1: 
					count = num_count(LoRa_CFG.addr);
					Show_Str(40+110+10,70+95,200,16,"                ",16,0);
					LCD_ShowNum(40+110+10,70+95,LoRa_CFG.addr,count,16);
					break;
			 case 2: 
					count = num_count(LoRa_CFG.chn);
			        Show_Str(40+110+10,95+95,200,16,"                ",16,0);
			        LCD_ShowxNum(40+110+10,95+95,LoRa_CFG.chn,count,16,0);
					break;
			 case 3:
				    Show_Str(40+110+10,120+95,200,16,"             ",16,0);
				    Show_Str(40+110+10,120+95,200,16,(u8*)cnttbl_rate[LoRa_CFG.wlrate],16,0);
					break;
			 case 4:
				    Show_Str(40+110+10,145+95,200,16,(u8*)cnttbl_power[LoRa_CFG.power],16,0);
					break;
			 case 5: 
				    Show_Str(40+110+10,170+95,200,16,(u8*)cnttbl_mode[LoRa_CFG.mode],16,0);
				    break;
			 case 6:
				    Show_Str(40+110+10,195+95,200,16,(u8*)cnttbl_sta[LoRa_CFG.mode_sta],16,0);
				    break;       
		  }
	  }
	  t++;
	  if(t==30)
	  {
		  t=0;
		  LED1=~LED1;
	  }
	  delay_ms(10);   
  }
		
}



u32 obj_addr=0;//记录目标地址（用户通过键盘数据输入时最大出现99999,所以需要定义u32类型）
u8 obj_chn=0;//记录目标信道
const u8* set_mode[2]={"Set_target_address:","Set_target_channel"};
u8 callbuf[5]={0};//存储用户触摸输入值

//设置目标地址和目标信道(定向传输)
void Dire_Set(void)
{
	u8 t=0;
	u8 chn=0;
	u32 addr=0;
	u8 key=0;
	u8 test[5]={0};
	u8 addrnumlen; //地址长度,最长5个数(65535) 
	u8 chnnumlen;  //信道长度,最长2个数(31)
	u8 mode=1;//0:主菜单页面 1:设置目标地址 2:设置目标信道
	
	LCD_Clear(WHITE);
 RESTART:
	addrnumlen=0;
	chnnumlen=0;
    POINT_COLOR=BLACK;	
	Show_Str(40,70,200,16,(u8*)set_mode[0],16,0);
    Show_Str(40+106,70,200,16,"          ",16,0);  	
	lora_load_keyboard(0,180,(u8**)kbd_tbl1);
	while(1)
	{
		key = lora_get_keynum(0,180);//获取触摸按键值
		
		if(key)
		{
			if(key<11)
			{
				if(mode==1)//设置目标地址下
				{   
					if(addrnumlen<5)
					{
						if(addrnumlen==1&&callbuf[0]=='0') callbuf[0]=kbd_tbl[key-1][0];
						else callbuf[addrnumlen++]=kbd_tbl[key-1][0];
					}
				}else if(mode==2)//设置目标信道
				{
					if(chnnumlen<2)
					{
						if(chnnumlen==1&&callbuf[0]=='0') callbuf[0]=kbd_tbl[key-1][0];
						else callbuf[chnnumlen++]=kbd_tbl[key-1][0];
					}
				}
			}
			else
			{
				if(key==11)//删除按键 
				{
					if(mode==1)//目标地址设置
					{
						if(addrnumlen) addrnumlen--;
					}else if(mode==2)//目标信道设置
					{
						if(chnnumlen) chnnumlen--;
					}
				}
				else if(key==12)//确定按键 
				{
					if(mode==1)//目标地址设置
					{
						if(addrnumlen)
						{
							addr = hexbuff_num(test,callbuf,addrnumlen);//字符转数字
							if(addr>65535)//设定地址越界
							{
								POINT_COLOR=RED;
								Show_Str(40,100,200,16,"Address over range. Reset！",16,0); 
								delay_ms(500);
								delay_ms(500);
								Show_Str(40,100,200,16,"                        ",16,0); 
								addrnumlen=0;
								POINT_COLOR=BLUE;
							}else //数据正常
							{
								obj_addr = addr;
								LCD_Fill(40+106,70,239,70+16,WHITE);
								mode=2;//进入目标信道设置界面
								POINT_COLOR=BLACK;
								Show_Str(40,70,200,16,(u8*)set_mode[1],16,0); 
								POINT_COLOR=BLUE;
							}
						}else //目标地址没设定
						{
							    POINT_COLOR=RED;
							    Show_Str(40,100,200,16,"Please set the address！",16,0); 
								delay_ms(500);
								delay_ms(500);
								Show_Str(40,100,200,16,"                        ",16,0); 
							    POINT_COLOR=BLUE;
						}
						
					}else if(mode==2)//目标信道设置
					{
						if(chnnumlen)
						{
							chn = hexbuff_num(test,callbuf,chnnumlen);//字符转数字
							if(chn>31)
							{
								POINT_COLOR=RED;
								Show_Str(40,100,200,16,"Channel over range. Reset！",16,0); 
								delay_ms(500);
								delay_ms(200);
								Show_Str(40,100,200,16,"                        ",16,0); 
								chnnumlen=0;
								POINT_COLOR=BLUE;
							}else
							{
								//目标信道设置成功,则返回测试界面
								obj_chn = chn;//保存设置的目标信道
								break;
							}
						}else//目标信道没设定
						{
							    POINT_COLOR=RED;
							    Show_Str(40,100,200,16,"Please set channel！",16,0); 
								delay_ms(500);
								delay_ms(200);
								Show_Str(40,100,200,16,"                        ",16,0); 
							    POINT_COLOR=BLUE;
						}
					}
				}else //返回按键
				{
					mode--;//返回上级
					if(mode==1) goto RESTART;//返回目标地址设置页面
					if(mode==0) break; //返回主菜单
				}
				
			 }
			    //数据更新显示
	            if(mode==1) callbuf[addrnumlen]=0; 
			    else if(mode==2) callbuf[chnnumlen]=0; 
				LCD_Fill(40+106,70,239,70+16,WHITE);
				Show_Str(40+106,70,200,16,callbuf,16,0);  
		}
		t++;
		if(t==30)
		{
			t=0;
			LED1=~LED1;
		}
		delay_ms(10);
	}
	
		
}


//测试界面UI
void Process_ui(void)
{
	u8 num=0;
	
	LCD_Clear(WHITE);
	
	POINT_COLOR=BLACK;
	Show_Str_Mid(0,30,"East_West",24,240);
//	Show_Str_Mid(0,30,"South_North",24,240);
	Show_Str(30,70,200,12,"Address:",12,0);
	Show_Str(130,70,200,12,"Channel:",12,0);
	
//	Show_Str_Mid(0,30,"East_West",24,240); 
//	Show_Str(10,20,200,12,"Address：",12,0);
//	Show_Str(10,40,200,12,"Channel：",12,0);
//	Show_Str(10,60,200,12,"Rate：",12,0);
//	Show_Str(10,80,200,12,"Power：",12,0);
//	Show_Str(10,100,200,12,"Mode：",12,0);
//	Show_Str(10,120,200,12,"State：",12,0);
	
	POINT_COLOR=RED;
	num = num_count(LoRa_CFG.addr);
	LCD_ShowNum(80,70,LoRa_CFG.addr,num,12);
	num = num_count(LoRa_CFG.chn);
	LCD_ShowxNum(180,70,LoRa_CFG.chn,num,12,0);
//	Show_Str(10+60,60,200,12,(u8*)cnttbl_rate[LoRa_CFG.wlrate],12,0);
//	Show_Str(10+60,80,200,12,(u8*)cnttbl_power[LoRa_CFG.power],12,0);
//	Show_Str(10+60,100,200,12,(u8*)cnttbl_mode[LoRa_CFG.mode],12,0);
//	Show_Str(10+60,120,200,12,(u8*)cnttbl_sta[LoRa_CFG.mode_sta],12,0);
	
	POINT_COLOR=GREEN;
	Show_Str(30,100,200,24,"GN:",24,0);
	POINT_COLOR=YELLOW;
	Show_Str(30,140,200,24,"YL:",24,0);
	POINT_COLOR=RED;
	Show_Str(30,180,200,24,"RD:",24,0);
	
	POINT_COLOR=BLACK;
	Show_Str(10,220,200,16,"Data_receive:",16,0);
	Show_Str(10,265,200,16,"Data_send:",16,0);
	
//	Show_Str(130+15,20,200,12,"KEY_UP: Menu",12,0);
//	Show_Str(130+15,40,200,12,"KEY1: Send_data",12,0);
	

	if(LoRa_CFG.mode_sta==LORA_STA_Dire)//若是定向传输
	{
//	  Show_Str(130+15,60,200,12,"KEY0: Set_parameter",12,0);
		Show_Str(80,70,200,12,"                                  ",12,0);//清空之前的显示
	  POINT_COLOR=BLACK;
		Show_Str(30,70,200,12,"Tar_ad:",12,0);
	  Show_Str(130,70,200,12,"Tar_ch:",12,0);
		
	  POINT_COLOR=RED;
	  num = num_count(obj_addr);
		LCD_ShowNum(80,70,obj_addr,num,12);		
	  num = num_count(obj_chn);
    LCD_ShowxNum(180,70,obj_chn,num,12,0);	
		
	}
	
}

