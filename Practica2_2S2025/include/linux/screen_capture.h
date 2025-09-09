#ifndef __SCREEN_CAPTURE_H
#define __SCREEN_CAPTURE_H

#include <stdint.h> 

struct screen_capture {
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint64_t buffer_size;
    uint64_t buffer; 
};

#endif