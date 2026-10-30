# onkyo-ri-wifi

*[Leer en español](README.es.md)*

Bring an older Onkyo receiver into the Wi-Fi era with a €5 ESP8266 board.

- **Follows your TV**: when a Samsung (Tizen) TV turns on, the receiver turns on and selects the TV input; when the TV turns off, the receiver goes to standby.
- **Phone-friendly web remote**: power, input, volume and mute from any browser on your network.
- **Updates over Wi-Fi (OTA)**: once flashed, you never need the USB cable again.

It talks **Onkyo RI** (Remote Interactive), the wired 3.5 mm "REMOTE CONTROL" jack on the back of many Onkyo receivers, so there is no IR LED to aim.

Tested on an **Onkyo TX-SR600E**, a **Samsung QE75Q70R**, a NodeMCU V3 (CH340), `arduino-cli` 1.5.1 and ESP8266 core 3.1.2. Other Onkyo models use different codes (see [Known limits](#known-limits)).

> Unofficial project, not affiliated with Onkyo or Samsung. You connect hardware to your receiver at your own risk.

## What you need

- A NodeMCU V3 (any ESP8266 board with a free GPIO works)
- A 3.5 mm **mono** plug cable (or one you can cut and re-wire)
- An Onkyo receiver with a **RI REMOTE CONTROL** jack
- A Samsung TV that answers the Tizen API on port 8001 (tested on a 2019 QLED; other models are untested)
- A USB charger to power the board

## Wiring

The RI jack is a mono 3.5 mm socket: **tip = data, sleeve = ground**.

```
NodeMCU                     Receiver "RI REMOTE CONTROL"
D2 (GPIO4)  ───────────────  tip     (signal)
GND         ───────────────  sleeve  (ground)
```

Cable colours vary: check with a multimeter which wire goes to the tip and which to the sleeve. In the author's cable the red wire is the one tied to GND.

The ESP8266 outputs 3.3 V and that is enough for the TX-SR600E.

## Receiver setup

The TV must reach the receiver on an input that RI can control properly. On the TX-SR600E that is **V3** (see the code table below). In the receiver's setup menu, assign the TV's **optical input (OPT 1) to V3**. Check your receiver manual for the exact menu.

## Install

```bash
# 1. Tools
arduino-cli config add board_manager.additional_urls https://arduino.esp8266.com/stable/package_esp8266com_index.json
arduino-cli core update-index
arduino-cli core install esp8266:esp8266

# 2. Settings: copy the example and fill it in
cd firmware/onkyo-ri-wifi
cp secrets.example.h secrets.h        # Wi-Fi name and password, TV IP, admin password

# 3. First flash, by USB
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 .
arduino-cli upload  --fqbn esp8266:esp8266:nodemcuv2 -p /dev/cu.usbserial-XXXX .
```

On macOS, the CH340 chip needs the WCH driver on older systems. Give the TV a fixed address: **reserve its IP in your router's DHCP settings** so it never changes.

### Updating over Wi-Fi

```bash
arduino-cli upload -p <board-ip> --protocol network \
  --fqbn esp8266:esp8266:nodemcuv2 \
  --upload-field password=<your admin password> .
```

Keep OTA in every version you flash. If you ever flash one without it, you will need the USB cable again.

On macOS with the firewall on, the board must be able to connect back to your computer. If the upload ends with `No response from device`, allow incoming connections for the `python3` bundled in `~/Library/Arduino15/packages/esp8266/tools/python3/`.

## Passwords

- **Never commit `secrets.h`.** It is in `.gitignore`. Only `secrets.example.h`, with placeholders, is in the repository.
- `CLAVE_ADMIN` protects the web (user `admin`) and the OTA updates. The firmware warns in the log if you leave the default `change-me`.
- The web uses plain HTTP with basic authentication. **Use it on your home network only and never port-forward it to the internet.**

## Using it

Open `http://<board-ip>/` in a browser (user `admin`). The page shows a remote with power, four inputs, volume, mute and a switch to turn the TV sync on or off. Holding `+` / `−` repeats the volume step.

The same actions are available as a small API (HTTP basic auth):

| Request | Action |
|---|---|
| `/cmd?c=on` / `off` | Power on (V3) / power off |
| `/cmd?c=off_forzado` | Switch to V3 first, then power off |
| `/cmd?c=in_v3`, `in_dvd`, `in_cd`, `in_tape` | Select input |
| `/cmd?c=vol_up`, `vol_down`, `mute_toggle` | Volume and mute |
| `/auto?v=0` or `1` | TV sync off (manual mode) / on |
| `/status`, `/log` | JSON status, last 40 events |
| `/raw?code=1A0` | Send a raw RI code (only `0x001`–`0x1FF`) |

## How the TV sync works

Every 2.5 s the board asks the TV (`http://<tv>:8001/api/v2/`) for `PowerState`. When it changes it sends the RI command once. Two safeguards avoid loops, for example if the receiver's protection trips: it **only acts when the TV state changes** and it waits **at least 15 s** between two actions. It never resends a command on its own.

## RI codes verified on the TX-SR600E

Codes are 12 bits. The input has to match the code for the "off" and volume commands to be accepted, and the receiver never reports its own state.

| Code | Effect | Notes |
|---|---|---|
| `0x1AF` | Power on + V3 | Documented for the SR600 family; works end to end with the TV |
| `0x1AE` | Power off | **Only accepted while V3 is selected.** From any other input it is ignored, which is useful: the TV turning off will not switch the receiver off while you listen to another source |
| `0x12F` | Power on + DVD | |
| `0x020` / `0x070` / `0x120` / `0x1A0` | Select CD / TAPE / DVD / V3 | Read from the front display |
| `0x1A2` / `0x1A3` | Volume up / down | Only on V3 |
| `0x1A4` / `0x1A5` | Mute / unmute | The "MUTING" indicator lights up |

**Do not work:**

- `0x12E` does not switch the receiver off from DVD.
- **Never send `0x420`–`0x43F`.** They are test and service codes: on the closely related TX-SR605 they open the setup screen or play loud test noise, and on other Onkyo models they can clear settings. The `/raw` endpoint refuses anything above `0x1FF`. On the TX-SR600E, `0x420` alone had no visible or audible effect (tried twice, the second time at a higher volume), but that says nothing about the rest of the range.
- `0x02F` is "power on + CD", not an off command.
- No other code from `0x010` to `0x1F0` (in steps of `0x10`) changed the input. V1, V2, V4, TUNER and PHONO cannot be selected this way.
- Codes known from other Onkyo models did nothing here: `0x002`/`0x003` (volume), `0x004` and `0x0DA`/`0x0EA` (power), `0x005` (mute), `0x006`–`0x00B`, `0x013`, `0x0E3` (inputs) and `0x0D5`/`0x0D6` (next/previous input). The volume was read on the display before and after eight steps of each, and stayed the same. They were also tried with CD, TAPE and DVD selected, together with the matching volume, mute and off codes of each input family (`0x022`–`0x025` and `0x02E` for CD, `0x072`–`0x075` and `0x07E` for TAPE, `0x122`–`0x125` and `0x12E` for DVD): no effect either.

How they were found: starting from the lists in [docbender/Onkyo-RI](https://github.com/docbender/Onkyo-RI), every input-select code was sent while a webcam photographed the receiver's display.

## Known limits

- **No feedback from the receiver.** The remote does not know if it is on, so Power On and Power Off are separate buttons.
- **Other models.** Codes differ between Onkyo models. Use the `/raw` endpoint and a webcam on the display to map yours, and please share the results.
- **Mute** was checked on the display, not by ear.
- We found no RI off command that works from every input on this receiver. Power off is accepted on V3 only, hence the "Switch to V3 first" option.

## Credits

- RI codes and protocol notes: [docbender/Onkyo-RI](https://github.com/docbender/Onkyo-RI).
- The TX-SR605 table in [node-red-contrib-onkyo-ri](https://github.com/dajuly20/node-red-contrib-onkyo-ri) (`resources/RiCodes.md`) matches our findings: only CD, TAPE, DVD and GAME/TV can be selected over RI, and volume, mute and power off only work while GAME/TV (our V3) is selected.

## License

MIT. See [LICENSE](LICENSE).
