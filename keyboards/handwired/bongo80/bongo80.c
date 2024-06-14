/* Copyright 2023 Sam Kelly (@samkellu)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "bongo80.h"
#include "doom.h"

#ifdef OLED_ENABLE


uint8_t curr_frame_index = 0;
uint8_t frame_set_index = 0;

static uint8_t curr_wpm = 0;
led_t led_usb_state;
oled_state screen_mode = OFF;
uint32_t time = 0;

static void render_wpm(void) {

    // Writes the WPM to the screen, and caps if enabled
    oled_set_cursor(0, 7);
    oled_write_P(PSTR("WPM:"), false);
    oled_write(get_u8_str(curr_wpm, ' '), false);
    oled_set_cursor(17, 0);
    if (led_usb_state.caps_lock) {
        oled_write_P(PSTR("CAPS"), false);

    } else {
        oled_write_P(PSTR("    "), false);
    }
}

static void render_bongocat(void) {

    oled_set_cursor(0, 0);
    // Allows for frameset to change to caps state without waiting for end of frame set
    struct frame_set curr_frame_set = frame_sets[led_usb_state.caps_lock][frame_set_index];

    // Updates frame being displayed based on set interval
    if (timer_elapsed(time) > curr_frame_set.frame_len) {

        time = timer_read();

        // Ensures a smoothly animated transition between the different states of the animation
        if (curr_frame_index == curr_frame_set.size - 1) {

            // Assigns frameset based on typing speed
            if (curr_wpm <= IDLE_UPPER_BOUND) {
                frame_set_index = IDLE;

            } else if (curr_wpm <= SLOW_UPPER_BOUND) {
                frame_set_index = SLOW;

            } else if (curr_wpm <= MED_UPPER_BOUND) {
                frame_set_index = MED;

            } else {
                frame_set_index = FAST;
            }

            curr_frame_index = 0;

        } else {
            curr_frame_index++;
        }

        oled_write_raw_P(curr_frame_set.frames[curr_frame_index], frame_size);
    }
}

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    
    if (!process_record_user(keycode, record)) {
        return false;
    }

    switch (keycode) {
        // Handles the keycode for turning on and off the oled screen
        case KC_OLED_STATE:
            if (record->event.pressed) {
                switch (screen_mode) {
                    case OFF:
                        screen_mode = CAT;
                        break;

                    case CAT:
                        screen_mode = DOOM;
                        break;

                    case DOOM:
                        screen_mode = OFF;
                        break
                }

                oled_clear();
            }

            return false;
    }
    return true;
}

bool oled_task_kb(void) {

    if (!oled_task_user()) return false;

    switch (screen_mode) {
        case CAT:
            curr_wpm = get_current_wpm();
            led_usb_state = host_keyboard_led_state();
            render_wpm();
            render_bongocat();
            break;

        case DOOM:
            
            break
    }

    return false;
}

#endif