#pragma once
#include "wlr/types/wlr_output.h"
typedef struct Monitor { struct wlr_output *wlr_output; } Monitor;
Monitor *monitor_at_point(double x, double y);
