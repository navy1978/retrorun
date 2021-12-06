// C program for reading
// struct from a file
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "imgs_screenshot.h"
#include<stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>   /* for uint8_t */
#include <stdio.h>    /* for printf */

/* tell compiler what those GIMP types are */
typedef int guint;
typedef uint8_t guint8;

// use like this:
// g++ -o converter image_converter_util.cpp
// ./converter imgs_press.h >paused_img_high.pnm

int main(){

   int w = sreenshot_high.width;
   int h = sreenshot_high.height;
   int i;
   uint16_t*  RGB565p = (uint16_t*)&(sreenshot_high.pixel_data);

   /* Print P3 PNM header on stdout */
   printf("P3\n%d %d\n255\n",w, h);

   /* Print RGB pixels, ASCII, one RGB pixel per line */
   for(i=0;i<w*h;i++){
      uint16_t RGB565 = *RGB565p++;
      uint8_t r = (RGB565 & 0xf800) >> 8;
      uint8_t g = (RGB565 & 0x07e0) >> 3;
      uint8_t b = (RGB565 & 0x001f) << 3;
      printf("%d %d %d\n", r, g ,b);
   }
}