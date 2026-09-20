// Quasizero Slicer - engine client implementation. GNU AGPLv3, part of the Quasizero fork of OrcaSlicer.
#include "QzEngine.hpp"

#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace Slic3r {

using nlohmann::json;

// ---- process (native pipes) ------------------------------------------------------------

struct QzEngineProcess::Impl
{
#ifdef _WIN32
    HANDLE process = nullptr, child_stdin = nullptr, child_stdout = nullptr;
#else
    pid_t pid = -1;
    int   child_stdin = -1, child_stdout = -1;
#endif
    bool started = false;
};

QzEngineProcess::QzEngineProcess() : m_impl(new Impl) {}
QzEngineProcess::~QzEngineProcess() { stop(); }

#ifdef _WIN32
static std::wstring to_wide(const std::string &s)
{
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int) s.size(), nullptr, 0);
    std::wstring w((size_t) n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int) s.size(), &w[0], n);
    return w;
}
static std::wstring quote_arg(const std::wstring &a)
{
    if (a.find_first_of(L" \t\"") == std::wstring::npos) return a;
    std::wstring q = L"\"";
    for (wchar_t c : a) { if (c == L'"') q += L"\\\""; else q += c; }
    return q + L"\"";
}
#endif

bool QzEngineProcess::start(const std::string &exe, const std::vector<std::string> &args, std::string &error)
{
    stop();
    m_buffer.clear();
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{}; sa.nLength = sizeof sa; sa.bInheritHandle = TRUE;
    HANDLE in_r = nullptr, in_w = nullptr, out_r = nullptr, out_w = nullptr;
    if (!CreatePipe(&in_r, &in_w, &sa, 0) || !CreatePipe(&out_r, &out_w, &sa, 1 << 20)) { error = "cannot create pipes"; return false; }
    SetHandleInformation(in_w, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0);
    std::wstring cmd = quote_arg(to_wide(exe)) + L" serve";
    for (const std::string &a : args) cmd += L" " + quote_arg(to_wide(a));
    // the engine's stderr goes nowhere (a GUI application has no console)
    HANDLE nul = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, &sa, OPEN_EXISTING, 0, nullptr);
    STARTUPINFOW si{}; si.cb = sizeof si; si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = in_r; si.hStdOutput = out_w; si.hStdError = nul;
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> cmdbuf(cmd.begin(), cmd.end()); cmdbuf.push_back(L'\0');
    const BOOL ok = CreateProcessW(nullptr, cmdbuf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(in_r); CloseHandle(out_w);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    if (!ok) { CloseHandle(in_w); CloseHandle(out_r); error = "cannot start " + exe + " (error " + std::to_string(GetLastError()) + ")"; return false; }
    CloseHandle(pi.hThread);
    m_impl->process = pi.hProcess; m_impl->child_stdin = in_w; m_impl->child_stdout = out_r;
#else
    int in_pipe[2], out_pipe[2];
    if (pipe(in_pipe) != 0 || pipe(out_pipe) != 0) { error = "cannot create pipes"; return false; }
    const pid_t pid = fork();
    if (pid < 0) { error = "cannot fork"; return false; }
    if (pid == 0) {
        dup2(in_pipe[0], STDIN_FILENO); dup2(out_pipe[1], STDOUT_FILENO);
        close(in_pipe[0]); close(in_pipe[1]); close(out_pipe[0]); close(out_pipe[1]);
        std::vector<char *> argv;
        argv.push_back(const_cast<char *>(exe.c_str()));
        argv.push_back(const_cast<char *>("serve"));
        for (const std::string &a : args) argv.push_back(const_cast<char *>(a.c_str()));
        argv.push_back(nullptr);
        execv(exe.c_str(), argv.data());
        _exit(127);
    }
    close(in_pipe[0]); close(out_pipe[1]);
    m_impl->pid = pid; m_impl->child_stdin = in_pipe[1]; m_impl->child_stdout = out_pipe[0];
#endif
    m_impl->started = true;
    return true;
}

void QzEngineProcess::stop()
{
    if (!m_impl->started) return;
#ifdef _WIN32
    if (m_impl->child_stdin) { CloseHandle(m_impl->child_stdin); m_impl->child_stdin = nullptr; }
    if (m_impl->process) {
        if (WaitForSingleObject(m_impl->process, 500) != WAIT_OBJECT_0) TerminateProcess(m_impl->process, 1);
        CloseHandle(m_impl->process); m_impl->process = nullptr;
    }
    if (m_impl->child_stdout) { CloseHandle(m_impl->child_stdout); m_impl->child_stdout = nullptr; }
#else
    if (m_impl->child_stdin >= 0) { close(m_impl->child_stdin); m_impl->child_stdin = -1; }
    if (m_impl->pid > 0) {
        int status = 0;
        for (int i = 0; i < 50 && waitpid(m_impl->pid, &status, WNOHANG) == 0; ++i) usleep(10000);
        if (waitpid(m_impl->pid, &status, WNOHANG) == 0) { kill(m_impl->pid, SIGKILL); waitpid(m_impl->pid, &status, 0); }
        m_impl->pid = -1;
    }
    if (m_impl->child_stdout >= 0) { close(m_impl->child_stdout); m_impl->child_stdout = -1; }
#endif
    m_impl->started = false;
}

bool QzEngineProcess::alive() const
{
    if (!m_impl->started) return false;
#ifdef _WIN32
    return m_impl->process && WaitForSingleObject(m_impl->process, 0) == WAIT_TIMEOUT;
#else
    if (m_impl->pid <= 0) return false;
    int status = 0;
    return waitpid(m_impl->pid, &status, WNOHANG) == 0;
#endif
}

bool QzEngineProcess::write_line(const std::string &line)
{
    if (!m_impl->started) return false;
    std::string data = line; data += '\n';
    size_t done = 0;
    while (done < data.size()) {
#ifdef _WIN32
        DWORD n = 0;
        if (!WriteFile(m_impl->child_stdin, data.data() + done, (DWORD) (data.size() - done), &n, nullptr) || n == 0) return false;
#else
        const ssize_t n = write(m_impl->child_stdin, data.data() + done, data.size() - done);
        if (n < 0) { if (errno == EINTR) continue; return false; }
        if (n == 0) return false;
#endif
        done += (size_t) n;
    }
    return true;
}

// read more bytes from the child into m_buffer; false on EOF/error
bool qz_engine_process_read_some(QzEngineProcess::Impl &impl, std::string &buffer)
{
    char chunk[65536];
#ifdef _WIN32
    DWORD n = 0;
    if (!ReadFile(impl.child_stdout, chunk, sizeof chunk, &n, nullptr) || n == 0) return false;
#else
    ssize_t n;
    do { n = read(impl.child_stdout, chunk, sizeof chunk); } while (n < 0 && errno == EINTR);
    if (n <= 0) return false;
#endif
    buffer.append(chunk, (size_t) n);
    return true;
}

bool QzEngineProcess::read_line(std::string &line)
{
    if (!m_impl->started) return false;
    for (;;) {
        const size_t nl = m_buffer.find('\n');
        if (nl != std::string::npos) {
            line.assign(m_buffer, 0, nl);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            m_buffer.erase(0, nl + 1);
            return true;
        }
        if (!qz_engine_process_read_some(*m_impl, m_buffer)) return false;
    }
}

bool QzEngineProcess::read_bytes(size_t n, std::string &out)
{
    if (!m_impl->started) return false;
    while (m_buffer.size() < n)
        if (!qz_engine_process_read_some(*m_impl, m_buffer)) return false;
    out.assign(m_buffer, 0, n);
    m_buffer.erase(0, n);
    return true;
}

// ---- discovery -------------------------------------------------------------------------

const char *qz_engine_exe_name()
{
#ifdef _WIN32
    return "qz-sim.exe";
#else
    return "qz-sim";
#endif
}

std::string qz_user_engine_dir()
{
#ifdef _WIN32
    const char *base = std::getenv("LOCALAPPDATA");
    if (!base || !*base) base = std::getenv("APPDATA");
    if (!base || !*base) return std::string();
    return std::string(base) + "\\Quasizero\\qz-sim";
#else
    const char *xdg = std::getenv("XDG_DATA_HOME");
    if (xdg && *xdg) return std::string(xdg) + "/quasizero/qz-sim";
    const char *home = std::getenv("HOME");
    if (!home || !*home) return std::string();
    return std::string(home) + "/.local/share/quasizero/qz-sim";
#endif
}

static bool file_exists(const std::string &p)
{
    if (p.empty()) return false;
#ifdef _WIN32
    const DWORD a = GetFileAttributesW(to_wide(p).c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st{};
    return stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode);
#endif
}

std::vector<std::string> qz_engine_candidates(const std::string &configured, const std::string &slicer_dir)
{
    std::vector<std::string> c;
    const std::string exe = qz_engine_exe_name();
#ifdef _WIN32
    const char sep = '\\';
#else
    const char sep = '/';
#endif
    if (!configured.empty()) c.push_back(configured);
    if (const char *env = std::getenv("QZ_SIM"); env && *env) c.push_back(env);
    if (!slicer_dir.empty()) { c.push_back(slicer_dir + sep + "qz-sim" + sep + exe); c.push_back(slicer_dir + sep + exe); }
    if (std::string u = qz_user_engine_dir(); !u.empty()) c.push_back(u + sep + exe);
#ifdef _WIN32
    if (const char *pf = std::getenv("ProgramFiles"); pf && *pf) c.push_back(std::string(pf) + "\\Quasizero\\qz-sim\\qz-sim.exe");
#else
    c.push_back("/usr/local/bin/qz-sim");
    c.push_back("/opt/quasizero/qz-sim/qz-sim");
#endif
    if (const char *path = std::getenv("PATH"); path && *path) {
#ifdef _WIN32
        const char psep = ';';
#else
        const char psep = ':';
#endif
        std::string p(path); size_t start = 0;
        while (start <= p.size()) {
            const size_t end = p.find(psep, start);
            const std::string dir = p.substr(start, end == std::string::npos ? std::string::npos : end - start);
            if (!dir.empty()) c.push_back(dir + sep + exe);
            if (end == std::string::npos) break;
            start = end + 1;
        }
    }
    return c;
}

std::string qz_find_engine(const std::string &configured, const std::string &slicer_dir)
{
    for (const std::string &p : qz_engine_candidates(configured, slicer_dir))
        if (file_exists(p)) return p;
    return std::string();
}

// ---- protocol client -------------------------------------------------------------------

bool QzEngineClient::request(const json &req, json &reply, std::string *binary)
{
    m_error.clear();
    if (!m_transport) { m_error = "engine not started"; return false; }
    if (!m_transport->write_line(req.dump())) { m_error = "engine not responding (write)"; return false; }
    std::string line;
    if (!m_transport->read_line(line)) { m_error = "engine stopped"; return false; }
    try { reply = json::parse(line); } catch (const std::exception &e) { m_error = std::string("bad reply: ") + e.what(); return false; }
    if (reply.is_object() && reply.contains("bytes")) {
        const size_t n = reply["bytes"].get<size_t>();
        std::string blob;
        if (!m_transport->read_bytes(n, blob)) { m_error = "engine stopped (binary block)"; return false; }
        if (binary) *binary = std::move(blob);
    }
    if (!reply.is_object() || !reply.value("ok", false)) {
        m_error = reply.is_object() ? reply.value("error", "error") + ": " + reply.value("detail", std::string()) : "bad reply";
        return false;
    }
    return true;
}

void QzEngineClient::fill_licence(const json &reply, QzEngineStatus &s)
{
    s.licensed = reply.value("licensed", false);
    s.licence_required = reply.value("required", true);
    s.licence_path = reply.value("licence_path", std::string());
    if (auto it = reply.find("licence"); it != reply.end() && it->is_object()) {
        const json &l = *it;
        s.licensee = l.value("licensee", std::string()); s.licence_id = l.value("id", std::string());
        s.expires = l.value("expires", std::string()); s.licence_error = l.value("error", std::string());
        s.licence_signed = l.value("signed", false); s.licence_expired = l.value("expired", false);
    }
}

bool QzEngineClient::hello(QzEngineStatus &s)
{
    json reply;
    s.running = false;
    if (!request(json{ { "op", "hello" } }, reply)) { s.error = m_error; return false; }
    s.running = true;
    s.version = reply.value("version", std::string());
    s.protocol = reply.value("protocol", std::string());
    fill_licence(reply, s);
    if (s.protocol != "qz-sim-serve/1") { s.error = "engine protocol " + s.protocol + " is not supported by this slicer"; return false; }
    s.error.clear();
    return true;
}

bool QzEngineClient::licence(const std::string &path, bool install, QzEngineStatus &s)
{
    json req; req["op"] = "licence";
    if (install) req["install"] = path; else req["path"] = path;
    json reply;
    if (!request(req, reply)) { s.error = m_error; return false; }
    fill_licence(reply, s);
    s.error.clear();
    return true;
}

bool QzEngineClient::load(const json &job, QzEngineLoadInfo &info)
{
    info = QzEngineLoadInfo();
    json reply;
    if (!request(json{ { "op", "load" }, { "job", job } }, reply)) { info.error = m_error; return false; }
    info.ok = true;
    if (auto it = reply.find("sim"); it != reply.end() && it->is_object()) {
        const json &sj = *it;
        info.sim_valid = sj.value("valid", false);
        info.collapse_step = sj.value("collapse_step", -1); info.collapse_time = sj.value("collapse_time", 0.0);
        info.hinge_layer = sj.value("hinge_layer", -1); info.by_buckling = sj.value("by_buckling", false);
        info.segments = sj.value("segments", (size_t) 0); info.bead_valid = sj.value("bead_valid", false);
    }
    if (auto it = reply.find("skeleton"); it != reply.end() && it->is_object()) info.nodes = it->value("nodes", (size_t) 0);
    return true;
}

bool QzEngineClient::frame(int top, double time, int seg_lo, int seg_hi, QzEngineFrame &f, std::string &error)
{
    json reply; std::string bin;
    f.valid = false; f.chains.clear();
    if (!request(json{ { "op", "frame" }, { "top", top }, { "time", time }, { "seg_lo", seg_lo }, { "seg_hi", seg_hi } }, reply, &bin)) { error = m_error; return false; }
    const json &fr = reply["frame"];
    f.valid = fr.value("valid", false);
    if (!f.valid) return true;
    f.phase = fr.value("phase", std::string("stable"));
    f.fold_angle_deg = fr.value("fold_angle_deg", 0.0); f.settle_f = fr.value("settle_f", 0.0); f.sway = fr.value("sway", 0.0);
    f.height_deformed = fr.value("height_deformed", 0.0); f.max_util = fr.value("max_util", 0.0); f.max_ratio = fr.value("max_ratio", 0.0);
    f.r_hinge = fr.value("r_hinge", 0.0); f.contact = fr.value("contact", false); f.collapsed = fr.value("collapsed", false);
    f.by_buckling = fr.value("by_buckling", false); f.fallen_count = fr.value("fallen_count", 0);
    f.hinge_layer = fr.value("hinge_layer", -1); f.k_collapse = fr.value("k_collapse", -1);
    f.strand_mm = fr.value("strand_mm", 0.0); f.strand_at_rest_mm = fr.value("strand_at_rest_mm", 0.0);
    f.has_bead = fr.contains("strand_mm");
    f.bead_w = reply.value("bead_w", 0.0f); f.bead_h = reply.value("bead_h", 0.0f);
    f.has_nozzle = false;
    if (auto it = reply.find("nozzle"); it != reply.end() && it->is_array() && it->size() == 3) {
        f.has_nozzle = true;
        for (size_t k = 0; k < 3; ++k) f.nozzle[k] = (*it)[k].get<float>();
    }
    const size_t n = reply.value("nodes", (size_t) 0);
    size_t chain_pts = 0;
    std::vector<size_t> counts;
    if (auto it = reply.find("chains"); it != reply.end() && it->is_array())
        for (const json &c : *it) { counts.push_back(c.get<size_t>()); chain_pts += counts.back(); }
    if (bin.size() != n * 28 + chain_pts * 12) { error = "engine frame block has an unexpected size"; f.valid = false; return false; }
    f.pose.resize(n);
    const char *p = bin.data();
    for (size_t i = 0; i < n; ++i, p += 28) {
        float v[7]; std::memcpy(v, p, 28);
        QuasiZero::QzSkelNodePose &q = f.pose[i];
        q.x = v[0]; q.y = v[1]; q.z = v[2]; q.w_scale = v[3]; q.h_scale = v[4]; q.value = v[5]; q.visible = v[6] > 0.5f;
    }
    f.chains.resize(counts.size());
    for (size_t c = 0; c < counts.size(); ++c) {
        f.chains[c].resize(counts[c]);
        for (size_t k = 0; k < counts[c]; ++k, p += 12) { float v[3]; std::memcpy(v, p, 12); f.chains[c][k] = { v[0], v[1], v[2] }; }
    }
    return true;
}

bool QzEngineClient::unload()
{
    json reply;
    return request(json{ { "op", "unload" } }, reply);
}

void QzEngineClient::quit()
{
    if (!m_transport) return;
    m_transport->write_line("{\"op\":\"quit\"}");
    std::string line; m_transport->read_line(line);
}

// ---- job -------------------------------------------------------------------------------

json qz_engine_job(const QuasiZero::QzPasteMaterial &m, const QuasiZero::QzStabilityOptions &o,
                   const std::vector<QuasiZero::QzLayerRecord> &layers, const std::vector<QuasiZero::QzLayerGeom> &geom,
                   const std::vector<QuasiZero::QzSimSegment> &segments)
{
    json j;
    j["schema"] = "qz-sim-job/1";
    j["material"] = { { "rho", m.rho }, { "tau0", m.tau0 }, { "athix", m.athix }, { "E0", m.E0 }, { "xiE", m.xiE }, { "nu", m.nu }, { "kp", m.kp } };
    j["stability"] = { { "base_confinement", o.base_confinement }, { "confinement_length", o.confinement_length }, { "safety_factor", o.safety_factor } };
    json L = json::array();
    for (const QuasiZero::QzLayerRecord &r : layers) L.push_back({ { "z_bottom", r.z_bottom }, { "height", r.height }, { "t_start", r.t_start }, { "t_end", r.t_end }, { "thickness", r.thickness } });
    json G = json::array();
    for (const QuasiZero::QzLayerGeom &g : geom) G.push_back({ { "cx", g.cx }, { "cy", g.cy }, { "area", g.area }, { "I_min", g.I_min }, { "dir_x", g.dir_x }, { "dir_y", g.dir_y }, { "r_max", g.r_max } });
    json S = json::array();
    for (const QuasiZero::QzSimSegment &s : segments) S.push_back({ { "x0", s.x0 }, { "y0", s.y0 }, { "x1", s.x1 }, { "y1", s.y1 }, { "z", s.z }, { "w", s.w }, { "h", s.h }, { "layer", s.layer }, { "t_end", s.t_end } });
    j["layers"] = std::move(L); j["geom"] = std::move(G); j["segments"] = std::move(S);
    j["output"] = { { "history", false }, { "poses", true }, { "bead", true }, { "field", false } };
    return j;
}

} // namespace Slic3r
