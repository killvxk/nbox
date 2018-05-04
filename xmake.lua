set_project 'nbox'

-- the debug mode
if is_mode("debug") then
    -- enable the debug symbols
    set_symbols("debug")
    -- disable optimization
    set_optimize("none")
end

-- the release mode
if is_mode("release") then
    -- set the symbols visibility: hidden
    set_symbols("hidden")
    -- enable fastest optimization
    set_optimize("fastest")
    -- strip all symbols
    set_strip("all")
end

-- add target
target 'lua53'
    set_kind 'shared'
    add_files('src/lua53/*.c|lua.c|luac.c')
    add_defines('LUA_BUILD_AS_DLL')

target 'nbox'
    set_kind 'shared'
    set_targetdir '$(buildir)/$(mode)/$(arch)'
    add_files('src/*.cpp')
    add_includedirs('include')
    add_includedirs('src')
    add_linkdirs('lib/$(arch)')
    add_links('capstone_static.lib', 'hook-handler.obj')

target 'test'
    set_kind 'binary'
    set_targetdir '$(buildir)/$(mode)/$(arch)'
    add_files('src/*.cpp', 'test/*.cpp')

target 'fasmlib'
    on_build(function(target)
        local X = val('arch') == 'x86' and '32' or '64'
        os.execv('fasm', {
            'src/hook-handler' .. X .. '.fasm',
            'lib/' .. val('arch') .. '/hook-handler.obj'
        })
    end)

-- FAQ
--
-- You can enter the project directory firstly before building project.
--   
--   $ cd projectdir
-- 
-- 1. How to build project?
--   
--   $ xmake
--
-- 2. How to configure project?
--
--   $ xmake f -p [macosx|linux|iphoneos ..] -a [x86_64|i386|arm64 ..] -m [debug|release]
--
-- 3. Where is the build output directory?
--
--   The default output directory is `./build` and you can configure the output directory.
--
--   $ xmake f -o outputdir
--   $ xmake
--
-- 4. How to run and debug target after building project?
--
--   $ xmake run [targetname]
--   $ xmake run -d [targetname]
--
-- 5. How to install target to the system directory or other output directory?
--
--   $ xmake install 
--   $ xmake install -o installdir
--
-- 6. Add some frequently-used compilation flags in xmake.lua
--
-- @code 
--    -- add macro defination
--    add_defines("NDEBUG", "_GNU_SOURCE=1")
--
--    -- set warning all as error
--    set_warnings("all", "error")
--
--    -- set language: c99, c++11
--    set_languages("c99", "cxx11")
--
--    -- set optimization: none, faster, fastest, smallest 
--    set_optimize("fastest")
--    
--    -- add include search directories
--    add_includedirs("/usr/include", "/usr/local/include")
--
--    -- add link libraries and search directories
--    add_links("tbox", "z", "pthread")
--    add_linkdirs("/usr/local/lib", "/usr/lib")
--
--    -- add compilation and link flags
--    add_cxflags("-stdnolib", "-fno-strict-aliasing")
--    add_ldflags("-L/usr/local/lib", "-lpthread", {force = true})
--
-- @endcode
--
-- 7. If you want to known more usage about xmake, please see http://xmake.io/#/home
--
    
