local dap = require('dap')

dap.configurations.c = {
    {
        name = "Launch",
        type = "codelldb",
        request = "launch",
        program = function()
            local build_cmd = "make build"
            vim.notify("Running " .. build_cmd, vim.log.levels.INFO)

            local build_output = vim.fn.system(build_cmd)

            if vim.v.shell_error == 0 then
                vim.notify("Build succeeded", vim.log.levels.INFO)
                vim.notify(vim.fn.getcwd() .. '/bin/main')
                return vim.fn.getcwd() .. '/bin/main'
            else
                vim.notify("Build failed: " .. build_output, vim.log.levels.ERROR)
                return ""
            end
        end,
        cwd = "${workspaceFolder}",
        stopOnEntry = false,
        args = {}
    },
    -- {
    --     name = "Test",
    --     type = "codelldb",
    --     request = "launch",
    --     program = function()
    --         local build_cmd = "make test-build"
    --         vim.notify("Running " .. build_cmd, vim.log.levels.INFO)
    --
    --         local build_output = vim.fn.system(build_cmd)
    --
    --         if vim.v.shell_error == 0 then
    --             vim.notify("Build succeeded", vim.log.levels.INFO)
    --             vim.notify(vim.fn.getcwd() .. '/bin/final')
    --             return vim.fn.getcwd() .. '/test/bin/test_app'
    --         else
    --             vim.notify("Build failed: " .. build_output, vim.log.levels.ERROR)
    --             return ""
    --         end
    --     end,
    --     cwd = "${workspaceFolder}",
    --     stopOnEntry = false,
    --     args = {}
    -- }
}
