-- _G.build_native_debug_config("c", 'make test debug', '/out/tests/test_runner')
_G.build_native_debug_config("c", 'make macos', '/out/macos/app')

vim.keymap.set('n', '<Leader>rr', function()
    -- Run the game debug build command and capture output
    local result = vim.fn.system("bun run build-game-debug")

    -- Notify the user based on command execution success or failure
    if vim.v.shell_error ~= 0 then
        vim.api.nvim_echo({
            { "Build game debug command failed!", "ErrorMsg" },
            { result,                             "ErrorMsg" }
        }, true, {})
    end
end)
