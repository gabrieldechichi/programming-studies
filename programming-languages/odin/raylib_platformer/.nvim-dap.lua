local dap = require('dap')

dap.configurations.odin = {
    {
        name = "Launch",
        type = "codelldb",
        request = "launch",
        program = function()
            local build_cmd = 'odin build . -debug -out=bin/asteroids'
            local executable_path = '/bin/asteroids'
            vim.notify("Running " .. build_cmd, vim.log.levels.INFO)

            local build_output = vim.fn.system(build_cmd)

            if vim.v.shell_error == 0 then
                vim.notify(build_output, vim.log.levels.INFO)
                return vim.fn.getcwd() .. executable_path
            else
                vim.notify("Build failed. Check the output for errors.", vim.log.levels.ERROR)
                return ""
            end
        end,
        cwd = "${workspaceFolder}",
        stopOnEntry = false,
        args = {}
    }
}
