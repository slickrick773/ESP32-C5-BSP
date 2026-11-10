// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stdint.h>

// Adapter must release high (open drain), never drive high. No scheduler,
// allocation, static state, peripheral driver, or unbounded waits.
// One transaction has at most 2 ms of stretching plus ~0.5 ms clocking.
// SDA-low on entry is treated as busy; do not clock another owner's transfer.
typedef struct {
    void (*sda)(bool released);
    void (*scl)(bool released);
    bool (*read_sda)(void);
    bool (*read_scl)(void);
    void (*delay_us)(uint32_t us);
} early_i2c_io;

enum early_i2c_result { EARLY_OK, EARLY_BUSY, EARLY_TIMEOUT, EARLY_NACK };

static bool early_raise_scl(const early_i2c_io *io, unsigned *budget) {
    io->scl(true);
    while (!io->read_scl()) {
        if (!*budget) return false;
        --*budget;
        io->delay_us(5);
    }
    io->delay_us(5);
    return true;
}

static enum early_i2c_result early_pwm_off(const early_i2c_io *io) {
    unsigned budget = 400;
    enum early_i2c_result result = EARLY_OK;
    const uint8_t bytes[] = { 0x48, 0x05, 0x00 }; // 0x24 write, PWM, off
    io->sda(true);
    if (!early_raise_scl(io, &budget)) { result = EARLY_TIMEOUT; goto release; }
    if (!io->read_sda()) { result = EARLY_BUSY; goto release; }
    io->sda(false); // START
    io->delay_us(5);
    io->scl(false);
    for (unsigned byte = 0; byte < sizeof(bytes); ++byte) {
        for (unsigned bit = 0; bit < 8; ++bit) {
            io->sda((bytes[byte] & (0x80u >> bit)) != 0);
            io->delay_us(5);
            if (!early_raise_scl(io, &budget)) { result = EARLY_TIMEOUT; goto stop; }
            // Arbitration lost: release without STOP, which would disrupt owner.
            if ((bytes[byte] & (0x80u >> bit)) && !io->read_sda()) {
                result = EARLY_BUSY; goto release;
            }
            io->scl(false);
        }
        io->sda(true);
        io->delay_us(5);
        if (!early_raise_scl(io, &budget)) { result = EARLY_TIMEOUT; goto stop; }
        const bool nack = io->read_sda();
        io->scl(false);
        if (nack) { result = EARLY_NACK; goto stop; }
    }
stop:
    io->scl(false);
    io->sda(false);
    io->delay_us(5);
    if (!early_raise_scl(io, &budget)) result = EARLY_TIMEOUT;
    io->sda(true); // STOP only when SCL actually rose; otherwise just release
    io->delay_us(5);
release:
    io->sda(true);
    io->scl(true);
    return result;
}
