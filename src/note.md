
## 连接建立流程

1. nspy连接到nvim，nvim通知ndbg
2. ndbg调用nspy的getDbgData方法，获取DbgData在nspy中的地址
3. DuplicateHandle，获取nspy中event，属于自己的Handle

## 断点触发的用户处理流程

1. 填写DbgData里的 record, context 字段
2. notify('ndbg#notify', 'onBreak')
3. 等待用户选择下一步动作 (wait event)
