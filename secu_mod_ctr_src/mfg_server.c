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

// 왜 push 안되냐

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <modbus/modbus.h>

#include <open62541/server.h>
#include <open62541/server_config_default.h>
#include <open62541/plugin/log_stdout.h>
#include <open62541/plugin/accesscontrol_default.h>

static volatile sig_atomic_t g_running = 1;
static void on_sigint(int sig) { (void)sig; g_running = 0; }

/* ---- persistent users (MUST be static, not stack) ---- */
static UA_UsernamePasswordLogin g_users[1];
static UA_Boolean g_users_inited = false;
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


/* ---------- Modbus Address Enumeration ---------- */
// 1. 디지털 비트 주소 (%QX) - modbus_read_bits / write_bit용
enum SCM_COIL_ADDR {
    SCM_QX_START   = 0,  // %QX0.0
    SCM_QX_STOP    = 1,  // %QX0.1
    SCM_QX_AUTO    = 2,  // %QX0.2

    SCM_QX_RESET1  = 3,  // %QX0.3  
    SCM_QX_RESET2  = 11, // %QX1.3 
    SCM_QX_RESET3  = 19, // %QX2.3
    
    SCM_QX_RUN1    = 4   // %QX0.4
    SCM_QX_RUN2    = 12  // %QX1.4
    SCM_QX_RUN3    = 20  // %QX2.4

    SCM_QX_GREEN1  = 5,  // %QX0.5
    SCM_QX_RED1    = 6,  // %QX0.6
    SCM_QX_GREEN2  = 13, // %QX1.5
    SCM_QX_RED2    = 14, // %QX1.6
    SCM_QX_GREEN3  = 21, // %QX2.5
    SCM_QX_RED3    = 22  // %QX2.6
};

// 2. 아날로그 레지스터 주소 (%MW) - modbus_read_registers용
enum SCM_REG_ADDR {
    SCM_MW_COUNT1  = 1025, // %MW1 - 현재 개수
    SCM_MW_COUNT2  = 1026, // %MW2
    SCM_MW_COUNT3  = 1027, // %MW3
    SCM_MW_MAX1    = 1028, // %MW4 - 목표 개수
    SCM_MW_MAX1    = 1029, // %MW5
    SCM_MW_MAX1    = 1030, // %MW6
    SCM_MW_SPEED1  = 1031, // %MW7 - 컨베이어 속도 (Pulse )
    SCM_MW_SPEED2  = 1032, // %MW8
    SCM_MW_SPEED1  = 1033  // %MW9
};

/* ---------- NodeIds ---------- */


/* ---------- Modbus(OpenPLC) ---------- */
static UA_NodeId QX0_0_ID; //기존
static UA_NodeId QX0_1_ID;
static UA_NodeId QX0_2_ID;

static UA_NodeId SCM_COUNT1_ID, SCM_COUNT2_ID, SCM_COUNT3_ID;
static UA_NodeId SCM_SPEED1_ID, SCM_SPEED2_ID, SCM_SPEED3_ID,

static modbus_t *g_mb = NULL;
static int g_mb_connected = 0;

static int modbus_connect_once(void) {
    if(!g_mb) return -1;
    if(modbus_connect(g_mb) == -1) {
        fprintf(stderr, "[MFG][MODBUS] connect failed: %s\n", modbus_strerror(errno));
        g_mb_connected = 0;
        return -1;
    }
    g_mb_connected = 1;
    printf("[MFG][MODBUS] connected to OpenPLC (10.10.16.209:502)\n");
    return 0;
}

static void modbus_reconnect(void) {
    if(!g_mb) return;
    modbus_close(g_mb);
    usleep(200 * 1000);
    (void)modbus_connect_once();
}


static void openplc_qx_update_cb(UA_Server *server, void *data) {
    (void)data;
    if(!g_mb) return;

    if(!g_mb_connected) {
        if(modbus_connect_once() != 0)
            return;
    }

    uint16_t counts[3] = {0};
    int rc = modbus_read_registers(g_mb, SCM_MW_COUNT1, 3, counts);

    if(rc != 3) {
        fprintf(stderr, "[MFG][MODBUS] read_bits failed(rc=%d): %s\n",
            rc, modbus_strerror(errno));
        modbus_reconnect();
        return;
    }
/*  현규 기존 코드
    UA_Boolean b0 = counts[0] ? true : false;
    UA_Boolean b1 = counts[1] ? true : false;
    UA_Boolean b2 = counts[2] ? true : false;

    UA_Variant v0, v1, v2;
    UA_Variant_init(&v0); UA_Variant_init(&v1); UA_Variant_init(&v2);
    UA_Variant_setScalar(&v0, &b0, &UA_TYPES[UA_TYPES_BOOLEAN]);
    UA_Variant_setScalar(&v1, &b1, &UA_TYPES[UA_TYPES_BOOLEAN]);
    UA_Variant_setScalar(&v2, &b2, &UA_TYPES[UA_TYPES_BOOLEAN]);

    (void)UA_Server_writeValue(server, QX0_0_ID, v0);
    (void)UA_Server_writeValue(server, QX0_1_ID, v1);
    (void)UA_Server_writeValue(server, QX0_2_ID, v2);
*/
    for(int i = 0; i < 3; i++) {
        UA_Int16 val = (UA_Int16)counts[i];
        UA_Variant v;
        UA_Variant_init(&v);
        UA_Variant_setScalar(&v, &val, &UA_TYPES[UA_TYPES_INT16]);

        // i=0(COUNT1), i=1(COUNT2), i=2(COUNT3) 노드에 쓰기
        UA_NodeId targetId = (i == 0) ? SCM_COUNT1_ID : 
                             (i == 1) ? SCM_COUNT2_ID : SCM_COUNT3_ID;
        UA_Server_writeValue(server, targetId, v);
    }    
}

// start, stop, auto 활용
static void add_bool_node(UA_Server *server, UA_NodeId id,
                          const char *browseName, const char *displayName) {
    
                            UA_VariableAttributes attr = UA_VariableAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT("en-US", (char*)displayName);
    UA_Boolean init = false;
    UA_Variant_setScalar(&attr.value, &init, &UA_TYPES[UA_TYPES_BOOLEAN]);
    attr.dataType = UA_TYPES[UA_TYPES_BOOLEAN].typeId;

    /*------------------------------- modbus_write ---------------------------------*/
    attr.accessLevel     = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
    attr.userAccessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
    
    /* -------------------------------------------------------------------------- */
    UA_StatusCode rc2 = UA_Server_addVariableNode(server,
        id,
        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
        UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
        UA_QUALIFIEDNAME(1, (char*)browseName),
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
        attr, NULL, NULL);

    if(rc2 != UA_STATUSCODE_GOOD)
        printf("[MFG] add %s failed: 0x%08x\n", browseName, (unsigned)rc2);
}

static void add_int16_node(UA_Server *server, UA_NodeId id,
                           const char *browseName, const char *displayName,
                           UA_Byte accessLevel) {
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT("en-US", (char*)displayName);
    
    // 초기값 0 설정
    UA_Int16 init = 0;
    UA_Variant_setScalar(&attr.value, &init, &UA_TYPES[UA_TYPES_INT16]);
    attr.dataType = UA_TYPES[UA_TYPES_INT16].typeId;

    // 권한 설정 (읽기 전용 혹은 읽기/쓰기)
    attr.accessLevel = accessLevel;
    attr.userAccessLevel = accessLevel;

    UA_Server_addVariableNode(server, id,
        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
        UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
        UA_QUALIFIEDNAME(1, (char*)browseName),
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
        attr, NULL, NULL);
}

//---------------------------------------------------------------------------------



 /*------------------------------- modbus_write ---------------------------------*/
static UA_StatusCode ensure_modbus_connected(void) {
    if(!g_mb) return UA_STATUSCODE_BADINTERNALERROR;
    if(g_mb_connected) return UA_STATUSCODE_GOOD;
    if(modbus_connect_once() == 0) return UA_STATUSCODE_GOOD;
    return UA_STATUSCODE_BADCOMMUNICATIONERROR;
}



// server : 현재 서버 인스턴스
// sessionId/ sessionContext L 누가 썼는 지 세션 정보
// nodeId : 어떤 노드가 Weite 되엇는지 Qx0_0인지 qx_0_1인지
// nodeContext
// range : 배열 일부만 쓸 때 쓰는 범위(스칼라면 보통 무시)
// data : 클라이언트가 Write로 보낸 값(BOOL TRUE/FALSE가 여기 들어옴)

static void qx_on_write(UA_Server *server,
                        const UA_NodeId *sessionId, void *sessionContext,
                        const UA_NodeId *nodeId, void *nodeContext,
                        const UA_NumericRange *range, const UA_DataValue *data) {
    (void)server; (void)sessionId; (void)sessionContext; (void)nodeId; (void)range;

    if(!nodeContext || !data || !data->hasValue)
        return;

    if(!UA_Variant_hasScalarType(&data->value, &UA_TYPES[UA_TYPES_BOOLEAN]))
        return;

    int coilAddr = *(int*)nodeContext;              // 0,1,2...
    UA_Boolean b = *(UA_Boolean*)data->value.data;  // TRUE/FALSE

    // 모드버스가 끊겨있으면 연결 시도하고 실패하면 Write를 포기
    if(ensure_modbus_connected() != UA_STATUSCODE_GOOD)
        return;

    // OpenPLC에 Coil 쓰기 함수
    int rc = modbus_write_bit(g_mb, coilAddr, b ? 1 : 0);
    if(rc != 1) {
        fprintf(stderr, "[MFG][MODBUS] write_bit addr=%d failed: %s\n",
                coilAddr, modbus_strerror(errno));
        modbus_reconnect();
        return;
    }
    // printf("[MFG][MODBUS] write QX0.%d <- %s\n", coilAddr, b ? "TRUE" : "FALSE");
}



/* -------------------------------------------------------------------------- */

/* ---------- Method callback ---------- */
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

    /* status = "Running" */
    UA_String running = UA_STRING("Running");
    UA_Variant v;
    UA_Variant_init(&v);
    UA_Variant_setScalar(&v, &running, &UA_TYPES[UA_TYPES_STRING]);
    (void)UA_Server_writeValue(server, STATUS_ID, v);

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

/* ---------- Security helpers ---------- */

static UA_String* findPolicyUri(UA_ServerConfig *cfg, const char *uri) {
    if(!cfg || !cfg->securityPolicies || cfg->securityPoliciesSize == 0)
        return NULL;
    UA_String target = UA_STRING((char*)uri);
    for(size_t i=0; i<cfg->securityPoliciesSize; i++) {
        if(UA_String_equal(&cfg->securityPolicies[i].policyUri, &target))
            return &cfg->securityPolicies[i].policyUri;
    return NULL;
    }
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

    /* --- Security material --- */
    UA_ByteString serverCert = loadFile("/home/pi240/opcua_prac/certs/mfg/cert.der");
    UA_ByteString serverKey  = loadFile("/home/pi240/opcua_prac/certs/mfg/key.der");
    UA_ByteString trustMes   = loadFile("/home/pi240/opcua_prac/certs/mfg/trust_mes.der");

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
        4840,
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


     /*------------------------------------------- ModBus ---------------------------------------------*/
    QX0_0_ID = UA_NODEID_STRING(1, "mfg/qx0_0");
    QX0_1_ID = UA_NODEID_STRING(1, "mfg/qx0_1");
    QX0_2_ID = UA_NODEID_STRING(1, "mfg/qx0_2");


    add_bool_node(server, QX0_0_ID, "MFG_QX0_0", "MFG_QX0.0");
    add_bool_node(server, QX0_1_ID, "MFG_QX0_1", "MFG_QX0.1");
    add_bool_node(server, QX0_2_ID, "MFG_QX0_2", "MFG_QX0.2");

    /* ------------------------------------------modbus_write-------------------------------------------*/
    static int COIL_QX0_0 = 0;
    static int COIL_QX0_1 = 1;
    static int COIL_QX0_2 = 2;


    // 해당 노드에 사용자가 원하는 데이터(포인터)를 붙여 놓는 것
    // qx_on_write() 안에서 noedContext를 꺼내서 coil 주소를 알아야함.
    UA_Server_setNodeContext(server, QX0_0_ID, &COIL_QX0_0);   
    UA_Server_setNodeContext(server, QX0_1_ID, &COIL_QX0_1);
    UA_Server_setNodeContext(server, QX0_2_ID, &COIL_QX0_2);

    UA_ValueCallback cb;
    cb.onRead  = NULL;
    cb.onWrite = qx_on_write;

    // 이 노드에 Read/Write 이벤트 발생 시 실행할 콜백 함수 등록

    UA_Server_setVariableNode_valueCallback(server, QX0_0_ID, cb);
    UA_Server_setVariableNode_valueCallback(server, QX0_1_ID, cb);
    UA_Server_setVariableNode_valueCallback(server, QX0_2_ID, cb);

    /*-----------------------------------------------------------------------------------------------------*/

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

    /* ---- Modbus ctx (OpenPLC local) ---- */
    g_mb = modbus_new_tcp("10.10.16.209", 502);
    if(!g_mb) {
        fprintf(stderr, "[MFG][MODBUS] modbus_new_tcp failed\n");
    } else {
        modbus_set_slave(g_mb, 1); // TCP라도 보통 1로 둠(안해도 되는 경우 많음)
        // 첫 연결은 콜백에서 해도 되지만, 여기서 한번 해두면 로그가 깔끔
        (void)modbus_connect_once();
    }

    UA_UInt64 qxCbId = 0;
    rc = UA_Server_addRepeatedCallback(server, openplc_qx_update_cb, NULL, 200 /*ms*/, &qxCbId);
    printf("[MFG] add QX callback rc=0x%08x\n", (unsigned)rc);

    /*----------------------------------------------------------------------------------------------*/



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

    //testforgithub
}