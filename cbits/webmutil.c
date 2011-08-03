#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VPX_CODEC_DISABLE_COMPAT 1
#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"
#define interface (vpx_codec_vp8_cx())
#define fourcc    0x30385056

#define IVF_FILE_HDR_SZ  (32)
#define IVF_FRAME_HDR_SZ (12)

typedef struct {
    int                  frame_cnt;
    vpx_codec_ctx_t      codec;
    vpx_codec_enc_cfg_t  cfg;
    vpx_image_t          raw;
    char                *buf;
    int                  buf_size;
    int                  buf_pos;
} Context;

// Utility code

static void mem_put_le16(char *mem, unsigned int val) {
    mem[0] = val;
    mem[1] = val>>8;
}

static void mem_put_le32(char *mem, unsigned int val) {
    mem[0] = val;
    mem[1] = val>>8;
    mem[2] = val>>16;
    mem[3] = val>>24;
}

static void die_codec(vpx_codec_ctx_t *ctx, const char *s) {
    const char *detail = vpx_codec_error_detail(ctx);

    printf("%s: %s\n", s, vpx_codec_error(ctx));
    if (detail) {
        printf("    %s\n", detail);
    }
}

static void copy(char *src, int size, Context *ctx) {
    if (ctx->buf_size < size + ctx->buf_pos) {
        int new_buf_size = ctx->buf_size;
        while (new_buf_size < size + ctx->buf_pos) {
            new_buf_size *= 2;
        }
        char *new_buf = calloc(1, new_buf_size);
        memcpy(new_buf, ctx->buf, ctx->buf_pos);
        free(ctx->buf);
        ctx->buf = new_buf;
        ctx->buf_size = new_buf_size;
    }
    memcpy(&ctx->buf[ctx->buf_pos], src, size);
    ctx->buf_pos += size;
}

static void write_ivf_file_header(Context *ctx, const vpx_codec_enc_cfg_t *cfg, int frame_cnt) {
    char header[32];

    if(cfg->g_pass != VPX_RC_ONE_PASS && cfg->g_pass != VPX_RC_LAST_PASS)
        return;
    header[0] = 'D';
    header[1] = 'K';
    header[2] = 'I';
    header[3] = 'F';
    mem_put_le16(header+4,  0);                   /* version */
    mem_put_le16(header+6,  32);                  /* headersize */
    mem_put_le32(header+8,  fourcc);              /* headersize */
    mem_put_le16(header+12, cfg->g_w);            /* width */
    mem_put_le16(header+14, cfg->g_h);            /* height */
    mem_put_le32(header+16, cfg->g_timebase.den); /* rate */
    mem_put_le32(header+20, cfg->g_timebase.num); /* scale */
    mem_put_le32(header+24, frame_cnt);           /* length */
    mem_put_le32(header+28, 0);                   /* unused */
    copy(header, 32, ctx);
}

static void write_ivf_frame_header(Context *ctx, const vpx_codec_cx_pkt_t *pkt) {
    char             header[12];
    vpx_codec_pts_t  pts;

    if(pkt->kind != VPX_CODEC_CX_FRAME_PKT)
        return;

    pts = pkt->data.frame.pts;
    mem_put_le32(header, pkt->data.frame.sz);
    mem_put_le32(header+4, pts&0xFFFFFFFF);
    mem_put_le32(header+8, pts >> 32);
    copy(header, 12, ctx);
}

// Binding code

Context *allocVP8(int width, int height) {
    vpx_codec_err_t res;

    Context *ctx = calloc(1,sizeof(Context));
    res = vpx_codec_enc_config_default(interface, &ctx->cfg, 0);
    if(res) {
        printf("Failed to get config: %s\n", vpx_codec_err_to_string(res));
        return 0;
    }

    ctx->cfg.rc_target_bitrate = width * height * ctx->cfg.rc_target_bitrate / ctx->cfg.g_w / ctx->cfg.g_h;
    ctx->cfg.g_w = width;
    ctx->cfg.g_h = height;

    if(vpx_codec_enc_init(&ctx->codec, interface, &ctx->cfg, 0)) {
        die_codec(&ctx->codec, "Failed to initialize encoder");
        free(ctx);
        return 0;
    }
    
    ctx->buf_size = 64*1024;
    ctx->buf = calloc(1,ctx->buf_size);
    return ctx;
}

void freeVP8(Context *ctx) {
    if(vpx_codec_destroy(&ctx->codec)) {
        die_codec(&ctx->codec, "Failed to destroy codec");
    }

    //if(!fseek(outfile, 0, SEEK_SET)) {
    //    write_ivf_file_header(outfile, &cfg, frame_cnt-1);
    //}
    free(ctx->buf);
    free(ctx);
}

char *rawFrameBuffer(Context *ctx) {
    return ctx->raw.planes[0];
}

char *encodeFrame(Context *ctx, int *buf_size) {
    vpx_codec_iter_t iter = NULL;
    const vpx_codec_cx_pkt_t *pkt;
    int flags = 0;

    ctx->buf_pos = 0;
    if(vpx_codec_encode(&ctx->codec, &ctx->raw, ctx->frame_cnt, 1, flags, VPX_DL_REALTIME)) {
        die_codec(&ctx->codec, "Failed to encode frame");
        return 0;
    }
    if (ctx->frame_cnt == 0) {
        write_ivf_file_header(ctx, &ctx->cfg, 0);
    }

    while( (pkt = vpx_codec_get_cx_data(&ctx->codec, &iter)) ) {
        switch(pkt->kind) {
            case VPX_CODEC_CX_FRAME_PKT:
                write_ivf_frame_header(ctx, pkt);
                copy(pkt->data.frame.buf, pkt->data.frame.sz,ctx);
                break;
            default:
                break;
            }
        //printf(pkt->kind == VPX_CODEC_CX_FRAME_PKT && (pkt->data.frame.flags & VPX_FRAME_IS_KEY)? "K":".");
        //fflush(stdout);
    }
    ctx->frame_cnt++;
    *buf_size = ctx->buf_pos;
    return ctx->buf;
}
