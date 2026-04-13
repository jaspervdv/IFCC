@echo off
cd supportApps/IfcSwap

REM installing the dependencies
echo Installing runtime dependencies...
cmd.exe /c npm install web-ifc @thatopen/fragments

echo Installing dev dependencies...
cmd.exe /c npm install -D typescript ts-node @types/node

REM set up the folder structure
if not exist dist (
    mkdir dist
)
echo Copying web-ifc WASM...
copy node_modules\web-ifc\web-ifc-node.wasm dist\

REM compiling IfcSwap
echo Compiling IfcSwap...
cmd.exe /c npx tsc --project tsconfig.json

echo Building IfcSwap EXE...
cmd.exe /c npx pkg dist\IfcSwap.js --targets node18-win-x64 --output dist\IfcSwap.exe

echo Build finished.
cd ../..
pause