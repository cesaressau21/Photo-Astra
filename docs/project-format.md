# Photo Astra Project — .pastra v4 (lector compatible con v1/v2/v3)

Formato propio inicial del proyecto, independiente de PSD. No pretende ser la
especificación definitiva de futuras capas vectoriales, grupos o máscaras.
Cambios incompatibles requieren otra versión y un lector/migración explícitos.

## Contenedor

| Posición | Contenido |
| --- | --- |
| 0–7 | ASCII `PASTRA04` (8 bytes); `PASTRA01`/`PASTRA02`/`PASTRA03` para versiones anteriores |
| 8–11 | Longitud N del JSON, uint32 big-endian |
| 12–43 | SHA-256 binario de los N bytes de JSON |
| 44–(43+N) | JSON UTF-8, sin terminador |
| Desde 44+N | Rasters concatenados en el orden de la tabla `rasters` |

No hay alineación, compresión, base64, rutas externas ni archivos ejecutables.
Cada raster ocupa exactamente ancho × alto × 4 bytes: RGBA8 premultiplicado sRGB,
filas compactas de arriba abajo. Su SHA-256 se declara en los metadatos. Las sumas
permiten detectar corrupción; no autentican al autor ni constituyen una firma.

## Metadatos

Todos los campos enumerados son obligatorios. Se rechazan campos desconocidos,
tipos distintos, versiones futuras y tipos de capa/efecto no reconocidos.

- Raíz: `version` entero 4 (1/2/3 en versiones anteriores), `title`, `width`, `height`, `encoding` igual a
  `srgb-rgba8-premul`, arrays `rasters` y `layers`.
- Raster: `width`, `height`, `sha256` como 64 caracteres hexadecimales minúsculos.
- Capa: `id` string decimal, `name`, `kind` igual a `raster`, `raster` índice entero
  en la tabla o −1 para capa transparente sin buffer, `opacity` numérica [0,1],
  `visible` booleano, `effects` array, `x` e `y` enteros en [−1.000.000,+1.000.000].
  En v3 también `scaleX` y `scaleY`, números finitos en [1/64,64], relativos al
  tamaño del raster original (1 = 100 %), anclados a la esquina superior izquierda.
  Desde v4 se exige `blendMode`: string `normal`, `multiply` o `screen`.
- Efecto: `id` string decimal, `kind` igual a `exposure`, `enabled` booleano,
  `stops` numérico finito [−8,+8].

Las capas se componen de abajo arriba según su orden; los efectos se procesan
en el orden del array. IDs positivos hasta 2^63−1, representación decimal canónica
sin ceros iniciales. Capas con IDs únicos por documento; efectos únicos por capa.
Se usan strings para evitar la pérdida de precisión de identificadores en JSON.

El escritor deduplica buffers por identidad compartida en memoria. Dos capas
pueden referir el mismo índice y el lector restaura ese recurso compartido.
No se deduplican imágenes distintas que casualmente tengan bytes iguales.

## Límites y validación

- JSON: máximo 1 MiB; título/nombre: no vacío, sin NUL, máximo 4096 bytes UTF-8.
- Máximo 256 capas y 256 rasters; máximo 16 efectos por capa.
- Documento y cada raster: dimensiones positivas, área máxima 33.554.432 píxeles.
- Total de rasters: máximo 512 MiB; archivo máximo 44 + 1 MiB + 512 MiB.
- Longitud del archivo exacta; se rechazan bytes faltantes o sobrantes.
- Metadatos y referencias se validan antes de reservar buffers de píxeles.
- Se verifica checksum por raster y RGB ≤ alpha para cada píxel premultiplicado.

El modelo completo solo se publica si todas las comprobaciones pasan. El archivo
se lee por bloques de hasta 1 MiB, comprobando cancelación entre bloques. No hay
recuperación parcial de archivos dañados.

## Guardado y estado de edición

QSaveFile conserva el destino previo mientras escribe en un temporal del mismo
directorio; no se permite fallback a escritura directa. Cancelar/fallar antes de
commit descarta el temporal. Cancelar después del commit no revierte el archivo.
Guardar correctamente actualiza ruta y marcador limpio, conservando Undo/Redo.
Abrir correctamente inicia un historial nuevo. Zoom/pan, selección UI, historial
Undo/Redo, archivos originales comprimidos y cachés GPU no forman parte del formato.

APIs oficiales consultadas: [QJsonDocument](https://doc.qt.io/qt-6/qjsondocument.html),
[QCryptographicHash](https://doc.qt.io/qt-6/qcryptographichash.html),
[QSaveFile](https://doc.qt.io/qt-6/qsavefile.html). Implementación probada con Qt 6.8.3.

## Migración v1/v2/v3 → v4

El lector exige que el identificador de cabecera y `version` coincidan. En v1 las
capas no tienen x/y y se cargan en (0,0); en v2 ambos campos son obligatorios.
En v1/v2 la escala se inicializa a (1,1); v3 exige ambos campos de escala.
En v1/v2/v3 el modo de fusión se inicializa en Normal. Se rechazan modos
desconocidos, campos ausentes y valores que no sean strings en v4.
El escritor siempre produce v4. La tabla de rasters, codificación y checksums no
cambian. Lectores antiguos rechazarán v4; no se guarda una versión anterior perdiendo
propiedades. Los tests reconstruyen contenedores v1/v2/v3 y verifican su lectura.
Se rechazan posiciones fraccionarias y escalas nulas, negativas o fuera de rango.
