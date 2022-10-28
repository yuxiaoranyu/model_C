#ifndef COORDINATETRANSFORM_H
#define COORDINATETRANSFORM_H

#include <QVector>

//坐标转换对象
class CoordinateTransform
{
public:
    CoordinateTransform();

    void setData(const QVector<float> &imgX, const QVector<float> &imgY, const QVector<float> &robotX, const QVector<float> &robotY); //添加图像坐标、机器人坐标到坐标转换对象
    bool generate(); //根据图像坐标、机器人坐标，生成坐标转换矩阵
    bool transform(int pointNum, float *srcX, float *srcY, float *dstX, float *dstY); //输入图像坐标，输出图像坐标对应的机器人坐标
    void reset(); //重置图像坐标、机器人坐标映射表

private:
    bool m_bTransform; //是否已经生成转换矩阵
    float m_transMatrix[9];  //坐标转换矩阵
    QVector<float> m_coordinateArray[4]; //图像坐标、机器人坐标映射表

};
#endif // COORDINATETRANSFORM_H
