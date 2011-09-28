/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef __INC_BLOCKD_H
#define __INC_BLOCKD_H

void vpx_log(const char *format, ...);

#include "../../vpx_ports/config.h"
#include "../../vpx_scale/yv12config.h"
#include "mv.h"
#include "treecoder.h"
#include "subpixel.h"
#include "../../vpx_ports/mem.h"

#include "../../vpx_config.h"
#if CONFIG_OPENCL
#include "opencl/vp8_opencl.h"
#endif

#define TRUE    1
#define FALSE   0

/*#define DCPRED 1*/
#define DCPREDSIMTHRESH 0
#define DCPREDCNTTHRESH 3

#define MB_FEATURE_TREE_PROBS   3
#define MAX_MB_SEGMENTS         4

#define MAX_REF_LF_DELTAS       4
#define MAX_MODE_LF_DELTAS      4

/* Segment Feature Masks */
#define SEGMENT_DELTADATA   0
#define SEGMENT_ABSDATA     1

typedef struct
{
    int r, c;
} POS;

#define PLANE_TYPE_Y_NO_DC    0
#define PLANE_TYPE_Y2         1
#define PLANE_TYPE_UV         2
#define PLANE_TYPE_Y_WITH_DC  3


typedef char ENTROPY_CONTEXT;
typedef struct
{
    ENTROPY_CONTEXT y1[4];
    ENTROPY_CONTEXT u[2];
    ENTROPY_CONTEXT v[2];
    ENTROPY_CONTEXT y2;
} ENTROPY_CONTEXT_PLANES;

extern const unsigned char vp8_block2left[25];
extern const unsigned char vp8_block2above[25];

#define VP8_COMBINEENTROPYCONTEXTS( Dest, A, B) \
    Dest = ((A)!=0) + ((B)!=0);


typedef enum
{
    KEY_FRAME = 0,
    INTER_FRAME = 1
} FRAME_TYPE;

typedef enum
{
    DC_PRED = 0,            /* average of above and left pixels */
    V_PRED = 1,             /* vertical prediction */
    H_PRED = 2,             /* horizontal prediction */
    TM_PRED = 3,            /* Truemotion prediction */
    B_PRED = 4,             /* block based prediction, each block has its own prediction mode */

    NEARESTMV = 5,
    NEARMV = 6,
    ZEROMV = 7,
    NEWMV = 8,
    SPLITMV = 9,

    MB_MODE_COUNT = 10
} MB_PREDICTION_MODE;

/* Macroblock level features */
typedef enum
{
    MB_LVL_ALT_Q = 0,               /* Use alternate Quantizer .... */
    MB_LVL_ALT_LF = 1,              /* Use alternate loop filter value... */
    MB_LVL_MAX = 2                  /* Number of MB level features supported */

} MB_LVL_FEATURES;

/* Segment Feature Masks */
#define SEGMENT_ALTQ    0x01
#define SEGMENT_ALT_LF  0x02

#define VP8_YMODES  (B_PRED + 1)
#define VP8_UV_MODES (TM_PRED + 1)

#define VP8_MVREFS (1 + SPLITMV - NEARESTMV)

typedef enum
{
    B_DC_PRED,          /* average of above and left pixels */
    B_TM_PRED,

    B_VE_PRED,           /* vertical prediction */
    B_HE_PRED,           /* horizontal prediction */

    B_LD_PRED,
    B_RD_PRED,

    B_VR_PRED,
    B_VL_PRED,
    B_HD_PRED,
    B_HU_PRED,

    LEFT4X4,
    ABOVE4X4,
    ZERO4X4,
    NEW4X4,

    B_MODE_COUNT
} B_PREDICTION_MODE;

#define VP8_BINTRAMODES (B_HU_PRED + 1)  /* 10 */
#define VP8_SUBMVREFS (1 + NEW4X4 - LEFT4X4)

/* For keyframes, intra block modes are predicted by the (already decoded)
   modes for the Y blocks to the left and above us; for interframes, there
   is a single probability table. */

union b_mode_info
{
    B_PREDICTION_MODE as_mode;
    int_mv mv;
};

typedef enum
{
    INTRA_FRAME = 0,
    LAST_FRAME = 1,
    GOLDEN_FRAME = 2,
    ALTREF_FRAME = 3,
    MAX_REF_FRAMES = 4
} MV_REFERENCE_FRAME;

typedef struct
{
    MB_PREDICTION_MODE mode, uv_mode;
    MV_REFERENCE_FRAME ref_frame;
    int_mv mv;

    unsigned char partitioning;
    unsigned char mb_skip_coeff;                                /* does this mb has coefficients at all, 1=no coefficients, 0=need decode tokens */
    unsigned char need_to_clamp_mvs;
    unsigned char segment_id;                  /* Which set of segmentation parameters should be used for this MB */
} MB_MODE_INFO;

typedef struct
{
    MB_MODE_INFO mbmi;
    union b_mode_info bmi[16];
} MODE_INFO;

typedef struct
{
    short *qcoeff_base;
    int qcoeff_offset;

    short *dqcoeff_base;
    int dqcoeff_offset;

    unsigned char *predictor_base;
    int predictor_offset;

    short *diff_base;
    int diff_offset;

    short *dequant;

#if CONFIG_OPENCL
    cl_command_queue cl_commands; //pointer to macroblock CL command queue

    cl_mem cl_diff_mem;
    cl_mem cl_predictor_mem;
    cl_mem cl_qcoeff_mem;
    cl_mem cl_dqcoeff_mem;
    cl_mem cl_eobs_mem;

    cl_mem cl_dequant_mem; //Block-specific, not shared

    cl_bool sixtap_filter; //Subpixel Prediction type (true=sixtap, false=bilinear)

#endif

    /* 16 Y blocks, 4 U blocks, 4 V blocks each with 16 entries */
    unsigned char **base_pre; //previous frame, same Macroblock, base pointer
    int pre;
    int pre_stride;

    unsigned char **base_dst; //destination base pointer
    int dst;
    int dst_stride;

    int eob; //only used in encoder? Decoder uses MBD.eobs

    char *eobs_base; //beginning of MB.eobs

    union b_mode_info bmi;
} BLOCKD;

typedef struct
{
    DECLARE_ALIGNED(16, short, diff[400]);      /* from idct diff */
    DECLARE_ALIGNED(16, unsigned char,  predictor[384]);
    DECLARE_ALIGNED(16, short, qcoeff[400]);
    DECLARE_ALIGNED(16, short, dqcoeff[400]);
    DECLARE_ALIGNED(16, char,  eobs[25]);

#if CONFIG_OPENCL
    cl_command_queue cl_commands; //Each macroblock gets its own command queue.
    cl_mem cl_diff_mem;
    cl_mem cl_predictor_mem;
    cl_mem cl_qcoeff_mem;
    cl_mem cl_dqcoeff_mem;
    cl_mem cl_eobs_mem;

    cl_bool sixtap_filter;
#endif

    /* 16 Y blocks, 4 U, 4 V, 1 DC 2nd order block, each with 16 entries. */
    BLOCKD block[25];

    YV12_BUFFER_CONFIG pre; /* Filtered copy of previous frame reconstruction */
    YV12_BUFFER_CONFIG dst; /* Destination buffer for current frame */

    MODE_INFO *mode_info_context;
    int mode_info_stride;

    FRAME_TYPE frame_type;

    int up_available;
    int left_available;

    /* Y,U,V,Y2 */
    ENTROPY_CONTEXT_PLANES *above_context;
    ENTROPY_CONTEXT_PLANES *left_context;

    /* 0 indicates segmentation at MB level is not enabled. Otherwise the individual bits indicate which features are active. */
    unsigned char segmentation_enabled;

    /* 0 (do not update) 1 (update) the macroblock segmentation map. */
    unsigned char update_mb_segmentation_map;

    /* 0 (do not update) 1 (update) the macroblock segmentation feature data. */
    unsigned char update_mb_segmentation_data;

    /* 0 (do not update) 1 (update) the macroblock segmentation feature data. */
    unsigned char mb_segement_abs_delta;

    /* Per frame flags that define which MB level features (such as quantizer or loop filter level) */
    /* are enabled and when enabled the proabilities used to decode the per MB flags in MB_MODE_INFO */
    vp8_prob mb_segment_tree_probs[MB_FEATURE_TREE_PROBS];         /* Probability Tree used to code Segment number */

    signed char segment_feature_data[MB_LVL_MAX][MAX_MB_SEGMENTS];            /* Segment parameters */

    /* mode_based Loop filter adjustment */
    unsigned char mode_ref_lf_delta_enabled;
    unsigned char mode_ref_lf_delta_update;

    /* Delta values have the range +/- MAX_LOOP_FILTER */
    signed char last_ref_lf_deltas[MAX_REF_LF_DELTAS];                /* 0 = Intra, Last, GF, ARF */
    signed char ref_lf_deltas[MAX_REF_LF_DELTAS];                     /* 0 = Intra, Last, GF, ARF */
    signed char last_mode_lf_deltas[MAX_MODE_LF_DELTAS];                      /* 0 = BPRED, ZERO_MV, MV, SPLIT */
    signed char mode_lf_deltas[MAX_MODE_LF_DELTAS];                           /* 0 = BPRED, ZERO_MV, MV, SPLIT */

    /* Distance of MB away from frame edges */
    int mb_to_left_edge;
    int mb_to_right_edge;
    int mb_to_top_edge;
    int mb_to_bottom_edge;

    unsigned int frames_since_golden;
    unsigned int frames_till_alt_ref_frame;

    vp8_subpix_fn_t  subpixel_predict;
    vp8_subpix_fn_t  subpixel_predict8x4;
    vp8_subpix_fn_t  subpixel_predict8x8;
    vp8_subpix_fn_t  subpixel_predict16x16;

    void *current_bc;

    int corrupted;

#if CONFIG_RUNTIME_CPU_DETECT
    struct VP8_COMMON_RTCD  *rtcd;
#endif
} MACROBLOCKD;


extern void vp8_build_block_doffsets(MACROBLOCKD *x);
extern void vp8_setup_block_dptrs(MACROBLOCKD *x);

static void update_blockd_bmi(MACROBLOCKD *xd)
{
    int i;
    int is_4x4;
    is_4x4 = (xd->mode_info_context->mbmi.mode == SPLITMV) ||
              (xd->mode_info_context->mbmi.mode == B_PRED);

    if (is_4x4)
    {
        for (i = 0; i < 16; i++)
        {
            xd->block[i].bmi = xd->mode_info_context->bmi[i];
        }
    }
}

#endif  /* __INC_BLOCKD_H */
