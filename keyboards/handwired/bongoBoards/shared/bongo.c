/* Copyright 2025 Sam Kelly (@samkellu)
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

#include "bongo.h"

const struct frame_set* frame_sets[] = {no_caps, caps};

uint8_t curr_frame_index = 0;
uint8_t frame_set_index = 0;

uint8_t curr_wpm = 0;
int frame_time = 0;

void bongo_update(void) {
    led_t led_usb_state = host_keyboard_led_state();
    curr_wpm = get_current_wpm();
    render_wpm(led_usb_state.caps_lock);
    render_bongocat(led_usb_state.caps_lock);
}

void render_wpm(bool caps_lock) {
    // Writes the WPM to the screen, and caps if enabled
    oled_set_cursor(0, 7);
    oled_write_P(PSTR("WPM:"), false);
    oled_write(get_u8_str(curr_wpm, ' '), false);
    oled_set_cursor(17, 0);
    if (caps_lock) {
        oled_write_P(PSTR("CAPS"), false);
        
    } else {
        oled_write_P(PSTR("    "), false);
    }
}

void render_bongocat(bool caps_lock) {
    
    oled_set_cursor(0, 0);
    // Allows for frameset to change to caps state without waiting for end of frame set
    struct frame_set curr_frame_set = frame_sets[caps_lock][frame_set_index];
    
    // Updates frame being displayed based on set interval
    if (timer_elapsed(frame_time) > curr_frame_set.frame_len) {
        
        frame_time = timer_read();
        
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