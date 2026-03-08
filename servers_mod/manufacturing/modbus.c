#include "modbus_mfg.h"
#include "modbus_mfg.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

static modbus_t *g_mb = NULL;
static int g_mb_connected = 0;

static int mb_connect_once(void) {
    if(!g_mb)
        return -1;

    if(modbus_connect(g_mb) == -1) {
        g_mb_connected = 0;
        return -1;
    }

    g_mb_connected = 1;
    return 0;
}

int mb_init(const char *ip, int port, int slave_id) {
    if(g_mb)
        return 0; /* already initialized */

    g_mb = modbus_new_tcp(ip, port);
    if(!g_mb) {
        fprintf(stderr, "[MFG][MODBUS] modbus_new_tcp failed\n");
        return -1;
    }

    modbus_set_slave(g_mb, slave_id);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 300 * 1000; /* 300 ms */
    modbus_set_response_timeout(g_mb, tv.tv_sec, tv.tv_usec);

    return mb_connect_once();
}

void mb_cleanup(void) {
    if(!g_mb)
        return;

    modbus_close(g_mb);
    modbus_free(g_mb);
    g_mb = NULL;
    g_mb_connected = 0;
}

int mb_is_connected(void) {
    return g_mb_connected;
}

int mb_reconnect(void) {
    if(!g_mb)
        return -1;

    modbus_close(g_mb);
    usleep(200 * 1000);

    return mb_connect_once();
}

int mb_write_bit_retry(int addr, int value) {
    if(!g_mb)
        return -1;

    int rc = modbus_write_bit(g_mb, addr, value);
    if(rc == 1)
        return 0;

    if(mb_reconnect() != 0)
        return -1;

    rc = modbus_write_bit(g_mb, addr, value);
    if(rc == 1)
        return 0;

    return -1;
}

int mb_write_reg_u16_retry(int addr, uint16_t value) {
    if(!g_mb)
        return -1;

    int rc = modbus_write_register(g_mb, addr, value);
    if(rc == 1)
        return 0;

    if(mb_reconnect() != 0)
        return -1;

    rc = modbus_write_register(g_mb, addr, value);
    if(rc == 1)
        return 0;

    return -1;
}

int mb_read_bit_retry(int addr, uint8_t *out) {
    if(!g_mb || !out)
        return -1;

    int rc = modbus_read_bits(g_mb, addr, 1, out);
    if(rc == 1)
        return 0;

    if(mb_reconnect() != 0)
        return -1;

    rc = modbus_read_bits(g_mb, addr, 1, out);
    if(rc == 1)
        return 0;

    return -1;
}

int mb_read_reg_u16_retry(int addr, uint16_t *out) {
    if(!g_mb || !out)
        return -1;

    uint16_t tmp = 0;
    int rc = modbus_read_registers(g_mb, addr, 1, &tmp);
    if(rc == 1) {
        *out = tmp;
        return 0;
    }

    if(mb_reconnect() != 0)
        return -1;

    rc = modbus_read_registers(g_mb, addr, 1, &tmp);
    if(rc == 1) {
        *out = tmp;
        return 0;
    }

    return -1;
}

void mb_pulse_coil(int addr, int pulse_ms) {
    if(!g_mb || !g_mb_connected) {
        fprintf(stderr, "[MFG][MODBUS] pulse skipped (not connected) addr=%d\n", addr);
        return;
    }

    if(mb_write_bit_retry(addr, 1) != 0)
        return;

    if(pulse_ms > 0)
        usleep((useconds_t)pulse_ms * 1000u);

    (void)mb_write_bit_retry(addr, 0);
}

void mb_set_coil(int addr, int value) {
    if(!g_mb || !g_mb_connected) {
        fprintf(stderr, "[MFG][MODBUS] set skipped (not connected) addr=%d\n", addr);
        return;
    }

    int rc = mb_write_bit_retry(addr, value ? 1 : 0);
    fprintf(stdout, "[MFG][MODBUS] set addr=%d <= %d (rc=%d)\n",
            addr, value ? 1 : 0, rc);

    uint8_t b = 0;
    int rr = mb_read_bit_retry(addr, &b);
    fprintf(stdout, "[MFG][MODBUS] readback addr=%d => %d (rr=%d)\n",
            addr, (int)b, rr);
}

modbus_t *mb_get_ctx(void) {
    return g_mb;
}