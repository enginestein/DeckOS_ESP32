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

int find_def(char lines[][SCRIPT_LINE_LEN], int total, const char *fname,
             int *body_start, int *body_end) {
    for (int i = 0; i < total; i++) {
        char buf[SCRIPT_LINE_LEN];
        strncpy(buf, lines[i], SCRIPT_LINE_LEN - 1);
        trim_inplace(buf);
        if (strncmp(buf, "def ", 4) != 0) continue;
        char defname[SCRIPT_VAR_NAME_LEN] = {0};
        sscanf(buf + 4, "%31s", defname);
        if (strcmp(defname, fname) != 0) continue;
        int depth = 1, j = i + 1;
        while (j < total && depth > 0) {
            char b2[SCRIPT_LINE_LEN];
            strncpy(b2, lines[j], SCRIPT_LINE_LEN - 1);
            trim_inplace(b2);
            char kw[32] = {0};
            sscanf(b2, "%31s", kw);
            if (!strcmp(kw, "def")) depth++;
            if (!strcmp(kw, "enddef")) depth--;
            j++;
        }
        *body_start = i + 1;
        *body_end = j - 1;
        return i;
    }
    return -1;
}

int __attribute__((unused)) split(const char *s, char parts[][SCRIPT_LINE_LEN], int max) {
    char tmp[SCRIPT_LINE_LEN];
    strncpy(tmp, s, SCRIPT_LINE_LEN - 1);
    int n = 0;
    char *tok = strtok(tmp, " \t");
    while (tok && n < max) { strncpy(parts[n++], tok, SCRIPT_LINE_LEN - 1); tok = strtok(NULL, " \t"); }
    return n;
}

int find_end(char lines[][SCRIPT_LINE_LEN], int total, int from,
             const char *kw_open, const char *kw_close) {
    int depth = 1;
    for (int i = from; i < total; i++) {
        char buf[SCRIPT_LINE_LEN];
        strncpy(buf, lines[i], SCRIPT_LINE_LEN - 1);
        trim_inplace(buf);
        char first[32] = {0};
        sscanf(buf, "%31s", first);
        if (!strcmp(first, kw_open)) depth++;
        if (!strcmp(first, kw_close)) { if (!--depth) return i; }
    }
    return -1;
}

int find_elif_else_end(char lines[][SCRIPT_LINE_LEN], int total, int from,
                       const char *which) {
    int depth = 1;
    for (int i = from; i < total; i++) {
        char buf[SCRIPT_LINE_LEN];
        strncpy(buf, lines[i], SCRIPT_LINE_LEN - 1);
        buf[SCRIPT_LINE_LEN - 1] = '\0';
        trim_inplace(buf);
        char first[32] = {0};
        sscanf(buf, "%31s", first);
        if (!strcmp(first, "if")) depth++;
        if (!strcmp(first, "endif")) {
            depth--;
            if (depth == 0) { if (!strcmp(which, "endif")) return i; return -1; }
        }
        if (depth == 1 && !strcmp(first, which)) return i;
    }
    return -1;
}

void arr_key(char *buf, int buflen, const char *name, int idx) {
    snprintf(buf, (size_t)buflen, "_arr_%s_%d", name, idx);
}

void arr_lenkey(char *buf, int buflen, const char *name) {
    snprintf(buf, (size_t)buflen, "_arr_%s_len", name);
}

int arr_get_len(script_ctx_t *ctx, const char *name) {
    char key[SCRIPT_VAR_NAME_LEN];
    arr_lenkey(key, sizeof(key), name);
    return atoi(var_get(ctx, key));
}

void arr_set_len(script_ctx_t *ctx, const char *name, int len) {
    char key[SCRIPT_VAR_NAME_LEN];
    arr_lenkey(key, sizeof(key), name);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", len);
    var_set(ctx, key, buf);
}
