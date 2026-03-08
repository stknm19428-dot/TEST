#ifndef MFG_MODBUS_MFG_H
#define MFG_MODBUS_MFG_H

#include <stdint.h>
#include <modbus/modbus.h>

/* ---------------- Coil Address ---------------- */
enum MFG_COIL_ADDR {
    /* Write : PLC control */
    MFG_COIL_START      = 24,  /* %QX3.0  PB_Start */
    MFG_COIL_STOP       = 25,  /* %QX3.1  PB_Stop */

    /* Read : Conveyor running state */
    MFG_COIL_RUN_MAIN   = 28,  /* %QX3.4  Motor_Conveyor1 */
    MFG_COIL_MAIN_MERGE = 31,  /* %QX3.7  Manual_Sensor_MAIN */
    MFG_COIL_RUN_MID    = 36,  /* %QX4.4  Motor_Conveyor2 */
    MFG_COIL_RUN_LOW    = 44   /* %QX5.4  Motor_Conveyor3 */
};

/* ---------------- Register Address ---------------- */
enum MFG_REG_ADDR {
    MFG_REG_FINAL_TOTAL   = 1034, /* %MW10 */
    MFG_REG_CURRENT_COUNT = 1034  /* 필요하면 이름 통일해서 하나만 써도 됨 */
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