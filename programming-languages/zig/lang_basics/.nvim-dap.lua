local codelldb_bin = require("mason-registry").get_package("codelldb"):get_install_path() ..
    "/extension/adapter/codelldb.exe"
local executable_path = "/zig-out/bin/lang_basics.exe"
local build_cmd = "zig build"

local dap = require("dap")

dap.adapters.codelldb = {
    type = 'server',
    port = "${port}",
    executable = {
        command = codelldb_bin,
        args = { "--port", "${port}" },
    }
}

dap.configurations.zig = {
    {
        name = "Launch",
        type = "codelldb",
        request = "launch",
        program = function()
            vim.notify("Running " .. build_cmd, vim.log.levels.INFO)

            local build_output = vim.fn.system(build_cmd)

            if vim.v.shell_error == 0 then
                vim.notify("Buld successful: " .. build_output, vim.log.levels.INFO)
                return vim.fn.getcwd() .. executable_path
            else
                vim.notify("Build failed. Check the output for errors.", vim.log.levels.ERROR)
                return ""
            end
        end,
        cwd = "${workspaceFolder}",
        stopOnEntry = false,
    }
}
