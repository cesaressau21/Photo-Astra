# Dependencia Skia reproducible

Motor: código oficial de https://skia.googlesource.com/skia.git, rama de origen
`chrome/m144`, fijado en `ed427fd003ba3bc6eb4a8ae0337f9cdafc39e5fb`.
No se usan binarios de terceros ni código de otros editores.

## Windows comprobado

```powershell
.\scripts\bootstrap-skia.ps1 -Configuration Release
.\scripts\bootstrap-skia.ps1 -Configuration Debug
```

Requiere Git, Python en PATH y Visual Studio Build Tools con C++ y CMake/Ninja.
El script consulta Visual Studio con vswhere y utiliza el Python real para GN,
evitando depender del alias `python3` de Microsoft Store. No usa depot_tools
ni descarga todos los módulos opcionales de Skia.

El script crea `.deps/skia` solo si no existe, descarga el commit exacto y lo
verifica. Si un checkout existente tiene otra revisión o cambios de código,
se detiene sin sobrescribirlo. Las descargas de preparación requieren red;
compilar posteriormente y ejecutar el editor no requieren servicios cloud.

GN se obtiene con `bin/fetch-gn` del commit fijado: ese archivo fija a su vez
GN en `b2afae122eeb6ce09c52d63f67dc53fc517dbdc8`. Sus argumentos comunes están
en `cmake/skia-common.gn`; los argumentos finales quedan en `args.gn` dentro
de cada directorio de salida:

| Configuración | Salida | Runtime MSVC |
| --- | --- | --- |
| Release | `.deps/skia/out/photoastra/skia.lib` | `/MD` |
| Debug | `.deps/skia/out/photoastra-debug/skia.lib` | `/MDd` |

No se mezclan las ABI de Debug y Release. `is_trivial_abi=false` evita exigir
el atributo de ABI específico de Clang al consumidor MSVC.

La biblioteca incluye raster CPU, Ganesh/OpenGL y SkSL. Los codecs PNG/JPEG y
la lectura de perfiles se hacen en el adaptador Qt de File I/O, por lo que
se desactivan sus equivalentes y otras dependencias opcionales en Skia.
Esto no afecta a la independencia de Document ni permite que un widget
decodifique imágenes. El backend puede ampliarse sin cambiar los píxeles del
modelo. No se ejecutan shaders suministrados por usuarios.

GN presupone un prefijo inglés para `/showIncludes`. El script detecta el
prefijo real de MSVC con una compilación mínima y lo configura en
`photoastra.ninja`, que incluye el build generado por GN. Así se conservan
las dependencias de cabeceras con MSVC en español, sin modificar fuentes de
Skia. Al cambiar ese prefijo limpia únicamente los productos de ese build.

Se ha usado MSVC 19.44. La documentación de Skia recomienda Clang para mejor
rendimiento de algunas rutas CPU. Este hito verifica corrección y GPU; no
incluye benchmarks ni afirma rendimiento de producción del raster CPU.

## Linux comprobado (Release)

Ubuntu 24.04 x86_64 y Clang 18: `bash scripts/bootstrap-skia-linux.sh Release 4`.
El script comparte los argumentos anteriores y fija cc/cxx a Clang 18. La
compilación completa Qt/Skia y las pruebas están documentadas en [Linux](linux.md).
Debug del editor completo y GPU física Linux siguen pendientes.

## macOS: instrucciones preparadas, no ejecutadas

Con Git, Python 3, Ninja, Clang/C++20 y las bibliotecas gráficas del sistema:

```sh
git clone https://skia.googlesource.com/skia.git .deps/skia
git -C .deps/skia checkout --detach ed427fd003ba3bc6eb4a8ae0337f9cdafc39e5fb
python3 .deps/skia/bin/fetch-gn
mkdir -p .deps/skia/out/photoastra
cp cmake/skia-common.gn .deps/skia/out/photoastra/args.gn
```

Añade `cc="clang"` y `cxx="clang++"` al `args.gn` según tu toolchain; en
macOS utiliza el toolchain de Xcode y una arquitectura compatible con Qt.

```sh
cd .deps/skia
bin/gn gen out/photoastra
ninja -C out/photoastra skia
```

Para Debug usa `out/photoastra-debug`, `is_debug=true`,
`is_official_build=false`, `skia_enable_spirv_validation=false` y
`skia_enable_gpu_debug_layers=false`, además de los argumentos comunes.
No añadas `/MD` o `/MDd` fuera de Windows. Se esperan archivos `libskia.a`
en los directorios correspondientes. Después compila el proyecto con CMake.
Los generadores multiconfiguración necesitan ambas bibliotecas.

OpenGL es la primera ruta GPU, encapsulada detrás de Renderer. No se ha
implementado Metal, Vulkan ni Graphite. En macOS OpenGL es una API de legado;
se evaluará un backend nativo antes de considerar soporte de producción.

## Referencias y licencia

- [Compilación oficial de Skia](https://skia.org/docs/user/build/).
- [Código de la revisión utilizada](https://skia.googlesource.com/skia/+/ed427fd003ba3bc6eb4a8ae0337f9cdafc39e5fb/).
- [SkSL / Runtime Effects](https://skia.org/docs/user/sksl/).
- [Ciclo de vida de QOpenGLWidget](https://doc.qt.io/qt-6/qopenglwidget.html).

La licencia original de Skia se conserva en
`third_party/licenses/Skia-LICENSE.txt` y en el checkout de la dependencia.
Las condiciones de distribución del proyecto completo siguen pendientes de
la elección de licencia y del inventario final de dependencias.
