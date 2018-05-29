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

set_targetdir '$(buildir)/$(mode)/$(arch)'
set_objectdir '$(buildir)/.objs/$(mode)/$(arch)'
add_linkdirs '$(buildir)/$(mode)/$(arch)'
add_linkdirs 'lib/$(arch)'
add_includedirs 'deps'
add_includedirs 'deps/capstone'

if is_mode 'debug' then
    add_cxxflags '/MDd'
else
    add_cxxflags '/MD'
end

-- add targets
add_subdirs 'deps/lua53'
add_subdirs 'deps/xval'
add_subdirs 'deps/nrpc'
add_subdirs 'src'

target 'test'
    set_kind 'binary'

    add_includedirs('src')
    add_files('test/*.cpp')
