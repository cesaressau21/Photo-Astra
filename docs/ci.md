# Integración continua y compilación multiplataforma

## Alcance de esta primera matriz

`.github/workflows/core.yml` configura seis trabajos: Windows 2022, Ubuntu 24.04
y macOS 15, cada uno en Debug y Release. Compilan el núcleo C++20 y ejecutan
`core.document`, con warnings tratados como errores. No necesitan Qt ni Skia.
La matriz comprueba Document, capas, History, efectos, transformaciones y Viewport;
no demuestra que la ventana, OpenGL o el empaquetado funcionen en esos sistemas.

Se ejecutará en push, pull request o lanzamiento manual desde GitHub Actions.
Repositorio: [cesaressau21/Photo-Astra](https://github.com/cesaressau21/Photo-Astra).
Los resultados remotos se consultan en [Actions](https://github.com/cesaressau21/Photo-Astra/actions/workflows/core.yml).
La tabla siguiente registra las comprobaciones locales previas a la primera subida.
El CI es opcional; compilar y usar el editor localmente no depende de ese servicio.

Los trabajos tienen diez minutos de límite, permisos de lectura y checkout sin
credenciales persistentes. Checkout v5.0.0 está fijado al SHA verificado
`08c6903cd8c0fde910a37f88322edcfb5dd907a8`. No se generan releases ni se publican
paquetes. Si falla un sistema, los restantes continúan. Los fallos de tests se
incluyen en el log; una ejecución sin tests también falla.

## Reproducción local

Desde la raíz, con CMake >= 3.24, Ninja y compilador C++20 en PATH:

```sh
cmake --preset core-debug
cmake --build --preset core-debug --parallel 2
ctest --preset core-debug

cmake --preset core-release
cmake --build --preset core-release --parallel 2
ctest --preset core-release
```

En Windows, con Visual Studio Build Tools y su componente CMake/Ninja:

```powershell
.\scripts\build-core.ps1 -Configuration Debug
.\scripts\build-core.ps1 -Configuration Release
```

El script prepara MSVC x64 y usa los mismos presets que el CI. Funciona también
invocándolo desde otra carpeta, y restaura la carpeta de trabajo al terminar.
Las salidas están en `build/core-debug` y `build/core-release`; la compilación
del editor conserva sus carpetas existentes. Los overrides personales pertenecen
a `CMakeUserPresets.json`, excluido de Git. Para cambiar compilador usa una carpeta
de build distinta o un preset personal; no reutilices la caché de otro toolchain.

## Estado comprobado el 17 de septiembre de 2026

| Comprobación | Resultado |
| --- | --- |
| Presets y script MSVC, Debug y Release | Compilados; 1/1 test en cada configuración |
| Workflow con actionlint 1.7.12 | Sin errores de sintaxis/expresiones; shellcheck/pyflakes no ejecutados |
| Matriz remota GitHub | Consultar Actions; no se contabiliza como prueba local |
| Linux local | No disponible; la consulta WSL no encontró un entorno utilizable |
| macOS local | No disponible en este equipo Windows |
| Editor Windows completo | Hito anterior: Release/Debug 4/4, GPU 23 casos; no modificado aquí |

Logs locales: `.tools/milestone10-core-debug.log` y
`.tools/milestone10-core-release.log`. El validador se descargó de la publicación
oficial de actionlint y se verificó su SHA-256 antes de ejecutarlo; queda en
`.tools/`, fuera del repositorio y de las dependencias del editor.

## Siguiente etapa: editor completo fuera de Windows

Antes de ampliar el CI a la UI hay que ejecutar una compilación real de Qt/Skia
en cada plataforma. La preparación manual está en [Skia](skia.md). Requisitos
concretos a verificar:

- Kit Qt 6 con Widgets, OpenGLWidgets, Concurrent y Test, y CMake capaz de encontrarlo.
- Skia en la revisión fijada, con Ganesh/OpenGL y configuración Debug/Release
  correspondiente. Qt, Skia y aplicación deben compartir arquitectura y ABI de C++.
- Linux: Clang y bibliotecas/cabeceras OpenGL y de hilos necesarias para el backend.
- macOS: Xcode/SDK y arquitectura del kit Qt; comprobar los frameworks declarados
  en `cmake/Skia.cmake` mediante una compilación real. No se ha verificado Metal.
- Separar tests offscreen de pruebas con contexto OpenGL real. Los primeros no
  bastan para declarar GPU funcional ni compatibilidad del paquete instalado.

No se han modificado los enlaces o argumentos de Skia sin poder comprobarlos en
esos sistemas. El siguiente resultado útil será una ejecución de la matriz core,
y después una compilación completa del editor en Linux con sus cuatro suites.

## Documentación oficial consultada

- [Presets compatibles con CMake 3.24](https://cmake.org/cmake/help/v3.24/manual/cmake-presets.7.html).
- [Runners de GitHub](https://docs.github.com/en/actions/reference/runners/github-hosted-runners).
- [Checkout v5.0.0](https://github.com/actions/checkout/tree/v5.0.0).
- [Actionlint 1.7.12](https://github.com/rhysd/actionlint/releases/tag/v1.7.12).
