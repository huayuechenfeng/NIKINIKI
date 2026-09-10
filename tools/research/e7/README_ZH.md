# E7 离线分析工具

本目录只保存可独立运行的主机离线分析器及其单元测试。工具只读取调用者显式提供的
ROM、DLL、code image 或 JSON/JSONL 文件；这些私有输入不属于仓库，也没有默认本机路径。

设备连接、CODA transport、进程启动/终止、观察器和设备绑定验证脚本已冻结在本地忽略归档
`.tmp/e7-private-tools-20260910/`。不要从本目录重建或继续设备实验。

运行全部主机测试：

```powershell
python -m unittest discover -s tools/research/e7 -p 'test_*.py'
```

每个分析器均支持 `--help`。没有输入样本时，`--help` 与单元测试仍可运行；需要样本的集成断言
会明确跳过。分析输出只能证明输入身份和离线映射，不能替代 GCCE 编译、SIS 打包或真机结果。
