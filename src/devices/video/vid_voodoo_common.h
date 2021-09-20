/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Header for the 3DFX Voodoo Graphics Controller
 *		Common functions.
 *
 * Version:	@(#)vid_voodoo_common.h	1.0.1	2021/09/14
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

#ifdef MIN
#undef MIN
#endif
#ifdef CLAMP
#undef CLAMP
#endif

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define CLAMP(x) (((x) < 0) ? 0 : (((x) > 0xff) ? 0xff : (x)))
#define CLAMP16(x) (((x) < 0) ? 0 : (((x) > 0xffff) ? 0xffff : (x)))

#define LOD_MAX 8

#define TEX_DIRTY_SHIFT 10

#define TEX_CACHE_MAX 64

enum
{
        VOODOO_1 = 0,
        VOODOO_SB50 = 1,
        VOODOO_2 = 2
};

typedef union {
        uint32_t i;
        float f;
} int_float;

typedef struct {
        uint8_t b, g, r;
        uint8_t pad;
} rgbp_t;
typedef struct {
        uint8_t b, g, r, a;
} rgba8_t;

typedef union {
        struct {
                uint8_t b, g, r, a;
        } rgba;
        uint32_t u;
} rgba_u;

#define FIFO_SIZE 65536
#define FIFO_MASK (FIFO_SIZE - 1)
#define FIFO_ENTRY_SIZE (1 << 31)

#define FIFO_ENTRIES (voodoo->fifo_write_idx - voodoo->fifo_read_idx)
#define FIFO_FULL    ((voodoo->fifo_write_idx - voodoo->fifo_read_idx) >= FIFO_SIZE-4)
#define FIFO_EMPTY   (voodoo->fifo_read_idx == voodoo->fifo_write_idx)

#define FIFO_TYPE 0xff000000
#define FIFO_ADDR 0x00ffffff

enum
{
        FIFO_INVALID    = (0x00 << 24),
        FIFO_WRITEL_REG = (0x01 << 24),
        FIFO_WRITEW_FB  = (0x02 << 24),
        FIFO_WRITEL_FB  = (0x03 << 24),
        FIFO_WRITEL_TEX = (0x04 << 24)
};

#define PARAM_SIZE 1024
#define PARAM_MASK (PARAM_SIZE - 1)
#define PARAM_ENTRY_SIZE (1 << 31)

#define PARAM_ENTRIES_1 (voodoo->params_write_idx - voodoo->params_read_idx[0])
#define PARAM_ENTRIES_2 (voodoo->params_write_idx - voodoo->params_read_idx[1])
#define PARAM_FULL_1 ((voodoo->params_write_idx - voodoo->params_read_idx[0]) >= PARAM_SIZE)
#define PARAM_FULL_2 ((voodoo->params_write_idx - voodoo->params_read_idx[1]) >= PARAM_SIZE)
#define PARAM_EMPTY_1   (voodoo->params_read_idx[0] == voodoo->params_write_idx)
#define PARAM_EMPTY_2   (voodoo->params_read_idx[1] == voodoo->params_write_idx)

typedef struct
{
        uint32_t addr_type;
        uint32_t val;
} fifo_entry_t;

typedef struct voodoo_params_t
{
        int command;

        int32_t vertexAx, vertexAy, vertexBx, vertexBy, vertexCx, vertexCy;

        uint32_t startR, startG, startB, startZ, startA;
        
         int32_t dBdX, dGdX, dRdX, dAdX, dZdX;
        
         int32_t dBdY, dGdY, dRdY, dAdY, dZdY;

        int64_t startW, dWdX, dWdY;

        struct
        {
                int64_t startS, startT, startW, p1;
                int64_t dSdX, dTdX, dWdX, p2;
                int64_t dSdY, dTdY, dWdY, p3;
        } tmu[2];

        uint32_t color0, color1;

        uint32_t fbzMode;
        uint32_t fbzColorPath;
        
        uint32_t fogMode;
        rgbp_t fogColor;
        struct
        {
                uint8_t fog, dfog;
        } fogTable[64];

        uint32_t alphaMode;
        
        uint32_t zaColor;
        
        int chromaKey_r, chromaKey_g, chromaKey_b;
        uint32_t chromaKey;

        uint32_t textureMode[2];
        uint32_t tLOD[2];

        uint32_t texBaseAddr[2], texBaseAddr1[2], texBaseAddr2[2], texBaseAddr38[2];
        
        uint32_t tex_base[2][LOD_MAX+2];
        uint32_t tex_end[2][LOD_MAX+2];
        int tex_width[2];
        int tex_w_mask[2][LOD_MAX+2];
        int tex_w_nmask[2][LOD_MAX+2];
        int tex_h_mask[2][LOD_MAX+2];
        int tex_shift[2][LOD_MAX+2];
        int tex_lod[2][LOD_MAX+2];
        int tex_entry[2];
        int detail_max[2], detail_bias[2], detail_scale[2];
        
        uint32_t draw_offset, aux_offset;

        int tformat[2];
        
        int clipLeft, clipRight, clipLowY, clipHighY;
        
        int sign;
        
        uint32_t front_offset;
        
        uint32_t swapbufferCMD;
        
        uint32_t stipple;
} voodoo_params_t;

typedef struct texture_t
{
        uint32_t base;
        uint32_t tLOD;
        volatile int refcount, refcount_r[2];
        int is16;
        uint32_t palette_checksum;
        uint32_t addr_start[4], addr_end[4];
        uint32_t *data;
} texture_t;

typedef struct vert_t
{
        float sVx, sVy;
        float sRed, sGreen, sBlue, sAlpha;
        float sVz, sWb;
        float sW0, sS0, sT0;
        float sW1, sS1, sT1;
} vert_t;

typedef struct voodoo_t
{
        mem_map_t mapping;
                
        int pci_enable;

        uint8_t dac_data[8];
        int dac_reg, dac_reg_ff;
        uint8_t dac_readdata;
        uint16_t dac_pll_regs[16];
        
        float pixel_clock;
        int line_time;
        
        voodoo_params_t params;
        
        uint32_t fbiInit0, fbiInit1, fbiInit2, fbiInit3, fbiInit4;
        uint32_t fbiInit5, fbiInit6, fbiInit7; /*Voodoo 2*/
        
        uint32_t initEnable;
        
        uint32_t lfbMode;
        
        uint32_t memBaseAddr;

        int_float fvertexAx, fvertexAy, fvertexBx, fvertexBy, fvertexCx, fvertexCy;

        uint32_t front_offset, back_offset;
        
        uint32_t fb_read_offset, fb_write_offset;
        
        int row_width;
        int block_width;
        
        uint8_t *fb_mem, *tex_mem[2];
        uint16_t *tex_mem_w[2];
               
        int rgb_sel;
        
        uint32_t trexInit1[2];
        
        uint32_t tmuConfig;
        
        int swap_count;
        
        int disp_buffer, draw_buffer;
        tmrval_t timer_count;
        
        int line;
        svga_t *svga;
        
        uint32_t backPorch;
        uint32_t videoDimensions;
        uint32_t hSync, vSync;
        
        int h_total, v_total, v_disp;
        int h_disp;
        int v_retrace;

        struct
        {
                uint32_t y[4], i[4], q[4];
        } nccTable[2][2];

        rgba_u palette[2][256];
        
        rgba_u ncc_lookup[2][2][256];
        int ncc_dirty[2];

        thread_t *fifo_thread;
        thread_t *render_thread[2];
        event_t *wake_fifo_thread;
        event_t *wake_main_thread;
        event_t *fifo_not_full_event;
        event_t *render_not_full_event[2];
        event_t *wake_render_thread[2];
        
        int voodoo_busy;
        int render_voodoo_busy[2];
        
        int render_threads;
        int odd_even_mask;
        
        int pixel_count[2], texel_count[2], tri_count, frame_count;
        int pixel_count_old[2], texel_count_old[2];
        int wr_count, rd_count, tex_count;
        
        int retrace_count;
        int swap_interval;
        uint32_t swap_offset;
        int swap_pending;
        
        int bilinear_enabled;
        
        int fb_size;
        uint32_t fb_mask;

        int texture_size;
        uint32_t texture_mask;
        
        int dual_tmus;
        int type;
        
        fifo_entry_t fifo[FIFO_SIZE];
        volatile int fifo_read_idx, fifo_write_idx;
	volatile int cmd_read, cmd_written, cmd_written_fifo;

        voodoo_params_t params_buffer[PARAM_SIZE];
        volatile int params_read_idx[2], params_write_idx;
        
        uint32_t cmdfifo_base, cmdfifo_end;
        int cmdfifo_rp;
        volatile int cmdfifo_depth_rd, cmdfifo_depth_wr;
        uint32_t cmdfifo_amin, cmdfifo_amax;
        
        uint32_t sSetupMode;
	vert_t verts[4];
        int vertex_num;
        int num_verticies;
        
        int flush;

        int scrfilter;
	int scrfilterEnabled;
	int scrfilterThreshold;
	int scrfilterThresholdOld;

        uint32_t last_write_addr;

        uint32_t fbiPixelsIn;
        uint32_t fbiChromaFail;
        uint32_t fbiZFuncFail;
        uint32_t fbiAFuncFail;
        uint32_t fbiPixelsOut;

        uint32_t bltSrcBaseAddr;
        uint32_t bltDstBaseAddr;
        int bltSrcXYStride, bltDstXYStride;
        uint32_t bltSrcChromaRange, bltDstChromaRange;
        int bltSrcChromaMinR, bltSrcChromaMinG, bltSrcChromaMinB;
        int bltSrcChromaMaxR, bltSrcChromaMaxG, bltSrcChromaMaxB;
        int bltDstChromaMinR, bltDstChromaMinG, bltDstChromaMinB;
        int bltDstChromaMaxR, bltDstChromaMaxG, bltDstChromaMaxB;

        int bltClipRight, bltClipLeft;
        int bltClipHighY, bltClipLowY;

        int bltSrcX, bltSrcY;
        int bltDstX, bltDstY;
        int bltSizeX, bltSizeY;
        int bltRop[4];
        uint16_t bltColorFg, bltColorBg;
        
        uint32_t bltCommand;

        struct
        {
                int dst_x, dst_y;
                int cur_x;
                int size_x, size_y;
                int x_dir, y_dir;
                int dst_stride;
        } blt;
                
        rgbp_t clutData[64];
        int clutData_dirty;
        rgbp_t clutData256[256];
        uint32_t video_16to32[0x10000];
        
        uint8_t dirty_line[1024];
        int dirty_line_low, dirty_line_high;
        
        int fb_write_buffer, fb_draw_buffer;
        int buffer_cutoff;

        tmrval_t read_time, write_time, burst_time;

        tmrval_t wake_timer;
                
        uint8_t thefilter[256][256]; // pixel filter, feeding from one or two
        uint8_t thefilterg[256][256]; // for green
        uint8_t thefilterb[256][256]; // for blue

        /* the voodoo adds purple lines for some reason */
        uint16_t purpleline[256][3];

        texture_t texture_cache[2][TEX_CACHE_MAX];
        uint8_t texture_present[2][4096];
        int texture_last_removed;
        
        uint32_t palette_checksum[2];
        int palette_dirty[2];

        uint64_t time;
        int render_time[2];
        
        int use_recompiler;        
        void *codegen_data;
        
        struct voodoo_set_t *set;
} voodoo_t;

typedef struct voodoo_set_t
{
        voodoo_t *voodoos[2];
        
        mem_map_t snoop_mapping;
        
        int nr_cards;
} voodoo_set_t;

/*
 * To conserve space in the .bss segment, these
 * are allocated on the heap at card activation.
 */
extern rgba8_t	*rgb332,
		*ai44,
		*rgb565,
		*argb1555,
		*argb4444,
		*ai88;

void voodoo_recalc(voodoo_t *voodoo);

void voodoo_update_ncc(voodoo_t *voodoo, int tmu);