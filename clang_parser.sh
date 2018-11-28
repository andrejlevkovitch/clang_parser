#!/bin/bash

BEG_DIR=$(pwd)

FILES=$(find . -type f -name "*.h")

for file in $FILES
do
  filename=$(echo $file | cut -c 3-)
  /home/levkovich/Public/temp/interface_description_creator/build/IDC "/home/levkovich/Public/temp/Platformv2.0/DSControll/component_creator/xmls" $filename $BEG_DIR/../LibDS $BEG_DIR/../LibDSM $BEG_DIR/../LibDSTL $BEG_DIR/..
done
cd $BEG_DIR
