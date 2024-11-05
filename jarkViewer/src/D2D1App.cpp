#include "D2D1App.h"
#include <dwmapi.h>

D2D1App::D2D1App() 
{
    loadSettings();
    m_parameters.DirtyRectsCount = 0;
    m_parameters.pDirtyRects = nullptr;
    m_parameters.pScrollRect = nullptr;
    m_parameters.pScrollOffset = nullptr;
}

D2D1App::~D2D1App()
{
    saveSettings();
    this->DiscardDeviceResources();
    Utils::SafeRelease(m_pD2DFactory);
    Utils::SafeRelease(m_pWICFactory);
    Utils::SafeRelease(m_pDWriteFactory);
}

void D2D1App::loadSettings() {
    const int MAX_PATH_LEN = 512;
    TCHAR szPath[MAX_PATH_LEN];

    if (GetModuleFileName(NULL, szPath, MAX_PATH_LEN) != 0) {

        exePath = wstring(szPath);
        settingPath = exePath.substr(0, exePath.length() - 3) + L"db";

        auto f = _wfopen(settingPath.c_str(), L"rb");
        if (f) {
            SettingParameter tmp{ 0 };
            auto readLen = fread(&tmp, 1, sizeof(SettingParameter), f);
            fclose(f);

            if (readLen == sizeof(SettingParameter) && !memcmp(settingHeader.data(), tmp.header, settingHeader.length() + 1))
                settingPar = tmp;

            if (settingPar.showCmd == SW_NORMAL) {
                int screenWidth = (::GetSystemMetrics(SM_CXFULLSCREEN));
                int screenHeight = (::GetSystemMetrics(SM_CYFULLSCREEN));

                if (settingPar.rect.left > screenWidth || settingPar.rect.top > screenHeight ||
                    (settingPar.rect.right - settingPar.rect.left) > screenWidth ||
                    (settingPar.rect.bottom - settingPar.rect.top) > screenHeight) {
                    settingPar.rect = { 100, 100, 100 + screenWidth / 2, 100 + screenHeight / 2 };
                }
            }
        }
    }
}

void D2D1App::saveSettings() {
    WINDOWPLACEMENT wp;
    wp.length = sizeof(WINDOWPLACEMENT);

    // 获取窗口的显示状态和位置信息
    if (GetWindowPlacement(m_hWnd, &wp) && wp.showCmd == SW_NORMAL) {
        settingPar.showCmd = SW_NORMAL;
        settingPar.rect = wp.rcNormalPosition;
    }
    else {
        settingPar.showCmd = SW_MAXIMIZE;
        settingPar.rect = {};
    }

    memcpy(settingPar.header, settingHeader.data(), settingHeader.length() + 1);

    auto f = _wfopen(settingPath.c_str(), L"wb");
    if (f) {
        fwrite(&settingPar, 1, sizeof(SettingParameter), f);
        fclose(f);
    }
}

// 初始化
HRESULT D2D1App::Initialize(HINSTANCE hInstance)
{
    HRESULT hr = E_FAIL;
    //register window class
    WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = D2D1App::WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = sizeof(void*);
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wcex.hbrBackground = nullptr;
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = L"D2D1WndClass";
    wcex.hIcon = nullptr;
    // 注册窗口
    RegisterClassExW(&wcex);

    RECT window_rect = settingPar.showCmd == SW_NORMAL ? settingPar.rect : RECT{ 0, 0, 800, 600 };
    DWORD window_style = WS_OVERLAPPEDWINDOW;
    m_hWnd = CreateWindowExW(0, L"D2D1WndClass", m_wndCaption.c_str(), window_style,
        window_rect.left, window_rect.top, window_rect.right - window_rect.left, window_rect.bottom - window_rect.top,
        0, 0, hInstance, this);
    hr = m_hWnd ? S_OK : E_FAIL;
    
    // 显示窗口
    if (SUCCEEDED(hr))
    {
        CreateDeviceIndependentResources();
        CreateDeviceResources();

        DragAcceptFiles(m_hWnd, TRUE);

        bool isDarkMode = false;
        HKEY hKey;
        if (RegOpenKeyEx(HKEY_CURRENT_USER,
            TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
            0, KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            DWORD value = 0;
            DWORD size = sizeof(value);
            if (RegQueryValueEx(hKey, TEXT("AppsUseLightTheme"), NULL, NULL,
                reinterpret_cast<LPBYTE>(&value), &size) == ERROR_SUCCESS)
            {
                isDarkMode = (value == 0);
            }
            RegCloseKey(hKey);
        }

        BOOL useSystemTheme = isDarkMode;
        DwmSetWindowAttribute(m_hWnd, DWMWINDOWATTRIBUTE::DWMWA_USE_IMMERSIVE_DARK_MODE, &useSystemTheme, sizeof(useSystemTheme));

        ShowWindow(m_hWnd, settingPar.showCmd == SW_NORMAL ? SW_NORMAL : SW_MAXIMIZE);
        UpdateWindow(m_hWnd);
    }
    return hr;
}

HRESULT D2D1App::CreateDeviceIndependentResources()
{
    // 创建D2D工厂
    HRESULT hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory1),
        reinterpret_cast<void**>(&m_pD2DFactory));
    // 创建 WIC 工厂.
    if (SUCCEEDED(hr))
    {
        hr = CoCreateInstance(
            CLSID_WICImagingFactory2,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&m_pWICFactory));
    }
    // 创建 DirectWrite 工厂.
    if (SUCCEEDED(hr))
    {
        hr = DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(m_pDWriteFactory),
            reinterpret_cast<IUnknown **>(&m_pDWriteFactory));
    }

    return hr;
}

HRESULT D2D1App::CreateDeviceResources()
{
    // DXGI 工厂
    IDXGIFactory2*						pDxgiFactory = nullptr;
    // DXGI 设备
    IDXGIDevice1*						pDxgiDevice = nullptr;

    HRESULT hr = S_OK;


    // 创建 D3D11设备与设备上下文 
    if (SUCCEEDED(hr))
    {
        // D3D11 创建flag 
        // 一定要有D3D11_CREATE_DEVICE_BGRA_SUPPORT，否则创建D2D设备上下文会失败
        UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
        // Debug状态 有D3D DebugLayer就可以取消注释
        creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
            D3D_FEATURE_LEVEL_9_3,
            D3D_FEATURE_LEVEL_9_2,
            D3D_FEATURE_LEVEL_9_1
        };
        // 创建设备
        hr = D3D11CreateDevice(
            nullptr,					// 设为空指针选择默认设备
            D3D_DRIVER_TYPE_HARDWARE,	// 强行指定硬件渲染
            nullptr,					// 强行指定WARP渲染 D3D_DRIVER_TYPE_WARP 没有软件接口
            creationFlags,				// 创建flag
            featureLevels,				// 欲使用的特性等级列表
            ARRAYSIZE(featureLevels),	// 特性等级列表长度
            D3D11_SDK_VERSION,			// SDK 版本
            &m_pD3DDevice,				// 返回的D3D11设备指针
            &m_featureLevel,			// 返回的特性等级
            &m_pD3DDeviceContext);		// 返回的D3D11设备上下文指针
    }

    // 创建 IDXGIDevice
    if (SUCCEEDED(hr))
        hr = m_pD3DDevice->QueryInterface(IID_PPV_ARGS(&pDxgiDevice));
    // 创建D2D设备
    if (SUCCEEDED(hr))
        hr = m_pD2DFactory->CreateDevice(pDxgiDevice, &m_pD2DDevice);
    // 创建D2D设备上下文
    if (SUCCEEDED(hr))
        hr = m_pD2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_pD2DDeviceContext);

    Utils::SafeRelease(pDxgiDevice);
    Utils::SafeRelease(pDxgiFactory);

    CreateWindowSizeDependentResources();

    return hr;
}

void D2D1App::CreateWindowSizeDependentResources()
{
    // DXGI 适配器
    IDXGIAdapter*						pDxgiAdapter = nullptr;
    // DXGI 工厂
    IDXGIFactory2*						pDxgiFactory = nullptr;
    // DXGI Surface 后台缓冲
    IDXGISurface*						pDxgiBackBuffer = nullptr;
    // DXGI 设备
    IDXGIDevice1*						pDxgiDevice = nullptr;

    HRESULT hr = S_OK;

    // 清除之前窗口的呈现器相关设备
    m_pD2DDeviceContext->SetTarget(nullptr);
    Utils::SafeRelease(m_pD2DTargetBimtap);
    m_pD3DDeviceContext->Flush();

    RECT rect = { 0 }; GetClientRect(m_hWnd, &rect);

    if (m_pSwapChain != nullptr)
    {
        // 如果交换链已经创建，则重设缓冲区
        hr = m_pSwapChain->ResizeBuffers(
            2, // Double-buffered swap chain.
            lround(rect.right - rect.left),
            lround(rect.bottom - rect.top),
            DXGI_FORMAT_B8G8R8A8_UNORM,
            0);

        assert( hr == S_OK );
    }
    else
    {
        // 否则用已存在的D3D设备创建一个新的交换链
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };
        swapChainDesc.Width = lround(rect.right - rect.left);
        swapChainDesc.Height = lround(rect.bottom - rect.top);
        swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        swapChainDesc.Stereo = false;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = 2;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        swapChainDesc.Flags = 0;
        swapChainDesc.Scaling = DXGI_SCALING_NONE;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

        // 获取 IDXGIDevice
        if (SUCCEEDED(hr))
        {
            hr = m_pD3DDevice->QueryInterface(IID_PPV_ARGS(&pDxgiDevice));
        }
        // 获取Dxgi适配器 可以获取该适配器信息
        if (SUCCEEDED(hr))
        {
            hr = pDxgiDevice->GetAdapter(&pDxgiAdapter);
        }
        // 获取Dxgi工厂
        if (SUCCEEDED(hr))
        {
            hr = pDxgiAdapter->GetParent(IID_PPV_ARGS(&pDxgiFactory));
        }
        // 创建交换链
        if (SUCCEEDED(hr))
        {
            hr = pDxgiFactory->CreateSwapChainForHwnd(
                m_pD3DDevice,
                m_hWnd,
                &swapChainDesc,
                nullptr,
                nullptr,
                &m_pSwapChain);
        }
        // 确保DXGI队列里边不会超过一帧
        if (SUCCEEDED(hr))
        {
            hr = pDxgiDevice->SetMaximumFrameLatency(1);
        }
    }

    // 设置屏幕方向
    if (SUCCEEDED(hr))
    {
        hr = m_pSwapChain->SetRotation(DXGI_MODE_ROTATION_IDENTITY);
    }
    // 利用交换链获取Dxgi表面
    if (SUCCEEDED(hr))
    {
        hr = m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pDxgiBackBuffer));
    }
    // 利用Dxgi表面创建位图
    if (SUCCEEDED(hr))
    {
        D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            96.0f,
            96.0f);
        hr = m_pD2DDeviceContext->CreateBitmapFromDxgiSurface(
            pDxgiBackBuffer,
            &bitmapProperties,
            &m_pD2DTargetBimtap);
    }
    // 设置
    if (SUCCEEDED(hr))
    {
        // 设置 Direct2D 渲染目标
        m_pD2DDeviceContext->SetTarget(m_pD2DTargetBimtap);
    }

    Utils::SafeRelease(pDxgiDevice);
    Utils::SafeRelease(pDxgiAdapter);
    Utils::SafeRelease(pDxgiFactory);
    Utils::SafeRelease(pDxgiBackBuffer);
}

// 丢弃设备相关资源
void D2D1App::DiscardDeviceResources()
{
    Utils::SafeRelease(m_pD2DTargetBimtap);
    Utils::SafeRelease(m_pSwapChain);
    Utils::SafeRelease(m_pD2DDeviceContext);
    Utils::SafeRelease(m_pD2DDevice);
    Utils::SafeRelease(m_pD3DDevice);
    Utils::SafeRelease(m_pD3DDeviceContext);
}

void D2D1App::Run()
{
    while (m_fRunning)
    {
        // 如果接收到Window消息，则处理这些消息
        MSG msg;
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        // 否则，则运行动画/游戏
        else
        {
            DrawScene();
        }
    }
}

void D2D1App::OnResize(UINT width, UINT height)
{
    CreateWindowSizeDependentResources();
}

void D2D1App::OnDestroy()
{
    m_fRunning = FALSE;
}

LRESULT D2D1App::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;

    if (message == WM_CREATE)
    {
        LPCREATESTRUCT pcs = (LPCREATESTRUCT)lParam;
        D2D1App *pD2DApp = (D2D1App *)pcs->lpCreateParams;

        ::SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            (LONG_PTR)pD2DApp
            );

        result = 1;
    }
    else
    {
        static TRACKMOUSEEVENT tme = {
            .cbSize = sizeof(TRACKMOUSEEVENT),
            .dwFlags = TME_LEAVE,
        };

        D2D1App *pD2DApp = reinterpret_cast<D2D1App *>(static_cast<LONG_PTR>(
            ::GetWindowLongPtrW(
                hwnd,
                GWLP_USERDATA
                )));

        bool wasHandled = false;

        if (pD2DApp)
        {
            switch(message)
            {
            case WM_LBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_RBUTTONDOWN:
                pD2DApp->OnMouseDown(message, LOWORD(lParam), HIWORD(lParam));
                break;

            case WM_SIZE:
                pD2DApp->OnResize(LOWORD(lParam), HIWORD(lParam));
                break;

            case WM_LBUTTONUP:
            case WM_MBUTTONUP:
            case WM_RBUTTONUP:
                pD2DApp->OnMouseUp(message, LOWORD(lParam), HIWORD(lParam));
                break;

            case WM_MOUSEMOVE:
                if (!tme.hwndTrack) {
                    tme.hwndTrack = hwnd;
                    TrackMouseEvent(&tme);
                }
                pD2DApp->OnMouseMove(message, LOWORD(lParam), HIWORD(lParam));
                break;

            case WM_MOUSELEAVE:
                tme.hwndTrack = NULL;
                pD2DApp->OnMouseLeave();
                break;
            
            case WM_MOUSEWHEEL:
                pD2DApp->OnMouseWheel(LOWORD(wParam), HIWORD(wParam), LOWORD(lParam), HIWORD(lParam));
                break;

            case WM_KEYDOWN:
                pD2DApp->OnKeyDown(wParam);
                break;

            case WM_KEYUP:
                pD2DApp->OnKeyUp(wParam);
                break;

            case WM_DROPFILES:
                pD2DApp->OnDropFiles(wParam);
                break;

            case WM_DESTROY:
                {
                    pD2DApp->OnDestroy();
                    PostQuitMessage(0);
                }
                result = 1;
                wasHandled = true;
                break;
            }
        }

        if (!wasHandled)
        {
            result = DefWindowProc(hwnd, message, wParam, lParam);
        }
    }

    return result;
}