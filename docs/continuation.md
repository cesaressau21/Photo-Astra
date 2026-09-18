# Punto de continuación — 17 de septiembre de 2026

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

Siguiente unidad recomendada: preparar y compilar Qt/Skia/editor en un entorno
Linux real. Ver docs/ci.md para alcance y dependencias. La matriz core está validada
en los tres sistemas, pero la UI, GPU y empaquetado fuera de Windows siguen pendientes.
No afirmar soporte completo validado hasta probar esos componentes.
No comenzar plugins, PSD ni pinceles en el siguiente hito.
