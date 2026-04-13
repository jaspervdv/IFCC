#!/usr/bin/env bash

set -e  # exit on error

cd supportApps/IfcSwap/

# installing the dependencies
echo "Installing runtime dependencies..."
npm install web-ifc @thatopen/fragments

echo "Installing dev dependencies..."
npm install -D typescript ts-node @types/node

# set up the folder structure
if [ ! -d "dist" ]; then
    mkdir dist
fi

echo "Copying web-ifc WASM..."
cp node_modules/web-ifc/web-ifc-node.wasm dist/

# compiling IfcSwap
echo "Compiling IfcSwap..."
npx tsc --project tsconfig.json

echo "Building IfcSwap binary..."
npx pkg dist/IfcSwap.js --targets node18-linux-x64 --output dist/IfcSwap/

echo "Build finished."

cd ../..

read -p "Press any key to continue..."