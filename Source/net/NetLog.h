#pragma once

#include "axmol.h"

#define NET_LOGV(fmtOrMsg, ...) AXLOGV("[net] " fmtOrMsg, ##__VA_ARGS__)
#define NET_LOGD(fmtOrMsg, ...) AXLOGD("[net] " fmtOrMsg, ##__VA_ARGS__)
#define NET_LOGI(fmtOrMsg, ...) AXLOGI("[net] " fmtOrMsg, ##__VA_ARGS__)
#define NET_LOGW(fmtOrMsg, ...) AXLOGW("[net] " fmtOrMsg, ##__VA_ARGS__)
#define NET_LOGE(fmtOrMsg, ...) AXLOGE("[net] " fmtOrMsg, ##__VA_ARGS__)
