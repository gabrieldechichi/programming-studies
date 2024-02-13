Write-Host "CMake Build"

cmake --build ./build --config Debug

if ($?) {
    Write-Host "Run"
    ./build/Debug/wolfestein.exe
}
else {
    Write-Error "Build failed. Fix compile errors before running the project."
}