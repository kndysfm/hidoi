#pragma once

#include <windows.h>
#include <hidusage.h>
#include "hidusagex.h"

#include <memory>
#include <vector>
#include <functional>
#include "tstring.h"
#include "utils.h"
#if !defined _HIDOI_PARSER_CPP_ && !defined _HIDOI_PARSER_REPORT_CPP_
#include "parser.h"
#  ifndef _HIDOI_DEVICE_CPP_
#include "device.h"
#    ifndef _HIDOI_RI_CPP_
#include "ri.h"
#      ifndef _HIDOI_WATCHER_CPP_
#include "watcher.h"
#      endif
#    endif
#  endif
#endif