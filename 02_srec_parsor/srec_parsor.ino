/*
  Blink

  Turns an LED on for one second, then off for one second, repeatedly.

  Most Arduinos have an on-board LED you can control. On the UNO, MEGA and ZERO
  it is attached to digital pin 13, on MKR1000 on pin 6. LED_BUILTIN is set to
  the correct LED pin independent of which board is used.
  If you want to know what pin the on-board LED is connected to on your Arduino
  model, check the Technical Specs of your board at:
  https://www.arduino.cc/en/Main/Products

  modified 8 May 2014
  by Scott Fitzgerald
  modified 2 Sep 2016
  by Arturo Guadalupi
  modified 8 Sep 2016
  by Colby Newman

  This example code is in the public domain.

  http://www.arduino.cc/en/Tutorial/Blink
*/

#include "stdint.h"
#include "string.h"

extern "C"

#define MAX_ADDRESS_BP 4
#define MAX_DATA_BP 32
#define MAX_PHSIZE_BP 37

typedef enum
{
    SFILE_ERROR_OK = 0,
    SFILE_ERROR_FORMAT = 1
} sfile_error_t;

typedef union
{
    uint8_t Byte[MAX_PHSIZE_BP]; /* Byte level access to the Phrase */
    struct
    {
        char PhraseType;                       /* Type of received record (e.g. S0, S1, S5, S9...) */
        uint8_t PhraseSize;                    /* Phrase size (address + data + checksum) */
        uint8_t PhraseAddress[MAX_ADDRESS_BP]; /* Address, depending on the type of record it might vary */
        uint8_t PhraseData[MAX_DATA_BP];       /* Maximum 32 data bytes */
        uint8_t PhraseCRC;                     /* Checksum of size + address + data */
    } F;
} BootPhraseStruct;

char s1[] = "S00600004844521B";
char s2[] = "S325000200201800D000709FE7F37090C0007078052054640010707B012854640010707FE700A0";

char srec_lld_line_buffer[82];
uint16_t line_buffer_process_idx = 0U;
sfile_error_t srec_sfile_error;
BootPhraseStruct srec_lld_sline_data;

int serial_putc(char c, struct __file *)
{
    Serial.write(c);
    return c;
}
void printf_begin(void)
{
    fdevopen(&serial_putc, 0);
}

uint8_t srec_lld_GetChar(uint16_t idx)
{
    return srec_lld_line_buffer[idx];
}

uint8_t srec_lld_char2u8(char c)
{
    if (c >= 'A')
    {
        return (c - 'A' + 10);
    }
    else
    {
        return (c - '0');
    }
}

uint8_t srec_lld_2char2u8(char *p_s)
{
    return ((srec_lld_char2u8(p_s[0]) << 4) + srec_lld_char2u8(p_s[1]));
}

void srec_get_phrase(BootPhraseStruct *BP)
{
    char type = 0;
    uint8_t length = 0U;
    uint8_t i = 0U;

    /* Get SREC line start ('S') */
    do
    {
        type = srec_lld_GetChar(line_buffer_process_idx++);
        if (type != 'S')
        {
            srec_sfile_error = SFILE_ERROR_FORMAT;
        }
        if (line_buffer_process_idx >= 80U)
        {
            /* already reach the end */
            line_buffer_process_idx = 0U;
            break;
        }
    } while (type != 'S');

    /* Store phrase type */
    BP->F.PhraseType = srec_lld_line_buffer[line_buffer_process_idx++];

    /* Get byte count */
    length = srec_lld_2char2u8(&srec_lld_line_buffer[line_buffer_process_idx]);
    BP->F.PhraseSize = length;
    line_buffer_process_idx += 2U;

    /* Check records with a 24-bit address field */
    if ((BP->F.PhraseType == '2') || (BP->F.PhraseType == '6') || (BP->F.PhraseType == '8'))
    {
        /* Get address */
        for (i = 0; i < 3; i++)
        {
            BP->F.PhraseAddress[i] = srec_lld_GetChar(line_buffer_process_idx++);
        }
        length = length - 3;
    }
    else if ((BP->F.PhraseType == '3') | (BP->F.PhraseType == '7'))
    {
        /* Check records with a 32-bit address field */
        /* Get address */
        for (i = 0; i < 4; i++)
        {
            BP->F.PhraseAddress[i] = srec_lld_2char2u8(&srec_lld_line_buffer[line_buffer_process_idx]);
            line_buffer_process_idx += 2U;
        }
        length = length - 4;
    }
    else
    {
        /* All the other records have a 16-bit address field */
        /* Get address */
        for (i = 0; i < 2; i++)
        {
            BP->F.PhraseAddress[i] = srec_lld_2char2u8(&srec_lld_line_buffer[line_buffer_process_idx]);
            line_buffer_process_idx += 2U;
        }
        length = length - 2;
    }

    /* Get data stream */
    for (i = 0; ((i < (length - 1)) && (length <= MAX_DATA_BP + 1)); i++)
    {
        BP->F.PhraseData[i] = srec_lld_2char2u8(&srec_lld_line_buffer[line_buffer_process_idx]);
        line_buffer_process_idx += 2U;
    }

    /* Get CRC */
    BP->F.PhraseCRC = srec_lld_2char2u8(&srec_lld_line_buffer[line_buffer_process_idx]);

    line_buffer_process_idx = 0;
}
// the setup function runs once when you press reset or power the board
void setup()
{
    uint8_t i = 0U;

    Serial.begin(115200);
    printf_begin();

    printf("test for sfile parse:\n");

    srec_lld_sline_data.F.PhraseType = 0;
    srec_lld_sline_data.F.PhraseSize = 0;
    *(uint32_t *)srec_lld_sline_data.F.PhraseAddress = 0;

    printf("test for S1\n");
    memcpy(srec_lld_line_buffer, s1, sizeof(s1));
    srec_get_phrase(&srec_lld_sline_data);
    printf("srec_lld_sline_data.F.PhraseType = %c\n", srec_lld_sline_data.F.PhraseType);
    printf("srec_lld_sline_data.F.PhraseSize = %d\n", srec_lld_sline_data.F.PhraseSize);
    printf("srec_lld_sline_data.F.PhraseAddress = 0x%02x%02x%02x%02x\n",
                                                    srec_lld_sline_data.F.PhraseAddress[0],
                                                    srec_lld_sline_data.F.PhraseAddress[1],
                                                    srec_lld_sline_data.F.PhraseAddress[2],
                                                    srec_lld_sline_data.F.PhraseAddress[3]);
    for(i = 0U; i < srec_lld_sline_data.F.PhraseSize - 5; i++)
    {
        printf("%02x", srec_lld_sline_data.F.PhraseData[i]);
    }
    printf("srec_lld_sline_data.F.PhraseCRC = %02x\n", srec_lld_sline_data.F.PhraseCRC);
    printf("\n");

    printf("test for S2\n");
    memcpy(srec_lld_line_buffer, s2, sizeof(s2));
    srec_get_phrase(&srec_lld_sline_data);
    printf("srec_lld_sline_data.F.PhraseType = %c\n", srec_lld_sline_data.F.PhraseType);
    printf("srec_lld_sline_data.F.PhraseSize = %d\n", srec_lld_sline_data.F.PhraseSize);
    printf("srec_lld_sline_data.F.PhraseAddress = 0x%02x%02x%02x%02x\n",
                                                    srec_lld_sline_data.F.PhraseAddress[0],
                                                    srec_lld_sline_data.F.PhraseAddress[1],
                                                    srec_lld_sline_data.F.PhraseAddress[2],
                                                    srec_lld_sline_data.F.PhraseAddress[3]);
    for(i = 0U; i < srec_lld_sline_data.F.PhraseSize - 5; i++)
    {
        printf("%02x", srec_lld_sline_data.F.PhraseData[i]);
    }
    printf("\n");
    printf("srec_lld_sline_data.F.PhraseCRC = %02x\n", srec_lld_sline_data.F.PhraseCRC);
}

// the loop function runs over and over again forever
void loop()
{
}
