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

#include "doom.h"
#include "quantum.h"

// Player location and direction of camera
vec2 p;
int pa;
int shot_timer;

float pow2(float x) { return x * x; }

float dist2(vec2 v, vec2 u) { return pow2(v.x - u.x) + pow2(v.y - u.y); }

void setup() {

  // Runs intro sequence
  oled_clear();
  oled_write_raw_P(doom_logo, LOGO_WIDTH);
  delay(START_TIME_MILLI);
  display.clearDisplay();

  // Initializes player state
  p = { 20.0f, 20.0f };
  pa = 0;
  shot_timer = 0;

  raycast(walls, p, pa, false);

  display.setTextSize(1);
  display.setTextColor(WHITE);

  int score = 0;

  // Waits for inputs continually (main game loop)
  while (true) {
    int l = digitalRead(l_btn);
    int r = digitalRead(r_btn);
    int f = digitalRead(f_btn);
    int shoot = digitalRead(shoot_btn);
    
    if (shoot == 0 && shot_timer == 0) {
      shot_timer = 5;
    }

    if (shot_timer > 0) {
      shot_timer--;
    }

    if (l == 0) {
      pa = (pa - rotSpeed < 0 ? pa - rotSpeed + 360 : pa - rotSpeed);
    }

    if (r == 0) {
      pa = (pa + rotSpeed >= 360 ? pa + rotSpeed - 360 : pa + rotSpeed);
    }

    if (f == 0) {
      vec2 pn = {p.x + 2*cos(pa * (PI/180)), p.y + 2*sin(pa * (PI/180))};
      if (!collision_detection(walls, pn)) {
        p = pn;
      }
    }

    display.clearDisplay();
    for (int i = 0; i < SCREEN_WIDTH; i++) {
      display.drawPixel(i, UI_HEIGHT, WHITE);
    }

    char buf[16];
    // Displays the players current score
    display.setCursor(2, UI_HEIGHT + 3);
    sprintf(buf, "SCORE: %i", score);
    display.println(buf);

    // Displays the current game time
    display.setCursor(SCREEN_WIDTH/2 + 2, UI_HEIGHT + 3);
    sprintf(buf, "TIME: %i", (millis() - START_TIME_MILLI)/1000);
    display.println(buf);

    // Runs the raycasting function if any input has been detected
    raycast(walls, p, pa, shot_timer > 0);
    display.display();
  }
}

// Runs a pseudo-3D raycasting algorithm on the environment around the player
void raycast(wall* walls, vec2 p, int pa, bool show_gun) {

  float x3 = p.x;
  float y3 = p.y;
  float x4, y4;

  // Defines the number of rays
  for (int i = 0; i < 128; i+=2) {

    // Calculates the angle at which the ray is projected
    float angle = (i*(fov/127.0f)) - (fov/2.0f);

    // Projects the endpoint of the ray
    x4 = p.x + dov*cosf((pa + angle) * (PI/180));
    y4 = p.y + dov*sinf((pa + angle) * (PI/180));

    float dist = 100000.0f;
    vec2 pt_final = { NULL, NULL };
    wall cur_wall;
    int cur_edge2pt;

    // Checks if the vector from the camera to the ray's endpoint intersects any walls
    for (int w = 0; w < NUM_WALLS; w++) {

      float x1 = walls[w].points[0].x;
      float y1 = walls[w].points[0].y;
      float x2 = walls[w].points[1].x;
      float y2 = walls[w].points[1].y;

      float denominator = (x1-x2)*(y3-y4)-(y1-y2)*(x3-x4);

      // vectors do not ever intersect
      if (denominator == 0) continue;

      float t = ((x1-x3)*(y3-y4)-(y1-y3)*(x3-x4))/denominator;
      float u = -((x1-x2)*(y1-y3)-(y1-y2)*(x1-x3))/denominator;

      // Case where the vectors intersect
      if (t > 0 && t < 1 && u > 0) {

        vec2 pt = { x1 + t * (x2 - x1), y1 + t * (y2 - y1) };
        float ptDist2 = dist2(pt, p);

        // Checks if the intersected wall is the closest to the camera
        if (ptDist2 < dist) {
          dist = ptDist2;
          pt_final = pt;
          cur_wall = walls[w];
          cur_edge2pt = dist2(pt, cur_wall.points[0]);
        }
      }
    }

    if (pt_final.x != NULL) {

      int length = 25000 / dist;

      // Draws lines at the edges of walls
      int wall_len = dist2(cur_wall.points[0], cur_wall.points[1]);
      if (cur_edge2pt < 2 || wall_len - cur_edge2pt < 2) {
        vertical_line(i, length);
        continue;
      }

      if (cur_wall.tex == CHECK) {
        check_line(i, length, (cur_edge2pt % 1000) < 500);
      }
//      } else if (cur_wall.tex == STRIPE_H) {
//
//      } else if (cur_wall.tex == STRIPE_V) {
//
//      } else if (cur_wall.tex == STRIPE_D) {
//
//      }

    }
  }

  if (show_gun) {
    display.drawBitmap(SCREEN_WIDTH/2 - FLASH_WIDTH/2 + 2, UI_HEIGHT - 3*FLASH_HEIGHT/4 - GUN_HEIGHT, muzzle_flash_bmp, FLASH_WIDTH, FLASH_HEIGHT, WHITE);
  }
  
  display.drawBitmap(SCREEN_WIDTH/2 - GUN_WIDTH/2, UI_HEIGHT - GUN_HEIGHT, gun_bmp_mask,  GUN_WIDTH, GUN_HEIGHT, BLACK);
  display.drawBitmap(SCREEN_WIDTH/2 - GUN_WIDTH/2, UI_HEIGHT - GUN_HEIGHT, gun_bmp,  GUN_WIDTH, GUN_HEIGHT, WHITE);
}

void vertical_line(int x, int half_length) {
  for (int i = 0; i < half_length; i+=2) {
    display.drawPixel(x, UI_HEIGHT/2 + WALL_OFFSET + i, WHITE);
    // Ensures that the wall doesnt overlap with the UI
    if (UI_HEIGHT/2 + WALL_OFFSET - i < UI_HEIGHT) {
      display.drawPixel(x, UI_HEIGHT/2 + WALL_OFFSET - i, WHITE);
    }
  }
}

void check_line(int x, int half_length, boolean phase) {
  int lower = UI_HEIGHT/2 - half_length + WALL_OFFSET;
  int upper = UI_HEIGHT/2 + half_length + WALL_OFFSET;

  for (int i = lower; i < upper; i+=2) {

    if (i > UI_HEIGHT) {break;}

    if (phase) {
      if (i == lower || (i >= lower + half_length && i <= lower + 3*half_length/2)) {
        i += half_length/2;
      }
    } else {
      if ((i >= lower + half_length/2 && i <= lower + half_length) || (i >= lower + 3*half_length/2 && i <= upper)) {
        i += half_length/2;
      }
    }

    display.drawPixel(x, i, WHITE);
  }
  display.drawPixel(x, UI_HEIGHT/2 - half_length + WALL_OFFSET, WHITE);
  display.drawPixel(x, UI_HEIGHT/2 + half_length + WALL_OFFSET, WHITE);
}

bool collision_detection(wall walls[], vec2 p) {

  int collision_dist2 = COLLISION_DIST * COLLISION_DIST;
  for (int i = 0; i < NUM_WALLS; i++) {
    wall w = walls[i];
    float w2 = dist2(w.points[0], w.points[1]);
    if (w2 == 0) continue;

    float t = ((p.x - w.points[0].x) * (w.points[1].x - w.points[0].x) + (p.y - w.points[0].y) * (w.points[1].y - w.points[0].y)) / w2;
    float d2 = dist2(p, {w.points[0].x + t * (w.points[1].x - w.points[0].x), w.points[0].y + t * (w.points[1].y - w.points[0].y)});

    if (d2 < collision_dist2) return true;
  }

  return false;
}


