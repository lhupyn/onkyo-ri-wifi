# Changelog

## 1.2.1
- Technical details panel: the TV state row is translated, and "Last automatic action" is now "Last TV change handled" (it is also updated in manual mode).
- README: the board's blue LED mirrors the TV state.

## 1.2.0
- Firmware, comments and log messages translated to English; web remote in English and Spanish (follows the browser, or `/?lang=es|en`).
- Settings moved to `config.h`; new `WEB_AUTH` option to drop the web password (OTA keeps its password) and `RECEIVER_NAME`.
- RI codes grouped in one block at the top of the sketch; API names are now `in_tv` and `off_any_input`.
- Web page moved to `webpage.h`.
- Docs: wiring diagram, screenshots, USB and driver notes, full code table in `docs/ri-codes.md`.

## 1.1.0
- New web remote: status chips with Wi-Fi bars, technical details panel, event log, restart button.
- Event log of 60 lines; volume steps are no longer logged.
- Mute button and the "Follow the TV" switch.

## 1.0.0
- First version: power on and off the receiver from the Samsung TV state, web remote, OTA updates.
