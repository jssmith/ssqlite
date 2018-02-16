#!/bin/bash

rm -rf build-dist
cp -R dist build-dist

pushd .
cd build-dist
rm -f ../benchmark-vfs-fn.zip
zip -r ../benchmark-vfs-fn.zip ./*
popd # dist directory
