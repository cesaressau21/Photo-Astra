# Validación: posición y arrastre de capas

## Séptimo hito — 17 de septiembre de 2026

- Release y Debug /W4 /WX: CTest **4/4** en cada configuración.
- Core sin Qt/Skia: compilación y CTest **1/1**.
- UI Windows GPU: **21 entradas aprobadas**, incluyendo inicialización/limpieza.
- Core: hit test, coordenadas de imagen, preview inmutable que comparte píxeles,
  retorno al origen sin comando y coordenadas no finitas rechazadas.
- UI: arrastre con zoom 1,2; preview visible antes de confirmar, un solo Undo,
  Redo, cancelación con Escape y pérdida de foco, clic sin movimiento sin entrada.
- Exportación: posición final y transparencia en el área desocupada verificadas.
- Posición numérica: X/Y, Undo/Redo, viewport conservado, límites y recorte.
- Persistencia: posiciones exactas en v2; v1 carga (0,0) y se guarda en v2.
  Posiciones fraccionarias/fuera de rango rechazadas.
- Captura position-controls.png revisada; GPU NVIDIA GeForce RTX 5070/PCIe/SSE2.
- Logs .tools/milestone7-release.log, .tools/milestone7-debug.log,
  build/release/position-gpu-tests.txt, .tools/milestone7-install.log.
- Paquete out/package actualizado y arranque comprobado.

Se implementó traslación en píxeles enteros; no escala/rotación ni selección por
alpha. Linux/macOS siguen pendientes de prueba en sus propios entornos.


## Sexto hito — 17 de septiembre de 2026

- Release y Debug /W4 /WX: CTest **4/4** en cada configuración.
- Suite project.file: round-trip de metadatos/píxeles, recursos compartidos,
  capas transparentes/ocultas, efectos y composición idéntica tras reabrir.
- 17 casos de archivo malformado: cabecera, truncado, bytes sobrantes, hashes,
  longitud de metadatos, versión, IDs, referencias, tamaños, tipos, alpha,
  opacidad, campos desconocidos y parámetros de efectos.
- Cancelar/fallar conserva el archivo previo y el documento. Guardar mantiene
  Undo/Redo y marca limpio; abrir reinicia historial. IDs nuevos no colisionan.
- UI Windows GPU: **19 entradas aprobadas** incluyendo setup/cleanup. Guardar
  añade extensión, reutiliza ruta, recupera capas/efectos y maneja cerrar con
  Guardar, Descartar, cancelar diálogo o fallo de escritura.
- Driver NVIDIA GeForce RTX 5070/PCIe/SSE2; todas las pruebas previas siguen pasando.
- Logs: .tools/milestone6-release.log, .tools/milestone6-debug.log,
  build/release/project-gpu-tests.txt, .tools/milestone6-install.log.
- Paquete out/package actualizado; arranque comprobado con runtime Qt local.

Los diálogos de archivos se automatizan con widgets Qt, no con el selector nativo
del sistema operativo. No se simularon fallos físicos del disco, corte de energía
ni una escritura concurrente externa. Linux/macOS siguen sin validación local.


## Quinto hito — 17 de septiembre de 2026

- Release y Debug /W4 /WX: CTest **3/3** en cada configuración.
- Core sin Qt/Skia: compilación y CTest **1/1**.
- UI Windows con GPU requerida: **14 entradas aprobadas**, incluyendo setup/cleanup.
- GPU observada: NVIDIA GeForce RTX 5070/PCIe/SSE2.
- +1 EV: gris sRGB 128 pasa a aproximadamente 175, alpha no cambia. Se verifican
  píxeles semitransparentes, alpha cero y opacidad de capa combinada con el efecto.
- Orden: +4 y −4 EV producen distinto resultado al invertir las etapas con clipping.
- Desactivar/eliminar restaura el original; los píxeles fuente se conservan.
- NaN, stops fuera de rango, IDs inválidos, tipo desconocido y más de 16 efectos
  se rechazan sin cambiar historial. Undo/Redo de efectos comprobados.
- UI: añadir, ajustar, ocultar, mover, borrar y deshacer. Canvas GPU y PNG exportado
  coinciden con tolerancia de dos niveles de color de 8 bits.
- Captura build/release/effects-gpu.png revisada visualmente.
- Logs: .tools/milestone5-release.log, .tools/milestone5-debug.log,
  build/release/effects-gpu-tests.txt. Paquete local actualizado.

No se midió rendimiento sostenido con documentos grandes. No se implementaron
shaders externos, bloom, efectos globales ni un Shader Editor. Linux/macOS
siguen sin validación en máquinas de esas plataformas.


## Cuarto hito — 17 de septiembre de 2026

- Release y Debug: C++20 /W4 /WX, CTest **3/3** en cada configuración.
- UI Windows/OpenGL: **13 entradas aprobadas**, incluidas inicialización/limpieza,
  cinco casos funcionales y seis escenarios del diálogo de exportación.
- Driver: NVIDIA GeForce RTX 5070/PCIe/SSE2.
- PNG: dimensiones nativas, perfil sRGB y alpha parcial/cero comprobados al releer.
- JPEG: composición sobre blanco y colores con tolerancia por compresión.
- Fallo de composición, ruta inválida, tamaño excesivo y cancelación conservan
  el destino existente; exportar conserva el documento y su historial.
- UI: Nuevo valida dimensiones; Exportar cancela, rechaza extensiones incompatibles,
  confirma o rechaza sobrescritura y añade .png/.jpg según el filtro elegido.
- Los tests usan los diálogos Qt de archivos para automatización, incluso en
  Windows; los diálogos del sistema operativo no se automatizaron.
- Paquete out/package actualizado y ejecutado sin añadir el SDK Qt a PATH.
- Logs: .tools/milestone4-release.log, .tools/milestone4-debug.log,
  .tools/milestone4-install.log, build/release/export-gpu-tests.txt.

La cancelación es cooperativa: si commit ya terminó, se informa éxito. El hito
no incluye formato editable, shaders ni pruebas en Linux/macOS. Los resultados
de los hitos anteriores quedan registrados a continuación.


## Tercer hito — 16 de septiembre de 2026

- Release y Debug: compilación C++20 con /W4 /WX; CTest **3/3** en ambas.
- Core sin Qt ni Skia: compilación y CTest **1/1**.
- UI nativa OpenGL y CPU: **4 casos funcionales** aprobados en cada backend
  (Qt Test informa 6 incluyendo inicialización y limpieza).
- GPU comprobada: NVIDIA GeForce RTX 5070/PCIe/SSE2. La prueba compara píxeles
  de dos capas, rojo y azul al 50 %, además de orientación, zoom y pan.
- Core: identidad y orden de capas, opacidad no finita/IDs repetidos rechazados
  atómicamente, no-ops, snapshots sin copiar píxeles, Undo/Redo, ramas, capacidad
  del historial y marcador limpio.
- Pipeline: seis casos funcionales; se añadieron composición por orden,
  visibilidad, opacidad cero/parcial, recorte y conservación de píxeles; también
  importación de capa reversible y bloqueo de ediciones mientras carga.
- UI: importar segunda capa, opacidad, visibilidad, reordenar, eliminar,
  Undo/Redo y mantener zoom/pan; cierre con cambios puede cancelarse.
- Captura `build/release/layers-gpu.png` revisada: capas, controles y composición
  visibles. Logs nativos `layers-gpu-tests.txt` y `layers-cpu-tests.txt`.
- Paquete local `out/package` actualizado con el runtime Qt.

No se probaron Linux/macOS en este hito. Tampoco hay exportación, grupos,
efectos ni límite de RAM global; el historial limita entradas, no bytes.
La validación DPI al 200 % de abajo pertenece al hito anterior.

## Registro anterior: Skia e importación / Canvas

Sistema comprobado: Windows 10 x64, MSVC 19.44, Qt 6.8.3 y Skia m144
`ed427fd003ba3bc6eb4a8ae0337f9cdafc39e5fb`. Corresponde al segundo hito de
V0.1; no declara terminado el MVP completo.

## Resultados ejecutados

| Comprobación | Resultado |
| --- | --- |
| Skia Release y Debug desde fuentes oficiales | Compiladas con el script del repositorio |
| Repetición del bootstrap Release | Incremental: `ninja: no work to do` |
| Aplicación Release, C++20 y MSVC /W4 /WX | Compila; CTest 3/3 aprobados |
| Aplicación Debug y Skia Debug /MDd | Compila; CTest 3/3 aprobados |
| Core sin Qt ni Skia | Compila; CTest 1/1 aprobado |
| UI nativa con OpenGL requerido | Tres casos funcionales aprobados |
| Driver OpenGL observado | NVIDIA GeForce RTX 5070/PCIe/SSE2 |
| UI nativa con escala Qt al 200 % | Tres casos funcionales aprobados |
| UI nativa forzando CPU | Tres casos funcionales aprobados |
| Recuperación a CPU tras rechazo del backend GL | Comprobada en la prueba nativa |
| Capturas del Canvas CPU/OpenGL | Revisadas visualmente; imagen y transparencia visibles |
| Dependencias de cabeceras de Skia/Ninja | Registradas y verificadas con `ninja -t deps` |
| Despliegue local con runtime Qt | Actualizado en `out/package`; incluye aviso Skia |
| Ejecutable desplegado sin añadir el SDK Qt a PATH | Arranca y carga `sample.png`; ventana respondiendo |

CTest tiene tres suites: `core.document`, `ui.main_window` e
`image.pipeline`. La suite de UI contiene tres casos funcionales; Qt Test
informa cinco entradas aprobadas al incluir inicialización y limpieza.
La suite de imágenes contiene cuatro casos funcionales.

## Qué comprueban las pruebas

- Invariantes de Document, validación del buffer RasterImage y snapshots que
  comparten los píxeles, sin copiarlos.
- Conversión entre coordenadas de imagen/vista, límites de zoom, valores no
  finitos y ajuste centrado al tamaño del Canvas.
- Importación PNG/JPEG con rutas Unicode, alpha premultiplicado y JPEG con
  EXIF orientación 6 (rotación 90 grados).
- Conversión real de un PNG etiquetado como sRGB lineal a sRGB.
- Rechazo de archivos inexistentes, corruptos, de otro formato aunque tengan
  extensión PNG y con dimensiones que exceden el límite antes de decodificar.
- Una importación activa, cancelación y conservación del documento anterior
  ante fallo/cancelación.
- Colores concretos producidos por Skia CPU, transparencia sobre tablero,
  validación de targets, limpieza tras pan, cambio de imagen, vida del buffer
  compartido y render con DPR 2.
- Ventana, paneles, apertura, acciones pendientes deshabilitadas, carga
  asíncrona y cierre.
- Lectura de píxeles del framebuffer OpenGL: rojo arriba y azul abajo; detecta
  un framebuffer vacío, colores incorrectos o inversión vertical.
- Eventos de rueda que conservan el punto bajo el cursor y arrastre con botón
  central o Espacio + botón izquierdo.
- Fallo deliberado de inicialización del renderer GL y cambio a superficie CPU.

La prueba de escala 200 % usa `QT_SCALE_FACTOR=2`. No equivale a validar
movimiento entre varios monitores con distintas escalas o perfiles de color.

## Incidencias resueltas

- El alias de Microsoft Store `python3` no apuntaba a Python utilizable. El
  bootstrap entrega a GN la ruta del Python instalado.
- Skia Debug intentaba incluir utilidades Android que requieren Expat.
  Se desactivaron esas utilidades ajenas al editor; se mantuvieron los backends
  CPU/OpenGL y SkSL.
- MSVC en español emite un prefijo localizado para includes. Se añadió un
  wrapper Ninja que detecta/conserva el prefijo sin modificar las fuentes Skia.
- Se actualizó la prueba de la ventana al introducir DocumentSession y
  transferencia explícita de propiedad del Renderer.
- Se desconecta la señal de destrucción del contexto antes de ejecutar el
  destructor base de QOpenGLWidget, siguiendo el ciclo de vida documentado.
- Una ejecución quedó temporalmente rechazada por el límite de uso de la
  revisión automática. La compilación se reanudó tras la instrucción del
  usuario y se completaron las comprobaciones.

## Límites y pendientes

No se han ejecutado builds Linux/macOS ni pruebas en otra GPU o un Windows
limpio. No hay benchmarks ni validación de pérdida de dispositivo durante
ejecución, agotamiento real de memoria o alternancia entre monitores.

La configuración informa que no encuentra headers Vulkan: este hito usa
OpenGL y no necesita Vulkan. El despliegue local puede informar traducciones
Qt ausentes porque se instaló qtbase sin qttranslations.

La lectura de codecs y normalización sRGB están comprobadas; perfiles de
monitor, OpenColorIO, preservación de alta profundidad de bits, tiles,
mipmaps y límites estrictos de RAM/VRAM no están implementados.

Tampoco están implementados ni probados: capas editables, composición multicapa,
Undo/Redo, exportación, Effect Stack o shaders de usuario.

## Hito 8 — escala no destructiva (17 de septiembre de 2026)

- Release y Debug: compilación con warnings estrictos y 4/4 suites aprobadas.
- Core independiente de Qt/Skia: 1/1 suite aprobada; escala válida, no-op,
  rechazo atómico de valores inválidos, Undo/Redo, píxeles compartidos y hit test.
- UI Windows/OpenGL en NVIDIA GeForce RTX 5070: 22 casos aprobados, incluidos
  inicio/cierre del conjunto; vista previa verificada leyendo framebuffer,
  aceptar/cancelar, ejes independientes y reset sin generar historial innecesario.
- Exportación PNG comprueba extensión de capa escalada, alpha y restauración de
  matriz antes de dibujar otra capa. Proyecto compara metadatos y composición
  tras guardar/abrir; lectura v1/v2 y rechazo de escalas inválidas.
- Capturas revisadas: `build/release/scale-preview.png` y `.png.dialog.png`.
- Paquete Windows actualizado; no se verificaron Linux/macOS en este equipo.
- Logs: `.tools/milestone8-release.log`, `milestone8-debug.log`,
  `milestone8-install.log`; GPU `build/release/scale-gpu-tests.txt`.

## Hito 9 — modos de fusión (17 de septiembre de 2026)

- Release y Debug: compilación sin errores y 4/4 suites aprobadas.
- Core sin Qt/Skia: 1/1 suite; comandos, no-op, validación atómica, Undo/Redo
  y conservación de buffers originales.
- Composición CPU: Normal/Multiplicar/Trama con alpha fuente/destino 0/128/255
  y opacidad 0/0,5/1. Se contrastan canales premultiplicados con fórmulas esperadas.
- Canvas CPU: una capa sobre transparencia conserva aspecto en todos los modos.
- UI Windows/OpenGL NVIDIA GeForce RTX 5070: 23 casos aprobados (incluyendo
  setup/cleanup). Selector, Undo/Redo, ocultar capa inferior y comparación de
  framebuffer con exportación PNG, tolerancia de hasta 2 niveles RGBA8.
- .pastra v4: round-trip de los tres modos; compatibilidad v1/v2/v3 en Normal,
  rechazo de modos desconocidos, nulos o con tipo incorrecto.
- Captura revisada: build/release/blend-controls.png. Reporte GPU:
  build/release/blend-gpu-tests.txt. Logs .tools/milestone9-{release,debug,install}.log.
- Linux/macOS y gestión avanzada de color siguen pendientes de validación.

## Hito 10 — presets y matriz CI del núcleo (17 de septiembre de 2026)

- Nuevas carpetas build/core-debug y build/core-release: configuradas y compiladas
  mediante scripts/build-core.ps1 y CMakePresets.json, con warnings como errores.
- Windows MSVC: 1/1 prueba aprobada en cada configuración.
- .github/workflows/core.yml validado con actionlint 1.7.12: sin errores;
  no se ejecutaron analizadores opcionales shellcheck/pyflakes.
- Checkout SHA verificado contra el tag oficial v5.0.0; validador descargado de
  su publicación oficial y archivo contrastado con el digest SHA-256 de GitHub.
- No hay remoto Git ni Linux/WSL utilizable; no se ejecutó CI remoto ni macOS.
  Los seis trabajos configurados no se cuentan como seis builds aprobados.
- Editor y motor no cambiaron; no se repitieron sus suites ajenas a este hito.
- Logs .tools/milestone10-core-{debug,release}.log; detalles en [CI](ci.md).

## Hito 11 — primera ejecución remota (17 de septiembre de 2026)

Repositorio conectado: cesaressau21/Photo-Astra; commit inicial `d5cea0d` en main.
[GitHub Actions 35302057942](https://github.com/cesaressau21/Photo-Astra/actions/runs/35302057942)
terminó en success, con seis trabajos completados en success:

- Windows 2022: Debug y Release.
- Ubuntu 24.04: Debug y Release.
- macOS 15: Debug y Release.

Cada trabajo configura y compila el núcleo con warnings como errores y ejecuta
core.document. No hubo errores de portabilidad que corregir. Esta comprobación
no cubre Qt/Skia, UI, GPU ni paquetes Linux/macOS. La licencia del proyecto queda
pendiente; se publicó código fuente, sin binarios ni dependencias descargadas.

## Evidencias locales ignoradas por Git

- `.tools/skia-bootstrap-release.log`, `.tools/skia-bootstrap-debug.log`.
- `.tools/milestone2-release.log`, `.tools/milestone2-debug.log`,
  `.tools/milestone2-core.log`.
- `.tools/milestone2-deploy.log`.
- `build/release/gpu-tests.txt`, `gpu-hidpi-tests.txt`, `cpu-tests.txt`.
- `build/release/canvas-gpu.png`, `canvas-cpu.png`, `sample.png`.
- `build/{debug,release,core}/Testing/Temporary/LastTest.log`.

La imagen de prueba es un PNG original generado por los tests con cuadrantes
rojo, verde, azul y transparente; no es una captura de otro editor.
