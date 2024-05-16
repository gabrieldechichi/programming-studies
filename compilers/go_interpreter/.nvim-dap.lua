local dap = require('dap')

local config = {
    delve = {
        path = "dlv",
        initialize_timeout_sec = 20,
        port = "${port}",
        args = {},
        build_flags = "",
        detached = true,
    },
}

local args = { "dap", "-l", "127.0.0.1:" .. config.delve.port }

dap.adapters.go = {
    type = "server",
    port = config.delve.port,
    executable = {
        command = config.delve.path,
        args = args,
        detached = config.delve.detached,
        cwd = config.delve.cwd,
    },
    options = {
        initialize_timeout_sec = config.delve.initialize_timeout_sec,
    },
}

dap.configurations.go = {
    -- {
    --     type = "go",
    --     name = "Debug",
    --     request = "launch",
    --     program = "${file}",
    --     buildFlags = configs.delve.build_flags,
    -- },
    -- {
    --     type = "go",
    --     name = "Debug (Arguments)",
    --     request = "launch",
    --     program = "${file}",
    --     args = get_arguments,
    --     buildFlags = configs.delve.build_flags,
    -- },
    -- {
    --     type = "go",
    --     name = "Debug Package",
    --     request = "launch",
    --     program = "${fileDirname}",
    --     buildFlags = configs.delve.build_flags,
    -- },
    -- {
    --     type = "go",
    --     name = "Attach",
    --     mode = "local",
    --     request = "attach",
    --     processId = filtered_pick_process,
    --     buildFlags = configs.delve.build_flags,
    -- },
    -- {
    --     type = "go",
    --     name = "Debug test",
    --     request = "launch",
    --     mode = "test",
    --     program = "${file}",
    --     buildFlags = configs.delve.build_flags,
    -- },
    {
        type = "go",
        name = "Debug test (go.mod)",
        request = "launch",
        mode = "test",
        program = "./${relativeFileDirname}",
        buildFlags = config.delve.build_flags,
    },
}

-- dap.configurations.c = {
--     {
--         name = "Launch",
--         type = "codelldb",
--         request = "launch",
--         program = function()
--             local build_cmd = "make"
--             vim.notify("Running " .. build_cmd, vim.log.levels.INFO)
--
--             local build_output = vim.fn.system(build_cmd)
--
--             if vim.v.shell_error == 0 then
--                 vim.notify("Build succeeded", vim.log.levels.INFO)
--                 vim.notify(vim.fn.getcwd() .. '/bin/final')
--                 return vim.fn.getcwd() .. '/bin/final'
--             else
--                 vim.notify("Build failed: " .. build_output, vim.log.levels.ERROR)
--                 return ""
--             end
--         end,
--         cwd = "${workspaceFolder}",
--         stopOnEntry = false,
--         args = { '-f', 'file.db', '-a', 'John,123,10' }
--     },
--     {
--         name = "Test",
--         type = "codelldb",
--         request = "launch",
--         program = function()
--             local build_cmd = "make test-build"
--             vim.notify("Running " .. build_cmd, vim.log.levels.INFO)
--
--             local build_output = vim.fn.system(build_cmd)
--
--             if vim.v.shell_error == 0 then
--                 vim.notify("Build succeeded", vim.log.levels.INFO)
--                 vim.notify(vim.fn.getcwd() .. '/bin/final')
--                 return vim.fn.getcwd() .. '/test/bin/test_app'
--             else
--                 vim.notify("Build failed: " .. build_output, vim.log.levels.ERROR)
--                 return ""
--             end
--         end,
--         cwd = "${workspaceFolder}",
--         stopOnEntry = false,
--         args = {}
--     }
-- }
