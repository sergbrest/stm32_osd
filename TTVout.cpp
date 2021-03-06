/*
 Copyright (c) 2010 Myles Metzer
 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated documentation
 files (the "Software"), to deal in the Software without
 restriction, including without limitation the rights to use,
 copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following
 conditions:
 The above copyright notice and this permission notice shall be
 included in all copies or substantial portions of the Software.
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 OTHER DEALINGS IN THE SOFTWARE.
*/

/* A note about how Color is defined for this version of TVout
 *
 * Where ever choosing a color is mentioned the following are true:
 *  BLACK =0
 *  WHITE =1
 *  INVERT  =2
 *  All others will be ignored.
*/

/*
// TTVout (TVout compatible library) for Arduino Stm 32 v0.1
// Modified date 2017/01/12 by Tamayoshi, partial diversion from original TVout.cpp
// Modified date 2017/02/04 Replace SPI's SPI 2. dmaSend () with my own function, by Tamiyoshi
// Correction date 2017/02/27 Modified to use bit band for drawing processing
// Modified date 2017/02/28 Original bug fix for draw_circle ()
// Correction date 2017/03/03 TNTSC -> v 2.2 compliant
// Correction date 2017/03/24 Initialization defect of tone () was fixed, PWM initialization processing was added
// Modification date 2017/04/13 draw_rect, change argument type of draw_circle, fix for 48 MHz compatibility
// Corrected date 2017/04/26 Correspondence of defect of original version of draw_rect
// Modified date 2017/04/26 Changed coordinate argument type of print system from uint8_t to uint16_t
// Updated date 2017/04/30 Fixed SPI 1, SPI 2 selection designation possible
// Update date 2017/05/10 Correspondence to draw_rect failure
// Updated date 2017/05/10 clock of 48 MHz supported
// updated date 2017/05/10 Correspondence to draw_circle defect
// Updated date 2017/05/16 Corresponds to bug that becomes HIGH when tone is stopped
// updated date 2017/06/25, NTSC object is modified to dynamic generation, NTSC external memory area specification supported
// Update date 2017/07/29, bug in UP processing of shift () (write to outside of VRAM)
// Update date 2017/11/18, change the return value of hres (), hres () to int16_t
//
// *Part of this program source is created by Myles Metzers, modified by Avamander and released
// I am diverting TVout library for Arduino.
// · Avamander modified version (diversion source library of this library)
// Public site Avamander / Arduino-tvout https://github.com/Avamander/arduino-tvout/
// · TVout original version
// Public site arduino-tvout https://code.google.com/archive/p/arduino-tvout/downloads
// Development blog http://vwlowen.co.uk/arduino/tvout/tvout.htm
//
// 2. It is support to see the following API.
// void force_vscale (char sfactor);
// void force_outstart (uint8_t time);
// void force_linestart (uint8_t line);
// void set_vbi_hook (void (* func) ());
// void set_hbi_hook (void (* func) ());
//
// 3. Font TVoutfonts for TVVout not included
// Get it from https://github.com/Avamander/arduino-tvout/ and use it
//
*/

#include <libmaple/bitband.h>
#include "TTVout.h"

//const int pwmOutPin = PB9;      // PWM output pin for tone

// for tone
short tone_pin = -1;        // pin for outputting sound
short tone_freq = 444;      // tone frequency (0=pause)

// Start using TTVout
void TTVout::begin(uint8_t mode, uint8_t spino, uint8_t* extram) {
    TNTSC->begin(mode, spino,extram);  // Start NTSC video output
    init( TNTSC->VRAM(),  // Start NTSC video output
    	TNTSC->width(),   // Specify horizontal screen size
    	TNTSC->height()   // Screen vertical size specification
     );
	// Set output pin for tone
//	pinMode(pwmOutPin, PWM);
	noTone();
}

//
// Initialization
//
void TTVout::init(uint8_t* vram, uint16_t width, uint16_t height) {
  _screen = vram;  
  _width  = width;
  _height = height;
  _hres   = _width/8;
  _vres   = _height;
  _adr = (volatile uint32_t*)(BB_SRAM_BASE + ((uint32_t)_screen - BB_SRAM_REF) * 32);
}
// Wait between frames
void TTVout::delay_frame(uint16_t x) {
  TNTSC->delay_frame(x);
}
// Blanking period start hook setting
void TTVout::setBktmStartHook(void (*func)()) {
  TNTSC->setBktmStartHook(func);
}
// Blanking period end hook setting
void TTVout::setBktmEndHook(void (*func)()) {
  TNTSC->setBktmEndHook(func);
}
// Draw points
void TTVout::set_pixel(int16_t x, int16_t y, uint8_t d) {
	if ((x < 0) || (y < 0) || (x >= _width) || (y >= _height))
	  return;
  sp(x,y,d);
}
// Clear screen
void TTVout::cls() {
  memset(_screen, 0, _vres*_hres);
}
// Draw a straight line
template <typename T> int _v_sgn(T val) {return (T(0) < val) - (val < T(0));}
#ifndef abs
#define abs(a)  (((a)>0) ? (a) : -(a))
#endif
#define swap(a,b) tmp =a;a=b;b=tmp
void TTVout::draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t dt){
   int dx=abs(x1-x0), dy=abs(y1-y0),sx=_v_sgn(x1-x0),sy=_v_sgn(y1-y0);
   int err=dx-dy; 
   if((x0!=x1)||(y0!=y1))set_pixel(x1,y1,dt);
   do{ set_pixel(x0,y0,dt);
       int e2=2*err;
       if (e2 > -dy){err-=dy;x0+=sx;}
       if (e2 <  dx){err+=dx;y0+=sy;}
   }   while ((x0!=x1)||(y0!=y1));
}
// Acquire the color of the specified coordinates
uint8_t TTVout::get_pixel(int16_t x, int16_t y) {
#if BITBAND==1
  return _adr[_width*y+ (x&0xf8) +7 -(x&7)];
#else
  if (x >= _width || y >= _height)
    return 0;
  if (_screen[(x>>3)+y*_width] & (0x80 >>(x&7)))
    return 1;
  return 0;
#endif
}

// Fill with the specified color of the whole screen
void TTVout::fill(uint8_t color) {
  switch(color) {
    case BLACK:
      _cursor_x = 0;
      _cursor_y = 0;
      for (int16_t i=0; i < _vres; i++)
        memset( &_screen[i*_hres], 0, _hres);
      break;
    case WHITE:
      _cursor_x = 0;
      _cursor_y = 0;
      for (int16_t i=0; i < _vres; i++)
        memset( &_screen[i*_hres], 0xff, _hres);
        break;
    case INVERT:
      for (int16_t i = 0; i < _vres; i++)
        for (int16_t j = 0; j < _hres; j++)
        _screen[i*_hres+j] = ~_screen[i*_hres+j];
      break;
  }
}

// Draw a horizontal line with the specified color
void TTVout::draw_row(int16_t line, int16_t x0, int16_t x1, uint8_t c) {
  uint8_t lbit, rbit;
  if (x0 == x1)
    set_pixel(x0,line,c);
  else {
    if (x0 > x1) {
      lbit = x0;
      x0 = x1;
      x1 = lbit;
    }
    lbit = 0xff >> (x0&7);
    x0 = x0/8 + _hres*line;
    rbit = ~(0xff >> (x1&7));
    x1 = x1/8 + _hres*line;
    if (x0 == x1) {
      lbit = lbit & rbit;
      rbit = 0;
    }
    if (c == WHITE) {
      _screen[x0++] |= lbit;
      while (x0 < x1)
        _screen[x0++] = 0xff;
      _screen[x0] |= rbit;
    }
    else if (c == BLACK) {
      _screen[x0++] &= ~lbit;
      while (x0 < x1)
        _screen[x0++] = 0;
      _screen[x0] &= ~rbit;
    }
    else if (c == INVERT) {
      _screen[x0++] ^= lbit;
      while (x0 < x1)
        _screen[x0++] ^= 0xff;
      _screen[x0] ^= rbit;
    }
  }	
}

// Draw a vertical line with the specified color
void TTVout::draw_column(int16_t row, int16_t y0, int16_t y1, uint8_t c) {

  uint8_t bit;
  int16_t byte;
  
  if (y0 == y1)
    set_pixel(row,y0,c);
  else {
    if (y1 < y0) {
      bit = y0;
      y0 = y1;
      y1 = bit;
    }
    bit = 0x80 >> (row&7);
    byte = row/8 + y0*_hres;
    if (c == WHITE) {
      while ( y0 <= y1) {
        _screen[byte] |= bit;
        byte += _hres;
        y0++;
      }
    }
    else if (c == BLACK) {
      while ( y0 <= y1) {
        _screen[byte] &= ~bit;
        byte += _hres;
        y0++;
      }
    }
    else if (c == INVERT) {
      while ( y0 <= y1) {
        _screen[byte] ^= bit;
        byte += _hres;
        y0++;
      }
    }
  }
}

// Rectangle drawing
void TTVout::draw_rect(int16_t x0, int16_t y0, int16_t w, int16_t h, uint8_t c, int8_t fc) {
	if (fc == -1) {
		w--;
		h--;
		if (w == 0 && h == 0) {
			 set_pixel(x0,y0,c);
		} else if (w == 0 || h == 0) {
			draw_line(x0,y0,x0+w,y0+h,c);
		} else {
			// Horizon
   		   draw_line(x0,y0  , x0+w, y0  , c);
   		   draw_line(x0,y0+h, x0+w, y0+h, c);
		   // vertical line
		   if (h>1) {	
	         draw_line(x0,  y0+1,x0  ,y0+h-1,c);
	         draw_line(x0+w,y0+1,x0+w,y0+h-1,c);
		   }
		}
	} else {
		for (int16_t i = y0; i < y0+h; i++) {
          draw_row(i,x0,x0+w,c);
		}
	}
}

// draw a circle
void TTVout::draw_circle(int16_t x0, int16_t y0, int16_t radius, uint8_t c, int8_t fc) {

  int16_t f = 1 - radius;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * radius;
  int16_t x = 0;
  int16_t y = radius;
  int16_t pyy = y,pyx = x;
  
  //there is a fill color

  if (fc != -1)
    draw_row(y0,x0-radius,x0+radius,c);
  
  set_pixel(x0, y0 + radius,c);
  set_pixel(x0, y0 - radius,c);
  set_pixel(x0 + radius, y0,c);
  set_pixel(x0 - radius, y0,c);
  
//  while(x < y) { 
  while(x+1 < y) {  // 2017/02 / 28 original defect fix
    if(f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;
    
    //there is a fill color
    if (fc != -1) {
      //prevent double draws on the same rows
      if (pyy != y) {
        draw_row(y0+y,x0-x,x0+x,c);
        draw_row(y0-y,x0-x,x0+x,c);
      }
    	
      if (pyx != x && x != y) {
        draw_row(y0+x,x0-y,x0+y,c);
        draw_row(y0-x,x0-y,x0+y,c);
      }

      pyy = y;
      pyx = x;
    }
    set_pixel(x0 + x, y0 + y,c);
    set_pixel(x0 - x, y0 + y,c);
    set_pixel(x0 + x, y0 - y,c);
    set_pixel(x0 - x, y0 - y,c);
    set_pixel(x0 + y, y0 + x,c);
    set_pixel(x0 - y, y0 + x,c);
    set_pixel(x0 + y, y0 - x,c);
    set_pixel(x0 - y, y0 - x,c);
  }
}

// Bitmap drawing
void TTVout::bitmap(uint16_t x, uint16_t y, const unsigned char * bmp,
           uint16_t i, uint16_t width, uint16_t lines) {

  uint8_t temp, lshift, rshift, save, xtra;
  uint16_t si = 0;
  
  rshift = x&7;
  lshift = 8-rshift;
  if (width == 0) {
  	width = *(bmp + i);
    i++;
  }
  if (lines == 0) {
    lines = *(bmp + i);
    i++;
  }
    
  if (width&7) {
    xtra = width&7;
    width = width/8;
    width++;
  }
  else {
    xtra = 8;
    width = width/8;
  }
  
  for (uint8_t l = 0; l < lines; l++) {
    si = (y + l)*_hres + x/8;
    if (width == 1)
      temp = 0xff >> rshift + xtra;
    else
      temp = 0;
    save = _screen[si];
    _screen[si] &= ((0xff << lshift) | temp);
  	temp = *(bmp + i++);
    _screen[si++] |= temp >> rshift;
    for ( uint16_t b = i + width-1; i < b; i++) {
      save = _screen[si];
      _screen[si] = temp << lshift;
    	temp = *(bmp + i);
      _screen[si++] |= temp >> rshift;
    }
    if (rshift + xtra < 8)
      _screen[si-1] |= (save & (0xff >> rshift + xtra)); //test me!!!
    if (rshift + xtra - 8 > 0)
      _screen[si] &= (0xff >> rshift + xtra - 8);
    _screen[si] |= temp << lshift;
  }
} // end of bitmap

// Scroll the screen
void TTVout::shift(uint8_t distance, uint8_t direction) {
  uint8_t * src;
  uint8_t * dst;
  uint8_t * end;
  uint8_t shift;
  uint8_t tmp;
  switch(direction) {
    case UP:
      dst = _screen;
      src = _screen + distance*_hres;
      end = _screen + _vres*_hres;
      while (src <  end) {
        *dst = *src;
        *src = 0;
        dst++;
        src++;
      }
      break;
    case DOWN:
      dst = _screen + _vres*_hres;
      src = dst - distance*_hres;
      end = _screen;
        
      while (src >= end) {
        *dst = *src;
        *src = 0;
        dst--;
        src--;
      }
      break;
    case LEFT:
      shift = distance & 7;
      
      for (uint8_t line = 0; line < _vres; line++) {
        dst = _screen + _hres*line;
        src = dst + distance/8;
        end = dst + _hres-2;
        while (src <= end) {
          tmp = 0;
          tmp = *src << shift;
          *src = 0;
          src++;
          tmp |= *src >> (8 - shift);
          *dst = tmp;
          dst++;
        }
        tmp = 0;
        tmp = *src << shift;
        *src = 0;
        *dst = tmp;
      }
      break;
    case RIGHT:
      shift = distance & 7;
      
      for (uint8_t line = 0; line < _vres; line++) {
        dst = _screen + _hres-1 + _hres*line;
        src = dst - distance/8;
        end = dst - _hres+2;
        while (src >= end) {
          tmp = 0;
          tmp = *src >> shift;
          *src = 0;
          src--;
          tmp |= *src << (8 - shift);
          *dst = tmp;
          dst--;
        }
        tmp = 0;
        tmp = *src >> shift;
        *src = 0;
        *dst = tmp;
      }
      break;
  }
} // end of shift


// Select font
void TTVout::select_font(const unsigned char * f) {
  _font = f;
}

// Display characters
void TTVout::print_char(uint16_t x, uint16_t y, uint8_t c) {
	c -= *(_font+2);
  bitmap(x, y, _font , c* *(_font+1) + 3, *_font , *(_font+1) );
}

void TTVout::inc_txtline() {
  if (_cursor_y >= (_vres - *(_font+1)))
    shift(*(_font+1),UP);
  else
    _cursor_y += *(_font+1);
}

void TTVout::write(const char *str) {
  while (*str)
    write(*str++);
}

void TTVout::write(const uint8_t *buffer, uint8_t size) {
  while (size--)
    write(*buffer++);
}

void TTVout::write(uint8_t c) {
  switch(c) {
    case '\0':      //null
      break;
    case '\n':      //line feed
      _cursor_x = 0;
      inc_txtline();
      break;
    case 8:       //backspace
      _cursor_x -= *_font;
      print_char(_cursor_x,_cursor_y,' ');
      break;
    case 13:      //carriage return
      _cursor_x = 0;
      break;
    case 14:      //form feed new page(clear screen)
      break;
    default:
      if (_cursor_x >= (_hres*8 - *_font)) {
        _cursor_x = 0;
        inc_txtline();
        print_char(_cursor_x,_cursor_y,c);
      }
      else
        print_char(_cursor_x,_cursor_y,c);
      _cursor_x += *_font;
  }
}

void TTVout::print(const char str[]){
  write(str);
}

void TTVout::print(char c, int base){
  print((long) c, base);
}

void TTVout::print(unsigned char b, int base){
  print((unsigned long) b, base);
}

void TTVout::print(int n, int base){
  print((long) n, base);
}

void TTVout::print(unsigned int n, int base){
  print((unsigned long) n, base);
}

void TTVout::print(long n, int base){
  if (base == 0) {
    write(n);
  } else if (base == 10) {
    if (n < 0) {
      print('-');
      n = -n;
    }
    printNumber(n, 10);
  } else {
    printNumber(n, base);
  }
}

void TTVout::print(unsigned long n, int base) {
  if (base == 0) write(n);
  else printNumber(n, base);
}

void TTVout::print(double n, int digits) {
  printFloat(n, digits);
}

void TTVout::println(void) {
  print('\r');  print('\n');  
}

void TTVout::println(const char c[]) {
  print(c);  println();
}

void TTVout::println(char c, int base) {
  print(c, base);  println();
}

void TTVout::println(unsigned char b, int base) {
  print(b, base);  println();
}

void TTVout::println(int n, int base) {
  print(n, base);  println();
}

void TTVout::println(unsigned int n, int base) {
  print(n, base);  println();
}

void TTVout::println(long n, int base) {
  print(n, base);  println();
}

void TTVout::println(unsigned long n, int base) {
  print(n, base);  println();
}

void TTVout::println(double n, int digits) {
  print(n, digits);  println();
}

void TTVout::printPGM(const char str[]) {
  char c;
  while (c = *str) {
    str++;
    write(c);
  }
}

void TTVout::printPGM(uint16_t x, uint16_t y, const char str[]) {
  char c;
  _cursor_x = x; _cursor_y = y;
  while (c = *str) {
    str++;
    write(c);
  }
}

void TTVout::set_cursor(uint16_t x, uint16_t y) {
  _cursor_x = x; _cursor_y = y;
}

void TTVout::print(uint16_t x, uint16_t y, const char str[]) {
  _cursor_x = x;  _cursor_y = y;
  write(str);
}

void TTVout::print(uint16_t x, uint16_t y, char c, int base) {
  _cursor_x = x;  _cursor_y = y;
  print((long) c, base);
}

void TTVout::print(uint16_t x, uint16_t y, unsigned char b, int base) {
  _cursor_x = x;  _cursor_y = y;
  print((unsigned long) b, base);
}


void TTVout::print(uint16_t x, uint16_t y, int n, int base) {
  _cursor_x = x;  _cursor_y = y;
  print((long) n, base);
}

void TTVout::print(uint16_t x, uint16_t y, unsigned int n, int base) {
  _cursor_x = x;  _cursor_y = y;
  print((unsigned long) n, base);
}

void TTVout::print(uint16_t x, uint16_t y, long n, int base) {
  _cursor_x = x;  _cursor_y = y;
  print(n,base);
}

void TTVout::print(uint16_t x, uint16_t y, unsigned long n, int base) {
  _cursor_x = x;  _cursor_y = y;
  print(n,base);
}

void TTVout::print(uint16_t x, uint16_t y, double n, int digits) {
  _cursor_x = x;  _cursor_y = y;
  print(n,digits);
}

void TTVout::println(uint16_t x, uint16_t y, const char c[]){
  _cursor_x = x;  _cursor_y = y;
  print(c);  println();
}

void TTVout::println(uint16_t x, uint16_t y, char c, int base){
  _cursor_x = x;  _cursor_y = y;
  print(c, base);  println();
}

void TTVout::println(uint16_t x, uint16_t y, unsigned char b, int base){
  _cursor_x = x;  _cursor_y = y;
  print(b, base);  println();
}

void TTVout::println(uint16_t x, uint16_t y, int n, int base){
  _cursor_x = x;  _cursor_y = y;
  print(n, base);  println();
}

void TTVout::println(uint16_t x, uint16_t y, unsigned int n, int base){
  _cursor_x = x;  _cursor_y = y;
  print(n, base);  println();
}

void TTVout::println(uint16_t x, uint16_t y, long n, int base){
  _cursor_x = x;  _cursor_y = y;
  print(n, base);  println();
}

void TTVout::println(uint16_t x, uint16_t y, unsigned long n, int base){
  _cursor_x = x; _cursor_y = y;
  print(n, base); println();
}

void TTVout::println(uint16_t x, uint16_t y, double n, int digits){
  _cursor_x = x;  _cursor_y = y;
  print(n, digits);  println();
}

void TTVout::printNumber(unsigned long n, uint8_t base){
  unsigned char buf[8 * sizeof(long)]; // Assumes 8-bit chars. 
  unsigned long i = 0;

  if (n == 0) {
    print('0');
    return;
  } 

  while (n > 0) {
    buf[i++] = n % base;
    n /= base;
  }

  for (; i > 0; i--)
    print((char) (buf[i - 1] < 10 ?
      '0' + buf[i - 1] :
      'A' + buf[i - 1] - 10));
}

void TTVout::printFloat(double number, uint8_t digits) { 
  // Handle negative numbers
  if (number < 0.0)
  {
     print('-');
     number = -number;
  }

  // Round correctly so that print(1.999, 2) prints as "2.00"
  double rounding = 0.5;
  for (uint8_t i=0; i<digits; ++i)
    rounding /= 10.0;
  
  number += rounding;

  // Extract the integer part of the number and print it
  unsigned long int_part = (unsigned long)number;
  double remainder = number - (double)int_part;
  print(int_part);

  // Print the decimal point, but only if there are digits beyond
  if (digits > 0)
    print("."); 

  // Extract digits from the remainder one at a time
  while (digits-- > 0)
  {
    remainder *= 10.0;
    int toPrint = int(remainder);
    print(toPrint);
    remainder -= toPrint; 
  } 
}

//
// sound out
// Arguments
// pin: PWM output pin (Currently fixed to PB 9)
// freq: Output frequency (Hz) 15 to 50000
// duration: output time (msec)
//
void TTVout::tone(uint16_t freq, uint16_t duration) {
  if (freq < 15 || freq > 50000 ) {
    noTone();
  } else {
    uint32_t f =1000000/(uint16_t)freq;
#if F_CPU == 72000000L
	Timer4.setPrescaleFactor(72); // Divide the system clock by 1 / 72
#elif  F_CPU == 48000000L
	Timer4.setPrescaleFactor(48); // Divide the system clock to 1 / 48
#endif
  	Timer4.setOverflow(f);
    Timer4.refresh();
    Timer4.resume(); 
  //  pwmWrite(pwmOutPin, f/2);  
    if (duration) {
      delay(duration);
      Timer4.pause(); 
      Timer4.setCount(0xffff);
    }
  }
}

//
// stop sound
// Arguments
// pin: PWM output pin (Currently fixed to PB 9)
//
void TTVout::noTone() {
    Timer4.pause();
	Timer4.setCount(0xffff);
}

