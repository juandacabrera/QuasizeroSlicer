// Quasizero Slicer - client of the Quasizero simulation engine (`qz-sim serve`), the
// external, separately licensed process that computes the PRO deformation view.
// GNU AGPLv3, part of the Quasizero fork of OrcaSlicer.
//
// The slicer never links the engine: it starts `qz-sim serve`, talks the documented
// protocol qz-sim-serve/1 over the child's stdin/stdout (one JSON object per line; a
// reply may be followed by a binary block whose size it announces in "bytes") and draws
// what comes back. No engine, no licence: the PRO rows of the Stability card explain how
// to get one and the rest of the slicer is unaffected.
#pragma once

#include "libslic3r/QuasiZero/QzSkeleton.hpp"
#include "libslic3r/QuasiZero/QzStabilityModel.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Slic3r {

// ---- transport -------------------------------------------------------------------------

// a line/bytes channel to the engine process (native pipes; a test can provide its own)
class QzEngineTransport
{
public:
    virtual ~QzEngineTransport() = default;
    virtual bool write_line(const std::string &line) = 0;   // line without the trailing '\n'
    virtual bool read_line(std::string &line) = 0;          // without the '\n'; false on EOF/error
    virtual bool read_bytes(size_t n, std::string &out) = 0;
    virtual bool alive() const = 0;
};

// the engine as a child process with piped stdin/stdout (Win32 or POSIX; no boost)
class QzEngineProcess : public QzEngineTransport
{
public:
    QzEngineProcess();
    ~QzEngineProcess() override;
    // start `exe serve <args>`; false with `error` when it cannot be started
    bool start(const std::string &exe, const std::vector<std::string> &args, std::string &error);
    void stop();   // asks nothing: closes the pipes and waits briefly, then terminates
    bool write_line(const std::string &line) override;
    bool read_line(std::string &line) override;
    bool read_bytes(size_t n, std::string &out) override;
    bool alive() const override;
    struct Impl;
private:
    std::unique_ptr<Impl> m_impl;
    std::string           m_buffer;   // bytes read ahead of the current line
};

// ---- discovery -------------------------------------------------------------------------

// where qz-sim may be: the configured path, next to the slicer (qz-sim/ subfolder), the
// per-user install of the PRO add-on, system locations, PATH. First existing wins.
std::vector<std::string> qz_engine_candidates(const std::string &configured, const std::string &slicer_dir);
std::string              qz_find_engine(const std::string &configured, const std::string &slicer_dir);
// executable name of the engine on this platform
const char *qz_engine_exe_name();

// ---- protocol client -------------------------------------------------------------------

struct QzEngineStatus
{
    bool        found = false;         // an executable exists
    bool        running = false;       // the process answered `hello`
    bool        licensed = false;      // the licence grants PRO (or the engine runs without licence checks)
    bool        licence_required = true;
    std::string exe_path, version, protocol;
    std::string licence_path, licensee, licence_id, expires, licence_error;
    bool        licence_signed = false, licence_expired = false;
    std::string error;                 // why the engine is not usable, for the UI
};

struct QzEngineLoadInfo
{
    bool   ok = false;
    bool   sim_valid = false;
    int    collapse_step = -1;
    double collapse_time = 0.0;
    int    hinge_layer = -1;
    bool   by_buckling = false;
    size_t nodes = 0, segments = 0;
    bool   bead_valid = false;
    std::string error;
};

// one answered frame: metadata + node poses (every skeleton node) + bead chains
struct QzEngineFrame
{
    bool        valid = false;
    std::string phase;                 // stable, pre_failure, failure, collapsed, post_collapse
    double      fold_angle_deg = 0, settle_f = 0, sway = 0, height_deformed = 0, max_util = 0, max_ratio = 0, r_hinge = 0;
    bool        contact = false, collapsed = false, by_buckling = false;
    int         fallen_count = 0, hinge_layer = -1, k_collapse = -1;
    double      strand_mm = 0, strand_at_rest_mm = 0;
    bool        has_bead = false, has_nozzle = false;
    float       bead_w = 0, bead_h = 0;
    float       nozzle[3] = { 0, 0, 0 };
    std::vector<QuasiZero::QzSkelNodePose>            pose;     // per skeleton node
    std::vector<std::vector<QuasiZero::QzSimPoint>>   chains;   // the post-collapse bead
};

class QzEngineClient
{
public:
    QzEngineClient() = default;
    explicit QzEngineClient(std::unique_ptr<QzEngineTransport> transport) : m_transport(std::move(transport)) {}
    void set_transport(std::unique_ptr<QzEngineTransport> t) { m_transport = std::move(t); }
    bool connected() const { return m_transport && m_transport->alive(); }

    // `hello`: fills status.running/version/licence fields; false when the engine did not answer
    bool hello(QzEngineStatus &status);
    // `licence`: point the engine at a file; with `install` the engine copies it into the user
    // directory first (what the activation dialog does)
    bool licence(const std::string &path, bool install, QzEngineStatus &status);
    // `load`: the job (qz-sim-job/1 object) -> summary
    bool load(const nlohmann::json &job, QzEngineLoadInfo &info);
    // `frame`
    bool frame(int top, double time, int seg_lo, int seg_hi, QzEngineFrame &out, std::string &error);
    bool unload();
    void quit();

    const std::string &last_error() const { return m_error; }

private:
    bool request(const nlohmann::json &req, nlohmann::json &reply, std::string *binary = nullptr);
    static void fill_licence(const nlohmann::json &reply, QzEngineStatus &status);

    std::unique_ptr<QzEngineTransport> m_transport;
    std::string m_error;
};

// the job document the slicer sends: material/options/layers/geom/segments
nlohmann::json qz_engine_job(const QuasiZero::QzPasteMaterial &material, const QuasiZero::QzStabilityOptions &options,
                             const std::vector<QuasiZero::QzLayerRecord> &layers, const std::vector<QuasiZero::QzLayerGeom> &geom,
                             const std::vector<QuasiZero::QzSimSegment> &segments);

// per-user path where the PRO add-on installs the engine (mirrors the engine's own rule)
std::string qz_user_engine_dir();

} // namespace Slic3r
