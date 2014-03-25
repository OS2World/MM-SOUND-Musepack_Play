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

#ifndef __WAV_H_INCLUDED_
#define __WAV_H_INCLUDED_

/* AFAIK, all of those also have BitsPerSample as format specific */
#define WAVE_FORMAT_PCM       0x0001
#define WAVE_FORMAT_ADPCM     0x0002
#define WAVE_FORMAT_ALAW      0x0006
#define WAVE_FORMAT_MULAW     0x0007
#define WAVE_FORMAT_OKI_ADPCM 0x0010
#define WAVE_FORMAT_DIGISTD   0x0015
#define WAVE_FORMAT_DIGIFIX   0x0016
#define IBM_FORMAT_MULAW      0x0101
#define IBM_FORMAT_ALAW       0x0102
#define IBM_FORMAT_ADPCM      0x0103

#define NO_WAV_FILE 200


/*
  The data bundle we pass around with our reader to store file
  position and size etc.
*/
typedef struct reader_data_t {
    FILE *file;
    long size;
    mpc_bool_t seekable;
} reader_data;

#define WFX_SIZE (2+2+4+4+2+2)

class WavWriter {
  public:
    WavWriter(unsigned p_nch, unsigned p_bps,
              unsigned p_srate)
    : m_nch(p_nch), m_bps(p_bps), m_srate(p_srate) {
//        assert(m_bps == 16 || m_bps == 24);
//        m_data_bytes_written = 0;
    }

    void setbuf(char* buf) {
         ourbuf=buf;
        }

    mpc_bool_t WriteSamples(const MPC_SAMPLE_FORMAT * p_buffer, unsigned p_size) {
        unsigned n;
        int clip_min = -1 << (m_bps - 1),
            clip_max = (1 << (m_bps - 1)) - 1, float_scale = 1 << (m_bps - 1);
        for (n = 0; n < p_size; n++) {
            int val;
#ifdef MPC_FIXED_POINT
            val =
                shift_signed(p_buffer[n],
                             m_bps - MPC_FIXED_POINT_SCALE_SHIFT);
#else
            val = (int)(p_buffer[n] * float_scale);
#endif
            if (val < clip_min)
                val = clip_min;
            else if (val > clip_max)
                val = clip_max;
            if (!WriteInt(val, m_bps))
                return false;
        }
        //m_data_bytes_written += p_size * (m_bps >> 3);
        return true;
    }

    ~WavWriter() {
/*
        if (m_data_bytes_written & 1) {
            char blah = 0;
            WriteRaw(&blah, 1);
            m_data_bytes_written++;
        }
        Seek(4);
        WriteDword((unsigned long)(m_data_bytes_written + 4 + 8 + WFX_SIZE +
                                   8));
        Seek(8 + 4 + 8 + WFX_SIZE + 4);
        WriteDword(m_data_bytes_written);
*/
    }

  private:

/*    mpc_bool_t Seek(unsigned p_offset) {
        return !fseek(m_file, p_offset, SEEK_SET);
    }
*/
    void WriteRaw(const void *p_buffer, unsigned p_bytes) {
        memcpy(ourbuf,p_buffer,p_bytes);
        ourbuf+=p_bytes;
    }

/*
    mpc_bool_t WriteDword(unsigned long p_val) {
        return WriteInt(p_val, 32);
    }
    mpc_bool_t WriteWord(unsigned short p_val) {
        return WriteInt(p_val, 16);
    }
*/
    // write a little-endian number properly
    mpc_bool_t WriteInt(unsigned int p_val, unsigned p_width_bits) {
        unsigned char temp;
        unsigned shift = 0;
//        assert((p_width_bits % 8) == 0);
        do {
            temp = (unsigned char)((p_val >> shift) & 0xFF);
            WriteRaw(&temp, 1);
            shift += 8;
        } while (shift < p_width_bits);
        return true;
    }

    unsigned m_nch, m_bps, m_srate;
    char* ourbuf;
//    FILE *m_file;
//    unsigned m_data_bytes_written;
};

/* definitions for id3tag.cpp */
#ifndef FILE_T
#define FILE_T FILE*
#endif
#ifndef OFF_T
#define OFF_T signed long
#endif
typedef signed int Int;

typedef struct {
    OFF_T         FileSize;
    Int           GenreNo;
    Int           TrackNo;
    char          Genre   [128];
    char          Year    [ 20];
    char          Track   [  8];
    char          Title   [256];
    char          Artist  [256];
    char          Album   [256];
    char          Comment [512];
    } TagInfo_t;

/* */


Int Read_APE_Tags ( FILE_T fp, TagInfo_t* tip );
Int Read_ID3V1_Tags ( FILE_T fp, TagInfo_t* tip );

//const long int samples_to_unpack=4096L;
class WAV
{
	reader_data     data;
	mpc_streaminfo  info;
	mpc_reader 		reader;
    mpc_decoder     decoder;
    WavWriter*		writer;

	uint32_t m_nRemain;
	uint32_t m_nPos;
	//int bps, channels,
	uint32_t samples;
	int inited;
	uint32_t vbr_acc,vbr_bits;

	MPC_SAMPLE_FORMAT *sample_buffer;

   public:

      WAV();
      ~WAV();
      int open(char *filename, int &samplerate,
               int &channels, int &bits, int &format);
      int init();
      int close();
      int readData(char *buffer, int bytes);
      int filepos();
      int jumpto(long offset);
      int skip(long ms);
      int filelength();

      int tags(TagInfo_t &taginfo) {
        return Read_ID3V1_Tags ( data.file, &taginfo )  ||  Read_APE_Tags ( data.file, &taginfo );
    	}
      long vbr() {
        return long(vbr_acc?((double)vbr_bits*info.sample_freq/vbr_acc/1000/1000):0.0);
    	}
};

const int mpc_bps=16;
const int mpc_chn=2;

#endif // __WAV_H_INCLUDED_

