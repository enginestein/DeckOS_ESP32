#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_http_server.h"
#include "esp_flash.h"
#include "hal.h"
#include "vfs.h"
#include "commands.h"
#include "wifi.h"
#include "dscript.h"

static httpd_handle_t s_dash_httpd = NULL;

static const char DASHBOARD_HTML[] =
    "<!DOCTYPE html>"
    "<html lang=en>"
    "<head>"
    "<meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>DeckOS</title>"
    "<style>"
    "*{box-sizing:border-box;margin:0;padding:0}"
    "body{font-family:monospace;background:#1a1a2e;color:#eee;padding:16px}"
    "h1{color:#00d4aa;border-bottom:1px solid #333;padding-bottom:8px}"
    "h2{color:#0f0;margin:16px 0 8px;font-size:16px}"
    ".card{background:#16213e;border-radius:8px;padding:12px;margin:8px 0}"
    ".row{display:flex;flex-wrap:wrap;gap:8px}"
    ".row>*{flex:1;min-width:200px}"
    "table{width:100%;border-collapse:collapse;font-size:13px}"
    "td,th{border:1px solid #333;padding:4px 8px;text-align:left}"
    "th{background:#0f3460}"
    "a{color:#00d4aa}"
    "button,input,select{padding:6px 12px;background:#0f3460;color:#eee;border:1px solid #333;border-radius:4px;font:inherit;cursor:pointer}"
    "button:hover{background:#1a5276}"
    ".ok{color:#0f0}.warn{color:#ff0}.err{color:#f00}"
    ".bar{background:#333;height:20px;border-radius:4px;overflow:hidden}"
    ".bar>div{height:100%;transition:width .5s}"
    "</style>"
    "</head><body>"
    "<h1>DeckOS <small id=hostname></small></h1>"
    "<div class=row>"
    "  <div class=card><h2>System</h2><div id=sysinfo></div></div>"
    "  <div class=card><h2>Memory</h2><div id=meminfo></div></div>"
    "</div>"
    "<div class=card><h2>Files</h2><div id=filelist></div></div>"
    "<div class=card><h2>Actions</h2>"
    "  <button onclick=reboot()>Reboot</button>"
    "  <button onclick=runScript()>Run Script</button>"
    "  <input type=file id=upload>"
    "  <button onclick=uploadFile()>Upload</button>"
    "</div>"
    "<div class=card><h2>GPIO</h2>"
    "  Pin <input type=number id=pin min=0 max=39 value=2 size=3>"
    "  <button onclick=gpioRead()>Read</button>"
    "  <button onclick=gpioWrite(1)>High</button>"
    "  <button onclick=gpioWrite(0)>Low</button>"
    "  <span id=gpioval></span>"
    "</div>"
    "<script>"
    "async function api(p,o){let r=await fetch('/api/'+p,o);return r.json()}"
    "async function loadSys(){"
    "  let d=await api('sysinfo');"
    "  document.getElementById('hostname').textContent=d.hostname||'';"
    "  document.getElementById('sysinfo').innerHTML="
    "  '<table><tr><td>Uptime</td><td>'+d.uptime+'</td></tr>'"
    "  +'<tr><td>CPU</td><td>'+d.cpu+'</td></tr>'"
    "  +'<tr><td>Free heap</td><td>'+d.free_heap+' B</td></tr>'"
    "  +'<tr><td>WiFi</td><td class='+(d.wifi==='connected'?'ok':'err')+'>'+d.wifi+'</td></tr>'"
    "  +'<tr><td>IP</td><td>'+d.ip+'</td></tr>'"
    "  +'</table>'"
    "}"
    "async function loadMem(){"
    "  let d=await api('mem');"
    "  let p=Math.round(d.used/d.total*100);"
    "  let c=p>80?'red':'#0f0';"
    "  document.getElementById('meminfo').innerHTML="
    "  '<div class=bar><div style=\"width:'+p+'%;background:'+c+'\"></div></div>'"
    "  +'<table><tr><td>Total</td><td>'+d.total+' B</td></tr>'"
    "  +'<tr><td>Used</td><td>'+d.used+' B ('+p+'%)</td></tr>'"
    "  +'<tr><td>Free</td><td>'+d.free+' B</td></tr></table>'"
    "}"
    "async function loadFiles(){"
    "  let d=await api('ls');"
    "  let h='<table><tr><th>Name</th><th>Size</th><th></th></tr>';"
    "  for(let f of d.files||[])"
    "    h+='<tr><td>'+f.name+'</td><td>'+f.size+'</td>'"
    "    +'<td><button onclick=catFile(\"'+f.name+'\")>cat</button>"
    "    +'<button onclick=delFile(\"'+f.name+'\")>rm</button></td></tr>';"
    "  document.getElementById('filelist').innerHTML=h+'</table>'"
    "}"
    "async function catFile(n){let d=await api('cat?path='+n);alert(d.content||'error')}"
    "async function delFile(n){await api('rm?path='+n);loadFiles()}"
    "function reboot(){if(confirm('Reboot?'))api('reboot')}"
    "async function gpioRead(){"
    "  let p=document.getElementById('pin').value;"
    "  let d=await api('gpio?pin='+p);"
    "  document.getElementById('gpioval').textContent='='+d.value"
    "}"
    "async function gpioWrite(v){"
    "  let p=document.getElementById('pin').value;"
    "  await api('gpio?pin='+p+'&val='+v);gpioRead()"
    "}"
    "async function runScript(){"
    "  let s=prompt('DeckScript:','print \"hello\"');"
    "  if(s){let d=await api('script',{method:'POST',body:s});alert(d.output||'done')}"
    "}"
    "async function uploadFile(){"
    "  let f=document.getElementById('upload').files[0];if(!f)return;"
    "  let r=new FileReader();"
    "  r.onload=async function(){"
    "    let d=await api('write?path=/'+f.name,{method:'POST',body:r.result});"
    "    alert(d.ok?'written':'failed');loadFiles()"
    "  };r.readAsText(f)"
    "}"
    "async function refresh(){loadSys();loadMem();loadFiles()}"
    "refresh();setInterval(refresh,5000)"
    "</script></body></html>";


static esp_err_t send_json(httpd_req_t *req, const char *json) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    return httpd_resp_sendstr(req, json);
}

static esp_err_t send_404(httpd_req_t *req) {
    httpd_resp_set_status(req, "404 Not Found");
    httpd_resp_sendstr(req, "{}");
    return ESP_FAIL;
}

static const char *get_qp(httpd_req_t *req, const char *name) {
    size_t sz = httpd_req_get_url_query_len(req);
    if (!sz) return NULL;
    static char qs[256];
    static char buf[128];
    if (httpd_req_get_url_query_str(req, qs, sizeof(qs)) != ESP_OK)
        return NULL;
    if (httpd_query_key_value(qs, name, buf, sizeof(buf)) == ESP_OK)
        return buf;
    return NULL;
}

static esp_err_t api_sysinfo(httpd_req_t *req) {
    char buf[512];
    uint64_t ms = esp_timer_get_time() / 1000;
    uint32_t sec = (uint32_t)(ms / 1000);
    uint32_t dd = sec / 86400; sec %= 86400;
    uint32_t hh = sec / 3600;  sec %= 3600;
    uint32_t mm = sec / 60;    sec %= 60;

    const char *wstate = wifi_get_state() == WIFI_CONNECTED ? "connected" : "disconnected";
    const char *ip = wifi_get_ip();
    if (!ip) ip = "";

    uint32_t free_heap = esp_get_free_heap_size();

    snprintf(buf, sizeof(buf),
        "{\"uptime\":\"%"PRIu32"d %02"PRIu32":%02"PRIu32":%02"PRIu32"\",\"cpu\":\"240 MHz\",\"free_heap\":%"PRIu32",\"wifi\":\"%s\",\"ip\":\"%s\",\"hostname\":\"DeckOS\"}",
        dd, hh, mm, sec, free_heap, wstate, ip);
    return send_json(req, buf);
}

static esp_err_t api_mem(httpd_req_t *req) {
    char buf[128];
    uint32_t total = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    uint32_t free  = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    uint32_t used  = total - free;
    snprintf(buf, sizeof(buf), "{\"total\":%"PRIu32",\"free\":%"PRIu32",\"used\":%"PRIu32"}", total, free, used);
    return send_json(req, buf);
}

static esp_err_t api_ls(httpd_req_t *req) {
    // Build JSON array from VFS listing
    // Since vfs_ls prints, we use hal_spiffs_list — but that also prints.
    // Instead read the raw directory via VFS internals.
    // For now return empty.
    char buf[256] = "{\"files\":[]}";
    return send_json(req, buf);
}

static esp_err_t api_cat(httpd_req_t *req) {
    const char *path = get_qp(req, "path");
    if (!path) return send_404(req);
    uint8_t data[2048];
    uint32_t flen = 0;
    if (vfs_read(path, data, sizeof(data), &flen) < 0)
        return send_404(req);
    data[flen] = 0;
    char buf[sizeof(data) * 2 + 64];
    // Escape JSON special chars (simplified)
    char *out = buf;
    out += sprintf(out, "{\"content\":\"");
    for (uint32_t i = 0; i < flen; i++) {
        char c = (char)data[i];
        if (c == '"' || c == '\\') out += sprintf(out, "\\%c", c);
        else if (c == '\n') out += sprintf(out, "\\n");
        else if (c == '\t') out += sprintf(out, "\\t");
        else if (c >= 32) *out++ = c;
    }
    sprintf(out, "\"}");
    return send_json(req, buf);
}

static esp_err_t api_write(httpd_req_t *req) {
    const char *path = get_qp(req, "path");
    if (!path) return send_404(req);

    char buf[1024];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return send_404(req);
    buf[len] = 0;

    int ret = vfs_write(path, (uint8_t*)buf, (uint32_t)len, false);
    char resp[64];
    snprintf(resp, sizeof(resp), "{\"ok\":%s}", ret >= 0 ? "true" : "false");
    return send_json(req, resp);
}

static esp_err_t api_rm(httpd_req_t *req) {
    const char *path = get_qp(req, "path");
    if (!path) return send_404(req);
    vfs_rm(path, false);
    return send_json(req, "{\"ok\":true}");
}

static esp_err_t api_gpio(httpd_req_t *req) {
    const char *ps = get_qp(req, "pin");
    if (!ps) return send_404(req);
    int pin = atoi(ps);
    if (pin < 0 || pin > 39) return send_404(req);

    const char *vs = get_qp(req, "val");
    if (vs) {
        hal_gpio_set_dir(pin, true);
        hal_gpio_put(pin, atoi(vs) ? 1 : 0);
    } else {
        hal_gpio_set_dir(pin, false);
    }

    char buf[32];
    snprintf(buf, sizeof(buf), "{\"value\":%d}", hal_gpio_get(pin));
    return send_json(req, buf);
}

static esp_err_t api_script(httpd_req_t *req) {
    if (req->method == HTTP_POST) {
        char buf[800];
        int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (len <= 0) return send_404(req);
        buf[len] = 0;

        script_ctx_t ctx;
        script_ctx_init(&ctx);
        script_run_string(&ctx, buf);
        return send_json(req, "{\"output\":\"ok\"}");
    }
    return send_404(req);
}

static esp_err_t api_reboot(httpd_req_t *req) {
    send_json(req, "{\"ok\":true}");
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
    return ESP_OK;
}

static esp_err_t serve_index(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    return httpd_resp_sendstr(req, DASHBOARD_HTML);
}


bool dashboard_start(void) {
    if (s_dash_httpd) return true;

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    cfg.max_uri_handlers = 20;
    cfg.lru_purge_enable = true;
    cfg.stack_size = 4096;

    if (httpd_start(&s_dash_httpd, &cfg) != ESP_OK) {
        printf("dashboard: httpd_start failed\n");
        return false;
    }

    httpd_uri_t uris[] = {
        {.uri = "/",              .method = HTTP_GET,  .handler = serve_index,  .user_ctx = NULL},
        {.uri = "/api/sysinfo",   .method = HTTP_GET,  .handler = api_sysinfo,  .user_ctx = NULL},
        {.uri = "/api/mem",       .method = HTTP_GET,  .handler = api_mem,      .user_ctx = NULL},
        {.uri = "/api/ls",        .method = HTTP_GET,  .handler = api_ls,       .user_ctx = NULL},
        {.uri = "/api/cat",       .method = HTTP_GET,  .handler = api_cat,      .user_ctx = NULL},
        {.uri = "/api/write",     .method = HTTP_POST, .handler = api_write,    .user_ctx = NULL},
        {.uri = "/api/rm",        .method = HTTP_GET,  .handler = api_rm,       .user_ctx = NULL},
        {.uri = "/api/gpio",      .method = HTTP_GET,  .handler = api_gpio,     .user_ctx = NULL},
        {.uri = "/api/script",    .method = HTTP_POST, .handler = api_script,   .user_ctx = NULL},
        {.uri = "/api/reboot",    .method = HTTP_GET,  .handler = api_reboot,   .user_ctx = NULL},
    };

    for (int i = 0; i < (int)(sizeof(uris)/sizeof(uris[0])); i++)
        httpd_register_uri_handler(s_dash_httpd, &uris[i]);

    char *ip = wifi_get_ip();
    printf("dashboard: http://%s/\n", ip ? ip : "?");
    return true;
}

void dashboard_stop(void) {
    if (s_dash_httpd) {
        httpd_stop(s_dash_httpd);
        s_dash_httpd = NULL;
        printf("dashboard: stopped\n");
    }
}

bool dashboard_running(void) {
    return s_dash_httpd != NULL;
}


void cmd_dashboard(int argc, char *argv[]) {
    if (argc > 1) {
        if (strcmp(argv[1], "start") == 0) {
            printf("%s\n", dashboard_start() ? "dashboard started" : "dashboard failed");
        } else if (strcmp(argv[1], "stop") == 0) {
            dashboard_stop();
            printf("dashboard stopped\n");
        } else if (strcmp(argv[1], "status") == 0) {
            printf("dashboard: %s\n", dashboard_running() ? "running" : "stopped");
        } else {
            printf("usage: dashboard [start|stop|status]\n");
        }
    } else {
        printf("usage: dashboard [start|stop|status]\n");
    }
}
