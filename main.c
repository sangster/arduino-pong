/*
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
/**
 * @file
 *
 * A simple game of Pong, using potentiometers as joysticks.
 *
 * MAIN COMPONENTS
 *   1. Arduino Uno (Rev.3)
 *   2. Nokia 5110 LCD
 *   3. Rotary Potentiometer (x3)
 *   4. Piezo Speaker
 */
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <sangster/pcd8544.h>
#include <sangster/pinout.h>
#include <sangster/util.h>

#include "logo.h"
#include "font.h"


/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define ADC_MAX          1024 ///< Max pot. reading
/* #define ADC_MAX        675 ///< If AVREF == 5v --> (3.3v / 5v * 1024) */
#define SENSOR_CLKS      2080 ///< `0.133 s == (2080 * 1024) / 16e6`

/// The distance of each paddle from the net
#define PONG_PADDLE_DIST  (PCD_COLS / 2 - PONG_PADDLE_W)
#define PONG_CX           (PCD_COLS / 2) ///< Col marking the court's center
#define PONG_CY           (PCD_ROWS / 2) ///< Row marking the court's center
#define PONG_SPLASH_MS    4000 ///< Time to show the splash screen, in ms.

#define PONG_PADDLE_W     2 ///< Paddle width
#define PONG_PADDLE_H     8 ///< Paddle height
#define PONG_NET_DASH     3 ///< The net's dash-length
#define PONG_SCORE_DIST   4 ///< Space between the net and each scoreboard
#define PONG_BALL_RADIUS  2 ///< The radius of the game ball
#define PONG_BALL_SPEED   3 ///< The base-speed of the ball
#define PONG_ACCEL_DIV    5 ///< How many vollies between speed increases

#define PONG_BALL_CX_MIN  (PONG_BALL_RADIUS - 1)
#define PONG_BALL_CX_MAX  (PCD_COLS - PONG_BALL_RADIUS)
#define PONG_BALL_CY_MIN  (PONG_BALL_RADIUS - 1)
#define PONG_BALL_CY_MAX  (PCD_ROWS - PONG_BALL_RADIUS)

#define PONG_TOUCH_DELAY_MS   10 ///< Length of the "tap" sound-effect, in ms
#define PONG_GOAL_DELAY_MS  1000 ///< Length of the goal sound-effect, in ms

/// The col representnig where Player 1 can touch the ball
#define PONG_PADDLE_SURF_1  (PONG_CX - PONG_PADDLE_DIST + PONG_BALL_RADIUS)
/// The col representnig where Player 2 can touch the ball
#define PONG_PADDLE_SURF_2  (PONG_CX + PONG_PADDLE_DIST - PONG_BALL_RADIUS)


/*******************************************************************************
 * Types
 ******************************************************************************/
/// The names of our champions
enum pong_player
{
    PLAYER_1,
    PLAYER_2,
};
typedef enum pong_player PongPlayer;

/// The direction a paddle has moved, between the previous and current frames
enum pong_paddle_dir
{
    PONG_PADDLE_DOWN,
    PONG_PADDLE_STOPPED,
    PONG_PADDLE_UP,
};
typedef enum pong_paddle_dir PongPaddleDir;

/// The game state
typedef struct pong_game PongGame;
struct pong_game
{
    PcdIdx ball_x; ///< The col the ball is currently in
    PcdIdx ball_y; ///< The row the ball is currently in

    uint16_t ball_accel; ///< Additonal speed added to the ball, for Difficulty

    float ball_dx; ///< The ball's change-in-col over time
    float ball_dy; ///< The ball's change-in-row over time

    PcdIdx player_1_y; ///< The row of the top of Player 1's paddle
    PcdIdx player_2_y; ///< The row of the top of Player 2's paddle

    PcdIdx player_1_y_prev; ///< Paddle location in the previous frame
    PcdIdx player_2_y_prev; ///< Paddle location in the previous frame

    uint8_t score_player_1; ///< Player 1's current score
    uint8_t score_player_2; ///< Player 2's current score
};


/*******************************************************************************
 * Function Declarations
 ******************************************************************************/
void setup();
void update_srand();
int8_t random_vector();

PcdIdx read_paddle_top(PongPlayer);
void read_paddle_positions();
PongPaddleDir pong_paddle_dir(PongPlayer);
uint16_t read_pot_player1();
uint16_t read_pot_player2();
uint16_t read_pot_contrast();

void draw_screen();
void draw_score(PongPlayer player);
void draw_paddle(PongPlayer);
void draw_net();
void draw_ball();
void set_contrast(uint16_t);

void ball_check_contact();
bool ball_is_touching(PongPlayer);
void ball_advance();
void ball_serve();
void ball_bounce_x();
void ball_bounce_y();
void ball_spin(PongPaddleDir);

void buzzer_blocking_touch();
void buzzer_blocking_goal();


/*******************************************************************************
 * Global Variables
 ******************************************************************************/
const Pinout pin_pot_contrast = PIN_DEF_ARDUINO_A0; ///< Constast dial
const Pinout pin_pot_player1  = PIN_DEF_ARDUINO_A1; ///< Paddle 1
const Pinout pin_pot_player2  = PIN_DEF_ARDUINO_A2; ///< Paddle 2
const Pinout pin_buzzer       = PIN_DEF_ARDUINO_3;  ///< Sound card

PongGame game; ///< The game state
PcdDraw  draw; ///< Interface for basical drawing primatives
PcdTrans tr;   ///< Transaction manager, to draw multiple things at once

/// The LCD screen's pins and state
Pcd screen = {
    .pin_led  = PIN_DEF_ARDUINO_8,
    .pin_sce_ = PIN_DEF_ARDUINO_7,
    .pin_res_ = PIN_DEF_ARDUINO_6,
    .pin_dc   = PIN_DEF_ARDUINO_5,
    .pin_sdin = PIN_DEF_ATMEGA328P_MOSI,
    .pin_sclk = PIN_DEF_ARDUINO_10,
};

volatile bool advance_frame  = false; ///< If the LCD should be repainted
         bool is_goal_scored = false; ///< Did someone just score?


/*******************************************************************************
 * Interrupts
 ******************************************************************************/
ISR(TIMER1_COMPA_vect)
{
    advance_frame = true;
}

ISR(TIMER2_COMPA_vect)
{
    pinout_toggle(pin_buzzer);
}


/// Main OS function
__attribute__((OS_main))
int main(void)
{
    setup();
    ball_serve();

    // Show Splash Image
    set_contrast(read_pot_contrast());
    pcd_bmp_draw_center(&draw, &LOGO);
    _delay_ms(PONG_SPLASH_MS);

    for (;;) {
        set_contrast(read_pot_contrast());

        if (advance_frame) {
            advance_frame = false;

            read_paddle_positions();
            ball_advance();
            ball_check_contact();
            draw_screen();

            if (is_goal_scored) {
                buzzer_blocking_goal();
                ball_serve();
                is_goal_scored = false;
            }
        }
    }
}


/*******************************************************************************
 * Function Definitions
 ******************************************************************************/
void setup()
{
    pinout_make_input(pin_pot_contrast);
    pinout_make_input(pin_pot_player1);
    pinout_make_input(pin_pot_player2);
    pinout_make_output(pin_buzzer);

    // Start update timer
    OCR1A = SENSOR_CLKS;
    TCCR1B |= _BV(WGM12) | _BV(CS12) | _BV(CS10); // CTC mode, 1024 prescaler
    TIMSK1 |= _BV(OCIE1A);

    // Setup buzzer
    TCCR2A |= _BV(WGM21); // CTC mode
    TCCR2B = _BV(CS22) | _BV(CS20); // 128 prescaler

    // Setup LCD screen
    pcd_setup(&screen);
    pcd_clr_all(&screen);
    pcd_draw_init(&draw, &screen);

    // Potentiometers: contrast dial and two "controllers"
    ADMUX |= _BV(REFS0);
    ADCSRA |= _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0); // 16 MHz / 128 == 125 kHz
    ADCSRA |= _BV(ADEN);

    game.score_player_1 = 0;
    game.score_player_2 = 0;
    sei();
}


/// Seed the PRNG with (hopefully) difficult to predict values
void update_srand()
{
    srandom(read_pot_contrast() + read_pot_player1() + read_pot_player2()
            + game.score_player_1 + game.score_player_2 + game.ball_accel
            + game.player_1_y + game.player_1_y_prev
            + game.player_2_y + game.player_2_y_prev);
}


/// A random speed (1...PONG_BALL_SPEED, inclusive) with a random direction
int8_t random_vector()
{
    return ((random() % 2) ? +1 : -1) * ((random() % PONG_BALL_SPEED) + 1);
}

/**
 * @param  player The player to query
 * @return Row of the top of `player`'s paddle
 */
PcdIdx read_paddle_top(const PongPlayer player)
{
    uint16_t in;
    if (player == PLAYER_1) {
        in = ADC_MAX - read_pot_player1(); // reversed dir: opp. side of screen
    } else {
        in = read_pot_player2();
    }
    if (in > ADC_MAX) {
        in = ADC_MAX;
    }
   return map_u16(in, 0, ADC_MAX, 0, PCD_ROWS - PONG_PADDLE_H);
}


/// Read the players' controllers and update the game stage
void read_paddle_positions()
{
    game.player_1_y_prev = game.player_1_y;
    game.player_2_y_prev = game.player_2_y;

    game.player_1_y = read_paddle_top(PLAYER_1);
    game.player_2_y = read_paddle_top(PLAYER_2);
}


/**
 * @param  player The player to query
 * @return The direction the player's paddle is currently moving
 */
PongPaddleDir pong_paddle_dir(const PongPlayer player)
{
    PcdIdx diff;
    if (player == PLAYER_1) {
        diff = game.player_1_y - game.player_1_y_prev;
    } else {
        diff = game.player_2_y - game.player_2_y_prev;
    }
    switch(diff > 0 ? 1 : (diff < 0 ? -1 : 0)) {
        case -1: return PONG_PADDLE_DOWN;
        case +1: return PONG_PADDLE_UP;
        default: return PONG_PADDLE_STOPPED;
    }
}


uint16_t read_pot_player1()
{
    ADMUX = (ADMUX & 0xF0) | _BV(MUX0); // select ADC1
    ADCSRA |= _BV(ADSC); // start comparision
    loop_until_bit_is_clear(ADCSRA, ADSC);
    return ADC;
}


uint16_t read_pot_player2()
{
    ADMUX = (ADMUX & 0xF0) | _BV(MUX1); // select ADC2
    ADCSRA |= _BV(ADSC); // start comparision
    loop_until_bit_is_clear(ADCSRA, ADSC);
    return ADC;
}


uint16_t read_pot_contrast()
{
    ADMUX = ADMUX & 0xF0; // select ADC0
    ADCSRA |= _BV(ADSC); // start comparision
    loop_until_bit_is_clear(ADCSRA, ADSC);
    return ADC;
}


void draw_screen()
{
    pcd_trans_start(&tr, &draw);

    pcd_fill(&draw, PCD_WHITE);

    draw_score(PLAYER_1);
    draw_score(PLAYER_2);

    draw_net();
    draw_ball();

    draw_paddle(PLAYER_1);
    draw_paddle(PLAYER_2);

    pcd_display(&screen, is_goal_scored ? PCD_INVERSE_VIDEO : PCD_NORMAL_MODE);

    pcd_trans_commit(&tr);
}


void draw_score(PongPlayer player)
{
    char str[4];
    PcdIdx x;

    if (player == PLAYER_1) {
        utoa(game.score_player_1, str, 10);
        x = PONG_CX + 1 - PONG_SCORE_DIST - pcd_text_width(FONT, strlen(str));
    } else {
        utoa(game.score_player_2, str, 10);
        x = PONG_CX + PONG_SCORE_DIST;
    }

    pcd_print(&draw, &FONT, x, 0, str, PCD_BLACK);
}


void draw_paddle(PongPlayer player)
{
    PcdIdx x1, x2, y;

    if (player == PLAYER_1) {
        x1 = PONG_CX - PONG_PADDLE_DIST - PONG_PADDLE_W;
        y = game.player_1_y;
    } else {
        x1 = PONG_CX + PONG_PADDLE_DIST;
        y = game.player_2_y;
    }
    x2 = x1 + PONG_PADDLE_W - 1;

    pcd_rect(&draw, x1, y, x2, y + PONG_PADDLE_H - 1, PCD_BLACK);
}


/// This is just decoration and doesn't affect gameplay.
void draw_net()
{
    for (PcdIdx y = 0; y < PCD_ROWS; y += PONG_NET_DASH) {
        pcd_xy(&draw, PONG_CX, y, PCD_BLACK);
    }
}


void draw_ball()
{
    pcd_circ(&draw, game.ball_x, game.ball_y, PONG_BALL_RADIUS, PCD_BLACK);
}


void set_contrast(const uint16_t contrast)
{
    pcd_op_voltage(&screen,
                   map_u16(ADC_MAX - contrast, 0, ADC_MAX, 0,
                           PCD_MAX_OP_VOLTAGE - PCD_MAX_OP_VOLTAGE / 3));
}


/// Handle if the ball is touching either a paddle or a goal line
void ball_check_contact()
{
    if (ball_is_touching(PLAYER_1)) {
        ball_bounce_x();
        ball_spin(pong_paddle_dir(PLAYER_1));
    } else if (ball_is_touching(PLAYER_2)) {
        ball_bounce_x();
        ball_spin(pong_paddle_dir(PLAYER_2));
    } else if (game.ball_x <= PONG_BALL_CX_MIN) {
        ++game.score_player_2;
        is_goal_scored = true;
    } else if (game.ball_x >= PONG_BALL_CX_MAX) {
        ++game.score_player_1;
        is_goal_scored = true;
    }
}


/// @return If the ball is touching the given player's paddle
bool ball_is_touching(PongPlayer player)
{
    PcdIdx y;
    if (player == PLAYER_1 && game.ball_dx < 0
            && game.ball_x <= PONG_PADDLE_SURF_1) {
        y = game.player_1_y;
    } else if (player == PLAYER_2 && game.ball_dx > 0
            && game.ball_x >= PONG_PADDLE_SURF_2) {
        y = game.player_2_y;
    } else {
        return false;
    }
    return game.ball_y >= y && game.ball_y <= y + PONG_PADDLE_H;
}


/**
 * Advance the ball from its previous location to its current location
 *
 * @note This function is also responsible for bouncing the ball off the top or
 *    bottom of the screen
 */
void ball_advance()
{
    uint8_t accel = game.ball_accel / PONG_ACCEL_DIV;

    const PcdIdx dx = game.ball_dx + accel * (game.ball_dx < 0 ? -1 : +1);
    const PcdIdx dy = game.ball_dy + accel * (game.ball_dy < 0 ? -1 : +1);

    game.ball_x += dx != 0 ? dx : random_vector();
    game.ball_y += dy;

    if (game.ball_x <= PONG_BALL_CX_MIN || game.ball_x >= PONG_BALL_CX_MAX) {
        game.ball_x = game.ball_dx < 0 ? PONG_BALL_CX_MIN : PONG_BALL_CX_MAX;
    }
    if (game.ball_y <= PONG_BALL_CY_MIN || game.ball_y >= PONG_BALL_CY_MAX) {
        game.ball_y = game.ball_dy < 0 ? PONG_BALL_CY_MIN : PONG_BALL_CY_MAX;
        ball_bounce_y();
    }
}


/**
 * Move ball back to the center of the court and start a new rally
 *
 * The player with the higest score serves the ball at a random angle and speed.
 * If the players are tied, the direction is random too.
 */
void ball_serve()
{
    update_srand();

    game.ball_x = PONG_CX;
    game.ball_y = PONG_CY;

    game.ball_dx = random_vector();
    game.ball_dy = random_vector();

    // The current winner serves
    if (game.score_player_1 > game.score_player_2) {
        if (game.ball_dx < 0) {
            game.ball_dx *= -1;
        }
    } else if (game.score_player_2 > game.score_player_1) {
        if (game.ball_dx > 0) {
            game.ball_dx *= -1;
        }
    }

    game.ball_accel = 0;
}


/// Switch the horizontal direction of the ball and accelerate the ball
void ball_bounce_x()
{
    game.ball_dx *= -1;
    ++game.ball_accel;
    buzzer_blocking_touch();
}


/// Switch the vertical direction of the ball
void ball_bounce_y()
{
    game.ball_dy *= -1;
    buzzer_blocking_touch();
}


/**
 * Add/remove spin from the ball
 *
 * If the player is moving their paddle in the same direction as the ball,
 * increase the vertical spee of the ball. If they are moving their paddle in
 * the opposite direction, reduce it.
 */
void ball_spin(PongPaddleDir dir)
{
    switch (dir) {
        case PONG_PADDLE_DOWN:
            if (game.ball_dy == 0) {
                game.ball_dy = -1;
            } else {
                game.ball_dy *= game.ball_dy > 0 ? 0.5f : 1.5f;
            }
            break;
        case PONG_PADDLE_UP:
            if (game.ball_dy == 0) {
                game.ball_dy = +1;
            } else {
                game.ball_dy *= game.ball_dy < 0 ? 0.5f : 1.5f;
            }
            break;
        case PONG_PADDLE_STOPPED:
            break;
    }
}


/// Play a tone to indicate the ball touched a wall or paddle
void buzzer_blocking_touch()
{
    OCR2A = F_CPU / 350 / 2 / 128 - 1;
    TIMSK2 |= _BV(OCIE2A);
    _delay_ms(PONG_TOUCH_DELAY_MS);
    TIMSK2 &= ~_BV(OCIE2A);
    pinout_clr(pin_buzzer);
}


/// Play a tone to indicate that one of the players has scored
void buzzer_blocking_goal()
{
    OCR2A = F_CPU / 350 / 2 / 128 - 1;
    TIMSK2 |= _BV(OCIE2A);
    _delay_ms(PONG_GOAL_DELAY_MS);
    TIMSK2 &= ~_BV(OCIE2A);
    pinout_clr(pin_buzzer);
}
