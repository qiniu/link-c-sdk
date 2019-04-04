## demo介绍

### 概要
- demo目录下是一个使用sdk上传ts切片的例子，通过读取raw 264和aac文件拿到一帧帧的264和aac数据，模拟ipc，并使用切片上传的sdk，打包成ts后上传到云存储
- 运行程序之前需要先将demo/ipc.conf下的文件写入正确的配置，然后拷贝到/tmp/ipc.conf目录下，程序才能够正常执行，ipc.conf有几个选项是必须要设置的
- 关于ipc.conf如何进行配置详细见文件demo/ipc.conf，里面对每一个选项有详细的介绍

### 运行时参数
- 通过设置TSUPLOADER_SDK_LOG_OUTPUT环境变量确定tsuploader sdk日志的输出方式
    - **local** 本地日志输出
    - **mqtt** 通过mqtt输出日志

### 代码说明
- main.c - 程序入口，初始化ts切片sdk，初始化模拟的ipc设备
- dev_core.c - 对于ipc的一个抽象层，提供通用的接口，来屏蔽ipc的细节。一个新的ipc平台如果想跑起来，只需要实现它的接口就可以。
- sim_dev.c - 通过raw 264和aac文件模拟出一个ipc
- cfg.c - 配置文件的解析
- dbg.c - 一些调试函数和日志输出，主要的日志输出方式有网络，文件，mqtt，控制台
- socket_logging.c - 通过网络去输出日志
- queue.c - 实现的一个queue，日志的异步和视音频数据的缓存会用到
- log2file.c - 日志重定向到文件
- stream.c - 处理视音频流

### 如何让demo在自己的平台上跑起来
- 新建一个设备文件，比如海思的摄像头，hisi_ipc.c
- 参照sim_dev.c实现CaptureDevice这个结构体里面的成员
- 在dev_config.h注册自己的设备
- CMakeLists.txt加入hisi_ipc.c

### CaptureDevice结构体说明
- audioType - 音频类型，有G711和AAC两种
- audioEnable - audio是否使能
- subStreamEnable - 是否使能子码流
- mainContext - 用来标识主码流的id
- subContext - 用来标识子码流的id
- audioCb - audio的callback
- videoCb - video的callback
- init - ipc的初始化
- deInit - 运行结束时需要释放资源在这个函数里面执行
- getDevId - 获取设备id，每个设备都需要有唯一的一个id，回放时根据设备id来回放
- startStream - ipc开始流的传输(即video和audio的callback开始调用)
- isAudioEnable - 获取音频是否使能
- registerAlarmCb - 注册事件通知函数，比如移动侦测
- alarmCallback - 事件通知的回调
- captureJpeg - 截图函数

