#include <windows.h>
#include <shellapi.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>

#include "core/poi_evidence.h"
#include "core/locator_model.h"
#include "core/viewport_policy.h"
#include "core/view_controller.h"
#include "tracking/win32_evidence_provider.h"
#include "tracking/uia_evidence_provider.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool;
using winrt::Windows::Graphics::Capture::GraphicsCaptureItem;
using winrt::Windows::Graphics::Capture::GraphicsCaptureSession;
using winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice;
using winrt::Windows::Graphics::DirectX::DirectXPixelFormat;
using AbiDxgiInterfaceAccess = Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess;

namespace {

constexpr wchar_t kWindowClass[] = L"VisualDetailWindow";
constexpr UINT kCaptureErrorMessage = WM_APP + 1;
constexpr UINT kCaptureSourceClosedMessage = WM_APP + 2;
constexpr DWORD kExcludeFromCapture = 0x00000011;

enum class ShutdownReason : int {
    User = 0,
    CaptureFailure = 1,
    CaptureSourceClosed = 2,
    DisplayTopologyChanged = 3,
};

std::atomic<int> g_shutdownReason{static_cast<int>(ShutdownReason::User)};
std::atomic<int> g_zoom{2};
std::atomic<int> g_previousMagnifiedZoom{2};
std::atomic<bool> g_trackingEnabled{true};
std::atomic<bool> g_captureFailed{false};

struct MonitorRecord {
    HMONITOR handle{};
    RECT rect{};
    bool primary{};
    std::wstring deviceName;
    DEVMODEW mode{};
};

struct Options {
    int sourceIndex{-1};
    int destIndex{-1};
    int zoom{2};
    bool forceSingleMonitor{false};
    std::filesystem::path logPath{L"visual_app_telemetry.csv"};
};

struct WindowPlacement {
    HWND hwnd{};
    UINT width{};
    UINT height{};
    bool singleMonitor{};
    bool excludedFromCapture{};
};

struct Vertex { float x, y, u, v; };
struct RenderConstants {
    float uvScale[2];
    float uvOffset[2];
    float locatorRect[4];
    float outputSize[2];
    float locatorVisible;
    float locatorKind;
};
static_assert((sizeof(RenderConstants) % 16) == 0);

LARGE_INTEGER qpc_now() { LARGE_INTEGER v{}; QueryPerformanceCounter(&v); return v; }
LARGE_INTEGER qpc_frequency() { LARGE_INTEGER v{}; QueryPerformanceFrequency(&v); return v; }

std::wstring format_hr(HRESULT hr) {
    wchar_t buffer[32]{};
    swprintf_s(buffer, L"0x%08X", static_cast<unsigned>(hr));
    return buffer;
}

BOOL CALLBACK monitor_enum_proc(HMONITOR monitor, HDC, LPRECT, LPARAM data) {
    auto* monitors = reinterpret_cast<std::vector<MonitorRecord>*>(data);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) return TRUE;

    MonitorRecord record{};
    record.handle = monitor;
    record.rect = info.rcMonitor;
    record.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
    record.deviceName = info.szDevice;
    record.mode.dmSize = sizeof(record.mode);
    EnumDisplaySettingsExW(record.deviceName.c_str(), ENUM_CURRENT_SETTINGS, &record.mode, 0);
    monitors->push_back(std::move(record));
    return TRUE;
}

std::vector<MonitorRecord> enumerate_monitors() {
    std::vector<MonitorRecord> monitors;
    EnumDisplayMonitors(nullptr, nullptr, monitor_enum_proc, reinterpret_cast<LPARAM>(&monitors));
    return monitors;
}

Options parse_options() {
    Options options{};
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return options;
    for (int i = 1; i < argc; ++i) {
        const std::wstring arg = argv[i];
        auto next_value = [&]() -> std::wstring {
            if (i + 1 >= argc) throw std::runtime_error("Missing command-line value");
            return argv[++i];
        };
        if (arg == L"--source") options.sourceIndex = std::stoi(next_value());
        else if (arg == L"--dest") options.destIndex = std::stoi(next_value());
        else if (arg == L"--zoom") options.zoom = std::stoi(next_value());
        else if (arg == L"--log") options.logPath = next_value();
        else if (arg == L"--single-monitor") options.forceSingleMonitor = true;
    }
    LocalFree(argv);
    if (options.zoom != 1 && options.zoom != 2 && options.zoom != 4) options.zoom = 2;
    return options;
}

class Telemetry {
public:
    explicit Telemetry(const std::filesystem::path& path) : stream_(path, std::ios::out | std::ios::trunc) {
        if (!stream_) throw std::runtime_error("Unable to create Visual telemetry file");
        stream_ << "frame,qpc,zoom,tracking,poi_present,poi_source,locator_visible,viewport_action,view_left,view_top,view_right,view_bottom,present_hr,pointer_age_ms,uia_snapshot_age_ms,uia_caret_present,uia_focus_present,selected_poi_age_ms,selected_left,selected_top,selected_right,selected_bottom\n";
        stream_.flush();
    }

    void frame(std::uint64_t sequence, LARGE_INTEGER qpc, int zoom, bool tracking, bool poiPresent,
               visual::core::PoiSource source, bool locatorVisible, visual::core::ViewportAction action,
               const visual::core::ScreenRect& viewport, HRESULT presentHr,
               double pointerAgeMs, double uiaSnapshotAgeMs, bool uiaCaretPresent, bool uiaFocusPresent,
               double selectedPoiAgeMs, const visual::core::ScreenRect& selectedRect) {
        std::scoped_lock lock(mutex_);
        stream_ << sequence << ',' << qpc.QuadPart << ',' << zoom << ',' << (tracking ? 1 : 0) << ','
                << (poiPresent ? 1 : 0) << ',' << static_cast<int>(source) << ',' << (locatorVisible ? 1 : 0) << ','
                << static_cast<int>(action) << ','
                << viewport.left << ',' << viewport.top << ',' << viewport.right << ',' << viewport.bottom << ','
                << static_cast<unsigned>(presentHr) << ','
                << pointerAgeMs << ',' << uiaSnapshotAgeMs << ',' << (uiaCaretPresent ? 1 : 0) << ','
                << (uiaFocusPresent ? 1 : 0) << ',' << selectedPoiAgeMs << ','
                << selectedRect.left << ',' << selectedRect.top << ',' << selectedRect.right << ',' << selectedRect.bottom << '\n';
        if (sequence == 1 || (sequence % 120) == 0) stream_.flush();
    }

    void event(const std::string& name) {
        std::scoped_lock lock(mutex_);
        stream_ << "# event=" << name << '\n';
        stream_.flush();
    }

private:
    std::ofstream stream_;
    std::mutex mutex_;
};

class Renderer {
public:
    void initialize(HWND hwnd, UINT width, UINT height) {
        width_ = width;
        height_ = height;

        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        D3D_FEATURE_LEVEL requested[] = {
            D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0
        };
        D3D_FEATURE_LEVEL actual{};
        winrt::check_hresult(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, requested,
            ARRAYSIZE(requested), D3D11_SDK_VERSION, device_.put(), &actual, context_.put()));

        auto dxgiDevice = device_.as<IDXGIDevice>();
        winrt::com_ptr<IDXGIAdapter> adapter;
        winrt::check_hresult(dxgiDevice->GetAdapter(adapter.put()));
        winrt::com_ptr<IDXGIFactory2> factory;
        winrt::check_hresult(adapter->GetParent(__uuidof(IDXGIFactory2), factory.put_void()));

        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = width_;
        desc.Height = height_;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.Scaling = DXGI_SCALING_STRETCH;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        winrt::check_hresult(factory->CreateSwapChainForHwnd(device_.get(), hwnd, &desc, nullptr, nullptr, swapChain_.put()));
        factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

        create_render_target();
        create_pipeline();
        create_winrt_device();
    }

    IDirect3DDevice winrt_device() const { return winrtDevice_; }

    void cache_source(ID3D11Texture2D* source) {
        if (!source) return;
        std::scoped_lock lock(d3dMutex_);
        D3D11_TEXTURE2D_DESC sourceDesc{};
        source->GetDesc(&sourceDesc);
        ensure_cached(sourceDesc);
        context_->CopyResource(cachedSource_.get(), source);
    }

    HRESULT render_cached(const RECT& sourceMonitorRect,
                          const visual::core::ScreenRect& viewportRect,
                          const visual::core::LocatorTarget& locator) {
        std::scoped_lock lock(d3dMutex_);
        ID3D11Texture2D* source = cachedSource_.get();
        if (!source) return S_FALSE;
        if (!viewportRect.valid()) return E_INVALIDARG;

        D3D11_TEXTURE2D_DESC sourceDesc{};
        source->GetDesc(&sourceDesc);
        ID3D11Texture2D* sampleTexture = source;
        if ((sourceDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE) == 0) {
            ensure_scratch(sourceDesc);
            context_->CopyResource(scratchTexture_.get(), source);
            sampleTexture = scratchTexture_.get();
        }

        winrt::com_ptr<ID3D11ShaderResourceView> srv;
        HRESULT hr = device_->CreateShaderResourceView(sampleTexture, nullptr, srv.put());
        if (FAILED(hr)) return hr;

        const visual::core::ScreenRect sourceRect{
            static_cast<double>(sourceMonitorRect.left), static_cast<double>(sourceMonitorRect.top),
            static_cast<double>(sourceMonitorRect.right), static_cast<double>(sourceMonitorRect.bottom)
        };
        if (!sourceRect.valid()) return E_INVALIDARG;

        const float cropLeft = static_cast<float>((viewportRect.left - sourceRect.left) / sourceRect.width() * sourceDesc.Width);
        const float cropTop = static_cast<float>((viewportRect.top - sourceRect.top) / sourceRect.height() * sourceDesc.Height);
        const float cropWidth = static_cast<float>(viewportRect.width() / sourceRect.width() * sourceDesc.Width);
        const float cropHeight = static_cast<float>(viewportRect.height() / sourceRect.height() * sourceDesc.Height);

        RenderConstants constants{
            {cropWidth / static_cast<float>(sourceDesc.Width), cropHeight / static_cast<float>(sourceDesc.Height)},
            {cropLeft / static_cast<float>(sourceDesc.Width), cropTop / static_cast<float>(sourceDesc.Height)},
            {static_cast<float>(locator.left), static_cast<float>(locator.top),
             static_cast<float>(locator.right), static_cast<float>(locator.bottom)},
            {static_cast<float>(width_), static_cast<float>(height_)},
            locator.visible ? 1.0f : 0.0f,
            static_cast<float>(locator.kind)
        };
        D3D11_MAPPED_SUBRESOURCE mapped{};
        hr = context_->Map(cropBuffer_.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (FAILED(hr)) return hr;
        *reinterpret_cast<RenderConstants*>(mapped.pData) = constants;
        context_->Unmap(cropBuffer_.get(), 0);

        FLOAT clear[4] = {0, 0, 0, 1};
        context_->ClearRenderTargetView(renderTarget_.get(), clear);
        UINT stride = sizeof(Vertex), offset = 0;
        ID3D11Buffer* vertexBuffers[] = {vertexBuffer_.get()};
        context_->IASetInputLayout(inputLayout_.get());
        context_->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        D3D11_VIEWPORT viewport{0, 0, static_cast<float>(width_), static_cast<float>(height_), 0.0f, 1.0f};
        context_->RSSetViewports(1, &viewport);
        ID3D11RenderTargetView* renderTargets[] = {renderTarget_.get()};
        context_->OMSetRenderTargets(1, renderTargets, nullptr);
        context_->VSSetShader(vertexShader_.get(), nullptr, 0);
        context_->PSSetShader(pixelShader_.get(), nullptr, 0);
        ID3D11ShaderResourceView* srvs[] = {srv.get()};
        ID3D11SamplerState* samplers[] = {sampler_.get()};
        ID3D11Buffer* constantBuffers[] = {cropBuffer_.get()};
        context_->PSSetShaderResources(0, 1, srvs);
        context_->PSSetSamplers(0, 1, samplers);
        context_->PSSetConstantBuffers(0, 1, constantBuffers);
        context_->Draw(6, 0);
        ID3D11ShaderResourceView* nullSrv[] = {nullptr};
        context_->PSSetShaderResources(0, 1, nullSrv);
        return swapChain_->Present(1, 0);
    }

private:
    void create_winrt_device() {
        auto dxgiDevice = device_.as<IDXGIDevice>();
        winrt::com_ptr<IInspectable> inspectable;
        winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), inspectable.put()));
        winrtDevice_ = inspectable.as<IDirect3DDevice>();
    }

    void create_render_target() {
        winrt::com_ptr<ID3D11Texture2D> backBuffer;
        winrt::check_hresult(swapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D), backBuffer.put_void()));
        winrt::check_hresult(device_->CreateRenderTargetView(backBuffer.get(), nullptr, renderTarget_.put()));
    }

    winrt::com_ptr<ID3DBlob> compile_shader(const char* source, const char* entry, const char* target) {
        winrt::com_ptr<ID3DBlob> shader, errors;
        const HRESULT hr = D3DCompile(source, strlen(source), nullptr, nullptr, nullptr, entry, target,
            D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, shader.put(), errors.put());
        if (FAILED(hr)) {
            if (errors) OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
            winrt::check_hresult(hr);
        }
        return shader;
    }

    void create_pipeline() {
        static constexpr char shaderSource[] = R"(
cbuffer RenderConstants : register(b0) {
    float2 uvScale;
    float2 uvOffset;
    float4 locatorRect;
    float2 outputSize;
    float locatorVisible;
    float locatorKind;
};
struct VSInput { float2 pos : POSITION; float2 uv : TEXCOORD0; };
struct VSOutput { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
VSOutput VSMain(VSInput input) { VSOutput o; o.pos=float4(input.pos,0,1); o.uv=input.uv; return o; }
Texture2D SourceTexture : register(t0);
SamplerState LinearSampler : register(s0);
float4 PSMain(VSOutput input) : SV_TARGET {
    float4 base = SourceTexture.Sample(LinearSampler, uvOffset + input.uv * uvScale);
    if (locatorVisible < 0.5) return base;

    float2 p = input.uv * outputSize;
    float2 r0 = locatorRect.xy * outputSize;
    float2 r1 = locatorRect.zw * outputSize;
    float2 c = 0.5 * (r0 + r1);
    float outer = 0.0;
    float inner = 0.0;

    if (locatorKind < 0.5) {
        // Pointer: fixed-size crosshair with a small central gap.
        float2 d = abs(p - c);
        float verticalOuter = (d.x <= 3.5 && d.y >= 5.0 && d.y <= 20.0) ? 1.0 : 0.0;
        float horizontalOuter = (d.y <= 3.5 && d.x >= 5.0 && d.x <= 20.0) ? 1.0 : 0.0;
        float verticalInner = (d.x <= 1.5 && d.y >= 6.0 && d.y <= 19.0) ? 1.0 : 0.0;
        float horizontalInner = (d.y <= 1.5 && d.x >= 6.0 && d.x <= 19.0) ? 1.0 : 0.0;
        outer = max(verticalOuter, horizontalOuter);
        inner = max(verticalInner, horizontalInner);
    } else {
        // Caret/focus: expand very small targets to a visible minimum and outline them.
        float2 minimumSize = locatorKind < 1.5 ? float2(12.0, 28.0) : float2(28.0, 28.0);
        float2 halfSize = max(0.5 * (r1 - r0), 0.5 * minimumSize);
        r0 = c - halfSize;
        r1 = c + halfSize;
        float inside = (p.x >= r0.x && p.x <= r1.x && p.y >= r0.y && p.y <= r1.y) ? 1.0 : 0.0;
        float edge = min(min(p.x - r0.x, r1.x - p.x), min(p.y - r0.y, r1.y - p.y));
        outer = inside * ((edge <= 5.0) ? 1.0 : 0.0);
        inner = inside * ((edge <= 2.0) ? 1.0 : 0.0);
    }

    if (inner > 0.5) return float4(1.0, 1.0, 1.0, 1.0);
    if (outer > 0.5) return float4(0.0, 0.0, 0.0, 1.0);
    return base;
}
)";
        auto vsBlob = compile_shader(shaderSource, "VSMain", "vs_5_0");
        auto psBlob = compile_shader(shaderSource, "PSMain", "ps_5_0");
        winrt::check_hresult(device_->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, vertexShader_.put()));
        winrt::check_hresult(device_->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, pixelShader_.put()));
        D3D11_INPUT_ELEMENT_DESC inputDesc[] = {
            {"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,8,D3D11_INPUT_PER_VERTEX_DATA,0},
        };
        winrt::check_hresult(device_->CreateInputLayout(inputDesc, ARRAYSIZE(inputDesc), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), inputLayout_.put()));
        const Vertex vertices[] = {{-1,1,0,0},{1,1,1,0},{1,-1,1,1},{-1,1,0,0},{1,-1,1,1},{-1,-1,0,1}};
        D3D11_BUFFER_DESC vbDesc{};
        vbDesc.ByteWidth = sizeof(vertices);
        vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA vbData{};
        vbData.pSysMem = vertices;
        winrt::check_hresult(device_->CreateBuffer(&vbDesc, &vbData, vertexBuffer_.put()));
        D3D11_BUFFER_DESC cbDesc{};
        cbDesc.ByteWidth = sizeof(RenderConstants);
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        winrt::check_hresult(device_->CreateBuffer(&cbDesc, nullptr, cropBuffer_.put()));
        D3D11_SAMPLER_DESC samplerDesc{};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        winrt::check_hresult(device_->CreateSamplerState(&samplerDesc, sampler_.put()));
    }

    void ensure_cached(const D3D11_TEXTURE2D_DESC& sourceDesc) {
        bool recreate = !cachedSource_;
        if (cachedSource_) {
            D3D11_TEXTURE2D_DESC existing{};
            cachedSource_->GetDesc(&existing);
            recreate = existing.Width != sourceDesc.Width || existing.Height != sourceDesc.Height || existing.Format != sourceDesc.Format;
        }
        if (!recreate) return;
        D3D11_TEXTURE2D_DESC desc = sourceDesc;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        winrt::check_hresult(device_->CreateTexture2D(&desc, nullptr, cachedSource_.put()));
    }
    void ensure_scratch(const D3D11_TEXTURE2D_DESC& sourceDesc) {
        bool recreate = !scratchTexture_;
        if (scratchTexture_) {
            D3D11_TEXTURE2D_DESC existing{};
            scratchTexture_->GetDesc(&existing);
            recreate = existing.Width != sourceDesc.Width || existing.Height != sourceDesc.Height || existing.Format != sourceDesc.Format;
        }
        if (!recreate) return;
        D3D11_TEXTURE2D_DESC desc = sourceDesc;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        winrt::check_hresult(device_->CreateTexture2D(&desc, nullptr, scratchTexture_.put()));
    }

    UINT width_{};
    UINT height_{};
    winrt::com_ptr<ID3D11Device> device_;
    winrt::com_ptr<ID3D11DeviceContext> context_;
    winrt::com_ptr<IDXGISwapChain1> swapChain_;
    winrt::com_ptr<ID3D11RenderTargetView> renderTarget_;
    winrt::com_ptr<ID3D11VertexShader> vertexShader_;
    winrt::com_ptr<ID3D11PixelShader> pixelShader_;
    winrt::com_ptr<ID3D11InputLayout> inputLayout_;
    winrt::com_ptr<ID3D11Buffer> vertexBuffer_;
    winrt::com_ptr<ID3D11Buffer> cropBuffer_;
    winrt::com_ptr<ID3D11SamplerState> sampler_;
    winrt::com_ptr<ID3D11Texture2D> scratchTexture_;
    winrt::com_ptr<ID3D11Texture2D> cachedSource_;
    std::mutex d3dMutex_;
    IDirect3DDevice winrtDevice_{nullptr};
};

class CaptureRunner {
public:
    CaptureRunner(Renderer& renderer, Telemetry& telemetry, const MonitorRecord& sourceMonitor, HWND notifyWindow)
        : renderer_(renderer), telemetry_(telemetry), sourceMonitor_(sourceMonitor), notifyWindow_(notifyWindow), qpcFreq_(qpc_frequency()) {}

    void start() {
        if (!GraphicsCaptureSession::IsSupported()) throw winrt::hresult_error(E_NOTIMPL, L"Windows Graphics Capture is not supported.");
        auto factory = winrt::get_activation_factory<GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
        winrt::check_hresult(factory->CreateForMonitor(sourceMonitor_.handle, winrt::guid_of<GraphicsCaptureItem>(), winrt::put_abi(item_)));
        itemClosedToken_ = item_.Closed([this](const GraphicsCaptureItem&, const winrt::Windows::Foundation::IInspectable&) {
            telemetry_.event("capture_item_closed");
            PostMessageW(notifyWindow_, kCaptureSourceClosedMessage, 0, 0);
        });
        itemClosedSubscribed_ = true;
        captureSize_ = item_.Size();
        framePool_ = Direct3D11CaptureFramePool::CreateFreeThreaded(renderer_.winrt_device(), DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, captureSize_);
        session_ = framePool_.CreateCaptureSession(item_);
        session_.IsCursorCaptureEnabled(false);
        token_ = framePool_.FrameArrived([this](const Direct3D11CaptureFramePool& sender, const winrt::Windows::Foundation::IInspectable&) { on_frame(sender); });
        session_.StartCapture();
        renderThread_ = std::jthread([this](std::stop_token stop) { render_loop(stop); });
        telemetry_.event("capture_started");
    }

    void stop() noexcept {
        if (renderThread_.joinable()) {
            renderThread_.request_stop();
            renderThread_.join();
        }
        try { if (framePool_) framePool_.FrameArrived(token_); } catch (...) {}
        try { if (item_ && itemClosedSubscribed_) item_.Closed(itemClosedToken_); } catch (...) {}
        itemClosedSubscribed_ = false;
        try { if (session_) session_.Close(); } catch (...) {}
        try { if (framePool_) framePool_.Close(); } catch (...) {}
        session_ = nullptr;
        framePool_ = nullptr;
        item_ = nullptr;
    }

    ~CaptureRunner() { stop(); }

private:
    [[nodiscard]] double age_ms(std::uint64_t timestamp, std::uint64_t now) const noexcept {
        if (timestamp == 0 || now < timestamp || qpcFreq_.QuadPart <= 0) return -1.0;
        return 1000.0 * static_cast<double>(now - timestamp) / static_cast<double>(qpcFreq_.QuadPart);
    }

    void on_frame(const Direct3D11CaptureFramePool& sender) noexcept {
        try {
            auto frame = sender.TryGetNextFrame();
            if (!frame) return;
            const auto size = frame.ContentSize();
            if (size.Width <= 0 || size.Height <= 0) return;
            if (size.Width != captureSize_.Width || size.Height != captureSize_.Height) {
                frame.Close();
                captureSize_ = size;
                framePool_.Recreate(renderer_.winrt_device(), DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, captureSize_);
                telemetry_.event("content_size_changed");
                return;
            }

            auto access = frame.Surface().as<AbiDxgiInterfaceAccess>();
            winrt::com_ptr<ID3D11Texture2D> texture;
            winrt::check_hresult(access->GetInterface(__uuidof(ID3D11Texture2D), texture.put_void()));
            renderer_.cache_source(texture.get());
            bool expected = false;
            if (firstSourceFrameObserved_.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
                telemetry_.event("first_source_frame");
            }
        } catch (const winrt::hresult_error&) {
            fail_capture();
        } catch (...) {
            fail_capture();
        }
    }

    void render_loop(std::stop_token stop) noexcept {
        try {
            const visual::core::ScreenRect sourceRect{
                static_cast<double>(sourceMonitor_.rect.left), static_cast<double>(sourceMonitor_.rect.top),
                static_cast<double>(sourceMonitor_.rect.right), static_cast<double>(sourceMonitor_.rect.bottom)
            };
            auto next = std::chrono::steady_clock::now();
            constexpr auto interval = std::chrono::milliseconds(16);

            while (!stop.stop_requested()) {
                next += interval;
                const auto now = qpc_now();
                const auto nowQpc = static_cast<std::uint64_t>(now.QuadPart);

                auto candidates = evidenceProvider_.sample(sourceRect, nowQpc);
                const auto uiaSnapshot = uiaEvidenceProvider_.diagnostic_snapshot();
                for (const auto& candidate : uiaSnapshot.candidates) {
                    const auto& r = candidate.screen_rect;
                    const bool intersectsSource = r.valid()
                        && r.right > sourceRect.left && r.left < sourceRect.right
                        && r.bottom > sourceRect.top && r.top < sourceRect.bottom;
                    if (intersectsSource) candidates.push_back(candidate);
                }

                const int zoom = g_zoom.load(std::memory_order_relaxed);
                const bool tracking = g_trackingEnabled.load(std::memory_order_relaxed);
                const auto view = viewController_.update(
                    sourceRect, zoom, tracking, candidates, nowQpc,
                    static_cast<std::uint64_t>(qpcFreq_.QuadPart));
                const auto selectedSource = view.selected_poi ? view.selected_poi->source : visual::core::PoiSource::Pointer;
                const auto locator = view.selected_poi
                    ? visual::core::make_locator_target(*view.selected_poi, view.viewport)
                    : visual::core::LocatorTarget{};

                const HRESULT presentHr = renderer_.render_cached(sourceMonitor_.rect, view.viewport, locator);
                if (presentHr != S_FALSE) {
                    if (SUCCEEDED(presentHr)) {
                        bool expected = false;
                        if (firstRenderObserved_.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
                            telemetry_.event("first_render_frame");
                        }
                    }
                    const double pointerAgeMs = age_ms(evidenceProvider_.last_pointer_movement_qpc(), nowQpc);
                    const double uiaSnapshotAgeMs = age_ms(uiaSnapshot.sampled_qpc, nowQpc);
                    const double selectedAgeMs = view.selected_poi ? age_ms(view.selected_poi->timestamp_qpc, nowQpc) : -1.0;
                    const auto selectedRect = view.selected_poi ? view.selected_poi->screen_rect : visual::core::ScreenRect{};
                    telemetry_.frame(++sequence_, now, zoom, tracking, view.selected_poi.has_value(), selectedSource,
                                     locator.visible, view.action, view.viewport, presentHr,
                                     pointerAgeMs, uiaSnapshotAgeMs, uiaSnapshot.current_caret_present,
                                     uiaSnapshot.current_focus_present, selectedAgeMs, selectedRect);
                    if (FAILED(presentHr)) throw winrt::hresult_error(presentHr);
                }

                std::this_thread::sleep_until(next);
                const auto afterSleep = std::chrono::steady_clock::now();
                if (afterSleep > next + interval * 4) next = afterSleep;
            }
        } catch (const winrt::hresult_error&) {
            fail_capture();
        } catch (...) {
            fail_capture();
        }
    }

    void fail_capture() noexcept {
        bool expected = false;
        if (failurePosted_.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
            try { telemetry_.event("capture_failure"); } catch (...) {}
            g_captureFailed.store(true, std::memory_order_relaxed);
            PostMessageW(notifyWindow_, kCaptureErrorMessage, static_cast<WPARAM>(E_FAIL), 0);
        }
    }

    Renderer& renderer_;
    Telemetry& telemetry_;
    MonitorRecord sourceMonitor_;
    HWND notifyWindow_{};
    LARGE_INTEGER qpcFreq_{};
    visual::tracking::Win32EvidenceProvider evidenceProvider_{};
    visual::tracking::UiaEvidenceProvider uiaEvidenceProvider_{};
    visual::core::ViewController viewController_{};
    GraphicsCaptureItem item_{nullptr};
    Direct3D11CaptureFramePool framePool_{nullptr};
    GraphicsCaptureSession session_{nullptr};
    winrt::event_token token_{};
    winrt::event_token itemClosedToken_{};
    bool itemClosedSubscribed_{};
    winrt::Windows::Graphics::SizeInt32 captureSize_{};
    std::jthread renderThread_{};
    std::atomic<bool> failurePosted_{false};
    std::atomic<bool> firstSourceFrameObserved_{false};
    std::atomic<bool> firstRenderObserved_{false};
    std::uint64_t sequence_{};
};

constexpr int kHotkeyZoom1 = 101;
constexpr int kHotkeyZoom2 = 102;
constexpr int kHotkeyZoom4 = 104;
constexpr int kHotkeyTracking = 110;
constexpr int kHotkeyNormalReturn = 120;
constexpr int kHotkeyExit = 199;

void set_zoom(int zoom) noexcept {
    if (zoom != 1 && zoom != 2 && zoom != 4) return;
    if (zoom > 1) g_previousMagnifiedZoom.store(zoom, std::memory_order_relaxed);
    g_zoom.store(zoom, std::memory_order_relaxed);
}

void toggle_normal_return() noexcept {
    const int current = g_zoom.load(std::memory_order_relaxed);
    if (current == 1) {
        int restore = g_previousMagnifiedZoom.load(std::memory_order_relaxed);
        if (restore != 2 && restore != 4) restore = 2;
        g_zoom.store(restore, std::memory_order_relaxed);
    } else {
        g_previousMagnifiedZoom.store(current, std::memory_order_relaxed);
        g_zoom.store(1, std::memory_order_relaxed);
    }
}

void toggle_tracking() noexcept {
    g_trackingEnabled.store(!g_trackingEnabled.load(std::memory_order_relaxed), std::memory_order_relaxed);
}

void register_hotkeys(HWND hwnd) noexcept {
    constexpr UINT modifiers = MOD_CONTROL | MOD_ALT | MOD_NOREPEAT;
    RegisterHotKey(hwnd, kHotkeyZoom1, modifiers, '1');
    RegisterHotKey(hwnd, kHotkeyZoom2, modifiers, '2');
    RegisterHotKey(hwnd, kHotkeyZoom4, modifiers, '4');
    RegisterHotKey(hwnd, kHotkeyTracking, modifiers, 'T');
    RegisterHotKey(hwnd, kHotkeyNormalReturn, modifiers, '0');
    RegisterHotKey(hwnd, kHotkeyExit, modifiers, 'Q');
}

void unregister_hotkeys(HWND hwnd) noexcept {
    UnregisterHotKey(hwnd, kHotkeyZoom1);
    UnregisterHotKey(hwnd, kHotkeyZoom2);
    UnregisterHotKey(hwnd, kHotkeyZoom4);
    UnregisterHotKey(hwnd, kHotkeyTracking);
    UnregisterHotKey(hwnd, kHotkeyNormalReturn);
    UnregisterHotKey(hwnd, kHotkeyExit);
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) { DestroyWindow(hwnd); return 0; }
        if (wParam == '1') { set_zoom(1); return 0; }
        if (wParam == '2') { set_zoom(2); return 0; }
        if (wParam == '4') { set_zoom(4); return 0; }
        if (wParam == 'T') { toggle_tracking(); return 0; }
        if (wParam == '0') { toggle_normal_return(); return 0; }
        break;
    case WM_HOTKEY:
        if (wParam == kHotkeyZoom1) { set_zoom(1); return 0; }
        if (wParam == kHotkeyZoom2) { set_zoom(2); return 0; }
        if (wParam == kHotkeyZoom4) { set_zoom(4); return 0; }
        if (wParam == kHotkeyTracking) { toggle_tracking(); return 0; }
        if (wParam == kHotkeyNormalReturn) { toggle_normal_return(); return 0; }
        if (wParam == kHotkeyExit) { DestroyWindow(hwnd); return 0; }
        break;
    case kCaptureErrorMessage:
        g_shutdownReason.store(static_cast<int>(ShutdownReason::CaptureFailure), std::memory_order_relaxed);
        MessageBoxW(hwnd, L"Capture/render loop failed. See visual_app_telemetry.csv.", L"Visual", MB_OK | MB_ICONERROR);
        DestroyWindow(hwnd);
        return 0;
    case kCaptureSourceClosedMessage:
        g_shutdownReason.store(static_cast<int>(ShutdownReason::CaptureSourceClosed), std::memory_order_relaxed);
        DestroyWindow(hwnd);
        return 0;
    case WM_DISPLAYCHANGE:
        g_shutdownReason.store(static_cast<int>(ShutdownReason::DisplayTopologyChanged), std::memory_order_relaxed);
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        unregister_hotkeys(hwnd);
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

WindowPlacement create_detail_window(HINSTANCE instance, const MonitorRecord& destination, bool singleMonitor) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClass;
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) winrt::throw_last_error();

    const int monitorWidth = destination.rect.right - destination.rect.left;
    const int monitorHeight = destination.rect.bottom - destination.rect.top;
    int x = destination.rect.left;
    int y = destination.rect.top;
    int width = monitorWidth;
    int height = monitorHeight;
    DWORD style = WS_POPUP;
    DWORD exStyle = WS_EX_APPWINDOW | WS_EX_NOACTIVATE;

    if (singleMonitor) {
        width = std::min(std::max(640, monitorWidth * 45 / 100), monitorWidth - 40);
        height = std::min(std::max(360, monitorHeight * 45 / 100), monitorHeight - 80);
        x = destination.rect.right - width - 20;
        y = destination.rect.top + 40;
        style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        exStyle |= WS_EX_TOPMOST;
    }

    HWND hwnd = CreateWindowExW(exStyle, kWindowClass, L"Visual — Detail", style, x, y, width, height,
                                nullptr, nullptr, instance, nullptr);
    if (!hwnd) winrt::throw_last_error();
    register_hotkeys(hwnd);
    SetWindowPos(hwnd, singleMonitor ? HWND_TOPMOST : HWND_TOP, x, y, width, height, SWP_SHOWWINDOW | SWP_NOACTIVATE);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(hwnd);

    WindowPlacement result{};
    result.hwnd = hwnd;
    result.width = static_cast<UINT>(width);
    result.height = static_cast<UINT>(height);
    result.singleMonitor = singleMonitor;
    if (singleMonitor) result.excludedFromCapture = SetWindowDisplayAffinity(hwnd, kExcludeFromCapture) != FALSE;
    return result;
}

int choose_source(const std::vector<MonitorRecord>& monitors, int requested) {
    if (requested >= 0) return requested;
    for (size_t i = 0; i < monitors.size(); ++i) if (monitors[i].primary) return static_cast<int>(i);
    return 0;
}

int choose_destination(const std::vector<MonitorRecord>& monitors, int source, int requested, bool singleMonitor) {
    if (singleMonitor) return source;
    if (requested >= 0) return requested;
    for (size_t i = 0; i < monitors.size(); ++i) if (static_cast<int>(i) != source) return static_cast<int>(i);
    return -1;
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    try {
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        const auto options = parse_options();
        g_zoom.store(options.zoom, std::memory_order_relaxed);
        if (options.zoom > 1) g_previousMagnifiedZoom.store(options.zoom, std::memory_order_relaxed);

        const auto monitors = enumerate_monitors();
        if (monitors.empty()) {
            MessageBoxW(nullptr, L"No active display was detected.", L"Visual", MB_OK | MB_ICONERROR);
            return 2;
        }

        const bool singleMonitor = options.forceSingleMonitor || monitors.size() == 1;
        const int sourceIndex = choose_source(monitors, options.sourceIndex);
        const int destIndex = choose_destination(monitors, sourceIndex, options.destIndex, singleMonitor);
        if (sourceIndex < 0 || sourceIndex >= static_cast<int>(monitors.size())
            || destIndex < 0 || destIndex >= static_cast<int>(monitors.size())
            || (!singleMonitor && sourceIndex == destIndex)) {
            MessageBoxW(nullptr, L"Invalid source/destination monitor selection.", L"Visual", MB_OK | MB_ICONERROR);
            return 3;
        }

        Telemetry telemetry(options.logPath);
        telemetry.event("telemetry_started");
        auto window = create_detail_window(instance, monitors[destIndex], singleMonitor);
        Renderer renderer;
        renderer.initialize(window.hwnd, window.width, window.height);

        CaptureRunner capture(renderer, telemetry, monitors[sourceIndex], window.hwnd);
        capture.start();
        MSG msg{};
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        capture.stop();
        const auto shutdown = static_cast<ShutdownReason>(g_shutdownReason.load(std::memory_order_relaxed));
        switch (shutdown) {
        case ShutdownReason::CaptureFailure:
            telemetry.event("shutdown_capture_failure");
            return 10;
        case ShutdownReason::CaptureSourceClosed:
            telemetry.event("shutdown_capture_source_closed");
            return 11;
        case ShutdownReason::DisplayTopologyChanged:
            telemetry.event("shutdown_display_topology_changed");
            return 12;
        case ShutdownReason::User:
        default:
            return g_captureFailed.load(std::memory_order_relaxed) ? 10 : 0;
        }
    } catch (const winrt::hresult_error& e) {
        const std::wstring text = L"HRESULT failure: " + format_hr(e.code()) + L"\n" + std::wstring(e.message().c_str());
        MessageBoxW(nullptr, text.c_str(), L"Visual", MB_OK | MB_ICONERROR);
        return 20;
    } catch (const std::exception& e) {
        const std::string s = e.what();
        const std::wstring message(s.begin(), s.end());
        MessageBoxW(nullptr, message.c_str(), L"Visual", MB_OK | MB_ICONERROR);
        return 21;
    } catch (...) {
        MessageBoxW(nullptr, L"Unknown fatal error.", L"Visual", MB_OK | MB_ICONERROR);
        return 22;
    }
}
