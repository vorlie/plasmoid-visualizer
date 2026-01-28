#!/bin/bash

# Exit on error
set -e

PROJECT_NAME="plasmoid-visualizer"
VERSION="1.0"
BUILD_DIR="build_release"
PLASMA_BUILD_DIR="build_plasma_release"
ASSETS_DIR="release_assets"

echo "Creating release assets for $PROJECT_NAME v$VERSION..."

# 1. Clean and create directories
rm -rf $BUILD_DIR $PLASMA_BUILD_DIR $ASSETS_DIR
mkdir -p $ASSETS_DIR

# 2. Build Standalone App
echo "Building standalone application..."
mkdir -p $BUILD_DIR && cd $BUILD_DIR
cmake ../ -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
cd ..

# 3. Build Plasma Plugin
echo "Building Plasma plugin..."
mkdir -p $PLASMA_BUILD_DIR && cd $PLASMA_BUILD_DIR
cmake ../plasma_widget -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
cd ..

# 4. Package Plasma Widget (.plasmoid)
echo "Packaging Plasma widget..."
# Copy plugin to the widget directory
cp $PLASMA_BUILD_DIR/libvisualizerv2plugin.so plasma_widget/contents/ui/
# Create the .plasmoid file (it's just a zip)
cd plasma_widget
zip -r ../$ASSETS_DIR/$PROJECT_NAME.plasmoid . -x "src/*" "CMakeLists.txt" "icon.png"
zip -u ../$ASSETS_DIR/$PROJECT_NAME.plasmoid icon.png
cd ..

# 5. Package Standalone Binary
echo "Packaging standalone binary..."
zip -j $ASSETS_DIR/$PROJECT_NAME-standalone-linux.zip $BUILD_DIR/PlasmoidVisualizer

echo "------------------------------------------------"
echo "Done! Assets created in $ASSETS_DIR/:"
ls -lh $ASSETS_DIR
echo "------------------------------------------------"
echo "You can now upload these files to your GitHub/GitLab release."
