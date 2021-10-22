/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of the 3DFX Voodoo Graphics Display.
 *
 * Version:	@(#)vid_voodoo_display.c	1.0.1	2021/09/14
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2021 Fred N. van Kempen.
 *		Copyright 2020 Sarah Walker.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free  Software  Foundation; either  version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is  distributed in the hope that it will be useful, but
 * WITHOUT   ANY  WARRANTY;  without  even   the  implied  warranty  of
 * MERCHANTABILITY  or FITNESS  FOR A PARTICULAR  PURPOSE. See  the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the:
 *
 *   Free Software Foundation, Inc.
 *   59 Temple Place - Suite 330
 *   Boston, MA 02111-1307
 *   USA.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <wchar.h>
#include <math.h>
#include "../../emu.h"
#include "../../timer.h"
#include "../../cpu/cpu.h"
#include "../../mem.h"
#include "../../device.h"
#include "../../plat.h"
#include "../system/clk.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_voodoo_common.h"
#include "vid_voodoo_blitter.h"
#include "vid_voodoo_regs.h"
#include "vid_voodoo_render.h"
#ifdef _MSC_VER
# include <malloc.h>
#endif

#define FILTDIV 256

static int FILTCAP, FILTCAPG, FILTCAPB = 0;	/* color filter threshold values */

void voodoo_update_ncc(voodoo_t *voodoo, int tmu)
{
        int tbl;
        
        for (tbl = 0; tbl < 2; tbl++) {
                int col;
                
                for (col = 0; col < 256; col++) {
                        int y = (col >> 4), i = (col >> 2) & 3, q = col & 3;
                        int i_r, i_g, i_b;
                        int q_r, q_g, q_b;
                        
                        y = (voodoo->nccTable[tmu][tbl].y[y >> 2] >> ((y & 3) * 8)) & 0xff;
                        
                        i_r = (voodoo->nccTable[tmu][tbl].i[i] >> 18) & 0x1ff;
                        if (i_r & 0x100)
                                i_r |= 0xfffffe00;
                        i_g = (voodoo->nccTable[tmu][tbl].i[i] >> 9) & 0x1ff;
                        if (i_g & 0x100)
                                i_g |= 0xfffffe00;
                        i_b = voodoo->nccTable[tmu][tbl].i[i] & 0x1ff;
                        if (i_b & 0x100)
                                i_b |= 0xfffffe00;

                        q_r = (voodoo->nccTable[tmu][tbl].q[q] >> 18) & 0x1ff;
                        if (q_r & 0x100)
                                q_r |= 0xfffffe00;
                        q_g = (voodoo->nccTable[tmu][tbl].q[q] >> 9) & 0x1ff;
                        if (q_g & 0x100)
                                q_g |= 0xfffffe00;
                        q_b = voodoo->nccTable[tmu][tbl].q[q] & 0x1ff;
                        if (q_b & 0x100)
                                q_b |= 0xfffffe00;
                        
                        voodoo->ncc_lookup[tmu][tbl][col].rgba.r = CLAMP(y + i_r + q_r);
                        voodoo->ncc_lookup[tmu][tbl][col].rgba.g = CLAMP(y + i_g + q_g);
                        voodoo->ncc_lookup[tmu][tbl][col].rgba.b = CLAMP(y + i_b + q_b);
                        voodoo->ncc_lookup[tmu][tbl][col].rgba.a = 0xff;
                }
        }
}

void voodoo_pixelclock_update(voodoo_t *voodoo)
{
        int m  =  (voodoo->dac_pll_regs[0] & 0x7f) + 2;
        int n1 = ((voodoo->dac_pll_regs[0] >>  8) & 0x1f) + 2;
        int n2 = ((voodoo->dac_pll_regs[0] >> 13) & 0x07);
        float t = (float) (14318184.0 * ((float)m / (float)n1)) / (float)(1 << n2);
        double clock_const;
        int line_length;
        
        if ((voodoo->dac_data[6] & 0xf0) == 0x20 ||
            (voodoo->dac_data[6] & 0xf0) == 0x60 ||
            (voodoo->dac_data[6] & 0xf0) == 0x70)
                t /= 2.0f;
                
        line_length = (voodoo->hSync & 0xff) + ((voodoo->hSync >> 16) & 0x3ff);
        
//        DEBUG("Pixel clock %f MHz hsync %08x line_length %d\n", t, voodoo->hSync, line_length);
        
        voodoo->pixel_clock = t;

        clock_const = cpu_clock / t;
        voodoo->line_time = (int)((double)line_length * clock_const * (double)(1 << TIMER_SHIFT));
}

static void voodoo_calc_clutData(voodoo_t *voodoo)
{
        int c;
        
        for (c = 0; c < 256; c++)
        {
                voodoo->clutData256[c].r = (voodoo->clutData[c >> 3].r*(8-(c & 7)) +
                                           voodoo->clutData[(c >> 3)+1].r*(c & 7)) >> 3;
                voodoo->clutData256[c].g = (voodoo->clutData[c >> 3].g*(8-(c & 7)) +
                                           voodoo->clutData[(c >> 3)+1].g*(c & 7)) >> 3;
                voodoo->clutData256[c].b = (voodoo->clutData[c >> 3].b*(8-(c & 7)) +
                                           voodoo->clutData[(c >> 3)+1].b*(c & 7)) >> 3;
        }

        for (c = 0; c < 65536; c++)
        {
                int r = (c >> 8) & 0xf8;
                int g = (c >> 3) & 0xfc;
                int b = (c << 3) & 0xf8;
//                r |= (r >> 5);
//                g |= (g >> 6);
//                b |= (b >> 5);
                
                voodoo->video_16to32[c] = (voodoo->clutData256[r].r << 16) | (voodoo->clutData256[g].g << 8) | voodoo->clutData256[b].b;
        }
}

void voodoo_generate_filter_v1(voodoo_t *voodoo)
{
        int g, h;
        float difference, diffg, diffb;
        float thiscol, thiscolg, thiscolb, lined;
	float fcr, fcg, fcb;
	
	fcr = (float) (FILTCAP * 5);
	fcg = (float) (FILTCAPG * 6);
	fcb = (float) (FILTCAPB * 5);

        for (g=0;g<FILTDIV;g++)         // pixel 1
        {
                for (h=0;h<FILTDIV;h++)      // pixel 2
                {
                        difference = (float)(h - g);
                        diffg = difference;
                        diffb = difference;

			thiscol = thiscolg = thiscolb = (float)g;

                        if (difference > (float)FILTCAP)
                                difference = (float)FILTCAP;
                        if (difference < (float)-FILTCAP)
                                difference = (float)-FILTCAP;

                        if (diffg > (float)FILTCAPG)
                                diffg = (float)FILTCAPG;
                        if (diffg < (float)-FILTCAPG)
                                diffg = (float)-FILTCAPG;

                        if (diffb > (float)FILTCAPB)
                                diffb = (float)FILTCAPB;
                        if (diffb < (float)-FILTCAPB)
                                diffb = (float)-FILTCAPB;
			
			// hack - to make it not bleed onto black
			//if (g == 0){
			//difference = diffg = diffb = 0;
			//}
			
			if ((difference < fcr) || (-difference > -fcr))
        			thiscol =  (float) (g + (difference / 2));
			if ((diffg < fcg) || (-diffg > -fcg))
        			thiscolg =  (float) (g + (diffg / 2));		/* need these divides so we can actually undither! */
			if ((diffb < fcb) || (-diffb > -fcb))
        			thiscolb =  (float) (g + (diffb / 2));

                        if (thiscol < 0)
                                thiscol = 0;
                        if (thiscol > FILTDIV-1)
                                thiscol = FILTDIV-1;

                        if (thiscolg < 0)
                                thiscolg = 0;
                        if (thiscolg > FILTDIV-1)
                                thiscolg = FILTDIV-1;

                        if (thiscolb < 0)
                                thiscolb = 0;
                        if (thiscolb > FILTDIV-1)
                                thiscolb = FILTDIV-1;

                        voodoo->thefilter[g][h] = (uint8_t)thiscol;
                        voodoo->thefilterg[g][h] = (uint8_t)thiscolg;
                        voodoo->thefilterb[g][h] = (uint8_t)thiscolb;
                }

                lined = (float) (g + 4);
                if (lined > 255)
                        lined = (float)255;
                voodoo->purpleline[g][0] = (uint16_t)lined;
                voodoo->purpleline[g][2] = (uint16_t)lined;

                lined = (float) (g + 0);
                if (lined > 255)
                        lined = (float)255;
                voodoo->purpleline[g][1] = (uint16_t)lined;
        }
}

void voodoo_generate_filter_v2(voodoo_t *voodoo)
{
        int g, h;
        float difference;
        float thiscol, thiscolg, thiscolb, lined;
	float clr, clg, clb = 0;
	float fcr, fcg, fcb = 0;

	// pre-clamping

	fcr = (float)FILTCAP;
	fcg = (float)FILTCAPG;
	fcb = (float)FILTCAPB;

	if (fcr > 32) fcr = (float)32;
	if (fcg > 32) fcg = (float)32;
	if (fcb > 32) fcb = (float)32;

        for (g=0;g<256;g++)         	// pixel 1 - our target pixel we want to bleed into
        {
		for (h=0;h<256;h++)      // pixel 2 - our main pixel
		{
			float avg;
			float avgdiff;
		
			difference = (float)(g - h);
			avg = (float)((g + g + g + g + h) / 5);
			avgdiff = avg - (float)((g + h + h + h + h) / 5);
			if (avgdiff < 0) avgdiff *= -1;
			if (difference < 0) difference *= -1;
		
			thiscol = thiscolg = thiscolb = (float)g;
	
			// try lighten
			if (h > g)
			{
				clr = clg = clb = avgdiff;
		
				if (clr>fcr) clr=fcr;
                                if (clg>fcg) clg=fcg;
				if (clb>fcb) clb=fcb;
		
		
				thiscol = g + clr;
				thiscolg = g + clg;
				thiscolb = g + clb;
		
				if (thiscol>g+FILTCAP)
					thiscol=(float) (g+FILTCAP);
				if (thiscolg>g+FILTCAPG)
					thiscolg=(float) (g+FILTCAPG);
				if (thiscolb>g+FILTCAPB)
					thiscolb=(float) (g+FILTCAPB);
		
		
				if (thiscol>g+avgdiff)
					thiscol=g+avgdiff;
				if (thiscolg>g+avgdiff)
					thiscolg=g+avgdiff;
				if (thiscolb>g+avgdiff)
					thiscolb=g+avgdiff;
		
			}
	
			if (difference > FILTCAP)
				thiscol = (float)g;
			if (difference > FILTCAPG)
				thiscolg = (float)g;
			if (difference > FILTCAPB)
				thiscolb = (float)g;
	
			// clamp 
			if (thiscol < 0) thiscol = (float)0;
			if (thiscolg < 0) thiscolg = (float)0;
			if (thiscolb < 0) thiscolb = (float)0;
	
			if (thiscol > 255) thiscol = (float)255;
			if (thiscolg > 255) thiscolg = (float)255;
			if (thiscolb > 255) thiscolb = (float)255;
	
			// add to the table
			voodoo->thefilter[g][h] = (uint8_t)thiscol;
			voodoo->thefilterg[g][h] = (uint8_t)thiscolg;
			voodoo->thefilterb[g][h] = (uint8_t)thiscolb;
	
			// debug the ones that don't give us much of a difference
			//if (difference < FILTCAP)
			//DEBUG("Voodoofilter: %ix%i - %f difference, %f average difference, R=%f, G=%f, B=%f\n", g, h, difference, avgdiff, thiscol, thiscolg, thiscolb);	
                }

                lined = (float) (g + 3);
                if (lined > 255)
                        lined = (float)255;
                voodoo->purpleline[g][0] = (uint16_t)lined;
                voodoo->purpleline[g][1] = (uint16_t)0;
                voodoo->purpleline[g][2] = (uint16_t)lined;
        }
}

void voodoo_threshold_check(voodoo_t *voodoo)
{
	int r, g, b;

	if (!voodoo->scrfilterEnabled)
		return;	/* considered disabled; don't check and generate */

	/* Check for changes, to generate anew table */
	if (voodoo->scrfilterThreshold != voodoo->scrfilterThresholdOld)
	{
		r = (voodoo->scrfilterThreshold >> 16) & 0xFF;
		g = (voodoo->scrfilterThreshold >> 8 ) & 0xFF;
		b = voodoo->scrfilterThreshold & 0xFF;
		
		FILTCAP = r;
		FILTCAPG = g;
		FILTCAPB = b;
		
		DEBUG("Voodoo Filter Threshold Check: %06x - RED %i GREEN %i BLUE %i\n", voodoo->scrfilterThreshold, r, g, b);	

		voodoo->scrfilterThresholdOld = voodoo->scrfilterThreshold;

		if (voodoo->type == VOODOO_2)
			voodoo_generate_filter_v2(voodoo);
		else
			voodoo_generate_filter_v1(voodoo);
	}
}

static void voodoo_filterline_v1(voodoo_t *voodoo, uint8_t *fil, int column, uint16_t *src, int line)
{
	int x;
	
	// Scratchpad for avoiding feedback streaks
#ifdef _MSC_VER
        if ((voodoo->h_disp) * 3 > 512 * 1024)
        {
            DEBUG("voodoo_filterline_v1: possible stack overflow detected. h_disp is %d", voodoo->h_disp);
            return;
        }
        uint8_t *fil3 = (uint8_t *)_malloca((voodoo->h_disp) * 3);
#else
        uint8_t fil3[(voodoo->h_disp) * 3];  
#endif

	/* 16 to 32-bit */
        for (x=0; x<column;x++)
        {
		fil[x*3] 	=	((src[x] & 31) << 3);
		fil[x*3+1] 	=	(((src[x] >> 5) & 63) << 2);
 		fil[x*3+2] 	=	(((src[x] >> 11) & 31) << 3);

		// Copy to our scratchpads
 		fil3[x*3+0] 	= fil[x*3+0];
 		fil3[x*3+1] 	= fil[x*3+1];
 		fil3[x*3+2] 	= fil[x*3+2];
        }


        /* lines */

        if (line & 1)
        {
                for (x=0; x<column;x++)
                {
                        fil[x*3] = (uint8_t)voodoo->purpleline[fil[x*3]][0];
                        fil[x*3+1] = (uint8_t)voodoo->purpleline[fil[x*3+1]][1];
                        fil[x*3+2] = (uint8_t)voodoo->purpleline[fil[x*3+2]][2];
                }
        }


        /* filtering time */

        for (x=1; x<column;x++)
        {
                fil3[(x)*3]   = voodoo->thefilterb[fil[x*3]][fil[	(x-1)		*3]];
                fil3[(x)*3+1] = voodoo->thefilterg[fil[x*3+1]][fil[	(x-1)		*3+1]];
                fil3[(x)*3+2] = voodoo->thefilter[fil[x*3+2]][fil[	(x-1)		*3+2]];
        }

        for (x=1; x<column;x++)
        {
                fil[(x)*3]   = voodoo->thefilterb[fil3[x*3]][fil3[	(x-1)		*3]];
                fil[(x)*3+1] = voodoo->thefilterg[fil3[x*3+1]][fil3[	(x-1)		*3+1]];
                fil[(x)*3+2] = voodoo->thefilter[fil3[x*3+2]][fil3[	(x-1)		*3+2]];
        }

        for (x=1; x<column;x++)
        {
                fil3[(x)*3]   = voodoo->thefilterb[fil[x*3]][fil[	(x-1)		*3]];
                fil3[(x)*3+1] = voodoo->thefilterg[fil[x*3+1]][fil[	(x-1)		*3+1]];
                fil3[(x)*3+2] = voodoo->thefilter[fil[x*3+2]][fil[	(x-1)		*3+2]];
        }

        for (x=0; x<column-1;x++)
        {
                fil[(x)*3]   = voodoo->thefilterb[fil3[x*3]][fil3[	(x+1)		*3]];
                fil[(x)*3+1] = voodoo->thefilterg[fil3[x*3+1]][fil3[	(x+1)		*3+1]]; 
                fil[(x)*3+2] = voodoo->thefilter[fil3[x*3+2]][fil3[	(x+1)		*3+2]]; 
        }
#ifdef _MSC_VER
        _freea(fil3);
#endif
}


static void voodoo_filterline_v2(voodoo_t *voodoo, uint8_t *fil, int column, uint16_t *src, int line)
{
	int x;

	// Scratchpad for blending filter
#ifdef _MSC_VER
        if ((voodoo->h_disp) * 3 > 512 * 1024)
        {
                DEBUG("voodoo_filterline_v2: possible stack overflow detected. h_disp is %d", voodoo->h_disp);
                return;
        }
        uint8_t *fil3 = (uint8_t *)_malloca((voodoo->h_disp) * 3);
#else
        uint8_t fil3[(voodoo->h_disp) * 3];
#endif
	
	/* 16 to 32-bit */
        for (x=0; x<column;x++)
        {
		// Blank scratchpads
 		fil3[x*3+0] = fil[x*3+0] =	((src[x] & 31) << 3);
 		fil3[x*3+1] = fil[x*3+1] =	(((src[x] >> 5) & 63) << 2);
 		fil3[x*3+2] = fil[x*3+2] =	(((src[x] >> 11) & 31) << 3);
        }

        /* filtering time */

	for (x=1; x<column-3;x++)
        {
		fil3[(x+3)*3]   = voodoo->thefilterb	[((src[x+3] & 31) << 3)]		[((src[x] & 31) << 3)];
		fil3[(x+3)*3+1] = voodoo->thefilterg	[(((src[x+3] >> 5) & 63) << 2)]		[(((src[x] >> 5) & 63) << 2)];
		fil3[(x+3)*3+2] = voodoo->thefilter	[(((src[x+3] >> 11) & 31) << 3)]	[(((src[x] >> 11) & 31) << 3)];

		fil[(x+2)*3]   = voodoo->thefilterb	[fil3[(x+2)*3]][((src[x] & 31) << 3)];
		fil[(x+2)*3+1] = voodoo->thefilterg	[fil3[(x+2)*3+1]][(((src[x] >> 5) & 63) << 2)];
		fil[(x+2)*3+2] = voodoo->thefilter	[fil3[(x+2)*3+2]][(((src[x] >> 11) & 31) << 3)];

		fil3[(x+1)*3]   = voodoo->thefilterb	[fil[(x+1)*3]][((src[x] & 31) << 3)];
		fil3[(x+1)*3+1] = voodoo->thefilterg	[fil[(x+1)*3+1]][(((src[x] >> 5) & 63) << 2)];
		fil3[(x+1)*3+2] = voodoo->thefilter	[fil[(x+1)*3+2]][(((src[x] >> 11) & 31) << 3)];

		fil[(x-1)*3]   = voodoo->thefilterb	[fil3[(x-1)*3]][((src[x] & 31) << 3)];
		fil[(x-1)*3+1] = voodoo->thefilterg	[fil3[(x-1)*3+1]][(((src[x] >> 5) & 63) << 2)];
		fil[(x-1)*3+2] = voodoo->thefilter	[fil3[(x-1)*3+2]][(((src[x] >> 11) & 31) << 3)];
        }

	// unroll for edge cases

	fil3[(column-3)*3]   = voodoo->thefilterb	[((src[column-3] & 31) << 3)]		[((src[column] & 31) << 3)];
	fil3[(column-3)*3+1] = voodoo->thefilterg	[(((src[column-3] >> 5) & 63) << 2)]	[(((src[column] >> 5) & 63) << 2)];
	fil3[(column-3)*3+2] = voodoo->thefilter	[(((src[column-3] >> 11) & 31) << 3)]	[(((src[column] >> 11) & 31) << 3)];

	fil3[(column-2)*3]   = voodoo->thefilterb	[((src[column-2] & 31) << 3)]		[((src[column] & 31) << 3)];
	fil3[(column-2)*3+1] = voodoo->thefilterg	[(((src[column-2] >> 5) & 63) << 2)]	[(((src[column] >> 5) & 63) << 2)];
	fil3[(column-2)*3+2] = voodoo->thefilter	[(((src[column-2] >> 11) & 31) << 3)]	[(((src[column] >> 11) & 31) << 3)];

	fil3[(column-1)*3]   = voodoo->thefilterb	[((src[column-1] & 31) << 3)]		[((src[column] & 31) << 3)];
	fil3[(column-1)*3+1] = voodoo->thefilterg	[(((src[column-1] >> 5) & 63) << 2)]	[(((src[column] >> 5) & 63) << 2)];
	fil3[(column-1)*3+2] = voodoo->thefilter	[(((src[column-1] >> 11) & 31) << 3)]	[(((src[column] >> 11) & 31) << 3)];

	fil[(column-2)*3]   = voodoo->thefilterb	[fil3[(column-2)*3]][((src[column] & 31) << 3)];
	fil[(column-2)*3+1] = voodoo->thefilterg	[fil3[(column-2)*3+1]][(((src[column] >> 5) & 63) << 2)];
	fil[(column-2)*3+2] = voodoo->thefilter		[fil3[(column-2)*3+2]][(((src[column] >> 11) & 31) << 3)];

	fil[(column-1)*3]   = voodoo->thefilterb	[fil3[(column-1)*3]][((src[column] & 31) << 3)];
	fil[(column-1)*3+1] = voodoo->thefilterg	[fil3[(column-1)*3+1]][(((src[column] >> 5) & 63) << 2)];
	fil[(column-1)*3+2] = voodoo->thefilter		[fil3[(column-1)*3+2]][(((src[column] >> 11) & 31) << 3)];

	fil3[(column-1)*3]   = voodoo->thefilterb	[fil[(column-1)*3]][((src[column] & 31) << 3)];
	fil3[(column-1)*3+1] = voodoo->thefilterg	[fil[(column-1)*3+1]][(((src[column] >> 5) & 63) << 2)];
	fil3[(column-1)*3+2] = voodoo->thefilter	[fil[(column-1)*3+2]][(((src[column] >> 11) & 31) << 3)];

#ifdef _MSC_VER
    _freea(fil3);
#endif
}

void voodoo_callback(void *priv)
{
        voodoo_t *voodoo = (voodoo_t *)priv;
	int y_add = (enable_overscan && !suppress_overscan) ? (overscan_y >> 1) : 0;
	int x_add = (enable_overscan && !suppress_overscan) ? 8 : 0;

    if (voodoo->fbiInit0 & FBIINIT0_VGA_PASS) {
	if (voodoo->line < voodoo->v_disp) {
                        voodoo_t *draw_voodoo;
                        int draw_line;
                        
                        if (SLI_ENABLED) {
                                if (voodoo == voodoo->set->voodoos[1])
                                        goto skip_draw;
                                
                                if (((voodoo->initEnable & INITENABLE_SLI_MASTER_SLAVE) ? 1 : 0) == (voodoo->line & 1))
                                        draw_voodoo = voodoo;
                                else
                                        draw_voodoo = voodoo->set->voodoos[1];
                                draw_line = voodoo->line >> 1;
                        }
                        else
                        {
                                if (!(voodoo->fbiInit0 & 1))
                                        goto skip_draw;
                                draw_voodoo = voodoo;
                                draw_line = voodoo->line;
                        }
                        
                        if (draw_voodoo->dirty_line[draw_line]) {
                                pel_t *p = &screen->line[voodoo->line + y_add][32 + x_add];
                                uint16_t *src = (uint16_t *)&draw_voodoo->fb_mem[draw_voodoo->front_offset + draw_line*draw_voodoo->row_width];
                                int x;

                                draw_voodoo->dirty_line[draw_line] = 0;
                                
                                if (voodoo->line < voodoo->dirty_line_low) {
                                        voodoo->dirty_line_low = voodoo->line;
                                        video_blit_wait_buffer();
                                }
                                if (voodoo->line > voodoo->dirty_line_high)
                                        voodoo->dirty_line_high = voodoo->line;
                                
                                if (voodoo->scrfilter && voodoo->scrfilterEnabled) {
#ifdef _MSC_VER
                                    if ((voodoo->h_disp) * 3 > 512 * 1024)
                                    {
                                        DEBUG("voodoo_callback: possible stack overflow detected. h_disp is %d", voodoo->h_disp);
                                        return;
                                    }
                                    uint8_t *fil = (uint8_t *)_malloca((voodoo->h_disp) * 3);
#else
                                    uint8_t fil[(voodoo->h_disp) * 3];              /* interleaved 24-bit RGB */
#endif

                			if (voodoo->type == VOODOO_2)
	                                        voodoo_filterline_v2(voodoo, fil, voodoo->h_disp, src, voodoo->line);
					else
                                        	voodoo_filterline_v1(voodoo, fil, voodoo->h_disp, src, voodoo->line);

                                        for (x = 0; x < voodoo->h_disp; x++)
                                        {
                                                p[x].val = (voodoo->clutData256[fil[x*3]].b << 0 | voodoo->clutData256[fil[x*3+1]].g << 8 | voodoo->clutData256[fil[x*3+2]].r << 16);
                                        }
                                } else {
                                        for (x = 0; x < voodoo->h_disp; x++) {
                                                p[x].val = draw_voodoo->video_16to32[src[x]];
                                        }
                                }
                        }
                }
        }
skip_draw:
        if (voodoo->line == voodoo->v_disp) {
//                DEBUG("retrace %i %i %08x %i\n", voodoo->retrace_count, voodoo->swap_interval, voodoo->swap_offset, voodoo->swap_pending);
                voodoo->retrace_count++;
                if (SLI_ENABLED && 
			(voodoo->fbiInit2 & FBIINIT2_SWAP_ALGORITHM_MASK) == FBIINIT2_SWAP_ALGORITHM_SLI_SYNC) {
                        if (voodoo == voodoo->set->voodoos[0]) {
                                voodoo_t *voodoo_1 = voodoo->set->voodoos[1];

                                thread_wait_mutex(voodoo->swap_mutex);
                                /*Only swap if both Voodoos are waiting for buffer swap*/
                                if (voodoo->swap_pending && (voodoo->retrace_count > voodoo->swap_interval) &&
                                    voodoo_1->swap_pending && (voodoo_1->retrace_count > voodoo_1->swap_interval))
                                {
                                        memset(voodoo->dirty_line, 1, 1024);
                                        voodoo->retrace_count = 0;
                                        voodoo->front_offset = voodoo->swap_offset;
                                        if (voodoo->swap_count > 0)
                                                voodoo->swap_count--;
                                        voodoo->swap_pending = 0;

                                        memset(voodoo_1->dirty_line, 1, 1024);
                                        voodoo_1->retrace_count = 0;
                                        voodoo_1->front_offset = voodoo_1->swap_offset;
                                        if (voodoo_1->swap_count > 0)
                                                voodoo_1->swap_count--;
                                        voodoo_1->swap_pending = 0;
                                        thread_release_mutex(voodoo->swap_mutex);
                                        
                                        thread_set_event(voodoo->wake_fifo_thread);
                                        thread_set_event(voodoo_1->wake_fifo_thread);
                                        
                                        voodoo->frame_count++;
                                        voodoo_1->frame_count++;
                                } else
                                      thread_release_mutex(voodoo->swap_mutex);
                        }
                } else {
                        thread_wait_mutex(voodoo->swap_mutex);
                        if (voodoo->swap_pending && (voodoo->retrace_count > voodoo->swap_interval)) {
                                
                                voodoo->front_offset = voodoo->swap_offset;
                                if (voodoo->swap_count > 0)
                                        voodoo->swap_count--;
                                voodoo->swap_pending = 0;
                                thread_release_mutex(voodoo->swap_mutex);

                                memset(voodoo->dirty_line, 1, 1024);
                                voodoo->retrace_count = 0;
                                thread_set_event(voodoo->wake_fifo_thread);
                                voodoo->frame_count++;
                        } else
				thread_release_mutex(voodoo->swap_mutex);
                }
                voodoo->v_retrace = 1;
        }
        voodoo->line++;
        
        if (voodoo->fbiInit0 & FBIINIT0_VGA_PASS) {
                if (voodoo->line == voodoo->v_disp) {
                        if (voodoo->dirty_line_high > voodoo->dirty_line_low)
                                svga_doblit(0, voodoo->v_disp, voodoo->h_disp, voodoo->v_disp-1, voodoo->svga);
                        if (voodoo->clutData_dirty) {
                                voodoo->clutData_dirty = 0;
                                voodoo_calc_clutData(voodoo);
                        }
                        voodoo->dirty_line_high = -1;
                        voodoo->dirty_line_low = 2000;
                }
        }
        
        if (voodoo->line >= voodoo->v_total) {
                voodoo->line = 0;
                voodoo->v_retrace = 0;
        }
        if (voodoo->line_time)
                voodoo->timer_count += voodoo->line_time;
        else
                voodoo->timer_count += TIMER_USEC * 32;
}




