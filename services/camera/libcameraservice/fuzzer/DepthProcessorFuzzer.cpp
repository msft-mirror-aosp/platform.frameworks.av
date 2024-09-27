/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "common/DepthPhotoProcessor.h"

#include <random>

#include <fuzzer/FuzzedDataProvider.h>

using namespace android;
using namespace android::camera3;

static const uint8_t kTotalDepthJpegBufferCount = 3;
static const uint8_t kIntrinsicCalibrationSize = 5;
static const uint8_t kLensDistortionSize = 5;
static const uint8_t kDqtSize = 5;

static const uint16_t kMinDimension = 2;
static const uint16_t kMaxDimension = 1024;

static const DepthPhotoOrientation kDepthPhotoOrientations[] = {
        DepthPhotoOrientation::DEPTH_ORIENTATION_0_DEGREES,
        DepthPhotoOrientation::DEPTH_ORIENTATION_90_DEGREES,
        DepthPhotoOrientation::DEPTH_ORIENTATION_180_DEGREES,
        DepthPhotoOrientation::DEPTH_ORIENTATION_270_DEGREES};

void generateDepth16Buffer(std::vector<uint16_t>* depth16Buffer /*out*/, size_t length,
                           FuzzedDataProvider& fdp) {
    std::default_random_engine gen(fdp.ConsumeIntegral<uint8_t>());
    std::uniform_int_distribution uniDist(0, UINT16_MAX - 1);
    for (size_t i = 0; i < length; ++i) {
        (*depth16Buffer)[i] = uniDist(gen);
    }
}

void fillRandomBufferData(std::vector<unsigned char>& buffer, size_t bytes,
                          FuzzedDataProvider& fdp) {
    while (bytes--) {
        buffer.push_back(fdp.ConsumeIntegral<uint8_t>());
    }
}

void addMarkersInJpegBuffer(std::vector<uint8_t>& Buffer, size_t& height, size_t& width,
                            FuzzedDataProvider& fdp) {
    /* Add the SOI Marker */
    Buffer.push_back(0xFF);
    Buffer.push_back(0xD8);

    /* Add the JFIF Header */
    const char header[] = {0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00,
                           0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00};
    Buffer.insert(Buffer.end(), header, header + sizeof(header));

    /* Add the SOF Marker */
    Buffer.push_back(0xFF);
    Buffer.push_back(0xC0);

    Buffer.push_back(0x00);  // Length high byte
    Buffer.push_back(0x11);  // Length low byte

    Buffer.push_back(fdp.ConsumeIntegral<uint8_t>());  // Random precision

    height = fdp.ConsumeIntegralInRange<uint16_t>(kMinDimension, kMaxDimension);  // Image height
    Buffer.push_back((height & 0xFF00) >> 8);
    Buffer.push_back(height & 0x00FF);

    width = fdp.ConsumeIntegralInRange<uint16_t>(kMinDimension, kMaxDimension);  // Image width
    Buffer.push_back((width & 0xFF00) >> 8);
    Buffer.push_back(width & 0x00FF);

    Buffer.push_back(0x03);  // Number of components (3 for Y, Cb, Cr)

    /* Add DQT (Define Quantization Table) Marker */
    Buffer.push_back(0xFF);
    Buffer.push_back(0xDB);

    Buffer.push_back(0x00);  // Length high byte
    Buffer.push_back(0x43);  // Length low byte

    Buffer.push_back(0x00);  // Precision and table identifier

    fillRandomBufferData(Buffer, kDqtSize, fdp);  // Random DQT data

    /* Add the Component Data */
    unsigned char componentData[] = {0x01, 0x21, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01};
    Buffer.insert(Buffer.end(), componentData, componentData + sizeof(componentData));

    /* Add the DHT (Define Huffman Table) Marker */
    Buffer.push_back(0xFF);
    Buffer.push_back(0xC4);
    Buffer.push_back(0x00);  // Length high byte
    Buffer.push_back(0x1F);  // Length low byte

    Buffer.push_back(0x00);                 // Table class and identifier
    fillRandomBufferData(Buffer, 16, fdp);  // 16 codes for lengths
    fillRandomBufferData(Buffer, 12, fdp);  // Values

    /* Add the SOS (Start of Scan) Marker */
    Buffer.push_back(0xFF);
    Buffer.push_back(0xDA);
    Buffer.push_back(0x00);  // Length high byte
    Buffer.push_back(0x0C);  // Length low byte

    Buffer.push_back(0x03);  // Number of components (3 for Y, Cb, Cr)
    unsigned char sosComponentData[] = {0x01, 0x00, 0x02, 0x11, 0x03, 0x11};
    Buffer.insert(Buffer.end(), sosComponentData, sosComponentData + sizeof(sosComponentData));

    Buffer.push_back(0x00);  // Spectral selection start
    Buffer.push_back(0x3F);  // Spectral selection end
    Buffer.push_back(0x00);  // Successive approximation

    size_t remainingBytes = (256 * 1024) - Buffer.size() - 2;  // Subtract 2 for EOI marker
    fillRandomBufferData(Buffer, remainingBytes, fdp);

    /* Add the EOI Marker */
    Buffer.push_back(0xFF);
    Buffer.push_back(0xD9);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    FuzzedDataProvider fdp(data, size);

    DepthPhotoInputFrame inputFrame;

    inputFrame.mIsLogical = fdp.ConsumeBool();
    inputFrame.mJpegQuality = fdp.ConsumeProbability<float>() * 100;
    inputFrame.mOrientation = fdp.PickValueInArray<DepthPhotoOrientation>(kDepthPhotoOrientations);

    if (fdp.ConsumeBool()) {
        for (uint8_t i = 0; i < kIntrinsicCalibrationSize; ++i) {
            inputFrame.mIntrinsicCalibration[i] = fdp.ConsumeFloatingPoint<float>();
        }
        inputFrame.mIsIntrinsicCalibrationValid = 1;
    }

    if (fdp.ConsumeBool()) {
        for (uint8_t i = 0; i < kLensDistortionSize; ++i) {
            inputFrame.mLensDistortion[i] = fdp.ConsumeFloatingPoint<float>();
        }
        inputFrame.mIsLensDistortionValid = 1;
    }

    std::vector<uint8_t> Buffer;
    size_t height, width;
    addMarkersInJpegBuffer(Buffer, height, width, fdp);
    inputFrame.mMainJpegBuffer = reinterpret_cast<const char*>(Buffer.data());

    inputFrame.mMainJpegHeight = height;
    inputFrame.mMainJpegWidth = width;
    inputFrame.mMainJpegSize = Buffer.size();
    // Worst case both depth and confidence maps have the same size as the main color image.
    inputFrame.mMaxJpegSize = inputFrame.mMainJpegSize * kTotalDepthJpegBufferCount;

    std::vector<uint16_t> depth16Buffer(height * width);
    generateDepth16Buffer(&depth16Buffer, height * width, fdp);
    inputFrame.mDepthMapBuffer = depth16Buffer.data();
    inputFrame.mDepthMapHeight = height;
    inputFrame.mDepthMapWidth = inputFrame.mDepthMapStride = width;

    std::vector<uint8_t> depthPhotoBuffer(inputFrame.mMaxJpegSize);
    size_t actualDepthPhotoSize = 0;

    processDepthPhotoFrame(inputFrame, depthPhotoBuffer.size(), depthPhotoBuffer.data(),
                           &actualDepthPhotoSize);

    return 0;
}
