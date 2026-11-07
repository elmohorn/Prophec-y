#include "p6/ProgramLayout.h"
#include "p6/SysexCodec.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct Options {
    fs::path baseline;
    fs::path outputDir;
    std::string captureSetId{"P6-A-YYYYMMDD-A"};
    std::string unitId{"P6-A"};
    std::string firmware{"RECORD_EXACT_VERSION"};
    int sampleRate{96000};
};

struct Test {
    std::string testId;
    int midiNote{60};
    p6::P6PatchState patch;
    std::map<std::uint16_t, std::uint16_t> auditRaw;
    std::string notes;
};

[[noreturn]] void fail(const std::string& message)
{
    throw std::runtime_error(message);
}

void usage(std::ostream& out)
{
    out <<
        "Usage:\n"
        "  p6_generate_p0a_capture_pack <baseline.syx> <output-dir> [options]\n\n"
        "Options:\n"
        "  --capture-set-id <id>   Manifest capture-set ID\n"
        "  --unit-id <id>          Pseudonymous hardware ID (default P6-A)\n"
        "  --firmware <version>    Exact Prophet-6 firmware version\n"
        "  --sample-rate <hz>      48000 or 96000 (default 96000)\n"
        "  --self-test             Run deterministic generator self-test\n"
        "  --help                  Show this help\n\n"
        "The baseline may be a Prophet-6 Program Dump or Edit Buffer dump. The\n"
        "tool preserves its complete 1024-byte payload, changes only verified\n"
        "serialized parameters, and emits Edit Buffer SysEx files so the\n"
        "capture pack does not write program memory.\n";
}

Options parseArgs(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        usage(std::cout);
        std::exit(0);
    }
    if (argc < 3) {
        usage(std::cerr);
        fail("baseline SysEx and output directory are required");
    }

    Options o;
    o.baseline = argv[1];
    o.outputDir = argv[2];

    for (int i = 3; i < argc; ++i) {
        const std::string arg = argv[i];
        auto value = [&]() -> std::string {
            if (++i >= argc)
                fail("missing value after " + arg);
            return argv[i];
        };

        if (arg == "--capture-set-id")
            o.captureSetId = value();
        else if (arg == "--unit-id")
            o.unitId = value();
        else if (arg == "--firmware")
            o.firmware = value();
        else if (arg == "--sample-rate")
            o.sampleRate = std::stoi(value());
        else if (arg == "--help") {
            usage(std::cout);
            std::exit(0);
        } else
            fail("unknown option: " + arg);
    }

    if (o.sampleRate != 48000 && o.sampleRate != 96000)
        fail("--sample-rate must be 48000 or 96000");
    return o;
}

std::vector<std::uint8_t> readBytes(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        fail("cannot open baseline SysEx: " + path.string());
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void writeBytes(const fs::path& path, const std::vector<std::uint8_t>& bytes)
{
    std::ofstream out(path, std::ios::binary);
    if (!out)
        fail("cannot write " + path.string());
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    if (!out)
        fail("failed while writing " + path.string());
}

void setRaw(p6::P6PatchState& patch, std::uint16_t nrpn, std::uint16_t raw)
{
    const auto* info = p6::findSerializedByNrpn(nrpn);
    if (!info)
        fail("capture generator references unknown NRPN " + std::to_string(nrpn));
    if (raw < info->minimum || raw > info->maximum) {
        fail("NRPN " + std::to_string(nrpn) + " raw value " + std::to_string(raw) +
             " outside verified range " + std::to_string(info->minimum) + ".." +
             std::to_string(info->maximum));
    }
    if (raw > 255)
        fail("capture generator only writes verified one-byte serialized parameters");
    patch.rawPayload[info->sysexOffset] = static_cast<std::uint8_t>(raw);
}

void neutralizeP0A(p6::P6PatchState& patch)
{
    setRaw(patch, 0, 24);
    setRaw(patch, 1, 0);
    setRaw(patch, 2, 127);
    setRaw(patch, 3, 0);
    setRaw(patch, 4, 127);
    setRaw(patch, 5, 24);
    setRaw(patch, 6, 127);
    setRaw(patch, 7, 0);
    setRaw(patch, 8, 0);
    setRaw(patch, 9, 127);
    setRaw(patch, 10, 0);
    setRaw(patch, 11, 1);
    setRaw(patch, 27, 0);
    setRaw(patch, 32, 0);
    setRaw(patch, 33, 0);

    setRaw(patch, 45, 164);
    setRaw(patch, 46, 0);
    setRaw(patch, 47, 0);
    setRaw(patch, 48, 0);
    setRaw(patch, 49, 0);
    setRaw(patch, 50, 0);
    setRaw(patch, 51, 0);
    setRaw(patch, 52, 0);

    setRaw(patch, 66, 127);
    setRaw(patch, 67, 0);
    setRaw(patch, 68, 127);
    setRaw(patch, 69, 127);
    setRaw(patch, 70, 0);
    setRaw(patch, 71, 0);
    setRaw(patch, 77, 127);
    setRaw(patch, 78, 0);
    setRaw(patch, 79, 127);
    setRaw(patch, 80, 127);
    setRaw(patch, 81, 0);
    setRaw(patch, 82, 127);

    setRaw(patch, 89, 0);
    setRaw(patch, 91, 0);
    for (const auto nrpn : {93, 94, 95, 96, 97, 98})
        setRaw(patch, static_cast<std::uint16_t>(nrpn), 0);

    setRaw(patch, 109, 127);
    for (const auto nrpn : {110, 111, 112, 113, 114, 115})
        setRaw(patch, static_cast<std::uint16_t>(nrpn), 0);

    setRaw(patch, 119, 0);
    setRaw(patch, 127, 0);
    setRaw(patch, 135, 0);

    setRaw(patch, 143, 127);
    setRaw(patch, 144, 127);
    for (const auto nrpn : {145, 146, 147, 148, 149})
        setRaw(patch, static_cast<std::uint16_t>(nrpn), 0);

    setRaw(patch, 156, 0);
    setRaw(patch, 160, 0);
    setRaw(patch, 29, 0);
    setRaw(patch, 63, 0);
    setRaw(patch, 64, 0);
}

std::string pad3(int v)
{
    std::ostringstream out;
    out << std::setw(3) << std::setfill('0') << v;
    return out.str();
}

std::string noteName(int midi)
{
    static constexpr std::array<std::string_view, 12> names{
        "C", "CS", "D", "DS", "E", "F", "FS", "G", "GS", "A", "AS", "B"
    };
    const int note = std::clamp(midi, 0, 127);
    const int octave = note / 12 - 1;
    return std::string(names[static_cast<std::size_t>(note % 12)]) +
           std::to_string(octave);
}

void addTest(std::vector<Test>& tests,
             const p6::P6PatchState& base,
             std::string testId,
             int note,
             std::map<std::uint16_t, std::uint16_t> changes,
             std::string notes)
{
    Test t;
    t.testId = std::move(testId);
    t.midiNote = note;
    t.patch = base;
    t.auditRaw = changes;
    t.notes = std::move(notes);
    for (const auto& [nrpn, raw] : changes)
        setRaw(t.patch, nrpn, raw);
    p6::populateRawParametersFromPayload(t.patch);
    tests.push_back(std::move(t));
}

std::vector<Test> makePlan(p6::P6PatchState base)
{
    neutralizeP0A(base);
    p6::populateRawParametersFromPayload(base);

    std::vector<Test> tests;
    tests.reserve(96);

    const std::array<int, 5> pitchNotes{36, 48, 60, 72, 84};
    for (const int coarse : {12, 24, 36}) {
        for (const int note : pitchNotes) {
            addTest(tests, base,
                    "OSC1_PITCH_COARSE" + pad3(coarse) + "_" + noteName(note),
                    note,
                    {{0, static_cast<std::uint16_t>(coarse)}},
                    "Osc1 pitch-law sweep; triangle, isolated dry path.");
        }
    }

    for (const int fine : {0, 64, 127, 190, 254}) {
        addTest(tests, base,
                "OSC2_FINE_C4_RAW" + pad3(fine),
                60,
                {{2, 0}, {5, 24}, {6, static_cast<std::uint16_t>(fine)}, {7, 127}},
                "Osc2 fine-tune law; Osc1 mixer level zero, Osc2 triangle isolated.");
    }

    const std::array<int, 9> shapeValues{0, 32, 64, 96, 127, 160, 192, 224, 254};
    for (const int shape : shapeValues) {
        addTest(tests, base,
                "OSC1_SHAPE_C4_RAW" + pad3(shape),
                60,
                {{2, 127}, {3, static_cast<std::uint16_t>(shape)}, {4, 127}},
                "Primary shape-morph sweep at C4.");
        if (shape == 0 || shape == 127 || shape == 254) {
            for (const int note : {36, 84}) {
                addTest(tests, base,
                        "OSC1_SHAPE_" + noteName(note) + "_RAW" + pad3(shape),
                        note,
                        {{2, 127}, {3, static_cast<std::uint16_t>(shape)}, {4, 127}},
                        "Shape anchor repeated away from C4 to expose frequency-dependent behaviour.");
            }
        }
    }

    const std::array<int, 12> pwValues{0, 16, 32, 64, 96, 127, 128, 160, 192, 224, 240, 255};
    for (const int pw : pwValues) {
        addTest(tests, base,
                "OSC1_PW_C4_RAW" + pad3(pw),
                60,
                {{2, 127}, {3, 254}, {4, static_cast<std::uint16_t>(pw)}},
                "Pulse-width sweep for duty, DC/mean and level-law measurement.");
    }

    const std::array<int, 9> levels{0, 16, 32, 48, 64, 80, 96, 112, 127};
    struct Source {
        std::string_view name;
        std::uint16_t nrpn;
    };
    const std::array<Source, 4> sources{{
        {"OSC1", 2}, {"OSC2", 7}, {"SUB", 27}, {"NOISE", 32}
    }};
    for (const auto source : sources) {
        for (const int level : levels) {
            std::map<std::uint16_t, std::uint16_t> changes{
                {2, 0}, {7, 0}, {27, 0}, {32, 0}
            };
            changes[source.nrpn] = static_cast<std::uint16_t>(level);
            if (source.nrpn == 7)
                changes[8] = 0;
            addTest(tests, base,
                    std::string("MIX_") + std::string(source.name) +
                        "_C4_RAW" + pad3(level),
                    60,
                    std::move(changes),
                    "Single-source mixer level sweep.");
        }
    }

    for (const auto& [osc1, osc2] :
         {std::pair{64, 64}, std::pair{96, 96}, std::pair{127, 127},
          std::pair{127, 96}, std::pair{96, 127}}) {
        addTest(tests, base,
                "MIX_OSC12_C4_" + pad3(osc1) + "_" + pad3(osc2),
                60,
                {{2, static_cast<std::uint16_t>(osc1)},
                 {7, static_cast<std::uint16_t>(osc2)},
                 {27, 0}, {32, 0}, {3, 0}, {8, 0}},
                "Two-oscillator upper-half mixer interaction/saturation probe.");
    }

    return tests;
}

std::string jsonEscape(std::string_view text)
{
    std::ostringstream out;
    for (const unsigned char c : text) {
        switch (c) {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (c < 0x20)
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                    << static_cast<int>(c) << std::dec;
            else
                out << static_cast<char>(c);
        }
    }
    return out.str();
}

void writeManifest(const fs::path& path,
                   const Options& options,
                   const std::vector<Test>& tests)
{
    std::ofstream out(path);
    if (!out)
        fail("cannot write manifest: " + path.string());

    out << "{\n"
        << "  \"schema_version\": \"p6-reference-capture/1\",\n"
        << "  \"capture_set_id\": \"" << jsonEscape(options.captureSetId) << "\",\n"
        << "  \"hardware\": {\n"
        << "    \"model\": \"Sequential Prophet-6\",\n"
        << "    \"unit_id\": \"" << jsonEscape(options.unitId) << "\",\n"
        << "    \"firmware\": \"" << jsonEscape(options.firmware) << "\",\n"
        << "    \"globals\": {\"vintage_mode\": \"RECORD_EXACT_SETTING\"}\n"
        << "  },\n"
        << "  \"acquisition\": {\n"
        << "    \"sample_rate_hz\": " << options.sampleRate << ",\n"
        << "    \"bit_depth\": 24,\n"
        << "    \"channels\": 2,\n"
        << "    \"format\": \"WAV_PCM\",\n"
        << "    \"interface_chain\": \"RECORD_INTERFACE_AND_INPUTS\",\n"
        << "    \"fixed_gain_reference\": \"RECORD_INTERFACE_GAIN\",\n"
        << "    \"output_route\": \"Stereo L/R\"\n"
        << "  },\n"
        << "  \"default_isolation\": {\n"
        << "    \"fx_off\": true,\n"
        << "    \"distortion_raw\": 0,\n"
        << "    \"pan_spread_raw\": 0,\n"
        << "    \"slop_raw\": 0,\n"
        << "    \"lfo_destinations_off\": true,\n"
        << "    \"poly_mod_destinations_off\": true,\n"
        << "    \"velocity_modulation_off\": true,\n"
        << "    \"aftertouch_modulation_off\": true\n"
        << "  },\n"
        << "  \"tests\": [\n";

    for (std::size_t i = 0; i < tests.size(); ++i) {
        const auto& t = tests[i];
        const std::string stem = t.testId;
        out << "    {\n"
            << "      \"capture_id\": \"" << jsonEscape(options.captureSetId + "-" + stem) << "\",\n"
            << "      \"test_id\": \"" << jsonEscape(stem) << "\",\n"
            << "      \"family\": \"P0-A\",\n"
            << "      \"capture_grade\": \"calibration\",\n"
            << "      \"sysex_file\": \"sysex/" << jsonEscape(stem) << ".syx\",\n"
            << "      \"audio_file\": \"audio/" << jsonEscape(stem) << ".wav\",\n"
            << "      \"midi\": {\"notes\": [" << t.midiNote
            << "], \"velocities\": [127], \"channel_pressure\": 0, "
               "\"mod_wheel\": 0, \"pitch_bend_14bit\": 8192, \"sustain\": false},\n"
            << "      \"timing\": {\"pre_roll_seconds\": 2.0, \"note_seconds\": 3.0, "
               "\"release_seconds\": 2.0},\n"
            << "      \"raw_parameters\": {";

        bool first = true;
        for (const auto& [nrpn, raw] : t.auditRaw) {
            if (!first)
                out << ", ";
            first = false;
            out << "\"" << nrpn << "\": " << raw;
        }
        out << "},\n"
            << "      \"isolation_deviations\": [],\n"
            << "      \"notes\": \"" << jsonEscape(t.notes) << "\"\n"
            << "    }";
        if (i + 1 != tests.size())
            out << ",";
        out << "\n";
    }

    out << "  ],\n"
        << "  \"notes\": \"Generated P0-A capture plan. Fill all RECORD_* placeholders "
           "and inspect the baseline/isolation state before calibration use.\"\n"
        << "}\n";
}

void writeInstructions(const fs::path& path, std::size_t count)
{
    std::ofstream out(path);
    if (!out)
        fail("cannot write capture instructions");
    out <<
        "P0-A CAPTURE PACK\n"
        "=================\n\n"
        "This pack contains " << count << " one-to-one Edit Buffer SysEx/audio tasks.\n"
        "For each manifest test in order:\n"
        "1. Send its sysex/<TEST_ID>.syx to the Prophet-6 edit buffer.\n"
        "2. Confirm the hardware/global conditions in docs/V0_19_SOUND_MATCHING.md.\n"
        "3. Record the specified MIDI note at velocity 127 with the declared timing.\n"
        "4. Save the untouched take as audio/<TEST_ID>.wav.\n"
        "5. Do not change interface gain within the sweep.\n\n"
        "Before recording, replace every RECORD_* placeholder in manifest.json.\n"
        "After recording, run tools/validate_reference_manifest.py manifest.json.\n\n"
        "Generated files are Edit Buffer dumps (command 0x03); they do not target\n"
        "a stored program slot. The source baseline's unknown bytes are preserved.\n";
}

int runSelfTest()
{
    p6::P6PatchState synthetic{};
    const auto tests = makePlan(synthetic);
    if (tests.size() != 88)
        fail("self-test expected 88 capture tasks, got " + std::to_string(tests.size()));

    std::set<std::string> ids;
    for (const auto& test : tests) {
        if (!ids.insert(test.testId).second)
            fail("self-test duplicate test ID: " + test.testId);
        const auto encoded = p6::SysexCodec::encodeEditBuffer(test.patch);
        const auto decoded = p6::SysexCodec::decode(encoded);
        if (!decoded.ok || decoded.kind != p6::SysexKind::EditBuffer ||
            decoded.patch.rawPayload != test.patch.rawPayload) {
            fail("self-test SysEx round-trip failed for " + test.testId);
        }
    }

    std::cout << "P0-A generator self-test OK: " << tests.size()
              << " unique Edit Buffer tasks\n";
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--self-test")
            return runSelfTest();

        const auto options = parseArgs(argc, argv);
        const auto bytes = readBytes(options.baseline);
        const auto decoded = p6::SysexCodec::decode(bytes);
        if (!decoded.ok)
            fail("baseline decode failed: " + decoded.error);

        const auto tests = makePlan(decoded.patch);
        fs::create_directories(options.outputDir / "sysex");
        fs::create_directories(options.outputDir / "audio");

        for (const auto& test : tests) {
            const auto encoded = p6::SysexCodec::encodeEditBuffer(test.patch);
            const auto path = options.outputDir / "sysex" / (test.testId + ".syx");
            writeBytes(path, encoded);

            const auto roundTrip = p6::SysexCodec::decode(encoded);
            if (!roundTrip.ok || roundTrip.patch.rawPayload != test.patch.rawPayload)
                fail("internal round-trip failed for " + test.testId);
        }

        writeManifest(options.outputDir / "manifest.json", options, tests);
        writeInstructions(options.outputDir / "CAPTURE_README.txt", tests.size());

        std::cout << "Generated " << tests.size() << " P0-A capture tasks in "
                  << options.outputDir << "\n";
        std::cout << "Baseline kind: "
                  << (decoded.kind == p6::SysexKind::EditBuffer ? "Edit Buffer" : "Program Dump")
                  << "; output kind: Edit Buffer\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "p6_generate_p0a_capture_pack: " << e.what() << "\n";
        return 1;
    }
}
