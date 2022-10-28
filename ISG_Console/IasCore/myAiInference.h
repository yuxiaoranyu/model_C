#ifndef MYAIINFERENCE_H
#define MYAIINFERENCE_H

#include "IasBase/BaseAiInference.h"
#include "IasBase/BaseAIResult.h"
#include <QList>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

class MyAiInference : public BaseAiInference
{
public:
    MyAiInference() {};
    ~MyAiInference() {};

public:
/*
    void addCameraId(int cameraId) override
    {
        cameraIdList.append(cameraId);
    }
*/
    bool load(const std::string &param) override { return true; }
    bool update(const std::string &param) override { return true; }
    std::vector<BaseAIResult> infer(const std::vector<int>& ids, const std::vector<cv::Mat>& images) override
    {
        BaseAIResult my;
        std::vector<BaseAIResult> myVector;

        my.setResult(false);

        QJsonObject jsonObj1;
        QJsonObject jsonObj2;

        jsonObj1.insert("x", 10);
        jsonObj1.insert("y", 12);
        jsonObj2.insert("kick_coordinate", jsonObj1);
        QString isgInfoString = QJsonDocument(jsonObj2).toJson();
        my.setIsgInfo(isgInfoString.toStdString());

        for (int i = 0; i < ids.size(); ++i)
        {
            myVector.push_back(my);
        }
        return myVector;
    }
    bool remove() override { return true; }
private:
//    QList<int> cameraIdList;
};

class MyAiInferenceFactory : public BaseAiInferenceFactory
{
public:
    BaseAiInference *getAiInference() override
    {
        return new MyAiInference();
    }
};

#endif // MYAIINFERENCE_H
