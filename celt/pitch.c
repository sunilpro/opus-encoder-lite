/* Copyright (c) 2007-2008 CSIRO
   Copyright (c) 2007-2009 Xiph.Org Foundation
   Written by Jean-Marc Valin */
/**
   @file pitch.c
   @brief Pitch analysis
 */

/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "pitch.h"
#include "os_support.h"
#include "modes.h"
#include "stack_alloc.h"
#include "mathops.h"
#include "celt_lpc.h"

static void find_best_pitch(opus_val32 *xcorr, opus_val16 *y, int len,
                            int max_pitch, int *best_pitch
#ifdef FIXED_POINT
                            , int yshift, opus_val32 maxcorr
#endif
                            )
{
   int i, j;
   opus_val32 Syy=1;
   opus_val16 best_num[2];
   opus_val32 best_den[2];
#ifdef FIXED_POINT
   int xshift;

   xshift = celt_ilog2(maxcorr)-14;
#endif

   best_num[0] = -1;
   best_num[1] = -1;
   best_den[0] = 0;
   best_den[1] = 0;
   best_pitch[0] = 0;
   best_pitch[1] = 1;
   for (j=0;j<len;j++)
      Syy = ADD32(Syy, SHR32(MULT16_16(y[j],y[j]), yshift));
   for (i=0;i<max_pitch;i++)
   {
      if (xcorr[i]>0)
      {
         opus_val16 num;
         opus_val32 xcorr16;
         xcorr16 = EXTRACT16(VSHR32(xcorr[i], xshift));
#ifndef FIXED_POINT
         /* Considering the range of xcorr16, this should avoid both underflows
            and overflows (inf) when squaring xcorr16 */
         xcorr16 *= 1e-12f;
#endif
         num = MULT16_16_Q15(xcorr16,xcorr16);
         if (MULT16_32_Q15(num,best_den[1]) > MULT16_32_Q15(best_num[1],Syy))
         {
            if (MULT16_32_Q15(num,best_den[0]) > MULT16_32_Q15(best_num[0],Syy))
            {
               best_num[1] = best_num[0];
               best_den[1] = best_den[0];
               best_pitch[1] = best_pitch[0];
               best_num[0] = num;
               best_den[0] = Syy;
               best_pitch[0] = i;
            } else {
               best_num[1] = num;
               best_den[1] = Syy;
               best_pitch[1] = i;
            }
         }
      }
      Syy += SHR32(MULT16_16(y[i+len],y[i+len]),yshift) - SHR32(MULT16_16(y[i],y[i]),yshift);
      Syy = MAX32(1, Syy);
   }
}

static void celt_fir5(const opus_val16 *x,
         const opus_val16 *num,
         opus_val16 *y,
         int N,
         opus_val16 *mem)
{
   int i;
   opus_val16 num0, num1, num2, num3, num4;
   opus_val32 mem0, mem1, mem2, mem3, mem4;
   num0=num[0];
   num1=num[1];
   num2=num[2];
   num3=num[3];
   num4=num[4];
   mem0=mem[0];
   mem1=mem[1];
   mem2=mem[2];
   mem3=mem[3];
   mem4=mem[4];
   for (i=0;i<N;i++)
   {
      opus_val32 sum = SHL32(EXTEND32(x[i]), SIG_SHIFT);
      sum = MAC16_16(sum,num0,mem0);
      sum = MAC16_16(sum,num1,mem1);
      sum = MAC16_16(sum,num2,mem2);
      sum = MAC16_16(sum,num3,mem3);
      sum = MAC16_16(sum,num4,mem4);
      mem4 = mem3;
      mem3 = mem2;
      mem2 = mem1;
      mem1 = mem0;
      mem0 = x[i];
      y[i] = ROUND16(sum, SIG_SHIFT);
   }
   mem[0]=mem0;
   mem[1]=mem1;
   mem[2]=mem2;
   mem[3]=mem3;
   mem[4]=mem4;
}

/* Pure C implementation. */
#ifdef FIXED_POINT
opus_val32
#else
void
#endif
celt_pitch_xcorr_c(const opus_val16 *_x, const opus_val16 *_y,
      opus_val32 *xcorr, int len, int max_pitch, int arch)
{

#if 0 /* This is a simple version of the pitch correlation that should work
         well on DSPs like Blackfin and TI C5x/C6x */
   int i, j;
#ifdef FIXED_POINT
   opus_val32 maxcorr=1;
#endif
#if !defined(OVERRIDE_PITCH_XCORR)
   (void)arch;
#endif
   for (i=0;i<max_pitch;i++)
   {
      opus_val32 sum = 0;
      for (j=0;j<len;j++)
         sum = MAC16_16(sum, _x[j], _y[i+j]);
      xcorr[i] = sum;
#ifdef FIXED_POINT
      maxcorr = MAX32(maxcorr, sum);
#endif
   }
#ifdef FIXED_POINT
   return maxcorr;
#endif

#else /* Unrolled version of the pitch correlation -- runs faster on x86 and ARM */
   int i;
   /*The EDSP version requires that max_pitch is at least 1, and that _x is
      32-bit aligned.
     Since it's hard to put asserts in assembly, put them here.*/
#ifdef FIXED_POINT
   opus_val32 maxcorr=1;
#endif
   celt_assert(max_pitch>0);
   celt_assert((((unsigned char *)_x-(unsigned char *)NULL)&3)==0);
   for (i=0;i<max_pitch-3;i+=4)
   {
      opus_val32 sum[4]={0,0,0,0};
      xcorr_kernel(_x, _y+i, sum, len, arch);
      xcorr[i]=sum[0];
      xcorr[i+1]=sum[1];
      xcorr[i+2]=sum[2];
      xcorr[i+3]=sum[3];
#ifdef FIXED_POINT
      sum[0] = MAX32(sum[0], sum[1]);
      sum[2] = MAX32(sum[2], sum[3]);
      sum[0] = MAX32(sum[0], sum[2]);
      maxcorr = MAX32(maxcorr, sum[0]);
#endif
   }
   /* In case max_pitch isn't a multiple of 4, do non-unrolled version. */
   for (;i<max_pitch;i++)
   {
      opus_val32 sum;
      sum = celt_inner_prod(_x, _y+i, len, arch);
      xcorr[i] = sum;
#ifdef FIXED_POINT
      maxcorr = MAX32(maxcorr, sum);
#endif
   }
#ifdef FIXED_POINT
   return maxcorr;
#endif
#endif
}

void pitch_search(const opus_val16 * OPUS_RESTRICT x_lp, opus_val16 * OPUS_RESTRICT y,
                  int len, int max_pitch, int *pitch, int arch)
{
   int i, j;
   int lag;
   int best_pitch[2]={0,0};
   VARDECL(opus_val16, x_lp4);
   VARDECL(opus_val16, y_lp4);
   VARDECL(opus_val32, xcorr);
#ifdef FIXED_POINT
   opus_val32 maxcorr;
   opus_val32 xmax, ymax;
   int shift=0;
#endif
   int offset;

   SAVE_STACK;

   celt_assert(len>0);
   celt_assert(max_pitch>0);
   lag = len+max_pitch;

   ALLOC(x_lp4, len>>2, opus_val16);
   ALLOC(y_lp4, lag>>2, opus_val16);
   ALLOC(xcorr, max_pitch>>1, opus_val32);

   /* Downsample by 2 again */
   for (j=0;j<len>>2;j++)
      x_lp4[j] = x_lp[2*j];
   for (j=0;j<lag>>2;j++)
      y_lp4[j] = y[2*j];

#ifdef FIXED_POINT
   xmax = celt_maxabs16(x_lp4, len>>2);
   ymax = celt_maxabs16(y_lp4, lag>>2);
   shift = celt_ilog2(MAX32(1, MAX32(xmax, ymax)))-11;
   if (shift>0)
   {
      for (j=0;j<len>>2;j++)
         x_lp4[j] = SHR16(x_lp4[j], shift);
      for (j=0;j<lag>>2;j++)
         y_lp4[j] = SHR16(y_lp4[j], shift);
      /* Use double the shift for a MAC */
      shift *= 2;
   } else {
      shift = 0;
   }
#endif

   /* Coarse search with 4x decimation */

#ifdef FIXED_POINT
   maxcorr =
#endif
   celt_pitch_xcorr(x_lp4, y_lp4, xcorr, len>>2, max_pitch>>2, arch);

   find_best_pitch(xcorr, y_lp4, len>>2, max_pitch>>2, best_pitch
#ifdef FIXED_POINT
                   , 0, maxcorr
#endif
                   );

   /* Finer search with 2x decimation */
#ifdef FIXED_POINT
   maxcorr=1;
#endif
   for (i=0;i<max_pitch>>1;i++)
   {
      opus_val32 sum;
      xcorr[i] = 0;
      if (abs(i-2*best_pitch[0])>2 && abs(i-2*best_pitch[1])>2)
         continue;
#ifdef FIXED_POINT
      sum = 0;
      for (j=0;j<len>>1;j++)
         sum += SHR32(MULT16_16(x_lp[j],y[i+j]), shift);
#else
      sum = celt_inner_prod(x_lp, y+i, len>>1, arch);
#endif
      xcorr[i] = MAX32(-1, sum);
#ifdef FIXED_POINT
      maxcorr = MAX32(maxcorr, sum);
#endif
   }
   find_best_pitch(xcorr, y, len>>1, max_pitch>>1, best_pitch
#ifdef FIXED_POINT
                   , shift+1, maxcorr
#endif
                   );

   /* Refine by pseudo-interpolation */
   if (best_pitch[0]>0 && best_pitch[0]<(max_pitch>>1)-1)
   {
      opus_val32 a, b, c;
      a = xcorr[best_pitch[0]-1];
      b = xcorr[best_pitch[0]];
      c = xcorr[best_pitch[0]+1];
      if ((c-a) > MULT16_32_Q15(QCONST16(.7f,15),b-a))
         offset = 1;
      else if ((a-c) > MULT16_32_Q15(QCONST16(.7f,15),b-c))
         offset = -1;
      else
         offset = 0;
   } else {
      offset = 0;
   }
   *pitch = 2*best_pitch[0]-offset;

   RESTORE_STACK;
}
