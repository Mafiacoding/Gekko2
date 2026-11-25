# BETA12.31e Wii IOS address ABI fix

Owner feedback on BETA12.31d shows SOCKET READY but send error -22, no sent
packets and no ACK. This establishes socket initialization progress, not packet
receipt. Do not diagnose Windows filtering from this screen.

Correct a confirmed sender/ACK ABI defect: public libogc sockaddr_in is 16 bytes
with BSD sin_zero padding, but IOS IPv4 sockaddr is 8 bytes. Unlike bind/connect,
libogc net_sendto forwards the supplied length unchanged. Send only the 8-byte
prefix (checked using offsetof), initialize receive storage, supply 8 bytes and
isolate its directly written peer buffer on a 32-byte cache line and
validate the returned length, sin_len, family, IP, port, boot and sequence. The
previous ACK length==16 check would reject normal IOS replies even if sent.
Keep protocol 0, IOS nonblock bit 4, bounded background queue and default OFF.
Saved endpoints, wire packets and Windows Logger 1.1 remain compatible.

## Evidence and limits

[libogc v3.1.0 network_wii.c](https://github.com/devkitPro/libogc/blob/v3.1.0/libogc/network_wii.c)
explicitly documents the 8-byte wii_sockaddr_in without sin_zero. Its bind/connect
wrappers force this length, but sendto does not. recvfrom copies the returned
sa_len to the caller's length. The delivered ELF confirms these behaviors.

Numeric -22 is ambiguous here: libogc net_malloc failure returns raw IPC_ENOMEM
(-22), while modeled IOS SO_EINVAL (-28) maps through libogc to errno EINVAL
(-22). The screenshot alone cannot prove which branch returned it. Do not claim
that physical receipt or the exclusive cause has been established.

The strengthened linked worker test rejects the old BETA12.31d ELF's 16-byte
address. Extend peer tests with wrong port/family and short address. Exercise
actual linked libogc sendto/recvfrom, the real heap allocator and bounce-buffer
copies against modeled IOS IPC, including address rejection, allocation failure,
repeated sending and the worker's matching ACK. These are host PPC simulations,
not real Wii networking. No core/JIT/GX/game/FPS change in this batch.

## Delivered build and validation

Wii build code: `c1a55aa`, devkitPPC r50 GCC16.1.0 / libogc3.1.0; existing
SPEED/DSI/AOT/GX/chain flags retained, experimental preferences unchanged.
Fresh build and DOL-to-ELF section comparison pass. MEM1 static end
`0x815e57b4`, remaining 2,205,772 bytes (minimum gate 1 MiB).

Actual outputs:

- PASS four existing CPU/menu/config suites, network protocol/collector,
  network menu and real EE/VU/GX profile work.
- PASS 33 actual linked PPC encoding/bounds/queue/ABI checks.
- PASS 58 actual linked PPC API/ABI checks, now including short peer,
  incorrect family and incorrect port; previous default-OFF and failure cases retained.
- PASS 4 linked libogc socket/fcntl IOS-boundary checks.
- PASS linked libogc UDP transport: real heap/bounce buffers and receive address
  size; persistent worker ACK through actual wrappers; allocation failure and
  64 repeated sends. IOS IPC and other platform services are modeled.
- Expected failure: BETA12.31d ELF violates the strengthened address-length test.

No new physical Wii or Dolphin run. Actual ACK receipt and exclusive cause of
owner's send error remain unconfirmed. Windows-native collector operation is
not newly validated here; real host UDP loopback is exercised by regression.

## Short owner check

Replace apps/Gekko2 with the new SD package, restart from HBC, retain saved
PC IP/port and Windows Logger 1.1. Start the listener, SAVE/APPLY, then SEND TEST
PACKET. Look for PC RECEIPT CONFIRMED. No BIOS or hour-long session required.
