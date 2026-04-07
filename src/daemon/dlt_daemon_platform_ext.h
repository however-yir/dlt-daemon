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

#ifndef DLT_DAEMON_PLATFORM_EXT_H
#define DLT_DAEMON_PLATFORM_EXT_H

#include <stdbool.h>

#include "dlt-daemon.h"

void dlt_daemon_platform_ext_init(const DltDaemonLocal *daemon_local);
bool dlt_daemon_platform_ext_allow_log_v1(DltDaemon *daemon,
                                          DltDaemonLocal *daemon_local,
                                          const DltMessage *msg);
bool dlt_daemon_platform_ext_allow_log_v2(DltDaemon *daemon,
                                          DltDaemonLocal *daemon_local,
                                          const DltMessageV2 *msgv2);
bool dlt_daemon_platform_ext_allow_control_client(DltDaemonLocal *daemon_local, int fd);

#endif /* DLT_DAEMON_PLATFORM_EXT_H */
