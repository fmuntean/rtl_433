/** @file
    ERT SCM+ sensors.

    Copyright (C) 2020 Florin Muntean.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "decoder.h"

/**
ERT SCM+ sensors.

Random information:

https://github.com/bemasher/rtlamr

https://github.com/bemasher/rtlamr/wiki/Protocol


Data layout:

Standard Consumption Message Plus (SCM+)
A 16 byte message containing total consumption information for a single meter.

Field	        Length	Value	Description
Frame Sync	    2	    0x16A3	
ProtocolID	    1	    0x1E	
Endpoint Type	1		        Least significant nibble is equivalent to SCM's endpoint type field.
Endpoint ID	    4		
Consumption	    4		        Total power consumed.
Tamper	        2		
Packet Checksum	2		        CRC-16-CCITT of packet starting at Protocol ID.

*/

static int scmp_decode(r_device *decoder, bitbuffer_t *bitbuffer)
{
    static const uint8_t IDM_PREAMBLE[]  = {0x16, 0xA3 }; //, 0x1E};
    uint8_t *b;
    uint8_t protocol,scm_type;
    uint32_t consumption, ert_id;
    uint16_t tamper,crc;
    data_t *data;

    if (bitbuffer->bits_per_row[0] != 16*8) //16 bytes long package
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


     crc = (b[14]<<8) | b[15];

//uint16_t calc_crc = crc16(&b[2], 12, 0x1021, 0);

    //if (crc_received != crc_calculated) {
    //    if (decoder->verbose > 1) {
  //  fprintf(stderr, "CRC check (0x%X != 0x%X)\n", calc_crc, crc);
    //    }
    //    return DECODE_FAIL_MIC;
   // }


	uint16_t calc_crc = crc16(&b[2], 12, 0x1021, 0xFFFF) ^ 0xFFFF;
 //fprintf(stderr, "CRC check (0x%X != 0x%X)\n", calc_crc, crc);

   // calc_crc = crc16(&b[2], 12, 0x8408, 0);
 //fprintf(stderr, "CRC check (0x%X != 0x%X)\n", calc_crc, crc);

//calc_crc = crc16(&b[2], 12, 0x8408, 0xFFFF);

 //fprintf(stderr, "CRC check (0x%X != 0x%X)\n", calc_crc, crc);



// fprintf(stderr, "CRC check (0x%X != 0x%X)\n", calc_crc, crc);

//      uint16_t calc_crc = crc16lsb(&b[2],12,0x8408,0);
    /* Extract parameters */
    protocol = b[2];
    scm_type = b[3];
    ert_id = (b[4]<<24) | (b[5]<<16) | (b[6]<<8) | b[7];
    consumption = (b[8]<<24) | (b[9]<<16) | (b[10]<<8) | b[11];
    tamper = (b[12]<<8) | b[13];
    //crc = (b[14]<<8) | b[15]; 


    char strData[16*2+1];
    const char *hex="0123456789ABCDEF";
    for(int i=0;i<16;i++)
    {
      strData[i*2] = hex[(b[i]>>4) & 0xF];
      strData[i*2+1] = hex[b[i] & 0x0F];
    }
    strData[16*2]=0;

    
    /* clang-format off */
    data = data_make(
            "model",           "",                  DATA_STRING, "SCM+",
            "protocol", "Protocol ID",              DATA_INT, protocol,
            "scm_type",        "SCM+ Type",         DATA_INT, scm_type,
            "id",              "Id",                DATA_INT, ert_id,
            "consumption",  "Consumption",          DATA_INT, consumption,
            "tamper", "Tamper",                     DATA_INT, tamper,
            "crc", "Packet CRC",                    DATA_INT, crc,
	    "calc_crc", "CRC",                      DATA_INT, calc_crc,
            "raw", "RAW DATA",			   DATA_STRING, strData,
	    "mic",             "Integrity",         DATA_STRING, "CRC",
            NULL);
    /* clang-format on */

    decoder_output_data(decoder, data);
    return 1;
}

static char *output_fields[] = {
        "model",
        "protocol",
        "scm_type",
        "id",
        "consumption",
        "tamper",
        "crc",
        "calc_crc",
	"raw",
        "mic",
        NULL
};

r_device ert_scmp = {
        .name        = "SCM+",
        .modulation  = OOK_PULSE_MANCHESTER_ZEROBIT,
        .short_width = 30,
        .long_width  = 30,
        .gap_limit   = 0,
        .reset_limit = 64,
        .decode_fn   = &scmp_decode,
        .disabled    = 0,
        .fields      = output_fields
};
