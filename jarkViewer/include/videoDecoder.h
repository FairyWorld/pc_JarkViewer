#pragma once

#include "Utils.h"


class H264VideoDecoder {
private:
    IMFSourceReader* m_pSourceReader = nullptr;
    IMFMediaType* m_pDecoderOutputType = nullptr;
    GUID m_outputFormat = GUID_NULL;
    bool m_initialized = false;

public:
    H264VideoDecoder() = default;

    ~H264VideoDecoder() {
        Cleanup();
    }

    HRESULT Initialize(const std::vector<uint8_t>& videoBuffer) {
        HRESULT hr = S_OK;

        // ��ʼ��Media Foundation
        hr = MFStartup(MF_VERSION);
        if (FAILED(hr)) {
            std::cerr << "Failed to initialize Media Foundation" << std::endl;
            return hr;
        }

        // �����ڴ��ֽ���
        IMFByteStream* pByteStream = nullptr;
        hr = MFCreateTempFile(MF_ACCESSMODE_READWRITE, MF_OPENMODE_DELETE_IF_EXIST,
            MF_FILEFLAGS_NONE, &pByteStream);
        if (FAILED(hr)) {
            std::cerr << "Failed to create byte stream" << std::endl;
            return hr;
        }

        // ����Ƶ����д���ֽ���
        ULONG bytesWritten = 0;
        hr = pByteStream->Write(videoBuffer.data(), static_cast<ULONG>(videoBuffer.size()), &bytesWritten);
        if (FAILED(hr)) {
            pByteStream->Release();
            return hr;
        }

        // ������λ�õ���ʼ
        QWORD currentPosition = 0;
        hr = pByteStream->Seek(msoBegin, 0, MFBYTESTREAM_SEEK_FLAG_CANCEL_PENDING_IO, &currentPosition);
        if (FAILED(hr)) {
            pByteStream->Release();
            return hr;
        }

        // ����Դ��ȡ��
        IMFAttributes* pAttributes = nullptr;
        hr = MFCreateAttributes(&pAttributes, 1);
        if (SUCCEEDED(hr)) {
            // ����Ӳ�����루��ѡ��
            hr = pAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
        }

        if (SUCCEEDED(hr)) {
            hr = MFCreateSourceReaderFromByteStream(pByteStream, pAttributes, &m_pSourceReader);
        }

        if (pAttributes) {
            pAttributes->Release();
        }
        pByteStream->Release();

        if (FAILED(hr)) {
            std::cerr << "Failed to create source reader" << std::endl;
            return hr;
        }

        // ���ý����������ʽΪRGB24
        hr = ConfigureDecoder();
        if (FAILED(hr)) {
            return hr;
        }

        m_initialized = true;
        return S_OK;
    }

    HRESULT ConfigureDecoder() {
        HRESULT hr = S_OK;

        // ���ȳ��Ի�ȡԭʼý������
        IMFMediaType* pNativeType = nullptr;
        hr = m_pSourceReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &pNativeType);
        if (FAILED(hr)) {
            std::cerr << "Failed to get native media type" << std::endl;
            return hr;
        }

        // ���ԭʼ��ʽ��Ϣ���ڵ���
        GUID subtype;
        if (SUCCEEDED(pNativeType->GetGUID(MF_MT_SUBTYPE, &subtype))) {
            std::wcout << L"Native subtype: ";
            if (subtype == MFVideoFormat_H264) {
                std::wcout << L"H264" << std::endl;
            }
            else {
                std::wcout << L"Other format" << std::endl;
            }
        }

        pNativeType->Release();

        // �������ý�����ͣ�ʹ��NV12��YUY2��ʽ�������ݣ�
        hr = MFCreateMediaType(&m_pDecoderOutputType);
        if (FAILED(hr)) {
            return hr;
        }

        hr = m_pDecoderOutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        if (FAILED(hr)) {
            return hr;
        }

        // ���ȳ���NV12��ʽ
        hr = m_pDecoderOutputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
        if (SUCCEEDED(hr)) {
            hr = m_pSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                nullptr, m_pDecoderOutputType);
        }

        // ���NV12ʧ�ܣ�����YUY2
        if (FAILED(hr)) {
            std::cout << "NV12 failed, trying YUY2..." << std::endl;
            hr = m_pDecoderOutputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_YUY2);
            if (SUCCEEDED(hr)) {
                hr = m_pSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                    nullptr, m_pDecoderOutputType);
            }
        }

        // ���YUY2Ҳʧ�ܣ�����RGB32
        if (FAILED(hr)) {
            std::cout << "YUY2 failed, trying RGB32..." << std::endl;
            hr = m_pDecoderOutputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
            if (SUCCEEDED(hr)) {
                hr = m_pSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                    nullptr, m_pDecoderOutputType);
            }
        }

        if (FAILED(hr)) {
            std::cerr << "Failed to set any supported output format, HRESULT: 0x" << std::hex << hr << std::endl;
        }
        else {
            // ��ȡ��ȷ���������õ�ý������
            IMFMediaType* pCurrentType = nullptr;
            hr = m_pSourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pCurrentType);
            if (SUCCEEDED(hr)) {
                if (SUCCEEDED(pCurrentType->GetGUID(MF_MT_SUBTYPE, &m_outputFormat))) {
                    std::cout << "Successfully set output format: ";
                    if (m_outputFormat == MFVideoFormat_NV12) {
                        std::cout << "NV12" << std::endl;
                    }
                    else if (m_outputFormat == MFVideoFormat_YUY2) {
                        std::cout << "YUY2" << std::endl;
                    }
                    else if (m_outputFormat == MFVideoFormat_RGB32) {
                        std::cout << "RGB32" << std::endl;
                    }
                    else {
                        std::cout << "Unknown format" << std::endl;
                    }
                }
                pCurrentType->Release();
            }
        }

        return hr;
    }

    std::vector<cv::Mat> DecodeAllFrames() {
        std::vector<cv::Mat> frames;

        if (!m_initialized) {
            std::cerr << "Decoder not initialized" << std::endl;
            return frames;
        }

        HRESULT hr = S_OK;
        DWORD streamFlags = 0;
        LONGLONG timeStamp = 0;
        IMFSample* pSample = nullptr;

        // ��ȡ��Ƶ��Ϣ
        IMFMediaType* pMediaType = nullptr;
        hr = m_pSourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pMediaType);
        if (FAILED(hr)) {
            std::cerr << "Failed to get media type" << std::endl;
            return frames;
        }

        UINT32 width, height;
        hr = MFGetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, &width, &height);
        pMediaType->Release();

        if (FAILED(hr)) {
            std::cerr << "Failed to get frame size" << std::endl;
            return frames;
        }

        std::cout << "Video dimensions: " << width << "x" << height << std::endl;

        // ��������֡
        while (true) {
            hr = m_pSourceReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                0, nullptr, &streamFlags, &timeStamp, &pSample);

            if (FAILED(hr)) {
                std::cerr << "Failed to read sample" << std::endl;
                break;
            }

            if (streamFlags & MF_SOURCE_READERF_ENDOFSTREAM) {
                std::cout << "End of stream reached" << std::endl;
                break;
            }

            if (pSample) {
                cv::Mat frame = ConvertSampleToMat(pSample, width, height);
                if (!frame.empty()) {
                    frames.push_back(frame);
                }
                pSample->Release();
                pSample = nullptr;
            }
        }

        std::cout << "Decoded " << frames.size() << " frames" << std::endl;
        return frames;
    }

private:
    cv::Mat ConvertSampleToMat(IMFSample* pSample, UINT32 width, UINT32 height) {
        HRESULT hr = S_OK;
        IMFMediaBuffer* pBuffer = nullptr;

        hr = pSample->ConvertToContiguousBuffer(&pBuffer);
        if (FAILED(hr)) {
            std::cerr << "Failed to convert to contiguous buffer" << std::endl;
            return cv::Mat();
        }

        BYTE* pData = nullptr;
        DWORD maxLength = 0, currentLength = 0;

        hr = pBuffer->Lock(&pData, &maxLength, &currentLength);
        if (FAILED(hr)) {
            pBuffer->Release();
            std::cerr << "Failed to lock buffer" << std::endl;
            return cv::Mat();
        }

        cv::Mat result;

        // ���ݱ���ĸ�ʽ����ת��
        if (m_outputFormat == MFVideoFormat_NV12) {
            // NV12��ʽ��Yƽ�� + UV����ƽ��
            cv::Mat yuvFrame(height * 3 / 2, width, CV_8UC1, pData);
            cv::cvtColor(yuvFrame, result, cv::COLOR_YUV2BGR_NV12);
        }
        else if (m_outputFormat == MFVideoFormat_YUY2) {
            // YUY2��ʽ��YUYV����
            cv::Mat yuvFrame(height, width, CV_8UC2, pData);
            cv::cvtColor(yuvFrame, result, cv::COLOR_YUV2BGR_YUY2);
        }
        else if (m_outputFormat == MFVideoFormat_RGB32) {
            // RGB32��ʽ��BGRA
            cv::Mat bgraFrame(height, width, CV_8UC4, pData);
            cv::cvtColor(bgraFrame, result, cv::COLOR_BGRA2BGR);
            // ������Ҫ��ֱ��ת
            cv::flip(result, result, 0);
        }
        else {
            std::cerr << "Unsupported format for conversion" << std::endl;
        }

        // ������������Ϊԭʼ���ݽ����ͷ�
        cv::Mat finalResult = result.clone();

        pBuffer->Unlock();
        pBuffer->Release();

        return finalResult;
    }

    void Cleanup() {
        if (m_pDecoderOutputType) {
            m_pDecoderOutputType->Release();
            m_pDecoderOutputType = nullptr;
        }

        if (m_pSourceReader) {
            m_pSourceReader->Release();
            m_pSourceReader = nullptr;
        }

        if (m_initialized) {
            MFShutdown();
            m_initialized = false;
        }
    }
};


class VideoDecoder {
public:
    // ʹ��ʾ��
    static std::vector<ImageNode> DecodeVideoFrames(const std::vector<uint8_t>& videoFileBuf) {
        H264VideoDecoder decoder;

        HRESULT hr = decoder.Initialize(videoFileBuf);
        if (FAILED(hr)) {
            std::cerr << "Failed to initialize decoder, HRESULT: 0x" << std::hex << hr << std::endl;
            return {};
        }

        std::vector<ImageNode> ret;
        for (auto& frame : decoder.DecodeAllFrames())
            ret.push_back({ frame, 33 });
        return ret;
    }
};
