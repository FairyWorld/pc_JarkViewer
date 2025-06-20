#pragma once

#include "Utils.h"

// ȫ�ֱ����洢UI״̬
struct PrintParams {
    //double topMargin = 5.0;       // �ϱ߾�ٷֱ�
    //double bottomMargin = 5.0;    // �±߾�ٷֱ�
    //double leftMargin = 5.0;      // ��߾�ٷֱ�
    //double rightMargin = 5.0;     // �ұ߾�ٷֱ�
    //int layoutMode = 0;           // 0=��Ӧ, 1=���, 2=ԭʼ����
    int brightness = 100;         // ���ȵ��� (0 ~ 200)
    int contrast = 100;           // �Աȶȵ��� (0 ~ 200)
    bool grayscale = true;        // �Ƿ�ڰ״�ӡ
    bool invertColors = false;    // �Ƿ���

    bool isParamsChange = false;
    bool confirmed = false;       // �û����ȷ��
    cv::Mat previewImage;         // Ԥ��ͼ��
};

class Printer {
private:
    const char* windowsName = "Print Preview";
    PrintParams params{};
    cv::Mat buttonUI;
    SettingParameter *settingParameter = nullptr;

    void Init() {
        auto rc = Utils::GetResource(IDB_PNG7, L"PNG");
        buttonUI = cv::imdecode(cv::Mat(1, (int)rc.size, CV_8UC1, (uint8_t*)rc.addr), cv::IMREAD_UNCHANGED);

        params.brightness = settingParameter->printerBrightness;
        params.contrast = settingParameter->printerContrast;
        params.grayscale = settingParameter->printerGrayscale;
        params.invertColors = settingParameter->printerInvertColors;

        // �쳣�����ָ�Ĭ��ֵ
        if (params.brightness < 0 || params.brightness > 200 || params.contrast < 0 || params.brightness > 200) {
            params.brightness = 100;
            params.contrast = 100;
            params.grayscale = true;
            params.invertColors = false;
        }
    }

public:
    Printer(SettingParameter* settingParameter) : settingParameter(settingParameter) {
        Init();
    }

    Printer(const cv::Mat& image, SettingParameter* settingParameter) : settingParameter(settingParameter) {
        Init();
        PrintMatImage(image);
    }

    ~Printer() {
        settingParameter->printerBrightness = params.brightness;
        settingParameter->printerContrast = params.contrast;
        settingParameter->printerGrayscale = params.grayscale;
        settingParameter->printerInvertColors = params.invertColors;
    }

    // �Ҷ�/BGR/BGRAͳһ��BGR
    cv::Mat matToBGR(const cv::Mat& image) {
        if (image.empty())
            return cv::Mat();

        cv::Mat bgrMat;
        if (image.channels() == 1) {
            cv::cvtColor(image, bgrMat, cv::COLOR_GRAY2BGR);
        }
        else if (image.channels() == 3) {
            bgrMat = image.clone();
        }
        else if (image.channels() == 4) {
            // Alpha��ϰ�ɫ���� (255, 255, 255)
            const int width = image.cols;
            const int height = image.rows;
            bgrMat = cv::Mat(height, width, CV_8UC3);
            for (int y = 0; y < height; y++) {
                const BYTE* srcRow = image.ptr<BYTE>(y);
                BYTE* desRow = bgrMat.ptr<BYTE>(y);

                for (int x = 0; x < width; x++) {
                    BYTE b = srcRow[x * 4];
                    BYTE g = srcRow[x * 4 + 1];
                    BYTE r = srcRow[x * 4 + 2];
                    BYTE a = srcRow[x * 4 + 3];

                    if (a == 0) {
                        desRow[x * 3] = 255;
                        desRow[x * 3 + 1] = 255;
                        desRow[x * 3 + 2] = 255;
                    }
                    else if (a == 255) {
                        desRow[x * 3] = b;
                        desRow[x * 3 + 1] = g;
                        desRow[x * 3 + 2] = r;
                    }
                    else {
                        desRow[x * 3] = static_cast<BYTE>((b * a + 255 * (255 - a) + 255) >> 8);     // B
                        desRow[x * 3 + 1] = static_cast<BYTE>((g * a + 255 * (255 - a) + 255) >> 8); // G
                        desRow[x * 3 + 2] = static_cast<BYTE>((r * a + 255 * (255 - a) + 255) >> 8); // R
                    }
                }
            }
        }

        return bgrMat;
    }


    // ��cv::Matת��ΪHBITMAP
    HBITMAP MatToHBITMAP(const cv::Mat& image) {
        if (image.empty()) {
            MessageBoxW(nullptr, L"MatToHBITMAPת��ͼ�����: ��ͼ��", L"����", MB_OK | MB_ICONERROR);
            return nullptr;
        }
        if (image.type() != CV_8UC3) {
            MessageBoxW(nullptr, L"MatToHBITMAPת��ͼ�����: ֻ����BGR/CV_8UC3����ͼ��", L"����", MB_OK | MB_ICONERROR);
            return nullptr;
        }

        const int width = image.cols;
        const int height = image.rows;

        const int stride = (width * 3 + 3) & ~3;  // 4�ֽڶ���
        const size_t dataSize = static_cast<size_t>(stride) * height;

        // ׼��BITMAPINFO�ṹ
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = height;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 24;          // 24λRGB
        bmi.bmiHeader.biCompression = BI_RGB;
        bmi.bmiHeader.biSizeImage = dataSize;

        // ����DIB����������
        HDC hdcScreen = GetDC(nullptr);
        void* pBits;
        HBITMAP hBitmap = CreateDIBSection(
            hdcScreen, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
        ReleaseDC(nullptr, hdcScreen);

        if (hBitmap) {
            for (int y = 0; y < height; y++) {
                const BYTE* srcRow = image.ptr<BYTE>(height - 1 - y);
                BYTE* dstRow = static_cast<BYTE*>(pBits) + y * stride;
                memcpy(dstRow, srcRow, stride);
            }
        }

        return hBitmap;
    }

    void adjustBrightnessContrast(cv::Mat& src, int brightnessInt, int contrastInt) {
        if (src.empty() || src.type() != CV_8UC3)
            return;

        if (brightnessInt < 1)
            brightnessInt = 1;
        else if (brightnessInt > 199)
            brightnessInt = 199;

        if (contrastInt < 0)
            contrastInt = 0;
        else if (contrastInt > 200)
            contrastInt = 200;

        // brightness: 0 ~ 200 ӳ�䵽 0 ~ 2.0
        // contrast:   0 ~ 200 ӳ�䵽 0 ~ 2.0
        double brightness = brightnessInt / 100.0;
        double contrast = contrastInt / 100.0;

        contrast = pow(contrast, 2); // ����Աȶȱ���

        for (int y = 0; y < src.rows; y++) {
            for (int x = 0; x < src.cols; x++) {
                cv::Vec3b pixel = src.at<cv::Vec3b>(y, x);
                for (int c = 0; c < 3; c++) {
                    // ��128Ϊ���Ľ��жԱȶȵ���
                    double adjusted = (pixel[c] - 128.0) * contrast + 128.0;

                    // �������ƫ��
                    adjusted = pow(adjusted / 255.0, 2.0 - brightness) * 255;

                    // ȷ��ֵ��0-255��Χ��
                    pixel[c] = cv::saturate_cast<uchar>(adjusted);
                }
                src.at<cv::Vec3b>(y, x) = pixel;
            }
        }
    }

    // ͼ���� �����Աȶ� ��ɫ �ڰ�
    void ApplyImageAdjustments(cv::Mat& image, int brightness, int contrast, bool grayscale, bool invertColors) {
        if (image.empty()) return;

        if (invertColors) {
            cv::bitwise_not(image, image);
        }

        // brightness: 0 ~ 200, 100���м�ֵ�����Ȳ�������
        // contrast:   0 ~ 200, 100���м�ֵ���ԱȶȲ�������
        adjustBrightnessContrast(image, brightness, contrast);

        if (grayscale) {
            cv::Mat gray;
            cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
            cv::cvtColor(gray, image, cv::COLOR_GRAY2BGR);
        }
    }


    // OpenCV �ص�����
    void refreshPreview(void* userdata) {
        PrintParams* params = static_cast<PrintParams*>(userdata);

        cv::Mat adjusted = params->previewImage.clone();
        ApplyImageAdjustments(adjusted, params->brightness, params->contrast, params->grayscale, params->invertColors);

        // ��չΪ�����λ���
        int outputSize = std::max(adjusted.rows, adjusted.cols);
        int x = (outputSize - adjusted.cols) / 2;
        int y = (outputSize - adjusted.rows) / 2;

        cv::Mat squareMat(outputSize, outputSize, CV_8UC3, cv::Scalar(255, 255, 255));
        cv::Mat roi = squareMat(cv::Rect(x, y, adjusted.cols, adjusted.rows));
        adjusted.copyTo(roi);

        cv::cvtColor(squareMat, squareMat, CV_BGR2BGRA);
        Utils::overlayImg(squareMat, buttonUI, squareMat.cols - buttonUI.cols, squareMat.rows - buttonUI.rows);
        cv::imshow(windowsName, squareMat);
    }


    // ��ӡǰԤ����
    cv::Mat ImagePreprocessingForPrint(const cv::Mat& image) {
        if (image.empty() || image.type() != CV_8UC3) {
            return cv::Mat();
        }

        // ����Ԥ��ͼ��
        const int previewMaxSize = 600;

        double scale = (double)previewMaxSize / std::max(image.rows, image.cols);
        cv::resize(image, params.previewImage, cv::Size(), scale, scale);

        // ��������ܼ��ˣ������򳬿����ſ����쳣
        if (params.previewImage.empty() || params.previewImage.rows <= 0 || params.previewImage.cols <= 0) {
            return cv::Mat();
        }

        // ����UI����
        cv::namedWindow(windowsName, cv::WINDOW_AUTOSIZE);
        cv::resizeWindow(windowsName, previewMaxSize, static_cast<int>(previewMaxSize * 1.2));

        cv::setMouseCallback(windowsName, [](int event, int x, int y, int flags, void* userdata) {
            if (event == cv::EVENT_LBUTTONUP) {
                if ((300 < x) && (x < 400) && (550 < y)) { // �л� ��ɫ
                    PrintParams* params = static_cast<PrintParams*>(userdata);
                    params->invertColors = !params->invertColors;
                    params->isParamsChange = true;
                }
                if ((400 < x) && (x < 500) && (550 < y)) { // �л� ��ɫ/ �ڰ�
                    PrintParams* params = static_cast<PrintParams*>(userdata);
                    params->grayscale = !params->grayscale;
                    params->isParamsChange = true;
                }
                if ((500 < x) && (x < 600) && (550 < y)) { // ȷ����ť
                    PrintParams* params = static_cast<PrintParams*>(userdata);
                    params->confirmed = true;
                    params->isParamsChange = true;
                    cv::destroyWindow("Print Preview");
                }
            }
            }, &params);

        cv::createTrackbar("Brightness", windowsName, nullptr, 200, [](int value, void* userdata) {
            PrintParams* params = static_cast<PrintParams*>(userdata);
            params->brightness = value;
            params->isParamsChange = true;
            }, &params);
        cv::setTrackbarPos("Brightness", windowsName, params.brightness);

        cv::createTrackbar("Contrast", windowsName, nullptr, 200, [](int value, void* userdata) {
            PrintParams* params = static_cast<PrintParams*>(userdata);
            params->contrast = value;
            params->isParamsChange = true;
            }, &params);
        cv::setTrackbarPos("Contrast", windowsName, params.contrast);

        // ��ʼԤ��
        refreshPreview(&params);

        // �¼�ѭ��
        while (cv::getWindowProperty(windowsName, cv::WND_PROP_VISIBLE) > 0) {
            if (params.isParamsChange) {
                params.isParamsChange = false;
                refreshPreview(&params);
            }
            int key = cv::waitKey(30);
            if (params.confirmed) break;
        }

        // �û�ȡ������
        if (!params.confirmed) return cv::Mat();

        // Ӧ�ò�����ԭʼͼ��
        cv::Mat finalImage = image.clone();
        ApplyImageAdjustments(finalImage, params.brightness, params.contrast, params.grayscale, params.invertColors);
        return finalImage;
    }


    // ��ӡͼ����
    bool PrintMatImage(const cv::Mat& image) {
        auto bgrMat = matToBGR(image);

        if (bgrMat.empty()) {
            MessageBoxW(nullptr, L"ͼ��ת����BGR��������", L"����", MB_OK | MB_ICONERROR);
            return false;
        }

        if (!Utils::limitSizeTo16K(bgrMat)) {
            MessageBoxW(nullptr, L"����ͼ��ߴ緢������", L"����", MB_OK | MB_ICONERROR);
            return false;
        }

        auto adjustMat = ImagePreprocessingForPrint(bgrMat);

        if (adjustMat.empty()) { // ȡ����ӡ
            return true;
        }

        // ��ʼ����ӡ�Ի���
        PRINTDLGW pd{};
        pd.lStructSize = sizeof(pd);
        pd.Flags = PD_RETURNDC | PD_NOPAGENUMS | PD_NOSELECTION;

        // ��ʾ��ӡ�Ի���
        if (!PrintDlgW(&pd))
            return false;
        if (!pd.hDC)
            return false;

        // ׼���ĵ���Ϣ
        DOCINFOW di{};
        di.cbSize = sizeof(di);
        di.lpszDocName = L"JarkViewer Printed Image";
        di.lpszOutput = nullptr;

        // ��ʼ��ӡ��ҵ
        if (StartDocW(pd.hDC, &di) <= 0) {
            DeleteDC(pd.hDC);
            return false;
        }

        // ��ʼ��ҳ��
        if (StartPage(pd.hDC) <= 0) {
            EndDoc(pd.hDC);
            DeleteDC(pd.hDC);
            return false;
        }

        // ��ȡ��ӡ����ͼ��ߴ�
        int pageWidth = GetDeviceCaps(pd.hDC, HORZRES);
        int pageHeight = GetDeviceCaps(pd.hDC, VERTRES);

        HBITMAP hBitmap = MatToHBITMAP(adjustMat);
        if (!hBitmap) {
            return false;
        }

        BITMAP bm{};
        GetObjectW(hBitmap, sizeof(BITMAP), &bm);
        int imgWidth = bm.bmWidth;
        int imgHeight = bm.bmHeight;

        // �������ű��������ֿ�߱ȣ�
        double scale = std::min(
            static_cast<double>(pageWidth) / imgWidth,
            static_cast<double>(pageHeight) / imgHeight
        );

        scale *= (pageHeight > pageWidth ? 0.893 : 0.879); // ����׼A4�߾� 1.0-31.8/297  1.0-2.54/210

        int scaledWidth = static_cast<int>(imgWidth * scale);
        int scaledHeight = static_cast<int>(imgHeight * scale);

        // ����λ��
        int xPos = (pageWidth - scaledWidth) / 2;
        int yPos = (pageHeight - scaledHeight) / 2;

        // �����ڴ�DC
        HDC hdcMem = CreateCompatibleDC(pd.hDC);
        SelectObject(hdcMem, hBitmap);

        // ����������
        SetStretchBltMode(pd.hDC, HALFTONE);
        SetBrushOrgEx(pd.hDC, 0, 0, nullptr);

        // ���Ƶ���ӡ��DC
        StretchBlt(
            pd.hDC, xPos, yPos, scaledWidth, scaledHeight,
            hdcMem, 0, 0, imgWidth, imgHeight, SRCCOPY
        );

        // ������Դ
        DeleteDC(hdcMem);
        DeleteObject(hBitmap);

        // ����ҳ����ĵ�
        EndPage(pd.hDC);
        EndDoc(pd.hDC);
        DeleteDC(pd.hDC);

        return true;
    }

};