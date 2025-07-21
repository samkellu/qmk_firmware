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

#include "doom.h"
#include "math.h"

// Player information
vec2 p;
int pa;
int shot_timer;
bool has_key = false;

// Game management information
uint32_t game_time;
uint32_t last_frame;
uint8_t score;
bool initialized = false;

// Gun animation
int gun_x = GUN_X;
int gun_y = GUN_Y + 10;

// Level
segment* walls = NULL;
int num_walls = 0;

// =================== MATH =================== //

float dot(vec2 u, vec2 v) { return u.x * v.x + u.y * v.y; }

double cross(vec2 u, vec2 v) { return u.x * v.y - u.y * v.x; }

float dist2(vec2 u, vec2 v) { return dot(sub(u, v), sub(u, v)); }

vec2 sub(vec2 u, vec2 v) { return (vec2) {u.x - v.x, u.y - v.y}; }

vec2 add(vec2 u, vec2 v) { return (vec2) {u.x + v.x, u.y + v.y}; }

vec2 proj(vec2 u, vec2 v) { 

    float t = dot(u, v) / dot(v, v);
    return (vec2) {t * v.x, t * v.y};
}

float point_ray_dist2(vec2 p, segment s) {

    vec2 up = sub(p, s.u);
    vec2 uv = sub(s.v, s.u);
    vec2 p_proj = add(proj(up, uv), s.u);
    vec2 u_p_proj = sub(p_proj, s.u);

    float k = uv.x != 0 ? u_p_proj.x / uv.x : u_p_proj.y / uv.y;
    return k <= 0 ? dist2(p, s.u) : k >= 1 ? dist2(p, s.v) : dist2(p, p_proj);
}

// Returns the point of intersection between two line segments
vec2 raycast(segment s1, segment s2, bool* hit) {

    *hit = false;
    vec2 ret = {-1, -1};
    float denominator = (s1.u.x - s1.v.x) * (s2.u.y - s2.v.y) - (s1.u.y - s1.v.y) * (s2.u.x - s2.v.x);

    // vectors do not ever intersect
    if (denominator == 0) return ret;

    float t = ((s1.u.x - s2.u.x) * (s2.u.y - s2.v.y) - (s1.u.y - s2.u.y) * (s2.u.x - s2.v.x)) / denominator;
    float u = -((s1.u.x - s1.v.x) * (s1.u.y - s2.u.y) - (s1.u.y - s1.v.y) * (s1.u.x - s2.u.x)) / denominator;

    // Case where the vectors intersect
    if (t > 0 && t < 1 && u > 0) {
        *hit = true;
        ret = (vec2) { s1.u.x + t * (s1.v.x - s1.u.x), s1.u.y + t * (s1.v.y - s1.u.y) };
    }

    return ret;
}

// A classic https://en.wikipedia.org/wiki/Fast_inverse_square_root
float inv_sqrt(float num) {
    
    float x2 = num * 0.5f, y = num;
    uint32_t i;
    memcpy(&i, &y, sizeof(float));
    i = 0x5f3759df - ( i >> 1 );
    memcpy(&y, &i, sizeof(float));
    return y * (1.5f - ( x2 * y * y ));
}

bool collision_detection(vec2 v, bool is_enemy) {

    int collision_dist2 = WALL_COLLISION_DIST * WALL_COLLISION_DIST;
    for (int i = 0; i < num_walls; i++) {
        segment w = walls[i];
        float d2 = point_ray_dist2(v, w);
        if (d2 < collision_dist2) {
            if (i == 0 && score >= 5) {
                doom_setup();
            }

           return true;
        }
    }

    if (is_enemy) return false;

    for (int i = 0; i < NUM_ENEMIES; i++) {
        enemy e = enemies[i];
        collision_dist2 = e.width * e.width;
        float d2 = dist2(v, e.pos);
        if (d2 < collision_dist2) return true;
    }

    return false;
}


// =================== MAP GENERATION =================== //


segment* bsp_wallgen(segment* walls, int* num_walls, int l, int r, int t, int b, int depth) {
    if (depth == 0) {
        // Prevent walls being created in small cells
        if (r - MIN_ROOM_WIDTH <= l + MIN_ROOM_WIDTH || b - MIN_ROOM_WIDTH <= t + MIN_ROOM_WIDTH) return walls;
        // Prevent thin walls being created
        if (r - l - 2 * MIN_ROOM_WIDTH < 5 || b - t - 2 * MIN_ROOM_WIDTH < 5) return walls;

        walls = (segment*) realloc(walls, sizeof(segment) * (*num_walls + 4));
        walls[(*num_walls)++] = (segment) {
            {l + MIN_ROOM_WIDTH, b - MIN_ROOM_WIDTH},
            {l + MIN_ROOM_WIDTH, t + MIN_ROOM_WIDTH},
            CHECK
        };

        walls[(*num_walls)++] = (segment) {
            {l + MIN_ROOM_WIDTH, t + MIN_ROOM_WIDTH},
            {r - MIN_ROOM_WIDTH, t + MIN_ROOM_WIDTH},
            CHECK
        };

        walls[(*num_walls)++] = (segment) {
            {r - MIN_ROOM_WIDTH, t + MIN_ROOM_WIDTH},
            {r - MIN_ROOM_WIDTH, b - MIN_ROOM_WIDTH},
            CHECK
        };

        walls[(*num_walls)++] = (segment) {
            {r - MIN_ROOM_WIDTH, b - MIN_ROOM_WIDTH},
            {l + MIN_ROOM_WIDTH, b - MIN_ROOM_WIDTH},
            CHECK
        };

        return walls;
    }
    

    // Split on longest axis
    if (r - l >= b - t) {
        if (r - l < MIN_ROOM_WIDTH) return walls;
        
        int split = rand() % (r - l);
        if (split > MIN_ROOM_WIDTH) {
            walls = bsp_wallgen(walls, num_walls, l, l + split, t, b, depth - 1);
        }

        if (r - l - split > MIN_ROOM_WIDTH) {
            walls = bsp_wallgen(walls, num_walls, l + split, r, t, b, depth - 1);
        }

    } else {
        if (b-t < MIN_ROOM_WIDTH) return walls;

        int split = rand() % (t - b);
        if (split > MIN_ROOM_WIDTH) {
            walls = bsp_wallgen(walls, num_walls, l, r, t, t + split, depth - 1);
        }

        if (b - t - split > MIN_ROOM_WIDTH) {
            walls = bsp_wallgen(walls, num_walls, l, r, t + split, b, depth - 1);
        }
    }

    return walls;
}


// =================== BUFFER =================== //


// void write_pixel(int x, int y, bool white)
// {
//     if (x < 0 || x >= SCREEN_WIDTH) return;
//     if (y < 0 || y >= SCREEN_HEIGHT - UI_HEIGHT) return;

//     int bit_offset = x + (y * SCREEN_WIDTH);
//     if (white) {
//         frame_buffer[FRAME_BUFFER_LENGTH] &= ~(1 << (bit_offset % sizeof(char)));
//     } else {
//         frame_buffer[FRAME_BUFFER_LENGTH] |= 1 << (bit_offset % sizeof(char));
//     }
// }

// void clear_frame_buffer() {
//     memset(frame_buffer, 0, FRAME_BUFFER_LENGTH);
// }

// void print_frame_buffer() {
//     int idx = 0;
//     for (int y = 0; y < SCREEN_HEIGHT - UI_HEIGHT; y++)
//     {
//         for (int x = 0; x < SCREEN_WIDTH; x++)
//         {
//             int val = frame_buffer[idx / sizeof(char)] & 1 << (idx % sizeof(char));
//             printf("%d", val);
//             idx++;
//         }
//         printf("\n");
//     }
// }


// =================== GRAPHICS =================== //


// 2.5D raycast renderer for the map and entities around the player
void render_map(vec2 p, int pa, bool is_shooting) {

    // Get walls that intersect the FOV
    segment* relevant_walls = NULL;
    int num_relevant = 0;

    segment cone_l = {p, {0, 0}};
    float bound_angle = -FOV / 2;
    cone_l.v.x = p.x + DOV * cosf((pa + bound_angle) * PI / 180);
    cone_l.v.y = p.y + DOV * sinf((pa + bound_angle) * PI / 180);

    segment cone_r = {p, {0, 0}};
    bound_angle = FOV / 2;
    cone_r.v.x = p.x + DOV * cosf((pa + bound_angle) * PI / 180);
    cone_r.v.y = p.y + DOV * sinf((pa + bound_angle) * PI / 180);

    float middle_ray = (SCREEN_WIDTH / 2) * (FOV / 127.0f) - (FOV / 2.0f);
    vec2 mid_vec = {
        DOV * cosf((pa + middle_ray) * PI / 180),
        DOV * sinf((pa + middle_ray) * PI / 180)
    };

    for (int i = 0; i < num_walls; i++) {
        segment w = walls[i];
        bool hit = false;

        // Check if intersects the bounds of the fov cone
        raycast(cone_l, w, &hit);
        if (!hit) {
            raycast(cone_r, w, &hit);
        }

        // Check if entirely contained within the FOV cone
        if (!hit) {
            vec2 endpoint_vec = {
                w.u.x - p.x,
                w.u.y - p.y
            };

            float from_mid = atan2f(cross(endpoint_vec, mid_vec), dot(endpoint_vec, mid_vec)) * 180 / PI;
            hit = from_mid >= -FOV / 2 && from_mid <= FOV / 2;
        }

        if (!hit) {
            vec2 endpoint_vec = {
                w.v.x - p.x,
                w.v.y - p.y
            };

            float from_mid = atan2f(cross(endpoint_vec, mid_vec), dot(endpoint_vec, mid_vec)) * 180 / PI;
            hit = from_mid >= -FOV / 2 && from_mid <= FOV / 2;
        }

        if (hit) {
            relevant_walls = (segment*) realloc(relevant_walls, sizeof(segment) * ++num_relevant);
            relevant_walls[num_relevant - 1] = w;
        }
    }
    
    // Stores the depth at each pixel and the phase of the wall it hit for easier reconstruction
    depth_buf_info depth_buf[SCREEN_WIDTH];
    segment ray = {p, {0, 0}};

    // Skips every second raycast on walls for performance
    for (int i = 0; i < SCREEN_WIDTH; i += 2) {
        float ray_angle = (i * FOV / SCREEN_WIDTH) - (FOV / 2);
        ray.v.x = p.x + DOV * cosf((pa + ray_angle) * PI / 180);
        ray.v.y = p.y + DOV * sinf((pa + ray_angle) * PI / 180);

        int wall2pt;
        bool hit_wall = false;
        segment closest_wall = {{0, 0}, {0, 0}, CHECK};
        depth_buf_info info = {MAX_VIEW_DIST, 0, 0, 0};

        // Checks if the ray from the camera intersects any walls
        for (int j = 0; j < num_relevant; j++) {
            bool hit = false;
            vec2 pt = raycast(relevant_walls[j], ray, &hit);
            if (!hit) continue;

            // Checks if the intersected wall is the closest to the camera
            float ptDist2 = dist2(pt, p);
            if (ptDist2 < info.depth) {
                info.depth = ptDist2;
                closest_wall = relevant_walls[j];
                hit_wall = true;
                wall2pt = dist2(pt, closest_wall.u);
            }
        }

        // If ray hit a wall
        if (hit_wall) {
            // Draws lines at the edges of walls
            int wall_len = 1 / inv_sqrt(dist2(closest_wall.u, closest_wall.v));
            wall2pt = 1 / inv_sqrt(wall2pt);

            info.phase = wall2pt % 10 < 5;
            info.length = 1000 * inv_sqrt(info.depth);

            switch (closest_wall.tex) {
                case CHECK:
                    if (wall2pt < 2 || wall2pt > wall_len - 2) {
                        vertical_line(i, info.length, 1, 2);
                        
                    } else {
                        info.is_checked = true;
                        check_line(i, info.length, info.phase);
                    }

                    break;
                
                case DOOR:
                    vertical_line(i, info.length, 1, 1);
                    break;

            }
        }

        depth_buf[i] = (depth_buf_info) {info.depth, info.phase, info.length, info.is_checked};
        depth_buf[i+1] = depth_buf[i];
    }

    for (int i = 0; i < NUM_ENEMIES; i++) {
        enemy e = enemies[i];
        vec2 e_vec = {
            e.pos.x - p.x,
            e.pos.y - p.y
        };

        float enemy_angle = atan2f(cross(e_vec, mid_vec), dot(e_vec, mid_vec)) * 180 / PI;
        if (enemy_angle >= FOV / 2 || enemy_angle < -FOV / 2) continue;

        // Walk across lateral pixels affected by sprite, if any have depth more than enemy distance draw enemy.
        float enemy_dist2 = dist2(e.pos, p);
        float enemy_dist_inv = inv_sqrt(enemy_dist2);
        int scale_height = e.s[e.anim_state].height * 50 * enemy_dist_inv;
        int scale_width = e.s[e.anim_state].width * 50 * enemy_dist_inv;

        int enemy_screen_x = SCREEN_WIDTH - SCREEN_WIDTH * (enemy_angle + (FOV / 2)) / FOV;
        int enemy_screen_l = MAX(MIN(enemy_screen_x - scale_width / 2, SCREEN_WIDTH - 1), 0);
        int enemy_screen_r = MAX(MIN(enemy_screen_x + scale_width / 2, SCREEN_WIDTH - 1), 0);

        bool draw = false;
        for (int j = enemy_screen_l; j < enemy_screen_r; j++) {
            if (depth_buf[j].depth > enemy_dist2) {
                draw = true;
                break;
            }
        }

        if (!draw) continue;

        int enemy_screen_y = WALL_OFFSET - scale_height / 3;
        if (is_shooting && enemy_angle >= -FOV / 8 && enemy_angle < FOV / 8) {
            oled_write_bmp_P_scaled(e.s_hurt[e.anim_state], scale_height, scale_width, enemy_screen_x - scale_width / 2, enemy_screen_y);
            if (--enemies[i].health < 0) {
                reload_enemy(&enemies[i]);
                score++;
            }
        
        } else {
            oled_write_bmp_P_scaled(e.s[e.anim_state], scale_height, scale_width, enemy_screen_x - scale_width / 2, enemy_screen_y);
        }

        // Redraw walls where entity sprite should be behind
        for (int j = enemy_screen_l; j < enemy_screen_r; j++) {
            depth_buf_info info = depth_buf[j];
            if (info.depth > enemy_dist2) continue;

            for (int k = 0; k < UI_HEIGHT; k++) {
                oled_write_pixel(j, k, 0);
            }
            
            vertical_line(j, SCREEN_HEIGHT, 0, 1);
            if (j % 2 != 0) continue;

            if (info.is_checked) {
                check_line(j, info.length, info.phase);
            
            } else {
                vertical_line(j, info.length, 1, 2);
            }
        }
    }

    free(relevant_walls);
}

void draw_gun(bool moving, bool show_flash) {

    // Walking animation
    if (moving) {
        int time_elapsed = timer_elapsed32(game_time);
        gun_x = GUN_X + 8 * sin((float) time_elapsed * 0.005);
        gun_y = GUN_Y + 3 + 3 * cos((float) time_elapsed * 0.01);
    
    // Slowly move gun back to centre when not moving
    } else {
        if (gun_y != GUN_Y) gun_y += GUN_Y > gun_y ? 1 : -1;
        if (gun_x != GUN_X) {
            int inc = abs(GUN_X - gun_x) > 2 ? 2 : 1;
            gun_x += GUN_X > gun_x ? inc : -inc;
        }
    } 

    oled_write_bmp_P(gun_sprite, gun_x - GUN_WIDTH/2, gun_y - GUN_HEIGHT);
    if (show_flash) {
        oled_write_bmp_P(muzzle_flash_sprite, gun_x - FLASH_WIDTH/2 + 2, gun_y - 3*FLASH_HEIGHT/4 - GUN_HEIGHT);
    }
}

void vertical_line(int x, int half_length, bool color, int skip) {
    
    for (int i = 0; i < half_length; i += skip) {
        if (WALL_OFFSET - i >= 0) {
            oled_write_pixel(x, WALL_OFFSET - i, color);
        }

        // Ensures that the wall doesnt overlap with the UI
        if (WALL_OFFSET + i < UI_HEIGHT) {
            oled_write_pixel(x, WALL_OFFSET + i, color);
        }
    }
}

void check_line(int x, int half_length, bool phase) {
   
    int lower = WALL_OFFSET - half_length;
    int upper = WALL_OFFSET + half_length;

    for (int i = lower; i < upper; i+=2) {
        if (phase) {
            if (i == lower || (i >= lower + half_length && i <= lower + 3 * half_length / 2)) {
                i += half_length / 2;
            }
        } else {
            if ((i >= lower + half_length / 2 && i <= lower + half_length) || (i >= lower + 3 * half_length / 2 && i <= upper)) {
                i += half_length / 2;
            }
        }
        
        if (i > UI_HEIGHT) break;
        
        oled_write_pixel(x, i, 1);
    }

    oled_write_pixel(x, WALL_OFFSET - half_length, 1);
    if (WALL_OFFSET + half_length < UI_HEIGHT) {
        oled_write_pixel(x, WALL_OFFSET + half_length, 1);
    }
}

void oled_write_bmp_P(sprite img, int x, int y) {

    int row = 0, col = 0;
    for (int i = 0; i < img.size; i++) {
        uint8_t c = pgm_read_byte(img.bmp++);
        uint8_t m = img.mask == NULL ? 0x00 : pgm_read_byte(img.mask++);

        for (int j = 0; j < 8; j++) {
            bool px = c & (1 << (7 - j));
            bool pxm = m & (1 << (7 - j));
            if (px) oled_write_pixel(x + col, y + row, true);
            if (pxm) oled_write_pixel(x + col, y + row, false);
            if (++col == img.width) {
                row++;
                col = 0;
                if (row + y >= UI_HEIGHT) return;
            }
        }
    }
}

void oled_write_bmp_P_scaled(sprite img, int draw_height, int draw_width, int x, int y) {

    if (draw_height < 1 || draw_width < 1) return;

    int row = 0, col = 0;
    for (int i = 0; i < img.size; i++) {
        uint8_t c = pgm_read_byte(img.bmp++);
        uint8_t m = pgm_read_byte(img.mask++);

        for (int j = 0; j < 8; j++) {
            int draw_row = draw_height * row / img.height;
            if (draw_row >= UI_HEIGHT) return;

            int draw_row_lim = draw_height * (row + 1) / img.height;
            int draw_col = draw_width * col / img.width;
            int draw_col_lim = draw_width * (col + 1) / img.width;
            bool px = c & (1 << (7 - j));
            bool pxm = m & (1 << (7 - j));

            for (int k = draw_row; k < draw_row_lim; k++) {
                if (y + k < 0) continue;
                if (y + k >= UI_HEIGHT) break;

                for (int l = draw_col; l < draw_col_lim; l++) {
                    if (x + l < 0) continue;
                    if (x + l >= SCREEN_WIDTH) break;
                    if (px) oled_write_pixel(x + l, y + k, true);
                    if (pxm) oled_write_pixel(x + l, y + k, false);
                }
            }

            if (++col == img.width) {
                row++;
                col = 0;
            }
        }
    }
}

// =================== ENEMY LOGIC ===================

void reload_enemy(enemy* e) {

    e->health = 10;
    while (1) {
        e->pos = get_valid_spawn();
        if (dist2(e->pos, p) > e->width * e->width) return;
    }
}

void enemy_update() {

    int enemy_vision_range2 = ENEMY_VISION_RANGE * ENEMY_VISION_RANGE;
    for (int i = 0; i < NUM_ENEMIES; i++) {
        enemy e = enemies[i];
        float player_dist2 = dist2(e.pos, p);
        if (player_dist2 > enemy_vision_range2 || player_dist2 <= 300) continue;

        if (abs(e.pos.y - p.y) > 1) {
            vec2 eny = {e.pos.x, e.pos.y + ENEMY_WALK_SPEED * (p.y - e.pos.y > 0 ? 1 : -1)};
            if (!collision_detection(eny, true)) enemies[i].pos.y = eny.y;
        }

        if (abs(e.pos.x - p.x) > 1) {
            vec2 enx = {e.pos.x + ENEMY_WALK_SPEED * (p.x - e.pos.x > 0 ? 1 : -1), e.pos.y};
            if (!collision_detection(enx, true)) enemies[i].pos.x = enx.x;
        }
    }
}


// =================== GAME LOGIC =================== //


vec2 get_valid_spawn(void) {

    int col_dist2 = WALL_COLLISION_DIST * WALL_COLLISION_DIST;
    while (1) {
        vec2 new_pos = {
            col_dist2 + (rand() % (MAP_WIDTH - col_dist2)),
            col_dist2 + (rand() % (MAP_HEIGHT - col_dist2))
        };

        bool valid = true;
        for (int i = 6; i < num_walls; i += 4) {
            vec2 lt = walls[i+1].u;
            vec2 rb = walls[i+3].u;

            valid &= new_pos.x > rb.x + col_dist2
                  || new_pos.x < lt.x - col_dist2
                  || new_pos.y < lt.y - col_dist2
                  || new_pos.y > rb.y + col_dist2;
                        
            if (!valid) continue;
        }

        if (valid) return new_pos;
    }
}

void doom_setup(void) {

    // Runs intro sequence
    oled_write_bmp_P(doom_logo_sprite, 0, 0);
    game_time = timer_read();
    srand(game_time);

    // Initializes the map and door
    walls = (segment*) malloc(sizeof(segment) * 6);
    num_walls = 1;
    walls[num_walls++] = (segment) {{0, MAP_HEIGHT}, {MAP_WIDTH, MAP_HEIGHT}, CHECK};
    walls[num_walls++] = (segment) {{MAP_WIDTH, MAP_HEIGHT}, {MAP_WIDTH, 0}, CHECK};
    walls[num_walls++] = (segment) {{MAP_WIDTH, 0}, {0, 0}, CHECK};
    walls[num_walls++] = (segment) {{0, 0}, {0, MAP_HEIGHT}, CHECK};

    segment* door_wall = &walls[1 + rand() % 4];
    float wall_len = sqrtf(dist2(door_wall->v, door_wall->u));
    float door_width_perc = DOOR_WIDTH / wall_len;
    float door_placement_perc = (rand() % (int) (100 - door_width_perc)) / (float) 100;
    float dx = door_wall->v.x - door_wall->u.x;
    float dy = door_wall->v.y - door_wall->u.y;

    vec2 door_start = {
        door_wall->u.x + (dx * door_placement_perc),
        door_wall->u.y + (dy * door_placement_perc)
    };

    vec2 door_end = {
        door_wall->u.x + (dx * (door_placement_perc + door_width_perc)),
        door_wall->u.y + (dy * (door_placement_perc + door_width_perc))
    };

    walls[0] = (segment) {door_start, door_end, DOOR};
    // Truncate wall to remove door hole
    walls[num_walls++] = (segment) {door_end, {door_wall->v.x, door_wall->v.y}, CHECK};
    door_wall->v = door_start;

    walls = bsp_wallgen(walls, &num_walls, 0, MAP_WIDTH, 0, MAP_HEIGHT, MAP_GEN_REC_DEPTH);

    // vec2 prev_avg;
    // int original_num_walls = num_walls;
    // for (int i = 6; i < original_num_walls; i += 4) {
    //     segment a = walls[i];
    //     segment b = walls[i+2];

    //     vec2 avg;
    //     avg.x = (a.u.x + a.v.x + b.u.x + b.v.x) / 4;
    //     avg.y = (a.u.y + a.v.y + b.u.y + b.v.y) / 4;

    //     if (i == 6) {
    //         prev_avg = avg;
    //         continue;
    //     }

    //     walls = realloc(walls, sizeof(segment) * (num_walls + 1));
    //     walls[num_walls++] = (segment) { avg, prev_avg, CHECK };
    //     prev_avg = avg;
    // }

    // Initializes the list of possible enemy spawn locations
    for (int i = 0; i < NUM_ENEMIES; i++) {
        enemies[i] = (enemy) {get_valid_spawn(), 10, 8, 0, 0, imp_sheet, sizeof(imp_sheet), imp_hurt_sheet, sizeof(imp_hurt_sheet)};
    }

    // Initializes player state
    p = get_valid_spawn();
    pa = 0;
    shot_timer = 0;
    last_frame = timer_read();
    score = 0;
    has_key = false;
    initialized = true;
}

void doom_dispose(void) {
    
    free(walls);
    initialized = false;
}

#ifdef RENDER_DEBUG
void bresenham_line(segment s, int offset)
{
    int x0 = (int) s.u.x + offset,
        x1 = (int) s.v.x + offset,
        y0 = (int) s.u.y + offset,
        y1 = (int) s.v.y + offset;

    int dx =  abs (x1 - x0),
        sx = x0 < x1 ? 1 : -1;

    int dy = -abs (y1 - y0),
        sy = y0 < y1 ? 1 : -1;

    int err = dx + dy,
        e2;
   
    // Bresenham from https://gist.github.com/bert/1085538
    for (;;) {
        oled_write_pixel(x0, y0, 1);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { 
            err += dy; 
            x0 += sx;
        }

        if (e2 <= dx) { 
            err += dx;
            y0 += sy;
        }
    }
}

void render_debug() {

    int offset = 50;
    for (int i = 0; i < num_walls; i++)
    {
        segment wall = walls[i];
        bresenham_line(wall, offset);
    }

    bresenham_line(walls[0], offset+1);
    bresenham_line(walls[0], offset+2);
    bresenham_line(walls[0], offset-1);
    bresenham_line(walls[0], offset-2);
    
    oled_write_pixel(p.x + offset, p.y + offset, 1);
    
    segment cone_l = {p, {0, 0}};
    float half_fov = FOV / 2;
    cone_l.v.x = p.x + DOV * cosf((pa - half_fov) * PI / 180);
    cone_l.v.y = p.y + DOV * sinf((pa - half_fov) * PI / 180);
    bresenham_line(cone_l, offset);
    
    segment cone_r = {p, {0, 0}};
    cone_r.v.x = p.x + DOV * cosf((pa + half_fov) * PI / 180);
    cone_r.v.y = p.y + DOV * sinf((pa + half_fov) * PI / 180);
    bresenham_line(cone_r, offset);
    
    for (int i = 0; i < NUM_ENEMIES; i++)
    {
        enemy e = enemies[i];
        oled_write_pixel(e.pos.x + offset, e.pos.y + offset, 1);
        oled_write_pixel(e.pos.x + offset - 1, e.pos.y + offset, 1);
        oled_write_pixel(e.pos.x + offset + 1, e.pos.y + offset, 1);
        oled_write_pixel(e.pos.x + offset, e.pos.y + offset - 1, 1);
        oled_write_pixel(e.pos.x + offset, e.pos.y + offset + 1, 1);
    }
}
#endif

void doom_update(controls c) {

    if (!initialized || timer_elapsed(game_time) < START_TIME_MILLI) return;
    
    int time_elapsed = timer_elapsed32(last_frame);
    if (time_elapsed < FRAME_TIME_MILLI) return;
    
    oled_clear();
    // clear_frame_buffer();
    if (shot_timer > 0) shot_timer--;
    if (c.shoot && shot_timer == 0) shot_timer = 2;

    if (c.l) {
        pa -= ROTATION_SPEED < 0 ? ROTATION_SPEED + 360 : ROTATION_SPEED;
    }

    if (c.r) {
        pa += ROTATION_SPEED >= 360 ? ROTATION_SPEED - 360 : ROTATION_SPEED;
    }

    if (!c.d != !c.u) {
        int walk_dist = c.u ? WALK_SPEED : -WALK_SPEED;
        vec2 pnx = {p.x + walk_dist * cos(pa * (PI / 180)), p.y};
        vec2 pny = {p.x, p.y + walk_dist * sin(pa * (PI / 180))};
        if (!collision_detection(pnx, false)) p.x = pnx.x;
        if (!collision_detection(pny, false)) p.y = pny.y;
    }
    
    enemies[0].anim_state = time_elapsed % 2000 < 1000 ? 0 : 1;
    enemies[1].anim_state = enemies[0].anim_state;
    if (time_elapsed % 200 < 100)
        enemy_update();

    #ifdef RENDER_DEBUG
        render_debug();
        last_frame = timer_read();
        return;
    #endif
        
    render_map(p, pa, shot_timer > 0 && c.shoot);
    draw_gun(c.u, shot_timer > 0);

    for (int i = 0; i < SCREEN_WIDTH; i++) {
        oled_write_pixel(i, UI_HEIGHT, 1);
    }
    
    // Displays the current game time
    oled_set_cursor(1, 7);
    oled_write_P(PSTR("TIME: "), false);
    oled_write(get_u32_str((time_elapsed - START_TIME_MILLI) / 1000, ' '), false);
    
    // Displays the players current score
    oled_set_cursor(12, 7);
    oled_write_P(PSTR("SCORE:"), false);
    oled_write(get_u8_str(score, ' '), false);
    
    time_elapsed = timer_elapsed32(last_frame);
    int fpms = 1000 / (float) time_elapsed;
    oled_set_cursor(0, 0);
    oled_write("FPS:", false);
    oled_write(get_u8_str(fpms, ' '), false);

    // print_frame_buffer();
    last_frame = timer_read();
}

const char* get_u32_str(uint32_t value, char pad) {
    static char buf[11] = {0};
    buf[10] = '\0';

    for (int i = 0; i < 10; i++) {
        char c = 0x30 + value % 10;
        buf[9 - i] = (c == 0x30 && i == 0) ? c : value > 0 ? c : pad;
        value /= 10;
    }

    return buf;
}