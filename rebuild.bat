@echo off
cd /d "%~dp0"
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" ForensicsProject.vcxproj /p:Configuration=Release /p:Platform=x64 /t:Rebuild /m /nologo
if errorlevel 1 (
    echo Build failed!
    exit /b 1
)
echo Build succeeded!
