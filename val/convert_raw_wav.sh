#!/bin/bash

function raw2wav16k() {
  if [ "$#" -eq 0 ]; then
    echo -n 'No filename given' >&2
    echo # Newline
    exit 1
  fi

  if [ $# -ne 1 ]; then
    echo -n "This function only accepts one argument as it's name "
    echo # Newline
    exit 2
  fi

  fullfilename=$(basename -- "$1")
  filename="${fullfilename%.*}"

  extension="${fullfilename##*.}"
  desired_extension="raw"

  if [[ "$extension" != "$desired_extension" ]]; then
    echo -n "The extension of the file must be .raw, no ${extension}"
    echo # Newline
    exit 3
  fi

  echo -n "Converting ${filename}.${extension} to ${filename}.wav"
  echo # Newline

  sox -r 16000 -c 1 -e signed -c 1 -e signed -b 16 "${filename}".raw "${filename}".wav
}

if [ $# -ne 1 ]; then
  echo -n "This script only accepts one argument, the directory with the .raw files sampled at 16kHz "
  echo # Newline
  exit 2
fi

if [[ ! -d "$1" ]]; then
  echo -n "The directory doesn't exist"
  echo # Newline
  exit 4
fi

pushd "${1}" || exit 4

for i in *.raw; do
  # The guard ensures that if there are no matching files, the loop will exit
  # without trying to process a non-existent file name *.raw
  [ -f "$i" ] || break
  raw2wav16k "$i"
done

popd || exit
