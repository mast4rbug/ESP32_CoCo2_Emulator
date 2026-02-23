#include "Mode.h"

const Mode Mode::MODE_640x400x70(16, 96, 48, 640, 12, 2, 35, 400, 25175000);
const Mode Mode::MODE_320x200x70(8, 48, 24, 320, 12, 2, 35, 200, 12587500, 0, 0, 2);
const Mode Mode::MODE_640x480x60(16, 96, 48, 640, 10, 2, 33, 480, 25175000);


const Mode Mode::MODE_320x240x60(8, 40, 16, 320, 10, 2, 33, 240, 12587500, 0, 0, 2);    //Official Final
const Mode Mode::MODE_640x240x60(8, 96, 16, 640, 10, 2, 33, 240, 25175000, 0, 0, 2);    //Official Final



const Mode Mode::MODE_320x240x60_4_3        //Perfect Match old 320x240 4:3 ratio   Official Final
(
    8,       // h_front_porch
    40,      // h_sync
    24,      // h_back_porch
    320+6,     // h_active  412 pixels
    10,      // v_front_porch
    2,       // v_sync
    29,      // v_back_porch
    240,     // v_active
    12587500, // pixel_clock ajusté pour ratio 4:3
    0, 0, 2  // autres paramètres
);
/*
const Mode Mode::MODE_320x240x60_4_3        //Perfect Match old 320x240 4:3 ratio   Official Final
(
    8,       // h_front_porch
    40,      // h_sync
    16,      // h_back_porch
    320+92,     // h_active  412 pixels
    10,      // v_front_porch
    2,       // v_sync
    33,      // v_back_porch
    240,     // v_active
    14573333, // pixel_clock ajusté pour ratio 4:3
    0, 0, 2  // autres paramètres
);
*/
const Mode Mode::MODE_640x240x60_4_3        //Perfect Match old 640 x 320 4:3 ratio //ToDO
(
    8,       // h_front_porch
    40,      // h_sync
    16,      // h_back_porch
    320+92,     // h_active  412 pixels
    10,      // v_front_porch
    2,       // v_sync
    33,      // v_back_porch
    240,     // v_active
    14573333, // pixel_clock ajusté pour ratio 4:3
    0, 0, 2  // autres paramètres
);







//const Mode Mode::MODE_640x240x60(16, 96, 48, 640, 10, 2, 33, 240, 25175000, 0, 0, 2);


const Mode Mode::MODE_800x600x56(24, 72, 128, 800, 1, 2, 22, 600, 36000000,0,0,2);
const Mode Mode::MODE_800x600x60(40, 128, 88, 800, 1, 4, 23, 600, 40000000);
const Mode Mode::MODE_400x300x60(20, 64, 44, 400, 1, 4, 23, 300, 20000000, 0, 0, 2);
const Mode Mode::MODE_1024x768x43(8, 176, 56, 1024, 0, 4, 20, 768, 44900000);
const Mode Mode::MODE_1024x768x60(24, 136, 160, 1024, 3, 6, 29, 768, 65000000);
const Mode Mode::MODE_1280x720x60(110, 40, 220, 1280, 5, 5, 20, 720, 74250000);
