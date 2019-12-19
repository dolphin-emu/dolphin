#!/bin/bash

set -e

ROOT_SRC_DIR=$PROJECT_DIR/DolphiniOS
ROOT_DOLPHIN_DIR=$PROJECT_DIR/../../..
ANDROID_RES_DIR=$ROOT_DOLPHIN_DIR/Source/Android/app/src/main/res
ONEX_DIR=$ANDROID_RES_DIR/drawable-hdpi
TWOX_DIR=$ANDROID_RES_DIR/drawable-xhdpi
THREEX_DIR=$ANDROID_RES_DIR/drawable-xxhdpi

# Clear all images
rm -rf $ROOT_SRC_DIR/DolphinImages.xcassets/*.imageset

for path in $ONEX_DIR/*
do
  FILENAME=$(basename "$path" .png)
  
  mkdir $ROOT_SRC_DIR/DolphinImages.xcassets/$FILENAME.imageset
  
  cp $ONEX_DIR/$FILENAME.png $ROOT_SRC_DIR/DolphinImages.xcassets/$FILENAME.imageset/$FILENAME\@1x.png
  cp $TWOX_DIR/$FILENAME.png $ROOT_SRC_DIR/DolphinImages.xcassets/$FILENAME.imageset/$FILENAME\@2x.png
  cp $THREEX_DIR/$FILENAME.png $ROOT_SRC_DIR/DolphinImages.xcassets/$FILENAME.imageset/$FILENAME\@3x.png
  
  sed "s/FILENAME/$FILENAME/g" $ROOT_SRC_DIR/BuildResources/DolphinImageTemplate.json.in > $ROOT_SRC_DIR/DolphinImages.xcassets/$FILENAME.imageset/Contents.json
done
