// A tiny VST3 host for testing Scope Synth the way FL Studio uses it: load the
// plugin, open its window inside a host window, play notes into it on an
// audio thread, use the window like a user would (drag the LFO onto a knob,
// play the on-screen keys, flip POLY/MONO, turn a knob), then shut it all down.
// Windows only; built with mingw and run under Wine:
//   x86_64-w64-mingw32-g++ -std=c++17 -O1 -I../crt-scope-vst/dpf/distrho/src tests/minihost.cpp -o minihost.exe -static -lgdi32 -lole32
//   wine minihost.exe 'C:\path\Scope-Synth.vst3\Contents\x86_64-win\Scope-Synth.vst3'
#define _USE_MATH_DEFINES
#include <windows.h>
#include <ole2.h>
#include <shlobj.h>

#include <atomic>
#include <chrono>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "travesty/audio_processor.h"
#include "travesty/base.h"
#include "travesty/bstream.h"
#include "travesty/component.h"
#include "travesty/edit_controller.h"
#include "travesty/events.h"
#include "travesty/factory.h"
#include "travesty/host.h"
#include "travesty/message.h"
#include "travesty/view.h"

#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s\n", msg); std::fflush(stdout); return 1; } } while (0)

static std::atomic<int> gEdits{0};
static std::mutex gEditLock;
static std::vector<std::pair<v3_param_id, double>> gPendingEdits;
static std::atomic<int> gLastParam{-1};
static std::atomic<double> gLastValue{-1};

// ---- minimal COM-style objects the plugin expects from a host ------------------------

template <class VT>
struct Obj {
    const VT* vtbl;
    std::atomic<uint32_t> refs{1};
};

static uint32_t V3_API addRef(void* self) { return ++static_cast<Obj<void>*>(self)->refs; }
static uint32_t V3_API noRelease(void*) { return 1; }  // host objects live for the whole test

// attribute list: a bag of named values
struct AttrList {
    struct VT { v3_funknown u; v3_attribute_list a; };
    const VT* vtbl;
    std::atomic<uint32_t> refs{1};
    std::map<std::string, int64_t> ints;
    std::map<std::string, double> floats;
    std::map<std::string, std::vector<int16_t>> strings;
    std::map<std::string, std::vector<uint8_t>> blobs;
    static AttrList* me(void* s) { return static_cast<AttrList*>(s); }
    static v3_result V3_API qi(void* self, const v3_tuid iid, void** obj)
    {
        if (v3_tuid_match(iid, v3_funknown_iid) || v3_tuid_match(iid, v3_attribute_list_iid)) {
            *obj = self; addRef(self); return V3_OK;
        }
        *obj = nullptr; return V3_NO_INTERFACE;
    }
    static uint32_t V3_API unref(void* self)
    {
        const uint32_t r = --me(self)->refs;
        if (r == 0) delete me(self);
        return r;
    }
    static v3_result V3_API setInt(void* s, const char* id, int64_t v) { me(s)->ints[id] = v; return V3_OK; }
    static v3_result V3_API getInt(void* s, const char* id, int64_t* v)
    {
        auto it = me(s)->ints.find(id); if (it == me(s)->ints.end()) return V3_INVALID_ARG; *v = it->second; return V3_OK;
    }
    static v3_result V3_API setFloat(void* s, const char* id, double v) { me(s)->floats[id] = v; return V3_OK; }
    static v3_result V3_API getFloat(void* s, const char* id, double* v)
    {
        auto it = me(s)->floats.find(id); if (it == me(s)->floats.end()) return V3_INVALID_ARG; *v = it->second; return V3_OK;
    }
    static v3_result V3_API setString(void* s, const char* id, const int16_t* str)
    {
        std::vector<int16_t> v; while (*str) v.push_back(*str++); v.push_back(0); me(s)->strings[id] = v; return V3_OK;
    }
    static v3_result V3_API getString(void* s, const char* id, int16_t* out, uint32_t size)
    {
        auto it = me(s)->strings.find(id); if (it == me(s)->strings.end() || size == 0) return V3_INVALID_ARG;
        const uint32_t n = std::min<uint32_t>(size / sizeof(int16_t), (uint32_t)it->second.size());
        std::memcpy(out, it->second.data(), n * sizeof(int16_t)); out[n - 1] = 0; return V3_OK;
    }
    static v3_result V3_API setBinary(void* s, const char* id, const void* data, uint32_t size)
    {
        me(s)->blobs[id].assign((const uint8_t*)data, (const uint8_t*)data + size); return V3_OK;
    }
    static v3_result V3_API getBinary(void* s, const char* id, const void** data, uint32_t* size)
    {
        auto it = me(s)->blobs.find(id); if (it == me(s)->blobs.end()) return V3_INVALID_ARG;
        *data = it->second.data(); *size = (uint32_t)it->second.size(); return V3_OK;
    }
    static const VT* table()
    {
        static const VT vt = {{qi, addRef, unref}, {setInt, getInt, setFloat, getFloat, setString, getString, setBinary, getBinary}};
        return &vt;
    }
    AttrList() : vtbl(table()) {}
};

struct Message {
    struct VT { v3_funknown u; v3_message m; };
    const VT* vtbl;
    std::atomic<uint32_t> refs{1};
    std::string id;
    AttrList* attrs = new AttrList();
    static Message* me(void* s) { return static_cast<Message*>(s); }
    static v3_result V3_API qi(void* self, const v3_tuid iid, void** obj)
    {
        if (v3_tuid_match(iid, v3_funknown_iid) || v3_tuid_match(iid, v3_message_iid)) {
            *obj = self; addRef(self); return V3_OK;
        }
        *obj = nullptr; return V3_NO_INTERFACE;
    }
    static uint32_t V3_API unref(void* self)
    {
        const uint32_t r = --me(self)->refs;
        if (r == 0) { AttrList::unref(me(self)->attrs); delete me(self); }
        return r;
    }
    static const char* V3_API getId(void* s) { return me(s)->id.c_str(); }
    static void V3_API setId(void* s, const char* id) { me(s)->id = id ? id : ""; }
    static v3_attribute_list** V3_API getAttrs(void* s) { return (v3_attribute_list**)me(s)->attrs; }
    static const VT* table()
    {
        static const VT vt = {{qi, addRef, unref}, {getId, setId, getAttrs}};
        return &vt;
    }
    Message() : vtbl(table()) {}
};

struct HostApp {
    struct VT { v3_funknown u; v3_host_application h; };
    const VT* vtbl;
    std::atomic<uint32_t> refs{1};
    static v3_result V3_API qi(void* self, const v3_tuid iid, void** obj)
    {
        if (v3_tuid_match(iid, v3_funknown_iid) || v3_tuid_match(iid, v3_host_application_iid)) {
            *obj = self; return V3_OK;
        }
        *obj = nullptr; return V3_NO_INTERFACE;
    }
    static v3_result V3_API getName(void*, v3_str_128 name)
    {
        const char* n = "CRT minihost"; int i = 0; for (; n[i]; ++i) name[i] = n[i]; name[i] = 0; return V3_OK;
    }
    static v3_result V3_API createInstance(void*, v3_tuid cid, v3_tuid iid, void** obj)
    {
        if (v3_tuid_match(cid, v3_message_iid) && v3_tuid_match(iid, v3_message_iid)) {
            *obj = new Message(); return V3_OK;
        }
        if (v3_tuid_match(cid, v3_attribute_list_iid) && v3_tuid_match(iid, v3_attribute_list_iid)) {
            *obj = new AttrList(); return V3_OK;
        }
        *obj = nullptr; return V3_NO_INTERFACE;
    }
    static const VT* table()
    {
        static const VT vt = {{qi, addRef, noRelease}, {getName, createInstance}};
        return &vt;
    }
    HostApp() : vtbl(table()) {}
};

// receives "the user changed a knob in the plugin window" (FL records automation from these)
struct Handler {
    struct VT { v3_funknown u; v3_component_handler h; };
    const VT* vtbl;
    std::atomic<uint32_t> refs{1};
    static v3_result V3_API qi(void* self, const v3_tuid iid, void** obj)
    {
        if (v3_tuid_match(iid, v3_funknown_iid) || v3_tuid_match(iid, v3_component_handler_iid)) {
            *obj = self; return V3_OK;
        }
        *obj = nullptr; return V3_NO_INTERFACE;
    }
    static v3_result V3_API begin(void*, v3_param_id) { return V3_OK; }
    static v3_result V3_API perform(void*, v3_param_id id, double v)
    {
        ++gEdits; gLastParam = (int)id; gLastValue = v;
        std::lock_guard<std::mutex> g(gEditLock);
        gPendingEdits.push_back({id, v});
        return V3_OK;
    }
    static v3_result V3_API end(void*, v3_param_id) { return V3_OK; }
    static v3_result V3_API restart(void*, int32_t) { return V3_OK; }
    static const VT* table()
    {
        static const VT vt = {{qi, addRef, noRelease}, {begin, perform, end, restart}};
        return &vt;
    }
    Handler() : vtbl(table()) {}
};

struct Frame {
    struct VT { v3_funknown u; v3_plugin_frame f; };
    const VT* vtbl;
    std::atomic<uint32_t> refs{1};
    static v3_result V3_API qi(void* self, const v3_tuid iid, void** obj)
    {
        if (v3_tuid_match(iid, v3_funknown_iid) || v3_tuid_match(iid, v3_plugin_frame_iid)) {
            *obj = self; return V3_OK;
        }
        *obj = nullptr; return V3_NO_INTERFACE;
    }
    static v3_result V3_API resize(void*, v3_plugin_view**, v3_view_rect*) { return V3_OK; }
    static const VT* table()
    {
        static const VT vt = {{qi, addRef, noRelease}, {resize}};
        return &vt;
    }
    Frame() : vtbl(table()) {}
};

// parameter changes handed to process(): one value per parameter, like a host
// passing on what the user just did in the plugin window
struct ParamQueue {
    struct VT { v3_funknown u; v3_param_value_queue q; };
    const VT* vtbl;
    std::atomic<uint32_t> refs{1};
    v3_param_id id = 0;
    double value = 0;
    static ParamQueue* me(void* s) { return static_cast<ParamQueue*>(s); }
    static v3_result V3_API qi(void* self, const v3_tuid iid, void** obj)
    {
        if (v3_tuid_match(iid, v3_funknown_iid) || v3_tuid_match(iid, v3_param_value_queue_iid)) {
            *obj = self; return V3_OK;
        }
        *obj = nullptr; return V3_NO_INTERFACE;
    }
    static v3_param_id V3_API getId(void* s) { return me(s)->id; }
    static int32_t V3_API count(void*) { return 1; }
    static v3_result V3_API getPoint(void* s, int32_t idx, int32_t* offset, double* v)
    {
        if (idx != 0) return V3_INVALID_ARG;
        *offset = 0; *v = me(s)->value; return V3_OK;
    }
    static v3_result V3_API addPoint(void*, int32_t, double, int32_t*) { return V3_NOT_IMPLEMENTED; }
    static const VT* table()
    {
        static const VT vt = {{qi, addRef, noRelease}, {getId, count, getPoint, addPoint}};
        return &vt;
    }
    ParamQueue() : vtbl(table()) {}
};

struct ParamChanges {
    struct VT { v3_funknown u; v3_param_changes c; };
    const VT* vtbl;
    std::atomic<uint32_t> refs{1};
    std::vector<ParamQueue*> queues;
    static ParamChanges* me(void* s) { return static_cast<ParamChanges*>(s); }
    static v3_result V3_API qi(void* self, const v3_tuid iid, void** obj)
    {
        if (v3_tuid_match(iid, v3_funknown_iid) || v3_tuid_match(iid, v3_param_changes_iid)) {
            *obj = self; return V3_OK;
        }
        *obj = nullptr; return V3_NO_INTERFACE;
    }
    static int32_t V3_API count(void* s) { return (int32_t)me(s)->queues.size(); }
    static v3_param_value_queue** V3_API get(void* s, int32_t idx)
    {
        return idx >= 0 && idx < (int32_t)me(s)->queues.size() ? (v3_param_value_queue**)me(s)->queues[idx] : nullptr;
    }
    static v3_param_value_queue** V3_API add(void*, const v3_param_id*, int32_t*) { return nullptr; }
    static const VT* table()
    {
        static const VT vt = {{qi, addRef, noRelease}, {count, get, add}};
        return &vt;
    }
    ParamChanges() : vtbl(table()) {}
};

// in-memory stream, for saving the plugin's state
struct Stream {
    struct VT { v3_funknown u; v3_bstream s; };
    const VT* vtbl;
    std::atomic<uint32_t> refs{1};
    std::vector<uint8_t> data;
    int64_t pos = 0;
    static Stream* me(void* s) { return static_cast<Stream*>(s); }
    static v3_result V3_API qi(void* self, const v3_tuid iid, void** obj)
    {
        if (v3_tuid_match(iid, v3_funknown_iid) || v3_tuid_match(iid, v3_bstream_iid)) {
            *obj = self; addRef(self); return V3_OK;
        }
        *obj = nullptr; return V3_NO_INTERFACE;
    }
    static v3_result V3_API read(void* s, void* buf, int32_t n, int32_t* got)
    {
        const int32_t k = (int32_t)std::max<int64_t>(0, std::min<int64_t>(n, (int64_t)me(s)->data.size() - me(s)->pos));
        std::memcpy(buf, me(s)->data.data() + me(s)->pos, k); me(s)->pos += k;
        if (got) *got = k;
        return V3_OK;
    }
    static v3_result V3_API write(void* s, void* buf, int32_t n, int32_t* put)
    {
        auto& d = me(s)->data;
        if ((int64_t)d.size() < me(s)->pos + n) d.resize(me(s)->pos + n);
        std::memcpy(d.data() + me(s)->pos, buf, n); me(s)->pos += n;
        if (put) *put = n;
        return V3_OK;
    }
    static v3_result V3_API seek(void* s, int64_t p, int32_t mode, int64_t* res)
    {
        me(s)->pos = mode == V3_SEEK_SET ? p : mode == V3_SEEK_CUR ? me(s)->pos + p : (int64_t)me(s)->data.size() + p;
        if (res) *res = me(s)->pos;
        return V3_OK;
    }
    static v3_result V3_API tell(void* s, int64_t* p) { *p = me(s)->pos; return V3_OK; }
    static const VT* table()
    {
        static const VT vt = {{qi, addRef, noRelease}, {read, write, seek, tell}};
        return &vt;
    }
    Stream() : vtbl(table()) {}
};

// ---- a file being dragged, as Explorer offers it (CF_HDROP)
static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

static void pump(double seconds)
{
    const double end = now() + seconds;
    while (now() < end) {
        MSG msg;
        while (now() < end && PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        Sleep(5);
    }
}

// screenshot of a window's client area, as a BMP (to check what the plugin drew, buttons included)
static void saveWindow(HWND w, const char* path)
{
    RECT rc;
    GetClientRect(w, &rc);
    const int W = rc.right, H = rc.bottom;
    HDC src = GetDC(w), mem = CreateCompatibleDC(src);
    HBITMAP bmp = CreateCompatibleBitmap(src, W, H);
    SelectObject(mem, bmp);
    BitBlt(mem, 0, 0, W, H, src, 0, 0, SRCCOPY);
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader); bi.bmiHeader.biWidth = W; bi.bmiHeader.biHeight = H;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 24; bi.bmiHeader.biCompression = BI_RGB;
    const int stride = (W * 3 + 3) & ~3;
    std::vector<uint8_t> px(stride * H);
    GetDIBits(mem, bmp, 0, H, px.data(), &bi, DIB_RGB_COLORS);
    BITMAPFILEHEADER fh = {};
    fh.bfType = 0x4D42; fh.bfOffBits = sizeof(fh) + sizeof(bi.bmiHeader); fh.bfSize = fh.bfOffBits + (DWORD)px.size();
    if (FILE* f = std::fopen(path, "wb")) {
        std::fwrite(&fh, sizeof(fh), 1, f); std::fwrite(&bi.bmiHeader, sizeof(bi.bmiHeader), 1, f);
        std::fwrite(px.data(), 1, px.size(), f); std::fclose(f);
    }
    DeleteObject(bmp); DeleteDC(mem); ReleaseDC(w, src);
}

static BOOL CALLBACK findChild(HWND h, LPARAM out)
{
    *(HWND*)out = h;
    return FALSE;
}

// notes handed to process(), like FL passing on its piano roll
struct Events {
    struct VT { v3_funknown u; v3_event_list l; };
    const VT* vtbl;
    std::atomic<uint32_t> refs{1};
    std::vector<v3_event> list;
    static Events* me(void* s) { return static_cast<Events*>(s); }
    static v3_result V3_API qi(void* self, const v3_tuid iid, void** obj)
    {
        if (v3_tuid_match(iid, v3_funknown_iid) || v3_tuid_match(iid, v3_event_list_iid)) {
            *obj = self; return V3_OK;
        }
        *obj = nullptr; return V3_NO_INTERFACE;
    }
    static uint32_t V3_API count(void* s) { return (uint32_t)me(s)->list.size(); }
    static v3_result V3_API get(void* s, int32_t i, v3_event* e)
    {
        if (i < 0 || i >= (int32_t)me(s)->list.size()) return V3_INVALID_ARG;
        *e = me(s)->list[i]; return V3_OK;
    }
    static v3_result V3_API add(void*, v3_event*) { return V3_NOT_IMPLEMENTED; }
    static const VT* table()
    {
        static const VT vt = {{qi, addRef, noRelease}, {count, get, add}};
        return &vt;
    }
    Events() : vtbl(table()) {}
};

static std::mutex gNoteLock;
static std::vector<std::pair<int, int>> gNotes;  // note, velocity (0 = off)
static std::atomic<float> gPeak{0};

static void midi(int note, int vel)
{
    std::lock_guard<std::mutex> g(gNoteLock);
    gNotes.push_back({note, vel});
}

static float peakSince(double seconds)
{
    gPeak = 0;
    pump(seconds);
    return gPeak.load();
}

// press, move in steps, let go (client coordinates of the plugin window)
static void drag(HWND child, int x0, int y0, int x1, int y1)
{
    PostMessageA(child, WM_MOUSEMOVE, 0, MAKELPARAM(x0, y0));
    PostMessageA(child, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x0, y0));
    pump(0.05);
    for (int i = 1; i <= 10; ++i) {
        PostMessageA(child, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(x0 + (x1 - x0) * i / 10, y0 + (y1 - y0) * i / 10));
        pump(0.05);
    }
    PostMessageA(child, WM_LBUTTONUP, 0, MAKELPARAM(x1, y1));
    pump(0.3);
}

static void click(HWND child, int x, int y)
{
    PostMessageA(child, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
    PostMessageA(child, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
    pump(0.3);
}

int main(int argc, char** argv)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    CHECK(argc > 1, "usage: minihost path\\to\\plugin.vst3");
    HMODULE lib = LoadLibraryA(argv[1]);
    CHECK(lib, "LoadLibrary");
    if (auto init = (bool (*)())GetProcAddress(lib, "InitDll")) init();
    auto getFactory = (v3_plugin_factory** (*)())GetProcAddress(lib, "GetPluginFactory");
    CHECK(getFactory, "GetPluginFactory export");
    v3_plugin_factory** factory = getFactory();
    CHECK(factory, "factory");

    static HostApp host;
    v3_tuid componentCid = {};
    bool found = false;
    const int32_t nClasses = v3_cpp_obj(factory)->num_classes(factory);
    for (int32_t i = 0; i < nClasses; ++i) {
        v3_class_info info;
        v3_cpp_obj(factory)->get_class_info(factory, i, &info);
        std::printf("class %d: %s (%s)\n", i, info.name, info.category);
        if (!std::strcmp(info.category, "Audio Module Class")) { std::memcpy(componentCid, info.class_id, 16); found = true; }
    }
    CHECK(found, "audio module class");

    v3_component** comp = nullptr;
    CHECK(v3_cpp_obj(factory)->create_instance(factory, componentCid, v3_component_iid, (void**)&comp) == V3_OK && comp, "create component");
    CHECK(v3_cpp_obj_initialize(comp, (v3_funknown**)&host) == V3_OK, "component initialize");
    std::printf("event inputs: %d, audio inputs: %d, audio outputs: %d\n",
                v3_cpp_obj(comp)->get_bus_count(comp, V3_EVENT, V3_INPUT),
                v3_cpp_obj(comp)->get_bus_count(comp, V3_AUDIO, V3_INPUT),
                v3_cpp_obj(comp)->get_bus_count(comp, V3_AUDIO, V3_OUTPUT));
    CHECK(v3_cpp_obj(comp)->get_bus_count(comp, V3_EVENT, V3_INPUT) == 1, "a note input");

    v3_edit_controller** ctrl = nullptr;
    bool separate = false;
    if (v3_cpp_obj_query_interface(comp, v3_edit_controller_iid, &ctrl) != V3_OK || !ctrl) {
        v3_tuid ctrlCid;
        CHECK(v3_cpp_obj(comp)->get_controller_class_id(comp, ctrlCid) == V3_OK, "controller class id");
        CHECK(v3_cpp_obj(factory)->create_instance(factory, ctrlCid, v3_edit_controller_iid, (void**)&ctrl) == V3_OK && ctrl, "create controller");
        CHECK(v3_cpp_obj_initialize(ctrl, (v3_funknown**)&host) == V3_OK, "controller initialize");
        separate = true;
        v3_connection_point **a = nullptr, **b = nullptr;
        if (v3_cpp_obj_query_interface(comp, v3_connection_point_iid, &a) == V3_OK &&
            v3_cpp_obj_query_interface(ctrl, v3_connection_point_iid, &b) == V3_OK) {
            v3_cpp_obj(a)->connect(a, b);
            v3_cpp_obj(b)->connect(b, a);
        }
    }
    static Handler handler;
    v3_cpp_obj(ctrl)->set_component_handler(ctrl, (v3_component_handler**)&handler);
    const int nParams = v3_cpp_obj(ctrl)->get_parameter_count(ctrl);
    const int base = nParams - 102;  // DPF puts its MIDI CC slots (pitch bend, mod wheel...) before the synth's own
    std::printf("parameters: %d (synth's own from %d)\n", nParams, base);

    v3_audio_processor** proc = nullptr;
    CHECK(v3_cpp_obj_query_interface(comp, v3_audio_processor_iid, &proc) == V3_OK && proc, "audio processor");
    v3_process_setup setup = {0 /*realtime*/, 0 /*32-bit*/, 512, 48000.0};
    CHECK(v3_cpp_obj(proc)->setup_processing(proc, &setup) == V3_OK, "setup_processing");
    CHECK(v3_cpp_obj(comp)->set_active(comp, true) == V3_OK, "set_active");
    v3_cpp_obj(proc)->set_processing(proc, true);

    // ---- audio thread: 512-frame blocks in real time, notes and knob moves passed in each block
    std::atomic<bool> running{true};
    std::atomic<int> blocks{0};
    std::atomic<bool> audioOk{true};
    std::thread audio([&] {
        std::vector<float> outL(512), outR(512);
        float* outs[2] = {outL.data(), outR.data()};
        v3_audio_bus_buffers out = {};
        out.num_channels = 2; out.channel_buffers_32 = outs;
        const double start = now();
        while (running) {
            ParamChanges changes;
            std::vector<std::unique_ptr<ParamQueue>> queues;
            {
                std::lock_guard<std::mutex> g(gEditLock);
                for (const auto& e : gPendingEdits) {
                    queues.emplace_back(new ParamQueue());
                    queues.back()->id = e.first;
                    queues.back()->value = e.second;
                    changes.queues.push_back(queues.back().get());
                }
                gPendingEdits.clear();
            }
            Events events;
            {
                std::lock_guard<std::mutex> g(gNoteLock);
                int at = 0;
                for (const auto& n : gNotes) {
                    v3_event e = {};
                    e.sample_offset = at;
                    at = (at + 37) % 512;  // spread through the block
                    if (n.second > 0) {
                        e.type = V3_EVENT_NOTE_ON;
                        e.note_on.pitch = (int16_t)n.first; e.note_on.velocity = n.second / 127.0f; e.note_on.note_id = -1;
                    } else {
                        e.type = V3_EVENT_NOTE_OFF;
                        e.note_off.pitch = (int16_t)n.first; e.note_off.velocity = 0.5f; e.note_off.note_id = -1;
                    }
                    events.list.push_back(e);
                }
                gNotes.clear();
            }
            v3_process_data d = {};
            d.input_params = (v3_param_changes**)&changes;
            d.input_events = (v3_event_list**)&events;
            ParamChanges outChanges;
            d.output_params = (v3_param_changes**)&outChanges;
            d.process_mode = 0; d.symbolic_sample_size = 0; d.nframes = 512;
            d.num_input_buses = 0; d.num_output_buses = 1; d.inputs = nullptr; d.outputs = &out;
            if (v3_cpp_obj(proc)->process(proc, &d) != V3_OK) audioOk = false;
            float pk = gPeak.load();
            for (int i = 0; i < 512; ++i) {
                if (!std::isfinite(outL[i]) || !std::isfinite(outR[i]) || std::fabs(outL[i]) > 1 || std::fabs(outR[i]) > 1)
                    audioOk = false;
                pk = std::max(pk, std::max(std::fabs(outL[i]), std::fabs(outR[i])));
            }
            gPeak = pk;
            ++blocks;
            const double wait = start + blocks * 512 / 48000.0 - now();
            if (wait > 0) Sleep((DWORD)(wait * 1000));
        }
    });

    // ---- open the plugin window inside a host window
    v3_plugin_view** view = v3_cpp_obj(ctrl)->create_view(ctrl, "editor");
    CHECK(view, "create_view");
    CHECK(v3_cpp_obj(view)->is_platform_type_supported(view, "HWND") == V3_OK, "HWND supported");
    v3_view_rect r = {};
    v3_cpp_obj(view)->get_size(view, &r);
    std::printf("editor size %dx%d\n", r.right - r.left, r.bottom - r.top);
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA; wc.hInstance = GetModuleHandleA(nullptr); wc.lpszClassName = "minihost";
    RegisterClassA(&wc);
    RECT wr = {0, 0, r.right - r.left, r.bottom - r.top};
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    HWND win = CreateWindowA("minihost", "Scope Synth test host", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                             CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top,
                             nullptr, nullptr, wc.hInstance, nullptr);
    CHECK(win, "host window");
    static Frame frame;
    v3_cpp_obj(view)->set_frame(view, (v3_plugin_frame**)&frame);
    CHECK(v3_cpp_obj(view)->attached(view, win, "HWND") == V3_OK, "attached");
    HWND child = nullptr;
    EnumChildWindows(win, findChild, (LPARAM)&child);
    CHECK(child, "plugin child window");
    RECT cr;
    GetClientRect(child, &cr);
    const double sc = cr.right / 1020.0;  // the window's layout is 1020 wide
    auto X = [&](double x) { return (int)std::lround(x * sc); };
    auto Y = [&](double y) { return X(y + 28); };  // under the 28-high menu bar
    std::printf("window open (scale %.2f)\n", sc);

    // ---- MINIHOST_TOUR=C:\path\shot: no tests, a tour for the README's screenshots
    // (needs SCOPESYNTH_DUMP set too; each snapshot is asked for through <dump>.req)
    if (const char* tour = std::getenv("MINIHOST_TOUR")) {
        const char* dump = std::getenv("SCOPESYNTH_DUMP");
        auto theme = [&](int t) { click(child, X(1020 - 21), X(14)); click(child, X(900), X(28 + 4 + (t + 1) * 16 + 8)); };
        // the preset menu: categories on the left (row 0 INIT, 1 BASS, 2 LEAD ...), their presets on the right
        auto preset = [&](int cat, int i) {
            click(child, X(386), X(14));
            click(child, X(330), X(28 + 4 + cat * 16 + 8));
            if (cat > 0) click(child, X(500), X(28 + 4 + i * 16 + 8));
        };
        auto shot = [&](const char* name) {
            if (FILE* f = std::fopen((std::string(dump) + ".req").c_str(), "w")) {
                std::fprintf(f, "%s-%s.ppm\n", tour, name);
                std::fclose(f);
            }
            pump(1.0);
            std::printf("shot %s\n", name);
        };
        pump(1.0);
        theme(0); preset(2, 0);  // GREEN, LEAD / PHOSPHOR LEAD
        midi(57, 110); pump(2.5); shot("main"); midi(57, 0); pump(1.0);
        theme(4); preset(5, 0);  // PINK, PAD / AURORA PAD
        midi(48, 100); midi(55, 100); midi(60, 100); midi(64, 100); pump(4.0); shot("pad");
        midi(48, 0); midi(55, 0); midi(60, 0); midi(64, 0); pump(3.0);
        theme(2); preset(8, 1);  // CYAN, ARP / ARP SPARKLE
        midi(57, 110); pump(2.2); shot("arp"); midi(57, 0); pump(1.0);
        theme(1); preset(13, 0);  // AMBER, SCOPE ART / SPINNING CUBE
        midi(57, 110); pump(2.5); shot("art"); midi(57, 0); pump(0.5);
        click(child, X(386), X(14)); click(child, X(330), X(28 + 4 + 1 * 16 + 8)); pump(0.5); shot("presets"); click(child, X(900), X(600));  // the menu at BASS
        theme(3); preset(0, 0);  // WHITE, INIT
        click(child, X(897), Y(122));  // NEW f(t) on OSC A
        for (const char* t = "sin(3t) + 0.4sin(7t + u)"; *t; ++t) { PostMessageA(child, WM_CHAR, (WPARAM)*t, 0); pump(0.02); }
        PostMessageA(child, WM_KEYDOWN, VK_TAB, 0); PostMessageA(child, WM_KEYUP, VK_TAB, 0xC0000001); pump(0.05);
        for (const char* t = "cos(2t) - 0.3cos(5t)"; *t; ++t) { PostMessageA(child, WM_CHAR, (WPARAM)*t, 0); pump(0.02); }
        pump(1.0); shot("editor");
        PostMessageA(child, WM_KEYDOWN, VK_ESCAPE, 0); PostMessageA(child, WM_KEYUP, VK_ESCAPE, 0xC0000001); pump(0.3);
        theme(0); preset(1, 0);  // GREEN, BASS / SUB SCOPE, then SAVE it as your own
        click(child, X(620), X(14));
        for (const char* t = "deep sub"; *t; ++t) { PostMessageA(child, WM_CHAR, (WPARAM)*t, 0); pump(0.02); }
        PostMessageA(child, WM_KEYDOWN, VK_TAB, 0); PostMessageA(child, WM_KEYUP, VK_TAB, 0xC0000001); pump(0.05);
        for (const char* t = "night drive"; *t; ++t) { PostMessageA(child, WM_CHAR, (WPARAM)*t, 0); pump(0.02); }
        pump(0.6); shot("save");
        PostMessageA(child, WM_KEYDOWN, VK_ESCAPE, 0); PostMessageA(child, WM_KEYUP, VK_ESCAPE, 0xC0000001); pump(0.3);
        v3_cpp_obj(view)->removed(view);
        v3_cpp_obj_unref(view);
        DestroyWindow(win);
        running = false;
        audio.join();
        v3_cpp_obj(proc)->set_processing(proc, false);
        v3_cpp_obj(comp)->set_active(comp, false);
        std::printf("tour done\n");
        return 0;
    }

    // ---- the theme menu: the gear, then AMBER (row 2 under its title)
    click(child, X(1020 - 21), X(14));
    click(child, X(900), X(28 + 4 + 2 * 16 + 8));
    const float quiet = peakSince(1.0);
    std::printf("before any note: peak %.4f\n", quiet);

    // ---- a chord from the "piano roll"
    midi(60, 100); midi(64, 100); midi(67, 100);
    float chord = 0; for (int i = 0; i < 5; ++i) { const float pk = peakSince(0.5); chord = std::max(chord, pk); std::printf("  chord %.1fs: %.3f\n", 0.5 * i, pk); }
    std::printf("C major chord: peak %.3f\n", chord);

    // ---- link the LFO to OSC A's ROTATE knob: drag its handle onto the knob
    drag(child, X(967), Y(451), X(868), Y(50));
    std::printf("LFO dragged onto ROTATE: param %d -> %.3f\n", gLastParam.load(), gLastValue.load());
    const bool linked = gLastParam == base + 42 + 12 + 3 && std::fabs(gLastValue - 0.75) < 1e-3;  // LFO > A Rotate = +50%
    for (int i = 0; i < 6; ++i) std::printf("  after link %.1fs: %.3f\n", 0.5 * i, peakSince(0.5));
    midi(60, 0); midi(64, 0); midi(67, 0);
    const float after = peakSince(0.6), released = peakSince(1.0);
    std::printf("after letting go: %.3f, then %.4f\n", after, released);

    // ---- play the on-screen keyboard: C4 (white key 14 from C2), held for 4 s
    const int keyX = X(21.5 * 1020 / 50), keyY = Y(690);  // C4: 21 white keys up from C1
    PostMessageA(child, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(keyX, keyY));
    const float key = peakSince(4.0);  // the 13 s snapshot: C4 lit up
    PostMessageA(child, WM_LBUTTONUP, 0, MAKELPARAM(keyX, keyY));
    const float keyOff = (peakSince(0.5), peakSince(1.5));
    std::printf("on-screen key C4: peak %.3f, after letting go %.4f\n", key, keyOff);

    // ---- POLY -> MONO, then two overlapping notes play as one voice
    click(child, X(8 + 45), Y(500 + 38));
    std::printf("POLY switch: param %d -> %.3f\n", gLastParam.load(), gLastValue.load());
    const bool mono = gLastParam == base + 20 && gLastValue == 0;
    // ---- turn up OSC B's LEVEL
    drag(child, X(924), Y(189), X(924), Y(189 - 80));
    std::printf("OSC B LEVEL dragged up: param %d -> %.3f\n", gLastParam.load(), gLastValue.load());
    const bool levelUp = gLastParam == base + 12 && gLastValue > 0.3;
    // ---- type a shape: NEW f(t) on OSC A, x = sin(5t), y = cos(4t), Enter
    click(child, X(897), Y(122));
    auto press = [&](WPARAM vk) { PostMessageA(child, WM_KEYDOWN, vk, 0); PostMessageA(child, WM_KEYUP, vk, 0xC0000001); pump(0.05); };
    auto type = [&](const char* t) { for (; *t; ++t) { PostMessageA(child, WM_CHAR, (WPARAM)*t, 0); pump(0.02); } };
    type("sin(5tx"); press(VK_BACK); type(")"); press(VK_TAB); type("cos(4t)");  // Backspace takes the x back out
    pump(0.5);  // the 13 s snapshot may land around here
    press(VK_RETURN);
    pump(0.5);
    click(child, X(696), Y(122));  // +: a second slot on OSC A
    Stream st;
    v3_cpp_obj(comp)->get_state(comp, (v3_bstream**)&st);
    const std::string blob(st.data.begin(), st.data.end());
    // the typed shape is saved with the project; + is an automatable parameter (A Shapes: 1 -> 2 of 1..8)
    const bool typed = blob.find("C sin(5t) | cos(4t)") != std::string::npos;
    const bool slotAdded = gLastParam == base + 24 && std::fabs(gLastValue - 1.0 / 7) < 1e-6;
    std::printf("typed shape saved: %s, + slot -> param %d = %.3f\n", typed ? "yes" : "NO", gLastParam.load(), gLastValue.load());
    if (!typed) {
        std::string shown = blob;
        for (char& ch : shown) if (ch == 0) ch = '~';
        std::printf("state: %s\n", shown.c_str());
    }
    // ---- SUB's shape selector: click = next (CIRCLE -> ROSE); NOISE's: right-click = back (WHITE -> CRACKLE)
    click(child, X(298), Y(536));
    const bool subSel = gLastParam == base + 96 && std::fabs(gLastValue - 1.0 / 27) < 1e-4;
    PostMessageA(child, WM_RBUTTONDOWN, MK_RBUTTON, MAKELPARAM(X(380), Y(536)));
    PostMessageA(child, WM_RBUTTONUP, 0, MAKELPARAM(X(380), Y(536)));
    pump(0.3);
    const bool noiseSel = gLastParam == base + 97 && gLastValue == 1;
    std::printf("SUB shape click: %s, NOISE type right-click: %s\n", subSel ? "next" : "WRONG", noiseSel ? "back to CRACKLE" : "WRONG");
    // ---- turn the filter's CUTOFF knob down
    drag(child, X(608), Y(538), X(608), Y(538 + 60));
    std::printf("CUTOFF dragged down: param %d -> %.3f\n", gLastParam.load(), gLastValue.load());
    const bool cutoffDown = gLastParam == base + 67 && gLastValue < 0.8;
    midi(48, 110); pump(0.3); midi(55, 110);
    const float monoPk = peakSince(4.0);  // the 19.5 s snapshot
    midi(48, 0); midi(55, 0);
    pump(1.0);

    // ---- LFO DRAW: tick DRAW in the LFO's title line, then draw across the top of its graph
    click(child, X(500 + 146 + 10), Y(390 + 6));
    std::printf("DRAW click: param %d -> %.3f\n", gLastParam.load(), gLastValue.load());
    const bool drawOn = gLastParam == base + 71 && gLastValue == 1;  // LFO Draw
    drag(child, X(500 + 170), Y(390 + 22 + 10), X(500 + 400), Y(390 + 22 + 10));
    std::printf("DRAW ticked: %s; drawing set param %d -> %.3f\n", drawOn ? "yes" : "NO", gLastParam.load(), gLastValue.load());
    const bool drawn = drawOn && gLastParam >= base + 73 && gLastParam < base + 73 + 8 && std::fabs(gLastValue - (11.0 / 12 + 1) / 2) < 1e-4;

    // ---- presets: open the menu, point at BASS (left), pick SUB SCOPE (right, first), play it
    click(child, X(386), X(14));
    click(child, X(330), X(28 + 4 + 1 * 16 + 8));
    click(child, X(500), X(28 + 4 + 0 * 16 + 8));
    Stream ps;
    v3_cpp_obj(comp)->get_state(comp, (v3_bstream**)&ps);
    const std::string pblob(ps.data.begin(), ps.data.end());
    const bool presetSaved = pblob.find("SUB SCOPE") != std::string::npos;
    midi(36, 110);
    const float presetPk = peakSince(1.0);
    midi(36, 0);
    pump(0.5);
    std::printf("preset SUB SCOPE: saved %s, plays peak %.3f\n", presetSaved ? "yes" : "NO", presetPk);
    // a preset with its own typed shape: SCOPE ART / DNA (category 13, 4th)
    click(child, X(386), X(14));
    click(child, X(330), X(28 + 4 + 13 * 16 + 8));
    click(child, X(500), X(28 + 4 + 3 * 16 + 8));
    Stream ds;
    v3_cpp_obj(comp)->get_state(comp, (v3_bstream**)&ds);
    const std::string dblob(ds.data.begin(), ds.data.end());
    const bool dnaShape = dblob.find("C sin(t) | 0.6sin(3t + u)") != std::string::npos && dblob.find("DNA") != std::string::npos;
    midi(57, 110);
    const float dnaPk = peakSince(1.0);
    midi(57, 0);
    pump(0.8);
    std::printf("preset DNA: its shape saved %s, plays peak %.3f\n", dnaShape ? "yes" : "NO", dnaPk);
    std::printf("MONO notes with both oscillators: peak %.3f\n", monoPk);

    // ---- your own presets: SAVE one, find it in the menu under YOUR PACKS, export the pack,
    // delete the preset, import the pack back
    auto pickFile = [&](const char* title, const char* file) {
        HWND dlg = nullptr;
        for (int i = 0; i < 50 && !dlg; ++i) { pump(0.1); dlg = FindWindowA(nullptr, title); }
        if (!dlg) { std::printf("file dialog \"%s\" DID NOT OPEN\n", title); return false; }
        SetDlgItemTextA(dlg, 0x47c, file);
        SetDlgItemTextA(dlg, 0x480, file);
        PostMessageA(dlg, WM_COMMAND, IDOK, 0);
        pump(1.5);
        return true;
    };
    const std::string presetDir = std::string(std::getenv("APPDATA")) + "\\ScopeSynth\\Presets\\MY PRESETS";
    const std::string savedFile = presetDir + "\\MY TEST.ssp";
    DeleteFileA(savedFile.c_str());
    click(child, X(620), X(14));  // SAVE
    type("my test"); press(VK_TAB); type("my presets"); press(VK_RETURN);  // the filled-in text is replaced by typing
    pump(0.5);
    const bool savedOk = GetFileAttributesA(savedFile.c_str()) != INVALID_FILE_ATTRIBUTES;
    Stream us;
    v3_cpp_obj(comp)->get_state(comp, (v3_bstream**)&us);
    const std::string ublob(us.data.begin(), us.data.end());
    std::printf("SAVE: file %s, the project says %s\n", savedOk ? "written" : "MISSING", ublob.find("MY PRESETS/MY TEST") != std::string::npos ? "MY PRESETS/MY TEST" : "SOMETHING ELSE");
    // the menu: INIT, 13 categories, # YOUR PACKS, then the packs (MY PRESETS first if it's the only one)
    click(child, X(1020 - 100), X(14));  // (somewhere harmless first: the gear menu opens and closes)
    click(child, X(700), X(600));
    click(child, X(386), X(14));
    click(child, X(330), X(28 + 4 + 15 * 16 + 8));  // MY PRESETS
    click(child, X(520), X(28 + 4 + 0 * 16 + 8));   // MY TEST
    Stream us2;
    v3_cpp_obj(comp)->get_state(comp, (v3_bstream**)&us2);
    const std::string ublob2(us2.data.begin(), us2.data.end());
    const bool loadedOwn = ublob2.find("MY PRESETS/MY TEST") != std::string::npos;
    // export: the pack, then EXPORT PACK... (row 1 on the right: under its one preset)
    const char* exported = "C:\\vst\\my-presets.sspack";
    DeleteFileA(exported);
    click(child, X(386), X(14));
    click(child, X(330), X(28 + 4 + 15 * 16 + 8));
    click(child, X(520), X(28 + 4 + 1 * 16 + 8));
    pickFile("Export a Scope Synth pack (.sspack)", exported);
    const bool exportedOk = GetFileAttributesA(exported) != INVALID_FILE_ATTRIBUTES;
    // delete: right-click it (asks), click it (gone)
    click(child, X(386), X(14));
    click(child, X(330), X(28 + 4 + 15 * 16 + 8));
    PostMessageA(child, WM_RBUTTONDOWN, MK_RBUTTON, MAKELPARAM(X(520), X(28 + 4 + 8)));
    PostMessageA(child, WM_RBUTTONUP, 0, MAKELPARAM(X(520), X(28 + 4 + 8)));
    pump(0.3);
    click(child, X(520), X(28 + 4 + 8));
    click(child, X(700), X(600));
    const bool deletedOk = GetFileAttributesA(savedFile.c_str()) == INVALID_FILE_ATTRIBUTES;
    // import the pack back: + IMPORT... is under the packs (none left now, so right under the header)
    click(child, X(386), X(14));
    click(child, X(330), X(28 + 4 + 15 * 16 + 8));
    pickFile("Import Scope Synth presets (.ssp or .sspack)", exported);
    const bool importedOk = GetFileAttributesA(savedFile.c_str()) != INVALID_FILE_ATTRIBUTES;
    std::printf("own presets: loaded from the menu %s, exported %s, deleted %s, imported back %s\n", loadedOwn ? "yes" : "NO",
                exportedOk ? "yes" : "NO", deletedOk ? "yes" : "NO", importedOk ? "yes" : "NO");
    const bool ownPresets = savedOk && loadedOwn && exportedOk && deletedOk && importedOk;
    DeleteFileA(savedFile.c_str());
    RemoveDirectoryA(presetDir.c_str());

    // ---- close the window, stop audio, unload (what FL does when you remove the plugin)
    v3_cpp_obj(view)->removed(view);
    v3_cpp_obj_unref(view);
    DestroyWindow(win);
    running = false;
    audio.join();
    std::printf("audio blocks processed: %d, sound %s\n", blocks.load(), audioOk ? "clean (finite, never over 1)" : "BAD");
    v3_cpp_obj(proc)->set_processing(proc, false);
    v3_cpp_obj(comp)->set_active(comp, false);
    v3_cpp_obj_unref(proc);
    if (separate) {
        v3_cpp_obj_terminate(ctrl);
        v3_cpp_obj_unref(ctrl);
    } else {
        v3_cpp_obj_unref(ctrl);
    }
    v3_cpp_obj_terminate(comp);
    v3_cpp_obj_unref(comp);
    v3_cpp_obj_unref(factory);
    if (auto exitDll = (bool (*)())GetProcAddress(lib, "ExitDll")) exitDll();
    FreeLibrary(lib);
    CHECK(audioOk, "sound clean");
    CHECK(quiet < 1e-4f, "silent before any note");
    CHECK(chord > 0.05f, "a chord makes sound");
    CHECK(linked, "dragging the LFO onto ROTATE should link it");
    CHECK(released < 1e-3f, "silent after the notes are let go");
    CHECK(key > 0.05f && keyOff < 1e-3f, "the on-screen keys play");
    CHECK(mono, "the POLY switch should go to MONO");
    CHECK(levelUp, "dragging OSC B LEVEL up should raise it");
    CHECK(monoPk > 0.05f, "MONO notes make sound");
    CHECK(typed, "a typed shape should be saved with the project");
    CHECK(drawn, "ticking DRAW and drawing in the LFO graph should set its steps");
    CHECK(subSel && noiseSel, "the SUB shape and NOISE type selectors should step through their choices");
    CHECK(cutoffDown, "dragging CUTOFF down should lower it");
    CHECK(ownPresets, "saving, loading, exporting, deleting and importing your own presets should all work");
    CHECK(dnaShape && dnaPk > 0.05f, "a preset's typed shape should load with it and play");
    CHECK(presetSaved && presetPk > 0.05f, "picking a preset from the menu should load it and play");
    CHECK(slotAdded, "+ should add a slot through the A Shapes parameter");
    std::printf("ALL OK\n");
    return 0;
}
