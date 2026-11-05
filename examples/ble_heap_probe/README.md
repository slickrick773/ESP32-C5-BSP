# C5 legacy advertiser heap probe

Build with stock arduino-esp32 3.3.10:

```sh
arduino-cli compile --fqbn 'esp32:esp32:esp32c5:PSRAM=enabled,PartitionScheme=huge_app,CDCOnBoot=cdc' examples/ble_heap_probe
```

Use a dedicated test board or preserve its existing firmware before flashing.
This sketch replaces the board application; it does not initialize the display.
Read USB serial at 115200. WiFi is never started. A minimal GATT service is
registered and the advertising/empty scan-response payloads are configured before
the command prompt, so pressure tests isolate advertising enable. The advertiser is legacy, connectable,
general-discoverable, with flags + one 128-bit UUID and no scan response.
It is a controller-memory diagnostic, not an application protocol implementation.

Commands (newline terminated):

- `h`: snapshot heap.
- `p 12500`: allocate 256-byte INTERNAL|8BIT blocks until near the requested
  free-byte target. Inspect the actual result; the target is not the measurement.
- `a`: start advertising; print raw return codes and host advertising state.
- `s`: stop advertising.
- `r`: release all pressure blocks.
- `f`: release alternating pressure blocks, creating holes while retaining the
  intervening blocks. Compare largest blocks and total free before/after.
- `x`: reboot. Use this between independent **first-start** trials; stop/start
  retries can reuse controller allocations and do not establish a cold threshold.

`cap0` is INTERNAL, `cap1` INTERNAL|8BIT, and `cap2` INTERNAL|8BIT|DMA.
All figures are bytes. Failed-allocation reporting uses a callback that only
records numbers; it does not allocate or print from inside the allocator.
The last failure and cumulative count include deliberate pressure allocations;
compare counts immediately before/after advertising to attribute a new failure.
A missing callback does not rule out exhaustion of a controller-private pool.

For a threshold experiment: reboot, initialize, apply pressure, start once,
record both free and largest eligible block and any failure size/capabilities.
Repeat near the boundary with fresh boots, then compare fragmented heaps with
similar free totals. Confirm actual discovery with a second BLE device, followed
by application connection, subscriptions, transfers, disconnect/reconnect and
advertising restart in the full application before declaring bridge support.
