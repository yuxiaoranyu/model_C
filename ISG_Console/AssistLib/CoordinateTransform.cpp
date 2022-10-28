#include "CoordinateTransform.h"
#include "Matrix.h"
#include <QDebug>

CoordinateTransform::CoordinateTransform():
    m_bTransform(false)
{
}

//添加图像坐标、机器人坐标到坐标转换对象
void CoordinateTransform::setData(const QVector<float> &imgX, const QVector<float> &imgY, const QVector<float> &robotX, const QVector<float> &robotY)
{
    Q_ASSERT((imgX.size() == imgY.size()) && (imgY.size() == robotX.size()) && (robotX.size() == robotY.size()));

    m_coordinateArray[0] = imgX;
    m_coordinateArray[1] = imgY;
    m_coordinateArray[2] = robotX;
    m_coordinateArray[3] = robotY;

    m_bTransform = false;
}

//根据图像坐标、机器人坐标，生成坐标转换矩阵
bool CoordinateTransform::generate()
{
    int count = m_coordinateArray[0].size();
    if (count < 3) {
        qInfo() << "CoordinateTransform: generateCoordinateTransformMatrix, the point number is less 3!";
        return false;
    }

    float* imgX = m_coordinateArray[0].data();
    float* imgY = m_coordinateArray[1].data();
    float* robotX = m_coordinateArray[2].data();
    float* robotY = m_coordinateArray[3].data();
    float meanErr;

    bool result = solveTransMatrix(imgX, imgY, robotX, robotY, count, m_transMatrix, &meanErr);
    if (!result || (meanErr < 0)) {
        qInfo() << "CoordinateTransform: generateCoordinateTransformMatrix, result is ERROR!";
        return false;
    }

    m_bTransform = true;
    return true;
}

//输入图像坐标，输出图像坐标对应的机器人坐标
bool CoordinateTransform::transform(int pointNum, float *srcX, float *srcY, float *dstX, float *dstY)
{
    for (int i = 0; i < pointNum; i++) {
        dstX[i] = srcX[i];
        dstY[i] = srcY[i];
    }

    if (!m_bTransform) {
        qInfo() << "CoordinateTransform: Have not generated transform matrix!";
        return false;
    }

    if (pointNum > 100) {
        qInfo() << "CoordinateTransform: Point number exceeds 100!";
        return false;
    }

    for (int i = 0; i < pointNum; i++) {
        dstX[i] = m_transMatrix[0] * srcX[i] + m_transMatrix[1] * srcY[i] + m_transMatrix[2];
        dstY[i] = m_transMatrix[3] * srcX[i] + m_transMatrix[4] * srcY[i] + m_transMatrix[5];
    }
    return true;
}

//重置图像坐标、机器人坐标映射表
void CoordinateTransform::reset()
{
    for (int i = 0; i < 4; i++) {
        m_coordinateArray[i].clear();
    }
    memset(m_transMatrix, 0, sizeof(m_transMatrix));
    m_bTransform = false;
}



