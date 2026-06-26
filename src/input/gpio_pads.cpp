//
// src/input/gpio_pads.cpp
//
#include "gpio_pads.h"
#include <circle/timer.h>

// Settle time after toggling SELECT before sampling. The pad's HC mux switches
// in tens of ns; a few µs is generous and keeps a full 8-phase poll well under
// 100 µs — negligible against the ~16.67 ms frame.
#define SELECT_SETTLE_US 5

GpioPads::GpioPads(void)
{
    for (unsigned p = 0; p < NUM_PORTS; ++p)
    {
        m_Buttons[p] = 0;
        m_Type[p] = SegaPadType::None;
    }
}

void GpioPads::Init(void)
{
    for (unsigned p = 0; p < NUM_PORTS; ++p)
    {
        m_Select[p].AssignPin(kBoardPinMap.port[p].select);
        m_Select[p].SetMode(GPIOModeOutput);
        m_Select[p].Write(1);   // idle SELECT high
        for (unsigned d = 0; d < 6; ++d)
        {
            m_Data[p][d].AssignPin(kBoardPinMap.port[p].data[d]);
            m_Data[p][d].SetMode(GPIOModeInputPullUp);
        }
    }
}

void GpioPads::PollPort(unsigned port, SegaSample out[SEGA_PHASES])
{
    for (unsigned i = 0; i < SEGA_PHASES; ++i)
    {
        unsigned sel = (i & 1) ? 0 : 1;          // hi, lo, hi, lo, ...
        m_Select[port].Write(sel);
        CTimer::SimpleusDelay(SELECT_SETTLE_US);
        u8 data = 0;
        for (unsigned d = 0; d < 6; ++d)
            data |= (u8)((m_Data[port][d].Read() & 1u) << d);
        out[i].sel = (u8) sel;
        out[i].data = data;
    }
    m_Select[port].Write(1);   // leave SELECT idle high (lets the 6-btn counter reset)
}

void GpioPads::Poll(void)
{
    for (unsigned p = 0; p < NUM_PORTS; ++p)
    {
        SegaSample phases[SEGA_PHASES];
        PollPort(p, phases);
        SegaDecoded d = sega_decode(phases);
        m_Type[p] = d.type;
        m_Buttons[p] =
            (d.type == SegaPadType::ThreeButton || d.type == SegaPadType::SixButton)
            ? d.buttons : 0;
    }
}

unsigned GpioPads::Buttons(unsigned port) const
{
    return (port < NUM_PORTS) ? m_Buttons[port] : 0;
}

boolean GpioPads::IsPresent(unsigned port) const
{
    if (port >= NUM_PORTS) return FALSE;
    return (m_Type[port] == SegaPadType::ThreeButton
         || m_Type[port] == SegaPadType::SixButton) ? TRUE : FALSE;
}

SegaPadType GpioPads::PadTypeAt(unsigned port) const
{
    return (port < NUM_PORTS) ? m_Type[port] : SegaPadType::None;
}
