//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#include "stdafx.h"
#include "Common.h"
#include <random>

// Due to inclusion of windows.h
#undef min
#undef max

#include "XorEvaluator.h"
#include "LstmSpeechEvaluator.h"

#include "CNTKLibrary.h"

namespace utf = boost::unit_test;

namespace pt = boost::property_tree;

namespace CNTK { namespace Test {

using Halide::ImageParam;

template<class T>
using Buffer = Halide::Buffer<T>;

BOOST_AUTO_TEST_SUITE(CodeGenDeserializationSuite)

BOOST_AUTO_TEST_CASE(XorOperation)
{
    // Halide
    XorEvaluator e;
    e.init("XorEvaluator.json");

    Halide::ImageParam features(Halide::type_of<float>(), 1);
    std::vector<float> v = { 0.f, 1.f };
    features.set(Halide::Buffer<float>(v.data(), 2));

    Halide::Buffer<float> result(1);

    e.Evaluate(features, result);
    auto result10 = result(0);

    v[0] = 1.f;
    e.Evaluate(features, result);
    auto result11 = result(0);

    // CNTK
    auto model = Function::Load(L"xor.model", DeviceDescriptor::CPUDevice());
    auto input = model->Arguments();
    auto output = model->Output();

    ValuePtr value = MakeSharedObject<Value>(
        MakeSharedObject<NDArrayView>(DataType::Float, NDShape({ 2 }), v.data(), sizeof(float) * 2, DeviceDescriptor::CPUDevice()));

    std::unordered_map<Variable, ValuePtr> i = { { input.front(), value } };
    std::unordered_map<Variable, ValuePtr> o = { { output, nullptr } };

    v[0] = 0.f;
    model->Forward(i, o, DeviceDescriptor::CPUDevice());
    auto result20 = *(o[output]->Data()->DataBuffer<float>());

    v[0] = 1.f;
    value = MakeSharedObject<Value>(
        MakeSharedObject<NDArrayView>(DataType::Float, NDShape({ 2 }), v.data(), sizeof(float) * 2, DeviceDescriptor::CPUDevice()));
    i = std::unordered_map<Variable, ValuePtr>({ { input.front(), value } });
    o = std::unordered_map<Variable, ValuePtr>({ { output, nullptr } });
    model->Forward(i, o, DeviceDescriptor::CPUDevice());
    auto result21 = *(o[output]->Data()->DataBuffer<float>());

    BOOST_REQUIRE_CLOSE(result10, result20, 0.1);
    BOOST_REQUIRE_CLOSE(result11, result21, 0.1);
}

BOOST_AUTO_TEST_CASE(SpeechLstmModel)
{
    const int InputDimension = 80;
    const int OutputDimension = 9404;

    // Halide
    LstmSpeechEvaluator e;
    e.init("LstmSpeechEvaluator.json");

    // CNTK
    auto model = Function::Load(L"lstm_speech.model", DeviceDescriptor::CPUDevice());
    auto input = model->Arguments();
    auto output = model->Output();
    auto inputShape = NDShape({ (size_t)InputDimension });

    // Input
    const int NumberOfFrames = 80;
    std::vector<std::vector<float>> frames;
    frames.resize(NumberOfFrames, std::vector<float>(InputDimension));

    {
        for (int i = 0; i < NumberOfFrames; ++i)
            for (size_t j = 0; j < frames[i].size(); ++j)
                frames[i][j] = (rand() % 256) / (float)256;
    }

    Halide::ImageParam features(Halide::type_of<float>(), 1);

    auto evaluateFrame = [&](int timestamp, std::vector<float>& f)
    {
        features.set(Halide::Buffer<float>(f.data(), InputDimension));

        Halide::Buffer<float> result(OutputDimension);
        e.Evaluate(timestamp, features, result);

        // CNTK evaluation
        auto data = MakeSharedObject<NDArrayView>(DataType::Float, NDShape({ f.size() }), f.data(),
            sizeof(float) * InputDimension, DeviceDescriptor::CPUDevice());
        std::vector<NDArrayViewPtr> d = { data };
        auto value = Value::Create(inputShape, d, { timestamp == 0 ? true : false }, DeviceDescriptor::CPUDevice());

        std::unordered_map<Variable, ValuePtr> i = { { input.front(), value } };
        std::unordered_map<Variable, ValuePtr> o = { { output, nullptr } };

        model->Forward(i, o, DeviceDescriptor::CPUDevice());
        auto result2 = o[output]->Data()->DataBuffer<float>();

        std::vector<float> output1(OutputDimension, 0), output2(OutputDimension, 0);
        for (int j = 0; j < OutputDimension; ++j)
        {
            output1[j] = result(j);
            output2[j] = result2[j];
        }

        return std::make_pair(output1, output2);
    };

    // Perform evaluation
    const int NumberOfIterations = 5;
    for (int i = 0; i < NumberOfIterations; i++)
    {
        int timestamp = 0;
        for (auto& f : frames)
        {
            auto result1 = evaluateFrame(timestamp++, f);
            for (int j = 0; j < result1.first.size(); ++j)
            {
                BOOST_REQUIRE_CLOSE(result1.first[j], result1.second[j], 0.1);
            }
        }
    }
}

class Timer
{
    LARGE_INTEGER m_freq, m_start;

public:
    Timer()
    {
        // count ticks per second
        if (!QueryPerformanceFrequency(&m_freq))
            throw std::exception("Timer: QueryPerformanceFrequency failed!");
    }

    void start()
    {
        if (!QueryPerformanceCounter(&m_start))
            throw std::exception("Timer: QueryPerformanceCounter failed!");
    }

    double stop(bool inMilliseconds = true) const
    {
        LARGE_INTEGER end;
        QueryPerformanceCounter(&end);
        return ((end.QuadPart - m_start.QuadPart) / (double)m_freq.QuadPart) * (inMilliseconds ? 1000 : 1);
    }
};

void MeasurePerf(const std::function<void()>& workload, const std::string& str, int numIterations = 10, bool warmupRun = true)
{
    Timer timer;

    // Warmup run
    if (warmupRun)
        workload();

    std::vector<double> executionTimes(numIterations);
    double averageTime = 0;
    for (int i = 0; i < numIterations; ++i)
    {
        timer.start();
        workload();
        executionTimes[i] = timer.stop();
        averageTime += executionTimes[i];
    }
    averageTime /= (double)numIterations;

    std::sort(executionTimes.begin(), executionTimes.end());

    double stdDev = 0.0;
    std::for_each(executionTimes.begin(), executionTimes.end(), 
        [&stdDev, averageTime, numIterations](double executionTime) { stdDev += (std::abs(executionTime - averageTime)) / numIterations; });

    printf("%s: Min=%.2lf ms, Median=%.2lf ms, Average=%.2lf ms, Max=%.2lf ms, StdDev=%.2lf ms.\n",
        str.c_str(),
        executionTimes[0],
        executionTimes[numIterations / 2],
        averageTime,
        executionTimes[numIterations - 1],
        stdDev);
}

BOOST_AUTO_TEST_CASE(SpeechLstmModelPerformance)
{
    const int InputDimension = 80;
    const int OutputDimension = 9404;

    // Halide
    LstmSpeechEvaluator e;
    e.init("LstmSpeechEvaluator.json");

    std::vector<float> frame;
    frame.resize(InputDimension);

    for (size_t j = 0; j < InputDimension; ++j)
        frame[j] = (rand() % 256) / (float)256;

    Halide::ImageParam features(Halide::type_of<float>(), 1);
    features.set(Halide::Buffer<float>(frame.data(), InputDimension));

    Halide::Buffer<float> result1(OutputDimension);

    auto halide = [&]()
    {
        e.Evaluate(0, features, result1);
    };

    halide();

    // CNTK
    auto model = Function::Load(L"lstm_speech.model", DeviceDescriptor::CPUDevice());
    auto input = model->Arguments();
    auto output = model->Output();
    auto inputShape = NDShape({ (size_t)InputDimension });

    auto data = MakeSharedObject<NDArrayView>(DataType::Float, NDShape({ frame.size() }), frame.data(),
            sizeof(float) * InputDimension, DeviceDescriptor::CPUDevice());
    std::vector<NDArrayViewPtr> d = { data };
    auto value = Value::Create(inputShape, d, { true }, DeviceDescriptor::CPUDevice());

    std::unordered_map<Variable, ValuePtr> i = { { input.front(), value } };
    std::unordered_map<Variable, ValuePtr> o = { { output, nullptr } };

    auto cntk = [&]()
    {
        o[output] = nullptr;
        model->Forward(i, o, DeviceDescriptor::CPUDevice());
    };

    cntk();

    auto result2 = o[output]->Data()->DataBuffer<float>();

    for (int j = 0; j < OutputDimension; ++j)
    {
        BOOST_REQUIRE_CLOSE(result1(j), result2[j], 0.1);
    }

    // All is set up correctly. Run the workloads for perf.
    MeasurePerf(halide, "Halide workload 1:");
    MeasurePerf(cntk, "CNTK workload 1:");

    //MeasurePerf(cntk, "CNTK workload 2:");
    //MeasurePerf(halide, "Halide workload 2:");

    //MeasurePerf(halide, "Halide workload 3:");
    //MeasurePerf(cntk, "CNTK workload 3:");
}

BOOST_AUTO_TEST_CASE(TestVectorQuantization)
{
    std::vector<float> f = { 1.11f, -1.09f };
    auto result = Quantize<float, short>(f, 1);
    BOOST_REQUIRE_CLOSE(result.first[0] * result.second, f[0], 0.01);
    BOOST_REQUIRE_CLOSE(result.first[1] * result.second, f[1], 0.01);
}

BOOST_AUTO_TEST_CASE(TestStorageOrder)
{
    std::vector<float> m;
    for (int i = 0; i < 24; ++i)
        m.push_back((float)i);

    auto bm = Buffer<float>(m.data(), { 4, 3, 2 });

    // First coordinate is column, so it changes the most frequently.
    BOOST_REQUIRE_EQUAL(bm(0, 0, 0), m[0]);
    BOOST_REQUIRE_EQUAL(bm(1, 0, 0), m[1]);
    BOOST_REQUIRE_EQUAL(bm(2, 0, 0), m[2]);
    BOOST_REQUIRE_EQUAL(bm(3, 0, 0), m[3]);
    BOOST_REQUIRE_EQUAL(bm(0, 1, 0), m[4]);
    BOOST_REQUIRE_EQUAL(bm(1, 1, 0), m[5]);
    BOOST_REQUIRE_EQUAL(bm(2, 1, 0), m[6]);
    BOOST_REQUIRE_EQUAL(bm(3, 1, 0), m[7]);
    BOOST_REQUIRE_EQUAL(bm(0, 1, 1), m[16]);
    BOOST_REQUIRE_EQUAL(bm(1, 1, 1), m[17]);
    BOOST_REQUIRE_EQUAL(bm(2, 1, 1), m[18]);
    BOOST_REQUIRE_EQUAL(bm(3, 1, 1), m[19]);
}

BOOST_AUTO_TEST_CASE(TestMatrixQuantization)
{
    std::vector<float> m = { 10.13f, -100.18f, 16.9f, 19.5f, -15.f, -20.f };

    auto r = Quantize<float, short>(m, 1);
    BOOST_REQUIRE_CLOSE(r.first[0] * r.second, m[0], 0.1);
    BOOST_REQUIRE_CLOSE(r.first[1] * r.second, m[1], 0.1);
    BOOST_REQUIRE_CLOSE(r.first[2] * r.second, m[2], 0.1);
    BOOST_REQUIRE_CLOSE(r.first[3] * r.second, m[3], 0.1);
    BOOST_REQUIRE_CLOSE(r.first[4] * r.second, m[4], 0.1);
    BOOST_REQUIRE_CLOSE(r.first[5] * r.second, m[5], 0.1);

    auto bm = Buffer<float>(m.data(), { 3, 2 });
    Halide::Func fbm;
    Halide::Var x, y;
    fbm(x, y) = bm(x, y);

    Halide::Buffer<short> result(3, 2);
    Halide::Buffer<float> step = Halide::Buffer<float>::make_scalar("step");

    auto quantized = Quantize<float, short>(fbm, 2, 3, 1);
    auto p = Halide::Pipeline(quantized);
    p.realize({ result, step });

    BOOST_REQUIRE_CLOSE(step(0), r.second, 0.001);
    BOOST_REQUIRE_EQUAL(result(0, 0), r.first[0]);
    BOOST_REQUIRE_EQUAL(result(1, 0), r.first[1]);
    BOOST_REQUIRE_EQUAL(result(2, 0), r.first[2]);
    BOOST_REQUIRE_EQUAL(result(0, 1), r.first[3]);
    BOOST_REQUIRE_EQUAL(result(1, 1), r.first[4]);
    BOOST_REQUIRE_EQUAL(result(2, 1), r.first[5]);
}

BOOST_AUTO_TEST_CASE(TestQuantizedMatrixMultiplication)
{
    std::vector<float> v = { 1.11f, -1.09f, 8.004f };
    std::vector<float> m = { 10.13f, -100.18f, 16.9f, 19.5f, -15.f, -20.f };
    auto bv = Buffer<float>(v.data(), 3);
    auto bm = Buffer<float>(m.data(), 2, 3);
    Halide::Func fbv;
    Halide::Var index;
    fbv(index) = bv(index);
    Halide::Func fbm;
    Halide::Var x, y;
    fbm(x, y) = bm(x, y);

    auto vq = Quantize<float, short>(fbv, 3, 2);

    // Checking that vector quantization is correct.
    Halide::Buffer<short> vqr(3);
    Halide::Buffer<float> vqs = Halide::Buffer<float>::make_scalar("step");
    Halide::Pipeline(vq).realize({ vqr, vqs });

    BOOST_REQUIRE_CLOSE(vqr(0) * vqs(0), v[0], 0.1);
    BOOST_REQUIRE_CLOSE(vqr(1) * vqs(0), v[1], 0.1);
    BOOST_REQUIRE_CLOSE(vqr(2) * vqs(0), v[2], 0.1);

    auto mq = Quantize<float, short>(fbm, 3, 2, 2);

    // Checking that matrix quantization is correct.
    Halide::Buffer<short> mqr(2, 3);
    Halide::Buffer<float> mqs = Halide::Buffer<float>::make_scalar("step");
    Halide::Pipeline(mq).realize({ mqr, mqs });

    BOOST_REQUIRE_CLOSE(mqr(0, 0) * mqs(0), m[0], 0.1);
    BOOST_REQUIRE_CLOSE(mqr(1, 0) * mqs(0), m[1], 0.1);
    BOOST_REQUIRE_CLOSE(mqr(0, 1) * mqs(0), m[2], 0.1);
    BOOST_REQUIRE_CLOSE(mqr(1, 1) * mqs(0), m[3], 0.1);
    BOOST_REQUIRE_CLOSE(mqr(0, 2) * mqs(0), m[4], 0.1);
    BOOST_REQUIRE_CLOSE(mqr(1, 2) * mqs(0), m[5], 0.1);

    // Actually doing the multiplication
    auto f = VectorByMatrixTimesQuantized(vq, mq, 3, 2);
    auto result = Buffer<float>(2);
    f.realize({ result });

    BOOST_REQUIRE_CLOSE(result(0), -127.2367f, 0.1);
    BOOST_REQUIRE_CLOSE(result(1), -291.9898f, 1);
}


BOOST_AUTO_TEST_CASE(TestPlus, *utf::tolerance(0.00001))
{
    float ca[] = { 1, 2, 3 };
    float cb[] = { 4, 5, 6 };

    ImageParam a(Halide::type_of<float>(), 1);
    ImageParam b(Halide::type_of<float>(), 1);

    auto result = Plus(a, b);
    a.set(Buffer<float>((float*)ca, 3));
    b.set(Buffer<float>((float*)cb, 3));

    Buffer<float> output(3);
    result.realize(output);

    BOOST_REQUIRE_EQUAL(output(0), 5);
    BOOST_REQUIRE_EQUAL(output(1), 7);
    BOOST_REQUIRE_EQUAL(output(2), 9);
}

BOOST_AUTO_TEST_CASE(TestSlice, *utf::tolerance(0.00001))
{
    ImageParam a(Halide::type_of<float>(), 1);
    auto result = Slice(a, 1 ,6);

    float ca[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    a.set(Buffer<float>((float*)ca, 10));

    Buffer<float> output(5);
    result.realize(output);

    for (int i = 0; i < 5; ++i)
    {
        BOOST_REQUIRE_EQUAL(output(i), i + 1);
    }
}

BOOST_AUTO_TEST_CASE(TestSplice, *utf::tolerance(0.00001))
{
    ImageParam a(Halide::type_of<float>(), 1);
    ImageParam b(Halide::type_of<float>(), 1);

    auto result = Splice(a, b, 2, 8);

    float ca[] = { 0, 1 };
    a.set(Buffer<float>((float*)ca, 2));

    float cb[] = { 2, 3, 4, 5, 6, 7, 8, 9 };
    b.set(Buffer<float>((float*)cb, 8));

    Buffer<float> output(10);
    result.realize(output);

    for (int i = 0; i < 10; ++i)
    {
        BOOST_REQUIRE_EQUAL(output(i), i);
    }
}


BOOST_AUTO_TEST_CASE(TestVecMultiply, *utf::tolerance(0.00001))
{
    float ca[] = { 1, 2, 3 };
    float cb[] = { 1, 2, 1, 2, 1, 2, 1, 2, 1 };

    ImageParam a(Halide::type_of<float>(), 1);
    ImageParam b(Halide::type_of<float>(), 2);

    auto result = VectorByMatrixTimes(a, b, 3, 3);
    a.set(Buffer<float>((float*)ca, 3));
    b.set(Buffer<float>((float*)cb, 3, 3));

    Buffer<float> output(3);
    result.realize(output);

    BOOST_REQUIRE_EQUAL(output(0), 8);
    BOOST_REQUIRE_EQUAL(output(1), 10);
    BOOST_REQUIRE_EQUAL(output(2), 8);
}

BOOST_AUTO_TEST_SUITE_END()

}}
