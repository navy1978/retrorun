#pragma once

#include <stddef.h>

class status;

bool uiRenderOverlays(const void* frame, unsigned width, unsigned height, size_t pitch);
status* uiCurrentOverlays();
