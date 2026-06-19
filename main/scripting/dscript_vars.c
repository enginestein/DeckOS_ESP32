#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "hal.h"
#include "dscript.h"
#include "vfs.h"
#include "commands.h"
#include "syslog.h"
#include "module.h"
#include "wifi.h"
#include "dscript_internal.h"

void trim_inplace(char *s) {
    int len = (int)strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
    int st = 0;
    while (s[st] && isspace((unsigned char)s[st])) st++;
    if (st) memmove(s, s + st, (size_t)(len - st + 1));
}

void script_ctx_init(script_ctx_t *ctx) { memset(ctx, 0, sizeof(*ctx)); }

const char *var_get(script_ctx_t *ctx, const char *name) {
    for (int i = 0; i < ctx->var_count; i++)
        if (strcmp(ctx->vars[i].name, name) == 0) return ctx->vars[i].value;
    return "";
}

void var_set(script_ctx_t *ctx, const char *name, const char *val) {
    for (int i = 0; i < ctx->var_count; i++) {
        if (strcmp(ctx->vars[i].name, name) == 0) {
            strncpy(ctx->vars[i].value, val, SCRIPT_VAR_VAL_LEN - 1);
            ctx->vars[i].value[SCRIPT_VAR_VAL_LEN - 1] = '\0';
            return;
        }
    }
    if (ctx->var_count >= SCRIPT_MAX_VARS) { printf("script: too many variables\n"); return; }
    strncpy(ctx->vars[ctx->var_count].name, name, SCRIPT_VAR_NAME_LEN - 1);
    strncpy(ctx->vars[ctx->var_count].value, val, SCRIPT_VAR_VAL_LEN - 1);
    ctx->var_count++;
}

void expand_vars(script_ctx_t *ctx, const char *in, char *out, int outlen) {
    int i = 0, o = 0;
    while (in[i] && o < outlen - 1) {
        if (in[i] == '$') {
            i++;
            char vname[SCRIPT_VAR_NAME_LEN] = {0};
            int vn = 0;
            while (in[i] && (isalnum((unsigned char)in[i]) || in[i] == '_') && vn < SCRIPT_VAR_NAME_LEN - 1)
                vname[vn++] = in[i++];
            const char *val = var_get(ctx, vname);
            while (*val && o < outlen - 1) out[o++] = *val++;
        } else { out[o++] = in[i++]; }
    }
    out[o] = '\0';
}
