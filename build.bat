if not exist "build" mkdir build

cl.exe lumilapio.c ^
    /I"vcpkg_installed\x64-windows\include" ^
    /Fe"build\lumilapio.exe" ^
    /Fo"build\lumilapio.obj" ^
    /link ^
    /LIBPATH:"vcpkg_installed\x64-windows\lib" ^
    libcurl.lib cjson.lib ^
    || exit /b

for %%f in (libcurl.dll cjson.dll zlib1.dll) do (
    if not exist build\%%f copy vcpkg_installed\x64-windows\bin\%%f build
)
