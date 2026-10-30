# RI codes: Onkyo TX-SR600E

*[Leer en español](ri-codes.es.md)*

Everything here was tested on one Onkyo TX-SR600E with the board in this repository. It is meant as a starting point for other models, whose codes differ.

## The protocol in short

- Each command is a **12-bit code, most significant bit first**, sent on a single wire (plus ground) at 3.3 V.
- Frame: a 3 ms high + 1 ms low header, then per bit 1 ms high followed by 1 ms low (bit 0) or 2 ms low (bit 1), then a 1 ms high trailer. The firmware sends each frame twice.
- The receiver **never reports its state**, so a remote cannot know whether it is on, muted or on which input it is.
- Most codes are only accepted **while the matching input is selected**. This is the rule behind the "power off only works on V3" behaviour.

## Verified on the TX-SR600E

| Code | Effect | How it was checked |
|---|---|---|
| `0x020` | Select CD | Read on the receiver's display |
| `0x070` | Select TAPE | Read on the display |
| `0x120` | Select DVD | Read on the display |
| `0x1A0` | Select V3 | Read on the display ("VIDEO3") |
| `0x12F` | Power on + DVD | Receiver powered on in DVD |
| `0x1AF` | Power on + V3 | Receiver powered on in V3 when the TV turned on |
| `0x1AE` | Power off | Works from V3, ignored from DVD |
| `0x1A2` / `0x1A3` | Volume up / down | Works on V3 |
| `0x1A4` / `0x1A5` | Mute / unmute | The "MUTING" indicator lights up and goes out |

## Tried with no effect

- Every other input-select code from `0x010` to `0x1F0` in steps of `0x10`. **V1, V2, V4, TUNER and PHONO cannot be selected over RI.**
- `0x12E` (DVD off), `0x02E` (CD off), `0x07E` (TAPE off): the receiver does not power off.
- `0x002`, `0x003` (volume on amplifiers), `0x004`, `0x0DA`, `0x0EA` (power), `0x005` (mute), `0x006`–`0x00B`, `0x013`, `0x0E3` (inputs) and `0x0D5` / `0x0D6` (next / previous input): no visible effect. The volume number on the display did not change after eight steps of `0x002` and `0x003`.
- `0x022`–`0x025`, `0x072`–`0x075`, `0x122`–`0x125` (volume and mute of the CD, TAPE and DVD families), tried with the matching input selected: no effect.
- `0x02F` is "power on + CD", not an off command.

## Never send `0x420`–`0x43F`

These are test and service codes. On the closely related TX-SR605 they open the setup screen or play loud test noise, and on other Onkyo models some of them clear the receiver's settings. On the TX-SR600E, `0x420` alone had no visible or audible effect (tried twice), but that says nothing about the rest of the range. The firmware refuses to send anything above `0x1FF`.

## Mapping another receiver

1. Put a webcam, or just your eyes, on the receiver's front display, and the receiver on a known input at a low volume.
2. Flash the firmware and send the input-select candidates one by one, three seconds apart:

   ```bash
   for n in $(seq 16 16 496); do
     curl -s -u admin:<password> "http://<board-ip>/raw?code=$(printf %X $n)"; echo " 0x$(printf %03X $n)"; sleep 3
   done
   ```

3. Note which codes change the input. Then, with that input selected, try the neighbouring codes: `+2` and `+3` for volume, `+4` and `+5` for mute, `+E` for power off and `+F` for power on.
4. Do not go above `0x1FF`, and do not try anything in `0x420`–`0x43F`.

If you map a model, please open an issue with your table so it can be added here.

## Where the codes come from

- [docbender/Onkyo-RI](https://github.com/docbender/Onkyo-RI) and its wiki: protocol and per-model code lists (mostly a TX-8020).
- The TX-SR605 table in [node-red-contrib-onkyo-ri](https://github.com/dajuly20/node-red-contrib-onkyo-ri) (`resources/RiCodes.md`): the closest model, and it matches the table above.
- [victorjacobs/onkyo-mqtt](https://github.com/victorjacobs/onkyo-mqtt): codes for an A-9010 amplifier, which did nothing on the receiver.
