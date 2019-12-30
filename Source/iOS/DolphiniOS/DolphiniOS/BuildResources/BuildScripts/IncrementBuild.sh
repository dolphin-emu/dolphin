#!/bin/bash

set -e

cd $PROJECT_DIR

xcrun agvtool next-version -all
