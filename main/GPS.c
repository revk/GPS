// Simple GPS
// Copyright (c) 2019 Adrian Kennard, Andrews & Arnold Limited, see LICENSE file (GPL)
static const char TAG[] = "GPS";

#include "revk.h"
#include <driver/i2c.h>
#include <driver/uart.h>
#include <math.h>
#include "oled.h"

#define settings	\
	s8(oledsda,27)	\
	s8(oledscl,14)	\
	s8(oledaddress,0x3D)	\
	u8(oledcontrast,127)	\
	b(oledflip,Y)	\
	s8(gpspps,34)	\
	s8(gpsuart,1)	\
	s8(gpsrx,33)	\
	s8(gpstx,32)	\
	s8(gpsfix,25)	\
	s8(gpsen,26)	\
	b(mph,Y)	\

#define u32(n,d)	uint32_t n;
#define s8(n,d)	int8_t n;
#define u8(n,d)	uint8_t n;
#define b(n,d) uint8_t n;
#define s(n,d) char * n;
settings
#undef u32
#undef s8
#undef u8
#undef b
#undef s
const char *
app_command (const char *tag, unsigned int len, const unsigned char *value)
{
   if (!strcmp (tag, "contrast"))
   {
      oled_set_contrast (atoi ((char *) value));
      return "";                // OK
   }
   return NULL;
}

double speed = 0;
double bearing = 0;
double lat = 0;
double lon = 0;
double alt = 0;
double gsep = 0;
double hdop = 0;
int sats = 0;

static void
nmea (char *s)
{
   if (!s || *s != '$' || s[1] != 'G' || s[2] != 'P')
      return;
   char *f[50];
   int n = 0;
   s++;
   while (*s && n < sizeof (f) / sizeof (*f))
   {
      f[n++] = s;
      while (*s && *s != ',')
         s++;
      if (!*s || *s != ',')
         break;
      *s++ = 0;
   }
   if (!n)
      return;
   if (!strncmp (f[0], "GPGGA", 5) && n >= 14)
   {                            // Fix
      if (strlen (f[2]) > 2)
         lat = ((f[2][0] - '0') * 10 + f[2][1] - '0' + strtod (f[2] + 2, NULL) / 60) * (f[3][0] == 'N' ? 1 : -1);
      if (strlen (f[4]) > 3)
         lon =
            ((f[4][0] - '0') * 100 + (f[4][1] - '0') * 10 + f[4][2] - '0' + strtod (f[4] + 3, NULL) / 60) * (f[5][0] ==
                                                                                                             'E' ? 1 : -1);
      sats = atoi (f[7]);
      hdop = strtod (f[8], NULL);
      alt = strtod (f[9], NULL);
      gsep = strtod (f[10], NULL);
      return;
   }
   if (!strncmp (f[0], "GPRMC", 5) && n >= 12)
   {
      if (strlen (f[1]) >= 6 && strlen (f[9]) >= 6)
      {
         struct tm t = { };
         t.tm_hour = (f[1][0] - '0') * 10 + f[1][1] - '0';
         t.tm_min = (f[1][2] - '0') * 10 + f[1][3] - '0';
         t.tm_sec = (f[1][4] - '0') * 10 + f[1][5] - '0';
         t.tm_year = 100 + (f[9][4] - '0') * 10 + f[9][5] - '0';
         t.tm_mon = (f[9][2] - '0') * 10 + f[9][3] - '0' - 1;
         t.tm_mday = (f[9][0] - '0') * 10 + f[9][1] - '0';
         struct timeval v = { };
         if (f[1][6] == '.')
            v.tv_usec = atoi (f[1] + 7) * 1000;
         v.tv_sec = mktime (&t);
         settimeofday (&v, NULL);
      }
      return;
   }
   if (!strncmp (f[0], "GPVTG", 5) && n >= 10)
   {
      speed = strtod (f[7], NULL);
      return;
   }
}

static void
display_task (void *p)
{
   p = p;
   while (1)
   {
      sleep (1);
      if (sats)
      {
         oled_lock ();
         char temp[100];
         int y = CONFIG_OLED_HEIGHT;
         {
            time_t now = time (0);
            struct tm nowt;
            localtime_r (&now, &nowt);
            if (nowt.tm_year > 100)
            {
               y -= 10;
               strftime (temp, sizeof (temp), "%F\004%T %Z", &nowt);
               oled_text (1, 0, y, temp);
            }
         }
         y -= 10;
         sprintf (temp, "Sats: %d   ", sats);
         oled_text (-1, 0, y, temp);
         y -= 10;
         sprintf (temp, "Lat: %lf   ", lat);
         oled_text (-1, 0, y, temp);
         y -= 10;
         sprintf (temp, "Lon: %lf   ", lon);
         oled_text (-1, 0, y, temp);
         y -= 10;
         sprintf (temp, "Alt: %6.2lfm   ", alt);
         oled_text (-1, 0, y, temp);
         double s = speed;
         if (mph)
            s /= 1.8;
         sprintf (temp, "%4.1lf", s);
         int x = oled_text (5, 0, 2, temp);
         oled_text (-1, x, 2, mph ? "mph" : "km/h");
         oled_unlock ();
      }
   }
}

void
app_main ()
{
   revk_init (&app_command);
#define b(n,d) revk_register(#n,0,sizeof(n),&n,#d,SETTING_BOOLEAN);
#define u32(n,d) revk_register(#n,0,sizeof(n),&n,#d,0);
#define s8(n,d) revk_register(#n,0,sizeof(n),&n,#d,SETTING_SIGNED);
#define u8(n,d) revk_register(#n,0,sizeof(n),&n,#d,0);
#define s(n,d) revk_register(#n,0,0,&n,d,0);
   settings
#undef u32
#undef s8
#undef u8
#undef b
#undef s
      if (oledsda >= 0 && oledscl >= 0)
      oled_start (1, oledaddress, oledscl, oledsda, oledflip);
   oled_set_contrast (oledcontrast);
   // Init UART
   uart_config_t uart_config = {
      .baud_rate = 9600,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
   };
   esp_err_t err;
   if ((err = uart_param_config (gpsuart, &uart_config)))
      revk_error (TAG, "UART param fail %s", esp_err_to_name (err));
   else if ((err = uart_set_pin (gpsuart, gpstx, gpsrx, -1, -1)))
      revk_error (TAG, "UART pin fail %s", esp_err_to_name (err));
   else if ((err = uart_driver_install (gpsuart, 256, 0, 0, NULL, 0)))
      revk_error (TAG, "UART install fail %s", esp_err_to_name (err));
   else
      revk_info (TAG, "PN532 UART %d Tx %d Rx %d", gpsuart, gpstx, gpsrx);
   if (gpsen >= 0)
   {                            // Enable
      gpio_set_level (gpsen, 1);
      gpio_set_direction (gpsen, GPIO_MODE_OUTPUT);
   }
   revk_task ("Display", display_task, NULL);
   // Main task...
   uint8_t buf[1000],
    *p = buf;
   while (1)
   {
      // Get line(s), the timeout should mean we see one or more whole lines typically
      int l = uart_read_bytes (gpsuart, p, buf + sizeof (buf) - p, 10);
      if (l < 0)
         sleep (1);
      if (l <= 0)
         continue;
      uint8_t *e = p + l;
      p = buf;
      while (p < e)
      {
         uint8_t *l = p;
         while (l < e && *l >= ' ')
            l++;
         if (l == e)
            break;
         if (*p == '$' && (l - p) >= 4 && l[-3] == '*' && isxdigit (l[-2]) && isxdigit (l[-1]))
         {
            // Checksum
            uint8_t c = 0,
               *x;
            for (x = p + 1; x < l - 3; x++)
               c ^= *x;
            if (((c >> 4) > 9 ? 7 : 0) + (c >> 4) + '0' != l[-2] || ((c & 0xF) > 9 ? 7 : 0) + (c & 0xF) + '0' != l[-1])
               revk_error (TAG, "[%.*s] (%02X)", l - p, p, c);
            else
            {                   // Process line
               l[-3] = 0;
               nmea ((char *) p);
            }
         } else
            revk_error (TAG, "[%.*s]", l - p, p);
         while (l < e && *l < ' ')
            l++;
         p = l;
      }
      if (p < e && (e - p) < sizeof (buf))
      {                         // Partial line
         memmove (buf, p, e - p);
         p = buf + (e - p);
         continue;
      }
      p = buf;                  // Start from scratch
   }
}
