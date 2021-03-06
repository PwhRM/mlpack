/**
 * @file gan_network_test.cpp
 * @author Kris Singh
 * @author Shikhar Jaiswal
 *
 * Tests the GAN network.
 *
 * mlpack is free software; you may redistribute it and/or modify it under the
 * terms of the 3-clause BSD license.  You should have received a copy of the
 * 3-clause BSD license along with mlpack.  If not, see
 * http://www.opensource.org/licenses/BSD-3-Clause for more information.
 */
#include <mlpack/core.hpp>

#include <mlpack/methods/ann/init_rules/gaussian_init.hpp>
#include <mlpack/methods/ann/loss_functions/cross_entropy_error.hpp>
#include <mlpack/methods/ann/loss_functions/sigmoid_cross_entropy_error.hpp>
#include <mlpack/methods/ann/gan.hpp>
#include <mlpack/methods/ann/ffn.hpp>
#include <mlpack/methods/ann/layer/layer.hpp>
#include <mlpack/methods/softmax_regression/softmax_regression.hpp>
#include <mlpack/core/optimizers/adam/adam.hpp>

#include <boost/test/unit_test.hpp>
#include "test_tools.hpp"

using namespace mlpack;
using namespace mlpack::ann;
using namespace mlpack::math;
using namespace mlpack::optimization;
using namespace mlpack::regression;
using namespace std::placeholders;

BOOST_AUTO_TEST_SUITE(GANNetworkTest);

/*
 * Load pre trained network values
 * for generating distribution that
 * is close to N(4, 0.5)
 */
BOOST_AUTO_TEST_CASE(GANTest)
{
  size_t generatorHiddenLayerSize = 8;
  size_t discriminatorHiddenLayerSize = 8;
  size_t generatorOutputSize = 1;
  size_t discriminatorOutputSize = 1;
  size_t discriminatorPreTrain = 0;
  size_t batchSize = 8;
  size_t noiseDim = 1;
  size_t generatorUpdateStep = 1;
  size_t numSamples = 10000;
  double multiplier = 1;

  arma::mat trainData(1, 10000);
  trainData.imbue( [&]() { return arma::as_scalar(RandNormal(4, 0.5));});
  trainData = arma::sort(trainData);

  // Create the Discriminator network
  FFN<CrossEntropyError<> > discriminator;
  discriminator.Add<Linear<> > (
      generatorOutputSize, discriminatorHiddenLayerSize * 2);
  discriminator.Add<ReLULayer<> >();
  discriminator.Add<Linear<> > (
      discriminatorHiddenLayerSize * 2, discriminatorHiddenLayerSize * 2);
  discriminator.Add<ReLULayer<> >();
  discriminator.Add<Linear<> > (
      discriminatorHiddenLayerSize * 2, discriminatorHiddenLayerSize * 2);
  discriminator.Add<ReLULayer<> >();
  discriminator.Add<Linear<> > (
      discriminatorHiddenLayerSize * 2, discriminatorOutputSize);
  discriminator.Add<SigmoidLayer<> >();
  // Create the Generator network
  FFN<CrossEntropyError<> > generator;
  generator.Add<Linear<> >(noiseDim, generatorHiddenLayerSize);
  generator.Add<SoftPlusLayer<> >();
  generator.Add<Linear<> >(generatorHiddenLayerSize, generatorOutputSize);

  // Create GAN
  GaussianInitialization gaussian(0, 0.1);
  std::function<double ()> noiseFunction = [](){ return math::Random(-8, 8) +
      math::RandNormal(0, 1) * 0.01;};
  GAN<FFN<CrossEntropyError<> >,
      GaussianInitialization,
      std::function<double()> >
  gan(trainData, generator, discriminator, gaussian, noiseFunction,
      noiseDim, batchSize, generatorUpdateStep, discriminatorPreTrain,
      multiplier);
  gan.Reset();

  Log::Info << "Loading Parameters" << std::endl;
  arma::mat parameters, generatorParameters;
  parameters.load("preTrainedGAN.arm");
  gan.Parameters() = parameters;

  // Generate samples
  Log::Info << "Sampling..." << std::endl;
  arma::mat noise(noiseDim, 1);

  size_t dim = std::sqrt(trainData.n_rows);
  arma::mat generatedData(2 * dim, dim * numSamples);

  for (size_t i = 0; i < numSamples; i++)
  {
    arma::mat samples;
    noise.imbue( [&]() { return noiseFunction(); } );

    generator.Forward(noise, samples);
    samples.reshape(dim, dim);
    samples = samples.t();

    generatedData.submat(0, i * dim, dim - 1, i * dim + dim - 1) = samples;

    samples = trainData.col(math::RandInt(0, trainData.n_cols));
    samples.reshape(dim, dim);
    samples = samples.t();

    generatedData.submat(dim,
        i * dim, 2 * dim - 1, i * dim + dim - 1) = samples;
  }

  double generatedMean = arma::as_scalar(arma::mean(
      generatedData.rows(0, dim - 1), 1));
  double originalMean = arma::as_scalar(arma::mean(
      generatedData.rows(dim, 2 * dim - 1), 1));
  double generatedStd = arma::as_scalar(arma::stddev(
      generatedData.rows(0, dim - 1), 0, 1));
  double originalStd = arma::as_scalar(arma::stddev(
      generatedData.rows(dim, 2 * dim - 1), 0, 1));

  BOOST_REQUIRE_LE(generatedMean - originalMean, 0.2);
  BOOST_REQUIRE_LE(generatedStd - originalStd, 0.2);
}

/*
 * Tests the GAN implementation of the O'Reilly Test on the MNIST dataset.
 * It's not viable to train on bigger parameters due to time constraints.
 * Please refer mlpack/models repository for the tutorial.
 */
BOOST_AUTO_TEST_CASE(GANMNISTTest)
{
  size_t dNumKernels = 32;
  size_t discriminatorPreTrain = 5;
  size_t batchSize = 5;
  size_t noiseDim = 100;
  size_t generatorUpdateStep = 1;
  size_t numSamples = 10;
  double stepSize = 0.0003;
  double eps = 1e-8;
  size_t numEpoches = 1;
  double tolerance = 1e-5;
  int datasetMaxCols = 10;
  bool shuffle = true;
  double multiplier = 10;

  Log::Info << std::boolalpha
      << " batchSize = " << batchSize << std::endl
      << " generatorUpdateStep = " << generatorUpdateStep << std::endl
      << " noiseDim = " << noiseDim << std::endl
      << " numSamples = " << numSamples << std::endl
      << " stepSize = " << stepSize << std::endl
      << " numEpoches = " << numEpoches << std::endl
      << " tolerance = " << tolerance << std::endl
      << " shuffle = " << shuffle << std::endl;

  arma::mat trainData;
  trainData.load("mnist_first250_training_4s_and_9s.arm");
  Log::Info << arma::size(trainData) << std::endl;

  if (datasetMaxCols > 0)
    trainData = trainData.cols(0, datasetMaxCols - 1);

  size_t numIterations = trainData.n_cols * numEpoches;
  numIterations /= batchSize;

  Log::Info << "Dataset loaded (" << trainData.n_rows << ", "
            << trainData.n_cols << ")" << std::endl;
  Log::Info << trainData.n_rows << "--------" << trainData.n_cols << std::endl;

  // Create the Discriminator network
  FFN<SigmoidCrossEntropyError<> > discriminator;
  discriminator.Add<Convolution<> >(1, dNumKernels, 5, 5, 1, 1, 2, 2, 28, 28);
  discriminator.Add<ReLULayer<> >();
  discriminator.Add<MeanPooling<> >(2, 2, 2, 2);
  discriminator.Add<Convolution<> >(dNumKernels, 2 * dNumKernels, 5, 5, 1, 1,
      2, 2, 14, 14);
  discriminator.Add<ReLULayer<> >();
  discriminator.Add<MeanPooling<> >(2, 2, 2, 2);
  discriminator.Add<Linear<> >(7 * 7 * 2 * dNumKernels, 1024);
  discriminator.Add<ReLULayer<> >();
  discriminator.Add<Linear<> >(1024, 1);

  // Create the Generator network
  FFN<SigmoidCrossEntropyError<> > generator;
  generator.Add<Linear<> >(noiseDim, 3136);
  generator.Add<ReLULayer<> >();
  generator.Add<Convolution<> >(1, noiseDim / 2, 3, 3, 2, 2, 1, 1, 56, 56);
  generator.Add<ReLULayer<> >();
  generator.Add<BilinearInterpolation<> >(28, 28, 56, 56, noiseDim / 2);
  generator.Add<Convolution<> >(noiseDim / 2, noiseDim / 4, 3, 3, 2, 2, 1, 1,
      56, 56);
  generator.Add<ReLULayer<> >();
  generator.Add<BilinearInterpolation<> >(28, 28, 56, 56, noiseDim / 4);
  generator.Add<Convolution<> >(noiseDim / 4, 1, 3, 3, 2, 2, 1, 1, 56, 56);
  generator.Add<TanHLayer<> >();

  // Create GAN
  GaussianInitialization gaussian(0, 1);
  Adam optimizer(stepSize, batchSize, 0.9, 0.999, eps, numIterations,
      tolerance, shuffle);
  std::function<double()> noiseFunction = [] () {
      return math::RandNormal(0, 1);};
  GAN<FFN<SigmoidCrossEntropyError<> >, GaussianInitialization,
      std::function<double()> > gan(trainData, generator, discriminator,
      gaussian, noiseFunction, noiseDim, batchSize, generatorUpdateStep,
      discriminatorPreTrain, multiplier);

  Log::Info << "Training..." << std::endl;
  gan.Train(optimizer);

  // Generate samples
  Log::Info << "Sampling..." << std::endl;
  arma::mat noise(noiseDim, batchSize);
  size_t dim = std::sqrt(trainData.n_rows);
  arma::mat generatedData(2 * dim, dim * numSamples);

  for (size_t i = 0; i < numSamples; i++)
  {
    arma::mat samples;
    noise.imbue( [&]() { return noiseFunction(); } );

    generator.Forward(noise, samples);
    samples.reshape(dim, dim);
    samples = samples.t();

    generatedData.submat(0, i * dim, dim - 1, i * dim + dim - 1) = samples;

    samples = trainData.col(math::RandInt(0, trainData.n_cols));
    samples.reshape(dim, dim);
    samples = samples.t();

    generatedData.submat(dim,
        i * dim, 2 * dim - 1, i * dim + dim - 1) = samples;
  }

  Log::Info << "Output generated!" << std::endl;
}

BOOST_AUTO_TEST_SUITE_END();
