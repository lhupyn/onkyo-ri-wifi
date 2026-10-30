# Códigos RI: Onkyo TX-SR600E

*[Read in English](ri-codes.md)*

Todo lo de aquí se probó en un solo Onkyo TX-SR600E con la placa de este repositorio. Sirve de punto de partida para otros modelos, cuyos códigos son distintos.

## El protocolo en pocas palabras

- Cada comando es un **código de 12 bits, empezando por el bit más significativo**, enviado por un solo hilo (más masa) a 3,3 V.
- Trama: cabecera de 3 ms en alto y 1 ms en bajo; después, por cada bit, 1 ms en alto seguido de 1 ms en bajo (bit 0) o 2 ms en bajo (bit 1); y una cola de 1 ms en alto. El firmware envía cada trama dos veces.
- El receptor **nunca informa de su estado**, así que un mando no puede saber si está encendido, en mute o en qué entrada está.
- La mayoría de los códigos solo se aceptan **con la entrada correspondiente seleccionada**. Esa es la regla detrás de que «el apagado solo funcione en V3».

## Verificados en el TX-SR600E

| Código | Efecto | Cómo se comprobó |
|---|---|---|
| `0x020` | Selecciona CD | Leído en la pantalla del receptor |
| `0x070` | Selecciona TAPE | Leído en la pantalla |
| `0x120` | Selecciona DVD | Leído en la pantalla |
| `0x1A0` | Selecciona V3 | Leído en la pantalla («VIDEO3») |
| `0x12F` | Encender + DVD | El receptor se encendió en DVD |
| `0x1AF` | Encender + V3 | El receptor se encendió en V3 al encender la tele |
| `0x1AE` | Apagar | Funciona desde V3, lo ignora desde DVD |
| `0x1A2` / `0x1A3` | Subir / bajar volumen | Funciona en V3 |
| `0x1A4` / `0x1A5` | Mute / quitar mute | Se enciende y se apaga el indicador «MUTING» |

## Probados sin efecto

- Cualquier otro código de selección de entrada de `0x010` a `0x1F0` en pasos de `0x10`. **V1, V2, V4, TUNER y PHONO no se pueden seleccionar por RI.**
- `0x12E` (apagar DVD), `0x02E` (apagar CD), `0x07E` (apagar TAPE): el receptor no se apaga.
- `0x002`, `0x003` (volumen en amplificadores), `0x004`, `0x0DA`, `0x0EA` (encendido), `0x005` (mute), `0x006`–`0x00B`, `0x013`, `0x0E3` (entradas) y `0x0D5` / `0x0D6` (entrada siguiente / anterior): sin efecto visible. El número de volumen de la pantalla no cambió tras ocho pasos de `0x002` y de `0x003`.
- `0x022`–`0x025`, `0x072`–`0x075`, `0x122`–`0x125` (volumen y mute de las familias CD, TAPE y DVD), probados con la entrada correspondiente seleccionada: sin efecto.
- `0x02F` es «encender + CD», no una orden de apagado.

## No envíes nunca `0x420`–`0x43F`

Son códigos de prueba y servicio. En el TX-SR605, un modelo muy cercano, abren la pantalla de configuración o emiten ruidos fuertes de prueba, y en otros Onkyo algunos borran los ajustes del receptor. En el TX-SR600E, `0x420` solo no tuvo ningún efecto visible ni audible (probado dos veces), pero eso no dice nada del resto del rango. El firmware se niega a enviar cualquier código por encima de `0x1FF`.

## Mapear otro receptor

1. Pon una webcam, o simplemente tus ojos, en la pantalla frontal del receptor, y el receptor en una entrada conocida con el volumen bajo.
2. Carga el firmware y envía los candidatos de selección de entrada uno a uno, con tres segundos entre ellos:

   ```bash
   for n in $(seq 16 16 496); do
     curl -s -u admin:<clave> "http://<ip-de-la-placa>/raw?code=$(printf %X $n)"; echo " 0x$(printf %03X $n)"; sleep 3
   done
   ```

3. Apunta qué códigos cambian la entrada. Después, con esa entrada seleccionada, prueba los vecinos: `+2` y `+3` para el volumen, `+4` y `+5` para el mute, `+E` para apagar y `+F` para encender.
4. No pases de `0x1FF`, y no pruebes nada de `0x420`–`0x43F`.

Si mapeas un modelo, abre un issue con tu tabla para añadirla aquí.

## De dónde salen los códigos

- [docbender/Onkyo-RI](https://github.com/docbender/Onkyo-RI) y su wiki: protocolo y listas de códigos por modelo (sobre todo un TX-8020).
- La tabla del TX-SR605 en [node-red-contrib-onkyo-ri](https://github.com/dajuly20/node-red-contrib-onkyo-ri) (`resources/RiCodes.md`): el modelo más parecido, y coincide con la tabla de arriba.
- [victorjacobs/onkyo-mqtt](https://github.com/victorjacobs/onkyo-mqtt): códigos de un amplificador A-9010, que no hicieron nada en el receptor.
