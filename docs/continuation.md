# Punto de continuación — 17 de septiembre de 2026

Último hito: presets y CI core preparados, Windows Debug/Release 1/1 por preset;
actionlint 1.7.12 sin errores. Matriz remota de seis trabajos aún NO ejecutada.
No hay remoto Git configurado; WSL consultado sin entorno Linux utilizable.
Archivos nuevos: CMakePresets.json, scripts/build-core.ps1,
.github/workflows/core.yml y docs/ci.md. Logs .tools/milestone10-core-*.log.
No se modificó el editor ni se repitieron sus pruebas en este hito.

Hito anterior, modos de fusión terminados: Release/Debug 4/4; core independiente 1/1;
UI Windows GPU 23/23. Paquete actualizado en out/package; iniciado y respondiendo
con título Sin título — Photo Astra · V0.1. Sin commits/publicación.
Mantener límite solicitado: detenerse si alguna ventana de uso alcanza 99 %.
Última lectura de uso: 92 % cinco horas / 61 % semanal.

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

Siguiente unidad recomendada: obtener la primera ejecución remota de la matriz
core cuando el usuario indique el repositorio y autorice publicar; no inventar
remoto ni subir el proyecto por cuenta propia. Después, preparar y compilar
Qt/Skia/editor en un entorno Linux real. Ver docs/ci.md para alcance y dependencias.
Las funciones mínimas V0.1 están presentes; Linux/macOS y empaquetado en esos
sistemas todavía no se han probado. No afirmar soporte validado sin ejecutarlo.
No comenzar plugins, PSD ni pinceles en el siguiente hito.
