//
// src/input/pad_device.cpp
//
#include "pad_device.h"

bool pad_use_6button(SegaPadType detected, PadType setting)
{
    if (detected == SegaPadType::SixButton)   return true;
    if (detected == SegaPadType::ThreeButton) return false;
    // None / Unsupported: no usable GPIO pad — honor the global setting.
    return setting == PadType::SixButton;
}
