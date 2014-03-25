/*
 * Copyright 1997-2003 Samuel Audet <guardia@step.polymtl.ca>
 *                     Taneli Lepp„ <rosmo@sektori.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *    3. The name of the author may not be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* The Monkey's Audio Codec player plug-in for PM123 */
/*** (obsolette) ***/

/* The WavPack Hybrid Lossless Audio Compression player plug-in for PM123 */

#define INCL_DOS
#define INCL_PM
#include <os2.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include <glob.h>
#include <sys/time.h>

#include <math.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>



#include "format.h"
#include "decoder_plug.h"
#include "plugin.h"
#include "mpcdec.h"
#include "wav.h"


typedef struct
{
   WAV *wavfile;
   FORMAT_INFO formatinfo;

   int (* _System output_play_samples)(void *a, FORMAT_INFO *format,char *buf,int len, int posmarker);
   void *a; /* only to be used with the precedent function */
   int buffersize;

   HEV play,ok;
   char filename[1024];
   BOOL stop,rew,ffwd;
   int jumpto;
   int status;

   ULONG decodertid;

   void (* _System error_display)(char *);
   HEV playsem;
   HWND hwnd;

   int last_length; // keeps the last length in memory to calls to
                    // decoder_length() remain valid after the file is closed
} WAVPLAY;


static void decoder_thread(void *arg)
{
   WAVPLAY *w = (WAVPLAY *) arg;
   ULONG resetcount;

   while(1)
   {
      char *buffer = NULL;
      int read = 0;

      DosWaitEventSem(w->play, (ULONG)-1);
      DosResetEventSem(w->play,&resetcount);

      w->status = DECODER_STARTING;
      buffer = (char*)malloc(w->buffersize);

      w->last_length = -1;

      DosResetEventSem(w->playsem,&resetcount);
      DosPostEventSem(w->ok);

      if(w->wavfile->open(w->filename,w->formatinfo.samplerate,
            w->formatinfo.channels,w->formatinfo.bits,w->formatinfo.format))
      {
         WinPostMsg(w->hwnd,WM_PLAYERROR,0,0);
         w->status = DECODER_STOPPED;
         DosPostEventSem(w->playsem);
         continue;
      }

      if (w->wavfile->init()==0)
      {
         WinPostMsg(w->hwnd,WM_PLAYERROR,0,0);
         w->status = DECODER_STOPPED;
         DosPostEventSem(w->playsem);
         continue;
      }

      w->jumpto = -1;
      w->rew = w->ffwd = 0;
      w->stop = 0;

      w->status = DECODER_PLAYING;
      w->last_length = decoder_length(w);

	  long vbr=w->wavfile->vbr();
      while((read = w->wavfile->readData(buffer,w->buffersize)) > 0 && !w->stop)
      {
         int written = w->output_play_samples(w->a, &w->formatinfo, buffer, read, (int)w->wavfile->filepos() );

         if(written < read)
         {
            WinPostMsg(w->hwnd,WM_PLAYERROR,0,0);
            break;
         }

         long _vbr=w->wavfile->vbr();
         if (vbr!=_vbr) {
         	vbr=_vbr;
            WinPostMsg(w->hwnd,WM_CHANGEBR,MPFROMLONG(vbr),0);
         }

         if(w->jumpto >= 0)
         {
            w->wavfile->jumpto(w->jumpto);
            w->jumpto = -1;
            WinPostMsg(w->hwnd,WM_SEEKSTOP,0,0);
         }
         if(w->ffwd)
         {
            w->wavfile->skip(1000);
         }
      }
      free(buffer); buffer = NULL;
      w->status = DECODER_STOPPED;
      w->wavfile->close();

      DosPostEventSem(w->playsem);
      WinPostMsg(w->hwnd,WM_PLAYSTOP,0,0);

      DosPostEventSem(w->ok);
   }
}

int _System decoder_init(void **W)
{
   WAVPLAY *w;

   *W = malloc(sizeof(WAVPLAY));
   w=(WAVPLAY *)*W;
   memset(w,0,sizeof(WAVPLAY));
   w->wavfile=new WAV;

   DosCreateEventSem(NULL,&w->play,0,FALSE);
   DosCreateEventSem(NULL,&w->ok,0,FALSE);

   w->decodertid = _beginthread(decoder_thread,0,64*1024,(void *) w);
   if(w->decodertid != -1)
      return w->decodertid;
   else
   {
      DosCloseEventSem(w->play);
      DosCloseEventSem(w->ok);
      free(w);
      return -1;
   }
}

BOOL _System decoder_uninit(void *W)
{
   WAVPLAY *w = (WAVPLAY *) W;
   int decodertid = w->decodertid;

   DosCloseEventSem(w->play);
   DosCloseEventSem(w->ok);

   free(w);

   return !DosKillThread(decodertid);
}


ULONG _System decoder_command(void *W, ULONG msg, DECODER_PARAMS *params)
{
   WAVPLAY *w = (WAVPLAY *) W;
   ULONG resetcount;

   switch(msg)
   {
      case DECODER_PLAY:
         if(w->status == DECODER_STOPPED)
         {
            strncpy(w->filename, params->filename,sizeof(w->filename)-1);
            DosResetEventSem(w->ok,&resetcount);
            DosPostEventSem(w->play);
            if(DosWaitEventSem(w->ok, 10000) == 640)
            {
               w->status = DECODER_STOPPED;
               DosKillThread(w->decodertid);
               w->decodertid = _beginthread(decoder_thread,0,64*1024,(void *) w);
               return 102;
            }
         }
         else
            return 101;
         break;

      case DECODER_STOP:
         if(w->status != DECODER_STOPPED)
         {
            DosResetEventSem(w->ok,&resetcount);
            w->stop = TRUE;
            if(DosWaitEventSem(w->ok, 10000) == 640)
            {
               w->status = DECODER_STOPPED;
               DosKillThread(w->decodertid);
               w->decodertid = _beginthread(decoder_thread,0,64*1024,(void *) w);
               return 102;
            }
         }
         else
            return 101;
         break;

      case DECODER_FFWD:
         w->ffwd = params->ffwd; //?10:0;
         break;

      case DECODER_REW:
         //w->rew = params->rew;
         break; // seek is too costly

      /* I multiply by the channels and bits last because I need to fall on
         a byte which is a multiple of those or else I'll get garbage */
      case DECODER_JUMPTO:
         w->jumpto = params->jumpto;
         break;

      case DECODER_EQ:
         return 1;

      case DECODER_SETUP:
         w->output_play_samples = params->output_play_samples;
         w->a = params->a;
         w->buffersize = params->audio_buffersize;
         w->error_display = params->error_display;
         w->hwnd = params->hwnd;
         w->playsem = params->playsem;
         DosPostEventSem(w->playsem);
         break;
   }
   return 0;
}

ULONG _System decoder_length(void *W)
{
   WAVPLAY *w = (WAVPLAY *) W;

   if(w->status == DECODER_PLAYING)
      w->last_length = (int)w->wavfile->filelength();

   if (w->last_length<0) w->last_length=0;
   return w->last_length;
}

ULONG _System decoder_status(void *W)
{
   WAVPLAY *w = (WAVPLAY *) W;
   return w->status;
}

ULONG _System decoder_fileinfo(char *filename, DECODER_INFO *info)
{
   memset(info,0,sizeof(*info));
   info->size = sizeof(*info);
   info->mpeg = 0;
   info->numchannels = 0;

   TagInfo_t taginfo;

   struct info_item {
    	char* s;
    	int size;
    	//char* name;
    	char* from; } song_infos[]={
            { info->title, 	128, /* "title", */ taginfo.Title},
    		{ info->artist, 128, /* "artist", */ taginfo.Artist},
            { info->album, 	128, /* "album", */ taginfo.Album},
            { info->year, 	128, /* "year", */ taginfo.Year},
//            { info->comment,128, "comment", */ taginfo.Comment},
            { info->genre, 128, /* "genre", */ taginfo.Genre}
//            ,{ info->tech_info, 128, "CUESHEET", */taginfo.}
            };

    WAV w;
    char sbuf[32];

    int r=w.open(filename,
    	info->format.samplerate,
        info->format.channels,
        info->format.bits,
        info->format.format);
	if (r==0) {
		info->songlength=w.filelength();
		snprintf( info->tech_info, sizeof(info->tech_info), "%d bits, %.1f kHz, %s",
			info->format.bits,(float) info->format.samplerate / 1000.0f,
			info->format.channels == 1 ? "Mono" : info->format.channels == 2 ? "Stereo" : _ltoa(info->format.channels,sbuf,10) );

		if (w.tags(taginfo)) {
			for (int i=0; i<sizeof(song_infos)/sizeof(song_infos[0]); i++) {
				strncpy(song_infos[i].s,song_infos[i].from,song_infos[i].size-1);
				song_infos[i].s[song_infos[i].size-1]='\0';
				}
            if (info->title && info->title[0])
                strcpy(info->comment,info->title);
            else
                {
                char* s=strchr(filename,'/');
                strncpy(info->comment,s&&*s?s+1:filename,128);
                info->comment[127]='\0';
                }
			}
		}
	return r;
}

ULONG _System decoder_trackinfo(char *drive, int track, DECODER_INFO *info)
{
   return 200;
}

ULONG _System decoder_cdinfo(char *drive, DECODER_CDINFO *info)
{
   return 100;
}


ULONG _System decoder_support(char *ext[], int *size)
{
   if(size)
   {
      if(ext != NULL)
      {
         if (*size >= 1) strcpy(ext[0],"*.mpc");
         if (*size >= 2) strcpy(ext[1],"*.mp+");
      }
      *size = 2;
   }
   return DECODER_FILENAME;
}

void _System plugin_query(PLUGIN_QUERYPARAM *param)
{
   param->type = PLUGIN_DECODER;
   param->author = "ntim";
   param->desc = "Musepack play 0.00";
   param->configurable = FALSE;
}

