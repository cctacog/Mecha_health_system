#include "MCP25625_RP2040.h"
// pico-sdk
#include "pico/stdlib.h"
#include "hardware/spi.h"


//again thx joseph :)


void MCP25625_Setup(spi_inst_t *spi, uint sck_pin, uint cs_pin, uint spi_rx, uint spi_tx){
    spi_init(spi, MCP25625_SPI_HZ);     // 10MHz
    
    gpio_init(cs_pin);
    gpio_set_dir(cs_pin, GPIO_OUT);
    gpio_pull_up(cs_pin);               // Pullup just in case
    gpio_put(cs_pin, 1);                // CS is active low

    gpio_set_function(sck_pin, GPIO_FUNC_SPI);
    gpio_set_function(spi_rx, GPIO_FUNC_SPI);
    gpio_set_function(spi_tx, GPIO_FUNC_SPI);

    spi_get_hw(spi)->SSPCR0 &= ~(0x000F);   // Clear data width
    spi_get_hw(spi)->SSPCR0 |= 0x0007;      // set to 8 bit txfr widths
}

// spi_write_read_blocking(spi_inst_t *spi, const uint8_t *src, uint8_t *dst, size_t len)
uint8_t MCP25625_Read_Blocking(spi_inst_t *spi ,uint8_t address){
    uint8_t tval[3]; 
    tval[0] = CAN_CTRL_READ; 
    tval[1] = address;
    uint8_t rval[3];
    spi_write_read_blocking(spi, tval, rval, sizeof(tval));
    return rval[2];
}

void MCP25625_CAN_Message_2_Raw_Arr(uint8_t *arr, struct CAN_Message *msg){
    if(msg && arr){
        arr[XBnDLC] = (msg->len & 0x0F);   // Set len
        arr[XBnSIDH] = ((msg->id >> 3) & 0x0F);
        arr[XBnSIDL] = ((msg->id & 0x07) << 5);
        if(msg->flags.ext){
            arr[XBnSIDL] |= (1 << 3);   // set extended
            arr[XBnSIDL] |= ((msg->id >> (16 + STDIDLEN)) & 0x03);
            arr[XBnEID8] =  (msg->id >> (8 + STDIDLEN));
            arr[XBnEID0] =  (msg->id >> (0 + STDIDLEN));
            arr[XBnDLC]  |= ((msg->flags.ext) ? (1 << 6) : 0);  // ext rtr
        } else {
            arr[XBnSIDL] |= (((msg->flags.remote) ? 1 : 0) << 4);
        }

        for(int n = 0; n < msg->len; ++n) arr[XBnD0 + n] = msg->data[n];
    }
}

void MCP25625_Raw_Arr_2_CAN_Message(struct CAN_Message *msg, uint8_t *arr){
    if(msg && arr){
        msg->id = (arr[XBnSIDL] >> 5) | arr[XBnSIDH];
        msg->flags.ext = arr[XBnSIDL] & (1 << 3);
        if(msg->flags.ext){
            msg->id |= (arr[XBnSIDL] & 0x03) << (16 + STDIDLEN);
            msg->id |= (arr[XBnEID8] << (8 + STDIDLEN));
            msg->id |= (arr[XBnEID0] << (0 + STDIDLEN));

            msg->flags.remote = arr[XBnDLC] & (1 << 6);
        } else {
            msg->flags.remote = arr[XBnSIDL] & (1 << 4);
        }

        msg->len = (arr[XBnDLC] & 0xF);

        for(int n = 0; n < msg->len; ++n) msg->data[n] = arr[XBnD0 + n]; 
    }
}

uint8_t MCP25625_Read_RX_Buffer_Blocking(spi_inst_t *spi, struct CAN_Message *msg, uint8_t buff_num){
    uint8_t tval[13]; 
    tval[0] = CAN_READ_RX_BUFF | buff_num; 
    uint8_t rval[13];
    // Starting transfer from top of RXBxSIDH -> 13 bytes total read
    //  Starting transfer from top of data field -> 8 bytes total read
    //  13 - 8 = 5
    if(buff_num & 0x01){    // Full read
        spi_write_read_blocking(spi, tval, rval, sizeof(tval));
        MCP25625_Raw_Arr_2_CAN_Message(msg, rval);
    } else {                // Just Data Read
        spi_write_read_blocking(spi, tval, rval, sizeof(tval) - MCP_MAILBOX_D0_OFFSET);
        for(int n = 0; n < 8; ++n) msg->data[n] = rval[n];
    }
    
    
    return rval[1];
}

// int spi_write_blocking(spi_inst_t *spi, const uint8_t *src, size_t len);
void MCP25625_Write_Blocking(spi_inst_t *spi, uint8_t address, uint8_t data){
    uint8_t tval[3];
    tval[0] = CAN_CTRL_WRITE;
    tval[1] = address;
    tval[2] = data;
    spi_write_blocking(spi, tval, sizeof(tval));
}

void MCP25625_Write_TX_Buffer_Blocking(spi_inst_t *spi, uint8_t buff_num, uint8_t data){
    uint8_t tval[2];
    tval[0] = CAN_LOAD_TX_BUFF | buff_num;
    tval[1] = data;
    spi_write_blocking(spi, tval, sizeof(tval));
}

void MCP25625_RTS_Blocking(spi_inst_t *spi, uint8_t buff_num){
    uint8_t tval = CAN_CTRL_RTS | buff_num;
    spi_write_blocking(spi, tval, sizeof(tval));
}

void MCP25625_Bit_Modify_Blocking(spi_inst_t *spi, uint8_t addr, uint8_t mask, uint8_t data){
    uint8_t tval[4];
    tval[0] = CAN_CTRL_WRITE;
    tval[1] = addr;
    tval[2] = mask;
    tval[3] = data;
    spi_write_blocking(spi, tval, sizeof(tval));
}

uint8_t MCP25625_Read_Status_Blocking(spi_inst_t *spi){
    uint8_t tval[3]; 
    tval[0] = CAN_READ_STATUS; 
    uint8_t rval[3];
    spi_write_read_blocking(spi, tval, rval, sizeof(tval));
    return rval[2]; // bytes [1] and [2] are the same
}

uint8_t MCP25625_Read_RX_Buff_Status_Blocking(spi_inst_t *spi){
    uint8_t tval[3]; 
    tval[0] = CAN_RX_BUFF_STATUS; 
    uint8_t rval[3];
    spi_write_read_blocking(spi, tval, rval, sizeof(tval));
    return rval[2]; // bytes [1] and [2] are the same
}