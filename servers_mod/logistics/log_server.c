// log_server_step_final.c  (open62541 v1.5.2)
//
// Security (same as MFG):
//  - Discovery: None-only
//  - Session: Basic256Sha256 + SignAndEncrypt
//  - trusted client cert: trust_mes.der
//  - Username/Password only
//
// Logistics:
//  - speed: log/conveyor_speed1..3 (double 0..100, RW)
//  - per warehouse signals:
//      log/wh{1..3}/loading (bool, RO)
//      log/wh{1..3}/loaded  (bool, RO)
//      log/wh{1..3}/load_qty (uint32, RO)  // current loaded count
//  - Move(wh:uint16, qty:uint32):
//      loading=true immediately
//      then increases load_qty by 1 per step until qty items done
//      then loaded=true
//      step period computed from speed+qty (100% => 1s per item)
//  - StopMove(): cancel jobs + reset loading/loaded (qty stays)
//
// Build:
//  gcc -Wall -Wextra -O2 log_server_step_final.c -o log_server \
//    -I/usr/local/include -L/usr/local/lib -lopen62541 \
//    -lssl -lcrypto -lpthread -lm -lrt

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>

#include "modbus_log.h"

#include <open62541/server.h>
#include <open62541/server_config_default.h>
#include <open62541/plugin/accesscontrol_default.h>





/* ----------------- graceful shutdown ----------------- */
static volatile sig_atomic_t g_running = 1;
static void on_sigint(int sig) { (void)sig; g_running = 0; }

/* ----------------- users ----------------- */
static UA_UsernamePasswordLogin g_users[1];
static UA_Boolean g_users_inited = false;
static void init_users_once(void) {
    if(g_users_inited) return;
    g_users[0].username = UA_STRING("mes");
    g_users[0].password = UA_STRING("mespw_change_me");
    g_users_inited = true;
}

/* ----------------- robust file loader ----------------- */
static UA_ByteString loadFile(const char *path) {
    UA_ByteString bs = UA_BYTESTRING_NULL;
    FILE *f = fopen(path, "rb");
    if(!f) {
        fprintf(stderr, "[LOG] cannot open file: %s\n", path);
        return bs;
    }
    if(fseek(f, 0, SEEK_END) != 0) { fclose(f); return bs; }
    long sz = ftell(f);
    if(sz <= 0) { fclose(f); return bs; }
    rewind(f);

    bs.data = (UA_Byte*)UA_malloc((size_t)sz);
    if(!bs.data) { fclose(f); return UA_BYTESTRING_NULL; }
    bs.length = (size_t)sz;

    size_t n = fread(bs.data, 1, (size_t)sz, f);
    fclose(f);

    if(n != (size_t)sz) {
        UA_ByteString_clear(&bs);
        return UA_BYTESTRING_NULL;
    }
    return bs;
}









/* ----------------- security helpers ----------------- */
static UA_String* findPolicyUri(UA_ServerConfig *cfg, const char *uri) {
    if(!cfg || !cfg->securityPolicies || cfg->securityPoliciesSize == 0)
        return NULL;

    UA_String target = UA_STRING((char*)uri);
    for(size_t i=0; i<cfg->securityPoliciesSize; i++) {
        if(UA_String_equal(&cfg->securityPolicies[i].policyUri, &target))
            return &cfg->securityPolicies[i].policyUri;
    }
    return NULL;
}

static void filter_user_tokens_no_anonymous(UA_EndpointDescription *ep) {
    if(!ep->userIdentityTokens || ep->userIdentityTokensSize == 0)
        return;

    size_t keep = 0;
    for(size_t i=0; i<ep->userIdentityTokensSize; i++) {
        if(ep->userIdentityTokens[i].tokenType != UA_USERTOKENTYPE_ANONYMOUS)
            keep++;
    }

    UA_UserTokenPolicy *newArr = NULL;
    if(keep > 0) {
        newArr = (UA_UserTokenPolicy*)UA_Array_new(keep, &UA_TYPES[UA_TYPES_USERTOKENPOLICY]);
        size_t w = 0;
        for(size_t i=0; i<ep->userIdentityTokensSize; i++) {
            if(ep->userIdentityTokens[i].tokenType == UA_USERTOKENTYPE_ANONYMOUS)
                continue;
            UA_UserTokenPolicy_copy(&ep->userIdentityTokens[i], &newArr[w++]);
        }
    }

    UA_Array_delete(ep->userIdentityTokens, ep->userIdentityTokensSize,
                    &UA_TYPES[UA_TYPES_USERTOKENPOLICY]);

    ep->userIdentityTokens = newArr;
    ep->userIdentityTokensSize = keep;
}

static void harden_endpoints_tokens_only(UA_ServerConfig *cfg) {
    if(!cfg || !cfg->endpoints || cfg->endpointsSize == 0)
        return;
    for(size_t i=0; i<cfg->endpointsSize; i++)
        filter_user_tokens_no_anonymous(&cfg->endpoints[i]);
}

static UA_StatusCode
activateSession_strict_cb(UA_Server *server,
                          UA_AccessControl *ac,
                          const UA_EndpointDescription *endpointDescription,
                          const UA_ByteString *secureChannelRemoteCertificate,
                          const UA_NodeId *sessionId,
                          const UA_ExtensionObject *userIdentityToken,
                          void **sessionContext) {
    (void)server; (void)ac; (void)endpointDescription;
    (void)secureChannelRemoteCertificate; (void)sessionId; (void)sessionContext;

    if(userIdentityToken &&
       userIdentityToken->content.decoded.type == &UA_TYPES[UA_TYPES_ANONYMOUSIDENTITYTOKEN])
        return UA_STATUSCODE_BADIDENTITYTOKENINVALID;

    if(!userIdentityToken ||
       userIdentityToken->content.decoded.type != &UA_TYPES[UA_TYPES_USERNAMEIDENTITYTOKEN])
        return UA_STATUSCODE_BADIDENTITYTOKENINVALID;

    UA_UserNameIdentityToken *tok =
        (UA_UserNameIdentityToken*)userIdentityToken->content.decoded.data;

    UA_String expectUser = UA_STRING("mes");
    if(!UA_String_equal(&tok->userName, &expectUser))
        return UA_STATUSCODE_BADUSERACCESSDENIED;

    const char *expectPw = "mespw_change_me";
    size_t expectLen = strlen(expectPw);

    if(tok->password.length != expectLen ||
       memcmp(tok->password.data, expectPw, expectLen) != 0)
        return UA_STATUSCODE_BADUSERACCESSDENIED;

    return UA_STATUSCODE_GOOD;
}

static void configure_accesscontrol_no_anonymous(UA_ServerConfig *cfg) {
    init_users_once();

    UA_String *pwPolicyUri = findPolicyUri(cfg,
        "http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256");

    if(cfg->accessControl.clear)
        cfg->accessControl.clear(&cfg->accessControl);

    UA_AccessControl_default(cfg, false, pwPolicyUri, 1, g_users);
    cfg->accessControl.activateSession = activateSession_strict_cb;
}

/* ----------------- NodeIds ----------------- */
static UA_NodeId STATUS_ID;
static UA_NodeId STOCK_ID;

static UA_NodeId LOG_TEMP_ID;
static UA_NodeId LOG_HUM_ID;
static UA_NodeId SPEED_ID[3];
static UA_NodeId WH_LOADING_ID[3];
static UA_NodeId WH_LOADED_ID[3];
static UA_NodeId WH_QTY_ID[3];
static UA_NodeId WH_LOWSTOCK_ID[3];

/* ----------------- Warehouse runtime state ----------------- */
static UA_UInt32 g_wh_qty[3] = {0,0,0};       /* current loaded */
static UA_UInt32 g_wh_target[3] = {0,0,0};    /* target loaded count (absolute) */
static UA_UInt64 g_step_cb_id[3] = {0,0,0};   /* callback ids */
static UA_UInt32 g_job_seq[3] = {0,0,0};      /* cancel guard */


/*----------------- Modbus --------------*/
static UA_NodeId SCM_RUN1_ID;
static UA_NodeId SCM_RUN2_ID;
static UA_NodeId SCM_RUN3_ID;


/* ------------- modbus cb*/
static void modbus_poll_cb(UA_Server *server, void *data) {
    (void)data;

    uint8_t b = 0;

    if(mb_read_bit_retry(SCM_QX_RUN1, &b) == 0) {
        UA_Boolean v = b ? UA_TRUE : UA_FALSE;
        UA_Variant vv;
        UA_Variant_setScalar(&vv, &v, &UA_TYPES[UA_TYPES_BOOLEAN]);
        (void)UA_Server_writeValue(server, SCM_RUN1_ID, vv);
    }

    if(mb_read_bit_retry(SCM_QX_RUN2, &b) == 0) {
        UA_Boolean v = b ? UA_TRUE : UA_FALSE;
        UA_Variant vv;
        UA_Variant_setScalar(&vv, &v, &UA_TYPES[UA_TYPES_BOOLEAN]);
        (void)UA_Server_writeValue(server, SCM_RUN2_ID, vv);
    }

    if(mb_read_bit_retry(SCM_QX_RUN3, &b) == 0) {
        UA_Boolean v = b ? UA_TRUE : UA_FALSE;
        UA_Variant vv;
        UA_Variant_setScalar(&vv, &v, &UA_TYPES[UA_TYPES_BOOLEAN]);
        (void)UA_Server_writeValue(server, SCM_RUN3_ID, vv);
    }
}



/*------------------------------------------------------*/





/* ===================== ADD: Modbus RUN1~3 OPC UA 변수 노드 생성 ===================== */
static void add_bool_ro_node(UA_Server *server, UA_NodeId nid,
                            const char *disp, const char *qname) {
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT("en-US", (char*)disp);

    UA_Boolean init = UA_FALSE;
    UA_Variant_setScalar(&attr.value, &init, &UA_TYPES[UA_TYPES_BOOLEAN]);

    /* ✅ 클라이언트는 읽기만 (READ ONLY) */
    attr.accessLevel     = UA_ACCESSLEVELMASK_READ;
    attr.userAccessLevel = UA_ACCESSLEVELMASK_READ;

    UA_StatusCode rc = UA_Server_addVariableNode(server,
        nid,
        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
        UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
        UA_QUALIFIEDNAME(1, (char*)qname),
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
        attr, NULL, NULL);

    if(rc != UA_STATUSCODE_GOOD)
        printf("[LOG] add %s failed: 0x%08x\n", disp, (unsigned)rc);
}


static UA_UInt16 read_u16_or_default(UA_Server *server, UA_NodeId id, UA_UInt16 defval) {
    UA_Variant v; UA_Variant_init(&v);
    UA_StatusCode rc = UA_Server_readValue(server, id, &v);
    if(rc == UA_STATUSCODE_GOOD && UA_Variant_hasScalarType(&v, &UA_TYPES[UA_TYPES_UINT16])) {
        UA_UInt16 out = *(UA_UInt16*)v.data;
        UA_Variant_clear(&v);
        return out;
    }
    UA_Variant_clear(&v);
    return defval;
}

/* ------------------------------------------------------------------------- */





typedef struct {
    int idx;
    UA_UInt32 seq;
    UA_UInt64 cb_id;
} step_ctx_t;

static void set_wh_flags(UA_Server *server, int idx, UA_Boolean loading, UA_Boolean loaded) {
    UA_Variant v1, v2;
    UA_Variant_setScalar(&v1, &loading, &UA_TYPES[UA_TYPES_BOOLEAN]);
    UA_Variant_setScalar(&v2, &loaded,  &UA_TYPES[UA_TYPES_BOOLEAN]);
    (void)UA_Server_writeValue(server, WH_LOADING_ID[idx], v1);
    (void)UA_Server_writeValue(server, WH_LOADED_ID[idx],  v2);
}

static void write_wh_qty(UA_Server *server, int idx) {
    UA_UInt32 q = g_wh_qty[idx];
    UA_Variant vQ;
    UA_Variant_setScalar(&vQ, &q, &UA_TYPES[UA_TYPES_UINT32]);
    (void)UA_Server_writeValue(server, WH_QTY_ID[idx], vQ);
}

static double read_speed_pct(UA_Server *server, int idx) {
    double spd = 0.0;
    UA_Variant v; UA_Variant_init(&v);
    if(UA_Server_readValue(server, SPEED_ID[idx], &v) == UA_STATUSCODE_GOOD &&
       UA_Variant_hasScalarType(&v, &UA_TYPES[UA_TYPES_DOUBLE])) {
        spd = *(double*)v.data;
    }
    UA_Variant_clear(&v);
    if(spd < 0.0) spd = 0.0;
    if(spd > 100.0) spd = 100.0;
    return spd;
}

static UA_UInt32 calc_total_ms(double speed_pct, UA_UInt32 qty) {
    if(speed_pct < 1.0 || qty == 0) return 0;
    /* 100% => 1s per item */
    double sec_per_item = 1.0 * (100.0 / speed_pct);
    double total_sec = sec_per_item * (double)qty;
    double ms = total_sec * 1000.0;
    if(ms < 200.0) ms = 200.0;
    if(ms > 600000.0) ms = 600000.0;
    return (UA_UInt32)ms;
}

static void load_step_cb(UA_Server *server, void *data) {
    step_ctx_t *ctx = (step_ctx_t*)data;
    if(!ctx) return;

    int idx = ctx->idx;

    /* cancel if not latest */
    if(ctx->seq != g_job_seq[idx]) {
        UA_Server_removeRepeatedCallback(server, ctx->cb_id);
        UA_free(ctx);
        return;
    }

    if(g_wh_qty[idx] < g_wh_target[idx]) {
        g_wh_qty[idx] += 1;
        if(g_wh_qty[idx] > 0) {
            UA_Boolean low = false;
            UA_Variant v;
            UA_Variant_setScalar(&v, &low, &UA_TYPES[UA_TYPES_BOOLEAN]);
            (void)UA_Server_writeValue(server, WH_LOWSTOCK_ID[idx], v);
        }
        write_wh_qty(server, idx);

        if(g_wh_qty[idx] >= g_wh_target[idx]) {
            set_wh_flags(server, idx, false, true);
            UA_Server_removeRepeatedCallback(server, ctx->cb_id);
            g_step_cb_id[idx] = 0;

            printf("[LOG] WH%d loaded complete (qty=%u)\n", idx+1, (unsigned)g_wh_qty[idx]);
            fflush(stdout);

            UA_free(ctx);
        }
        return;
    }

    /* already reached */
    set_wh_flags(server, idx, false, true);
    UA_Server_removeRepeatedCallback(server, ctx->cb_id);
    g_step_cb_id[idx] = 0;
    UA_free(ctx);
}

/* ----------------- Methods ----------------- */
static UA_StatusCode
Move_cb(UA_Server *server,
        const UA_NodeId *sessionId, void *sessionContext,
        const UA_NodeId *methodId, void *methodContext,
        const UA_NodeId *objectId, void *objectContext,
        size_t inputSize, const UA_Variant *input,
        size_t outputSize, UA_Variant *output) {
    (void)sessionId; (void)sessionContext; (void)methodId; (void)methodContext;
    (void)objectId; (void)objectContext; (void)outputSize; (void)output;

    if(inputSize != 2 ||
       !UA_Variant_hasScalarType(&input[0], &UA_TYPES[UA_TYPES_UINT16]) ||
       !UA_Variant_hasScalarType(&input[1], &UA_TYPES[UA_TYPES_UINT32]))
        return UA_STATUSCODE_BADINVALIDARGUMENT;

    UA_UInt16 wh = *(UA_UInt16*)input[0].data;
    UA_UInt32 qty = *(UA_UInt32*)input[1].data;
    UA_UInt32 req_qty = qty;
    UA_UInt32 stock_after = 0;

    if(wh < 1 || wh > 3)
        return UA_STATUSCODE_BADOUTOFRANGE;

    int idx = (int)wh - 1;

    printf("[LOG] Move called wh=%u qty=%u\n", (unsigned)wh, (unsigned)qty);

    /* ---------------------------- modbus (per-conveyor) -----------------------*/
    {
        static const uint16_t MW_MAX[3]   = { SCM_MW_MAX1,   SCM_MW_MAX2,   SCM_MW_MAX3   };
        static const uint16_t MW_SPEED[3] = { SCM_MW_SPEED1, SCM_MW_SPEED2, SCM_MW_SPEED3 };
        static const uint16_t QX_START[3] = { SCM_QX_START1, SCM_QX_START2, SCM_QX_START3 };

        /* 1) 발주량(qty) -> MAX 레지스터 */
        uint16_t maxv = (qty > 65535u) ? 65535u : (uint16_t)qty;
        (void)mb_write_reg_u16_retry(MW_MAX[idx], maxv);

        /* 2) speed(double 0..100) -> SPEED 레지스터 (0..100 정수로 보냄) */
        double spd_pct = read_speed_pct(server, idx);
        uint16_t spdv  = (uint16_t)(spd_pct + 0.5);
        if(spdv > 100) spdv = 100;
        (void)mb_write_reg_u16_retry(MW_SPEED[idx], spdv);

        /* 3) 해당 컨베이어만 START */
        mb_set_coil(SCM_QX_STOP, 0);          /* STOP 공용이면 OK */
        
        mb_set_coil(SCM_QX_AUTO, 1);          /* AUTO 공용이면 OK */

        mb_set_coil(QX_START[idx], 1);        /* wh에 해당하는 START만 */
        printf("[LOG][MODBUS] WH%d order=%u -> MW_MAX=%u (addr=%u), speed=%u\n",
            idx+1, (unsigned)qty, (unsigned)maxv, (unsigned)MW_MAX[idx], (unsigned)spdv);
    }

    /*---------------------------------------------------------------------------------*/


    /* cancel existing job */
    g_job_seq[idx]++;

    if(g_step_cb_id[idx] != 0) {
        UA_Server_removeRepeatedCallback(server, g_step_cb_id[idx]);
        g_step_cb_id[idx] = 0;
    }

    /* status -> Moving */
    {
        UA_String moving = UA_STRING("Moving");
        UA_Variant vS;
        UA_Variant_setScalar(&vS, &moving, &UA_TYPES[UA_TYPES_STRING]);
        (void)UA_Server_writeValue(server, STATUS_ID, vS);
    }
    /* Move 시작 시 low_stock 해제(입고 예정) */
    {
        UA_Boolean low = false;
        UA_Variant v;
        UA_Variant_setScalar(&v, &low, &UA_TYPES[UA_TYPES_BOOLEAN]);
        (void)UA_Server_writeValue(server, WH_LOWSTOCK_ID[idx], v);
    }
    /* set loading flags */
    set_wh_flags(server, idx, true, false);

    /* compute timing */
    double spd = read_speed_pct(server, idx);
    UA_UInt32 total_ms = calc_total_ms(spd, qty);
    if(total_ms == 0 || qty == 0) {
        printf("[LOG] WH%d Move rejected (speed=%.1f%% or qty=0)\n", idx+1, spd);
        set_wh_flags(server, idx, false, false);
        return UA_STATUSCODE_BADINVALIDSTATE;   /* Busy/InvalidState 대용 */
    }

    g_wh_target[idx] = g_wh_qty[idx] + qty;

    UA_UInt32 step_ms = total_ms / qty;
    if(step_ms < 50) step_ms = 50; /* UI/CPU 보호 */

    step_ctx_t *ctx = (step_ctx_t*)UA_malloc(sizeof(*ctx));
    if(!ctx) return UA_STATUSCODE_BADOUTOFMEMORY;

    ctx->idx = idx;
    ctx->seq = g_job_seq[idx];
    ctx->cb_id = 0;

    UA_UInt64 cbId = 0;
    UA_StatusCode rc2 = UA_Server_addRepeatedCallback(server, load_step_cb, ctx, step_ms, &cbId);
    if(rc2 != UA_STATUSCODE_GOOD) {
        UA_free(ctx);
        printf("[LOG] addRepeatedCallback(step) failed: 0x%08x\n", (unsigned)rc2);
        return rc2;
    }

    ctx->cb_id = cbId;
    g_step_cb_id[idx] = cbId;

    printf("[LOG] WH%d loading start: speed=%.1f%% qty=%u total=%ums step=%ums target=%u\n",
           idx+1, spd, (unsigned)qty, (unsigned)total_ms, (unsigned)step_ms, (unsigned)g_wh_target[idx]);
    fflush(stdout);

    return UA_STATUSCODE_GOOD;
}

static UA_StatusCode
StopMove_cb(UA_Server *server,
            const UA_NodeId *sessionId, void *sessionContext,
            const UA_NodeId *methodId, void *methodContext,
            const UA_NodeId *objectId, void *objectContext,
            size_t inputSize, const UA_Variant *input,
            size_t outputSize, UA_Variant *output) {
    (void)server;
    (void)sessionId; (void)sessionContext; (void)methodId; (void)methodContext;
    (void)objectId; (void)objectContext; (void)inputSize; (void)input;
    (void)outputSize; (void)output;

    printf("[LOG] StopMove called\n");

/* ----------------------------modbus------------------------------- */
    mb_set_coil(SCM_QX_START1, 0);
    mb_set_coil(SCM_QX_START2, 0);
    mb_set_coil(SCM_QX_START3, 0);
    mb_set_coil(SCM_QX_AUTO,   0);
    mb_set_coil(SCM_QX_STOP,   1);




    for(int i=0;i<3;i++){
        g_job_seq[i]++;

        if(g_step_cb_id[i] != 0) {
            UA_Server_removeRepeatedCallback(server, g_step_cb_id[i]);
            g_step_cb_id[i] = 0;
        }

        set_wh_flags(server, i, false, false);
    }

    fflush(stdout);
    return UA_STATUSCODE_GOOD;
}

static UA_StatusCode
Consume_cb(UA_Server *server,
    const UA_NodeId *sessionId, void *sessionContext,
    const UA_NodeId *methodId, void *methodContext,
    const UA_NodeId *objectId, void *objectContext,
    size_t inputSize, const UA_Variant *input,
    size_t outputSize, UA_Variant *output)
{
    (void)sessionId;(void)sessionContext;
    (void)methodId;(void)methodContext;
    (void)objectId;(void)objectContext;
    (void)outputSize;(void)output;

    if(inputSize!=2)
        return UA_STATUSCODE_BADINVALIDARGUMENT;

    UA_UInt16 wh=*(UA_UInt16*)input[0].data;
    UA_UInt32 qty=*(UA_UInt32*)input[1].data;

    if(wh<1 || wh>3)
        return UA_STATUSCODE_BADOUTOFRANGE;

    int idx=wh-1;
    /* 입고(loading) 중이면 출고 금지 */
    UA_Variant lv; UA_Variant_init(&lv);
    UA_Boolean loading = false;
    if(UA_Server_readValue(server, WH_LOADING_ID[idx], &lv) == UA_STATUSCODE_GOOD &&
    UA_Variant_hasScalarType(&lv, &UA_TYPES[UA_TYPES_BOOLEAN])) {
        loading = *(UA_Boolean*)lv.data;
    }
    UA_Variant_clear(&lv);

    if(loading) {
        printf("[LOG] WH%d consume rejected: LOADING\n", idx+1);
        fflush(stdout);
        return UA_STATUSCODE_BADINVALIDSTATE;
    }
    /* ⭐ 재고가 0이면 출고 불가: low_stock 올리고 실패 */
    if(g_wh_qty[idx] == 0) {
        UA_Boolean low = true;
        UA_Variant v;
        UA_Variant_setScalar(&v, &low, &UA_TYPES[UA_TYPES_BOOLEAN]);
        (void)UA_Server_writeValue(server, WH_LOWSTOCK_ID[idx], v);

        printf("[LOG] WH%d consume rejected: EMPTY (qty=0)\n", idx+1);
        fflush(stdout);

        return UA_STATUSCODE_BADOUTOFRANGE;
    }

    if(g_wh_qty[idx] < qty)
    {
        UA_Boolean low=true;

        UA_Variant v;
        UA_Variant_setScalar(&v,&low,&UA_TYPES[UA_TYPES_BOOLEAN]);

        UA_Server_writeValue(server,
            WH_LOWSTOCK_ID[idx],v);

        return UA_STATUSCODE_BADOUTOFRANGE;
    }

    g_wh_qty[idx]-=qty;

    write_wh_qty(server,idx);

    printf("[LOG] WH%d consume %u → qty=%u\n",
        wh,(unsigned)qty,(unsigned)g_wh_qty[idx]);

    if(g_wh_qty[idx]==0)
    {
        UA_Boolean low=true;

        UA_Variant v;
        UA_Variant_setScalar(&v,&low,&UA_TYPES[UA_TYPES_BOOLEAN]);

        UA_Server_writeValue(server,
            WH_LOWSTOCK_ID[idx],v);

        printf("[LOG] WH%d LOW STOCK\n",wh);
    }

    fflush(stdout);

    return UA_STATUSCODE_GOOD;
}
/*====================read dht helper================*/
static int find_dht_iio_device(char *outPath, size_t outLen) {
    const char *base = "/sys/bus/iio/devices";
    DIR *d = opendir(base);
    if(!d) return -1;

    struct dirent *ent;
    while((ent = readdir(d))) {
        if(strncmp(ent->d_name, "iio:device", 10) != 0) continue;

        char namePath[512];
        int n = snprintf(namePath, sizeof(namePath), "%s/%s/name", base, ent->d_name);
        if(n < 0 || (size_t)n >= sizeof(namePath)) continue;

        FILE *f = fopen(namePath, "r");
        if(!f) continue;

        char name[128] = {0};
        if(fgets(name, sizeof(name), f)) {
            name[strcspn(name, "\r\n")] = 0;
            if(strstr(name, "dht") || strstr(name, "DHT")) {
                int m = snprintf(outPath, outLen, "%s/%s", base, ent->d_name);
                fclose(f);
                closedir(d);
                if(m < 0 || (size_t)m >= outLen) return -1;
                return 0;
            }
        }
        fclose(f);
    }
    closedir(d);
    return -1;
}

static int read_long_file(const char *path, long *out) {
    FILE *f = fopen(path, "r");
    if(!f) return -1;
    char buf[64];
    if(!fgets(buf, sizeof(buf), f)) { fclose(f); return -1; }
    fclose(f);
    errno = 0;
    long v = strtol(buf, NULL, 10);
    if(errno) return -1;
    *out = v;
    return 0;
}

static int read_dht11_iio(double *tempC, double *humPct) {
    char dev[512];
    if(find_dht_iio_device(dev, sizeof(dev)) != 0) return -1;

    char tPath[600], hPath[600];
    int nt = snprintf(tPath, sizeof(tPath), "%s/in_temp_input", dev);
    int nh = snprintf(hPath, sizeof(hPath), "%s/in_humidityrelative_input", dev);
    if(nt < 0 || (size_t)nt >= sizeof(tPath)) return -1;
    if(nh < 0 || (size_t)nh >= sizeof(hPath)) return -1;

    long tRaw=0, hRaw=0;
    if(read_long_file(tPath, &tRaw) != 0) return -1;
    if(read_long_file(hPath, &hRaw) != 0) return -1;

    *tempC = (double)tRaw / 1000.0;
    *humPct = (double)hRaw / 1000.0;
    return 0;
}

static void log_dht_update_cb(UA_Server *server, void *data) {
    (void)data;
    double t=0.0, h=0.0;
    if(read_dht11_iio(&t, &h) == 0) {
        UA_Variant vT, vH;
        UA_Variant_setScalar(&vT, &t, &UA_TYPES[UA_TYPES_DOUBLE]);
        UA_Variant_setScalar(&vH, &h, &UA_TYPES[UA_TYPES_DOUBLE]);
        (void)UA_Server_writeValue(server, LOG_TEMP_ID, vT);
        (void)UA_Server_writeValue(server, LOG_HUM_ID,  vH);
    }
}

/* ----------------- main ----------------- */
int main(void) {
    signal(SIGINT, on_sigint);

    /* ---------------- Modbus init ---------------- */
    if(mb_init("10.10.16.208", 502, 1) != 0) {
        fprintf(stderr, "[LOG] modbus init failed\n");
        /* 필요하면 여기서 종료해도 되고,
           일단 OPC UA 서버만 띄우고 싶으면 종료 안 해도 됨 */
    }

    UA_ByteString serverCert = loadFile("/home/pi/opcua_demo/certs/log/cert.der");
    UA_ByteString serverKey  = loadFile("/home/pi/opcua_demo/certs/log/key.der");
    UA_ByteString trustMes   = loadFile("/home/pi/opcua_demo/certs/log/trust_mes.der");

    if(serverCert.length == 0 || serverKey.length == 0 || trustMes.length == 0) {
        fprintf(stderr, "[LOG] cert/key/trust load failed (check paths)\n");
        UA_ByteString_clear(&serverCert);
        UA_ByteString_clear(&serverKey);
        UA_ByteString_clear(&trustMes);
        return 1;
    }

    const UA_ByteString trustList[1] = { trustMes };

    UA_Server *server = UA_Server_new();
    UA_ServerConfig *cfg = UA_Server_getConfig(server);

    UA_StatusCode rc = UA_ServerConfig_setDefaultWithSecurityPolicies(
        cfg, 4860,
        &serverCert, &serverKey,
        trustList, 1,
        NULL, 0,
        NULL, 0
    );
    /*------------------------------- modified ------------------------------------*/
    cfg->applicationDescription.applicationUri = UA_STRING_ALLOC("urn:opcua:log");
    /*-------------------------------------------------------------------------------*/

    if(rc != UA_STATUSCODE_GOOD) {
        printf("[LOG] Security config failed: 0x%08x\n", (unsigned)rc);
        UA_Server_delete(server);
        UA_ByteString_clear(&serverCert);
        UA_ByteString_clear(&serverKey);
        UA_ByteString_clear(&trustMes);
        return 1;
    }

    cfg->securityPolicyNoneDiscoveryOnly = true;
    cfg->allowNonePolicyPassword = false;

    configure_accesscontrol_no_anonymous(cfg);
    harden_endpoints_tokens_only(cfg);

    /* NodeIds */
    STATUS_ID = UA_NODEID_STRING(1, "log/status");
    STOCK_ID  = UA_NODEID_STRING(1, "log/stock_raw");
    
    LOG_TEMP_ID = UA_NODEID_STRING(1, "log/temp");
    LOG_HUM_ID  = UA_NODEID_STRING(1, "log/hum");

    SPEED_ID[0] = UA_NODEID_STRING(1, "log/conveyor_speed1");
    SPEED_ID[1] = UA_NODEID_STRING(1, "log/conveyor_speed2");
    SPEED_ID[2] = UA_NODEID_STRING(1, "log/conveyor_speed3");

    WH_LOADING_ID[0] = UA_NODEID_STRING(1, "log/wh1/loading");
    WH_LOADED_ID[0]  = UA_NODEID_STRING(1, "log/wh1/loaded");
    WH_QTY_ID[0]     = UA_NODEID_STRING(1, "log/wh1/load_qty");

    WH_LOADING_ID[1] = UA_NODEID_STRING(1, "log/wh2/loading");
    WH_LOADED_ID[1]  = UA_NODEID_STRING(1, "log/wh2/loaded");
    WH_QTY_ID[1]     = UA_NODEID_STRING(1, "log/wh2/load_qty");

    WH_LOADING_ID[2] = UA_NODEID_STRING(1, "log/wh3/loading");
    WH_LOADED_ID[2]  = UA_NODEID_STRING(1, "log/wh3/loaded");
    WH_QTY_ID[2]     = UA_NODEID_STRING(1, "log/wh3/load_qty");

    WH_LOWSTOCK_ID[0] = UA_NODEID_STRING(1,"log/wh1/low_stock");
    WH_LOWSTOCK_ID[1] = UA_NODEID_STRING(1,"log/wh2/low_stock");
    WH_LOWSTOCK_ID[2] = UA_NODEID_STRING(1,"log/wh3/low_stock");

    /*------------------------OpenPLC(Modbus) to Client--------------------------*/
    SCM_RUN1_ID   = UA_NODEID_STRING(1, "log/run1");
    SCM_RUN2_ID   = UA_NODEID_STRING(1, "log/run2");
    SCM_RUN3_ID   = UA_NODEID_STRING(1, "log/run3");


    /* main() 안에서, SCM_RUNx_ID 설정한 뒤에 호출 */
    add_bool_ro_node(server, SCM_RUN1_ID, "SCM_RUN1", "SCM_RUN1");
    add_bool_ro_node(server, SCM_RUN2_ID, "SCM_RUN2", "SCM_RUN2");
    add_bool_ro_node(server, SCM_RUN3_ID, "SCM_RUN3", "SCM_RUN3");




    /* status */
    {
        UA_VariableAttributes attr = UA_VariableAttributes_default;
        attr.displayName = UA_LOCALIZEDTEXT("en-US", "LOG_Status");
        UA_String init = UA_STRING("Idle");
        UA_Variant_setScalar(&attr.value, &init, &UA_TYPES[UA_TYPES_STRING]);
        (void)UA_Server_addVariableNode(server,
            STATUS_ID,
            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
            UA_QUALIFIEDNAME(1, "LOG_Status"),
            UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
            attr, NULL, NULL);
    }

    /* stock */
    {
        UA_VariableAttributes attr = UA_VariableAttributes_default;
        attr.displayName = UA_LOCALIZEDTEXT("en-US", "Stock_Raw");
        UA_UInt32 stock = 100;
        UA_Variant_setScalar(&attr.value, &stock, &UA_TYPES[UA_TYPES_UINT32]);
        (void)UA_Server_addVariableNode(server,
            STOCK_ID,
            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
            UA_QUALIFIEDNAME(1, "Stock_Raw"),
            UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
            attr, NULL, NULL);
    }
    /* log/temp */
    {
        UA_VariableAttributes attr = UA_VariableAttributes_default;
        attr.displayName = UA_LOCALIZEDTEXT("en-US", "LOG_Temp");
        UA_Double t = 0.0;
        UA_Variant_setScalar(&attr.value, &t, &UA_TYPES[UA_TYPES_DOUBLE]);
        attr.accessLevel = UA_ACCESSLEVELMASK_READ;
        attr.userAccessLevel = UA_ACCESSLEVELMASK_READ;

        UA_Server_addVariableNode(server,
            LOG_TEMP_ID,
            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
            UA_QUALIFIEDNAME(1, "LOG_Temp"),
            UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
            attr, NULL, NULL);
    }

    /* log/hum */
    {
        UA_VariableAttributes attr = UA_VariableAttributes_default;
        attr.displayName = UA_LOCALIZEDTEXT("en-US", "LOG_Hum");
        UA_Double h = 0.0;
        UA_Variant_setScalar(&attr.value, &h, &UA_TYPES[UA_TYPES_DOUBLE]);
        attr.accessLevel = UA_ACCESSLEVELMASK_READ;
        attr.userAccessLevel = UA_ACCESSLEVELMASK_READ;

        UA_Server_addVariableNode(server,
            LOG_HUM_ID,
            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
            UA_QUALIFIEDNAME(1, "LOG_Hum"),
            UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
            attr, NULL, NULL);
    }

    /* speeds (RW) */
    for(int i=0;i<3;i++){
        char name[64];
        snprintf(name, sizeof(name), "LOG_ConveyorSpeed%d", i+1);

        UA_VariableAttributes attr = UA_VariableAttributes_default;
        attr.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", name);
        double spd = 0.0;
        UA_Variant_setScalar(&attr.value, &spd, &UA_TYPES[UA_TYPES_DOUBLE]);
        attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
        attr.userAccessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;

        (void)UA_Server_addVariableNode(server,
            SPEED_ID[i],
            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
            UA_QUALIFIEDNAME_ALLOC(1, name),
            UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
            attr, NULL, NULL);
    }

    /* wh flags + qty (RO) */
    for(int i=0;i<3;i++){
        char nL[64], nD[64], nQ[64];
        snprintf(nL, sizeof(nL), "WH%d_Loading", i+1);
        snprintf(nD, sizeof(nD), "WH%d_Loaded",  i+1);
        snprintf(nQ, sizeof(nQ), "WH%d_LoadQty", i+1);

        UA_Boolean bfalse = false;

        UA_VariableAttributes a1 = UA_VariableAttributes_default;
        a1.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", nL);
        UA_Variant_setScalar(&a1.value, &bfalse, &UA_TYPES[UA_TYPES_BOOLEAN]);
        a1.accessLevel = UA_ACCESSLEVELMASK_READ;
        a1.userAccessLevel = UA_ACCESSLEVELMASK_READ;
        (void)UA_Server_addVariableNode(server,
            WH_LOADING_ID[i],
            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
            UA_QUALIFIEDNAME_ALLOC(1, nL),
            UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
            a1, NULL, NULL);

        UA_VariableAttributes a2 = UA_VariableAttributes_default;
        a2.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", nD);
        UA_Variant_setScalar(&a2.value, &bfalse, &UA_TYPES[UA_TYPES_BOOLEAN]);
        a2.accessLevel = UA_ACCESSLEVELMASK_READ;
        a2.userAccessLevel = UA_ACCESSLEVELMASK_READ;
        (void)UA_Server_addVariableNode(server,
            WH_LOADED_ID[i],
            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
            UA_QUALIFIEDNAME_ALLOC(1, nD),
            UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
            a2, NULL, NULL);

        UA_VariableAttributes a3 = UA_VariableAttributes_default;
        a3.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", nQ);
        UA_UInt32 q0 = 0;
        UA_Variant_setScalar(&a3.value, &q0, &UA_TYPES[UA_TYPES_UINT32]);
        a3.accessLevel = UA_ACCESSLEVELMASK_READ;
        a3.userAccessLevel = UA_ACCESSLEVELMASK_READ;
        (void)UA_Server_addVariableNode(server,
            WH_QTY_ID[i],
            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
            UA_QUALIFIEDNAME_ALLOC(1, nQ),
            UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
            a3, NULL, NULL);
    }

    for(int i=0;i<3;i++){
        UA_Boolean low=false;

        UA_VariableAttributes a4 = UA_VariableAttributes_default;
        a4.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US","WH_LowStock");

        UA_Variant_setScalar(&a4.value,&low,&UA_TYPES[UA_TYPES_BOOLEAN]);

        a4.accessLevel = UA_ACCESSLEVELMASK_READ;
        a4.userAccessLevel = UA_ACCESSLEVELMASK_READ;

        UA_Server_addVariableNode(server,
            WH_LOWSTOCK_ID[i],
            UA_NODEID_NUMERIC(0,UA_NS0ID_OBJECTSFOLDER),
            UA_NODEID_NUMERIC(0,UA_NS0ID_ORGANIZES),
            UA_QUALIFIEDNAME_ALLOC(1,"WH_LowStock"),
            UA_NODEID_NUMERIC(0,UA_NS0ID_BASEDATAVARIABLETYPE),
            a4,NULL,NULL);
    }
    /* Method: Move(warehouse:uint16, qty:uint32) */
    {
        UA_Argument inArgs[2];
        UA_Argument_init(&inArgs[0]);
        inArgs[0].name = UA_STRING("warehouse");
        inArgs[0].dataType = UA_TYPES[UA_TYPES_UINT16].typeId;
        inArgs[0].valueRank = -1;

        UA_Argument_init(&inArgs[1]);
        inArgs[1].name = UA_STRING("qty");
        inArgs[1].dataType = UA_TYPES[UA_TYPES_UINT32].typeId;
        inArgs[1].valueRank = -1;

        UA_MethodAttributes ma = UA_MethodAttributes_default;
        ma.displayName = UA_LOCALIZEDTEXT("en-US", "Move");
        ma.executable = true;
        ma.userExecutable = true;

        (void)UA_Server_addMethodNode(server,
            UA_NODEID_STRING(1, "log/Move"),
            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
            UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
            UA_QUALIFIEDNAME(1, "Move"),
            ma,
            &Move_cb,
            2, inArgs,
            0, NULL,
            NULL, NULL);
    }

    /* Method: StopMove() */
    {
        UA_MethodAttributes ma = UA_MethodAttributes_default;
        ma.displayName = UA_LOCALIZEDTEXT("en-US", "StopMove");
        ma.executable = true;
        ma.userExecutable = true;

        (void)UA_Server_addMethodNode(server,
            UA_NODEID_STRING(1, "log/StopMove"),
            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
            UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
            UA_QUALIFIEDNAME(1, "StopMove"),
            ma,
            &StopMove_cb,
            0, NULL,
            0, NULL,
            NULL, NULL);
    }

/* Method: Consume(warehouse:uint16, qty:uint32) */
    {
        UA_Argument inArgs[2];

        UA_Argument_init(&inArgs[0]);
        inArgs[0].name = UA_STRING("warehouse");
        inArgs[0].dataType = UA_TYPES[UA_TYPES_UINT16].typeId;
        inArgs[0].valueRank = -1;

        UA_Argument_init(&inArgs[1]);
        inArgs[1].name = UA_STRING("qty");
        inArgs[1].dataType = UA_TYPES[UA_TYPES_UINT32].typeId;
        inArgs[1].valueRank = -1;

        UA_MethodAttributes ma = UA_MethodAttributes_default;
        ma.displayName = UA_LOCALIZEDTEXT("en-US", "Consume");
        ma.executable = true;
        ma.userExecutable = true;

        (void)UA_Server_addMethodNode(server,
            UA_NODEID_STRING(1, "log/Consume"),
            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
            UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
            UA_QUALIFIEDNAME(1, "Consume"),
            ma,
            &Consume_cb,
            2, inArgs,
            0, NULL,
            NULL, NULL);
    }
    printf("[LOG] Ready: discovery(None-only) + session(encrypted), no Anonymous\n");

    rc = UA_Server_run_startup(server);
    UA_UInt64 dhtCbId = 0;
    UA_StatusCode rc3 = UA_Server_addRepeatedCallback(server, log_dht_update_cb, NULL, 1200, &dhtCbId);
    printf("[LOG] startup rc=0x%08x\n", (unsigned)rc);
    fflush(stdout);

    /*----------------------------ModBus----------------------------------------*/
    UA_UInt64 mbCbId = 0;
    UA_StatusCode rcMb = UA_Server_addRepeatedCallback(server, modbus_poll_cb, NULL, 200 /*ms*/, &mbCbId);
    printf("[Log] modbus poll cb rc=0x%08x\n", (unsigned)rcMb);

    /*----------------------------------------------------------------------------------------*/


    if(rc != UA_STATUSCODE_GOOD) {
        printf("[LOG] STARTUP FAILED -> NO LISTEN\n");
        UA_Server_delete(server);
        UA_ByteString_clear(&serverCert);
        UA_ByteString_clear(&serverKey);
        UA_ByteString_clear(&trustMes);
        return 1;
    }

    printf("[LOG] running... Ctrl+C to stop\n");
    while(g_running) {
        UA_Server_run_iterate(server, false);
        usleep(10 * 1000); /* 10ms */
    }

    UA_Server_run_shutdown(server);
    UA_Server_delete(server);

    UA_ByteString_clear(&serverCert);
    UA_ByteString_clear(&serverKey);
    UA_ByteString_clear(&trustMes);
    mb_cleanup();

    printf("[LOG] shutdown complete\n");
    return 0;
}