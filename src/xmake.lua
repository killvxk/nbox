
-- Assembly lib for hook
target 'fasmlib'
    on_build(function(target)
        local X = val('arch') == 'x86' and '32' or '64'
        os.execv('fasm', {
            'src/detour' .. X .. '.fasm',
            'lib/' .. val('arch') .. '/hook-handler.lib'
        })
    end)

-- Functions Library
target 'nbox'
    set_kind 'static'

    add_links('capstone_static', 'hook-handler', 'nrpc', 'xval', 'lua53')
    add_files('nbox*.cpp')

-- Dynamic library injected to debuggee
target 'nspy'
    set_kind 'shared'

    add_files('nspy*.cpp', 'luaapi.cpp')
    add_links('nrpc', 'xval', 'nbox', 'lua53')

    add_deps('nrpc', 'nbox', 'lua53')
