﻿using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace CNTK.CSTrainingExamples
{
    public class MNISTClassifier
    {
        /// <summary>
        /// test execution folder is: CNTK/x64/BuildFolder
        /// data folder is: CNTK/Tests/EndToEndTests/Image/Data
        /// </summary>
        public static string ImageDataFolder = "../../Tests/EndToEndTests/Image/Data";

        public static void TrainAndEvaluate(DeviceDescriptor device, bool useConvolution, bool forceRetrain)
        {
            var featureStreamName = "features";
            var labelsStreamName = "labels";
            var classifierName = "classifierOutput";
            Function classifierOutput;
            int[] imageDim = useConvolution ? new int[] { 28, 28, 1 } : new int[] { 784 };
            var input = CNTKLib.InputVariable(imageDim, DataType.Float, featureStreamName);
            int imageSize = 28 * 28;
            int numClasses = 10;

            IList<StreamConfiguration> streamConfigurations = new StreamConfiguration[]
                { new StreamConfiguration(featureStreamName, imageSize), new StreamConfiguration(labelsStreamName, numClasses) };

            string modelFile = useConvolution ? "MNISTConvolution.model" : "MNISTMLP.model";
            if (File.Exists(modelFile) && !forceRetrain)
            {
                var minibatchSourceExistModel = MinibatchSource.TextFormatMinibatchSource(
                    Path.Combine(ImageDataFolder, "Test_cntk_text.txt"), streamConfigurations);
                TestHelper.ValidateModelWithMinibatchSource(modelFile, minibatchSourceExistModel,
                                    imageDim, numClasses, featureStreamName, labelsStreamName, classifierName, device);
                return;
            }

            if (useConvolution)
            {

                var scaledInput = CNTKLib.ElementTimes(Constant.Scalar<float>(0.00390625f, device), input);
                classifierOutput = CreateConvolutionalNeuralNetwork(scaledInput, numClasses, device, classifierName);
            }
            else
            {
                int hiddenLayerDim = 200;
                var scaledInput = CNTKLib.ElementTimes(Constant.Scalar<float>(0.00390625f, device), input);
                classifierOutput = CreateMLPClassifier(device, numClasses, hiddenLayerDim, scaledInput, classifierName);
            }

            var labels = CNTKLib.InputVariable(new int[] { numClasses }, DataType.Float, labelsStreamName);
            var trainingLoss = CNTKLib.CrossEntropyWithSoftmax(new Variable(classifierOutput), labels, "lossFunction");
            var prediction = CNTKLib.ClassificationError(new Variable(classifierOutput), labels, "classificationError");
            
            var minibatchSource = MinibatchSource.TextFormatMinibatchSource(
                Path.Combine(ImageDataFolder, "Train_cntk_text.txt"), streamConfigurations, MinibatchSource.InfinitelyRepeat);

            var featureStreamInfo = minibatchSource.StreamInfo(featureStreamName);
            var labelStreamInfo = minibatchSource.StreamInfo(labelsStreamName);

            CNTK.TrainingParameterScheduleDouble learningRatePerSample = new CNTK.TrainingParameterScheduleDouble(
                0.003125, TrainingParameterScheduleDouble.UnitType.Sample);

            IList<Learner> parameterLearners = new List<Learner>() { Learner.SGDLearner(classifierOutput.Parameters(), learningRatePerSample) };
            var trainer = Trainer.CreateTrainer(classifierOutput, trainingLoss, prediction, parameterLearners);

            const uint minibatchSize = 64;
            int outputFrequencyInMinibatches = 20, i = 0;
            int epochs = 5;
            while (epochs > 0)
            {
                var minibatchData = minibatchSource.GetNextMinibatch(minibatchSize, device);
                var arguments = new Dictionary<Variable, MinibatchData>
                {
                    { input, minibatchData[featureStreamInfo] },
                    { labels, minibatchData[labelStreamInfo] }
                };
                trainer.TrainMinibatch(arguments, device);
                TestHelper.PrintTrainingProgress(trainer, i++, outputFrequencyInMinibatches);
                if (TestHelper.MiniBatchDataIsSweepEnd(minibatchData.Values))
                {
                    epochs--;
                }
            }

            classifierOutput.Save(modelFile);

            var minibatchSourceNewModel = MinibatchSource.TextFormatMinibatchSource(
                Path.Combine(ImageDataFolder, "Test_cntk_text.txt"), streamConfigurations, MinibatchSource.FullDataSweep);
            TestHelper.ValidateModelWithMinibatchSource(modelFile, minibatchSourceNewModel,
                                imageDim, numClasses, featureStreamName, labelsStreamName, classifierName, device);
        }

        private static Function CreateMLPClassifier(DeviceDescriptor device, int numOutputClasses, int hiddenLayerDim, 
            Function scaledInput, string classifierName)
        {
            Function dense1 = TestHelper.Dense(scaledInput, hiddenLayerDim, device, Activation.Sigmoid, "");
            Function classifierOutput = TestHelper.Dense(dense1, numOutputClasses, device, Activation.None, classifierName);
            return classifierOutput;
        }

        static Function CreateConvolutionalNeuralNetwork(Variable features, int outDims, DeviceDescriptor device, string classifierName)
        {
            int kernelWidth1 = 3, kernelHeight1 = 3, numInputChannels1 = 1, outFeatureMapCount1 = 4;
            int hStride1 = 2, vStride1 = 2;
            int poolingWindowWidth1 = 3, poolingWindowHeight1 = 3;

            // // 28x28x1 -> 14x14x4
            Function pooling1 = ConvolutionWithMaxPooling(features, device, kernelWidth1, kernelHeight1,
                numInputChannels1, outFeatureMapCount1, hStride1, vStride1, poolingWindowWidth1, poolingWindowHeight1);

            // return TestHelper.Dense(pooling1, outDims, device, Activation.None, classifierName);

            // 14x14x8 -> 7x7x8
            int kernelWidth2 = 3, kernelHeight2 = 3, numInputChannels2 = outFeatureMapCount1, outFeatureMapCount2 = 8;
            int hStride2 = 2, vStride2 = 2;
            int poolingWindowWidth2 = 3, poolingWindowHeight2 = 3;

            Function pooling2 = ConvolutionWithMaxPooling(pooling1, device, kernelWidth2, kernelHeight2,
                numInputChannels2, outFeatureMapCount2, hStride2, vStride2, poolingWindowWidth2, poolingWindowHeight2);

            Function denseLayer = TestHelper.Dense(pooling2, outDims, device, Activation.None, classifierName);
            return denseLayer;
        }

        private static Function ConvolutionWithMaxPooling(Variable features, DeviceDescriptor device, 
            int kernelWidth, int kernelHeight, int numInputChannels, int outFeatureMapCount, 
            int hStride, int vStride, int poolingWindowWidth, int poolingWindowHeight)
        {
            double convWScale = 0.26;
            var convParams = new Parameter(new int[] { kernelWidth, kernelHeight, numInputChannels, outFeatureMapCount }, DataType.Float,
                CNTKLib.GlorotUniformInitializer(convWScale, -1, 2), device);
            Function convFunction = CNTKLib.ReLU(CNTKLib.Convolution(convParams, features, new int[] { 1, 1, numInputChannels } /* strides */));

            Function pooling = CNTKLib.Pooling(convFunction, PoolingType.Max,
                new int[] { poolingWindowWidth, poolingWindowHeight }, new int[] { hStride, vStride }, new bool[] { true });
            return pooling;
        }
    }
}
