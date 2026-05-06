set PATH=%CD%\install\bin;%PATH%
set VCPKG_DIR=%CD%\build\vcpkg_installed
set PATH=%VCPKG_DIR%\x64-windows-release\bin;%PATH%
set PATH=%VCPKG_DIR%\x64-windows-release\plugins;%PATH%
set PATH=%VCPKG_DIR%\x64-windows-release\tools\proj;%PATH%
set GDAL_DATA=%VCPKG_DIR%\x64-windows-release\share\gdal
set PROJ_DATA=%VCPKG_DIR%\x64-windows-release\share\proj
