//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#include "stdafx.h"
#include "Common.h"

#pragma warning(push)
#pragma warning(disable : 4715)
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#pragma warning(pop)

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
    // Halide
    LstmSpeechEvaluator e;
    e.init("LstmSpeechEvaluator.json");

    Halide::ImageParam features(Halide::type_of<float>(), 1);
    std::vector<float> v;
    v.resize(80);
    features.set(Halide::Buffer<float>(v.data(), 80));

    Halide::Buffer<float> result(9404);
    e.Evaluate(0, features, result);
    auto result10 = result(0);

    // CNTK
    auto model = Function::Load(L"lstm_speech.model", DeviceDescriptor::CPUDevice());
    auto input = model->Arguments();
    auto output = model->Output();

    ValuePtr value = MakeSharedObject<Value>(
        MakeSharedObject<NDArrayView>(DataType::Float, NDShape({ 80 }), v.data(), sizeof(float) * 80, DeviceDescriptor::CPUDevice()));

    std::unordered_map<Variable, ValuePtr> i = { { input.front(), value } };
    std::unordered_map<Variable, ValuePtr> o = { { output, nullptr } };

    model->Forward(i, o, DeviceDescriptor::CPUDevice());
    auto result20 = *(o[output]->Data()->DataBuffer<float>());

    BOOST_REQUIRE_CLOSE(result10, result20, 0.0001);
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
    a.set(Buffer<float>((float*)ca, 8));

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
