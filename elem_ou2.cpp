/* elem_ou2.cpp: formatting elements into SOF (Standard Orbit Format)

Copyright (C) 2016, Project Pluto

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301, USA.    */

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "watdefs.h"
#include "comets.h"
#include "afuncs.h"
#include "mpc_obs.h"
#include "monte0.h"     /* for put_double_in_buff() proto */
#include "date.h"

#ifdef _MSC_VER   /* MSVC/C++ lacks snprintf.  See 'ephem0.cpp' for details. */
int snprintf( char *string, const size_t max_len, const char *format, ...);
#endif
int add_sof_to_file( const char *filename,         /* elem_ou2.cpp */
             const ELEMENTS *elem,
             const int n_obs, const OBSERVE *obs);
FILE *fopen_ext( const char *filename, const char *permits);   /* miscell.cpp */

const double PI = 3.1415926535897932384626433832795028841971693993751058209749445923;
const double jd_1970 = 2440587.5;

int64_t nanoseconds_since_1970( void);                      /* mpc_obs.c */

int put_comet_data_into_sof( char *obuff, const char *templat,
         const ELEMENTS *elem,
         const int n_obs, const OBSERVE *obs)
{
   int rval = 0;

   while( *templat >= ' ')
      {
      size_t i = 0;
      int j;
      double val_to_put = 0.;
      double date_to_put = 0.;
      double angle_to_put = 0.;
      int integer_to_put = 0;
      const char *text_to_put = NULL;
      char tbuff[80];

      while( templat[i] >= ' ' && templat[i] != '|')
         i++;

      if( *templat == 'N')
         text_to_put = obs->packed_id;
      else if( *templat == 'n' && templat[1] == '_')
         {
         switch( templat[2])
            {
            case 'o':            /* n_obs:  total observations,  minus duplicates */
               for( j = 0; j < n_obs; j++)
                  if( !(obs[j].flags & OBS_DONT_USE))
                     integer_to_put++;
               break;
            case 'u':            /* n_used:  number actually in the orbit fit */
               for( j = 0; j < n_obs; j++)
                  if( !(obs[j].flags & OBS_DONT_USE) && obs[j].is_included)
                     integer_to_put++;
               break;
            }
         }
      else if( templat[1] == ' ')    /* single-char specifier */
         switch( *templat)
            {
            case 'q':
               val_to_put = elem->q;
               break;
            case 'a':
               val_to_put = elem->major_axis;
               break;
            case 'Q':
               val_to_put = 2. * elem->major_axis - elem->q;
               break;
            case 'e':
               val_to_put = elem->ecc;
               break;
            case 'H':
               val_to_put = elem->abs_mag;
               break;
            case 'G':
               val_to_put = elem->slope_param;
               break;
            case 'i':
               angle_to_put = elem->incl;
               break;
            case 'C':
               integer_to_put = elem->central_obj;
            default:
               break;
            }
      else if( *templat == 'T')
         switch( templat[1])
            {
            case 'p':
               date_to_put = elem->perih_time;
               break;
            case 'e':
               date_to_put = elem->epoch;
               break;
            case 'f':         /* Tfirst */
            case 'l':         /* Tlast  */
               for( j = 0; j < n_obs; j++)
                  if( !(obs[j].flags & OBS_DONT_USE) && obs[j].is_included)
                     {
                     date_to_put = obs[j].jd;
                     if( templat[1] == 'f')     /* stop on first valid obs */
                        j = n_obs;
                     }
               break;
            case 'w':        /* Twritten */
               date_to_put = jd_1970 +
                        (double)nanoseconds_since_1970( ) * 1e-9 / seconds_per_day;
               break;
            default:
               break;
            }
      else if( templat[0] == 'M' && (templat[1] == 'T' || templat[1] == 'N'))
         {
         extern double comet_total_magnitude, comet_nuclear_magnitude;

         val_to_put = (templat[1] == 'T' ?
                      comet_total_magnitude : comet_nuclear_magnitude);
         }
      else if( templat[0] == 'r')
         val_to_put = compute_rms( obs, n_obs);
      else if( templat[0] == 'M' && templat[1] == 'A')
         angle_to_put = elem->mean_anomaly;
      else if( templat[0] == 'O' && templat[1] == 'm')
         angle_to_put = elem->asc_node;
      else if( templat[0] == 'o' && templat[1] == 'm')
         angle_to_put = elem->arg_per;
      else if( templat[0] == 'A' && templat[1] >= '1' && templat[1] <= '3')
         {
         extern double solar_pressure[];

         text_to_put = put_double_in_buff( tbuff,
                                       solar_pressure[templat[1] - '1']);
         }
      else        /* couldn't puzzle out what value we're storing; */
         rval++;  /* increment the error count */

      if( angle_to_put)
         {
         val_to_put = fmod( angle_to_put * 180. / PI, 360.);
         if( val_to_put < 0.)
            val_to_put += 360.;
         }
      if( date_to_put)
         {
         text_to_put = tbuff;
         full_ctime( tbuff, date_to_put, FULL_CTIME_YMD
                  | FULL_CTIME_LEADING_ZEROES
                  | FULL_CTIME_NO_SPACES | FULL_CTIME_FORMAT_DAY
                  | FULL_CTIME_11_PLACES | FULL_CTIME_MONTHS_AS_DIGITS);
         }
      if( text_to_put)
         snprintf( obuff, i + 1, "%*s", (int)i, text_to_put);
      else if( val_to_put)
         {
         size_t j = 1;

         while( j < i && templat[j] != '.')
            j++;
         if( j == i)       /* no decimal point found */
            j = 0;
         snprintf( obuff, i + 1, "%*.*f", (int)i, (int)(i - j) - 1, val_to_put);
         }
      else if( integer_to_put)
         snprintf( obuff, i + 1, "%*d", (int)i, integer_to_put);
      else
         memset( obuff, ' ', i);
      templat += i;
      obuff += i;
      if( *templat == '|')      /* more fields */
         {
         templat++;
         *obuff++ = ' ';
         }
      }
   strcpy( obuff, templat);     /* CR/LF or LF */
   return( rval);    /* indicates number of failed fields */
}

#define MAX_SOF_LEN 400

char *get_file_name( char *filename, const char *template_file_name);

int add_sof_to_file( const char *filename,
             const ELEMENTS *elem,
             const int n_obs, const OBSERVE *obs)
{
   char templat[MAX_SOF_LEN], obuff[MAX_SOF_LEN];
   char output_filename[100];
   FILE *fp;
   int rval = -1, forking;

   fp = fopen_ext( filename, "ca+b");
   get_file_name( output_filename, filename);
   forking = strcmp( output_filename, filename);
   if( fp)
      {
      fseek( fp, 0L, SEEK_SET);
      if( !fgets( templat, sizeof( templat), fp))
         assert( 1);
      if( forking)
         {
         fclose( fp);
         fp = fopen_ext( output_filename, "ca+b");
         assert( fp);
         }
      fseek( fp, 0L, SEEK_END);
      rval = put_comet_data_into_sof( obuff, templat, elem, n_obs, obs);
      fwrite( obuff, strlen( obuff), 1, fp);
      fclose( fp);
      }
   return( rval);
}
