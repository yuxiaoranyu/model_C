#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>

QJsonObject genPointJson(double x, double y);
QJsonObject genDrawObjJson(QString objType, QVector<QJsonObject> pointJsonVector, QString colorStr);
QJsonObject genDrawObjJson(int objType, QVector<QJsonObject> pointJsonVector, QString colorStr);
QJsonObject genDrawJson(QJsonObject draw1, QJsonObject draw2);
