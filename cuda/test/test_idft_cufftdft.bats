#!/usr/bin/env bats

load ../../test/wct-bats.sh

@test "test_idft with torchdft" {
    usepkg aux                  # to get test_dft
    run test_idft cuFftDFT WireCellCuda
    echo "$output"
    [[ "$status" -eq 0 ]]
}
