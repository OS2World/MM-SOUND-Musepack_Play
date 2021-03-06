/*
 * Musepack audio compression
 * Copyright (C) 1999-2004 Buschmann/Klemm/Piecha/Wolf
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

#include <limits.h>

/*
 *  List of known Genres. 256 genres are possible with version 1/1.1 tags,
 *  but not yet used.
 */

typedef unsigned char       Uint8_t;    // guaranteed  8 bit unsigned integer type with range 0...255
typedef signed   char       Int8_t;     // guaranteed  8 bit signed   integer type with range -128...127
#define FILEPOS(fp)            ftell  (fp)                  // FILEPOS returns -1 on error, otherwise >= 0
#define SEEK(fp,offs,lbl)      fseek  (fp, offs, lbl)       // SEEK    returns -1 on error, otherwise >= 0
#define READ(fp,ptr,len)       fread  (ptr, 1, len, fp)     // READ    returns -1 or 0 on error/EOF, otherwise > 0

#if   __INT_MAX == 2147483647L
typedef unsigned int        Uint32_t;   // guaranteed 32 bit unsigned integer type with range 0...4294967295
typedef signed   int        Int32_t;    // guaranteed 32 bit signed   integer type with range -2147483648...2147483647
#elif __LONG_MAX == 2147483647L
typedef unsigned long int   Uint32_t;   // guaranteed 32 bit unsigned integer type with range 0...4294967295
typedef signed   long int   Int32_t;    // guaranteed 32 bit signed   integer type with range -2147483648...2147483647
#else
#ifndef INT_MAX
# error INT_MAX not defined!
#endif
#ifndef LONG_MAX
# error LONG_MAX not defined!
#endif
# error No 32 bit int type found. Tested: int, long
#endif

static const char*  GenreList [] = {
    "Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge",
    "Hip-Hop", "Jazz", "Metal", "New Age", "Oldies", "Other", "Pop", "R&B",
    "Rap", "Reggae", "Rock", "Techno", "Industrial", "Alternative", "Ska",
    "Death Metal", "Pranks", "Soundtrack", "Euro-Techno", "Ambient",
    "Trip-Hop", "Vocal", "Jazz+Funk", "Fusion", "Trance", "Classical",
    "Instrumental", "Acid", "House", "Game", "Sound Clip", "Gospel", "Noise",
    "AlternRock", "Bass", "Soul", "Punk", "Space", "Meditative",
    "Instrumental Pop", "Instrumental Rock", "Ethnic", "Gothic", "Darkwave",
    "Techno-Industrial", "Electronic", "Pop-Folk", "Eurodance", "Dream",
    "Southern Rock", "Comedy", "Cult", "Gangsta", "Top 40", "Christian Rap",
    "Pop/Funk", "Jungle", "Native American", "Cabaret", "New Wave",
    "Psychadelic", "Rave", "Showtunes", "Trailer", "Lo-Fi", "Tribal",
    "Acid Punk", "Acid Jazz", "Polka", "Retro", "Musical", "Rock & Roll",
    "Hard Rock", "Folk", "Folk/Rock", "National Folk", "Swing", "Fast-Fusion",
    "Bebob", "Latin", "Revival", "Celtic", "Bluegrass", "Avantgarde",
    "Gothic Rock", "Progressive Rock", "Psychedelic Rock", "Symphonic Rock",
    "Slow Rock", "Big Band", "Chorus", "Easy Listening", "Acoustic", "Humour",
    "Speech", "Chanson", "Opera", "Chamber Music", "Sonata", "Symphony",
    "Booty Bass", "Primus", "Porn Groove", "Satire", "Slow Jam", "Club",
    "Tango", "Samba", "Folklore", "Ballad", "Power Ballad", "Rhythmic Soul",
    "Freestyle", "Duet", "Punk Rock", "Drum Solo", "A capella", "Euro-House",
    "Dance Hall", "Goa", "Drum & Bass", "Club House", "Hardcore", "Terror",
    "Indie", "BritPop", "NegerPunk", "Polsk Punk", "Beat", "Christian Gangsta",
    "Heavy Metal", "Black Metal", "Crossover", "Contemporary C",
    "Christian Rock", "Merengue", "Salsa", "Thrash Metal", "Anime", "JPop",
    "SynthPop"
};


/*
 *  Copies src to dst. Copying is stopped at `\0' char is detected or if
 *  len chars are copied.
 *  Trailing blanks are removed and the string is `\0` terminated.
 */

static void
memcpy_crop ( char* dst, /*const char*/ Uint8_t* src, size_t len )
{
    size_t  i;

    for ( i = 0; i < len; i++ )
        if  ( src[i] != '\0' )
            dst[i] = src[i];
        else
            break;

    // dst[i] points behind the string contents

    while ( i > 0  &&  dst [i-1] == ' ' )
        i--;
    dst [i] = '\0';
}


/*
 *  Evaluate ID version 1/1.1 tags of a file given by 'fp' and fills out Tag
 *  information in 'tip'. Tag information also contains the effective file
 *  length (without the tags if tags are present). Return 1 if there is
 *  usable information inside the file. Note there's also the case possible
 *  that the file contains empty tags, the the file size is truncated by the
 *  128 bytes but the function returns 0.
 *
 *  If there're no tags, all strings containing '\0', the Genre pointer is
 *  NULL and GenreNo and TrackNo are -1.
 */

Int
Read_ID3V1_Tags ( FILE_T fp, TagInfo_t* tip )
{
    Uint8_t  tmp [128];
    OFF_T    file_pos;

    memset ( tip, 0, sizeof(*tip) );
    tip->GenreNo = -1;
    tip->TrackNo = -1;

    if ( -1 == (file_pos = FILEPOS (fp)) )
        return 0;
    if ( -1 == SEEK ( fp, -128L, SEEK_END ) )
        return 0;

    tip->FileSize = FILEPOS (fp);
    if ( 128 != READ ( fp, tmp, 128 ) )
        return 0;
    SEEK ( fp, file_pos, SEEK_SET );

    if ( 0 != memcmp ( tmp, "TAG", 3 ) ) {
        tip->FileSize += 128;
        return 0;
    }

    if ( !tmp[3]  &&  !tmp[33]  &&  !tmp[63]  &&  !tmp[93]  &&  !tmp[97] )
        return 0;

    memcpy_crop  ( tip->Title  , tmp +  3, 30 );
    memcpy_crop  ( tip->Artist , tmp + 33, 30 );
    memcpy_crop  ( tip->Album  , tmp + 63, 30 );
    memcpy_crop  ( tip->Year   , tmp + 93,  4 );
    memcpy_crop  ( tip->Comment, tmp + 97, 30 );

    strcpy ( tip->Genre, tmp[127] < sizeof(GenreList)/sizeof(*GenreList)  ?
                         GenreList [tip->GenreNo = tmp[127]]  :  "???" );

    // Index 0 may be true if file is very short
    if ( tmp[125] == 0  &&  (tmp[126] != 0  ||  tip->FileSize < 66000 ) )
        sprintf ( tip->Track, "[%02d]", tip->TrackNo = tmp[126] );
    else
        strcpy ( tip->Track, "    " );

    return 1;
}


struct APETagFooterStruct {
    Uint8_t   ID       [8];    // should equal 'APETAGEX'
    Uint8_t   Version  [4];    // currently 1000 (version 1.000)
    Uint8_t   Length   [4];    // the complete size of the tag, including this footer
    Uint8_t   TagCount [4];    // the number of fields in the tag
    Uint8_t   Flags    [4];    // the tag flags (none currently defined)
    Uint8_t   Reserved [8];    // reserved for later use
};


static Uint32_t
Read_LE_Uint32 ( const Uint8_t* p )
{
    return ((Uint32_t)p[0] <<  0) |
           ((Uint32_t)p[1] <<  8) |
           ((Uint32_t)p[2] << 16) |
           ((Uint32_t)p[3] << 24);
}


#define TAG_ANALYZE(item,elem)                      \
    if ( 0 == memcmp (p, #item, sizeof #item ) ) {  \
        p += sizeof #item;                          \
        memcpy ( tip->elem, p, len );               \
        p += len;                                   \
    } else


Int
Read_APE_Tags ( FILE_T fp, TagInfo_t* tip )
{
    OFF_T                      file_pos;
    Uint32_t                   len;
    Uint32_t                   flags;
    unsigned char              buff [8192];
    unsigned char*             p;
    unsigned char*             end;
    struct APETagFooterStruct  T;
    Uint32_t                   TagLen;
    Uint32_t                   TagCount;
    Uint32_t                   tmp;

    memset ( tip, 0, sizeof(*tip) );
    tip->GenreNo = -1;
    tip->TrackNo = -1;

    if ( -1 == (file_pos = FILEPOS (fp)) )
        goto notag;
    if ( -1 == SEEK ( fp, 0L, SEEK_END ) )
        goto notag;
    tip->FileSize = FILEPOS (fp);
    if ( -1 == SEEK ( fp, -(long)sizeof T, SEEK_END ) )
        goto notag;
    if ( sizeof(T) != READ ( fp, &T, sizeof T ) )
        goto notag;
    if ( memcmp ( T.ID, "APETAGEX", sizeof(T.ID) ) != 0 )
        goto notag;
    tmp = Read_LE_Uint32 (T.Version);
    if (  tmp != 1000  &&  tmp != 2000 )
        goto notag;
    TagLen = Read_LE_Uint32 (T.Length);
    if ( TagLen <= sizeof T )
        goto notag;
    if ( -1 == SEEK ( fp, -(long)TagLen, SEEK_END ) )
        goto notag;
    tip->FileSize = FILEPOS (fp);
    memset ( buff, 0, sizeof(buff) );
    if ( TagLen - sizeof T != READ ( fp, buff, TagLen - sizeof T ) )
        goto notag;
    SEEK ( fp, file_pos, SEEK_SET );

    TagCount = Read_LE_Uint32 (T.TagCount);
    end = buff + TagLen - sizeof T;
    for ( p = buff; p < end  &&  TagCount--; ) {
        len   = Read_LE_Uint32 ( p ); p += 4;
        flags = Read_LE_Uint32 ( p ); p += 4;
        TAG_ANALYZE ( Title  , Title   )
        TAG_ANALYZE ( Album  , Album   )
        TAG_ANALYZE ( Artist , Artist  )
        TAG_ANALYZE ( Album  , Album   )
        TAG_ANALYZE ( Comment, Comment )
        TAG_ANALYZE ( Track  , Track   )
        TAG_ANALYZE ( Year   , Year    )
        TAG_ANALYZE ( Genre  , Genre   )
        {
            p += strlen((char*)p) + 1 + len;
        }
    }

    if ( tip->Track[0] != '\0' )
        sprintf ( tip->Track, "[%02d]", tip->TrackNo = atoi (tip->Track) );
    else
        strcpy ( tip->Track, "    " );

    /* genre is not yet entirely decoded */
    return 1;

notag:
    SEEK ( fp, file_pos, SEEK_SET );
    return 0;
}

/* end of id3tag.c */
