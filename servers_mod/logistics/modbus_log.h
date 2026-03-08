#ifndef MFG_MODBUS_LOG_H
#define MFG_MODBUS_LOG_H

#include <stdint.h>
#include <modbus/modbus.h>

/*------------------PLC ADDR-----------------------------*/
enum SCM_COIL_ADDR {
    SCM_QX_START1 = 0,
    SCM_QX_STOP   = 1,
    SCM_QX_AUTO   = 2,
    SCM_QX_START2 = 8,
    SCM_QX_START3 = 16,
    SCM_QX_RUN1   = 4,
    SCM_QX_RUN2   = 12,
    SCM_QX_RUN3   = 20
};

enum SCM_REG_ADDR {
    SCM_MW_COUNT1 = 1025,
    SCM_MW_COUNT2 = 1026,
    SCM_MW_COUNT3 = 1027,
    SCM_MW_MAX1   = 1028,
    SCM_MW_MAX2   = 1029,
    SCM_MW_MAX3   = 1030,
    SCM_MW_SPEED1 = 1031,
    SCM_MW_SPEED2 = 1032,
    SCM_MW_SPEED3 = 1033
};

/* lifecycle */
int  mb_init(const char *ip, int port, int slave_id);
void mb_cleanup(void);
int  mb_is_connected(void);

/* reconnect */
int  mb_reconnect(void);

/* single bit/register R/W */
int  mb_write_bit_retry(int addr, int value);
int  mb_write_reg_u16_retry(int addr, uint16_t value);
int  mb_read_bit_retry(int addr, uint8_t *out);
int  mb_read_reg_u16_retry(int addr, uint16_t *out);

/* helpers */
void mb_pulse_coil(int addr, int pulse_ms);
void mb_set_coil(int addr, int value);

/* optional: raw access if needed later */
modbus_t *mb_get_ctx(void);

#endif