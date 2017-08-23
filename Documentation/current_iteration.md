# Aug-Sept 2017 Notes

## Documentation

### Add HTML version of tutorials and manuals so that they can be searchable
### Add missing evaluation documents

## System 

### 16bit support for training on Volta GPU (limited functionality)
### Update learner interface to simplify parameter setting and adding new learners (**Potential breaking change**) 
### A C#/.NET API that enables people to build and train networks. 
##### Basic training support is added to C#/.NET API. New training examples include:
##### 1. Convolution neural network for image classification of the MNIST dataset. (https://github.com/Microsoft/CNTK/tree/master/Examples/TrainingCShape/Common/MNISTClassifier.cs)
##### 2. Build, train, and evaluate a ResNet model with C#/.NET API. (https://github.com/Microsoft/CNTK/tree/master/Examples/TrainingCShape/Common/CifarResNetClassifier.cs)
##### 3. Transfer learning with C#/.NET API. (https://github.com/Microsoft/CNTK/tree/master/Examples/TrainingCShape/Common/TransferLearning.cs)
##### 4. Build and train a LSTM sequence classifier with C#/.NET API. (https://github.com/Microsoft/CNTK/tree/master/Examples/TrainingCShape/Common/LSTMSequenceClassifier.cs)

### R-binding for training and evaluation (will be published in a separate repository) 
### Improve statistics for distributed evaluation 

## Examples
### Faster R-CNN object detection 
### New example for natural language processing (NLP) 
### Semantic segmentation (stretch goal) 

## Operations
### Noise contrastive estimation node 
### Aggregation on sparse gradient for embedded layer
### Gradient as an operator (stretch goal) 
### Reduced rank for convolution in C++ to enable convolution on 1D data 
### Dilated convolution 

## Performance 
### Asynchronous evaluation API (Python and C#) 
### Intel MKL update to improve inference speed on CPU by around 2x on AlexNet 

## Keras and Tensorboard 
### Example on Keras and SKLearn multi-GPU support on CNTK 
### Image feature support with Tensorboard for CNTK 

## Others 
### Continue work on [Deep Learning Explained](https://www.edx.org/course/deep-learning-explained-microsoft-dat236x) course on edX. 
