# Punto de continuación — 18 de septiembre de 2026

Editor Linux Release validado: Ubuntu 24.04, Clang 18, Qt 6.8.3 (qtbase + icu),
Skia fijado compilado y cacheado. Commit a9a0485: CTest 4/4, OpenGL UI 23/23.
Ejecución aprobada 35357936757. Mesa llvmpipe LLVM 20.1.2, OpenGL 4.5, Xvfb.
GDB localizó el crash en GrGLExtensions::init; el resolver Qt ahora devuelve
nullptr para nombres EGL y mantiene funciones GL. Regresión incluida en test
de fallback. No se modificó Skia ni se deshabilitó GPU para pasar los tests.
Evidencia .tools/linux-verified: XML, QtTest y captura revisada. GDB se retira
de la ejecución habitual después de diagnosticar; el workflow prueba sin él.
Correcciones ya publicadas: cast uint64_t de altura en FileActions; ICU faltante;
referencias const en tests JSON; helper enterDialogFile escribe fileNameEdit
porque QFileDialog::selectFile ignora el campo visible cuando tiene foco.
Windows Release final 4/4, GPU NVIDIA 23/23: .tools/milestone12-resolver-windows.log
y build/release/resolver-gpu-tests.txt.
Guía nueva docs/linux.md; bootstrap scripts/bootstrap-skia-linux.sh.
Respetar límite de uso del usuario: detenerse al alcanzar 99 % en cualquier ventana.

Último hito: repositorio conectado y primera matriz core remota aprobada, 6/6.
Windows 2022, Ubuntu 24.04 y macOS 15, Debug/Release; commit inicial d5cea0d.
Origen: https://github.com/cesaressau21/Photo-Astra.git, rama main.
Ejecución: https://github.com/cesaressau21/Photo-Astra/actions/runs/35302057942.
Los presets locales Windows también pasaron; actionlint 1.7.12 sin errores.
Archivos nuevos: CMakePresets.json, scripts/build-core.ps1,
.github/workflows/core.yml y docs/ci.md. Logs .tools/milestone10-core-*.log.
No se modificó el editor ni se repitieron sus pruebas en este hito.

Hito anterior, modos de fusión terminados: Release/Debug 4/4; core independiente 1/1;
UI Windows GPU 23/23. Paquete actualizado en out/package; iniciado y respondiendo
con título Sin título — Photo Astra · V0.1. Binarios y dependencias fuera de Git.
La licencia propia sigue pendiente; no se aplicó GPL ni se publicaron releases.

Capas → Escalar…: porcentajes X/Y, enlace opcional, reset, vista previa aislada,
aceptar un SetScale y Undo/Redo; cancelar/Escape no modifica sesión. Escala
positiva [1/64,64], anclaje superior izquierdo, sin reescribir raster original.
Arrastre reconoce rectángulo escalado; Skia CPU/GPU y exportación usan la escala.
.pastra v4 guarda escala y blendMode; lector admite v1/v2/v3. Versiones anteriores
a v3 usan escala (1,1); anteriores a v4 usan fusión Normal.

Capas → Fusión: Normal, Multiplicar y Trama con SetBlendMode, Undo/Redo y
exportación. Modelo independiente de Qt/Skia, validación de enum y tipos de archivo.
Skia usa kSrcOver/kMultiply/kScreen. Canvas aísla composición con saveLayer cuando
hay modos no Normal visibles para excluir el tablero del cálculo; exportación
compone directamente sobre transparencia. Fusión en sRGB codificado, sin nuevos
buffers originales ni dependencias. Archivo UI anterior LayerScaleDialog.cpp intacto.
Captura y reporte GPU: build/release/blend-controls.png, blend-gpu-tests.txt.
Logs: .tools/milestone9-{release,debug,install}.log.

Siguiente unidad recomendada: compilar y probar el editor Qt/Skia completo en macOS.
La matriz core ya está validada en tres sistemas. Linux completo Release está
validado con OpenGL por software; GPU física Linux, Wayland, Debug Linux y paquetes
fuera de Windows siguen pendientes. No afirmar soporte completo de esos escenarios.
No comenzar plugins, PSD ni pinceles en el siguiente hito.
