/*
 * Copyright (C) 2021 magicxqq <xqq@xqq.im>. All rights reserved.
 *
 * This file is part of libaribcaption.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
 
#ifndef ARIBCAPTION_B24_CONTROLSETS_HPP
#define ARIBCAPTION_B24_CONTROLSETS_HPP

#include <cstdint>

namespace aribcaption {

enum C0 : uint8_t {
    NUL = 0x00,
    BEL = 0x07,
    APB = 0x08,
    APF = 0x09,
    APD = 0x0A,
    APU = 0x0B,
    CS = 0x0C,
    APR = 0x0D,
    LS1 = 0x0E,
    LS0 = 0x0F,
    PAPF = 0x16,
    CAN = 0x18,
    SS2 = 0x19,
    ESC = 0x1B,
    APS = 0x1C,
    SS3 = 0x1D,
    RS = 0x1E,
    US = 0x1F,
    SP = 0x20
};

enum C1 : uint8_t {
    DEL = 0x7F,
    BKF = 0x80,
    RDF = 0x81,
    GRF = 0x82,
    YLF = 0x83,
    BLF = 0x84,
    MGF = 0x85,
    CNF = 0x86,
    WHF = 0x87,
    SSZ = 0x88,
    MSZ = 0x89,
    NSZ = 0x8A,
    SZX = 0x8B,
    COL = 0x90,
    FLC = 0x91,
    CDC = 0x92,
    POL = 0x93,
    WMM = 0x94,
    MACRO = 0x95,
    HLC = 0x97,
    RPC = 0x98,
    SPL = 0x99,
    STL = 0x9A,
    CSI = 0x9B,
    TIME = 0x9D
};

enum ESC : uint8_t {
    LS2 = 0x6E,
    LS3 = 0x6F,
    LS1R = 0x7E,
    LS2R = 0x7D,
    LS3R = 0x7C
};

enum CSI : uint8_t {
    GSM = 0x42,
    SWF = 0x53,
    CCC = 0x54,
    SDF = 0x56,
    SSM = 0x57,
    SHS = 0x58,
    SVS = 0x59,
    PLD = 0x5B,
    PLU = 0x5C,
    GAA = 0x5D,
    SRC = 0x5E,
    SDP = 0x5F,
    ACPS = 0x61,
    TCC = 0x62,
    ORN = 0x63,
    MDF = 0x64,
    CFS = 0x65,
    XCS = 0x66,
    SCR = 0x67,
    PRA = 0x68,
    ACS = 0x69,
    UED = 0x6A,
    RCS = 0x6E,
    SCS = 0x6F
};


}  // namespace aribcaption

#endif  // ARIBCAPTION_B24_CONTROLSETS_HPP
