#include "GenJsonObject.h"

QJsonObject genPointJson(double x, double y)
{
    QJsonObject pointJson;

    pointJson.insert("x", x);
    pointJson.insert("y", y);

    return pointJson;
}

QJsonObject genDrawObjJson(QString objType, QVector<QJsonObject> pointJsonVector, QString colorStr)
{
    QJsonArray  pointArrayJson;
    QJsonObject drawObjJson;

    drawObjJson.insert("type", objType);
    drawObjJson.insert("color", colorStr);

    for (int i = 0; i < pointJsonVector.size(); i++)
    {
        pointArrayJson.append(pointJsonVector[i]);
    }

    drawObjJson.insert("points", pointArrayJson);

    return drawObjJson;
}

QJsonObject genDrawObjJson(int objType, QVector<QJsonObject> pointJsonVector, QString colorStr)
{
    QJsonArray  pointArrayJson;
    QJsonObject drawObjJson;

    drawObjJson.insert("type", objType);
    drawObjJson.insert("color", colorStr);

    for (int i = 0; i < pointJsonVector.size(); i++)
    {
        pointArrayJson.append(pointJsonVector[i]);
    }

    drawObjJson.insert("points", pointArrayJson);

    return drawObjJson;
}

QJsonObject genDrawJson(QJsonObject draw1, QJsonObject draw2)
{
    QJsonObject drawJson;

    if (draw1.empty())
    {
        drawJson.insert("draw", draw2);
    }
    else if (draw2.empty())
    {
        drawJson.insert("draw", draw1);
    }
    else
    {
        QJsonArray  arrayJson;

        arrayJson.append(draw1);
        arrayJson.append(draw2);
        drawJson.insert("draw", arrayJson);
    }

    return drawJson;
}
