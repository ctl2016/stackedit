#!/bin/bash

if [ -d /work ]; then
    echo "** using mounted /work directory"
    cd /work
fi

echo "start notebook"
pwd
ls -lh ~/
ls -lh ~/micromamba/etc
micromamba activate
jupyter lab --ip='*' --port=8888 --no-browser

