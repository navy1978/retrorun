#pragma once

#include <stddef.h>

class status;
struct rr_surface;
void uiDrawLoadingSurface(rr_surface* target, int width, int height);

bool uiRenderOverlays(const void* frame, unsigned width, unsigned height, size_t pitch);
status* uiCurrentOverlays();
void uiNotifyLoadingPresented();
bool uiLoadingMinimumDurationElapsed();
