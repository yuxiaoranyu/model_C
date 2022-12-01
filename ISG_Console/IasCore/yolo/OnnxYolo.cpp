#include "OnnxYolo.h"

OnnxYolo::OnnxYolo(const std::string &model_path, const bool use_gpu)
{
    Ort::Env _env{nullptr};
    Ort::SessionOptions _session_options{nullptr};

    _env = Ort::Env(OrtLoggingLevel::ORT_LOGGING_LEVEL_WARNING, "ONNX_DETECTION");

    _session_options = Ort::SessionOptions();
    _session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    std::vector<std::string> availableProviders = Ort::GetAvailableProviders();
    auto cudaAvailable = std::find(availableProviders.begin(), availableProviders.end(), "CUDAExecutionProvider");

    if (use_gpu && (cudaAvailable == availableProviders.end()))
    {
        std::cout << "CUDA is not supported by your ONNXRuntime build. Fallback to CPU." << std::endl;
        std::cout << "Inference device: CPU" << std::endl;
    }
    else if (use_gpu && (cudaAvailable != availableProviders.end()))
    {
        OrtCUDAProviderOptions cudaOption{ 0, OrtCudnnConvAlgoSearch::EXHAUSTIVE, std::numeric_limits<size_t>::max(), 0, true};

        std::cout << "Inference device: CUDA" << std::endl;
        _session_options.AppendExecutionProvider_CUDA(cudaOption);
    }
    else
    {
        std::cout << "Inference device: CPU" << std::endl;
    }

    m_ort_session = Ort::Session(_env, model_path.c_str(), _session_options);

    //输入输出节点数量和名称
    size_t num_input_nodes = m_ort_session.GetInputCount();
    size_t num_output_nodes = m_ort_session.GetOutputCount();

    for (size_t i = 0; i < num_input_nodes; i++)
    {
        Ort::AllocatorWithDefaultOptions _allocator;

        auto input_node_name = m_ort_session.GetInputName(i, _allocator);
        m_ort_input_names.push_back(input_node_name);

        Ort::TypeInfo type_info = m_ort_session.GetInputTypeInfo(i);
        auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
        std::vector<int64_t> input_node_dim = tensor_info.GetShape();
        if (i == 0) //仅支持一个维度输入
        {
            m_ort_input_tensor_shape = input_node_dim;
            int sum = 1;
            for (size_t j = 0; j < m_ort_input_tensor_shape.size(); j++)
            {
                sum = sum * m_ort_input_tensor_shape.at(j);
            }
            m_input_tensor_size = sum;
            m_resized_image_size = cv::Size2f(input_node_dim[3], input_node_dim[2]);
        }
    }

    for (size_t i = 0; i < num_output_nodes; i++)
    {
        Ort::AllocatorWithDefaultOptions _allocator;

        auto output_node_name = m_ort_session.GetOutputName(i, _allocator);
        m_ort_output_names.push_back(output_node_name);
    }
}

void OnnxYolo::preprocessing(const std::vector<cv::Mat> &images, float *blob, std::vector<cv::Size> &original_size)
{
    cv::Mat resizedImage, floatImage;

    for (size_t i = 0; i < images.size(); ++i)
    {
        original_size.push_back(images[i].size());
        letterbox(images[i], resizedImage, m_resized_image_size,
                  cv::Scalar(114, 114, 114), false,
                  false, true, 32);
        cv::cvtColor(resizedImage, resizedImage, cv::COLOR_BGR2RGB);
        resizedImage.convertTo(floatImage, CV_32FC3, 1 / 255.0);
        cv::Size floatImageSize{floatImage.cols, floatImage.rows};

        std::vector<cv::Mat> chw(floatImage.channels());
        for (int j = 0; j < floatImage.channels(); ++j)
        {
            chw[j] = cv::Mat(floatImageSize, CV_32FC1, blob + i * floatImage.channels() * floatImageSize.width * floatImageSize.height + j * floatImageSize.width * floatImageSize.height);
        }
        cv::split(floatImage, chw);
    }
}

void OnnxYolo::getBestClassInfo(std::vector<float>::iterator it, const int numClasses, float &bestConf, int &bestClassId)
{
    // first 5 element are box and obj confidence
    bestClassId = 0;
    bestConf = 0;

    for (int i = 5; i < numClasses + 5; i++)
    {
        if (it[i] > bestConf)
        {
            bestConf = it[i];
            bestClassId = i - 5;
        }
    }
}

std::vector<std::vector<YoloResult>> OnnxYolo::postprocessing(const std::vector<cv::Size> &original_size, std::vector<Ort::Value> &outputTensors)
{
    auto *rawOutput = outputTensors[0].GetTensorData<float>();
    std::vector<int64_t> outputShape = outputTensors[0].GetTensorTypeAndShapeInfo().GetShape();
    size_t count = outputTensors[0].GetTensorTypeAndShapeInfo().GetElementCount();
    std::vector<float> output(rawOutput, rawOutput + count);

    // first 5 elements are box[4] and obj confidence
    int numClasses = (int)outputShape[2] - 5;
    int elementsInBatch = (int)(outputShape[1] * outputShape[2]);

    std::vector<cv::Rect> boxes;  //区域
    std::vector<float> confs;     //置信度
    std::vector<int> classIds;    //分类ID

    for (auto it = output.begin(); it != output.begin() + elementsInBatch; it += outputShape[2])
    {
        float clsConf = it[4];

        if (clsConf > _conf_thres)
        {
            int centerX = (int)(it[0]);
            int centerY = (int)(it[1]);
            int width = (int)(it[2]);
            int height = (int)(it[3]);
            int left = centerX - width / 2;
            int top = centerY - height / 2;

            float objConf;
            int classId;
            this->getBestClassInfo(it, numClasses, objConf, classId);

            float confidence = clsConf * objConf;

            boxes.emplace_back(left, top, width, height);
            confs.emplace_back(confidence);
            classIds.emplace_back(classId);
        }
    }

    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, confs, _conf_thres, _iou_thres, indices);

    std::vector<YoloResult> d;

    for (int idx : indices)
    {
        YoloResult det;

        det.box = cv::Rect(boxes[idx]);
        scaleCoords(m_resized_image_size, det.box, original_size[0]);
        det.conf = confs[idx];

        det.classId = classIds[idx];
        d.emplace_back(det);
    }

    std::vector<std::vector<YoloResult>> result;

    result.push_back(d);
    return result;
}

bool OnnxYolo::infer(const std::vector<cv::Mat> &images, std::vector<std::vector<YoloResult>> &yoloResult)
{
    auto t1 = std::chrono::system_clock::now();
    std::vector<Ort::Value> inputTensors;

    float *blob = new float[m_input_tensor_size];
    std::vector<cv::Size> original_size;
    this->preprocessing(images, blob, original_size);

    std::vector<float> inputTensorValues(blob, blob + m_input_tensor_size);

    auto t2 = std::chrono::system_clock::now();
    Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);

    inputTensors.push_back(Ort::Value::CreateTensor<float>(
        memoryInfo,
        inputTensorValues.data(),
        m_input_tensor_size,
        m_ort_input_tensor_shape.data(),
        m_ort_input_tensor_shape.size()));

    // 执行推断
    std::vector<Ort::Value> outputTensors = m_ort_session.Run(Ort::RunOptions{nullptr},
                                                         m_ort_input_names.data(),
                                                         inputTensors.data(),
                                                         inputTensors.size(),
                                                         m_ort_output_names.data(),
                                                         1);
    auto t3 = std::chrono::system_clock::now();
    std::vector<std::vector<YoloResult>> res = this->postprocessing(original_size, outputTensors);
    auto t4 = std::chrono::system_clock::now();
    std::cout << "preprocess time: " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << "ms "
              << "gpu time: " << std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count() << "ms "
              << "postprocess time: " << std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count() << "ms "
              << std::endl;

    delete[] blob;
    yoloResult = res;
    return true;
}
