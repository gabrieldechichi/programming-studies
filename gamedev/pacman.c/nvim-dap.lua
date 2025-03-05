_G.build_native_debug_config("c", 'make build-debug', '/build/pacman')


vim.keymap.set('n', '<Leader>rr', function() 
    vim.fn.system("make build-game-debug")
end)
