// mfg_server.c  (Hardened OPC UA Server for open62541 v1.5.2)
//
// ✅ 목표(완성본):
//   1) Discovery(GetEndpoints 등)는 "None"으로만 허용 (그래야 클라이언트가 endpoint 목록을 볼 수 있음)
//      -> cfg->securityPolicyNoneDiscoveryOnly = true
//   2) 실제 통신(Read/Write/Call/Subscribe)은 반드시 암호화(Sign/SignAndEncrypt)로만
//      -> None 모드는 discovery에서만 동작
//   3) Anonymous 로그인 금지(Username/Password만) + endpoint token에서도 Anonymous 제거
//   4) 기존 기능(노드/메서드/DHT 업데이트) 유지
//
// Build (너 환경 pc 이슈 반영):
//   gcc -Wall -Wextra -O2 mfg_server.c -o mfg_server \
//     $(pkg-config --cflags open62541) \
//     -L/usr/local/lib -lopen62541 \
//     -lpthread -lm -lrt -lssl -lcrypto
//
// Run:
//   ./mfg_server

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <open62541/server.h>
#include <open62541/server_config_default.h>
#include <open62541/plugin/log_stdout.h>
#include <open62541/plugin/accesscontrol_default.h>
#define DEFECT_THRESHOLD 40  /* 10% */

/* ---------- Production state ---------- */
static UA_UInt64 g_prod_cb_id = 0;
static UA_Boolean g_prod_cb_active = false;

static uint64_t g_attempt_count = 0;
static uint64_t g_total_count = 0;
static uint64_t g_defect_count = 0;

static volatile sig_atomic_t g_running = 1;
static void on_sigint(int sig) { (void)sig; g_running = 0; }

/* ---- persistent users (MUST be static, not stack) ---- */
static UA_UsernamePasswordLogin g_users[1];
static UA_Boolean g_users_inited = false;
static UA_Boolean g_mfg_running = false;
static void init_users_once(void) {
    if(g_users_inited) return;
    g_users[0].username = UA_STRING("mes");
    g_users[0].password = UA_STRING("mespw_change_me");
    g_users_inited = true;
}

/* ---------- file loader ---------- */
static UA_ByteString loadFile(const char *path) {
    UA_ByteString bs = UA_BYTESTRING_NULL;
    FILE *f = fopen(path, "rb");
    if(!f) {
        fprintf(stderr, "[MFG] cannot open file: %s\n", path);
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
/*======cov speed->period======*/
static UA_UInt32 speed_to_period_ms(double speed_pct) {
    if(speed_pct < 0.0) speed_pct = 0.0;
    if(speed_pct > 100.0) speed_pct = 100.0;//max speed

    /* 100% => 5초/개, 50% => 10초/개
       공식: period = 5s * (100 / speed) */
    if(speed_pct < 1.0)
        return 0; /* 생산 정지 */

    double sec = 5.0 * (100.0 / speed_pct);
    double ms  = sec * 1000.0;

    if(ms > 600000.0) ms = 600000.0;     /* 10분 상한 */

    return (UA_UInt32)ms;
}

/* ---------- NodeIds ---------- */
static UA_NodeId STATUS_ID;
static UA_NodeId TEMP_ID;
static UA_NodeId HUM_ID;
static UA_NodeId SPEED_ID;
static UA_NodeId PROD_COUNT_ID;
static UA_NodeId DEFECT_COUNT_ID;
static UA_NodeId DEFECT_RATE_ID;
static UA_NodeId DEFECT_CODE_ID;
static UA_NodeId ATTEMPT_COUNT_ID;
/* =========read Speed_id helper===============*/
static double read_speed_pct(UA_Server *server) {
    double spd = 0.0;
    UA_Variant v; UA_Variant_init(&v);

    UA_StatusCode rc = UA_Server_readValue(server, SPEED_ID, &v);
    if(rc == UA_STATUSCODE_GOOD &&
       UA_Variant_hasScalarType(&v, &UA_TYPES[UA_TYPES_DOUBLE])) {
        spd = *(double*)v.data;
    }
    UA_Variant_clear(&v);

    /* 0~100 클램프 */
    if(spd < 0.0) spd = 0.0;
    if(spd > 100.0) spd = 100.0;
    return spd;

}
/*==========Method callback:produce=======*/
static void production_cb(UA_Server *server, void *data) {
    (void)data;

    if(!g_mfg_running)
        return;

    double spd = read_speed_pct(server);
    if(spd < 1.0)
        return;

    int roll = rand() % 100; /* 0..99 */
    UA_Boolean defect = (roll < DEFECT_THRESHOLD) ? UA_TRUE : UA_FALSE;
    UA_UInt16 defect_code = 0;
    if(defect) {
        defect_code = (UA_UInt16)(1 + (rand() % 3)); /* 1..3 */
    }
    g_attempt_count++;
    if(defect) {
        g_defect_count++;
    } else {
        g_total_count++;
    }

    UA_UInt64 prod_count_u64   = (UA_UInt64)g_total_count;
    UA_UInt64 defect_count_u64 = (UA_UInt64)g_defect_count;
    UA_UInt64 attempt_u64 = (UA_UInt64)g_attempt_count;
    UA_Double defect_rate = (g_attempt_count == 0) ? 0.0 :
        ((UA_Double)g_defect_count * 100.0 / (UA_Double)g_attempt_count);

    UA_Variant v1,v2,v3,v4;
    UA_Variant vA;
    UA_Variant_setScalar(&vA, &attempt_u64, &UA_TYPES[UA_TYPES_UINT64]);
    UA_Variant_setScalar(&v1, &prod_count_u64,   &UA_TYPES[UA_TYPES_UINT64]);
    UA_Variant_setScalar(&v2, &defect_count_u64, &UA_TYPES[UA_TYPES_UINT64]);
    UA_Variant_setScalar(&v3, &defect_rate,      &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_Variant_setScalar(&v4, &defect_code,      &UA_TYPES[UA_TYPES_UINT16]);

    UA_Server_writeValue(server, ATTEMPT_COUNT_ID, vA);
    UA_Server_writeValue(server, PROD_COUNT_ID,   v1);
    UA_Server_writeValue(server, DEFECT_COUNT_ID, v2);
    UA_Server_writeValue(server, DEFECT_RATE_ID,  v3);
    UA_Server_writeValue(server, DEFECT_CODE_ID,  v4);

    const char *codeStr =
        (defect_code==0) ? "OK" :
        (defect_code==1) ? "파손" :
        (defect_code==2) ? "기준미달" :
        (defect_code==3) ? "치수오차" : "UNKNOWN";
    printf("[MFG][PROD] attempt=%lu ok=%lu defect=%lu rate=%.2f%% code=%u(%s)\n",
       g_attempt_count,
       g_total_count,
       g_defect_count,
       defect_rate,
       defect_code,
       codeStr);
    fflush(stdout);
}

/* ---------- Method callback:StartOrder ---------- */
static UA_StatusCode
StartOrder_cb(UA_Server *server,
              const UA_NodeId *sessionId, void *sessionContext,
              const UA_NodeId *methodId, void *methodContext,
              const UA_NodeId *objectId, void *objectContext,
              size_t inputSize, const UA_Variant *input,
              size_t outputSize, UA_Variant *output) {
    (void)sessionId; (void)sessionContext; (void)methodId; (void)methodContext;
    (void)objectId; (void)objectContext; (void)outputSize; (void)output;

    if(inputSize != 1 || !UA_Variant_hasScalarType(&input[0], &UA_TYPES[UA_TYPES_STRING]))
        return UA_STATUSCODE_BADINVALIDARGUMENT;

    UA_String orderId = *(UA_String*)input[0].data;
    printf("[MFG] StartOrder called: %.*s\n", (int)orderId.length, (char*)orderId.data);
    /* 현재 컨베이어 속도 읽어서 출력 */
    {
        UA_Variant v; UA_Variant_init(&v);
        UA_StatusCode rc = UA_Server_readValue(server, SPEED_ID, &v);
        if(rc == UA_STATUSCODE_GOOD &&
        UA_Variant_hasScalarType(&v, &UA_TYPES[UA_TYPES_DOUBLE])) {
            double spd = *(double*)v.data;
            printf("[MFG] ConveyorSpeed = %.1f %%\n", spd);
        } else {
            printf("[MFG] ConveyorSpeed read failed: 0x%08x\n", (unsigned)rc);
        }
        UA_Variant_clear(&v);
    }
    /* status = "Running" */
    UA_String running = UA_STRING("Running");
    UA_Variant v;
    UA_Variant_init(&v);
    UA_Variant_setScalar(&v, &running, &UA_TYPES[UA_TYPES_STRING]);
    (void)UA_Server_writeValue(server, STATUS_ID, v);
    g_mfg_running = true;
    /* 생산 콜백 등록(속도 기반 주기) */
    if(!g_prod_cb_active) {
        double spd = read_speed_pct(server);
        UA_UInt32 period = speed_to_period_ms(spd);

        if(period == 0) {
            printf("[MFG] Production NOT started (speed=%.1f%%)\n", spd);
        } else {
            UA_StatusCode rc2 = UA_Server_addRepeatedCallback(server, production_cb, NULL, period, &g_prod_cb_id);
            if(rc2 == UA_STATUSCODE_GOOD) {
                g_prod_cb_active = true;
                printf("[MFG] Production started: speed=%.1f%% period=%ums\n", spd, (unsigned)period);
            } else {
                printf("[MFG] Production start failed: 0x%08x\n", (unsigned)rc2);
            }
        }
    }
    return UA_STATUSCODE_GOOD;
}

/* ---------- Method callback: StopOrder ---------- */
static UA_StatusCode
StopOrder_cb(UA_Server *server,
             const UA_NodeId *sessionId, void *sessionContext,
             const UA_NodeId *methodId, void *methodContext,
             const UA_NodeId *objectId, void *objectContext,
             size_t inputSize, const UA_Variant *input,
             size_t outputSize, UA_Variant *output) {
    (void)sessionId; (void)sessionContext; (void)methodId; (void)methodContext;
    (void)objectId; (void)objectContext; (void)inputSize; (void)input;
    (void)outputSize; (void)output;

    printf("[MFG] StopOrder called\n");

    /* status = "Stopped" */
    UA_String stopped = UA_STRING("Stopped");
    UA_Variant v;
    UA_Variant_init(&v);
    UA_Variant_setScalar(&v, &stopped, &UA_TYPES[UA_TYPES_STRING]);
    (void)UA_Server_writeValue(server, STATUS_ID, v);
    g_mfg_running = false;
    /* 생산 콜백 제거 */
    if(g_prod_cb_active) {
        UA_Server_removeRepeatedCallback(server, g_prod_cb_id);
        g_prod_cb_active = false;
        printf("[MFG] Production stopped\n");
    }
    /* 생산 통계 초기화 */
    g_attempt_count = 0;
    g_total_count   = 0;
    g_defect_count  = 0;
    return UA_STATUSCODE_GOOD;
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

    /* 1) Anonymous 차단 */
    if(userIdentityToken &&
       userIdentityToken->content.decoded.type == &UA_TYPES[UA_TYPES_ANONYMOUSIDENTITYTOKEN]) {
        return UA_STATUSCODE_BADIDENTITYTOKENINVALID;
    }

    /* 2) Username 토큰만 허용 */
    if(!userIdentityToken ||
       userIdentityToken->content.decoded.type != &UA_TYPES[UA_TYPES_USERNAMEIDENTITYTOKEN]) {
        return UA_STATUSCODE_BADIDENTITYTOKENINVALID;
    }

    UA_UserNameIdentityToken *tok =
        (UA_UserNameIdentityToken*)userIdentityToken->content.decoded.data;

    /* 3) username 검사 */
    UA_String expectUser = UA_STRING("mes");
    if(!UA_String_equal(&tok->userName, &expectUser))
        return UA_STATUSCODE_BADUSERACCESSDENIED;

    /* 4) password 검사
       - 대부분의 경우 tok->password는 "복호화된 바이트"로 들어온다.
       - 만약 환경에 따라 암호화 blob으로 들어오면, 아래 비교가 실패하고 접속이 거절될 것(안전측). */
    const char *expectPw = "mespw_change_me";
    size_t expectLen = strlen(expectPw);

    if(tok->password.length != expectLen ||
       memcmp(tok->password.data, expectPw, expectLen) != 0) {
        return UA_STATUSCODE_BADUSERACCESSDENIED;
    }

    return UA_STATUSCODE_GOOD;
}
/*==========defect======================*/
static UA_Boolean is_defect_fixed(int *outRoll) {
    int roll = rand() % 100;        // 0..99
    if(outRoll) *outRoll = roll;
    return (roll < DEFECT_THRESHOLD) ? UA_TRUE : UA_FALSE;
}

/* ---------- DHT11(IIO) helper ---------- */
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

/* 흔한 단위: milli (예: 23000 => 23.0C, 45000 => 45.0%) */
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

static void dht_update_cb(UA_Server *server, void *data) {
    (void)data;

    double t=0.0, h=0.0;
    if(read_dht11_iio(&t, &h) == 0) {
        UA_Variant vT, vH;
        UA_Variant_setScalar(&vT, &t, &UA_TYPES[UA_TYPES_DOUBLE]);
        UA_Variant_setScalar(&vH, &h, &UA_TYPES[UA_TYPES_DOUBLE]);
        (void)UA_Server_writeValue(server, TEMP_ID, vT);
        (void)UA_Server_writeValue(server, HUM_ID, vH);
    }
}

/* ---------- Security helpers ---------- */

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

/* ✅ Endpoint 배열을 "삭제/재구성"하지 않고, 토큰만 정리 (open62541 내부와 덜 충돌) */
static void harden_endpoints_tokens_only(UA_ServerConfig *cfg) {
    if(!cfg || !cfg->endpoints || cfg->endpointsSize == 0)
        return;
    for(size_t i=0; i<cfg->endpointsSize; i++)
        filter_user_tokens_no_anonymous(&cfg->endpoints[i]);
}

/* ✅ Anonymous 토큰이 남아있더라도 세션 Activate 단계에서 한 번 더 차단 */
static UA_StatusCode
activateSession_cb(UA_Server *server,
                   UA_AccessControl *ac,
                   const UA_EndpointDescription *endpointDescription,
                   const UA_ByteString *secureChannelRemoteCertificate,
                   const UA_NodeId *sessionId,
                   const UA_ExtensionObject *userIdentityToken,
                   void **sessionContext) {
    (void)server; (void)ac; (void)endpointDescription;
    (void)secureChannelRemoteCertificate; (void)sessionId; (void)sessionContext;

    if(userIdentityToken &&
       userIdentityToken->content.decoded.type == &UA_TYPES[UA_TYPES_ANONYMOUSIDENTITYTOKEN]) {
        return UA_STATUSCODE_BADIDENTITYTOKENINVALID;
    }
    return UA_STATUSCODE_GOOD;
}

static void configure_accesscontrol_no_anonymous(UA_ServerConfig *cfg) {
    init_users_once();

    UA_String *pwPolicyUri = findPolicyUri(cfg,
        "http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256");

    if(cfg->accessControl.clear)
        cfg->accessControl.clear(&cfg->accessControl);

    /* allowAnonymous=false (username/password만) */
    UA_AccessControl_default(cfg,
        false,        /* allowAnonymous */
        pwPolicyUri,  /* policyUri for encrypting credentials (can be NULL) */
        1, g_users);

    /* 추가 안전장치 */
    cfg->accessControl.activateSession = activateSession_cb;
}

int main(void) {
    signal(SIGINT, on_sigint);
    srand((unsigned)time(NULL));

    /* --- Security material --- */
    UA_ByteString serverCert = loadFile("/home/pi240/opcua_demo/certs/mfg/cert.der");
    UA_ByteString serverKey  = loadFile("/home/pi240/opcua_demo/certs/mfg/key.der");
    UA_ByteString trustMes   = loadFile("/home/pi240/opcua_demo/certs/mfg/trust_mes.der");

    if(serverCert.length == 0 || serverKey.length == 0 || trustMes.length == 0) {
        fprintf(stderr, "[MFG] cert/key/trust load failed (check file paths)\n");
        UA_ByteString_clear(&serverCert);
        UA_ByteString_clear(&serverKey);
        UA_ByteString_clear(&trustMes);
        return 1;
    }

    const UA_ByteString trustList[1] = { trustMes };

    UA_Server *server = UA_Server_new();
    UA_ServerConfig *cfg = UA_Server_getConfig(server);

    /* --- Secure config with policies (port 4840) --- */
    UA_StatusCode rc = UA_ServerConfig_setDefaultWithSecurityPolicies(
        cfg,
        4850,
        &serverCert,
        &serverKey,
        trustList, 1,   /* trusted client certs */
        NULL, 0,        /* issuer */
        NULL, 0         /* revocation */
    );

    /* ✅ rc 체크는 반드시 바로 */
    if(rc != UA_STATUSCODE_GOOD) {
        printf("[MFG] Security config failed: 0x%08x\n", (unsigned)rc);
        UA_Server_delete(server);
        UA_ByteString_clear(&serverCert);
        UA_ByteString_clear(&serverKey);
        UA_ByteString_clear(&trustMes);
        return 1;
    }

    /* ✅ 핵심: Discovery는 None으로만 허용 (GetEndpoints 가능하게) */
    cfg->securityPolicyNoneDiscoveryOnly = true;
    /* ✅ None에서 password 평문 허용 금지 */
    cfg->allowNonePolicyPassword = false;

    /* ✅ 익명 금지 + 엔드포인트 토큰에서 Anonymous 제거 */
    configure_accesscontrol_no_anonymous(cfg);
    harden_endpoints_tokens_only(cfg);
    cfg->accessControl.activateSession = activateSession_strict_cb;
    /* --- Address Space: Status(String), Temp(Double), Hum(Double) --- */
    STATUS_ID = UA_NODEID_STRING(1, "mfg/status");
    TEMP_ID   = UA_NODEID_STRING(1, "mfg/temp");
    HUM_ID    = UA_NODEID_STRING(1, "mfg/hum");
    SPEED_ID = UA_NODEID_STRING(1, "mfg/conveyor_speed");
    PROD_COUNT_ID   = UA_NODEID_STRING(1, "mfg/prod_count");
    DEFECT_COUNT_ID = UA_NODEID_STRING(1, "mfg/defect_count");
    DEFECT_RATE_ID  = UA_NODEID_STRING(1, "mfg/defect_rate");
    DEFECT_CODE_ID  = UA_NODEID_STRING(1, "mfg/defect_code");
    ATTEMPT_COUNT_ID = UA_NODEID_STRING(1, "mfg/attempt_count");
    /* Status node */
    {
        UA_VariableAttributes attr = UA_VariableAttributes_default;
        attr.displayName = UA_LOCALIZEDTEXT("en-US", "MFG_Status");
        UA_String init = UA_STRING("Idle");
        UA_Variant_setScalar(&attr.value, &init, &UA_TYPES[UA_TYPES_STRING]);

        rc = UA_Server_addVariableNode(server,
            STATUS_ID,
            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
            UA_QUALIFIEDNAME(1, "MFG_Status"),
            UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
            attr, NULL, NULL);

        if(rc != UA_STATUSCODE_GOOD)
            printf("[MFG] add status failed: 0x%08x\n", (unsigned)rc);
    }

    /* Temp node */
    {
        UA_VariableAttributes attr = UA_VariableAttributes_default;
        attr.displayName = UA_LOCALIZEDTEXT("en-US", "MFG_Temp");
        double t = 20.0;
        UA_Variant_setScalar(&attr.value, &t, &UA_TYPES[UA_TYPES_DOUBLE]);

        rc = UA_Server_addVariableNode(server,
            TEMP_ID,
            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
            UA_QUALIFIEDNAME(1, "MFG_Temp"),
            UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
            attr, NULL, NULL);

        if(rc != UA_STATUSCODE_GOOD)
            printf("[MFG] add temp failed: 0x%08x\n", (unsigned)rc);
    }

    /* Humidity node */
    {
        UA_VariableAttributes attr = UA_VariableAttributes_default;
        attr.displayName = UA_LOCALIZEDTEXT("en-US", "MFG_Humidity");
        double h = 50.0;
        UA_Variant_setScalar(&attr.value, &h, &UA_TYPES[UA_TYPES_DOUBLE]);

        rc = UA_Server_addVariableNode(server,
            HUM_ID,
            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
            UA_QUALIFIEDNAME(1, "MFG_Humidity"),
            UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
            attr, NULL, NULL);

        if(rc != UA_STATUSCODE_GOOD)
            printf("[MFG] add hum failed: 0x%08x\n", (unsigned)rc);
    }
    /* Conveyor Speed (0~100 %) */
    {

        UA_VariableAttributes attr = UA_VariableAttributes_default;
        attr.displayName = UA_LOCALIZEDTEXT("en-US", "MFG_ConveyorSpeed");

        double speed = 0.0;  // 초기 0%
        UA_Variant_setScalar(&attr.value, &speed, &UA_TYPES[UA_TYPES_DOUBLE]);

        /* Read/Write 허용 */
        attr.accessLevel =
            UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
        attr.userAccessLevel =
            UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;

        UA_StatusCode rc2 = UA_Server_addVariableNode(server,
            SPEED_ID,
            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
            UA_QUALIFIEDNAME(1, "MFG_ConveyorSpeed"),
            UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
            attr, NULL, NULL);

        if(rc2 != UA_STATUSCODE_GOOD)
            printf("[MFG] add speed failed: 0x%08x\n", (unsigned)rc2);
    }
    {
        UA_VariableAttributes a = UA_VariableAttributes_default;
        a.displayName = UA_LOCALIZEDTEXT("en-US", "MFG_ProdCount");
        UA_UInt64 v0 = 0;
        UA_Variant_setScalar(&a.value, &v0, &UA_TYPES[UA_TYPES_UINT64]);
        a.accessLevel = UA_ACCESSLEVELMASK_READ;
        a.userAccessLevel = UA_ACCESSLEVELMASK_READ;

        UA_StatusCode rc2 = UA_Server_addVariableNode(server,
            PROD_COUNT_ID,
            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
            UA_QUALIFIEDNAME(1, "MFG_ProdCount"),
            UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
            a, NULL, NULL);

        if(rc2 != UA_STATUSCODE_GOOD)
            printf("[MFG] add prod_count failed: 0x%08x\n", (unsigned)rc2);
    }

    /* defect_count (UInt64) */
    {
        UA_VariableAttributes a = UA_VariableAttributes_default;
        a.displayName = UA_LOCALIZEDTEXT("en-US", "MFG_DefectCount");
        UA_UInt64 v0 = 0;
        UA_Variant_setScalar(&a.value, &v0, &UA_TYPES[UA_TYPES_UINT64]);
        a.accessLevel = UA_ACCESSLEVELMASK_READ;
        a.userAccessLevel = UA_ACCESSLEVELMASK_READ;

        UA_StatusCode rc2 = UA_Server_addVariableNode(server,
            DEFECT_COUNT_ID,
            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
            UA_QUALIFIEDNAME(1, "MFG_DefectCount"),
            UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
            a, NULL, NULL);

        if(rc2 != UA_STATUSCODE_GOOD)
            printf("[MFG] add defect_count failed: 0x%08x\n", (unsigned)rc2);
    }

    /* defect_rate (Double) */
    {
        UA_VariableAttributes a = UA_VariableAttributes_default;
        a.displayName = UA_LOCALIZEDTEXT("en-US", "MFG_DefectRate");
        UA_Double v0 = 0.0;
        UA_Variant_setScalar(&a.value, &v0, &UA_TYPES[UA_TYPES_DOUBLE]);
        a.accessLevel = UA_ACCESSLEVELMASK_READ;
        a.userAccessLevel = UA_ACCESSLEVELMASK_READ;

        UA_StatusCode rc2 = UA_Server_addVariableNode(server,
            DEFECT_RATE_ID,
            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
            UA_QUALIFIEDNAME(1, "MFG_DefectRate"),
            UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
            a, NULL, NULL);

        if(rc2 != UA_STATUSCODE_GOOD)
            printf("[MFG] add defect_rate failed: 0x%08x\n", (unsigned)rc2);
    }

    /* defect_code (UInt16) : 0 OK, 1~3 error */
    {
        UA_VariableAttributes a = UA_VariableAttributes_default;
        a.displayName = UA_LOCALIZEDTEXT("en-US", "MFG_DefectCode");
        UA_UInt16 v0 = 0;
        UA_Variant_setScalar(&a.value, &v0, &UA_TYPES[UA_TYPES_UINT16]);
        a.accessLevel = UA_ACCESSLEVELMASK_READ;
        a.userAccessLevel = UA_ACCESSLEVELMASK_READ;

        UA_StatusCode rc2 = UA_Server_addVariableNode(server,
            DEFECT_CODE_ID,
            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
            UA_QUALIFIEDNAME(1, "MFG_DefectCode"),
            UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
            a, NULL, NULL);

        if(rc2 != UA_STATUSCODE_GOOD)
            printf("[MFG] add defect_code failed: 0x%08x\n", (unsigned)rc2);
    }
    /* attempt_count (UInt64) */
    {
        UA_VariableAttributes a = UA_VariableAttributes_default;
        a.displayName = UA_LOCALIZEDTEXT("en-US", "MFG_AttemptCount");
        UA_UInt64 v0 = 0;
        UA_Variant_setScalar(&a.value, &v0, &UA_TYPES[UA_TYPES_UINT64]);

        a.accessLevel = UA_ACCESSLEVELMASK_READ;
        a.userAccessLevel = UA_ACCESSLEVELMASK_READ;

        UA_StatusCode rc2 = UA_Server_addVariableNode(server,
            ATTEMPT_COUNT_ID,
            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
            UA_QUALIFIEDNAME(1, "MFG_AttemptCount"),
            UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
            a, NULL, NULL);

        if(rc2 != UA_STATUSCODE_GOOD)
            printf("[MFG] add attempt_count failed: 0x%08x\n", (unsigned)rc2);
    }


    /* Method: StartOrder(orderId:String) */
    {
        UA_Argument inArg;
        UA_Argument_init(&inArg);
        inArg.name = UA_STRING("orderId");
        inArg.dataType = UA_TYPES[UA_TYPES_STRING].typeId;
        inArg.valueRank = -1;

        UA_MethodAttributes ma = UA_MethodAttributes_default;
        ma.displayName = UA_LOCALIZEDTEXT("en-US", "StartOrder");
        ma.executable = true;
        ma.userExecutable = true;

        rc = UA_Server_addMethodNode(server,
            UA_NODEID_STRING(1, "mfg/StartOrder"),
            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
            UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
            UA_QUALIFIEDNAME(1, "StartOrder"),
            ma,
            &StartOrder_cb,
            1, &inArg,
            0, NULL,
            NULL, NULL);

        if(rc != UA_STATUSCODE_GOOD)
            printf("[MFG] add method failed: 0x%08x\n", (unsigned)rc);
    }
    /* Method: StopOrder() */
    {
        UA_MethodAttributes ma = UA_MethodAttributes_default;
        ma.displayName = UA_LOCALIZEDTEXT("en-US", "StopOrder");
        ma.executable = true;
        ma.userExecutable = true;

        UA_StatusCode rc2 = UA_Server_addMethodNode(server,
            UA_NODEID_STRING(1, "mfg/StopOrder"),
            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
            UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
            UA_QUALIFIEDNAME(1, "StopOrder"),
            ma,
            &StopOrder_cb,
            0, NULL,
            0, NULL,
            NULL, NULL);

        if(rc2 != UA_STATUSCODE_GOOD)
            printf("[MFG] add StopOrder failed: 0x%08x\n", (unsigned)rc2);
    }

    printf("[MFG] Ready: discovery(None-only) + session(encrypted), no Anonymous\n");

    /* --- IMPORTANT: startup opens the port --- */
    rc = UA_Server_run_startup(server);
    printf("[MFG] startup rc=0x%08x\n", (unsigned)rc);
    fflush(stdout);

    if(rc != UA_STATUSCODE_GOOD) {
        printf("[MFG] STARTUP FAILED -> NO LISTEN\n");
        UA_Server_delete(server);
        UA_ByteString_clear(&serverCert);
        UA_ByteString_clear(&serverKey);
        UA_ByteString_clear(&trustMes);
        return 1;
    }

    /* ---- Add DHT update timer ONCE ---- */
    UA_UInt64 cbId = 0;
    rc = UA_Server_addRepeatedCallback(server, dht_update_cb, NULL, 1200 /*ms*/, &cbId);
    printf("[MFG] addRepeatedCallback rc=0x%08x\n", (unsigned)rc);

    /* ---- Main server loop ---- */
    printf("[MFG] running... Ctrl+C to stop\n");
    while(g_running) {
        UA_Server_run_iterate(server, true);
    }

    /* ---- Shutdown ---- */
    UA_Server_run_shutdown(server);
    UA_Server_delete(server);

    UA_ByteString_clear(&serverCert);
    UA_ByteString_clear(&serverKey);
    UA_ByteString_clear(&trustMes);

    printf("[MFG] shutdown complete\n");
    return 0;
}