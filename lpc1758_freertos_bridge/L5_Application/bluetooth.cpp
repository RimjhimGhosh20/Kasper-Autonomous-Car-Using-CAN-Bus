/*
 * bluetooth.cpp
 *
 *  Created on: Oct 20, 2016
 *      Author: Bharat Khanna
 */

#include"bluetooth.hpp"
#include<uart2.hpp>
#include<string.h>
#include "printf_lib.h"
#include "../../_can_dbc/generated_can.h"
#include"stdio.h"
#include "io.hpp"
#include <string.h>
#include <stdlib.h>
#include <string.h>
#define print_cordinate 1

SemaphoreHandle_t Bluetooth_Lat_Lon_Semaphore = NULL;

char Bluetooth_Buffer[512];
char stored_Bluetooth_data[512];
char Location_Buffer_temp[512];
//char send_cor_data[]= "loc,37.3352682,-121.881361$";
char send_cor_data[64];
char send_ACK[]= "ack$";
char CONNECT[] = "connect";
char START[] = "start";
char STOP[] = "stop";
char NULLPOINTER[] = "nullptr";
bool check_received = false;
float store_cor[512];
int check_point_total;
//char Send_these coordinate[] = 12.456789;
Bluetooth_Received *Bluetooth_Rec;
SemaphoreHandle_t BluetoothSemaphore = NULL;
char send_end_line[6];
//dbc_encode_and_send_BRIDGE_TOTAL_CHECKPOINT
GPS_LOCATION_t GPS_LOCATION_RECEIVE = {0};

int Send_App_curr_loc(char *lat, char *lon)
{
    //printf("%s \n",lat);
    //printf("%s \n",lon);
	memset(send_cor_data, 0, sizeof(send_cor_data));
    strcat(send_cor_data,"loc,");
    strcat(send_cor_data,lat);
    strcat(send_cor_data,",");
    strcat(send_cor_data,lon);
    strcat(send_cor_data,"$");
    //printf("%s \n",send_cor_data);
    return 0;
}

int indent(char *buffer)
{
    int count = 0;
    char Start_Location[1];
    char *token = NULL;
    int counter=0;
    float temp=0.0;

    token = strtok((char *)buffer, ",");
    if (! token)
        return false;
    Start_Location[0] = token[0];
    check_point_total = int(Start_Location[0])-'0';
    printf("check_point_total is %d\n",check_point_total);
    count = check_point_total*2;
    printf ("count = %d\n",count);

    while(count != 0)
    {
        token = strtok (NULL, " ,");
        temp = atof(token);
        store_cor[counter]=temp;
        counter++;
        count--;
    }
#if print_cordinate
    for(int j=0;j< (check_point_total*2);j++)
    {
         printf("store_cor[%d] = %f\n",j,store_cor[j]);
    }
#endif

    return 0;
}

void Send_BLUETOOTH_DATA_t()
{

}
bool dbc_app_send_can_msg(uint32_t mid, uint8_t dlc, uint8_t bytes[8])
{
	can_msg_t can_msg = { 0 };
	can_msg.msg_id                = mid;
	can_msg.frame_fields.data_len = dlc;
	memcpy(can_msg.data.bytes, bytes, dlc);

	return CAN_tx(can1, &can_msg, 0);
}

void canBusErrorCallBackRx(uint32_t ibits)
{
	const uint32_t errbit = (ibits >> 16) & 0x1F;
	const char * rxtx = ((ibits >> 21) & 1) ? "RX" : "TX";
	const uint32_t errc = (ibits >> 22) & 3;

	u0_dbg_put("\n\n ***** CAN BUS ENTERED ERROR STATE!\n");
	u0_dbg_printf("ERRC = %#x, ERRBIT = %#x while %s\n", errc, errbit, rxtx);
}

bool uart_putchar(char character)
{
	Uart2& Bluetooth_uart_2 = Uart2::getInstance();
	Bluetooth_uart_2.init(115200);
	Bluetooth_uart_2.putChar(character);
	while(! (LPC_UART2->LSR & (1 << 6)));
	return true;
}

Bluetooth_Enable::Bluetooth_Enable(uint8_t priority) :
       scheduler_task("Bluetooth_Enable", 2000, priority),Bluetooth_uart_2(Uart2::getInstance())
{
	Bluetooth_uart_2.init(Baud_Rate_Bluetooth_Uart2,Rx_Q_Size,Tx_Q_Size);
}
bool Bluetooth_Enable::init(void)
{
	   return true;
}


bool Bluetooth_Enable::run(void *p)
{
	//4,37.3352856722927,-121.88128352165222,37.33539123714316,-121.88132978975773,37.33547067726107,-121.88138745725155,37.3355319900463,-121.88143841922282,
	Bluetooth_uart_2.gets(Bluetooth_Buffer, sizeof(Bluetooth_Buffer), portMAX_DELAY);
	strcpy(stored_Bluetooth_data,Bluetooth_Buffer);
	if(strcmp(stored_Bluetooth_data,CONNECT) == 0)
	{
		u0_dbg_printf("Sent send_cor_data \n",send_cor_data);
		Bluetooth_uart_2.putline(send_cor_data, sizeof(send_cor_data));
	}
	else if((strcmp(stored_Bluetooth_data,START) == 0) || (strcmp(stored_Bluetooth_data,STOP) == 0) || (strcmp(stored_Bluetooth_data,NULLPOINTER) == 0))
	{
		// do nothing
	}
	else
	{
		strcpy(Location_Buffer_temp,Bluetooth_Buffer);
		indent(Location_Buffer_temp);
		Send_BLUETOOTH_DATA_t();
		Bluetooth_uart_2.putline(send_ACK, sizeof(send_ACK));
		check_received = true;
	}
	u0_dbg_printf("I have crossed the gets function %s\n",stored_Bluetooth_data);
	return true;
}


void Check_Start_STOP_Condition()
{
	START_CMD_APP_t START_CONDITION	=	{0};
	STOP_CMD_APP_t STOP_CONDITION	=	{0};
	BLUETOOTH_DATA_t BLUETOOTH_DATA_Location = {0};
	BRIDGE_TOTAL_CHECKPOINT_t BRIDGE_TOTAL_CHECKPOINT = {0};
	//if(stored_Bluetooth_data[0] == '1')
	if(strcmp(stored_Bluetooth_data,START) == 0)
	{
		START_CONDITION.START_CMD_APP_data = 1;
		dbc_encode_and_send_START_CMD_APP(&START_CONDITION);
		printf("sent start condition");
		memset(stored_Bluetooth_data, 0, sizeof(stored_Bluetooth_data));
	}
	//if((stored_Bluetooth_data[0] == '0') && (stored_Bluetooth_data[1] == '1'))
	else if(strcmp(stored_Bluetooth_data,STOP) == 0)
	{
		STOP_CONDITION.STOP_CMD_APP_data = 1;
		dbc_encode_and_send_STOP_CMD_APP(&STOP_CONDITION);
		printf("sent stop condition");
		memset(stored_Bluetooth_data, 0, sizeof(stored_Bluetooth_data));
	}
	else if(check_received == true)
	{
		int loop =0;
		BRIDGE_TOTAL_CHECKPOINT.BRIDGE_TOTAL_CHECKPOINT_NUMBER = check_point_total;
		dbc_encode_and_send_BRIDGE_TOTAL_CHECKPOINT(&BRIDGE_TOTAL_CHECKPOINT);
		for(loop = 0; loop < (check_point_total*2) ;loop=loop+2)
		{
			u0_dbg_printf("Checkpoint received");
			//u0_dbg_printf("LAT = %f\n",store_cor[loop]);
			BLUETOOTH_DATA_Location.BLUETOOTH_DATA_LAT = store_cor[loop];
			//u0_dbg_printf("LON =  %f\n",store_cor[loop+1]);
			BLUETOOTH_DATA_Location.BLUETOOTH_DATA_LON = store_cor[loop+1];
			dbc_encode_and_send_BLUETOOTH_DATA(&BLUETOOTH_DATA_Location);
		}
		check_received = false;
	}

}

void Can_Receive_ID_Task()
{
	can_msg_t can_msg_Info;
	RESET_t REST_Info = { 0 };
    char lat[16];
    char lon[16];
	if (CAN_rx(can1, &can_msg_Info, 0))
	{
		LE.off(1);
		dbc_msg_hdr_t can_msg_hdr;
		can_msg_hdr.dlc = can_msg_Info.frame_fields.data_len;
		can_msg_hdr.mid = can_msg_Info.msg_id;
		//u0_dbg_printf("id :%d\n",can_msg_hdr.mid);
		if(can_msg_Info.msg_id == STOP_CAR_HDR.mid)
			printf("Case_STOP_CAR\n");
		if(can_msg_Info.msg_id == RESET_HDR.mid)
			dbc_decode_RESET(&REST_Info, can_msg_Info.data.bytes, &can_msg_hdr);
		if(can_msg_Info.msg_id == CURRENT_LOCATION_ACK_HDR.mid)
			printf("Case_CURRENT_LOCATION_ACK\n");
		if(can_msg_Info.msg_id == RECEIVE_START_ACK_HDR.mid)
			printf("Case_RECEIVE_START_ACKt\n");
		if(can_msg_Info.msg_id == BRIDGE_POWER_SYNC_HDR.mid)
			printf("BRIDGE_POWER_SYNC_data\n");
		if(can_msg_Info.msg_id == GPS_LOCATION_HDR.mid)
		{
			dbc_decode_GPS_LOCATION(&GPS_LOCATION_RECEIVE, can_msg_Info.data.bytes, &can_msg_hdr);
			//printf("GPS_LOCATION_RECEIVE\n");
			sprintf(lat,"%f",GPS_LOCATION_RECEIVE.GPS_LOCATION_latitude);
			sprintf(lon,"%f",GPS_LOCATION_RECEIVE.GPS_LOCATION_longitude);
		    Send_App_curr_loc(lat,lon);
			//u0_dbg_printf("Latitude: %f ,Longitude: %f \n",GPS_LOCATION_RECEIVE.GPS_LOCATION_latitude,GPS_LOCATION_RECEIVE.GPS_LOCATION_longitude);
		}
		if(dbc_handle_mia_RESET(&REST_Info, 1))
		{
			REST_Info.RESET_data=RESET__MIA_MSG.RESET_data;
			LD.setNumber(REST_Info.RESET_data);
			LE.on(1);
		}
	}
}
