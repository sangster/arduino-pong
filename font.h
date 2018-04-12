#ifndef IMG2PCD8544_FONT_H
#define IMG2PCD8544_FONT_H
/* @file
 *
 * Arduino Pong is a Pong clone written for the Arduino Uno (or similar).
 * Copyright (C) 2018  Jon Sangster
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <avr/pgmspace.h>
#include <sangster/pcd8544/text.h>

/// Automatically generated from "scoreboard-font.png" with img2pcd8544
const PROGMEM uint8_t FONT_CHARS[] = {
    //  ####
    // #    #
    // #   ##
    // #  # #
    // # #  #
    // ##   #
    //  ####
    //
    0x3e, 0x61, 0x51, 0x49, 0x45, 0x3e,  // '0'

    //   #
    //  ##
    // # #
    //   #
    //   #
    //   #
    // #####
    //
    0x44, 0x42, 0x7f, 0x40, 0x40, 0x00,  // '1'

    //  ####
    // #    #
    //      #
    //    ##
    //  ##
    // #    #
    // ######
    //
    0x62, 0x51, 0x51, 0x49, 0x49, 0x66,  // '2'

    //  ####
    // #    #
    //      #
    //   ###
    //      #
    // #    #
    //  ####
    //
    0x22, 0x41, 0x49, 0x49, 0x49, 0x36,  // '3'

    //    #
    //   ##
    //  # #
    // #  #
    // ######
    //    #
    //   ###
    //
    0x18, 0x14, 0x52, 0x7f, 0x50, 0x10,  // '4'

    // ######
    // #
    // #####
    //      #
    //      #
    // #    #
    //  ####
    //
    0x27, 0x45, 0x45, 0x45, 0x45, 0x39,  // '5'

    //   ###
    //  #
    // #
    // #####
    // #    #
    // #    #
    //  ####
    //
    0x3c, 0x4a, 0x49, 0x49, 0x49, 0x30,  // '6'

    // ######
    // #    #
    //     #
    //    #
    //   #
    //   #
    //   #
    //
    0x03, 0x01, 0x71, 0x09, 0x05, 0x03,  // '7'

    //  ####
    // #    #
    // #    #
    //  ####
    // #    #
    // #    #
    //  ####
    //
    0x36, 0x49, 0x49, 0x49, 0x49, 0x36,  // '8'

    //  ####
    // #    #
    // #    #
    //  #####
    //      #
    //     #
    //  ###
    //
    0x06, 0x49, 0x49, 0x49, 0x29, 0x1e,  // '9'
};

const PcdFont FONT = {
    .chars   = FONT_CHARS,
    .width  = 6,
    .first  = '0',
    .count  = 10,
};
#endif//IMG2PCD8544_FONT_H
