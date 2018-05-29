
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

local function onMsgBox(addr)
    printx('ESP', ESP)
    local pCaption = readPtr(ESP + 0xC)
    printx('pCaption', pCaption)
    local caption = readString(pCaption)
    printx('caption', caption)
end

print('---------start---------')

printx('MessageBoxA', getAddress 'USER32!MessageBoxA')
local CreateFile = getAddress('CreateFileA')
printx('CreateFile', CreateFile, type(CreateFile))

print('setHook', setHook('USER32!MessageBoxA', onMsgBox))
TEST()

print('----------------------')
print('removeHook', removeHook('USER32!MessageBoxA'))
TEST()

print('----------------------')
print('addBreakpoint', addBreakpoint('USER32!MessageBoxA', onMsgBox))
TEST()

print('----------------------')
print('removeBreakpoint', removeBreakpoint('USER32!MessageBoxA'))
TEST()
