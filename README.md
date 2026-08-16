# mini-doorstop

A minimal proxy DLL that boots **BepInEx 6 (IL2CPP)** into Unity games.

Primarily built for the Epic Online Services SDK (`EOSSDK-Win64-Shipping.dll`),
but can be adapted to target any Unity plugin DLL with some renaming.


The proxy sits in the game's plugins folder with the same name as the original
DLL. It:

1. **Forwards every export** to the original dll via pure linker forwarders
2. **Boots BepInEx** from its own thread once `GameAssembly.dll` is loaded
   (`main.cpp` -> `BootBepInExThread`): hosts `dotnet\coreclr.dll`, builds a TPA
   list from `dotnet` + `BepInEx\core`, sets the `DOORSTOP_*` env vars, and
   invokes `BepInEx.Unity.IL2CPP`'s `Doorstop.Entrypoint.Start()`.

## Build

    ./build.sh

Output: the proxy DLL. `gen_def.py` regenerates `proxy.def` from `dump.txt`.

To generate `dump.txt` from your target DLL:

    objdump -p foo.dll > dump.txt

## Install

The game tree must already contain the original DLL renamed to
`OriginalDllName_orig.dll` and the new mini-doorstop dll named to the original name.


Env knobs:

    RUNTIME_SRC=<dir>  source of dotnet/ + BepInEx/   (default ../runtime)
    SETUP_NO_BUILD=1   use the already-built proxy
