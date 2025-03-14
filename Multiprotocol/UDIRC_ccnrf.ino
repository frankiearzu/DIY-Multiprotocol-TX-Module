/*
 This project is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

Multiprotocol is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with Multiprotocol.  If not, see <http://www.gnu.org/licenses/>.
 */
#if defined(UDIRC_CCNRF_INO)

#include "iface_xn297.h"

#define FORCE_UDIRC_ORIGINAL_ID

#define UDIRC_PAYLOAD_SIZE			15
#define UDIRC_RF_NUM_CHANNELS		4
#define UDIRC_PACKET_PERIOD			8000
#define UDIRC_BIND_COUNT			2000
#define UDIRC_WRITE_TIME			1500

enum {
	UDIRC_DATA=0,
	UDIRC_RX,
};

static void __attribute__((unused)) UDIRC_send_packet()
{
	if(rf_ch_num==0)
	{
		XN297_Hopping(hopping_frequency_no);
		debug("H %d ",hopping_frequency_no);
		hopping_frequency_no++;
		hopping_frequency_no &= 3;
	}

	memset(&packet[3], 0x00, 12);
	if(bind_counter)
	{//Bind in progress
		bind_counter--;
		if(bind_counter)
		{//Bind
			packet[0] = 0x01;
			memcpy(&packet[1],rx_tx_addr,5);
		}
		else
		{//Switch to normal
			BIND_DONE;
			XN297_SetTXAddr(rx_tx_addr, 5);
			XN297_SetRXAddr(rx_tx_addr, UDIRC_PAYLOAD_SIZE);
		}
	}
	if(!bind_counter)
	{//Normal
		packet[0] = 0x08;
		packet[1] = convert_channel_16b_limit(AILERON,0,200);	//ST
		packet[2] = convert_channel_16b_limit(THROTTLE,0,200);	//TH
		packet[3] = convert_channel_16b_limit(ELEVATOR,0,200);	//CH4
		packet[4] = convert_channel_16b_limit(RUDDER,0,200);	//CH3
	}
	//packet[5/6..8] = 00 unknown
	packet[9] = convert_channel_16b_limit(CH5,0,200);			//ESP
	packet[10] = convert_channel_16b_limit(CH6,0,200);			//ST_TRIM
	packet[11] = convert_channel_16b_limit(CH7,0,200);			//ST_DR
	packet[12] = GET_FLAG(CH8_SW,  0x40)						//TH.REV
				|GET_FLAG(CH9_SW,  0x80);						//ST.REV
	//packet[13] = 00 unknown
	for(uint8_t i=0;i<UDIRC_PAYLOAD_SIZE-1;i++)
		packet[14] += packet[i];
	// Send
	XN297_SetPower();
	XN297_SetTxRxMode(TX_EN);
	XN297_WriteEnhancedPayload(packet, UDIRC_PAYLOAD_SIZE,false);
	#ifdef DEBUG_SERIAL
		for(uint8_t i=0; i < UDIRC_PAYLOAD_SIZE; i++)
			debug("%02X ", packet[i]);
		debugln();
	#endif
}

static void __attribute__((unused)) UDIRC_initialize_txid()
{
	#ifdef FORCE_UDIRC_ORIGINAL_ID
		rx_tx_addr[0] = 0xD0;
		rx_tx_addr[1] = 0x06;
		rx_tx_addr[2] = 0x00;
		rx_tx_addr[3] = 0x00;
		rx_tx_addr[4] = 0x81;
		hopping_frequency[0] = 45;
		hopping_frequency[1] = 59;
		hopping_frequency[2] = 52;
		hopping_frequency[3] = 67;
	#endif
}

static void __attribute__((unused)) UDIRC_RF_init()
{
	XN297_Configure(XN297_CRCEN, XN297_SCRAMBLED, XN297_250K);
	//Bind address
	XN297_SetTXAddr((uint8_t*)"\x01\x03\x05\x07\x09", 5);
	XN297_SetRXAddr((uint8_t*)"\x01\x03\x05\x07\x09", UDIRC_PAYLOAD_SIZE);
	XN297_HoppingCalib(UDIRC_RF_NUM_CHANNELS);
}

uint16_t UDIRC_callback()
{
	bool rx;
	switch(phase)
	{
		case UDIRC_DATA:
			rx = XN297_IsRX();
			XN297_SetTxRxMode(TXRX_OFF);
			#ifdef MULTI_SYNC
				telemetry_set_input_sync(UDIRC_PACKET_PERIOD);
			#endif
			UDIRC_send_packet();
			if(rx)
			{
				uint8_t val=XN297_ReadEnhancedPayload(packet_in, UDIRC_PAYLOAD_SIZE);
				debug("RX %d",val);
				if(val==0)
				{
					rf_ch_num = 1;
					if(bind_counter)
						bind_counter=1;
				}
				else
				{
					#ifdef DEBUG_SERIAL
						for(uint8_t i=0; i < UDIRC_PAYLOAD_SIZE; i++)
							debug(" %02X", packet_in[i]);
						debugln();
					#endif
				}
				//else
				//	debug(" NOK");
				debugln("");
			}
			phase++;
			return UDIRC_WRITE_TIME;
		default: //UDIRC_RX
			//Wait for the packet transmission to finish
			while(XN297_IsPacketSent()==false);
			//Switch to RX
			XN297_SetTxRxMode(TXRX_OFF);
			XN297_SetTxRxMode(RX_EN);
			phase = UDIRC_DATA;
			return UDIRC_PACKET_PERIOD - UDIRC_WRITE_TIME;
	}
	return 0;
}

void UDIRC_init()
{
	UDIRC_initialize_txid();
	UDIRC_RF_init();

	bind_counter = IS_BIND_IN_PROGRESS ? UDIRC_BIND_COUNT : 1;
	phase = UDIRC_DATA;
	hopping_frequency_no = 0;
	rf_ch_num = 0;
}

#endif
