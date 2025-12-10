local is_windows = vim.fn.has('win32') == 1
local is_mac = vim.fn.has('mac') == 1

if is_windows then
    _G.build_native_debug_config("c", './hzbuild.exe windows', './out/windows/app.exe')
elseif is_mac then
    _G.build_native_debug_config("c", './hzbuild macos', '/out/macos/app')
end

vim.keymap.set('n', '<Leader>rr', function()
    -- Run the game debug build command and capture output
    -- local result = vim.fn.system("./hz_build.exe windows --dll --hotreload")
    local result = vim.fn.system("make")

    -- Notify the user based on command execution success or failure
    if vim.v.shell_error ~= 0 then
        vim.api.nvim_echo({
            { "Build game debug command failed!", "ErrorMsg" },
            { result,                             "ErrorMsg" }
        }, true, {})
    else
        vim.api.nvim_echo({
            { "Build succeded"}
        }, true, {})
    end
end)
