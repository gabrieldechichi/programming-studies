rm -r build
mkdir build
Push-Location "./build"
cmake .. -G "Visual Studio 17 2022" -A x64
Pop-Location
