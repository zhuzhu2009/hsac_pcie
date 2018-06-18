去除桌面水印通用补丁（支持Vista SP1 SP2/Server 2008/Windows 7, 32/64位）

软件名称： Remove Watermark (去除桌面水印通用补丁)
作者邮箱： deepxw#126.com
Blog:     http://deepxw.lingd.net
操作系统： Windows Vista SP1 SP2 /Server 2008 /Windows 7
          All 32bit(x86) / 64bit(x64), All Service Pack.

去除桌面水印通用补丁，包括“测试模式”、“评估副本”、版本号等所有水印。

通用补丁，适合所有语言的Windows！
特性码查找，能支持各种SP版本的Windows、甚至未来的SPx。

本程序提供两种方法去除水印。
* 默认方法，修改 user32.dll.mui。这是一个安全的方法。
  在64位Vista / Windows 7 下，需要重建 MUI 缓存, 可能需要几分钟，请耐心等待。

* 方法二，修改 user32.dll。100% 去除所有水印。 (以参数 "-enforce" 启动程序。)

  请不要在Windows 7 6956 及以后版本中使用方法二！
  在Windows 7 6956 及以后版本中，存在未知的缓存bug，任何对文件user32.dll 的改动，都会造成Windows 7无法正常使用兼容模式。
  Windows 7 Server / Vista 没有这个问题。

说明：
1, 可以在正常模式下操作。

2, 请根据你的系统运行对应的程序：
   32位系统请运行RemoveWatermarkX86.exe；
   64位系统必须运行RemoveWatermarkX64.exe。

3, 需要管理员权限，UAC关闭。右键点程序，选择以管理员身份运行。

4, 如果提示成功，需要重启生效。

5, 程序有校验，对不支持的系统，不会乱改。

6, 万一失败，请到\windows\system32\zh-CN， 将user32.dll.mui.backup恢复为user32.dll.mui
   zh-CN视系统语言而定，可能是en-US, zh-TW等。

7, 带参数 "-silent" 运行程序，将不提示信息，静默运行，修改完成后自动退出。


更新历史：
2008.11.04 V0.2
  + 支持Windows 7 M3 6801 x64.

2008.12.10 V0.3
  + 支持带参数运行静默安装： -silent
  * 恢复UAC状态
  (测试在Windows 7 6956 上可以正常使用。）


2009.01.17 V0.4
   + 支持修改 user32.dll.mui 去除水印字符串。
   * 默认修改方法，从修改user32.dll 更换为修改user32.dll.mui。绕过Windows 7 的bug，保证兼容性。

   Test in Windows 7 6.1.7000.0, OK.

其他作品：
TCP-Z,  TCP半开连接数破解全能完美补丁（修改内存方式，支持XP SP2 SP3/Vista SP1 SP2/Win 7, 32/64位）
Windows主题破解通用补丁（XP/2003/2008/Vista/Windows 7, 32/64位）