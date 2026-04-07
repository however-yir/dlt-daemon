/*
 * SPDX license identifier: MPL-2.0
 *
 * Copyright (C) 2026, MyCo
 *
 * This file is part of COVESA Project DLT - Diagnostic Log and Trace.
 *
 * This Source Code Form is subject to the terms of the
 * Mozilla Public License (MPL), v. 2.0.
 * If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * For further information see http://www.covesa.org/.
 */

#include "dlt_daemon_platform_ext.h"

#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>

#include "dlt_common.h"

#define DLT_PLATFORM_MAX_APPS 256

typedef struct
{
    bool in_use;
    char appid[DLT_V2_ID_SIZE + 1];
    unsigned int tokens;
    time_t last_refill;
} DltPlatformRateBucket;

typedef struct
{
    bool initialized;

    int backpressure_enabled;
    unsigned int backpressure_high_watermark;
    unsigned int backpressure_hard_limit;
    int backpressure_drop_mtin_threshold;
    int degrade_on_overload;

    unsigned int app_rate_limit_per_second;
    unsigned int app_rate_limit_burst;

    int control_auth_mode;
    char control_auth_allowlist[DLT_DAEMON_FLAG_MAX];

    int json_export_enable;
    char json_export_path[DLT_PATH_MAX];

    int prometheus_enable;
    char prometheus_path[DLT_PATH_MAX];
    time_t last_metrics_write;

    bool degraded_mode;

    uint64_t messages_allowed;
    uint64_t messages_dropped;
    uint64_t dropped_backpressure;
    uint64_t dropped_quota;
    uint64_t control_denied;

    DltPlatformRateBucket buckets[DLT_PLATFORM_MAX_APPS];
} DltPlatformExtState;

static DltPlatformExtState g_dlt_platform;

static void dlt_platform_trim(char *text)
{
    char *start = text;
    char *end = NULL;

    if (text == NULL)
        return;

    while (*start == ' ' || *start == '\t')
        start++;

    if (start != text)
        memmove(text, start, strlen(start) + 1U);

    end = text + strlen(text);

    while ((end > text) && (end[-1] == ' ' || end[-1] == '\t'))
        end--;

    *end = '\0';
}

static void dlt_platform_copy_identifier(char *dst,
                                         size_t dst_size,
                                         const char *src,
                                         size_t src_len)
{
    size_t i = 0U;
    size_t copy_len = 0U;

    if ((dst == NULL) || (dst_size == 0U))
        return;

    dst[0] = '\0';

    if ((src == NULL) || (src_len == 0U))
        return;

    copy_len = src_len;

    if (copy_len > dst_size - 1U)
        copy_len = dst_size - 1U;

    for (i = 0U; i < copy_len; ++i) {
        const unsigned char c = (unsigned char)src[i];

        if (c < 32U || c > 126U)
            dst[i] = '_';
        else
            dst[i] = (char)c;
    }

    dst[copy_len] = '\0';
}

static bool dlt_platform_allowlist_contains(const char *ip)
{
    char work[DLT_DAEMON_FLAG_MAX];
    char *entry = NULL;
    char *saveptr = NULL;

    if ((ip == NULL) || (ip[0] == '\0'))
        return false;

    strncpy(work,
            g_dlt_platform.control_auth_allowlist,
            sizeof(work) - 1U);
    work[sizeof(work) - 1U] = '\0';

    entry = strtok_r(work, ",;", &saveptr);

    while (entry != NULL) {
        dlt_platform_trim(entry);

        if (strcmp(entry, ip) == 0)
            return true;

        entry = strtok_r(NULL, ",;", &saveptr);
    }

    return false;
}

static bool dlt_platform_is_loopback(const char *ip)
{
    if (ip == NULL)
        return false;

    return (strncmp(ip, "127.", 4U) == 0) || (strcmp(ip, "::1") == 0);
}

static void dlt_platform_write_metrics_file(void)
{
    char tmp_path[DLT_PATH_MAX + 5U];
    FILE *f = NULL;

    if (!g_dlt_platform.prometheus_enable ||
        (g_dlt_platform.prometheus_path[0] == '\0'))
        return;

    if ((strlen(g_dlt_platform.prometheus_path) + 4U) >= sizeof(tmp_path))
        return;

    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", g_dlt_platform.prometheus_path);

    f = fopen(tmp_path, "w");

    if (f == NULL)
        return;

    fprintf(f,
            "# TYPE dlt_platform_messages_allowed_total counter\n"
            "dlt_platform_messages_allowed_total %llu\n",
            (unsigned long long)g_dlt_platform.messages_allowed);
    fprintf(f,
            "# TYPE dlt_platform_messages_dropped_total counter\n"
            "dlt_platform_messages_dropped_total %llu\n",
            (unsigned long long)g_dlt_platform.messages_dropped);
    fprintf(f,
            "# TYPE dlt_platform_messages_dropped_backpressure_total counter\n"
            "dlt_platform_messages_dropped_backpressure_total %llu\n",
            (unsigned long long)g_dlt_platform.dropped_backpressure);
    fprintf(f,
            "# TYPE dlt_platform_messages_dropped_quota_total counter\n"
            "dlt_platform_messages_dropped_quota_total %llu\n",
            (unsigned long long)g_dlt_platform.dropped_quota);
    fprintf(f,
            "# TYPE dlt_platform_control_denied_total counter\n"
            "dlt_platform_control_denied_total %llu\n",
            (unsigned long long)g_dlt_platform.control_denied);
    fprintf(f,
            "# TYPE dlt_platform_degraded_mode gauge\n"
            "dlt_platform_degraded_mode %d\n",
            g_dlt_platform.degraded_mode ? 1 : 0);

    fclose(f);
    (void)rename(tmp_path, g_dlt_platform.prometheus_path);
}

static void dlt_platform_maybe_write_metrics(void)
{
    time_t now = time(NULL);

    if (!g_dlt_platform.prometheus_enable)
        return;

    if (now == (time_t)-1)
        return;

    if (g_dlt_platform.last_metrics_write == now)
        return;

    g_dlt_platform.last_metrics_write = now;
    dlt_platform_write_metrics_file();
}

static void dlt_platform_json_export(const char *version,
                                     const char *apid,
                                     const char *ctid,
                                     int mtin)
{
    FILE *f = NULL;
    char tsbuf[64] = { 0 };
    time_t now = time(NULL);
    struct tm tm_now;

    if (!g_dlt_platform.json_export_enable ||
        (g_dlt_platform.json_export_path[0] == '\0'))
        return;

    if (now == (time_t)-1)
        return;

    memset(&tm_now, 0, sizeof(tm_now));

    if (gmtime_r(&now, &tm_now) == NULL)
        return;

    if (strftime(tsbuf, sizeof(tsbuf), "%Y-%m-%dT%H:%M:%SZ", &tm_now) == 0U)
        return;

    f = fopen(g_dlt_platform.json_export_path, "a");

    if (f == NULL)
        return;

    fprintf(f,
            "{\"ts\":\"%s\",\"version\":\"%s\",\"apid\":\"%s\",\"ctid\":\"%s\",\"mtin\":%d}\n",
            tsbuf,
            (version == NULL) ? "unknown" : version,
            (apid == NULL) ? "" : apid,
            (ctid == NULL) ? "" : ctid,
            mtin);

    fclose(f);
}

static DltPlatformRateBucket *dlt_platform_get_bucket(const char *appid)
{
    size_t i = 0U;
    size_t free_slot = DLT_PLATFORM_MAX_APPS;

    for (i = 0U; i < DLT_PLATFORM_MAX_APPS; ++i) {
        if (g_dlt_platform.buckets[i].in_use) {
            if (strncmp(g_dlt_platform.buckets[i].appid,
                        appid,
                        sizeof(g_dlt_platform.buckets[i].appid)) == 0) {
                return &g_dlt_platform.buckets[i];
            }
        } else if (free_slot == DLT_PLATFORM_MAX_APPS) {
            free_slot = i;
        }
    }

    if (free_slot >= DLT_PLATFORM_MAX_APPS)
        return NULL;

    g_dlt_platform.buckets[free_slot].in_use = true;
    strncpy(g_dlt_platform.buckets[free_slot].appid,
            appid,
            sizeof(g_dlt_platform.buckets[free_slot].appid) - 1U);
    g_dlt_platform.buckets[free_slot].tokens =
        (g_dlt_platform.app_rate_limit_burst > 0U) ?
        g_dlt_platform.app_rate_limit_burst :
        g_dlt_platform.app_rate_limit_per_second;
    g_dlt_platform.buckets[free_slot].last_refill = time(NULL);

    return &g_dlt_platform.buckets[free_slot];
}

static bool dlt_platform_allow_by_quota(const char *appid)
{
    time_t now;
    unsigned int burst;
    DltPlatformRateBucket *bucket;

    if (g_dlt_platform.app_rate_limit_per_second == 0U)
        return true;

    if ((appid == NULL) || (appid[0] == '\0'))
        return true;

    bucket = dlt_platform_get_bucket(appid);

    if (bucket == NULL)
        return true;

    now = time(NULL);

    if (now == (time_t)-1)
        return true;

    burst = (g_dlt_platform.app_rate_limit_burst > 0U) ?
            g_dlt_platform.app_rate_limit_burst :
            g_dlt_platform.app_rate_limit_per_second;

    if (bucket->last_refill == (time_t)-1)
        bucket->last_refill = now;

    if (now > bucket->last_refill) {
        const time_t elapsed = now - bucket->last_refill;
        uint64_t refill_u64 = (uint64_t)elapsed *
                              (uint64_t)g_dlt_platform.app_rate_limit_per_second;

        if (refill_u64 > (uint64_t)burst)
            refill_u64 = (uint64_t)burst;

        if ((uint64_t)bucket->tokens + refill_u64 > (uint64_t)burst)
            bucket->tokens = burst;
        else
            bucket->tokens = (unsigned int)((uint64_t)bucket->tokens + refill_u64);

        bucket->last_refill = now;
    }

    if (bucket->tokens == 0U)
        return false;

    bucket->tokens--;
    return true;
}

static bool dlt_platform_allow_by_backpressure(DltDaemon *daemon, int mtin)
{
    int count = 0;

    if (!g_dlt_platform.backpressure_enabled || (daemon == NULL))
        return true;

    count = dlt_buffer_get_message_count(&(daemon->client_ringbuffer));

    if (count < 0)
        count = 0;

    if ((g_dlt_platform.backpressure_hard_limit > 0U) &&
        ((unsigned int)count >= g_dlt_platform.backpressure_hard_limit)) {
        g_dlt_platform.degraded_mode = (g_dlt_platform.degrade_on_overload != 0);
        return false;
    }

    if ((g_dlt_platform.backpressure_high_watermark > 0U) &&
        ((unsigned int)count >= g_dlt_platform.backpressure_high_watermark)) {
        g_dlt_platform.degraded_mode = (g_dlt_platform.degrade_on_overload != 0);

        if (mtin > g_dlt_platform.backpressure_drop_mtin_threshold)
            return false;

        return true;
    }

    g_dlt_platform.degraded_mode = false;
    return true;
}

static bool dlt_platform_allow_log_common(DltDaemon *daemon,
                                          const char *version,
                                          const char *appid,
                                          const char *ctid,
                                          int mtin)
{
    if (!g_dlt_platform.initialized)
        return true;

    if (!dlt_platform_allow_by_quota(appid)) {
        g_dlt_platform.messages_dropped++;
        g_dlt_platform.dropped_quota++;
        dlt_platform_maybe_write_metrics();
        return false;
    }

    if (!dlt_platform_allow_by_backpressure(daemon, mtin)) {
        g_dlt_platform.messages_dropped++;
        g_dlt_platform.dropped_backpressure++;
        dlt_platform_maybe_write_metrics();
        return false;
    }

    g_dlt_platform.messages_allowed++;
    dlt_platform_json_export(version, appid, ctid, mtin);
    dlt_platform_maybe_write_metrics();
    return true;
}

void dlt_daemon_platform_ext_init(const DltDaemonLocal *daemon_local)
{
    if (daemon_local == NULL)
        return;

    memset(&g_dlt_platform, 0, sizeof(g_dlt_platform));

    g_dlt_platform.backpressure_enabled = daemon_local->flags.backpressureEnabled;
    g_dlt_platform.backpressure_high_watermark = daemon_local->flags.backpressureHighWatermark;
    g_dlt_platform.backpressure_hard_limit = daemon_local->flags.backpressureHardLimit;
    g_dlt_platform.backpressure_drop_mtin_threshold = daemon_local->flags.backpressureDropMtinThreshold;
    g_dlt_platform.degrade_on_overload = daemon_local->flags.degradeOnOverload;

    g_dlt_platform.app_rate_limit_per_second = daemon_local->flags.appRateLimitPerSecond;
    g_dlt_platform.app_rate_limit_burst = daemon_local->flags.appRateLimitBurst;

    g_dlt_platform.control_auth_mode = daemon_local->flags.controlAuthMode;
    strncpy(g_dlt_platform.control_auth_allowlist,
            daemon_local->flags.controlAuthAllowlist,
            sizeof(g_dlt_platform.control_auth_allowlist) - 1U);

    g_dlt_platform.json_export_enable = daemon_local->flags.jsonExportEnable;
    strncpy(g_dlt_platform.json_export_path,
            daemon_local->flags.jsonExportPath,
            sizeof(g_dlt_platform.json_export_path) - 1U);

    g_dlt_platform.prometheus_enable = daemon_local->flags.prometheusMetricsEnable;
    strncpy(g_dlt_platform.prometheus_path,
            daemon_local->flags.prometheusMetricsPath,
            sizeof(g_dlt_platform.prometheus_path) - 1U);

    if (g_dlt_platform.backpressure_drop_mtin_threshold < DLT_LOG_OFF)
        g_dlt_platform.backpressure_drop_mtin_threshold = DLT_LOG_INFO;
    if (g_dlt_platform.backpressure_drop_mtin_threshold > DLT_LOG_VERBOSE)
        g_dlt_platform.backpressure_drop_mtin_threshold = DLT_LOG_VERBOSE;

    g_dlt_platform.initialized = true;
    dlt_platform_write_metrics_file();
}

bool dlt_daemon_platform_ext_allow_log_v1(DltDaemon *daemon,
                                          DltDaemonLocal *daemon_local,
                                          const DltMessage *msg)
{
    char apid[DLT_V2_ID_SIZE + 1U] = { 0 };
    char ctid[DLT_V2_ID_SIZE + 1U] = { 0 };
    int mtin = DLT_LOG_INFO;

    (void)daemon_local;

    if ((msg != NULL) && (msg->extendedheader != NULL)) {
        dlt_platform_copy_identifier(apid,
                                     sizeof(apid),
                                     msg->extendedheader->apid,
                                     DLT_ID_SIZE);
        dlt_platform_copy_identifier(ctid,
                                     sizeof(ctid),
                                     msg->extendedheader->ctid,
                                     DLT_ID_SIZE);
        mtin = DLT_GET_MSIN_MTIN(msg->extendedheader->msin);
    }

    return dlt_platform_allow_log_common(daemon, "v1", apid, ctid, mtin);
}

bool dlt_daemon_platform_ext_allow_log_v2(DltDaemon *daemon,
                                          DltDaemonLocal *daemon_local,
                                          const DltMessageV2 *msgv2)
{
    char apid[DLT_V2_ID_SIZE + 1U] = { 0 };
    char ctid[DLT_V2_ID_SIZE + 1U] = { 0 };
    int mtin = DLT_LOG_INFO;

    (void)daemon_local;

    if (msgv2 != NULL) {
        dlt_platform_copy_identifier(apid,
                                     sizeof(apid),
                                     msgv2->extendedheaderv2.apid,
                                     msgv2->extendedheaderv2.apidlen);
        dlt_platform_copy_identifier(ctid,
                                     sizeof(ctid),
                                     msgv2->extendedheaderv2.ctid,
                                     msgv2->extendedheaderv2.ctidlen);
        mtin = DLT_GET_MSIN_MTIN(msgv2->headerextrav2.msin);
    }

    return dlt_platform_allow_log_common(daemon, "v2", apid, ctid, mtin);
}

bool dlt_daemon_platform_ext_allow_control_client(DltDaemonLocal *daemon_local, int fd)
{
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);
    char ip[INET6_ADDRSTRLEN] = { 0 };

    (void)daemon_local;

    if (!g_dlt_platform.initialized)
        return true;

    if (g_dlt_platform.control_auth_mode <= 0)
        return true;

    memset(&addr, 0, sizeof(addr));

    if (getpeername(fd, (struct sockaddr *)&addr, &addr_len) < 0) {
        g_dlt_platform.control_denied++;
        dlt_platform_maybe_write_metrics();
        return false;
    }

    if (addr.ss_family == AF_UNIX)
        return true;

    if (addr.ss_family == AF_INET) {
        const struct sockaddr_in *addr4 = (const struct sockaddr_in *)&addr;

        if (inet_ntop(AF_INET, &addr4->sin_addr, ip, sizeof(ip)) == NULL)
            ip[0] = '\0';
    } else if (addr.ss_family == AF_INET6) {
        const struct sockaddr_in6 *addr6 = (const struct sockaddr_in6 *)&addr;

        if (inet_ntop(AF_INET6, &addr6->sin6_addr, ip, sizeof(ip)) == NULL)
            ip[0] = '\0';
    } else {
        g_dlt_platform.control_denied++;
        dlt_platform_maybe_write_metrics();
        return false;
    }

    if (g_dlt_platform.control_auth_mode == 1) {
        const bool allowed = dlt_platform_is_loopback(ip);

        if (!allowed) {
            g_dlt_platform.control_denied++;
            dlt_platform_maybe_write_metrics();
        }

        return allowed;
    }

    if (g_dlt_platform.control_auth_mode == 2) {
        const bool allowed = dlt_platform_allowlist_contains(ip);

        if (!allowed) {
            g_dlt_platform.control_denied++;
            dlt_platform_maybe_write_metrics();
        }

        return allowed;
    }

    return true;
}
