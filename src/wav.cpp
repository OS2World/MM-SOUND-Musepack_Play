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

#define INCL_DOS
#include <os2.h>
#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <memory.h>

#include <unistd.h>
#include <glob.h>
#include <sys/time.h>

#include <math.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stdint.h>

#include "mpcdec.h"
#include "wav.h"

/*
  Our implementations of the mpc_reader callback functions.
*/
mpc_int32_t
read_impl(void *data, void *ptr, mpc_int32_t size)
{
    reader_data *d = (reader_data *) data;
    return fread(ptr, 1, size, d->file);
}

mpc_bool_t
seek_impl(void *data, mpc_int32_t offset)
{
    reader_data *d = (reader_data *) data;
    return d->seekable ? !fseek(d->file, offset, SEEK_SET) : false;
}

mpc_int32_t
tell_impl(void *data)
{
    reader_data *d = (reader_data *) data;
    return ftell(d->file);
}

mpc_int32_t
get_size_impl(void *data)
{
    reader_data *d = (reader_data *) data;
    return d->size;
}

mpc_bool_t
canseek_impl(void *data)
{
    reader_data *d = (reader_data *) data;
    return d->seekable;
}

#ifdef MPC_FIXED_POINT
static int
shift_signed(MPC_SAMPLE_FORMAT val, int shift)
{
    if (shift > 0)
        val <<= shift;
    else if (shift < 0)
        val >>= -shift;
    return (int)val;
}
#endif

WAV::WAV()
{
    reader.read = read_impl;
    reader.seek = seek_impl;
    reader.tell = tell_impl;
    reader.get_size = get_size_impl;
    reader.canseek = canseek_impl;
    reader.data = &data;

    data.file=NULL;
    sample_buffer=NULL;
    writer = NULL;
    inited = 0;
	}

WAV::~WAV()
{
	close();
}

int WAV::open(char *filename, int &samplerate,
              int &channels, int &bits, int &format)
{
    m_nPos = 0;
    m_nRemain = 0;


    FILE *input = fopen(filename, "rb");
    if (input == 0) {
        return NO_WAV_FILE;
    }

    /* initialize our reader_data tag the reader will carry around with it */
    data.file = input;
    data.seekable = true;
    fseek(data.file, 0, SEEK_END);
    data.size = ftell(data.file);
    fseek(data.file, 0, SEEK_SET);

    /* read file's streaminfo data */
    mpc_streaminfo_init(&info);
    if (mpc_streaminfo_read(&info, &reader) != ERROR_CODE_OK) {
        return NO_WAV_FILE;
    }

    samplerate = info.sample_freq;
    channels = mpc_chn; //info.channels;
    bits = mpc_bps; //info.bitrate;
    format = WAVE_FORMAT_PCM;
	samples=mpc_streaminfo_get_length_samples(&info);

    return 0;
}

int WAV::init()
{
    writer=new WavWriter(mpc_chn, mpc_bps, info.sample_freq);
    if (!sample_buffer)
    	sample_buffer=new MPC_SAMPLE_FORMAT[MPC_DECODER_BUFFER_LENGTH];
	mpc_decoder_setup(&decoder, &reader);
    vbr_acc=vbr_bits=0;
	return (inited=mpc_decoder_initialize(&decoder, &info));
}

int WAV::close()
{
	if(data.file) {
		fclose(data.file);
		data.file=NULL;
	}
    if (sample_buffer) {
        delete [] sample_buffer;
        sample_buffer=NULL;
        }
    if (writer) {
    	delete writer;
    	writer=NULL;
    	}
    inited = 0;
	return 0;
}

int WAV::readData(char *buffer, int bytes)
{
	if (!inited) return -1;

	int total = 0;
	int needsamples=bytes / (mpc_bps>>2);
    writer->setbuf(buffer);

    while(needsamples > 0) {
		if (m_nRemain) {

			uint32_t n = ( needsamples < m_nRemain )?needsamples:m_nRemain;
			uint32_t b = n<<1;
			writer->WriteSamples(sample_buffer+m_nPos,b);

			m_nRemain 	-= n;
			needsamples -= n;
            m_nPos    	+= b;
            b  *= (mpc_bps >> 3);

			buffer    += b;
			total     += b;
			}
		else {
			m_nPos = 0;
			if (total==0)
			    vbr_acc=vbr_bits=0;

    		if ( uint32_t(-1) == (m_nRemain = mpc_decoder_decode(&decoder, sample_buffer, &vbr_acc, &vbr_bits))) {
    			break;
    			}
    		if (m_nRemain==0)
    			break;
			}
    	}
	return total;
	}

int WAV::filepos()
{
    int r=0;
	if(inited) {
        r=decoder.SampleRate?(uint32_t)((double)(decoder.DecodedFrames*MPC_FRAME_LENGTH-m_nRemain)*1000/decoder.SampleRate):0;
	}
	return r;
}

int WAV::filelength()
{
	int r=0;
	if(inited) {
		r=decoder.SampleRate?(uint32_t)((double)decoder.OverallFrames*MPC_FRAME_LENGTH*1000/decoder.SampleRate):0;
	}
	if (r==0) {
		r=(samples>0&&info.sample_freq>0)?(uint32_t)((double)samples*1000/info.sample_freq):0;
	}
	return r;
}

int WAV::jumpto(long offset)
{
	int r=0;
	if(inited) {
		m_nRemain=0;
		r=mpc_decoder_seek_sample(&decoder, (mpc_int64_t)offset*info.sample_freq/1000);
		}
	return r;
}

int WAV::skip(long ms)
{
    if (!inited) return -1;

	long skipsamples=ms*info.sample_freq/1000;

    while(skipsamples > 0) {
        if (m_nRemain) {
            uint32_t n = ( skipsamples < m_nRemain )?skipsamples:m_nRemain;
            m_nRemain -= n;
            skipsamples -= n;
            }
        else {
            m_nPos = 0;

            if ( uint32_t(-1) == (m_nRemain = mpc_decoder_decode(&decoder, sample_buffer, 0, 0))) {
                break;
                }
            if (m_nRemain==0)
                break;
            }
        }
    return skipsamples;
    }

