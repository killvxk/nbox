
assert(systhread)

local print = LOG

function printx(...)
    local args = {...}
    for i = 1, #args do
        args[i] = type(args[i]) == 'number'
                    and string.format('%08X', args[i]) or args[i]
    end
    return print(table.unpack(args))
end

print('---------start---------')

printx('MessageBoxA', getAddress 'USER32!MessageBoxA')
local CreateFile = getAddress('CreateFileA')
printx('CreateFile', CreateFile, type(CreateFile))

printx('setHook', setHook('USER32!MessageBoxA', function(ct)
    printx('ESP', ct.ESP)
    local ppStr = readPointer(ct.ESP + 0xC)
    printx('ppStr', ppStr)
    local Arg2 = readString(ppStr)
    printx('Arg2', Arg2)
end))
TEST()

print('----------------------')
print('removeHook', removeHook('USER32!MessageBoxA'))
TEST()

print('----------------------')
print('addBreakpoint', addBreakpoint('USER32!MessageBoxA', function(addr)
    print('Breakpoint occured')
end))

print('----------------------')
removeBreakpoint('USER32!MessageBoxA')
TEST()
