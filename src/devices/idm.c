/** @file
    ERT IDM sensors.

    Copyright (C) 2020 Florin Muntean.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "decoder.h"
#include "decoder_util.h"
#include <stdlib.h>
/**
ERT IDM sensors.

Random information:

https://github.com/bemasher/rtlamr

https://github.com/bemasher/rtlamr/wiki/Protocol


Data layout:

Interval Data Message (IDM) for Net Meters
A 92 byte message containing differential consumption intervals as well as total consumed and generated power.

Field	                        Length	        Value	    Description
Preamble	                        2	        0x5555	
Sync Word	                        2	        0x16A3	
Protocol ID 	                    1	        0x1C	
Packet Length	                    1	        0x5C	
Hamming Code	                    1	        0xC6	    Hamming code of first byte.
Application Version	                1		
Endpoint Type	                    1		                Least significant nibble is equivalent to SCM's commodity type field.
Endpoint ID	                        4		
Consumption Interval Count	        1		                Interval counter indicates when a received message is reporting a different interval from the last.
Programming State	                1		
Unknown	                            13		
Last Generation Count	            3		                Total power generated.
Unknown	                            3		
Last Consumption Count	            4		                Total power consumed.
Differential Consumption Intervals	48		                27 intervals of 14-bit unsigned integers.
Transmit Time Offset	            2		                1/16ths of a second since the first transmission for this interval.
Meter ID Checksum	                2		                CRC-16-CCITT of Meter ID.
Packet Checksum	                    2		                CRC-16-CCITT of packet starting at Packet Type.

https://web.archive.org/web/20090828043201/http://www.openamr.org/wiki/ItronERTModel45

*/

static int idm_decode(r_device *decoder, bitbuffer_t *bitbuffer)
{
    static const uint8_t IDM_PREAMBLE[]  = {0x55, 0x55, 0x16, 0xA3, 0x1C, 0x5C, 0xC6};
    uint8_t *b;
    uint8_t idm_type, app_version, consumption_interval_count, programming_state;
    uint32_t last_consumption, ert_id, last_generation, last_consumption_net;
    uint16_t sn_crc, packet_crc;
    data_t *data;

    if (bitbuffer->bits_per_row[0] != 92*8) //92 bytes long package
        return DECODE_ABORT_LENGTH;

    //MFD: TODO: implement CRC checking

    b = bitbuffer->bb[0];
    /* old crc from ert.c
    
    if (crc16(&b[2], 10, 0x6F63, 0))
        return DECODE_FAIL_MIC;
    */
    
    /* CRC from rtlamr go lang
    // If the checksum fails, bail.
		if residue := p.Checksum(p.data.Bytes[4:92]); residue != p.Residue {
			continue
		}

		// If the serial checksum fails, bail.
		buf := make([]byte, 6)
		copy(buf, p.data.Bytes[9:13])
		copy(buf[4:], p.data.Bytes[88:90])
		if residue := p.Checksum(buf); residue != p.Residue {
			continue
		}
    */


    for(unsigned int i=0;i<sizeof(IDM_PREAMBLE);i++)
      if (IDM_PREAMBLE[i] != b[i])
         return DECODE_ABORT_EARLY; // no preamble matching


    /* Extract parameters */
    app_version = b[7];
    idm_type = b[8] & 0x0F;
    ert_id = (b[9]<<24) | (b[10]<<16) | (b[11]<<8) | b[12];
    consumption_interval_count = b[13];
    programming_state = b[14];
    last_consumption = (b[25]<<16) | (b[26]<<8) | b[27];
    last_generation  = (b[28]<<16) | (b[29]<<8) | b[30];
    last_consumption_net = (b[34]<<24) | (b[35]<<16) | (b[36]<<8) | b[37];
    sn_crc = (b[88]<<8) | b[89];
    packet_crc = (b[90]<<8) | b[91]; 
    
//extract raw data for further processing if needed

    char strData[92*2+1];
    const char *hex="0123456789ABCDEF";
    for(int i=0;i<92;i++)
    {
      strData[i*2] = hex[(b[i]>>4) & 0xF];
      strData[i*2+1] = hex[b[i] & 0x0F];
    }
    strData[92*2]=0;

//    char *strData = bitrow_asprint_code(bitbuffer->bb[0], bitbuffer->bits_per_row[0]);  


    /* clang-format off */
    data = data_make(
            "model",                  "",                        DATA_STRING, "netIDM",
            "id",                     "Id",                      DATA_INT,    ert_id,
            "version",                "Version",                 DATA_INT, app_version,
            "idm_type",               "IDM Type",                DATA_INT, idm_type,
            "consumption_interval",   "Consumption Interval",    DATA_INT, consumption_interval_count,
            "programming_state",      "Programming State",       DATA_INT, programming_state,
            "generation",             "Generation Count",        DATA_INT, last_generation,
            "consumption",            "Consumption Count",       DATA_INT, last_consumption,
            "net",                    "Consumption NET",         DATA_INT, last_consumption_net,
            "sn_crc",                 "Serial Number CRC",       DATA_INT, sn_crc,
            "packet_crc",             "Packet CRC",              DATA_INT, packet_crc,
            "codes",                  "RAW DATA",					       DATA_STRING, strData,
            "mic",                    "Integrity",               DATA_STRING, "CRC",
            NULL);
    /* clang-format on */

    decoder_output_data(decoder, data);
    
//    free(strData);
    return 1;
}

static char *output_fields[] = {
        "model",
        "id",
        "version",
        "idm_type",
        "consumption_interval",
        "programming_state",
        "generation",
        "consumption",
        "net",
        "sn_crc",
        "packet_crc",
        "codes",
	"mic",
        NULL
};

r_device ert_idm = {
        .name        = "IDM",
        .modulation  = OOK_PULSE_MANCHESTER_ZEROBIT,
        .short_width = 30,
        .long_width  = 30,
        .gap_limit   = 0,
        .reset_limit = 64,
        .decode_fn   = &idm_decode,
        .disabled    = 0,
        .fields      = output_fields,
        .tolerance   = 10 //us
};
