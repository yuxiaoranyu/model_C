文件来源说明：
	大华相机编译环境来源
	win：
		安装包下载地址http://download.huaraytech.com/pub/sdk/Ver2.2.3/Windows/Base_ver/
		
		下载完成后，进行安装，
		从安装路径下\DahuaTech\MV Viewer\Development\Samples\QT\ShowQImage拷贝depends和include到本地
	linux：
		从http://download.huaraytech.com/pub/sdk/Ver2.2.3/Linux/x86/ 下载linux的大华驱动安装包
		在linux环境下使用root权限，执行"bash MVviewer_Ver2.2.3_Linux_x86_Build20200323.run" 命令进行相机驱动的安装
		安装完成后，使用root权限 执行 " ln -s libMVSDK.so.2.1.0.108758 libMVSDKmd.so " 生成编译所需要的文件

版本编译方法：
	win:
		在qt工程中添加的 INCLUDEPATH = ThirdParty/DHDriver/inc
		在qt工程中添加的 LIBS += -L$$PWD/ThirdParty/DHDriver/lib/x64/vs2013shared/ -lMVSDKmd
    		                 LIBS += -L$$PWD/ThirdParty/DHDriver/lib/x64/vs2013shared/ -lImageConvert
		在编译前 修改QT工程的编译套件为 MSVC 2017 的版本
		点击构建，即可成功编译带有大华相机的windows版程序
	linux:
		
		修改qt工程文件的 INCLUDEPATH = /opt/DahuaTech/MVviewer/include
		修改qt工程文件的 LIBS += -L/opt/DahuaTech/MVviewer/lib -lMVSDKmd
    				 LIBS += -L/opt/DahuaTech/MVviewer/lib -lImageConvert
		Qt的编译套件选择默认的gcc64 既可编译生成linux版的程序。

