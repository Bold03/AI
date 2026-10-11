// AICopilotLogger - plugin X-Plane untuk merekam state pesawat + input pilot ke CSV.
// Dataref yang direkam ditentukan oleh file profil di folder "profiles":
//   standard.txt  -> selalu dimuat (dataref bawaan X-Plane)
//   <ICAO>.txt    -> dimuat sesuai pesawat (Zibo 737 = B738.txt)
//
// Format profil, satu baris satu dataref:
//   nama_kolom  nama/dataref            (skalar)
//   nama_kolom  nama/dataref[3]         (elemen array index 3)
// Baris berawalan # diabaikan.

#include <XPLMDataAccess.h>
#include <XPLMPlugin.h>
#include <XPLMProcessing.h>
#include <XPLMUtilities.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#if IBM
#define PLUGIN_API extern "C" __declspec(dllexport)
#else
#define PLUGIN_API extern "C" __attribute__((visibility("default")))
#endif

namespace fs = std::filesystem;

namespace {

constexpr float kInterval = 0.1f;  // 10 Hz

struct Ref {
    std::string column;
    std::string name;
    int index;  // -1 kalau bukan elemen array
    XPLMDataRef ref;
    XPLMDataTypeID type;
};

std::vector<Ref> g_refs;
std::ofstream g_csv;
fs::path g_root;       // folder X-Plane
fs::path g_pluginDir;  // folder AICopilotLogger (induk dari win_x64 / lin_x64 / mac_x64)
float g_t0 = 0.f;
int g_rowCount = 0;
XPLMDataRef g_paused = nullptr;
XPLMDataRef g_icao = nullptr;

void Log(const std::string& s) {
    XPLMDebugString(("[AICopilot] " + s + "\n").c_str());
}

std::string AircraftICAO() {
    char buf[64] = {0};
    if (g_icao) XPLMGetDatab(g_icao, buf, 0, 63);
    std::string out;
    for (char c : std::string(buf)) {
        if (std::isalnum(static_cast<unsigned char>(c))) out += static_cast<char>(std::toupper(c));
    }
    return out.empty() ? "UNKNOWN" : out;
}

double ReadRef(const Ref& r) {
    const XPLMDataTypeID t = r.type;
    if (r.index >= 0) {
        if (t & xplmType_FloatArray) {
            float v = 0;
            XPLMGetDatavf(r.ref, &v, r.index, 1);
            return v;
        }
        if (t & xplmType_IntArray) {
            int v = 0;
            XPLMGetDatavi(r.ref, &v, r.index, 1);
            return v;
        }
        return 0;
    }
    if (t & xplmType_Double) return XPLMGetDatad(r.ref);
    if (t & xplmType_Float) return XPLMGetDataf(r.ref);
    if (t & xplmType_Int) return XPLMGetDatai(r.ref);
    return 0;
}

void LoadProfile(const fs::path& path) {
    std::ifstream in(path);
    if (!in) {
        Log("Profil tidak ditemukan (dilewati): " + path.string());
        return;
    }
    Log("Memuat profil: " + path.string());
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string col, name;
        ss >> col >> name;
        if (col.empty() || name.empty()) continue;

        int idx = -1;
        const auto b = name.find('[');
        if (b != std::string::npos) {
            idx = std::atoi(name.c_str() + b + 1);
            name = name.substr(0, b);
        }
        XPLMDataRef ref = XPLMFindDataRef(name.c_str());
        if (!ref) {
            Log("Dataref TIDAK ADA, dilewati: " + name);
            continue;
        }
        g_refs.push_back({col, name, idx, ref, XPLMGetDataRefTypes(ref)});
    }
}

void CloseSession() {
    if (g_csv.is_open()) {
        g_csv.flush();
        g_csv.close();
    }
}

void StartSession() {
    CloseSession();
    g_refs.clear();

    const std::string icao = AircraftICAO();
    const fs::path profiles = g_pluginDir / "profiles";
    LoadProfile(profiles / "standard.txt");
    LoadProfile(profiles / (icao + ".txt"));

    std::error_code ec;
    const fs::path dir = g_root / "Output" / "copilot_logs";
    fs::create_directories(dir, ec);

    char stamp[32];
    std::time_t now = std::time(nullptr);
    std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", std::localtime(&now));
    const fs::path file = dir / ("flight_" + icao + "_" + stamp + ".csv");

    g_csv.open(file);
    if (!g_csv) {
        Log("Gagal membuka file log: " + file.string());
        return;
    }
    g_csv << "t";
    for (const auto& r : g_refs) g_csv << "," << r.column;
    g_csv << "\n";

    g_t0 = XPLMGetElapsedTime();
    g_rowCount = 0;
    Log("Merekam " + std::to_string(g_refs.size()) + " dataref ke " + file.string());
}

float FlightLoop(float, float, int, void*) {
    if (!g_csv.is_open()) return 1.0f;
    if (g_paused && XPLMGetDatai(g_paused)) return kInterval;

    g_csv << (XPLMGetElapsedTime() - g_t0);
    for (const auto& r : g_refs) g_csv << "," << ReadRef(r);
    g_csv << "\n";
    if (++g_rowCount % 10 == 0) g_csv.flush();
    return kInterval;
}

}  // namespace

PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc) {
    std::strcpy(outName, "AI Copilot Logger");
    std::strcpy(outSig, "com.aicopilot.logger");
    std::strcpy(outDesc, "Merekam state pesawat dan input pilot ke CSV");

    XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);

    char sys[512];
    XPLMGetSystemPath(sys);
    g_root = sys;

    char plug[512];
    XPLMGetPluginInfo(XPLMGetMyID(), nullptr, plug, nullptr, nullptr);
    g_pluginDir = fs::path(plug).parent_path().parent_path();

    g_paused = XPLMFindDataRef("sim/time/paused");
    g_icao = XPLMFindDataRef("sim/aircraft/view/acf_ICAO");
    return 1;
}

PLUGIN_API void XPluginStop() { CloseSession(); }

PLUGIN_API int XPluginEnable() {
    XPLMRegisterFlightLoopCallback(FlightLoop, kInterval, nullptr);
    StartSession();
    return 1;
}

PLUGIN_API void XPluginDisable() {
    XPLMUnregisterFlightLoopCallback(FlightLoop, nullptr);
    CloseSession();
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID, int msg, void* param) {
    // Pesawat user baru dimuat -> mulai sesi log baru dengan profil pesawat itu
    if (msg == XPLM_MSG_PLANE_LOADED && reinterpret_cast<intptr_t>(param) == 0) {
        StartSession();
    }
}
