#!/usr/bin/env bash
set -euo pipefail

clang++ -g -O3 -std=c++20 ./main.cpp

KiB=1024
GiB=$(( 1024 * 1024 * 1024 ))
GiB1p5=$(( (1024 * 1024 * 1024) + (512 * 1024 * 1024) ))

resize_file()
{
	touch "${1}"
	if [ "$(stat -c %s "${1}")" != "${2}" ]; then
		truncate -s 0 "${1}"
		head -c "${2}" /dev/urandom > "${1}"
	fi
}

resize_file ./here 0
resize_file ./here-1KiB "${KiB}"
resize_file ./here-1GiB "${GiB}"
resize_file ./here-1.5GiB "${GiB1p5}"

if ./a.out; then
	>&2 echo "No parameters should have failed (file empty)!"
	exit 1;
fi

if ./a.out ./here-1KiB; then
	>&2 echo "./here-1KiB should have failed (file too small)!"
	exit 1
fi

if ! ./a.out ./here-1GiB; then
	>&2 echo "./here-1GiB should have succeeded!"
	exit 1
fi

if ./a.out ./here-1GiB 0 "${KiB}"; then
	>&2 echo "./here-1GiB with 1KiB offset should have failed (offset not aligned to 1GB boundary)!"
	exit 1
fi

if ! ./a.out ./here-1.5GiB; then
	>&2 echo "./here-1.5GiB should have succeeded!"
	exit 1
fi

if ./a.out ./here-1.5GiB 0 "${KiB}"; then
	>&2 echo "./here-1.5GiB with 1KiB offset should have failed (offset not aligned to 1GB boundary)!"
	exit 1
fi

if ./a.out ./here-1.5GiB 0 "${GiB}"; then
	>&2 echo "./here-1.5GiB with 1KiB offset should have failed (offset leads to 1GB boundary, but results in bus error past 512MiB)!"
	exit 1
fi

echo "Success!"
