if not exist "dist" mkdir dist

cl.exe upload.c ^
    /I"vcpkg_installed\x64-windows\include" ^
    /Fe"dist\upload.exe" ^
    /Fo"dist\upload.obj" ^
    /link ^
    /LIBPATH:"vcpkg_installed\x64-windows\lib" ^
    libcurl.lib cjson.lib ^
    || exit /b

for %%f in (libcurl.dll cjson.dll zlib1.dll) do (
    if not exist dist\%%f copy vcpkg_installed\x64-windows\bin\%%f dist
)
