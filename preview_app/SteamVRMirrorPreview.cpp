#include <windows.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>

#include <openvr.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>
#include <string>

using Microsoft::WRL::ComPtr;

namespace
{
    constexpr vr::EVREye PreviewEye = vr::Eye_Left;
    constexpr wchar_t WindowClassName[] = L"SteamVRMirrorPreviewWindow"; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
    constexpr wchar_t WindowTitle[] = L"SteamVR Mirror Preview";  // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
    constexpr UINT InitialClientWidth = 1280;
    constexpr UINT InitialClientHeight = 720;
    constexpr DWORD SleepMs = 16;
    constexpr float ClearR = 0.02f;
    constexpr float ClearG = 0.02f;
    constexpr float ClearB = 0.025f;
    constexpr float Half = 0.5f;

    std::wstring ToWide(const char *text)
    {
        if (text == nullptr || text[0] == '\0') {
            return L"";
        }

        int length = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
        if (length <= 0) {
            return L"";
        }

        std::wstring wide(static_cast<size_t>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text, -1, wide.data(), length);
        if (!wide.empty() && wide.back() == L'\0') {
            wide.pop_back();
        }
        return wide;
    }

    std::wstring HrToString(HRESULT hr)
    {
        wchar_t *buffer = nullptr;
        const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
        DWORD length = FormatMessageW(flags, nullptr, static_cast<DWORD>(hr), 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        std::wstring message = length > 0 && buffer != nullptr ? std::wstring(buffer, length) : L"Unknown error";
        if (buffer != nullptr) {
            LocalFree(buffer);
        }

        while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n')) {
            message.pop_back();
        }
        return message;
    }

    std::wstring CompositorErrorToString(vr::EVRCompositorError error)
    {
        switch (error) {
        case vr::VRCompositorError_None:
            return L"OK";
        case vr::VRCompositorError_RequestFailed:
            return L"RequestFailed";
        case vr::VRCompositorError_IncompatibleVersion:
            return L"IncompatibleVersion";
        case vr::VRCompositorError_DoNotHaveFocus:
            return L"DoNotHaveFocus";
        case vr::VRCompositorError_InvalidTexture:
            return L"InvalidTexture";
        case vr::VRCompositorError_IsNotSceneApplication:
            return L"IsNotSceneApplication";
        case vr::VRCompositorError_TextureIsOnWrongDevice:
            return L"TextureIsOnWrongDevice";
        case vr::VRCompositorError_TextureUsesUnsupportedFormat:
            return L"TextureUsesUnsupportedFormat";
        case vr::VRCompositorError_SharedTexturesNotSupported:
            return L"SharedTexturesNotSupported";
        case vr::VRCompositorError_IndexOutOfRange:
            return L"IndexOutOfRange";
        case vr::VRCompositorError_AlreadySubmitted:
            return L"AlreadySubmitted";
        case vr::VRCompositorError_InvalidBounds:
            return L"InvalidBounds";
        default:
            return L"Unknown compositor error";
        }
    }

    class PreviewApp
    {
    public:
        int Run(HINSTANCE instance, int showCommand)
        {
            instance_ = instance;

            if (!CreateMainWindow(showCommand)) {
                return 1;
            }

            if (!InitializeOpenVR()) {
                Shutdown();
                return 1;
            }

            if (!InitializeD3D()) {
                Shutdown();
                return 1;
            }

            UpdateWindowTitle(L"Waiting for a scene app to submit frames...");

            MSG message = {};
            while (running_) {
                while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                    if (message.message == WM_QUIT) {
                        running_ = false;
                        break;
                    }

                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }

                if (!running_) {
                    break;
                }

                if (is_minimized_) {
                    Sleep(SleepMs);
                    continue;
                }

                RenderFrame();
            }

            Shutdown();
            return 0;
        }

    private:
        bool CreateMainWindow(int showCommand)
        {
            WNDCLASSEXW windowClass = {};
            windowClass.cbSize = sizeof(windowClass);
            windowClass.lpfnWndProc = &PreviewApp::StaticWindowProc;
            windowClass.hInstance = instance_;
            windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            windowClass.lpszClassName = WindowClassName;

            if (RegisterClassExW(&windowClass) == 0) {
                ShowFatalError(L"Failed to register window class.");
                return false;
            }

            RECT windowRect = { 0, 0, static_cast<LONG>(InitialClientWidth), static_cast<LONG>(InitialClientHeight) };
            AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

            hwnd_ = CreateWindowExW(
                0,
                WindowClassName,
                WindowTitle,
                WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                windowRect.right - windowRect.left,
                windowRect.bottom - windowRect.top,
                nullptr,
                nullptr,
                instance_,
                this);

            if (hwnd_ == nullptr) {
                ShowFatalError(L"Failed to create preview window.");
                return false;
            }

            ShowWindow(hwnd_, showCommand);
            UpdateWindow(hwnd_);
            return true;
        }

        bool InitializeOpenVR()
        {
            vr::EVRInitError initError = vr::VRInitError_None;
            vr_system_ = vr::VR_Init(&initError, vr::VRApplication_Background);
            if (initError != vr::VRInitError_None || vr_system_ == nullptr) {
                std::wstring message = L"OpenVR initialization failed:\n";
                message += ToWide(vr::VR_GetVRInitErrorAsEnglishDescription(initError));
                ShowFatalError(message);
                return false;
            }

            vr_compositor_ = vr::VRCompositor();
            if (vr_compositor_ == nullptr) {
                ShowFatalError(L"SteamVR compositor is not available.");
                return false;
            }

            return true;
        }

        bool InitializeD3D()
        {
            HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory_));
            if (FAILED(hr)) {
                ShowFatalError(L"CreateDXGIFactory2 failed:\n" + HrToString(hr));
                return false;
            }

            ComPtr<IDXGIAdapter1> adapter;
            int32_t adapterIndex = -1;
            vr_system_->GetDXGIOutputInfo(&adapterIndex);
            if (adapterIndex >= 0) {
                hr = factory_->EnumAdapters1(static_cast<UINT>(adapterIndex), &adapter);
                if (FAILED(hr)) {
                    ShowFatalError(L"Failed to locate the graphics adapter SteamVR is using:\n" + HrToString(hr));
                    return false;
                }
            }

            UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
            const D3D_FEATURE_LEVEL requestedLevels[] = { // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
                D3D_FEATURE_LEVEL_11_1,
                D3D_FEATURE_LEVEL_11_0,
            };

            D3D_FEATURE_LEVEL createdLevel = D3D_FEATURE_LEVEL_11_0;
            hr = D3D11CreateDevice(
                adapter.Get(),
                adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
                nullptr,
                creationFlags,
                requestedLevels,
                static_cast<UINT>(std::size(requestedLevels)),
                D3D11_SDK_VERSION,
                &device_,
                &createdLevel,
                &context_);

            if (FAILED(hr)) {
                ShowFatalError(L"D3D11CreateDevice failed:\n" + HrToString(hr));
                return false;
            }

            DetectTearingSupport();

            if (!CreateSwapChainResources(InitialClientWidth, InitialClientHeight)) {
                return false;
            }

            if (!CreateShaders()) {
                return false;
            }

            return CreateSampler();
        }

        bool CreateSwapChainResources(UINT width, UINT height)
        {
            width = std::max(width, 1u);
            height = std::max(height, 1u);

            if (!swap_chain_) {
                DXGI_SWAP_CHAIN_DESC1 desc = {};
                desc.Width = width;
                desc.Height = height;
                desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                desc.SampleDesc.Count = 1;
                desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                desc.BufferCount = 2;
                desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
                desc.Scaling = DXGI_SCALING_STRETCH;
                desc.Flags = SwapChainFlags();

                HRESULT hr = factory_->CreateSwapChainForHwnd(device_.Get(), hwnd_, &desc, nullptr, nullptr, &swap_chain_);
                if (FAILED(hr)) {
                    ShowFatalError(L"CreateSwapChainForHwnd failed:\n" + HrToString(hr));
                    return false;
                }

                factory_->MakeWindowAssociation(hwnd_, DXGI_MWA_NO_ALT_ENTER);
                ConfigureSwapChainLatency();
            } else {
                context_->OMSetRenderTargets(0, nullptr, nullptr);
                backbuffer_rtv_.Reset();
                context_->Flush();

                HRESULT hr = swap_chain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, SwapChainFlags());
                if (FAILED(hr)) {
                    ShowFatalError(L"ResizeBuffers failed:\n" + HrToString(hr));
                    return false;
                }
            }

            ComPtr<ID3D11Texture2D> backbuffer;
            HRESULT hr = swap_chain_->GetBuffer(0, IID_PPV_ARGS(&backbuffer));
            if (FAILED(hr)) {
                ShowFatalError(L"Failed to get the swap chain backbuffer:\n" + HrToString(hr));
                return false;
            }

            hr = device_->CreateRenderTargetView(backbuffer.Get(), nullptr, &backbuffer_rtv_);
            if (FAILED(hr)) {
                ShowFatalError(L"CreateRenderTargetView failed:\n" + HrToString(hr));
                return false;
            }

            client_width_ = width;
            client_height_ = height;
            return true;
        }

        void DetectTearingSupport()
        {
            ComPtr<IDXGIFactory5> factory5;
            if (FAILED(factory_.As(&factory5))) {
                return;
            }

            BOOL allowTearing = FALSE;
            if (SUCCEEDED(factory5->CheckFeatureSupport(
                    DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                    &allowTearing,
                    sizeof(allowTearing)))) {
                allow_tearing_ = allowTearing == TRUE;
            }
        }

        [[nodiscard]] UINT SwapChainFlags() const
        {
            return allow_tearing_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
        }

        void ConfigureSwapChainLatency()
        {
            ComPtr<IDXGISwapChain2> swapChain2;
            if (SUCCEEDED(swap_chain_.As(&swapChain2))) {
                swapChain2->SetMaximumFrameLatency(1);
            }
        }

        bool CreateShaders()
        {
            static constexpr char VertexShaderSource[] = // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
                "struct VSOut {\n"
                "    float4 position : SV_Position;\n"
                "    float2 uv : TEXCOORD0;\n"
                "};\n"
                "VSOut main(uint vertexId : SV_VertexID) {\n"
                "    float2 positions[4] = {\n"
                "        float2(-1.0f, -1.0f),\n"
                "        float2(-1.0f,  1.0f),\n"
                "        float2( 1.0f, -1.0f),\n"
                "        float2( 1.0f,  1.0f)\n"
                "    };\n"
                "    float2 uvs[4] = {\n"
                "        float2(0.0f, 1.0f),\n"
                "        float2(0.0f, 0.0f),\n"
                "        float2(1.0f, 1.0f),\n"
                "        float2(1.0f, 0.0f)\n"
                "    };\n"
                "    VSOut output;\n"
                "    output.position = float4(positions[vertexId], 0.0f, 1.0f);\n"
                "    output.uv = uvs[vertexId];\n"
                "    return output;\n"
                "}\n";

            static constexpr char PixelShaderSource[] = // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
                "Texture2D mirrorTexture : register(t0);\n"
                "SamplerState mirrorSampler : register(s0);\n"
                "struct VSOut {\n"
                "    float4 position : SV_Position;\n"
                "    float2 uv : TEXCOORD0;\n"
                "};\n"
                "float4 main(VSOut input) : SV_Target {\n"
                "    float4 color = mirrorTexture.Sample(mirrorSampler, input.uv);\n"
                "    color.rgb = pow(saturate(color.rgb), 1.0f / 2.2f);\n"
                "    color.a = 1.0f;\n"
                "    return color;\n"
                "}\n";

            ComPtr<ID3DBlob> vertexBlob;
            if (!CompileShader(VertexShaderSource, "main", "vs_5_0", &vertexBlob)) {
                return false;
            }

            HRESULT hr = device_->CreateVertexShader(
                vertexBlob->GetBufferPointer(),
                vertexBlob->GetBufferSize(),
                nullptr,
                &vertex_shader_);
            if (FAILED(hr)) {
                ShowFatalError(L"CreateVertexShader failed:\n" + HrToString(hr));
                return false;
            }

            ComPtr<ID3DBlob> pixelBlob;
            if (!CompileShader(PixelShaderSource, "main", "ps_5_0", &pixelBlob)) {
                return false;
            }

            hr = device_->CreatePixelShader(
                pixelBlob->GetBufferPointer(),
                pixelBlob->GetBufferSize(),
                nullptr,
                &pixel_shader_);
            if (FAILED(hr)) {
                ShowFatalError(L"CreatePixelShader failed:\n" + HrToString(hr));
                return false;
            }

            return true;
        }

        bool CompileShader(const char *source, const char *entryPoint, const char *target, ID3DBlob **outBlob)
        {
            UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
            ComPtr<ID3DBlob> shaderBlob;
            ComPtr<ID3DBlob> errorBlob;
            HRESULT hr = D3DCompile(
                source,
                strlen(source),
                nullptr,
                nullptr,
                nullptr,
                entryPoint,
                target,
                flags,
                0,
                &shaderBlob,
                &errorBlob);

            if (FAILED(hr)) {
                std::wstring message = L"Shader compilation failed.";
                if (errorBlob) {
                    message += L"\n";
                    message += ToWide(static_cast<const char *>(errorBlob->GetBufferPointer()));
                }
                ShowFatalError(message);
                return false;
            }

            *outBlob = shaderBlob.Detach();
            return true;
        }

        bool CreateSampler()
        {
            D3D11_SAMPLER_DESC samplerDesc = {}; // NOLINT(bugprone-invalid-enum-default-initialization)
            samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
            samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
            samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

            HRESULT hr = device_->CreateSamplerState(&samplerDesc, &sampler_state_);
            if (FAILED(hr)) {
                ShowFatalError(L"CreateSamplerState failed:\n" + HrToString(hr));
                return false;
            }

            return true;
        }

        void RenderFrame()
        {
            if (!backbuffer_rtv_) {
                return;
            }

            const float clearColor[4] = { ClearR, ClearG, ClearB, 1.0f }; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
            context_->OMSetRenderTargets(1, backbuffer_rtv_.GetAddressOf(), nullptr);
            context_->ClearRenderTargetView(backbuffer_rtv_.Get(), clearColor);

            vr::EVRCompositorError mirrorError = EnsureMirrorTexture();
            if (mirrorError == vr::VRCompositorError_None && mirror_srv_ != nullptr) {
                RenderMirrorTexture(mirror_srv_);

                if (last_status_ != L"streaming") {
                    UpdateWindowTitle(
                        std::to_wstring(mirror_width_) +
                        L"x" +
                        std::to_wstring(mirror_height_));
                    last_status_ = L"streaming";
                }
            } else {
                std::wstring status = mirrorError == vr::VRCompositorError_None
                    ? L"waiting-for-frames"
                    : L"mirror-error:" + CompositorErrorToString(mirrorError);
                if (last_status_ != status) {
                    UpdateWindowTitle(
                        mirrorError == vr::VRCompositorError_None
                            ? L"Waiting for a scene app to submit frames..."
                            : CompositorErrorToString(mirrorError));
                    last_status_ = status;
                }
            }

            swap_chain_->Present(0, allow_tearing_ ? DXGI_PRESENT_ALLOW_TEARING : 0);
            vr_compositor_->PostPresentHandoff();
        }

        vr::EVRCompositorError EnsureMirrorTexture()
        {
            if (mirror_srv_ != nullptr) {
                return vr::VRCompositorError_None;
            }

            return vr_compositor_->GetMirrorTextureD3D11(
                PreviewEye,
                device_.Get(),
                reinterpret_cast<void **>(&mirror_srv_)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        }

        void ReleaseMirrorTexture()
        {
            if (mirror_srv_ != nullptr && vr_compositor_ != nullptr) {
                vr_compositor_->ReleaseMirrorTextureD3D11(mirror_srv_);
                mirror_srv_ = nullptr;
            }
        }

        void RenderMirrorTexture(ID3D11ShaderResourceView *mirrorSrv)
        {
            ComPtr<ID3D11Resource> resource;
            mirrorSrv->GetResource(&resource);

            ComPtr<ID3D11Texture2D> texture;
            if (FAILED(resource.As(&texture))) {
                return;
            }

            D3D11_TEXTURE2D_DESC textureDesc = {};
            texture->GetDesc(&textureDesc);
            mirror_width_ = textureDesc.Width;
            mirror_height_ = textureDesc.Height;

            D3D11_VIEWPORT viewport = CalculateViewport(client_width_, client_height_, mirror_width_, mirror_height_);

            context_->RSSetViewports(1, &viewport);
            context_->IASetInputLayout(nullptr);
            context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            context_->VSSetShader(vertex_shader_.Get(), nullptr, 0);
            context_->PSSetShader(pixel_shader_.Get(), nullptr, 0);
            context_->PSSetSamplers(0, 1, sampler_state_.GetAddressOf());
            context_->PSSetShaderResources(0, 1, &mirrorSrv);
            context_->Draw(4, 0);

            ID3D11ShaderResourceView *nullSrv = nullptr;
            context_->PSSetShaderResources(0, 1, &nullSrv);
        }

        [[nodiscard]] static D3D11_VIEWPORT CalculateViewport(UINT targetWidth, UINT targetHeight, UINT sourceWidth, UINT sourceHeight)
        {
            D3D11_VIEWPORT viewport = {};
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 1.0f;

            if (targetWidth == 0 || targetHeight == 0 || sourceWidth == 0 || sourceHeight == 0) {
                viewport.Width = static_cast<FLOAT>(std::max(targetWidth, 1u));
                viewport.Height = static_cast<FLOAT>(std::max(targetHeight, 1u));
                return viewport;
            }

            const float targetAspect = static_cast<float>(targetWidth) / static_cast<float>(targetHeight);
            const float sourceAspect = static_cast<float>(sourceWidth) / static_cast<float>(sourceHeight);

            if (targetAspect > sourceAspect) {
                viewport.Height = static_cast<FLOAT>(targetHeight);
                viewport.Width = viewport.Height * sourceAspect;
                viewport.TopLeftX = (static_cast<FLOAT>(targetWidth) - viewport.Width) * Half;
                viewport.TopLeftY = 0.0f;
            } else {
                viewport.Width = static_cast<FLOAT>(targetWidth);
                viewport.Height = viewport.Width / sourceAspect;
                viewport.TopLeftX = 0.0f;
                viewport.TopLeftY = (static_cast<FLOAT>(targetHeight) - viewport.Height) * Half;
            }

            return viewport;
        }

        void Resize(UINT width, UINT height)
        {
            if (!swap_chain_ || width == 0 || height == 0) {
                return;
            }

            CreateSwapChainResources(width, height);
        }

        void Shutdown()
        {
            ReleaseMirrorTexture();

            sampler_state_.Reset();
            pixel_shader_.Reset();
            vertex_shader_.Reset();
            backbuffer_rtv_.Reset();
            swap_chain_.Reset();
            context_.Reset();
            device_.Reset();
            factory_.Reset();

            if (vr_system_ != nullptr) {
                vr::VR_Shutdown();
                vr_system_ = nullptr;
                vr_compositor_ = nullptr;
            }

            if (hwnd_ != nullptr) {
                DestroyWindow(hwnd_);
                hwnd_ = nullptr;
            }

            UnregisterClassW(WindowClassName, instance_);
        }

        void ShowFatalError(const std::wstring &message)
        {
            MessageBoxW(hwnd_, message.c_str(), WindowTitle, MB_ICONERROR | MB_OK);
        }

        void UpdateWindowTitle(const std::wstring &suffix)
        {
            std::wstring title = suffix.rfind(WindowTitle, 0) == 0 ? suffix : std::wstring(WindowTitle) + L" - " + suffix;
            SetWindowTextW(hwnd_, title.c_str());
        }

        LRESULT WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
        {
            switch (message) {
            case WM_SIZE:
                is_minimized_ = (wParam == SIZE_MINIMIZED);
                if (!is_minimized_) {
                    Resize(LOWORD(lParam), HIWORD(lParam));
                }
                return 0;

            case WM_KEYDOWN:
                if (wParam == VK_ESCAPE) {
                    DestroyWindow(hwnd);
                }
                return 0;

            case WM_DESTROY:
                running_ = false;
                hwnd_ = nullptr;
                PostQuitMessage(0);
                return 0;

            default:
                return DefWindowProcW(hwnd, message, wParam, lParam);
            }
        }

        static LRESULT CALLBACK StaticWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
        {
            PreviewApp *app = nullptr;
            if (message == WM_NCCREATE) {
                auto *createStruct = reinterpret_cast<CREATESTRUCTW *>(lParam); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,performance-no-int-to-ptr)
                app = static_cast<PreviewApp *>(createStruct->lpCreateParams);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                app->hwnd_ = hwnd;
            } else {
                app = reinterpret_cast<PreviewApp *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,performance-no-int-to-ptr)
            }

            if (app != nullptr) {
                return app->WindowProc(hwnd, message, wParam, lParam);
            }

            return DefWindowProcW(hwnd, message, wParam, lParam);
        }

        HINSTANCE instance_ = nullptr;
        HWND hwnd_ = nullptr;
        bool running_ = true;
        bool is_minimized_ = false;
        UINT client_width_ = InitialClientWidth;
        UINT client_height_ = InitialClientHeight;
        UINT mirror_width_ = 0;
        UINT mirror_height_ = 0;
        bool allow_tearing_ = false;
        std::wstring last_status_;

        vr::IVRSystem *vr_system_ = nullptr;
        vr::IVRCompositor *vr_compositor_ = nullptr;
        ID3D11ShaderResourceView *mirror_srv_ = nullptr;

        ComPtr<IDXGIFactory2> factory_;
        ComPtr<ID3D11Device> device_;
        ComPtr<ID3D11DeviceContext> context_;
        ComPtr<IDXGISwapChain1> swap_chain_;
        ComPtr<ID3D11RenderTargetView> backbuffer_rtv_;
        ComPtr<ID3D11VertexShader> vertex_shader_;
        ComPtr<ID3D11PixelShader> pixel_shader_;
        ComPtr<ID3D11SamplerState> sampler_state_;
    };
}

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE prevInstance, _In_ PWSTR cmdLine, _In_ int showCommand) // NOLINT(bugprone-easily-swappable-parameters,readability-non-const-parameter)
{
    (void)prevInstance;
    (void)cmdLine;
    PreviewApp app;
    return app.Run(instance, showCommand);
}
