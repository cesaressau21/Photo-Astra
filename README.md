# Photo Astra

Editor de imágenes de escritorio en C++20. **V0.1 en desarrollo: hito de
modos de fusión básicos completado y validado en Windows.** La aplicación funciona localmente,
sin servicios cloud.

## Qué funciona

- Abrir documentos .pastra e imágenes PNG/JPEG desde Archivo → Abrir o pasando
  la ruta al ejecutable.
- Decodificación en un hilo de trabajo, orientación EXIF y normalización a
  sRGB RGBA8 con alpha premultiplicado.
- Renderizado real con Skia m144: OpenGL cuando está disponible y CPU como
  alternativa. El panel Propiedades muestra el backend activo.
- Transparencia sobre tablero, zoom con rueda anclado al cursor y desplazamiento
  con botón central o Espacio + arrastrar con el botón izquierdo.
- Ajustar al lienzo (Ctrl+0), tamaño 100 % (Ctrl+1), resolución y zoom en estado.
  El 100 % corresponde a un píxel de imagen por unidad lógica de Qt.
- Paneles acoplables, pestañas de propiedades/efectos y manejo de errores.
- Cancelar importación con Escape: el resultado se descarta y se conserva el
  documento actual. La llamada de decodificación Qt no se interrumpe a mitad.
- Al fallar una importación, la imagen anterior permanece intacta.

- Capas raster: añadir una capa transparente sin reservar píxeles, importar otra
  PNG/JPEG, eliminar, mostrar/ocultar, cambiar opacidad y subir/bajar.
- Composición de abajo hacia arriba, con recorte al documento. Cada
  imagen importada conserva su tamaño y comienza en la esquina superior izquierda.
- Deshacer/Rehacer (Ctrl+Z/Ctrl+Y), hasta 100 operaciones. Los cambios de capas
  conservan el zoom/pan y comparten píxeles inmutables.
- Indicador de cambios pendientes y confirmación antes de cerrar/reemplazar.

- Nuevo documento: nombre, ancho y alto configurables, fondo transparente.
- Exportación asíncrona de la composición: PNG con transparencia y JPEG con fondo
  blanco y calidad 95. Mantiene tamaño nativo y etiqueta sRGB; no incluye el tablero.
- La escritura usa un archivo temporal y confirma el reemplazo solo tras terminar.
  Exportar no guarda capas ni limpia el indicador de cambios editables pendientes.

- Efectos por capa: añadir exposición, activar/desactivar, ajustar entre −8 y +8 EV,
  reordenar y eliminar; todas las operaciones participan en Undo/Redo.
- Exposición SkSL no destructiva en Skia GPU/CPU y exportación. Conserva alpha;
  calcula luz lineal y recorta cada etapa al rango de salida SDR.

- Guardar (Ctrl+S) y Guardar como (Ctrl+Mayús+S) conservan capas, píxeles y efectos
  en el formato propio .pastra. Guardar correctamente limpia el asterisco; Undo
  vuelve a marcar cambios y Redo hasta el punto guardado los limpia de nuevo.
- Al cerrar, abrir o crear con cambios: Guardar / Descartar / Cancelar. Si guardar
  falla o se cancela, se conserva el documento. Exportar usa Ctrl+Alt+E.

- Posición X/Y por capa y arrastre con botón izquierdo sobre la capa seleccionada.
  El movimiento se redondea a píxeles enteros y se confirma al soltar como un solo
  paso de Undo. Escape cancela; Espacio o botón central siguen desplazando la vista.
- Escalar… en Capas abre porcentajes horizontal/vertical con vista previa, ejes
  vinculados opcionales y restablecer al 100 %. Aceptar genera un solo Undo;
  Cancelar o Escape descarta la vista previa. Anclaje superior izquierdo.
- .pastra v4 conserva posición, escala y modo de fusión. Se siguen leyendo proyectos v1/v2/v3;
  al guardarlos se escriben como v4. Exportar incluye la escala sin cambiar los
  píxeles originales ni el tamaño del documento.

- Selector Fusión: Normal, Multiplicar y Trama; cambios con Undo/Redo, guardado
  y exportación. La fusión respeta opacidad y transparencia y se calcula entre
  capas, sin mezclar el tablero de fondo con el documento.

**Pendientes:** rotación, otros anclajes, grupos, máscaras, más modos de fusión, efectos espaciales,
efectos del documento completo y Shader Editor. Al iniciar
se crea un documento transparente de 1920 × 1080; Abrir sustituye el documento,
Importar en Capas añade contenido. Las acciones pendientes están deshabilitadas.

## Arquitectura y archivos

```text
CMakeLists.txt
CMakePresets.json  Builds reproducibles del núcleo, Debug/Release
.github/workflows/ Matriz CI del núcleo Windows/Linux/macOS (6/6 aprobados)
cmake/       Opciones, integración y configuración fijada de Skia
src/
  app/       main.cpp: ensamblado de dependencias y argumentos
  core/      Document, capas, efectos, History, LayerMoveGesture, RasterImage y Viewport
  io/        ImageLoader / ImageWriter / ProjectFile: imágenes y documentos
  application/ DocumentSession / ImageExport: comandos y tareas asíncronas
  render/    Renderer, SkiaRenderer, SkiaCompositor y EffectPipeline privados
  ui/        MainWindow, FileActions, LayersPanel, EffectsPanel y CanvasWidget; adaptadores CPU/OpenGL de Qt
tests/       Modelo, importación, renderizado y eventos de ventana
scripts/     Preparación de Skia y compilación Windows del editor/núcleo
docs/        Arquitectura, dependencias, licencia y resultados
third_party/licenses/ Avisos conservados de dependencias
```

[Arquitectura](docs/architecture.md) · [Skia y reproducción del build](docs/skia.md)
· [Resultados comprobados](docs/validation.md) · [Estado de licencia](docs/licensing.md)
· [CI y compilación multiplataforma](docs/ci.md)

## Dependencias

- C++20, CMake >= 3.24 y Ninja u otro generador CMake.
- Qt >= 6.5: Widgets, OpenGLWidgets, Concurrent; Test solo para pruebas.
- Skia m144, commit `ed427fd003ba3bc6eb4a8ae0337f9cdafc39e5fb`.
- Python y Git para preparar Skia; GN se descarga desde la revisión fijada por
  su propio script oficial.
- Windows comprobado con Qt **6.8.3 MSVC x64**, MSVC **19.44**, CMake **3.31.6**
  y Ninja de Visual Studio Build Tools 2022.
- OpenColorIO no está integrado.

Qt se enlaza dinámicamente y Skia estáticamente. Las dependencias y builds
locales se excluyen de Git mediante `.deps/`, `.tools/`, `build/` y `out/`.

## Compilar y ejecutar en este equipo

En PowerShell, desde `C:\Users\Shen\Desktop\photo-astra`:

```powershell
.\scripts\build-windows.ps1 -Configuration Release -Run
```

El script configura, compila y ejecuta CTest antes de abrir la aplicación.
Para Debug:

```powershell
.\scripts\build-windows.ps1 -Configuration Debug
```

Qt y Skia ya están preparados en este equipo. En un checkout nuevo, instala
Qt para MSVC x64 y prepara las dos configuraciones de Skia:

```powershell
.\scripts\bootstrap-skia.ps1 -Configuration Release
.\scripts\bootstrap-skia.ps1 -Configuration Debug
.\scripts\build-windows.ps1 -QtRoot C:\Qt\6.8.3\msvc2022_64 -Configuration Release -Run
```

Qt local por defecto: `.deps/Qt/6.8.3/msvc2022_64`. Otra forma de preparar Qt,
usando aqtinstall como herramienta de desarrollo:

```powershell
python -m venv .tools/aqt
.\.tools\aqt\Scripts\python.exe -m pip install aqtinstall==3.3.0
.\.tools\aqt\Scripts\python.exe -m aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 --archives qtbase --outputdir .deps/Qt
```

El script de compilación no modifica el PATH global. Ajusta el entorno de
Visual Studio y la codificación de consola en el proceso actual. `-Fresh`
regenera la configuración de CMake.

## Linux/macOS y núcleo independiente

El núcleo ya compila y pasa sus pruebas en Linux/macOS mediante CI. **La aplicación
completa con Qt/Skia todavía no está validada allí.** Prepara Skia según
[docs/skia.md](docs/skia.md), Qt y un compilador:

```sh
cmake -S . -B build/dev -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/ruta/al/kit/Qt -DPHOTO_ASTRA_WARNINGS_AS_ERRORS=ON
cmake --build build/dev --parallel
ctest --test-dir build/dev --output-on-failure
```

Para compilar las pruebas del núcleo sin instalar Qt **ni Skia**, desde un
entorno con el compilador disponible:

```sh
cmake --preset core-debug
cmake --build --preset core-debug --parallel 2
ctest --preset core-debug
```

Requiere Ninja. En Windows, `scripts/build-core.ps1 -Configuration Debug` prepara
MSVC y ejecuta estos pasos. Usa `core-release` o `-Configuration Release` para
Release. La primera matriz CI pasó los seis trabajos de Windows/Linux/macOS.
Ver [ejecución comprobada](https://github.com/cesaressau21/Photo-Astra/actions/runs/35302057942)
y [alcance del CI](docs/ci.md).

## Ejecutable y despliegue local

El binario de desarrollo es `build/release/photo_astra.exe` y necesita Qt en
PATH; `build-windows.ps1 -Run` prepara ese entorno. Para una carpeta ejecutable
con el runtime Qt, desde Developer PowerShell con CMake disponible:

```powershell
cmake --install build/release --prefix "$PWD/out/package"
.\out\package\bin\photo_astra.exe
.\out\package\bin\photo_astra.exe "C:\ruta\imagen.png"
.\out\package\bin\photo_astra.exe --cpu "C:\ruta\imagen.jpg"
```

`--prefix` debe ser absoluto. macOS usa un bundle `.app`. Esto todavía es un
despliegue de desarrollo, no un instalador público probado en máquinas limpias.

## Límites actuales

La importación admite archivos hasta 128 MiB e imágenes hasta 33.554.432
píxeles. La conversión puede usar buffers temporales; el pico de RAM puede
superar el tamaño de la imagen final. Se realiza una copia al almacenamiento
independiente de Qt; pan/zoom y snapshots no copian el raster completo.

Solo hay una importación activa a la vez. La caché GPU tiene un presupuesto
de 128 MiB, sin constituir un límite estricto de VRAM de todo el proceso.
Tiles, mipmaps, render asíncrono y perfiles del monitor quedan pendientes.
El render CPU crea un buffer del tamaño del viewport, no del documento.

El historial limita operaciones, todavía no bytes totales: varias importaciones
pueden retener mucha RAM hasta expulsar entradas, reemplazar el documento o cerrar.
La caché del compositor libera fuentes que ya no pertenecen al documento actual;
el historial puede seguir conservándolas para Deshacer. No hay grupos ni árbol
anidado en este hito: las capas raster forman la lista de nodos raíz inicial.

La exportación usa Skia CPU en un worker independiente del contexto OpenGL del
Canvas y un buffer a resolución nativa. JPEG requiere otro buffer para aplanar
sobre blanco. Escape solicita cancelación cooperativa; no interrumpe un codec
a mitad ni revierte un archivo ya confirmado. La UI añade la extensión elegida
y solicita confirmación antes de sobrescribir. Nuevo permite lados de hasta
32.768 píxeles y limita el área total a 33.554.432 píxeles.

## Usar el primer efecto

Abre una imagen, selecciona su capa y entra en la pestaña **Efectos**.
Pulsa **Añadir exposición** y cambia el valor EV. +1 duplica la luz lineal;
−1 la reduce a la mitad. La casilla permite comparar con el original.
El orden de la lista es el orden de procesamiento (arriba hacia abajo).
Exportar incluye los efectos visibles. Hasta 16 efectos por capa; el código
SkSL está incluido en el ejecutable y no se admiten shaders externos todavía.

## Guardar documentos editables

Usa **Archivo → Guardar** para crear un .pastra; **Guardar como** permite otra ruta.
PNG/JPEG siguen siendo exportaciones aplanadas. Los proyectos mantienen capas
transparentes, ocultas, opacidad, orden, IDs, píxeles y Effect Stack. El historial
Undo/Redo se conserva en la sesión, pero no se almacena en el archivo.

El formato inicial guarda píxeles sin compresión para conservar sus valores exactos.
Puede ocupar bastante más que un PNG/JPEG. Límites de archivo: 256 capas, 1 MiB de
metadatos y 512 MiB de píxeles únicos; cada imagen sigue limitada a 33.554.432
píxeles. Abrir/guardar trabaja en segundo plano, con cancelación entre bloques.
Versiones desconocidas y archivos dañados se rechazan sin reemplazar el documento.
Consulta [la especificación del formato](docs/project-format.md).

Para mover, selecciona una capa en Layers y arrastra dentro de su rectángulo
visible en el Canvas. No hay selección automática por píxel ni handles todavía.
También puedes escribir X/Y en el panel (rango −1.000.000 a +1.000.000 px).
Las capas vacías, ocultas o con opacidad cero se reposicionan con estos campos.
Perder el foco, cambiar documento/selección o iniciar una operación cancela la
vista previa. Los píxeles originales permanecen intactos, incluso fuera del lienzo.
