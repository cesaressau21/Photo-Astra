# Editor completo en Linux

Entorno del primer workflow: Ubuntu 24.04 x86_64, Clang 18, Qt 6.8.3 y la
revisión fijada de Skia m144. Configuración Release. El workflow
[Linux editor](https://github.com/cesaressau21/Photo-Astra/actions/workflows/linux-editor.yml)
conserva los resultados de CTest, el registro de QtTest y una captura del Canvas.

## Preparación

Instala Git, Python 3 con venv, CMake >= 3.24, Ninja, Clang 18, bibliotecas de
desarrollo OpenGL/EGL y dependencias de Qt/X11. La lista exacta de paquetes Ubuntu
está en `.github/workflows/linux-editor.yml`. Xvfb, xauth y mesa-utils se usan
para probar sin una sesión gráfica; no son funciones fundamentales del editor.

Desde la raíz del repositorio, instala el kit Qt:

```sh
python3 -m venv .tools/aqt
.tools/aqt/bin/python -m pip install aqtinstall==3.3.0
.tools/aqt/bin/python -m aqt install-qt linux desktop 6.8.3 linux_gcc_64 --archives qtbase --outputdir .deps/Qt
bash scripts/bootstrap-skia-linux.sh Release 4
```

El bootstrap exige Linux y Clang 18; verifica el commit y rechaza fuentes Skia
modificadas o una revisión diferente. Descarga GN mediante el script oficial de
esa revisión. Comparte `cmake/skia-common.gn` con Windows; no modifica sus fuentes.
La salida es `.deps/skia/out/photoastra/libskia.a`. Admite `Debug` en una carpeta
separada, pero el primer workflow completo comprueba solo Release.

## Compilar, probar y abrir

```sh
CC=clang-18 CXX=clang++-18 cmake -S . -B build/linux-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$PWD/.deps/Qt/6.8.3/gcc_64" \
  -DPHOTO_ASTRA_WARNINGS_AS_ERRORS=ON -DBUILD_TESTING=ON
cmake --build build/linux-release --parallel 4
ctest --test-dir build/linux-release --output-on-failure
build/linux-release/src/app/photo_astra
```

El último comando requiere una sesión gráfica. Las pruebas CTest usan el backend
CPU y la plataforma Qt offscreen, más los tests del modelo y archivos. En una
sesión X11 puedes probar el recorrido OpenGL por separado:

```sh
PHOTO_ASTRA_REQUIRE_GPU=1 build/linux-release/tests/ui_tests -platform xcb
```

`PHOTO_ASTRA_REQUIRE_GPU` exige que se active el backend Ganesh/OpenGL; no mide
rendimiento ni demuestra que el proveedor OpenGL use hardware. El CI define
`LIBGL_ALWAYS_SOFTWARE=1` y usa Xvfb/Mesa: comprueba esa ruta gráfica por software,
incluyendo lectura del framebuffer y comparación con la exportación PNG.

## Límites de la comprobación

El resultado del workflow se debe consultar antes de afirmar que una revisión
está validada. No se publica un instalador Linux en este hito. Siguen pendientes
GPU física Linux, Wayland, otros sistemas/distribuciones, Debug del editor completo
y el editor Qt/Skia en macOS. La matriz del núcleo en los tres sistemas es otra
comprobación y no sustituye estas pruebas.

Referencias: [Qt 6.8/X11](https://doc.qt.io/qt-6.8/linux-requirements.html),
[aqtinstall 3.3.0](https://aqtinstall.readthedocs.io/en/v3.3.0/cli.html),
[compilación oficial de Skia](https://skia.org/docs/user/build/).
