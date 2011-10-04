/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <math.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "onyx_int.h"
#include "tokenize.h"
#include "vpx_mem/vpx_mem.h"

/* Global event counters used for accumulating statistics across several
   compressions, then generating context.c = initial stats. */

#ifdef ENTROPY_STATS
_int64 context_counters[BLOCK_TYPES] [COEF_BANDS] [PREV_COEF_CONTEXTS] [MAX_ENTROPY_TOKENS];
#endif
void vp8_stuff_mb(VP8_COMP *cpi, MACROBLOCKD *x, TOKENEXTRA **t) ;
void vp8_fix_contexts(MACROBLOCKD *x);

static TOKENVALUE dct_value_tokens[DCT_MAX_VALUE*2];
const TOKENVALUE *vp8_dct_value_tokens_ptr;
static int dct_value_cost[DCT_MAX_VALUE*2];
const int *vp8_dct_value_cost_ptr;
#if 0
int skip_true_count = 0;
int skip_false_count = 0;
#endif
static void fill_value_tokens()
{

    TOKENVALUE *const t = dct_value_tokens + DCT_MAX_VALUE;
    vp8_extra_bit_struct *const e = vp8_extra_bits;

    int i = -DCT_MAX_VALUE;
    int sign = 1;

    do
    {
        if (!i)
            sign = 0;

        {
            const int a = sign ? -i : i;
            int eb = sign;

            if (a > 4)
            {
                int j = 4;

                while (++j < 11  &&  e[j].base_val <= a) {}

                t[i].Token = --j;
                eb |= (a - e[j].base_val) << 1;
            }
            else
                t[i].Token = a;

            t[i].Extra = eb;
        }

        // initialize the cost for extra bits for all possible coefficient value.
        {
            int cost = 0;
            vp8_extra_bit_struct *p = vp8_extra_bits + t[i].Token;

            if (p->base_val)
            {
                const int extra = t[i].Extra;
                const int Length = p->Len;

                if (Length)
                    cost += vp8_treed_cost(p->tree, p->prob, extra >> 1, Length);

                cost += vp8_cost_bit(vp8_prob_half, extra & 1); /* sign */
                dct_value_cost[i + DCT_MAX_VALUE] = cost;
            }

        }

    }
    while (++i < DCT_MAX_VALUE);

    vp8_dct_value_tokens_ptr = dct_value_tokens + DCT_MAX_VALUE;
    vp8_dct_value_cost_ptr   = dct_value_cost + DCT_MAX_VALUE;
}

static void tokenize2nd_order_b
(
    MACROBLOCKD *x,
    TOKENEXTRA **tp,
    VP8_COMP *cpi
)
{
    int pt;             /* near block/prev token context index */
    int c;              /* start at DC */
    TOKENEXTRA *t = *tp;/* store tokens starting here */
    const BLOCKD *b;
    const short *qcoeff_ptr;
    ENTROPY_CONTEXT * a;
    ENTROPY_CONTEXT * l;
    int band, rc, v, token;

    b = x->block + 24;
    qcoeff_ptr = b->qcoeff_base + b->qcoeff_offset;
    a = (ENTROPY_CONTEXT *)x->above_context + 8;
    l = (ENTROPY_CONTEXT *)x->left_context + 8;

    VP8_COMBINEENTROPYCONTEXTS(pt, *a, *l);

    for (c = 0; c < b->eob; c++)
    {
        rc = vp8_default_zig_zag1d[c];
        band = vp8_coef_bands[c];
        v = qcoeff_ptr[rc];

        t->Extra = vp8_dct_value_tokens_ptr[v].Extra;
        token    = vp8_dct_value_tokens_ptr[v].Token;

        t->Token = token;
        t->context_tree = cpi->common.fc.coef_probs [1] [band] [pt];

        t->skip_eob_node = ((pt == 0) && (band > 0));

        ++cpi->coef_counts       [1] [band] [pt] [token];

        pt = vp8_prev_token_class[token];
        t++;
    }
    if (c < 16)
    {
        band = vp8_coef_bands[c];
        t->Token = DCT_EOB_TOKEN;
        t->context_tree = cpi->common.fc.coef_probs [1] [band] [pt];

        t->skip_eob_node = ((pt == 0) && (band > 0));

        ++cpi->coef_counts       [1] [band] [pt] [DCT_EOB_TOKEN];

        t++;
    }

    *tp = t;
    pt = (c != 0); /* 0 <-> all coeff data is zero */
    *a = *l = pt;

}

static void tokenize1st_order_b
(
    MACROBLOCKD *x,
    TOKENEXTRA **tp,
    int type,           /* which plane: 0=Y no DC, 1=Y2, 2=UV, 3=Y with DC */
    VP8_COMP *cpi
)
{
    unsigned int block;
    const BLOCKD *b;
    int pt;             /* near block/prev token context index */
    int c;
    int token;
    TOKENEXTRA *t = *tp;/* store tokens starting here */
    const short *qcoeff_ptr;
    ENTROPY_CONTEXT * a;
    ENTROPY_CONTEXT * l;
    int band, rc, v;
    int tmp1, tmp2;

    b = x->block;
    /* Luma */
    for (block = 0; block < 16; block++, b++)
    {
        tmp1 = vp8_block2above[block];
        tmp2 = vp8_block2left[block];
        qcoeff_ptr = b->qcoeff_base + b->qcoeff_offset;
        a = (ENTROPY_CONTEXT *)x->above_context + tmp1;
        l = (ENTROPY_CONTEXT *)x->left_context + tmp2;

        VP8_COMBINEENTROPYCONTEXTS(pt, *a, *l);

        c = type ? 0 : 1;

        for (; c < b->eob; c++)
        {
            rc = vp8_default_zig_zag1d[c];
            band = vp8_coef_bands[c];
            v = qcoeff_ptr[rc];

            t->Extra = vp8_dct_value_tokens_ptr[v].Extra;
            token    = vp8_dct_value_tokens_ptr[v].Token;

            t->Token = token;
            t->context_tree = cpi->common.fc.coef_probs [type] [band] [pt];

            t->skip_eob_node = pt == 0 &&
                ((band > 0 && type > 0) || (band > 1 && type == 0));

            ++cpi->coef_counts       [type] [band] [pt] [token];

            pt = vp8_prev_token_class[token];
            t++;
        }
        if (c < 16)
        {
            band = vp8_coef_bands[c];
            t->Token = DCT_EOB_TOKEN;
            t->context_tree = cpi->common.fc.coef_probs [type] [band] [pt];

            t->skip_eob_node = pt == 0 &&
                ((band > 0 && type > 0) || (band > 1 && type == 0));

            ++cpi->coef_counts       [type] [band] [pt] [DCT_EOB_TOKEN];

            t++;
        }
        *tp = t;
        pt = (c != !type); /* 0 <-> all coeff data is zero */
        *a = *l = pt;

    }
    /* Chroma */
    for (block = 16; block < 24; block++, b++)
    {
        tmp1 = vp8_block2above[block];
        tmp2 = vp8_block2left[block];
        qcoeff_ptr = b->qcoeff_base + b->qcoeff_offset;
        a = (ENTROPY_CONTEXT *)x->above_context + tmp1;
        l = (ENTROPY_CONTEXT *)x->left_context + tmp2;

        VP8_COMBINEENTROPYCONTEXTS(pt, *a, *l);

        for (c = 0; c < b->eob; c++)
        {
            rc = vp8_default_zig_zag1d[c];
            band = vp8_coef_bands[c];
            v = qcoeff_ptr[rc];

            t->Extra = vp8_dct_value_tokens_ptr[v].Extra;
            token    = vp8_dct_value_tokens_ptr[v].Token;

            t->Token = token;
            t->context_tree = cpi->common.fc.coef_probs [2] [band] [pt];

            t->skip_eob_node = ((pt == 0) && (band > 0));

            ++cpi->coef_counts       [2] [band] [pt] [token];

            pt = vp8_prev_token_class[token];
            t++;
        }
        if (c < 16)
        {
            band = vp8_coef_bands[c];
            t->Token = DCT_EOB_TOKEN;
            t->context_tree = cpi->common.fc.coef_probs [2] [band] [pt];

            t->skip_eob_node = ((pt == 0) && (band > 0));

            ++cpi->coef_counts       [2] [band] [pt] [DCT_EOB_TOKEN];

            t++;
        }
        *tp = t;
        pt = (c != 0); /* 0 <-> all coeff data is zero */
        *a = *l = pt;
    }

}


static int mb_is_skippable(MACROBLOCKD *x, int has_y2_block)
{
    int skip = 1;
    int i = 0;

    if (has_y2_block)
    {
        for (i = 0; i < 16; i++)
            skip &= (x->block[i].eob < 2);
    }

    for (; i < 24 + has_y2_block; i++)
        skip &= (!x->block[i].eob);

    return skip;
}


void vp8_tokenize_mb(VP8_COMP *cpi, MACROBLOCKD *x, TOKENEXTRA **t)
{
    int plane_type;
    int has_y2_block;

    has_y2_block = (x->mode_info_context->mbmi.mode != B_PRED
                    && x->mode_info_context->mbmi.mode != SPLITMV);

    x->mode_info_context->mbmi.mb_skip_coeff = mb_is_skippable(x, has_y2_block);
    if (x->mode_info_context->mbmi.mb_skip_coeff)
    {
        cpi->skip_true_count++;

        if (!cpi->common.mb_no_coeff_skip)
            vp8_stuff_mb(cpi, x, t) ;
        else
        {
            vp8_fix_contexts(x);
        }

        return;
    }

    cpi->skip_false_count++;

    plane_type = 3;
    if(has_y2_block)
    {
        tokenize2nd_order_b(x, t, cpi);
        plane_type = 0;

    }

    tokenize1st_order_b(x, t, plane_type, cpi);

}


#ifdef ENTROPY_STATS

void init_context_counters(void)
{
    vpx_memset(context_counters, 0, sizeof(context_counters));
}

void print_context_counters()
{

    int type, band, pt, t;

    FILE *const f = fopen("context.c", "w");

    fprintf(f, "#include \"entropy.h\"\n");

    fprintf(f, "\n/* *** GENERATED FILE: DO NOT EDIT *** */\n\n");

    fprintf(f, "int Contexts[BLOCK_TYPES] [COEF_BANDS] [PREV_COEF_CONTEXTS] [MAX_ENTROPY_TOKENS];\n\n");

    fprintf(f, "const int default_contexts[BLOCK_TYPES] [COEF_BANDS] [PREV_COEF_CONTEXTS] [MAX_ENTROPY_TOKENS] = {");

# define Comma( X) (X? ",":"")

    type = 0;

    do
    {
        fprintf(f, "%s\n  { /* block Type %d */", Comma(type), type);

        band = 0;

        do
        {
            fprintf(f, "%s\n    { /* Coeff Band %d */", Comma(band), band);

            pt = 0;

            do
            {
                fprintf(f, "%s\n      {", Comma(pt));

                t = 0;

                do
                {
                    const _int64 x = context_counters [type] [band] [pt] [t];
                    const int y = (int) x;

                    assert(x == (_int64) y);  /* no overflow handling yet */
                    fprintf(f, "%s %d", Comma(t), y);

                }
                while (++t < MAX_ENTROPY_TOKENS);

                fprintf(f, "}");
            }
            while (++pt < PREV_COEF_CONTEXTS);

            fprintf(f, "\n    }");

        }
        while (++band < COEF_BANDS);

        fprintf(f, "\n  }");
    }
    while (++type < BLOCK_TYPES);

    fprintf(f, "\n};\n");
    fclose(f);
}
#endif


void vp8_tokenize_initialize()
{
    fill_value_tokens();
}


static __inline void stuff2nd_order_b
(
    TOKENEXTRA **tp,
    ENTROPY_CONTEXT *a,
    ENTROPY_CONTEXT *l,
    VP8_COMP *cpi
)
{
    int pt; /* near block/prev token context index */
    TOKENEXTRA *t = *tp;        /* store tokens starting here */
    VP8_COMBINEENTROPYCONTEXTS(pt, *a, *l);

    t->Token = DCT_EOB_TOKEN;
    t->context_tree = cpi->common.fc.coef_probs [1] [0] [pt];
    t->skip_eob_node = 0;
    ++cpi->coef_counts       [1] [0] [pt] [DCT_EOB_TOKEN];
    ++t;

    *tp = t;
    pt = 0;
    *a = *l = pt;

}

static __inline void stuff1st_order_b
(
    TOKENEXTRA **tp,
    ENTROPY_CONTEXT *a,
    ENTROPY_CONTEXT *l,
    VP8_COMP *cpi
)
{
    int pt; /* near block/prev token context index */
    TOKENEXTRA *t = *tp;        /* store tokens starting here */
    VP8_COMBINEENTROPYCONTEXTS(pt, *a, *l);

    t->Token = DCT_EOB_TOKEN;
    t->context_tree = cpi->common.fc.coef_probs [0] [1] [pt];
    t->skip_eob_node = 0;
    ++cpi->coef_counts       [0] [1] [pt] [DCT_EOB_TOKEN];
    ++t;
    *tp = t;
    pt = 0; /* 0 <-> all coeff data is zero */
    *a = *l = pt;

}
static __inline
void stuff1st_order_buv
(
    TOKENEXTRA **tp,
    ENTROPY_CONTEXT *a,
    ENTROPY_CONTEXT *l,
    VP8_COMP *cpi
)
{
    int pt; /* near block/prev token context index */
    TOKENEXTRA *t = *tp;        /* store tokens starting here */
    VP8_COMBINEENTROPYCONTEXTS(pt, *a, *l);

    t->Token = DCT_EOB_TOKEN;
    t->context_tree = cpi->common.fc.coef_probs [2] [0] [pt];
    t->skip_eob_node = 0;
    ++cpi->coef_counts[2] [0] [pt] [DCT_EOB_TOKEN];
    ++t;
    *tp = t;
    pt = 0; /* 0 <-> all coeff data is zero */
    *a = *l = pt;

}

void vp8_stuff_mb(VP8_COMP *cpi, MACROBLOCKD *x, TOKENEXTRA **t)
{
    ENTROPY_CONTEXT * A = (ENTROPY_CONTEXT *)x->above_context;
    ENTROPY_CONTEXT * L = (ENTROPY_CONTEXT *)x->left_context;
    int plane_type;
    int b;

    stuff2nd_order_b(t,
                     A + vp8_block2above[24], L + vp8_block2left[24], cpi);
    plane_type = 0;

    for (b = 0; b < 16; b++)
        stuff1st_order_b(t,
                         A + vp8_block2above[b],
                         L + vp8_block2left[b], cpi);

    for (b = 16; b < 24; b++)
        stuff1st_order_buv(t,
                           A + vp8_block2above[b],
                           L + vp8_block2left[b], cpi);

}
void vp8_fix_contexts(MACROBLOCKD *x)
{
    /* Clear entropy contexts for Y2 blocks */
    if (x->mode_info_context->mbmi.mode != B_PRED && x->mode_info_context->mbmi.mode != SPLITMV)
    {
        vpx_memset(x->above_context, 0, sizeof(ENTROPY_CONTEXT_PLANES));
        vpx_memset(x->left_context, 0, sizeof(ENTROPY_CONTEXT_PLANES));
    }
    else
    {
        vpx_memset(x->above_context, 0, sizeof(ENTROPY_CONTEXT_PLANES)-1);
        vpx_memset(x->left_context, 0, sizeof(ENTROPY_CONTEXT_PLANES)-1);
    }

}
