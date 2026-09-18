# Arquitectura de Photo Astra

## Alcance de este hito

Séptimo hito de V0.1: posición y arrastre de capas, con persistencia .pastra v2,
efectos SkSL, exportación, historial y Canvas CPU/OpenGL.

## Dependencias actuales

```text
app (composition root)
 ├─ application / DocumentSession (Qt Core/Concurrent, sin widgets)
 │   ├─ io / ImageLoader, ImageWriter y ProjectFile (codecs y persistencia)
 │   │   └─ core (C++20, sin Qt ni Skia)
 │   └─ render / Renderer (composición para exportar)
 ├─ render / SkiaRenderer (Skia privado; contrato sin Qt/Skia)
 │   └─ core
 └─ ui (Qt Widgets/OpenGLWidgets; consume los contratos anteriores)
```

`app` construye una sesión y transfiere un Renderer al Canvas. Los widgets usan
ownership padre/hijo de Qt; el backend usa RAII/PImpl. No hay singleton de
aplicación. MainWindow solicita importaciones a DocumentSession y presenta
snapshots; no modifica píxeles ni decodifica archivos.

Document valida dimensiones/título y contiene una lista de capas raster raíz,
ordenadas de abajo hacia arriba. Cada capa tiene ID estable, nombre, opacidad,
visibilidad y un RasterImage inmutable compartido (nulo significa transparente).
Una copia de Document copia metadatos y comparte los rasters, sin duplicar píxeles. RasterImage
define explícitamente RGBA8, sRGB, alpha premultiplicado y filas compactas.
Viewport contiene matemáticas de zoom/pan comprobables sin Qt; el Canvas posee
ese estado de vista, separado del documento.

File I/O hace una copia al almacenar el resultado de Qt en el raster del core.
Skia referencia esos bytes mediante un propietario compartido; al cambiar el
documento elimina de su caché las fuentes ausentes. History puede retenerlas. El buffer CPU se dimensiona al viewport.
La ruta OpenGL presenta directamente sobre el FBO de Qt, sin leer la imagen
GPU completa hacia CPU por frame. Se usa el FBO vigente en cada render y se
restaura el estado de Skia tras las operaciones de Qt. Los recursos del contexto
se liberan antes de destruir el widget; se abandonan si el contexto ya no existe.

El fallback CPU se activa cuando falla la creación de contexto o del backend.
Puede forzarse con `--cpu`. `gpuAccelerated` identifica la ruta OpenGL; un driver
OpenGL puede ser software, por lo que no es una medición del hardware físico.

DocumentSession admite una importación activa, ejecutada fuera del hilo UI.
Solo publica el nuevo documento tras completar la validación. Fallo/cancelación
conservan el anterior. La cancelación se comprueba antes/después de decodificar
y durante la copia; no interrumpe internamente el codec. Al cerrar la sesión
el worker no accede a widgets ni a un objeto destruido. El cierre del proceso
puede esperar al trabajo acotado pendiente del pool Qt.

Se extraen módulos cuando tienen comportamiento: en este hito se añadieron
`io` y `application`. No hay módulos vacíos para todas las funciones futuras.
La elección de Qt como adaptador de codecs evita incorporar dependencias de
decodificación duplicadas a Skia y mantiene el modelo independiente de ambos.

## Comandos e historial implementados

History vive en core, sin Qt. EditCommand es una variante tipada para añadir,
eliminar, mover, ocultar y cambiar opacidad. Primero genera/valida el nuevo
snapshot; un comando inválido no altera el documento ni el historial. No-ops no
crean entradas. Undo/Redo comparten snapshots; editar después de Undo descarta
la rama de Redo. La capacidad actual es 100 operaciones, no un presupuesto de RAM.
El marcador limpio usa identidad del snapshot y no retiene píxeles por sí solo.

DocumentSession coordina History y asigna IDs que no retroceden al deshacer.
Abrir reemplaza el documento y reinicia History; Importar capa es un comando.
Durante una importación se bloquean ediciones para que el resultado se aplique
sobre el documento previsto. Fallo/cancelación no cambian historial ni contenido.
LayersPanel muestra una proyección del modelo y solicita comandos a la sesión.
El Canvas solo reajusta la vista al reemplazar documento, no al editar capas.

SkiaCompositor está separado del manejo de superficies CPU/OpenGL. Usa mezcla
source-over, opacidad premultiplicada, recorte al documento y caché de imágenes
por fuente inmutable. El tablero de transparencia pertenece a la presentación,
no al compositor; esto permite exportar sobre una superficie transparente después.

La lista raíz solo contiene rasters por ahora. Los grupos requerirán nodos con
payload raster/grupo y recorrido recursivo; no se simulan con aplanado. Los IDs,
contratos por comando y fuentes compartidas seguirán siendo válidos al introducir
ese árbol. Máscaras, transformaciones y efectos vivirán en el modelo, nunca en
QListWidget ni en los recursos Skia de la caché.

## Arquitectura restante del MVP (diseñada, aún no implementada)

| Módulo | Responsabilidad y límites |
| --- | --- |
| Core / Document | Dimensiones, identidad/revisión, árbol de capas, selección y stack de efectos del documento. Sin QObject, QWidget, QImage ni recursos GPU. |
| Layers | Nodos con ID estable, propiedades comunes (opacidad, visibilidad, blend, transformación, máscaras, clipping, stack). Payload por tipo: raster, grupo, texto, vector o ajuste. Solo raster se implementará inicialmente. |
| Application / Commands | Operaciones validadas y coordinación; History ejecuta/revierte comandos sobre Document. UI solicita operaciones y recibe cambios; no es la autoridad de los datos. |
| Canvas | Estado de vista (zoom/pan), coordenadas y eventos de entrada. Presenta frames del renderer y traduce gestos a herramientas/comandos. |
| Renderer / Skia backend | Recursos Skia, superficies CPU/GPU y presentación. Ningún tipo Skia en Document. La interfaz se definirá con solicitudes de región, escala, revisión y resultados con handles propios. |
| Compositor | Recorre el árbol de abajo arriba, resuelve grupos, alpha premultiplicado, opacidad, máscaras y blends. Produce la entrada al stack del documento. |
| Effects / Shaders | Datos de efectos separados de programas compilados. Compilación, bindings SkSL y caché privados del backend. |
| File I/O | Decodificación PNG/JPEG y exportación a través de una frontera independiente de widgets. El futuro formato nativo será versionado y conservará la estructura, sin usar PSD como representación interna. |
| Color | Contratos explícitos de espacio/perfil, precisión y alpha en las imágenes. Inicialmente sRGB definido; adaptador OpenColorIO posterior. |
| Tools | Transforman entrada en comandos. No poseen documentos ni dibujan directamente en widgets. |
| Plugins | Solo diseño futuro: contratos versionados y política de permisos/aislamiento antes de cargar código de terceros. |

### Composición y edición no destructiva

```text
Fuente raster → efectos de capa ordenados → transformación/máscara/opacidad
             → compositor de capas/grupos → efectos de documento → presentación
```

El orden exacto de máscaras/transformaciones se fijará y probará al incorporarlas.
Las capas de ajuste operarán sobre la composición inferior, no como raster vacío.
Los grupos conservarán su jerarquía; no se modelará todo como una lista plana.

Cada instancia de efecto tendrá ID, tipo/versionado, enabled y parámetros tipados.
El stack conserva orden; añadir, desactivar, modificar, mover y eliminar serán
comandos reversibles. Los píxeles fuente permanecen separados del resultado.
El primer efecto implementado es Exposure con un parámetro acotado y shader incluido en
el proyecto, compilado con `SkRuntimeEffect`; no ejecutará shaders externos.

La futura introspección de uniforms se hará a partir del programa compilado.
Un float no define por sí mismo rango, unidad o valor inicial: un esquema de
parámetros aportará esos metadatos para generar controles. Se validarán tipos,
valores finitos, tamaños y children requeridos. Los errores de compilación se
presentarán sin sustituir el último programa válido. La compilación SkSL no es
una garantía de aislamiento: antes de aceptar código externo habrá límites de
recursos, estrategia para bloqueos GPU y aislamiento de procesos donde proceda.

### Imágenes grandes y concurrencia

Los futuros datos raster se compartirán mediante recursos inmutables y tiles
con copy-on-write. Undo conservará cambios de tiles/metadatos, no una copia
completa del documento por acción. Las solicitudes de render usarán snapshots
de una revisión; el backend no recorrerá un documento que otro hilo modifica.

La caché tendrá claves por ID/revisión, efecto, escala y espacio de color.
Las invalidaciones viajarán como regiones sucias; un efecto de vecindad deberá
declarar el margen requerido y un efecto global podrá invalidar todo. Se
presupuestarán memoria RAM/VRAM, tiles, mipmaps y previews con expulsión LRU.
Los trabajos asíncronos llevarán cancelación y revisión; los resultados obsoletos
se descartarán. Los recursos GPU se crearán y liberarán en su contexto/hilo
propietario. Estas optimizaciones no están implementadas en la base actual.

## Evolución por unidades verificables

1. **Implementado en Windows:** Skia fijado, CPU/OpenGL, importación PNG/JPEG y
   Canvas con zoom/pan. Portabilidad Linux/macOS aún sin ejecutar.
2. Árbol con capas raster, composición Normal/opacidad, comandos e historial;
   tests de orden, visibilidad, eliminación y reversión.
3. Exportación y primer Effect Stack con Exposure; comprobar identidad al
   desactivar, orden y conservación de los píxeles originales.
4. Robustecer límites, cancelación/caché y validar Windows, Linux y macOS.

## Referencias oficiales consultadas

- [Qt: CMake](https://doc.qt.io/qt-6/cmake-get-started.html)
- [Qt: deployment](https://doc.qt.io/qt-6/qt-generate-deploy-app-script.html)
- [Qt 6.8 para Windows](https://doc.qt.io/qt-6.8/windows.html)
- [Skia: SkSL y Runtime Effects](https://skia.org/docs/user/sksl/)

Se revisará la documentación de la revisión exacta de Skia antes de integrar
su API. No se da por elegida todavía una única API GPU para las tres plataformas.

## Creación y exportación implementadas

FileActions contiene los diálogos y delega en DocumentSession. Nuevo valida
nombre/dimensiones, reemplaza el snapshot y reinicia History. Durante importación
o exportación se deshabilitan acciones que modificarían el documento.

ImageExport recibe un Document inmutable y un Renderer. composeRaster representa
el documento completo a tamaño nativo sobre transparencia; comparte el compositor
con el Canvas, pero excluye vista y tablero. La sesión crea un SkiaRenderer CPU
exclusivo del worker, sin compartir caché ni contexto GPU con la UI. La instancia
se destruye en ese hilo. Los errores de construcción/composición/escritura se
convierten en resultados de error para la UI.

ImageWriter recibe la imagen sRGB premultiplicada. PNG conserva alpha; JPEG
aplana sobre blanco, con calidad 95. QSaveFile escribe en temporal y confirma
mediante commit, sin fallback de escritura directa. Fallar/cancelar antes del
commit conserva el destino anterior. Una cancelación que llegue después del
commit no revierte el archivo y se informa éxito. Los codecs y la composición
no se interrumpen internamente; se comprueba cancelación entre fases.

Exportar no reinicia History ni marca como guardado el documento editable.
El formato .pastra conserva las capas mediante Guardar; exportar sigue siendo una operación distinta.

APIs consultadas: [QSaveFile](https://doc.qt.io/qt-6/qsavefile.html) y
[QImageWriter](https://doc.qt.io/qt-6/qimagewriter.html), usando funciones
disponibles en Qt 6.8.3. No se añadieron dependencias.

## Effect Stack y Exposure implementados

Core/Effect define IDs, tipo Exposure, enabled y stops. Document valida el stack:
IDs no nulos/únicos por capa, tipo conocido, valores finitos en [−8,+8], máximo
16 entradas. SetEffects reemplaza los metadatos como un comando atómico; History
comparte los píxeles originales. La sesión asigna IDs, la UI controla selección.

EffectPipeline es privado de render. Compila el SkSL incorporado con
SkRuntimeEffect::MakeForColorFilter, una vez por instancia de renderer. Skia
integra el filtro en su programa GPU; la misma API funciona en CPU al exportar.
Para Exposure basta un filtro de color por píxel, sin superficies intermedias.
Los efectos espaciales futuros requerirán shaders con entrada de imagen u otras
pasadas; esta primera implementación no pretende resolver bloom o distorsiones.

El shader divide RGB por alpha, convierte a sRGB lineal, multiplica por 2^EV,
recorta [0,1], vuelve al espacio de trabajo y premultiplica. Alpha cero devuelve
cero; el alpha se conserva. Cada etapa recorta a SDR, por lo que el orden puede
cambiar el resultado. Exposure 0 y efectos desactivados se omiten exactamente.
La opacidad de capa y source-over se resuelven por Skia, conservando alpha.

SkiaCompositor compone los filtros en el orden del modelo y los cachea por capa
y metadatos. Cambiar zoom/pan no recompila SkSL ni reconstruye filtros; cambiar
parámetros reconstruye solo esa cadena. Borrar capas elimina sus entradas. No
hay buffers de imagen nuevos por efecto. No se afirma una tasa de FPS medida.

La compilación devuelve diagnóstico en lugar de abortar o aplicar silenciosamente
un efecto fallido. Canvas muestra el error y exportación no confirma el archivo.
No se carga código externo. Antes del Shader Editor harán falta validación de
uniforms, límites de recursos y aislamiento; compilar SkSL no es un sandbox.

Referencia oficial: [SkSL y Runtime Effects](https://skia.org/docs/user/sksl/).
APIs comprobadas contra las cabeceras del commit fijado de Skia m144.

## Persistencia editable implementada

ProjectFile traduce snapshots del core a un contenedor propio versionado y
viceversa. No introduce Qt en Document/History. Un encabezado y metadatos JSON
acotados describen rasters y capas; los bloques RGBA8 se escriben directamente
sin codificarlos como imágenes ni copiar el documento completo. Fuentes compartidas
se almacenan una vez y recuperan su propiedad compartida al cargar.

La sesión mantiene ruta del proyecto, una operación activa y watchers para
abrir/guardar. Save captura el snapshot inmutable y solo marca History como limpio
tras commit exitoso. El guardado no elimina Undo/Redo. Open publica el snapshot
solo después de validarlo por completo y reinicia History. Fallo/cancelación no
alteran la ruta actual ni el documento. IDs de capas nuevas avanzan tras cargar.

MainWindow guarda una continuación al elegir Guardar antes de cerrar/reemplazar.
Solo la ejecuta tras projectSaved; fallo o cancelación la descartan. Mientras una
operación está activa, cerrar se pospone hasta que termine o se cancele con Escape.
Cerrar descartando se agenda fuera del evento de cierre para evitar reentrancia.

Ver [ProjectFile](project-format.md) para contrato binario, límites y esquema actuales.

## Posición y gesto de movimiento

LayerPosition es parte de core: coordenadas enteras con límite ±1.000.000, validadas
en Document. SetPosition participa en History. SkiaCompositor dibuja la imagen en
esa posición; los buffers, el stack y la caché de imágenes no cambian al mover.
Exportar y guardar usan la misma posición del modelo.

LayerMoveGesture contiene inicio, hit test del rectángulo, cálculo de desplazamiento
en coordenadas de imagen, redondeo, límites y snapshots transitorios. No depende
de Qt ni Skia. Canvas traduce coordenadas/eventos y presenta esos snapshots sin
publicarlos en DocumentSession. Soltar emite un comando final; un clic o volver
al origen no genera historial. La sesión sigue siendo la autoridad del documento.

Escape, perder foco, cambiar selección/documento, redimensionar o ajustar la vista,
iniciar pan u otra operación cancelan el gesto. Mientras se arrastra se ignora zoom
para conservar la relación entre cursor y coordenadas de imagen. No hay hit test
por alpha, selección automática, escala o rotación en este hito.

## Escala no destructiva

LayerScale almacena dos factores positivos finitos [1/64,64] en el modelo.
SetScale valida y participa en History; no cambia dimensiones ni buffers fuente.
SkiaCompositor guarda la matriz, aplica traslación y escala local, dibuja y restaura
la matriz para la siguiente capa. CPU, GPU y exportación comparten este recorrido.
El recorte del documento permanece fijo. LayerMoveGesture usa el rectángulo escalado.

LayerScaleDialog presenta valores y genera snapshots mediante History temporal
independiente de DocumentSession. Canvas muestra esos snapshots sin alterar zoom;
aceptar envía un SetScale, cancelar restaura el documento de la sesión. La lógica
de validación y aplicación del comando sigue en core. Anclaje superior izquierdo;
no hay rotación, remuestreo destructivo, tiradores ni generación de mipmaps todavía.
El muestreo es bilineal: reducciones grandes pueden perder detalle o mostrar aliasing.

## Modos de fusión básicos

BlendMode es un enum del modelo, independiente de Qt/Skia. SetBlendMode conserva
los snapshots inmutables y valida modos admitidos antes de modificar History.
El selector del panel usa los valores del enum como datos, no el texto traducido.

SkiaCompositor asigna Normal a kSrcOver, Multiplicar a kMultiply y Trama a kScreen
en el paint de cada capa, junto con opacidad y efectos. Se usa el espacio de destino
sRGB codificado actual, sin conversión a composición lineal. No se usa kModulate,
que tiene una semántica de alpha diferente de Multiplicar.

En Canvas, cuando hay capas visibles con fusión no Normal, SkiaRenderer compone
el documento en un saveLayer transparente recortado al documento y a la vista.
Después lo presenta sobre el tablero. Así el fondo UI no participa en las fusiones.
Este recurso temporal añade memoria de superficie visible; no duplica los rasters
originales ni reserva una superficie del documento entero al hacer zoom. Exportación
ya comienza en transparencia y no requiere esta superficie adicional. Normal conserva
su recorrido anterior. Para comprobarlo se contrastan CPU/GPU/exportación y alpha parcial.

APIs comprobadas en los headers de la revisión fijada de Skia m144:
include/core/SkBlendMode.h, SkCanvas.h y SkPaint.h. Sin dependencias nuevas.
