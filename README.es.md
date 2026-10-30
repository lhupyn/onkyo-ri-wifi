# onkyo-ri-wifi

*[Read in English](README.md)*

Lleva un receptor Onkyo antiguo a la era del WiFi con una placa ESP8266 de pocos euros.

- **Sigue a tu tele**: al encender una Samsung (Tizen), el receptor se enciende y selecciona la entrada de la tele; al apagarla, pasa a standby.
- **Mando web para el móvil**: encendido, entrada, volumen y mute desde cualquier navegador de tu red.
- **Actualizaciones por WiFi (OTA)**: una vez cargado, no vuelves a necesitar el cable USB.

Habla **Onkyo RI** (Remote Interactive), el conector de 3,5 mm «REMOTE CONTROL» de la parte trasera de muchos Onkyo, así que no hay que apuntar ningún LED infrarrojo.

Probado con un **Onkyo TX-SR600E**, una **Samsung QE75Q70R**, un NodeMCU V3 (CH340), `arduino-cli` 1.5.1 y el núcleo ESP8266 3.1.2. Otros modelos Onkyo usan otros códigos (ver [Límites conocidos](#límites-conocidos)).

> Proyecto no oficial, sin relación con Onkyo ni Samsung. Conectas el hardware a tu receptor bajo tu responsabilidad.

## Qué necesitas

- Un NodeMCU V3 (sirve cualquier placa ESP8266 con un GPIO libre)
- Un cable con jack de 3,5 mm **mono** (o uno que puedas cortar y recablear)
- Un receptor Onkyo con conector **RI REMOTE CONTROL**
- Una tele Samsung que responda a la API Tizen en el puerto 8001 (probado en un QLED de 2019; otros modelos sin probar)
- Un cargador USB para alimentar la placa

## Cableado

El conector RI es un jack mono de 3,5 mm: **punta = datos, cuerpo = masa**.

```
NodeMCU                     Receptor "RI REMOTE CONTROL"
D2 (GPIO4)  ───────────────  punta   (señal)
GND         ───────────────  cuerpo  (masa)
```

Los colores del cable varían: comprueba con un polímetro qué hilo va a la punta y cuál al cuerpo. En el cable del autor, el hilo rojo es el que va a GND.

El ESP8266 saca 3,3 V y es suficiente para el TX-SR600E.

## Configuración del receptor

La tele tiene que llegar al receptor por una entrada que RI pueda controlar bien. En el TX-SR600E es **V3** (ver la tabla de códigos). En el menú de configuración del receptor, asigna la **entrada óptica de la tele (OPT 1) a V3**. Mira el manual de tu receptor para el menú exacto.

## Instalación

```bash
# 1. Herramientas
arduino-cli config add board_manager.additional_urls https://arduino.esp8266.com/stable/package_esp8266com_index.json
arduino-cli core update-index
arduino-cli core install esp8266:esp8266

# 2. Ajustes: copia el ejemplo y rellénalo
cd firmware/onkyo-ri-wifi
cp secrets.example.h secrets.h        # nombre y clave del WiFi, IP de la tele, clave de admin

# 3. Primera carga, por USB
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 .
arduino-cli upload  --fqbn esp8266:esp8266:nodemcuv2 -p /dev/cu.usbserial-XXXX .
```

En macOS antiguos, el chip CH340 necesita el driver de WCH. Dale a la tele una dirección fija: **reserva su IP en el DHCP de tu router** para que no cambie nunca.

### Actualizar por WiFi

```bash
arduino-cli upload -p <ip-de-la-placa> --protocol network \
  --fqbn esp8266:esp8266:nodemcuv2 \
  --upload-field password=<tu clave de admin> .
```

Mantén OTA en todas las versiones que cargues. Si cargas una sin él, tendrás que volver al cable USB.

En macOS con el cortafuegos activado, la placa debe poder conectarse de vuelta a tu ordenador. Si la subida termina con `No response from device`, permite las conexiones entrantes del `python3` incluido en `~/Library/Arduino15/packages/esp8266/tools/python3/`.

## Claves

- **Nunca subas `secrets.h` a git.** Está en `.gitignore`. En el repositorio solo está `secrets.example.h`, con valores de ejemplo.
- `CLAVE_ADMIN` protege la web (usuario `admin`) y las actualizaciones OTA. Si dejas la clave por defecto `change-me`, el firmware lo avisa en el registro.
- La web usa HTTP normal con autenticación básica. **Úsala solo en tu red doméstica y nunca la abras a internet.**

## Uso

Abre `http://<ip-de-la-placa>/` en el navegador (usuario `admin`). La página es un mando con encendido, cuatro entradas, volumen, mute y un interruptor para activar o desactivar la sincronización con la tele. Mantener pulsado `+` o `−` repite el paso de volumen.

Las mismas acciones están disponibles como una pequeña API (autenticación básica HTTP):

| Petición | Acción |
|---|---|
| `/cmd?c=on` / `off` | Encender (V3) / apagar |
| `/cmd?c=off_forzado` | Cambia primero a V3 y luego apaga |
| `/cmd?c=in_v3`, `in_dvd`, `in_cd`, `in_tape` | Seleccionar entrada |
| `/cmd?c=vol_up`, `vol_down`, `mute_toggle` | Volumen y mute |
| `/auto?v=0` o `1` | Sincronización con la tele desactivada (modo manual) / activada |
| `/status`, `/log` | Estado en JSON, últimos 40 eventos |
| `/raw?code=1A0` | Envía un código RI en bruto (solo `0x001`–`0x1FF`) |

## Cómo funciona la sincronización con la tele

Cada 2,5 s la placa pregunta a la tele (`http://<tele>:8001/api/v2/`) por `PowerState`. Cuando cambia, envía el comando RI una sola vez. Dos medidas evitan bucles, por ejemplo si salta la protección del receptor: **solo actúa cuando cambia el estado de la tele** y espera **al menos 15 s** entre dos acciones. Nunca reenvía un comando por su cuenta.

## Códigos RI verificados en el TX-SR600E

Los códigos son de 12 bits. La entrada debe coincidir con el código para que el receptor acepte el apagado y el volumen, y el receptor nunca informa de su estado.

| Código | Efecto | Notas |
|---|---|---|
| `0x1AF` | Encender + V3 | Documentado para la familia SR600; funciona de punta a punta con la tele |
| `0x1AE` | Apagar | **Solo lo acepta con V3 seleccionada.** Desde cualquier otra entrada lo ignora, lo cual es útil: al apagar la tele no se apaga el receptor si estás escuchando otra fuente |
| `0x12F` | Encender + DVD | |
| `0x020` / `0x070` / `0x120` / `0x1A0` | Seleccionar CD / TAPE / DVD / V3 | Leído en la pantalla del receptor |
| `0x1A2` / `0x1A3` | Subir / bajar volumen | Solo en V3 |
| `0x1A4` / `0x1A5` | Mute / quitar mute | Se enciende el indicador «MUTING» |

**No funcionan:**

- `0x12E` no apaga el receptor desde DVD.
- **No envíes nunca `0x420`–`0x43F`.** Son códigos de prueba y servicio: en el TX-SR605, un modelo muy cercano, abren la pantalla de configuración o emiten ruidos fuertes de prueba, y en otros Onkyo pueden borrar ajustes. El endpoint `/raw` rechaza cualquier código por encima de `0x1FF`.
- `0x02F` es «encender + CD», no una orden de apagado.
- Ningún otro código de `0x010` a `0x1F0` (de `0x10` en `0x10`) cambió la entrada. V1, V2, V4, TUNER y PHONO no se pueden seleccionar así.
- Los códigos de otros modelos Onkyo no hicieron nada aquí: `0x002`/`0x003` (volumen), `0x004` y `0x0DA`/`0x0EA` (encendido), `0x005` (mute), `0x006`–`0x00B`, `0x013`, `0x0E3` (entradas) y `0x0D5`/`0x0D6` (entrada siguiente/anterior). El volumen se leyó en la pantalla antes y después de ocho pasos de cada uno y no cambió. También se probaron con CD, TAPE y DVD seleccionadas, junto con los códigos de volumen, mute y apagado de cada familia de entrada (`0x022`–`0x025` y `0x02E` para CD, `0x072`–`0x075` y `0x07E` para TAPE, `0x122`–`0x125` y `0x12E` para DVD): tampoco hicieron nada.

Cómo se encontraron: partiendo de las listas de [docbender/Onkyo-RI](https://github.com/docbender/Onkyo-RI), se enviaron todos los códigos de selección de entrada mientras una webcam fotografiaba la pantalla del receptor.

## Límites conocidos

- **El receptor no informa de su estado.** El mando no sabe si está encendido, por eso encender y apagar son botones separados.
- **Otros modelos.** Los códigos cambian entre modelos Onkyo. Usa `/raw` y una webcam apuntando a la pantalla para mapear el tuyo, y comparte los resultados.
- **El mute** se comprobó en la pantalla, no a oído.
- No hemos encontrado ninguna orden RI de apagado que funcione desde cualquier entrada en este receptor. El apagado solo se acepta en V3, de ahí la opción «Cambiar primero a V3».

## Créditos

- Códigos RI y notas del protocolo: [docbender/Onkyo-RI](https://github.com/docbender/Onkyo-RI).
- La tabla del TX-SR605 en [node-red-contrib-onkyo-ri](https://github.com/dajuly20/node-red-contrib-onkyo-ri) (`resources/RiCodes.md`) coincide con nuestros resultados: por RI solo se pueden seleccionar CD, TAPE, DVD y GAME/TV, y el volumen, el mute y el apagado solo funcionan con GAME/TV (nuestra V3) seleccionada.

## Licencia

MIT. Ver [LICENSE](LICENSE).
