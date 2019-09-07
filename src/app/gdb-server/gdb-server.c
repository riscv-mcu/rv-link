#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "riscv_encoding.h"
#include "interface/led.h"
#include "gdb-server.h"

#include "interface/rvl-target.h"
#include "gdb-serial.h"

typedef int16_t gdb_server_tid_t;

typedef struct gdb_server_s
{
    struct pt pt_server;
    struct pt pt_cmd;
    const char *cmd;
    size_t cmd_len;
    char *res;
    size_t res_len;
    bool target_running;
    bool gdb_connected;
    uint32_t target_pc;
    uint32_t continue_start;
    rvl_target_addr_t mem_addr;
    size_t mem_len;
    uint8_t mem_buffer[GDB_SERIAL_RESPONSE_BUFFER_SIZE / 2];
    int i;
    rvl_target_reg_t regs[RVL_TARGET_REG_NUM];

    gdb_server_tid_t tid_g;
    gdb_server_tid_t tid_G;
    gdb_server_tid_t tid_m;
    gdb_server_tid_t tid_M;
    gdb_server_tid_t tid_c;
}gdb_server_t;

static gdb_server_t gdb_server_i;
#define self gdb_server_i


PT_THREAD(gdb_server_cmd_q(void));
PT_THREAD(gdb_server_cmd_Q(void));
PT_THREAD(gdb_server_cmd_H(void));
PT_THREAD(gdb_server_cmd_g(void));
PT_THREAD(gdb_server_cmd_k(void));
PT_THREAD(gdb_server_cmd_c(void));
PT_THREAD(gdb_server_cmd_m(void));
PT_THREAD(gdb_server_cmd_question_mark(void));
PT_THREAD(gdb_server_cmd_ctrl_c(void));

PT_THREAD(gdb_server_check_target(void));

static void gdb_server_connected(void);
static void gdb_server_disconnected(void);

static void gdb_server_reply_ok(void);
static void gdb_server_reply_empty(void);

static void bin_to_hex(const uint8_t *bin, char *hex, size_t nbyte);
static void hex_to_bin(const char *hex, uint8_t *bin, size_t nbyte);
static void word_to_hex_le(uint32_t word, char *hex);
static void hex_to_word_le(const char *hex, uint32_t *word);


void gdb_server_init(void)
{
    PT_INIT(&self.pt_server);
    PT_INIT(&self.pt_cmd);

    self.target_running = false;
    self.gdb_connected = false;
}


PT_THREAD(gdb_server_poll(void))
{
    char c;

    PT_BEGIN(&self.pt_server);

    while (1) {
        if(self.gdb_connected && self.target_running) {
            self.cmd = gdb_serial_command_buffer();
            if(self.cmd != NULL) {
                self.cmd_len = gdb_serial_command_length();
                if(*self.cmd == '\x03' && self.cmd_len == 1) {
                    PT_WAIT_THREAD(&self.pt_server, gdb_server_cmd_ctrl_c());
                }
                gdb_serial_command_done();
            }

            PT_WAIT_THREAD(&self.pt_server, gdb_server_check_target());
            PT_YIELD(&self.pt_server);
        } else {
            PT_WAIT_UNTIL(&self.pt_server, (self.cmd = gdb_serial_command_buffer()) != NULL);
            self.cmd_len = gdb_serial_command_length();

            PT_WAIT_UNTIL(&self.pt_server, (self.res = gdb_serial_response_buffer()) != NULL);
            self.res_len = 0;

            c = *self.cmd;
            if(c == 'q') {
                PT_WAIT_THREAD(&self.pt_server, gdb_server_cmd_q());
            } else if(c == 'Q') {
                PT_WAIT_THREAD(&self.pt_server, gdb_server_cmd_Q());
            } else if(c == 'H') {
                PT_WAIT_THREAD(&self.pt_server, gdb_server_cmd_H());
            } else if(c == '?') {
                PT_WAIT_THREAD(&self.pt_server, gdb_server_cmd_question_mark());
            } else if(c == 'g') {
                PT_WAIT_THREAD(&self.pt_server, gdb_server_cmd_g());
            } else if(c == 'k') {
                PT_WAIT_THREAD(&self.pt_server, gdb_server_cmd_k());
            } else if(c == 'c') {
                PT_WAIT_THREAD(&self.pt_server, gdb_server_cmd_c());
            } else if(c == 'm') {
                PT_WAIT_THREAD(&self.pt_server, gdb_server_cmd_m());
            } else {
                gdb_server_reply_empty();
            }

            gdb_serial_command_done();
        }
    }

    PT_END(&self.pt_server);
}


const char qSupported_res[] = "PacketSize=400;QStartNoAckMode+";


/*
 * ‘q name params...’
 * General query (‘q’) and set (‘Q’).
 */
PT_THREAD(gdb_server_cmd_q(void))
{
    PT_BEGIN(&self.pt_cmd);

    if(strncmp(self.cmd, "qSupported:", 11) == 0){
        gdb_server_connected();
        strncpy(self.res, qSupported_res, GDB_SERIAL_RESPONSE_BUFFER_SIZE);
        gdb_serial_response_done(strlen(qSupported_res), GDB_SERIAL_SEND_FLAG_ALL);
    } else {
        gdb_server_reply_empty();
    }

    PT_END(&self.pt_cmd);
}


/*
 * ‘Q name params...’
 * General query (‘q’) and set (‘Q’).
 */
PT_THREAD(gdb_server_cmd_Q(void))
{
    PT_BEGIN(&self.pt_cmd);

    if(strncmp(self.cmd, "QStartNoAckMode", 15) == 0){
        gdb_server_reply_ok();
        gdb_serial_no_ack_mode(true);
    } else {
        gdb_server_reply_empty();
    }

    PT_END(&self.pt_cmd);
}


/*
 * ‘H op thread-id’
 * Set thread for subsequent operations (‘m’, ‘M’, ‘g’, ‘G’, et.al.).
 */
PT_THREAD(gdb_server_cmd_H(void))
{
    unsigned int n;
    char cmd;
    gdb_server_tid_t tid;

    PT_BEGIN(&self.pt_cmd);

    cmd = self.cmd[1];
    if(cmd == 'g' || cmd == 'G' || cmd == 'm' || cmd == 'M' || cmd == 'c'){
        sscanf(&self.cmd[2], "%x", &n);
        gdb_server_reply_ok();

        tid = (gdb_server_tid_t)n;
        if(cmd == 'g') {
            self.tid_g = tid;
        } else if(cmd == 'G') {
            self.tid_G = tid;
        } else if(cmd == 'm') {
            self.tid_m = tid;
        } else if(cmd == 'M'){
            self.tid_M = tid;
        } else {
            self.tid_c = tid;
        }
    } else {
        gdb_server_reply_empty();
    }

    PT_END(&self.pt_cmd);
}


/*
 * ‘?’
 * Indicate the reason the target halted. The reply is the same as for step and continue.
 */
PT_THREAD(gdb_server_cmd_question_mark(void))
{
    PT_BEGIN(&self.pt_cmd);

    self.target_pc = (uint32_t)(&gdb_server_cmd_question_mark);

    strncpy(self.res, "S02", GDB_SERIAL_RESPONSE_BUFFER_SIZE);
    gdb_serial_response_done(3, GDB_SERIAL_SEND_FLAG_ALL);

    PT_END(&self.pt_cmd);
}


/*
 * ‘g’
 * Read general registers.
 */
PT_THREAD(gdb_server_cmd_g(void))
{
    int i;

    PT_BEGIN(&self.pt_cmd);

    PT_WAIT_THREAD(&self.pt_cmd, rvl_target_read_registers(&self.regs[0]));

    for(i = 0; i < RVL_TARGET_REG_NUM; i++) {
        word_to_hex_le(self.regs[i], &self.res[i * (RVL_TARGET_REG_WIDTH / 8 * 2)]);
    }

    gdb_serial_response_done(RVL_TARGET_REG_WIDTH / 8 * 2 * RVL_TARGET_REG_NUM, GDB_SERIAL_SEND_FLAG_ALL);

    PT_END(&self.pt_cmd);
}


/*
 * ‘m addr,length’
 * Read length addressable memory units starting at address addr.
 * Note that addr may not be aligned to any particular boundary.
 */
PT_THREAD(gdb_server_cmd_m(void))
{
    char *p;

    PT_BEGIN(&self.pt_cmd);

    p = strchr(&self.cmd[1], ',');
    p++;
#if RVL_TARGET_ADDR_WIDTH == 32
    sscanf(&self.cmd[1], "%x", (unsigned int*)(&self.mem_addr));
#else
#error
#endif
    sscanf(p, "%x", (unsigned int*)(&self.mem_len));
    if(self.mem_len > sizeof(self.mem_buffer)) {
        self.mem_len = sizeof(self.mem_buffer);
    }

    PT_WAIT_THREAD(&self.pt_cmd, rvl_target_read_memory(self.mem_buffer, self.mem_addr, self.mem_len));

    bin_to_hex(self.mem_buffer, self.res, self.mem_len);
    gdb_serial_response_done(self.mem_len * 2, GDB_SERIAL_SEND_FLAG_ALL);

    PT_END(&self.pt_cmd);
}


/*
 * ‘k’
 * Kill request.
 */
PT_THREAD(gdb_server_cmd_k(void))
{
    PT_BEGIN(&self.pt_cmd);

    gdb_server_reply_ok();
    gdb_server_disconnected();

    PT_END(&self.pt_cmd);
}


/*
 * ‘c [addr]’
 * Continue at addr, which is the address to resume. If addr is omitted, resume
 * at current address.
 */
PT_THREAD(gdb_server_cmd_c(void))
{
    PT_BEGIN(&self.pt_cmd);

    self.continue_start = read_csr(mcycle);
    self.target_running = true;

    PT_END(&self.pt_cmd);
}


/*
 * Ctrl+C
 */
PT_THREAD(gdb_server_cmd_ctrl_c(void))
{
    PT_BEGIN(&self.pt_cmd);

    // TODO halt target
    self.target_running = false;
    self.target_pc = (uint32_t)(&gdb_server_cmd_ctrl_c);

    PT_END(&self.pt_cmd);
}


PT_THREAD(gdb_server_check_target(void))
{
    uint32_t end;

    PT_BEGIN(&self.pt_cmd);

    // TODO check target halt status
    end = read_csr(mcycle);
    if(end - self.continue_start > 96000000U * 30) {
        self.target_running = false;
        self.target_pc = (uint32_t)(&gdb_server_check_target);
    }

    if(!self.target_running) {
        strncpy(self.res, "S02", GDB_SERIAL_RESPONSE_BUFFER_SIZE);
        gdb_serial_response_done(3, GDB_SERIAL_SEND_FLAG_ALL);
    }

    PT_END(&self.pt_cmd);
}


static void gdb_server_reply_ok(void)
{
    strncpy(self.res, "OK", GDB_SERIAL_RESPONSE_BUFFER_SIZE);
    gdb_serial_response_done(2, GDB_SERIAL_SEND_FLAG_ALL);
}


static void gdb_server_reply_empty(void)
{
    gdb_serial_response_done(0, GDB_SERIAL_SEND_FLAG_ALL);
}


static void gdb_server_connected(void)
{
    gdb_serial_no_ack_mode(false);
    rvl_led_gdb_connect(1);


    // TODO init jtag, halt target
    self.target_pc = (uint32_t)(&gdb_server_connected);
    self.target_running = false;
    self.gdb_connected = true;
}


static void gdb_server_disconnected(void)
{
    rvl_led_gdb_connect(0);

    // TODO resume target, fini jtag

    self.target_running = true;
    self.gdb_connected = false;
}


static void bin_to_hex(const uint8_t *bin, char *hex, size_t nbyte)
{
    size_t i;
    uint8_t hi;
    uint8_t lo;

    for(i = 0; i < nbyte; i++) {
        hi = (*bin >> 4) & 0xf;
        lo = *bin & 0xf;

        if(hi < 10) {
            *hex = '0' + hi;
        } else {
            *hex = 'a' + hi - 10;
        }

        hex++;

        if(lo < 10) {
            *hex = '0' + lo;
        } else {
            *hex = 'a' + lo - 10;
        }

        hex++;
        bin++;
    }
}


static void hex_to_bin(const char *hex, uint8_t *bin, size_t nbyte)
{
    size_t i;
    uint8_t hi, lo;
    for(i = 0; i < nbyte; i++) {
        if(hex[i * 2] <= '9') {
            hi = hex[i * 2] - '0';
        } else if(hex[i * 2] <= 'F') {
            hi = hex[i * 2] - 'A' + 10;
        } else {
            hi = hex[i * 2] - 'a' + 10;
        }

        if(hex[i * 2 + 1] <= '9') {
            lo = hex[i * 2 + 1] - '0';
        } else if(hex[i * 2 + 1] <= 'F') {
            lo = hex[i * 2 + 1] - 'A' + 10;
        } else {
            lo = hex[i * 2 + 1] - 'a' + 10;
        }

        bin[i] = (hi << 4) | lo;
    }
}


static void word_to_hex_le(uint32_t word, char *hex)
{
    uint8_t bytes[4];

    bytes[0] = word & 0xff;
    bytes[1] = (word >> 8) & 0xff;
    bytes[2] = (word >> 16) & 0xff;
    bytes[3] = (word >> 24) & 0xff;

    bin_to_hex(bytes, hex, 4);
}


static void hex_to_word_le(const char *hex, uint32_t *word)
{
    uint8_t bytes[4];

    hex_to_bin(hex, bytes, 4);

    *word = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
}
