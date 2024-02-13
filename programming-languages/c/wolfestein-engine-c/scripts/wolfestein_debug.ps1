Write-Host "CMake Build"

cmake --build ./build --config Debug

if ($?) {
    Write-Host "Run"
    remedybg.exe open-session ./remedy/session.rdbg && remedybg.exe start-debugging
}
else {
    Write-Error "Build failed. Fix compile errors before running the project."
}
