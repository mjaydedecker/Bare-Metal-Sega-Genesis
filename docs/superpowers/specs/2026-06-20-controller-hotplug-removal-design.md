# Controller Hotplug-Removal Handling — Design

**Date:** 2026-06-20
**Status:** IMPLEMENTED — pure `pad_reconcile` helper + host test (2e58bc8), wired into `Gamepad::Poll()` (03ed646); `test_pad_reconcile` passes. Pending hardware verification only.
**Related:** [[project-two-controllers]] (deferred item: "a yanked pad's last bitmask may
stick until reboot"); `src/input/gamepad.{h,cpp}`; M7 input.

## Summary

Make the input layer react to a USB controller being **unplugged**. Today `Gamepad::Poll()`
lazily acquires a pad and then never lets go: on removal the per-port button cache keeps its
last value (ghost inputs — a held direction sticks, menus auto-scroll), and because the
device slot stays non-null, a **re-plugged pad is never re-registered** and stays dead until
reboot.

Behavior on removal: **silent neutral + auto re-acquire.** The removed port's inputs go
neutral, no message and no pause; re-plugging restores that port. This matches how a real
console behaves.

## Background / Key Facts

- `Gamepad` manages two pads via `m_pDevice[MAX_PADS]` and a `static volatile unsigned
  s_buttons[MAX_PADS]` cache written asynchronously by per-pad status handlers
  (`Handler0`/`Handler1`). See [[project-two-controllers]].
- Circle removes the name-service entry on unplug: `CUSBGamePadDevice`'s destructor calls
  `CDeviceNameService::RemoveDevice("upad", …)` (`libs/circle/lib/usb/usbgamepad.cpp:50`).
  Therefore `GetDevice("upad", i+1, FALSE)` returns **NULL after removal**, which makes a
  poll-based reconcile reliable.
- Circle reuses the lowest free `upad` index, so a re-plugged pad returns to the **same
  port** it had.

## Approach (chosen)

**Poll-based reconcile** inside `Gamepad::Poll()`. Each frame, query `GetDevice` for every
port and reconcile against the cached pointer. All state stays local to `gamepad.cpp`; no
callbacks, no static flags, no `void*`-context plumbing. Detection latency is one poll cycle
(~16 ms at 60 fps), which is imperceptible for ghost-input suppression.

Rejected alternative — `RegisterRemovedHandler` callback: event-driven with zero latency, but
adds a static re-acquire flag, context casting, and handler lifecycle for a ≤16 ms benefit no
one can perceive in this loop. Not worth the extra moving parts.

## Detailed Design

Replace the acquire-only loop in `Gamepad::Poll()` with a reconcile loop:

```cpp
void Gamepad::Poll(void)
{
    static TGamePadStatusHandler *const handlers[MAX_PADS] = { Handler0, Handler1 };

    for (unsigned i = 0; i < MAX_PADS; i++)
    {
        CUSBGamePadDevice *dev = (CUSBGamePadDevice *)
            m_pNameService->GetDevice("upad", i + 1, FALSE);

        if (dev == 0)                       // unplugged (or never present)
        {
            if (m_pDevice[i] != 0)          // was present -> removed
            {
                s_buttons[i] = 0;           // kill ghost inputs
                m_pDevice[i] = 0;
            }
        }
        else if (dev != m_pDevice[i])       // first acquire or re-plug / swap
        {
            m_pDevice[i] = dev;
            s_buttons[i] = 0;
            dev->RegisterStatusHandler(handlers[i]);
        }
        // else: same device, nothing to do
    }
}
```

### Why each case is correct
- **Removed** (`dev == 0`, had a device): zeroing `s_buttons[i]` makes `Buttons(port)` and
  `MenuButtons()` read neutral immediately, ending ghost inputs within one frame. Clearing
  `m_pDevice[i]` lets the next case re-acquire on re-plug.
- **(Re)acquired** (`dev != cached`): covers first plug-in, re-plug after removal, and a
  device-object swap at the same index. We zero the cache (the handler will repopulate it on
  the next report) and register the per-slot status handler.
- **Unchanged** (`dev == cached`): no-op, as today.

### Memory safety
After removal the name-service entry is gone, so `GetDevice` returns NULL — we never compare
against or dereference the freed device pointer. We only ever dereference `dev` when it is a
live pointer freshly returned by `GetDevice`.

## Scope / Non-Goals

- **No public API change.** `Buttons`, `MenuButtons`, `IsPresent` keep their signatures;
  `IsPresent` simply becomes accurate (reflects `m_pDevice[port]`).
- **No settings/struct changes.**
- Still deferred (unchanged by this work): multitap / 4+ players, 3/6-button toggle,
  controller-agnostic auto-mapping.

## Testing

`Poll()` calls into Circle (`GetDevice`, `RegisterStatusHandler`), so it is **hardware-tested,
not unit-tested** — consistent with the existing code (only the pure `joypad_map`/`settings`
helpers have host tests, and this change touches neither). Manual hardware checks:

1. Hold a D-pad direction, unplug the pad mid-press → on-screen motion stops within ~a frame
   (no stuck input).
2. Re-plug the same pad → it works again and drives the **same** port.
3. With two pads, unplug P2 → P1 is unaffected; unplug P1 → P2 is unaffected.
4. Either pad still opens the pause menu (`MenuButtons()`) after a remove/re-plug cycle.
