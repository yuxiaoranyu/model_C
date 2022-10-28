QT += core gui network serialport serialbus widgets concurrent

CONFIG += c++14 console
CONFIG -= app_bundle
CONFIG +=warn_off
# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS QT_MESSAGELOGCONTEXT

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG(debug, debug|release){
    DEFINES -= QT_NO_DEBUG_OUTPUT \
               QT_NO_WARNING_OUTPUT
}
else{
    DEFINES += QT_NO_DEBUG_OUTPUT \
               QT_NO_WARNING_OUTPUT
}

win32 {
#    DEFINES += WIN32_LEAN_AND_MEAN
#    LIBS += -L$$PWD/ThirdParty/DHDriver/lib/x64/vs2013shared/ -lMVSDKmd
#    LIBS += -L$$PWD/ThirdParty/DHDriver/lib/x64/vs2013shared/ -lImageConvert
#    LIBS += -lWS2_32
#    LIBS += -lwinmm
#    INCLUDEPATH += $$PWD/ThirdParty/DHDriver/inc

    DEFINES += USING_CAMERA_BASLER_6_2
    LIBS += -L$$PWD/ThirdParty/Basler/lib/x64/ -lGCBase_MD_VC141_v3_1_Basler_pylon
    LIBS += -L$$PWD/ThirdParty/Basler/lib/x64/ -lGenApi_MD_VC141_v3_1_Basler_pylon
    LIBS += -L$$PWD/ThirdParty/Basler/lib/x64/ -lPylonBase_v6_3
    LIBS += -L$$PWD/ThirdParty/Basler/lib/x64/ -lPylonC
    LIBS += -L$$PWD/ThirdParty/Basler/lib/x64/ -lPylonGUI_v6_3
    LIBS += -L$$PWD/ThirdParty/Basler/lib/x64/ -lPylonUtility_v6_3

    INCLUDEPATH += $$PWD/ThirdParty/Basler/include

    HEADERS += \
#        ImgSrc/DahuaCamera.h \
        ImgSrc/BaslerCCDCamera.h

    SOURCES += \
#        ImgSrc/DahuaCamera.cpp \
        ImgSrc/BaslerCCDCamera.cpp

    INCLUDEPATH += $$PWD/ThirdParty
    LIBS += -L$$PWD/ThirdParty/opencv2_lib/x64/vc15/lib/ -lopencv_world460
}

unix {
#opencv
INCLUDEPATH += /usr/local/include/opencv4
LIBS += -L/usr/local/lib -lopencv_core -lopencv_imgcodecs -lopencv_imgproc -lopencv_ximgproc \
                 -lopencv_video -lopencv_calib3d -lopencv_dnn -lopencv_features2d \
                 -lopencv_flann -lopencv_barcode

### tensorRT
TENSORRT_PATH = /home/ai/build_group/TensorRT/TensorRT-7.1.3.4
INCLUDEPATH += $${TENSORRT_PATH}/include/
LIBS +=-L$${TENSORRT_PATH}/lib \
    -lnvcaffe_parser \
    -lnvinfer_plugin \
    -lnvparsers \
    -lnvinfer

#CUDA
message("PROJECT_DIR: "$$QMAKE_FILE_OUT)
CUDA_DIR = /usr/local/cuda
INCLUDEPATH  += $$CUDA_DIR/include
OBJECTS_DIR  = $$OUT_PWD/obj
QMAKE_LIBDIR += $$CUDA_DIR/lib64
CUDA_SOURCES  += \
                    IasCore/yolo/yololayer.cu \
                    IasCore/yolo/preprocess.cu

LIBS += -lcudart -lcuda
SYSTEM_NAME = linux
SYSTEM_TYPE = 64
CUDA_ARCH = compute_35
CUDA_CODE = sm_35
CUDA_INC = $$join(INCLUDEPATH,'" -I"','-I"','"')
cuda.input = CUDA_SOURCES
cuda.output = $$OBJECTS_DIR/${QMAKE_FILE_BASE}_cuda.o
NVCC_OPTIONS = --use_fast_math -Xcompiler -fPIC
NVCC_LIBS = $$LIBS
cuda.commands = $$CUDA_DIR/bin/nvcc $$NVCC_OPTIONS $$CUDA_INC $$NVCC_LIBS --machine $$SYSTEM_TYPE -arch=$$CUDA_ARCH -O3 -c -o ${QMAKE_FILE_OUT} ${QMAKE_FILE_NAME}
        QMAKE_EXTRA_COMPILERS += cuda

### libtorch
message("AI_WENCAI:libtorch")
LIBTORCH_PATH = /home/ai/build_group/LibTorch/libtorch
INCLUDEPATH += $${LIBTORCH_PATH}/include/ \
        $${LIBTORCH_PATH}/include/torch/csrc/api/include/
LIBS += -L$${LIBTORCH_PATH}/lib -lc10_cuda -lc10 -ltorch_cpu -ltorch_cuda -ltorch -ltorch_global_deps -lonnx -lonnx_proto

### ONNX
#message("AI_WENCAI:ONNX")
#ONNXRUNTIME_PATH = /home/ai/build_group/OnnxRunTime/onnxruntime-linux-x64-gpu-1.6.0
#INCLUDEPATH += $${ONNXRUNTIME_PATH}/include
#LIBS += -L$${ONNXRUNTIME_PATH}/lib -lonnxruntime
}

unix:contains(DEFINES, USING_CAMERA_BASLER):{
    LIBS += -L/opt/pylon5/lib64 -lpylonbase
    LIBS += -L/opt/pylon5/lib64 -lpylonutility
    LIBS += -L/opt/pylon5/lib64 -lGCBase_gcc_v3_0_Basler_pylon_v5_0
    INCLUDEPATH += /opt/pylon5/include

    HEADERS += ImgSrc/BaslerCCDCamera.h
    SOURCES += ImgSrc/BaslerCCDCamera.cpp
}

unix:contains(DEFINES, USING_CAMERA_BASLER_6_0):{
    INCLUDEPATH += /opt/pylon6/include

    QMAKE_CXXFLAGS+=$(shell  /opt/pylon6/bin/pylon-config --cflags)
    QMAKE_LFLAGS+=$(shell  /opt/pylon6/bin/pylon-config --libs-rpath)
    LIBS +=$(shell  /opt/pylon6/bin/pylon-config --libs)

    HEADERS += ImgSrc/BaslerCCDCamera.h
    SOURCES += ImgSrc/BaslerCCDCamera.cpp
}

unix:contains(DEFINES, USING_CAMERA_BASLER_6_2):{
    INCLUDEPATH += /opt/pylon6.2/include

    QMAKE_CXXFLAGS+=$(shell  /opt/pylon6.2/bin/pylon-config --cflags)
    QMAKE_LFLAGS+=$(shell  /opt/pylon6.2/bin/pylon-config --libs-rpath)
    LIBS +=$(shell  /opt/pylon6.2/bin/pylon-config --libs)

    HEADERS += ImgSrc/BaslerCCDCamera.h
    SOURCES += ImgSrc/BaslerCCDCamera.cpp
}

unix:contains(DEFINES, USING_CAMERA_DAHUA):{
    LIBS += -L/opt/HuarayTech/MVviewer/lib -lMVSDK
    LIBS += -L/opt/HuarayTech/MVviewer/lib -lImageConvert
    INCLUDEPATH += /opt/HuarayTech/MVviewer/include

    HEADERS += ImgSrc/DahuaCamera.h
    SOURCES += ImgSrc/DahuaCamera.cpp
}

unix {
    LIBS += -L$$PWD/ThirdParty/license -lLicenseDevice
    INCLUDEPATH += $$PWD/ThirdParty/license
}

unix {
#    LIBS += -L/home/ai/Workspace/yanglikun/iasCoreV3/build/Generate/IasInfer -lDefaultLocator
#    LIBS += -L/home/ai/Workspace/yanglikun/iasCoreV3/build/Generate/IasInfer -lColorDetector

#    LIBS += -L$$PWD/ThirdParty/IasInfer -lDefaultLocator
#    LIBS += -L$$PWD/ThirdParty/IasInfer -lColorDetector
#    QMAKE_LFLAGS += "-Wl,-rpath,\'\$$ORIGIN/IasInfer\'"
}

#unix {
#INCLUDEPATH += /usr/local/include/opencv4
#LIBS += -L/usr/local/lib -lopencv_core
#}

include(ThirdParty/ThirdParty.Pri)

HEADERS += \
    AssistLib/AuxTask.h \
    AssistLib/CoordinateTransform.h \
    AssistLib/Matrix.h \
    AssistLib/Tea.h \
    BaseLib/Attribute.h \
    BaseLib/EngineObject.h \
    BaseLib/EngineProject.h \
    BaseLib/Protocol.h \
    BaseLib/ProtocolStream.h \
    IOCtrl/ModbusCtrl.h \
    IOCtrl/PCPSerialCtrl.h \
    IOCtrl/PCPUdpCtrl.h \
    IOCtrl/RobotCtrl.h \
    IOCtrl/PLCUdpCtrl.h \
    IasAgent/AICtrl.h \
    IasBase/BaseAIResult.h \
    IasBase/BaseAiInference.h \
    IasBase/BaseDTResult.h \
    IasBase/BaseDetector.h \
    IasBase/BaseLocator.h \
    IasCore/common/IasCommonDefine.h \
    IasCore/common/IasJson.h \
    IasCore/myAiInference.h \
    IasCore/myDetector.h \
    IasCore/myLocator.h \
    IasCore/patchcore/PatchCorePt.h \
    IasCore/patchcore/PatchCore.h \
    IasCore/patchcore/corepatchcore.h \
    IasCore/tensorrt/logging.h \
    IasCore/tensorrt/TensorRt.h \
    IasCore/resnet/ResNet.h \
    IasCore/resnet/ResNetTensorRt.h \
    IasCore/resnet/coreresnet.h \
    IasCore/yolo/Yolov5TensorRt.h \
    IasCore/yolo/coreyolo.h \
    IasCore/yolo/cuda_utils.h \
    IasCore/yolo/logging.h \
    IasCore/yolo/macros.h \
    IasCore/yolo/preprocess.h \
    IasCore/yolo/utils.h \
    IasCore/yolo/yololayer.h \
    IasCore/yolo/Yolov5.h \
    IasCore/yolo/Yolov5TensorRt.h \
    IasCore/yoloresnet/YoloResNetInference.h \
    IasAgent/batchInfer.h \
    IasAgent/channelInfer.h \
    IasAgent/iasInfer.h \
    IasAgent/iasJsonParser.h \
    IasAgent/ias_type.h \
    IasAgent/imageGroup.h \
    ImgSrc/ImageSrcDev.h \
    ImgSrc/SoftCamera.h \
    Inspector/Infer.h \
    IopAgent/RealAlarm.h \
    IopAgent/TcpClient.h \
    IopAgent/TcpImageReport.h \
    Multipler/ImageMultipler.h \
    Multipler/AbstractObject.h \
    Multipler/PacketResult.h \
    Multipler/BoxResult.h \
    Sensor/BarCoder.h

SOURCES += \
    AssistLib/AuxTask.cpp \
    AssistLib/CoordinateTransform.cpp \
    AssistLib/Matrix.cpp \
    AssistLib/Tea.cpp \
    BaseLib/Attribute.cpp \
    BaseLib/EngineObject.cpp \
    BaseLib/EngineProject.cpp \
    BaseLib/ProtocolStream.cpp \
    IOCtrl/ModbusCtrl.cpp \
    IOCtrl/PCPSerialCtrl.cpp \
    IOCtrl/PCPUdpCtrl.cpp \
    IOCtrl/RobotCtrl.cpp \
    IOCtrl/PLCUdpCtrl.cpp \
    IasAgent/AICtrl.cpp \
    IasAgent/batchInfer.cpp \
    IasAgent/channelInfer.cpp \
    IasAgent/iasJsonParser.cpp \
    IasAgent/imageGroup.cpp \
    IasCore/common/IasJson.cpp \
    IasCore/patchcore/PatchCore.cpp \
    IasCore/patchcore/PatchCorePt.cpp \
    IasCore/tensorrt/TensorRt.cpp \
    IasCore/resnet/ResNet.cpp \
    IasCore/resnet/ResNetTensorRt.cpp \
    IasCore/yolo/Yolov5.cpp \
    IasCore/yolo/Yolov5TensorRt.cpp \
    IasCore/yoloresnet/YoloResNetInference.cpp \
    ImgSrc/ImageSrcDev.cpp \
    ImgSrc/SoftCamera.cpp \
    IopAgent/RealAlarm.cpp \
    IopAgent/TcpClient.cpp \
    IopAgent/TcpImageReport.cpp \
    Multipler/ImageMultipler.cpp \
    Multipler/AbstractObject.cpp \
    Multipler/PacketResult.cpp \
    Multipler/BoxResult.cpp \
    Sensor/BarCoder.cpp \
    main.cpp

unix {
    HEADERS += IOCtrl/SiemensS7PLC.h
    SOURCES += IOCtrl/SiemensS7PLC.cpp
}

RESOURCES += \
    Resource/ISG_Console_res.qrc

TRANSLATIONS += \
    Resource/ISG_Console_zh_CN.ts

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target


