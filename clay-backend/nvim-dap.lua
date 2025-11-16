-- _G.build_native_debug_config("c", 'make test debug', '/out/tests/test_runner')
-- _G.build_native_debug_config("c", 'make macosdll debug', '/out/macos/app')
-- _G.build_native_debug_config("c", 'make windowsdll debug', './out/windows/app.exe')
_G.build_native_debug_config("c", 'make build', '/out/meta/meta')

vim.keymap.set('n', '<Leader>rr', function()
    -- Run the game debug build command and capture output
    -- local result = vim.fn.system("./hz_build.exe windows --dll --hotreload")
    local result = vim.fn.system("make build")

    -- Notify the user based on command execution success or failure
    if vim.v.shell_error ~= 0 then
        vim.api.nvim_echo({
            { "Build game debug command failed!", "ErrorMsg" },
            { result,                             "ErrorMsg" }
        }, true, {})
    end
end)
