/* Copyright (c) 2009-2010 Xiph.Org Foundation
   Written by Jean-Marc Valin */
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

#include "celt_lpc.h"
#include "stack_alloc.h"
#include "mathops.h"
#include "pitch.h"

void celt_fir_c(
         const opus_val16 *x,
         const opus_val16 *num,
         opus_val16 *y,
         int N,
         int ord,
         int arch)
{
   int i,j;
   VARDECL(opus_val16, rnum);
   SAVE_STACK;

   ALLOC(rnum, ord, opus_val16);
   for(i=0;i<ord;i++)
      rnum[i] = num[ord-i-1];
   for (i=0;i<N-3;i+=4)
   {
      opus_val32 sum[4];
      sum[0] = SHL32(EXTEND32(x[i  ]), SIG_SHIFT);
      sum[1] = SHL32(EXTEND32(x[i+1]), SIG_SHIFT),
      sum[2] = SHL32(EXTEND32(x[i+2]), SIG_SHIFT);
      sum[3] = SHL32(EXTEND32(x[i+3]), SIG_SHIFT);
      xcorr_kernel(rnum, x+i-ord, sum, ord, arch);
      y[i  ] = ROUND16(sum[0], SIG_SHIFT);
      y[i+1] = ROUND16(sum[1], SIG_SHIFT);
      y[i+2] = ROUND16(sum[2], SIG_SHIFT);
      y[i+3] = ROUND16(sum[3], SIG_SHIFT);
   }
   for (;i<N;i++)
   {
      opus_val32 sum = SHL32(EXTEND32(x[i]), SIG_SHIFT);
      for (j=0;j<ord;j++)
         sum = MAC16_16(sum,rnum[j],x[i+j-ord]);
      y[i] = ROUND16(sum, SIG_SHIFT);
   }
   RESTORE_STACK;
}

int _celt_autocorr(
                   const opus_val16 *x,   /*  in: [0...n-1] samples x   */
                   opus_val32       *ac,  /* out: [0...lag-1] ac values */
                   const opus_val16       *window,
                   int          overlap,
                   int          lag,
                   int          n,
                   int          arch
                  )
{
   opus_val32 d;
   int i, k;
   int fastN=n-lag;
   int shift;
   const opus_val16 *xptr;
   VARDECL(opus_val16, xx);
   SAVE_STACK;
   ALLOC(xx, n, opus_val16);
   celt_assert(n>0);
   celt_assert(overlap>=0);
   if (overlap == 0)
   {
      xptr = x;
   } else {
      for (i=0;i<n;i++)
         xx[i] = x[i];
      for (i=0;i<overlap;i++)
      {
         xx[i] = MULT16_16_Q15(x[i],window[i]);
         xx[n-i-1] = MULT16_16_Q15(x[n-i-1],window[i]);
      }
      xptr = xx;
   }
   shift=0;
#ifdef FIXED_POINT
   {
      opus_val32 ac0;
      ac0 = 1+(n<<7);
      if (n&1) ac0 += SHR32(MULT16_16(xptr[0],xptr[0]),9);
      for(i=(n&1);i<n;i+=2)
      {
         ac0 += SHR32(MULT16_16(xptr[i],xptr[i]),9);
         ac0 += SHR32(MULT16_16(xptr[i+1],xptr[i+1]),9);
      }

      shift = celt_ilog2(ac0)-30+10;
      shift = (shift)/2;
      if (shift>0)
      {
         for(i=0;i<n;i++)
            xx[i] = PSHR32(xptr[i], shift);
         xptr = xx;
      } else
         shift = 0;
   }
#endif
   celt_pitch_xcorr(xptr, xptr, ac, fastN, lag+1, arch);
   for (k=0;k<=lag;k++)
   {
      for (i = k+fastN, d = 0; i < n; i++)
         d = MAC16_16(d, xptr[i], xptr[i-k]);
      ac[k] += d;
   }
#ifdef FIXED_POINT
   shift = 2*shift;
   if (shift<=0)
      ac[0] += SHL32((opus_int32)1, -shift);
   if (ac[0] < 268435456)
   {
      int shift2 = 29 - EC_ILOG(ac[0]);
      for (i=0;i<=lag;i++)
         ac[i] = SHL32(ac[i], shift2);
      shift -= shift2;
   } else if (ac[0] >= 536870912)
   {
      int shift2=1;
      if (ac[0] >= 1073741824)
         shift2++;
      for (i=0;i<=lag;i++)
         ac[i] = SHR32(ac[i], shift2);
      shift += shift2;
   }
#endif

   RESTORE_STACK;
   return shift;
}
